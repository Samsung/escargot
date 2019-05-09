/* Copyright 2019-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var map = new Map();

var objectKey = {};
var stringKey = 'keykeykey';
var numberKey = 42.24;
var booleanKey = true;
var undefinedKey = undefined;
var nullKey = null;
var nanKey = NaN;
var zeroKey = 0;
var minusZeroKey = -0;

assert(map.size === 0);

map.set(objectKey, 'aaa');
map.set(stringKey, 'bbb');
map.set(numberKey, 'ccc');
map.set(booleanKey, 'ddd');
map.set(undefinedKey, 'eee');
map.set(nullKey, 'fff');
map.set(nanKey, 'ggg');
map.set(zeroKey, 'hhh');

assert(8 === map.size);

assert('aaa' === map.get(objectKey));
assert('bbb' === map.get(stringKey));
assert('ccc' === map.get(numberKey));
assert('ddd' === map.get(booleanKey));
assert('eee' === map.get(undefinedKey));
assert('fff' === map.get(nullKey));
assert('ggg' === map.get(nanKey));
assert('hhh' === map.get(zeroKey));

assert(undefined === map.get({}));
assert('bbb' === map.get('keykeykey'));
assert('ccc' === map.get(42.24));
assert('ddd' === map.get(true));
assert('eee' === map.get(undefined));
assert('fff' === map.get(null));
assert('ggg' === map.get(NaN));
assert('hhh' === map.get(0));
assert('hhh' === map.get(-0));
assert('hhh' === map.get(1 / Infinity));
assert('hhh' === map.get(-1 / Infinity));

var set = new Set();

var objectKey = {};
var stringKey = 'keykeykey';
var numberKey = 42.24;
var booleanKey = true;
var undefinedKey = undefined;
var nullKey = null;
var nanKey = NaN;
var zeroKey = 0;
var minusZeroKey = -0;

assert(set.size === 0);

set.add(objectKey);
set.add(stringKey);
set.add(numberKey);
set.add(booleanKey);
set.add(undefinedKey);
set.add(nullKey);
set.add(nanKey);
set.add(zeroKey);

assert(8 === set.size);

assert(set.has(objectKey));
assert(set.has(stringKey));
assert(set.has(numberKey));
assert(set.has(booleanKey));
assert(set.has(undefinedKey));
assert(set.has(nullKey));
assert(set.has(nanKey));
assert(set.has(zeroKey));

assert(!set.has({}));
assert(set.has('keykeykey'));
assert(set.has(42.24));
assert(set.has(true));
assert(set.has(undefined));
assert(set.has(null));
assert(set.has(NaN));
assert(set.has(0));
assert(set.has(-0));
assert(set.has(1 / Infinity));
assert(set.has(-1 / Infinity));

(function TestSetIteratorSymbol() {
  assert(Set.prototype[Symbol.iterator] === Set.prototype.values);
  assert(Set.prototype.hasOwnProperty(Symbol.iterator));
  assert(!Set.prototype.propertyIsEnumerable(Symbol.iterator));

  var iter = new Set().values();
  assert(iter === iter[Symbol.iterator]());
  assert(iter[Symbol.iterator].name === '[Symbol.iterator]');
})();

(function TestMapIteratorSymbol() {
  assert(Map.prototype[Symbol.iterator], Map.prototype.entries);
  assert(Map.prototype.hasOwnProperty(Symbol.iterator));
  assert(!Map.prototype.propertyIsEnumerable(Symbol.iterator));

  var iter = new Map().values();
  assert(iter === iter[Symbol.iterator]());
  assert(iter[Symbol.iterator].name === '[Symbol.iterator]');
})();
