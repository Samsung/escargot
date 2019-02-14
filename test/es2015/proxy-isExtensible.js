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

var monster1 = {
canEvolve: true
};

var handler1 = {
    isExtensible(target) {
        return Reflect.isExtensible(target);
    },
    preventExtensions(target) {
        target.canEvolve = false;
        return Reflect.preventExtensions(target);
    }
};

var proxy1 = new Proxy(monster1, handler1);

assert(Object.isExtensible(proxy1));
// expected output: true

assert(monster1.canEvolve);
// expected output: true

Object.preventExtensions(proxy1);

assert(Object.isExtensible(proxy1) === false);
// expected output: false

assert(monster1.canEvolve === false);
// expected output: false

