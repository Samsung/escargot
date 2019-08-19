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

var handler = {
    setPrototypeOf: function (target, proto) {
        target.geneticallyModified = true;
        return false;
    }
};
var monsterProto = {};

var monster1 = {
    geneticallyModified: false
};
var proxy1 = new Proxy(monster1, handler);

assertThrows('Object.setPrototypeOf(proxy1, monsterProto)'); // throws a TypeError
assert(monster1.geneticallyModified); // expected output: true

var monster2 = {
    geneticallyModified: false
};
var proxy2 = new Proxy(monster2, handler);

Reflect.setPrototypeOf(proxy2, monsterProto) // Don't throws a TypeError
assert(monster2.geneticallyModified); // expected output: true
