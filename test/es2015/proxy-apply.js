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

function sum(a, b) {
    return a + b;
}

var handler = {
apply: function(target, thisArg, argumentsList) {
           return target(argumentsList[0], argumentsList[1]) * 10;
       }
};

var proxy1 = new Proxy(sum, handler);

assert(sum(1, 2) === 3);
// expected output: 3
assert(proxy1(1, 2) === 30);
// expected output: 30
