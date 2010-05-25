/* Bindings for http://msgpack.sourceforge.net/ */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <msgpack.h>
#include <math.h>

using namespace v8;
using namespace node;

// Convert a v8 object to a MessagePack object.
//
// This method is recursive. It will probably blow out the stack on objects
// with extremely deep nesting.
static void
v8_to_msgpack(Handle<Value> v8obj, msgpack_object *mo, msgpack_zone *mz) {
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
        Local<Array> a = Local<Array>::Cast(v8obj->ToObject());

        mo->type = MSGPACK_OBJECT_ARRAY;
        mo->via.array.size = a->Length();
        mo->via.array.ptr = (msgpack_object*) msgpack_zone_malloc(
            mz,
            sizeof(msgpack_object) * mo->via.array.size
        );

        for (int i = 0; i < a->Length(); i++) {
            Local<Value> v = a->Get(i);
            v8_to_msgpack(v, &mo->via.array.ptr[i], mz);
        }
    } else {
    }
}

// var buf = msgpack.pack(obj);
//
// Returns a Buffer object representing the serialized state of the provided
// JavaScript object. If more arguments are provided, their serialized state
// will be accumulated to the end of the previous value(s).
static Handle<Value>
pack(const Arguments &args) {
    HandleScope scope;

    msgpack_packer pk;
    msgpack_sbuffer sbuf;
    msgpack_zone mz;
  
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_zone_init(&mz, 1024);

    for (int i = 0; i < args.Length(); i++) {
        msgpack_object mo;

        v8_to_msgpack(args[0], &mo, &mz);
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

extern "C" void
init(Handle<Object> target) {
    HandleScope scope;

    NODE_SET_METHOD(target, "pack", pack);
}

// vim:ts=4 sw=4 et
