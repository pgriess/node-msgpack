`node-msgpack` is an addon for [node.js](http://nodejs.org) that provides an
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
object and produces a JavaScript object.

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

### Command Line Utilities

As a convenience and for debugging, `bin/json2msgpack` and `bin/msgpack2json`
are provided to convert JSON data to and from MessagePack data, reading from
stdin and writing to stdout.

    % echo '[1, 2, 3]' | ./bin/json2msgpack | xxd -
    0000000: 9301 0203                                ....
    % echo '[1, 2, 3]' | ./bin/json2msgpack | ./bin/msgpack2json 
    [1,2,3]

### Building and installation

This module depends on an unreleased feature of node.js; you must build it
against a tree that contains change
[3768aaae](http://github.com/ry/node/commit/3768aaaea406dd32db2f74395f6414bcd26dba74).
An official release that contains this change should be out shortly.

Installation is a manual process: use `make` to build the add-on, then manually
copy it to wherever your node.js installation will look for it (or add the
build directory to your `$NODE_PATH`).

    % ls
    LICENSE  Makefile  README.md  deps/  src/  tags  test.js
    % make

The MessagePack library on which this depends is packaged with `node-msgpack`
and will be built as part of this process.

**Note:** MessagePack may fail to build if you do not have a modern version of
gcc in your `$PATH`. On Mac OS X Snow Leopard (10.5.x), you may have to use
`gcc-4.2`, which should come with your box but is not used by default.

    % make CC=gcc-4.2 CXX=gcc-4.2
