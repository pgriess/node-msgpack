var fs      = require('fs'),
    assert  = require('assert'),
    msgpack = require('../lib/msgpack');

var obj       = {a:123},
    fileName  = 'packed-data';
    packedObj = msgpack.pack(obj);

fs.writeFileSync(fileName, packedObj);
packedObj2 = fs.readFileSync(fileName);
assert.deepEqual(packedObj, packedObj2);
var unpackedObj = msgpack.unpack(packedObj2); // crash
assert.deepEqual(obj, unpackedObj);
