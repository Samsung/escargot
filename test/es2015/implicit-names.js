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

var g = function* () { };
var f = function () { };
var a = () => { };
var o = { a: function () { }, b: () => { }, c: function* () { } };
var d = f;

assert(o.name === undefined)
assert(o.a.name === "a")
assert(o.b.name === "b")
assert(o.c.name === "c")
assert(g.name === "g")
assert(a.name ==="a")
assert(f.name === "f")
assert(d.name === 'f')
assert(d.toString() === "function () { }");

var test = function t() {
    var g = function* () { };
    var f = function () { };
    var a = () => { };
    var o = { a: function () { }, b: () => { }, c: function* () { } };
    var d = a;

    assert(o.name === undefined)
    assert(o.a.name === "a")
    assert(o.b.name === "b")
    assert(o.c.name === "c")
    assert(g.name === "g")
    assert(a.name ==="a")
    assert(f.name === "f")
    assert(d.name === 'a')
    assert(d.toString() === "function () { }");

}
test();
assert(test.name === "t")

var s0 =  'function Z';
var s1 =  '\u02b1(Z';
var s2 =  '\u02b2, b) { }';
var sEval = s0 + s1 + s2;
eval(sEval);

assert(Z\u02b1.toString() === sEval);
