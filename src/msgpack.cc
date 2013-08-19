#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <msgpack.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <stack>

using namespace std;
using namespace v8;
using namespace node;

#define SBUF_POOL 50000

// MSC does not support C99 trunc function.
#ifdef _MSC_BUILD
double trunc(double d){ return (d>0) ? floor(d) : ceil(d) ; }
#endif

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

static stack<msgpack_sbuffer *> sbuffers;

#define DBG_PRINT_BUF(buf, name) \
    do { \
        fprintf(stderr, "Buffer %s has %lu bytes:\n", \
            (name), Buffer::Length(buf) \
        ); \
        for (uint32_t i = 0; i * 16 < Buffer::Length(buf); i++) { \
            fprintf(stderr, "  "); \
            for (uint32_t ii = 0; \
                 ii < 16 && (i * 16) + ii < Buffer::Length(buf); \
                 ii++) { \
                fprintf(stderr, "%s%2.2hhx", \
                    (ii > 0 && (ii % 2 == 0)) ? " " : "", \
                    Buffer::Data(buf)[i * 16 + ii] \
                ); \
            } \
            fprintf(stderr, "\n"); \
        } \
    } while (0)

// This will be passed to Buffer::New so that we can manage our own memory.
// In other news, I am unsure what to do with hint, as I've never seen this
// coding pattern before.  For now I have overloaded it to be a void pointer
// to a msgpack_sbuffer.  This let's us push it onto the stack for use later.
static void
_free_sbuf(char *data, void *hint) {
    if (data != NULL && hint != NULL) {
        msgpack_sbuffer *sbuffer = (msgpack_sbuffer *)hint;
        if (sbuffers.size() > SBUF_POOL ||
            sbuffer->alloc > (MSGPACK_SBUFFER_INIT_SIZE * 5)) {
            msgpack_sbuffer_free(sbuffer);
        } else {
            sbuffer->size = 0;
            sbuffers.push(sbuffer);
        }
    }
}

