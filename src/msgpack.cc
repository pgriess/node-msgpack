/* Bindings for http://msgpack.sourceforge.net/ */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <msgpack.h>
#include <math.h>

using namespace v8;
using namespace node;

static Persistent<String> msgpack_tag_symbol;

#define CHECK_CIRCULAR_REFS(o, t) \
    do { \
        Local<Value> ov = (o)->GetHiddenValue(msgpack_tag_symbol); \
        if (!ov.IsEmpty() && ov->Equals(t)) { \
            return false; \
        } \
        (o)->SetHiddenValue(msgpack_tag_symbol, (t)); \
    } while(0)

// Convert a V8 object to a MessagePack object.
//
// This method is recursive. It will probably blow out the stack on objects
// with extremely deep nesting.
//
// This method can detect circular references in provided objects. It utilizes
// the v8::Object::SetHiddenValue() facility to tag each encountered object
// with a value unique to this serialization run. We leave the tags attached to
// each object for ease-of-implementation (otherwise we'd have to track each
// tagged v8::Object instance and un-tag at the end). This decision can be
// revisited in the future if this proves problematic. I'd expect most objects
// being packed to be short-lived anyway.
//
// Returns false if we detected a circular reference; true otherwise.
static bool
v8_to_msgpack(Handle<Value> v8obj, msgpack_object *mo, msgpack_zone *mz, Handle<Value> tag) {
    if (v8obj->IsUndefined() || v8obj->IsNull()) {
        mo->type = MSGPACK_OBJECT_NIL;
    } else if (v8obj->IsBoolean()) {
        mo->type = MSGPACK_OBJECT_BOOLEAN;
        mo->via.boolean = v8obj->BooleanValue();
    } else if (v8obj->IsNumber()) {
        double d = v8obj->NumberValue();
        if (trunc(d) != d) {
            mo->type = MSGPACK_OBJECT_DOUBLE;
            mo->via.dec = d;
        } else if (d > 0) {
            mo->type = MSGPACK_OBJECT_POSITIVE_INTEGER;
            mo->via.u64 = d;
        } else {
            mo->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
            mo->via.i64 = d;
        }
    } else if (v8obj->IsString()) {
        mo->type = MSGPACK_OBJECT_RAW;
        mo->via.raw.size = DecodeBytes(v8obj, UTF8);
        mo->via.raw.ptr = (char*) msgpack_zone_malloc(mz, mo->via.raw.size);

        DecodeWrite((char*) mo->via.raw.ptr, mo->via.raw.size, v8obj, UTF8);
    } else if (v8obj->IsArray()) {
        Local<Object> o = v8obj->ToObject();
        Local<Array> a = Local<Array>::Cast(o);

        CHECK_CIRCULAR_REFS(o, tag);

        mo->type = MSGPACK_OBJECT_ARRAY;
        mo->via.array.size = a->Length();
        mo->via.array.ptr = (msgpack_object*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object) * mo->via.array.size
        );

        for (int i = 0; i < a->Length(); i++) {
            Local<Value> v = a->Get(i);
            if (!v8_to_msgpack(v, &mo->via.array.ptr[i], mz, tag)) {
                return false;
            }
        }
    } else {
        Local<Object> o = v8obj->ToObject();
        Local<Array> a = o->GetPropertyNames();

        CHECK_CIRCULAR_REFS(o, tag);

        mo->type = MSGPACK_OBJECT_MAP;
        mo->via.map.size = a->Length();
        mo->via.map.ptr = (msgpack_object_kv*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object_kv) * mo->via.map.size
        );

        for (int i = 0; i < a->Length(); i++) {
            Local<Value> k = a->Get(i);

            if (!v8_to_msgpack(k, &mo->via.map.ptr[i].key, mz, tag)) {
                return false;
            }

            if (!v8_to_msgpack(o->Get(k), &mo->via.map.ptr[i].val, mz, tag)) {
                return false;
            }
        }
    }

    return true;
}

