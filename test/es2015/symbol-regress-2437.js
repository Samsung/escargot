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

function assertEquals(expectedResult, value)
{
  assert(expectedResult == value);
}


 // Test Regexp.prototype.exec
 r = /a/;
 r.lastIndex = 1;
 r.exec("zzzz");
 assertEquals(1, r.lastIndex);

 // Test Regexp.prototype.test
 r = /a/;
 r.lastIndex = 1;
 r.test("zzzz");
 assertEquals(1, r.lastIndex);

 // Test String.prototype.match
 r = /a/;
 r.lastIndex = 1;
 "zzzz".match(r);
 assertEquals(1, r.lastIndex);

 // Test String.prototype.replace with atomic regexp and empty string.
 r = /a/;
 r.lastIndex = 1;
 "zzzz".replace(r, "");
 assertEquals(1, r.lastIndex);

 // Test String.prototype.replace with non-atomic regexp and empty string.
 r = /\d/;
 r.lastIndex = 1;
 "zzzz".replace(r, "");
 assertEquals(1, r.lastIndex);

 // Test String.prototype.replace with atomic regexp and non-empty string.
 r = /a/;
 r.lastIndex = 1;
 "zzzz".replace(r, "a");
 assertEquals(1, r.lastIndex);

 // Test String.prototype.replace with non-atomic regexp and non-empty string.
 r = /\d/;
 r.lastIndex = 1;
 "zzzz".replace(r, "a");
 assertEquals(1, r.lastIndex);

 // Test String.prototype.replace with replacement function
 r = /a/;
 r.lastIndex = 1;
 "zzzz".replace(r, function() { return ""; });
 assertEquals(1, r.lastIndex);

 // Regexp functions that returns multiple results:
 // A global regexp always resets lastIndex regardless of whether it matches.
 r = /a/g;
 r.lastIndex = -1;
 "0123abcd".replace(r, "x");
 assertEquals(0, r.lastIndex);

 r.lastIndex = -1;
 "01234567".replace(r, "x");
 assertEquals(0, r.lastIndex);

 r.lastIndex = -1;
 "0123abcd".match(r);
 assertEquals(0, r.lastIndex);

 r.lastIndex = -1;
 "01234567".match(r);
 assertEquals(0, r.lastIndex);

 // A non-global regexp resets lastIndex iff it does not match.
 r = /a/;
 r.lastIndex = -1;
 "0123abcd".replace(r, "x");
 assertEquals(-1, r.lastIndex);

 r.lastIndex = -1;
 "01234567".replace(r, "x");
 assertEquals(-1, r.lastIndex);

 r.lastIndex = -1;
 "0123abcd".match(r);
 assertEquals(-1, r.lastIndex);

 r.lastIndex = -1;
 "01234567".match(r);
 assertEquals(-1, r.lastIndex);

 // Also test RegExp.prototype.exec and RegExp.prototype.test
 r = /a/g;
 r.lastIndex = 1;
 r.exec("01234567");
 assertEquals(0, r.lastIndex);

 r.lastIndex = 1;
 r.exec("0123abcd");
 assertEquals(5, r.lastIndex);

 r = /a/;
 r.lastIndex = 1;
 r.exec("01234567");
 assertEquals(1, r.lastIndex);

 r.lastIndex = 1;
 r.exec("0123abcd");
 assertEquals(1, r.lastIndex);

 r = /a/g;
 r.lastIndex = 1;
 r.test("01234567");
 assertEquals(0, r.lastIndex);

 r.lastIndex = 1;
 r.test("0123abcd");
 assertEquals(5, r.lastIndex);

 r = /a/;
 r.lastIndex = 1;
 r.test("01234567");
 assertEquals(1, r.lastIndex);

 r.lastIndex = 1;
 r.test("0123abcd");
 assertEquals(1, r.lastIndex);
