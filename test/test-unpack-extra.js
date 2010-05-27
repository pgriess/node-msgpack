// Verify that unpacking a buffer with extra bytes doesn't lose the extra data

var assert = require('assert');
var msgpack = require('msgpack');
var buffer = require('buffer');

// Object to test with 
var o = [1, 2, 3];

// Create two buffers full of packed data, 'b' and 'bb', with the latter
// containing 3 extra bytes
var b = msgpack.pack(o);
var bb = new buffer.Buffer(b.length + 3);
b.copy(bb, 0, 0, b.length);

// Expect no remaining bytes when unpacking 'b'
assert.deepEqual(msgpack.unpack(b), o);
assert.deepEqual(msgpack.unpack.bytes_remaining, 0);

// Expect 3 remaining bytes when unpacking 'bb'
assert.deepEqual(msgpack.unpack(bb), o);
assert.equal(msgpack.unpack.bytes_remaining, 3);
