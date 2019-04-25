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

var assert = print;

assert(RegExp.prototype.hasOwnProperty(Symbol.split));
assert(RegExp.prototype[Symbol.split] instanceof Function);
assert(RegExp.prototype[Symbol.split].name == "[Symbol.split]");

function assertEquals(a, b) {
    assert(JSON.stringify(a) == JSON.stringify(b));
}

// some corner cases
assertEquals("y".split(/(x)?\1y/), ["", undefined, ""]) ;
assertEquals("y".split(/(x)?y/), ["", undefined, ""]);
assertEquals("abc".split(/$/), ["abc"]);
assertEquals("a1b2c3".split(/\d/), ["a", "b", "c", ""]);
assertEquals("1a2b3c".split(/\d/), ["", "a", "b", "c"]);
assertEquals("test".split(/(?:)/, -1), ["t", "e", "s", "t"]);

function SplitTest1(value) {
    this.value = value;
}

SplitTest1.prototype[Symbol.split] = function(string) {
    var index = string.indexOf(this.value);
    return string.substr(0, index) + this.value + "/" + string.substr(index + this.value.length);
}

var str = new String("foobar");
assert(str.split(new SplitTest1("foo")) == "foo/bar");
assert(str.split(new SplitTest1("o")) == "fo/obar");
assert(str.split(new SplitTest1("foobar")) == "foobar/");
assert(str.split(new SplitTest1("")) == "/foobar");
