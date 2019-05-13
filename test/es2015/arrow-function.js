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

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Arrow functions are like functions, except they throw when using the
// "new" operator on them.
assert("function" === typeof (() => {}));
assert(Function.prototype === Object.getPrototypeOf(() => {}));
assertThrows(function() { new (() => {}); }, TypeError);
assert(!("prototype" in (() => {})));

// Check the different syntax variations
assert(1 === (() => 1)());
assert(2 === (a => a + 1)(1));
assert(3 === (() => { return 3; })());
assert(4 === (a => { return a + 3; })(1));
assert(5 === ((a, b) => a + b)(1, 4));
assert(6 === ((a, b) => { return a + b; })(1, 5));

// The following are tests from:
// http://wiki.ecmascript.org/doku.php?id=harmony:arrow_function_syntax

// Empty arrow function returns undefined
var empty = () => {};
assert(undefined === empty());

// Single parameter case needs no parentheses around parameter list
var identity = x => x;
assert(empty === identity(empty));

// No need for parentheses even for lower-precedence expression body
var square = x => x * x;
assert(9 === square(3));

// Parenthesize the body to return an object literal expression
var key_maker = val => ({key: val});
assert(empty === key_maker(empty).key);

// Statement body needs braces, must use 'return' explicitly if not void
var evens = [0, 2, 4, 6, 8];
var evenMap = evens.map(v => v + 1);
assert(evenMap[0] === 1);
assert(evenMap[1] === 3);
assert(evenMap[2] === 5);
assert(evenMap[3] === 7);
assert(evenMap[4] === 9);

var fives = [];
[1, 2, 3, 4, 5, 6, 7, 8, 9, 10].forEach(v => {
  if (v % 5 === 0) fives.push(v);
});
assert(fives[0] === 5);
assert(fives[1] === 10);

// v8:4474
(function testConciseBodyReturnsRegexp() {
  var arrow1 = () => /foo/
  var arrow2 = () => /foo/;
  var arrow3 = () => /foo/i
  var arrow4 = () => /foo/i;
  assert(arrow1.toString() === "() => /foo/");
  assert(arrow2.toString() === "() => /foo/");
  assert(arrow3.toString() === "() => /foo/i");
  assert(arrow4.toString() === "() => /foo/i");
});
