// A stupid benchmarking tool.

var assert = require('assert');
var msgpack = require('msgpack');
var sys = require('sys');

var DATA_TEMPLATE = {'abcdef' : 1, 'qqq' : 13, '19' : [1, 2, 3, 4]};
var DATA = [];

for (var i = 0; i < 500000; i++) {
    DATA.push(JSON.parse(JSON.stringify(DATA_TEMPLATE)));
}

var testJSON = function(d) {
    var d = JSON.stringify(d)
    JSON.parse(d);
};

var testMP = function(d) {
    var d = msgpack.pack(d);
    msgpack.unpack(d);
};

DATA.forEach(testMP);
