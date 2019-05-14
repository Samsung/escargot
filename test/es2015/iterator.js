// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var arrayIteratorPrototype = [].entries().__proto__;
var iteratorPrototype = arrayIteratorPrototype.__proto__;

assert(Object.prototype === Object.getPrototypeOf(iteratorPrototype));
assert(Object.isExtensible(iteratorPrototype));
assert(0 === Object.getOwnPropertyNames(iteratorPrototype).length);
assert(1 === Object.getOwnPropertySymbols(iteratorPrototype).length);
assert(Symbol.iterator ===
             Object.getOwnPropertySymbols(iteratorPrototype)[0]);

var descr = Object.getOwnPropertyDescriptor(iteratorPrototype, Symbol.iterator);
assert(descr.configurable);
assert(!descr.enumerable);
assert(descr.writable);

var iteratorFunction = descr.value;
assert('function' === typeof iteratorFunction);
assert(0 === iteratorFunction.length);
assert('[Symbol.iterator]' === iteratorFunction.name);

var obj = {};
assert(obj === iteratorFunction.call(obj));
assert(iteratorPrototype === iteratorPrototype[Symbol.iterator]());

var mapIteratorPrototype = new Map().entries().__proto__;
var setIteratorPrototype = new Set().values().__proto__;
var stringIteratorPrototype = 'abc'[Symbol.iterator]().__proto__;
assert(iteratorPrototype === mapIteratorPrototype.__proto__);
assert(iteratorPrototype === setIteratorPrototype.__proto__);
assert(iteratorPrototype === stringIteratorPrototype.__proto__);

var typedArrays = [
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
];

for (var constructor of typedArrays) {
  var array = new constructor();
  var iterator = array[Symbol.iterator]();
  assert(iteratorPrototype === iterator.__proto__.__proto__);
}
