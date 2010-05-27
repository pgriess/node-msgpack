// Verify that a msgpack.Stream can handle partial messages arriving.

var assert = require('assert');
var buffer = require('buffer');
var msgpack = require('msgpack');
var net = require('net');
var netBindings = process.binding('net');
var sys = require('sys');

var MSGS = [
    [1, 2, 3],
    {'a' : 1, 'b' : 2},
    {'test' : [1, 'a', 3]}
];

// Write a buffer to a stream with a delay
var writemsg = function(s, buf, delay) {
    setTimeout(function() {
            sys.debug(sys.inspect(buf));
            s.write(buf);
        },
        delay
    );
};

var fds = netBindings.socketpair();

var is = new net.Stream(fds[0]);
var ims = new msgpack.Stream(is);
var os = new net.Stream(fds[1]);

var msgsReceived = 0;
ims.addListener('msg', function(m) {
    sys.debug('received msg: ' + sys.inspect(m));

    assert.deepEqual(m, MSGS[msgsReceived]);

    if (++msgsReceived == MSGS.length) {
        is.end();
        os.end();
    }
});
is.resume();

// Serialize the messages into a single large buffer
var buf = new buffer.Buffer(0);
MSGS.forEach(function (m) {
    var b = msgpack.pack(m);
    var bb = new buffer.Buffer(buf.length + b.length);
    buf.copy(bb, 0, 0, buf.length);
    b.copy(bb, buf.length, 0, b.length);

    buf = bb;
});

// Slice our single large buffer into 3 pieces and send them all off
// separately.
var nwrites = 3;
var sz = Math.ceil(buf.length / nwrites);
for (var i = 0; i < nwrites; i++) {
    writemsg(
        os,
        buf.slice(i * sz, Math.min((i + 1) * sz, buf.length)),
        (i + 1) * 1000
    );
}
