var assert = require('assert');
var msgpack = require('msgpack');

var testEqual = function(v) {
    var vv = msgpack.unpack(msgpack.pack(v));

    assert.deepEqual(vv, v);
};

var testCircular = function(v) {
    try {
        msgpack.pack(v);
        assert.ok(false, 'expected exception');
    } catch (e) {
        assert.equal(
            e.message,
            'Cowardly refusing to pack object with circular reference'
        );
    }
}

testEqual('abcdef');
testEqual(123);
testEqual(null);
testEqual(-1243.111);
testEqual(-123);
testEqual(true);
testEqual(false);
testEqual([1, 2, 3]);
testEqual([1, 'abc', false, null]);
testEqual({'a' : [1, 2, 3], 'b' : 'cdef', 'c' : {'nuts' : 'qqq'}});

// Make sure we're catching circular references for arrays
var a = [1, 2, 3, 4];
a.push(a);
testCircular(a);

// Make sure we're catching circular references in objects
var d = {}
d.qqq = d;
testCircular(d);

// Make sure we can serialize the same object repeatedly and that our circular
// reference marking algorithm doesn't get in the way
var d = {};
for (var i = 0; i < 10; i++) {
    msgpack.pack(d);
}
