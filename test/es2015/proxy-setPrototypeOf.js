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

var handler1 = {
setPrototypeOf : function (monster1, monsterProto) {
                     monster1.geneticallyModified = true;
                     return false;
                 }
};

var monsterProto = {};
var monster1 = {
geneticallyModified : false
};

var proxy1 = new Proxy(monster1, handler1);
assertThrows('Object.setPrototypeOf(proxy1, monsterProto)'); // throws a TypeError

assert(monster1.geneticallyModified);
// expected output: true
