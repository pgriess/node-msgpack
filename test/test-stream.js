// Verify that msgpack.Stream can pass multiple messages around as expected.

var assert = require('assert');
var msgpack = require('msgpack');
var net = require('net');
var netBindings = process.binding('net');
var sys = require('sys');

var MSGS = [
    [1, 2, 3],
    {'a' : 1, 'b' : 2},
    {'test' : [1, 'a', 3]}
];
var fds = netBindings.socketpair();

var is = new net.Stream(fds[0]);
var ims = new msgpack.Stream(is);
var os = new net.Stream(fds[1]);
var oms = new msgpack.Stream(os);

var msgsReceived = 0;
ims.addListener('msg', function(m) {
    assert.deepEqual(m, MSGS[msgsReceived]);

    if (++msgsReceived == MSGS.length) {
        is.end();
        os.end();
    }
});
is.resume();

MSGS.forEach(function (m) {
    oms.send(m);
});
