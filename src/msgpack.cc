/* Bindings for http://msgpack.sourceforge.net/ */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <msgpack.h>
#include <math.h>
#include <vector>

using namespace v8;
using namespace node;

static Persistent<FunctionTemplate> msgpack_unpack_template;

// An exception class that wraps a textual message
class MsgpackException {
    public:
        MsgpackException(const char *str) :
            msg(String::New(str)) {
        }

        Handle<Value> getThrownException() {
            return Exception::TypeError(msg);
        }

    private:
        const Handle<String> msg;
};

// A holder for a msgpack_zone object; ensures destruction on scope exit
class MsgpackZone {
    public:
        msgpack_zone _mz;

        MsgpackZone(size_t sz = 1024) {
            msgpack_zone_init(&this->_mz, sz);
        }

        ~MsgpackZone() {
            msgpack_zone_destroy(&this->_mz);
        }
};

// A holder for a msgpack_sbuffer object; ensures destruction on scope exit
class MsgpackSbuffer {
    public:
        msgpack_sbuffer _sbuf;

        MsgpackSbuffer() {
            msgpack_sbuffer_init(&this->_sbuf);
        }

        ~MsgpackSbuffer() {
            msgpack_sbuffer_destroy(&this->_sbuf);
        }
};

// Object to check for cycles when packing.
//
// The implementation tracks all previously-seen values in an unordered
// std::vector and performs a simple membership check using
// v8::Value::StrictEquals(). Thus, serialization requires O(n^2) checks where
// n is the number of array/object instances found in the object being
// serialized. This is lame.
//
// XXX: Change this to a std::multimap based on v8::Object::GetIdentityHash()
//      to reduce the search space. This should get us down to O(n log n) with
//      a std::multimap built on top of a RBT or similar.
//
// XXX: An even better fix for this would be to use
//      v8::Object::SetHiddenValue(), but this causes memory leaks for some
//      reason (see http://github.com/pgriess/node-msgpack/issues/#issue/4)
class MsgpackCycle {
    public:
        MsgpackCycle() {
        }

        ~MsgpackCycle() {
            _objs.clear();
        }

        void check(Handle<Value> v) {
            if (!v->IsArray() && !v->IsObject()) {
                return;
            }

            Handle<Object> o = v->ToObject();

            for (std::vector< Handle<Object> >::iterator iter = _objs.begin();
                 iter != _objs.end();
                 iter++) {
                if ((*iter)->StrictEquals(o)) {
                    // This message should not change without updating
                    // test/test.js to expect the new text
                    throw MsgpackException( \
                        "Cowardly refusing to pack object with circular reference" \
                    ); 
                }
            }

            _objs.push_back(o);
        }

    private:
        std::vector< Handle<Object> > _objs;
};

#define DBG_PRINT_BUF(buf, name) \
    do { \
        fprintf(stderr, "Buffer %s has %lu bytes:\n", \
            (name), (buf)->length() \
        ); \
        for (uint32_t i = 0; i * 16 < (buf)->length(); i++) { \
            fprintf(stderr, "  "); \
            for (uint32_t ii = 0; \
                 ii < 16 && (i * 16) + ii < (buf)->length(); \
                 ii++) { \
                fprintf(stderr, "%s%2.2hhx", \
                    (ii > 0 && (ii % 2 == 0)) ? " " : "", \
                    (buf)->data()[i * 16 + ii] \
                ); \
            } \
            fprintf(stderr, "\n"); \
        } \
    } while (0)

