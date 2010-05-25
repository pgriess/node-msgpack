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

### Building and installation

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