// Convert a V8 object to a MessagePack object.
//
// This method is recursive. It will probably blow out the stack on objects
// with extremely deep nesting.
//
// If a circular reference is detected, an exception is thrown.
static void
v8_to_msgpack(Handle<Value> v8obj, msgpack_object *mo, msgpack_zone *mz, size_t depth) {
    static const Persistent<String> TOJSON = NODE_PSYMBOL("toJSON");

    if (512 < ++depth) {
        throw MsgpackException("Cowardly refusing to pack object with circular reference");
    }

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
            mo->via.u64 = static_cast<uint64_t>(d);
        } else {
            mo->type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
            mo->via.i64 = static_cast<int64_t>(d);
        }
    } else if (v8obj->IsString()) {
        mo->type = MSGPACK_OBJECT_RAW;
        mo->via.raw.size = static_cast<uint32_t>(DecodeBytes(v8obj, UTF8));
        mo->via.raw.ptr = (char*) msgpack_zone_malloc(mz, mo->via.raw.size);

        DecodeWrite((char*) mo->via.raw.ptr, mo->via.raw.size, v8obj, UTF8);
    } else if (v8obj->IsDate()) {
        mo->type = MSGPACK_OBJECT_RAW;
        Handle<Date> date = Handle<Date>::Cast(v8obj);
        Handle<Function> func = Handle<Function>::Cast(date->Get(String::New("toISOString")));
        Handle<Value> argv[1] = {};
        Handle<Value> result = func->Call(date, 0, argv);
        mo->via.raw.size = static_cast<uint32_t>(DecodeBytes(result, UTF8));
        mo->via.raw.ptr = (char*) msgpack_zone_malloc(mz, mo->via.raw.size);

        DecodeWrite((char*) mo->via.raw.ptr, mo->via.raw.size, result, UTF8);
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
            v8_to_msgpack(v, &mo->via.array.ptr[i], mz, depth);
        }
    } else if (Buffer::HasInstance(v8obj)) {
        Local<Object> buf = v8obj->ToObject();


        mo->type = MSGPACK_OBJECT_RAW;
        mo->via.raw.size = static_cast<uint32_t>(Buffer::Length(buf));
        mo->via.raw.ptr = Buffer::Data(buf);
    } else {
        Local<Object> o = v8obj->ToObject();

        // for o.toJSON()
        if (o->Has(TOJSON) && o->Get(TOJSON)->IsFunction()) {
            Local<Function> fn = Local<Function>::Cast(o->Get(TOJSON));
            v8_to_msgpack(fn->Call(o, 0, NULL), mo, mz, depth);
            return;
        }

        Local<Array> a = o->GetPropertyNames();

        mo->type = MSGPACK_OBJECT_MAP;
        mo->via.map.size = a->Length();
        mo->via.map.ptr = (msgpack_object_kv*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object_kv) * mo->via.map.size
        );

        for (uint32_t i = 0; i < a->Length(); i++) {
            Local<Value> k = a->Get(i);

            v8_to_msgpack(k, &mo->via.map.ptr[i].key, mz, depth);
            v8_to_msgpack(o->Get(k), &mo->via.map.ptr[i].val, mz, depth);
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
        // As per Issue #42, we need to use the base Number
        // class as opposed to the subclass Integer, since
        // only the former takes 64-bit inputs. Using the
        // Integer subclass will truncate 64-bit values.
        return Number::New(static_cast<double>(mo->via.u64));

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
        // See comment for MSGPACK_OBJECT_POSITIVE_INTEGER
        return Number::New(static_cast<double>(mo->via.i64));

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
// serialized to the same bytestream, back-to-back.
static Handle<Value>
pack(const Arguments &args) {
    HandleScope scope;

    msgpack_packer pk;
    MsgpackZone mz;
    msgpack_sbuffer *sb;

    if (!sbuffers.empty()) {
        sb = sbuffers.top();
        sbuffers.pop();
    } else {
        sb = msgpack_sbuffer_new();
    }

    msgpack_packer_init(&pk, sb, msgpack_sbuffer_write);

    for (int i = 0; i < args.Length(); i++) {
        msgpack_object mo;

        try {
            v8_to_msgpack(args[i], &mo, &mz._mz, 0);
        } catch (MsgpackException e) {
            return ThrowException(e.getThrownException());
        }

        if (msgpack_pack_object(&pk, mo)) {
            return ThrowException(Exception::Error(
                String::New("Error serializaing object")));
        }
    }

    v8::Local<Buffer> slowBuffer = node::Buffer::New(
        sb->data, sb->alloc, _free_sbuf, (void *)sb
    );

    // godsflaw: this part makes msgpack.pack() 1x slower than JSON.stringify()
    // reaching back into JS appears to be expensive.
    v8::Local<Object> global = v8::Context::GetCurrent()->Global();
    v8::Local<Value> bv = global->Get(String::NewSymbol("Buffer"));

    assert(bv->IsFunction());

    Local<Function> bc = v8::Local<Function>::Cast(bv);
    Handle<Value> cArgs[3] = {
        slowBuffer->handle_,
        v8::Integer::New(sb->size),
        v8::Integer::New(0)
    };
    v8::Local<Object> fastBuffer = bc->NewInstance(3, cArgs);

    return scope.Close(fastBuffer);
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

    Local<Object> buf = args[0]->ToObject();

    MsgpackZone mz;
    msgpack_object mo;
    size_t off = 0;

    switch (msgpack_unpack(Buffer::Data(buf), Buffer::Length(buf), &off, &mz._mz, &mo)) {
    case MSGPACK_UNPACK_EXTRA_BYTES:
    case MSGPACK_UNPACK_SUCCESS:
        try {
            msgpack_unpack_template->GetFunction()->Set(
                msgpack_bytes_remaining_symbol,
                Integer::New(static_cast<int32_t>(Buffer::Length(buf) - off))
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

NODE_MODULE(msgpackBinding, init);
// vim:ts=4 sw=4 et