// Convert a MessagePack object to a V8 object.
//
// This method is recursive. It will probably blow out the stack on objects
// with extremely deep nesting.
static Handle<Value>
msgpack_to_v8(msgpack_object *mo) {
    switch (mo->type) {
    case MSGPACK_OBJECT_NIL:
        return Null();

    case MSGPACK_OBJECT_BOOLEAN:
        return (mo->via.boolean) ?
            True() :
            False();

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
        return Integer::NewFromUnsigned(mo->via.u64);

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
        return Integer::New(mo->via.i64);

    case MSGPACK_OBJECT_DOUBLE:
        return Number::New(mo->via.dec);

    case MSGPACK_OBJECT_ARRAY: {
        Local<Array> a = Array::New(mo->via.array.size);

        for (int i = 0; i < mo->via.array.size; i++) {
            a->Set(i, msgpack_to_v8(&mo->via.array.ptr[i]));
        }

        return a;
    }

    case MSGPACK_OBJECT_RAW:
        return String::New(mo->via.raw.ptr, mo->via.raw.size);

    case MSGPACK_OBJECT_MAP: {
        Local<Object> o = Object::New();

        for (int i = 0; i < mo->via.map.size; i++) {
            o->Set(
                msgpack_to_v8(&mo->via.map.ptr[i].key),
                msgpack_to_v8(&mo->via.map.ptr[i].val)
            );
        }

        return o;
    }

    default:
        return ThrowException(Exception::Error(
            String::New("Encountered unknown MessagePack object type")));
    }
}

// var buf = msgpack.pack(obj[, obj ...]);
//
// Returns a Buffer object representing the serialized state of the provided
// JavaScript object. If more arguments are provided, their serialized state
// will be accumulated to the end of the previous value(s).
//
// Any number of objects can be provided as arguments, and all will be
// serialized to the same bytestream, back-ty-back.
static Handle<Value>
pack(const Arguments &args) {
    static int64_t tag_i = 0;

    HandleScope scope;

    msgpack_packer pk;
    msgpack_sbuffer sbuf;
    msgpack_zone mz;
    Local<Value> tag = Integer::New(++tag_i);
  
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_zone_init(&mz, 1024);

    for (int i = 0; i < args.Length(); i++) {
        msgpack_object mo;

        if (!v8_to_msgpack(args[0], &mo, &mz, tag)) {
            return ThrowException(Exception::TypeError(String::New(
                "Cowardly refusing to pack object with circular reference"
            )));
        }

        if (msgpack_pack_object(&pk, mo)) {
            return ThrowException(Exception::Error(
                String::New("Error serializaing object")));
        }
    }

    Buffer *bp = Buffer::New(sbuf.size);
    memcpy(bp->data(), sbuf.data, sbuf.size);

    msgpack_zone_destroy(&mz);
    msgpack_sbuffer_destroy(&sbuf);

    return scope.Close(bp->handle_);
}

// var o = msgpack.unpack(buf);
//
// Return the JavaScript object resulting from unpacking the contents of the
// specified buffer.
static Handle<Value>
unpack(const Arguments &args) {
    HandleScope scope;

    if (args.Length() < 0 || !Buffer::HasInstance(args[0])) {
        return ThrowException(Exception::TypeError(
            String::New("First argument must be a Buffer")));
    }

    Buffer *buf = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

    msgpack_zone mz;
    msgpack_object mo;
    size_t off = 0;

    msgpack_zone_init(&mz, 1024);

    switch (msgpack_unpack(buf->data(), buf->length(), &off, &mz, &mo)) {
    case MSGPACK_UNPACK_EXTRA_BYTES:
        fprintf(stderr, "msgpack::unpack() got %d extra bytes\n", off);
        /* fall through */

    case MSGPACK_UNPACK_SUCCESS:
        msgpack_zone_destroy(&mz);
        return scope.Close(msgpack_to_v8(&mo));

    default:
        msgpack_zone_destroy(&mz);
        return ThrowException(Exception::Error(
            String::New("Error de-serializing object")));
    }
}

extern "C" void
init(Handle<Object> target) {
    HandleScope scope;

    NODE_SET_METHOD(target, "pack", pack);
    NODE_SET_METHOD(target, "unpack", unpack);

    msgpack_tag_symbol = NODE_PSYMBOL("msgpack::tag");
}

// vim:ts=4 sw=4 et
