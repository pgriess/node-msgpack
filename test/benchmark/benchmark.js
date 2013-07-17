var fs      = require('fs'),
    sys     = require('sys'),
    msgpack = require("../../lib/msgpack"),
    stub    = require("../fixtures/stub");

var DATA_TEMPLATE = {'abcdef' : 1, 'qqq' : 13, '19' : [1, 2, 3, 4]};
var DATA = [];

for (var i = 0; i < 500000; i++) {
    DATA.push(JSON.parse(JSON.stringify(DATA_TEMPLATE)));
}

function _set_up(callback) {
  this.backup = {};
  callback();
}

function _tear_down(callback) {
  callback();
}

exports.benchmark = {
  setUp : _set_up,
  tearDown : _tear_down,
  'JSON.stringify no more than 6x faster than msgpack.pack' : function (test) {
    var jsonStr;
    var now = Date.now();
    DATA.forEach(function(d) {
        jsonStr = JSON.stringify(d);
    });
    var stringifyTime = (Date.now() - now);

    var mpBuf;
    now = Date.now();
    DATA.forEach(function(d) {
        mpBuf = msgpack.pack(d);
    });
    var packTime = (Date.now() - now);

    console.log(
      "msgpack.pack: "+packTime+"ms, JSON.stringify: "+stringifyTime+"ms"
    );
    console.log(
      "ratio of JSON.stringify/msgpack.pack: " + packTime/stringifyTime
    );
    test.expect(1);
    test.ok(
      packTime/stringifyTime < 6,
      "msgpack.pack: "+packTime+"ms, JSON.stringify: "+stringifyTime+"ms"
    );
    test.done();
  },
  'JSON.parse no more than 5x faster than msgpack.unpack' : function (test) {
    var jsonStr;
    DATA.forEach(function(d) {
        jsonStr = JSON.stringify(d);
    });

    var mpBuf;
    DATA.forEach(function(d) {
        mpBuf = msgpack.pack(d);
    });

    var now = Date.now();
    DATA.forEach(function(d) {
        JSON.parse(jsonStr);
    });
    var parseTime = (Date.now() - now);

    now = Date.now();
    DATA.forEach(function(d) {
        msgpack.unpack(mpBuf);
    });
    var unpackTime = (Date.now() - now);

    console.log(
      "msgpack.unpack: "+unpackTime+"ms, JSON.parse: "+parseTime+"ms"
    );
    console.log("ratio of JSON.parse/msgpack.unpack: " + unpackTime/parseTime);
    test.expect(1);
    test.ok(
      unpackTime/parseTime < 5,
      "msgpack.unpack: "+unpackTime+"ms, JSON.parse: "+parseTime+"ms"
    );
    test.done();
  },
  'output above is from three runs of benchmarks' : function (test) {
    console.log();
    for (var i = 0; i < 3; i++) {
      var mpBuf;
      var now = Date.now();
      DATA.forEach(function(d) {
        mpBuf = msgpack.pack(d);
      });
      console.log('msgpack pack:   ' + (Date.now() - now) + ' ms');
    
      now = Date.now();
      DATA.forEach(function(d) {
        msgpack.unpack(mpBuf);
      });
      console.log('msgpack unpack: ' + (Date.now() - now) + ' ms');
    
      var jsonStr;
      now = Date.now();
      DATA.forEach(function(d) {
        jsonStr = JSON.stringify(d);
      });
      console.log('json    pack:   ' + (Date.now() - now) + ' ms');
    
      now = Date.now();
      DATA.forEach(function(d) {
        JSON.parse(jsonStr);
      });
      console.log('json    unpack: ' + (Date.now() - now) + ' ms');
      console.log();
    }

    test.expect(1);
    test.ok(1);
    test.done();
  }
};
