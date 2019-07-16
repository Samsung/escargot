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

var key1 = {
  toString: function() {
    return 'b';
  }
};
var key2 = {
  toString: function() {
    return 'd';
  }
};
class TestClass {
  a() { return 'A'; }
  [key1]() { return 'B'; }
  c() { return 'C'; }
  [key2]() { return 'D'; }
}

var expect1 = [];
var expect2 = ['constructor', 'a', 'b', 'c', 'd'];

var test1 = Object.keys(TestClass.prototype);
assert(test1.length === expect1.length);
for (var i = 0 ; i < expect1.length; ++i) {
    assert(test1[i]);
    assert(test1[i] === expect1[i]);
}

var test2 = Object.getOwnPropertyNames(TestClass.prototype);
assert(test2.length === expect2.length);
for (var i = 0 ; i < expect2.length; ++i) {
    assert(test2[i] === expect2[i]);
}
