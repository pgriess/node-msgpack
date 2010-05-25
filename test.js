var assert = require('assert');
var msgpack = require('msgpack');

var test = function(v) {
    var vv = msgpack.unpack(msgpack.pack(v));

    assert.deepEqual(vv, v);
};

test('abcdef');
test(123);
test(null);
test(-1243.111);
test(-123);
test(true);
test(false);
test([1, 2, 3]);
test([1, 'abc', false, null]);
test({'a' : [1, 2, 3], 'b' : 'cdef', 'c' : {'nuts' : 'qqq'}});
