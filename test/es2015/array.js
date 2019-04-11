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

function assertEquals(actual, expected) {
    assert(JSON.stringify(actual) == JSON.stringify(expected))
}

// Array.prototype.splice
var arr = ["a"];
var elem = arr.splice(0);
assertEquals(arr, []);
assert(elem == "a");

arr = ["a"];
elem = arr.splice(0, 0);
assertEquals(arr, ["a"]);
assert(elem == "");

elem = arr.splice(0, 1);
assertEquals(arr, []);
assert(elem == "a");

arr = ["a"];
elem = arr.splice(0, 0, "b");
assertEquals(arr, ["b", "a"]);
assert(elem == "");

arr = ["a"];
elem = arr.splice(0, 2, "b", "c");
assertEquals(arr, ["b", "c"]);
assert(elem == "a");

elem = arr.splice(0, 2, "a");
assertEquals(arr, ["a"]);
assertEquals(elem, ["b", "c"]);

var obj = {0: "a", length: 1, splice: Array.prototype.splice};

elem = obj.splice(0);
assertEquals(obj, {length: 0});
assert(elem == "a");

obj = {0: "a", length: 1, splice: Array.prototype.splice};
elem = obj.splice(0, 0);
assertEquals(obj, {0: "a", length: 1});
assert(elem == "");

elem = obj.splice(0, 1);
assertEquals(obj, {length: 0});
assert(elem == "a");

obj = {0: "a", length: 1, splice: Array.prototype.splice};
elem = obj.splice(0, 0, "b");
assertEquals(obj, {0: "b", length: 2, 1: "a"});
assert(elem == "");

obj = {0: "a", length: 1, splice: Array.prototype.splice};
elem = obj.splice(0, 2, "b", "c");
assertEquals(obj, {0: "b", length: 2, 1: "c"});
assert(elem == "a");

elem = obj.splice(0, 2, "a");
assertEquals(obj, {0: "a", length: 1});
assertEquals(elem, ["b", "c"]);

// Array.prototype.slice
arr = ["a", "b", "c"]
assertEquals(arr.slice(), ["a", "b", "c"]);
assertEquals(arr.slice(1), ["b", "c"]);
assertEquals(arr.slice(0, 2), ["a", "b"]);

obj = {0: "a", 1: "b", 2: "c", length: 3, slice: Array.prototype.slice};
assertEquals(obj.slice(), ["a", "b", "c"]);
assertEquals(obj.slice(1), ["b", "c"]);
assertEquals(obj.slice(0, 2), ["a", "b"]);

// Array.prototype.filter
arr = ["foo", "bar", "foobar"];
var result = arr.filter(function(word) { return word.length > 3; });
assertEquals(result, ["foobar"]);

obj = {0: "foo", 1: "bar", 2: "foobar", length: 3, filter: Array.prototype.filter};
result = obj.filter(function(word) { return word.length > 3; });
assertEquals(result, ["foobar"]);

// Array.prototype.map
arr = [1, 2, 3, 4];
var map = arr.map(function(n) { return n * n; });
assertEquals(map, [1, 4, 9, 16]);

obj = {0: 1, 1: 2, 2: 3, 3: 4, length: 4, map: Array.prototype.map};
map = obj.map(function(n) { return n * n; });
assertEquals(map, [1, 4, 9, 16]);
