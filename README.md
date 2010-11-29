`node-msgpack` is an addon for [NodeJS](http://nodejs.org) that provides an
API for serializing and de-serializing JavaScript objects using the
[MessagePack](http://msgpack.sourceforge.net) library. The performance of this
addon compared to the native `JSON` object is quite good, and the space
required for serialized data is far less than JSON.

### Performance

`node-msgpack` outperforms the built-in `JSON.stringify()` and `JSON.parse()`
methods handily. The following tests were performed with 500,000 instances of
the JavaScript object `{'abcdef' : 1, 'qqq' : 13, '19' : [1, 2, 3, 4]}`:

   * `JSON.stringify()` 7.17 seconds
   * `JSON.parse(JSON.stringify())` 22.18 seconds
   * `msgpack.pack()` 5.80 seconds
   * `msgpack.unpack(msgpack.pack())` 8.62 seconds

Note that `node-msgpack` produces and consumes Buffer objects, and a such does
not incur encoding/decoding overhead when performing I/O with native strings.

### Usage

This module provides two methods: `pack()`, which consumes a JavaScript object
and produces a node Buffer object; and `unpack()`, which consumes a node Buffer
object and produces a JavaScript object. Packing of all native JavaScript types
(undefined, boolean, numbers, strings, arrays and objects) is supported, as
is the node Buffer type.

The below code snippet packs and then unpacks a JavaScript object, verifying
the resulting object at the end using `assert.deepEqual()`.

    var assert = require('assert');
    var msgpack = require('msgpack');

    var o = {"a" : 1, "b" : 2, "c" : [1, 2, 3]};
    var b = msgpack.pack(o);
    var oo = msgpack.unpack(b);

    assert.deepEqual(oo, o);

As a convenience, a higher level streaming API is provided in the
`msgpack.Stream` class, which can be constructed around a `net.Stream`
instance. This object emits `msg` events when an object has been received.

    var msgpack = require('msgpack');

    // ... get a net.Stream instance, s, from somewhere
    
    var ms = new msgpack.Stream(s);
    ms.addListener('msg', function(m) {
        sys.debug('received message: ' + sys.inspect(m));
    });

### Type Mapping

The JavaScript type system does not map cleanly on to the MsgPack type system,
though it's pretty close.

When packing, JavaScript values are mapped to MsgPack types as follows

   * `undefined` and `null` values map to `MSGPACK_OBJECT_NIL`
   * `boolean` values map to `MSGPACK_OBJECT_BOOLEAN`
   * `number` values map differently depending on their value
      * Floating point values map to `MSGPACK_OBJECT_DOUBLE`
      * Positive values map to `MSGPACK_OBJECT_POSITIVE_INTEGER`
      * Negative values map to `MSGPACK_OBJECT_NEGATIVE_INTEGER`
   * `string` values map to `MSGPACK_OBJECT_RAW`; all strings are serialized
     with UTF-8 encoding
   * Array values (as defined by `Array.isArray()`) map to
     `MSGPACK_OBJECT_ARRAY`; each element in the array is packed individually
     the rules in this list
   * NodeJS Buffer values map to `MSGPACK_OBJECT_RAW`
   * Everything else maps to `MSGPACK_OBJECT_MAP`, where we iterate over the object's
     properties and pack them and their values as per the mappings in this list

When unpacking, MsgPack types are mapped to JavaScript values as follows

   * `MSGPACK_OBJECT_NIL` values map to the `null` value
   * `MSGPACK_OBJECT_BOOLEAN` values map to `boolean` values
   * `MSGPACK_OBJECT_POSITIVE_INTEGER`, `MSGPACK_OBJECT_NEGATIVE_INTEGER` and
     `MSGPACK_OBJECT_DOUBLE` values map to `number` values
   * `MSGPACK_OBJECT_ARRAY` values map to arrays; each object in the array is
      packed individually using the rules in this list
   * `MSGPACK_OBJECT_RAW` values are mapped to `string` values; these values are
     unpacked using either UTF-8 or ASCII encoding, depending on the contents
     of the raw buffer
   * `MSGPACK_OBJECT_MAP` values are mapped to JavaScript objects; keys and values
     are unpacked individually using the rules in this list

Strings are particularly problematic here, as it's difficult to get hints down
into the packing and unpacking codepaths about how to interpret a particular
string or `MSGPACK_OBJECT_RAW`. If you have strict requirements about the
encoding of your strings, it's recommended that you populate a Buffer object
yourself (e.g. using `Buffer.write()`) and pack that buffer rather than the
string. This will ensure that you can control what gets packed.

When unpacking, things are trickier as there is no way to know the encoding
used when a string was packed. There is an [an open
ticket](http://github.com/msgpack/msgpack/issues/issue/13) for the MsgPack
format to address this.

### Command Line Utilities

As a convenience and for debugging, `bin/json2msgpack` and `bin/msgpack2json`
are provided to convert JSON data to and from MessagePack data, reading from
stdin and writing to stdout.

    % echo '[1, 2, 3]' | ./bin/json2msgpack | xxd -
    0000000: 9301 0203                                ....
    % echo '[1, 2, 3]' | ./bin/json2msgpack | ./bin/msgpack2json 
    [1,2,3]

### Building and installation

There are two ways to install msgpack.

## npm

		npm install msgpack

This should build and install msgpack for you. Then just `require('msgpack')`.

## Manually

Use `make` to build the add-on, then manually copy `build/default/mpBindings.node` 
and `lib/msgpack.js` it to wherever your node.js installation will look for it (or
add the build directory to your `$NODE_PATH`).

    % ls
    LICENSE  Makefile  README.md  deps/  src/  tags  test.js
    % make

The MessagePack library on which this depends is packaged with `node-msgpack`
and will be built as part of this process.

**Note:** MessagePack may fail to build if you do not have a modern version of
gcc in your `$PATH`. On Mac OS X Snow Leopard (10.5.x), you may have to use
`gcc-4.2`, which should come with your box but is not used by default.

    % make CC=gcc-4.2 CXX=gcc-4.2