// Convert a V8 object to a MessagePack object.
//
// This method is recursive. It will probably blow out the stack on objects
// with extremely deep nesting.
//
// If a circular reference is detected, an exception is thrown.
static void
v8_to_msgpack(Handle<Value> v8obj, msgpack_object *mo, msgpack_zone *mz,
              MsgpackCycle *mc) {
    mc->check(v8obj);

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

        mo->type = MSGPACK_OBJECT_ARRAY;
        mo->via.array.size = a->Length();
        mo->via.array.ptr = (msgpack_object*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object) * mo->via.array.size
        );

        for (uint32_t i = 0; i < a->Length(); i++) {
            Local<Value> v = a->Get(i);
            v8_to_msgpack(v, &mo->via.array.ptr[i], mz, mc);
        }
    } else if (Buffer::HasInstance(v8obj)) {
        Local<Object> buf = v8obj->ToObject();

        mo->type = MSGPACK_OBJECT_RAW;
        mo->via.raw.size = Buffer::Length(buf);
        mo->via.raw.ptr = Buffer::Data(buf);
    } else {
        Local<Object> o = v8obj->ToObject();
        Local<Array> a = o->GetPropertyNames();

        mo->type = MSGPACK_OBJECT_MAP;
        mo->via.map.size = a->Length();
        mo->via.map.ptr = (msgpack_object_kv*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object_kv) * mo->via.map.size
        );

        for (uint32_t i = 0; i < a->Length(); i++) {
            Local<Value> k = a->Get(i);

            v8_to_msgpack(k, &mo->via.map.ptr[i].key, mz, mc);
            v8_to_msgpack(o->Get(k), &mo->via.map.ptr[i].val, mz, mc);
        }
    }
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

        for (uint32_t i = 0; i < mo->via.array.size; i++) {
            a->Set(i, msgpack_to_v8(&mo->via.array.ptr[i]));
        }

        return a;
    }

    case MSGPACK_OBJECT_RAW:
        return String::New(mo->via.raw.ptr, mo->via.raw.size);

    case MSGPACK_OBJECT_MAP: {
        Local<Object> o = Object::New();

        for (uint32_t i = 0; i < mo->via.map.size; i++) {
            o->Set(
                msgpack_to_v8(&mo->via.map.ptr[i].key),
                msgpack_to_v8(&mo->via.map.ptr[i].val)
            );
        }

        return o;
    }

    default:
        throw MsgpackException("Encountered unknown MesssagePack object type");
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
    HandleScope scope;

    msgpack_packer pk;
    MsgpackZone mz;
    MsgpackSbuffer sb;
    MsgpackCycle mc;

    msgpack_packer_init(&pk, &sb._sbuf, msgpack_sbuffer_write);

    for (int i = 0; i < args.Length(); i++) {
        msgpack_object mo;

        try {
            v8_to_msgpack(args[0], &mo, &mz._mz, &mc);
        } catch (MsgpackException e) {
            return ThrowException(e.getThrownException());
        }

        if (msgpack_pack_object(&pk, mo)) {
            return ThrowException(Exception::Error(
                String::New("Error serializaing object")));
        }
    }

    Buffer *bp = Buffer::New(sb._sbuf.size);
    memcpy(bp->data(), sb._sbuf.data, sb._sbuf.size);

    return scope.Close(bp->handle_);
}

// var o = msgpack.unpack(buf);
//
// Return the JavaScript object resulting from unpacking the contents of the
// specified buffer. If the buffer does not contain a complete object, the
// undefined value is returned.
static Handle<Value>
unpack(const Arguments &args) {
    static Persistent<String> msgpack_bytes_remaining_symbol = 
        NODE_PSYMBOL("bytes_remaining");

    HandleScope scope;

    if (args.Length() < 0 || !Buffer::HasInstance(args[0])) {
        return ThrowException(Exception::TypeError(
            String::New("First argument must be a Buffer")));
    }

    Buffer *buf = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

    MsgpackZone mz;
    msgpack_object mo;
    size_t off = 0;

    switch (msgpack_unpack(buf->data(), buf->length(), &off, &mz._mz, &mo)) {
    case MSGPACK_UNPACK_EXTRA_BYTES:
    case MSGPACK_UNPACK_SUCCESS:
        try {
            msgpack_unpack_template->GetFunction()->Set(
                msgpack_bytes_remaining_symbol,
                Integer::New(buf->length() - off)
            );
            return scope.Close(msgpack_to_v8(&mo));
        } catch (MsgpackException e) {
            return ThrowException(e.getThrownException());
        }
    
    case MSGPACK_UNPACK_CONTINUE:
        return scope.Close(Undefined());

    default:
        return ThrowException(Exception::Error(
            String::New("Error de-serializing object")));
    }
}

extern "C" void
init(Handle<Object> target) {
    HandleScope scope;

    NODE_SET_METHOD(target, "pack", pack);

    // Go through this mess rather than call NODE_SET_METHOD so that we can set
    // a field on the function for 'bytes_remaining'.
    msgpack_unpack_template = Persistent<FunctionTemplate>::New(
        FunctionTemplate::New(unpack)
    );
    target->Set(
        String::NewSymbol("unpack"),
        msgpack_unpack_template->GetFunction()
    );
}

// vim:ts=4 sw=4 et
