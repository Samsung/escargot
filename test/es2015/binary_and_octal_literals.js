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

function must_throw_strict(str) {
  try {
    eval ("'use strict';" + str);
    assert(false);
  } catch (e) {
  }
}

//ES5 method
assert(parseInt("111110111", 2) === 503);
assert(parseInt("767", 8) === 503);

//ES6 literal method
assert(0b111110111 === 503)
assert(0B111110111 === 503)
assert(0o767 === 503)
assert(0O767 === 503)

//this should only work in non-strict as this is backward compatibility thing
assert(0767 === 503)
//test for throwing error in strict
must_throw_strict("0767 === 503")

//some "should not work" tests
//these should equal to 503 as shown above
assertThrows("assert(0b111110111 === 502)")
assertThrows("assert(0o767 === 502)")
//test for not existing literal markers
assertThrows("assert(0a767 === 502)")
assertThrows("assert(0C767 === 502)")
assertThrows("assert(0y767 === 502)")