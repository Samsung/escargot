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
texture: 'scaly'
};

var handler1 = {
    deleteProperty(target, prop) {
        if (prop in target) {
            delete target[prop];
        }
    }
};

assert(monster1.texture === "scaly");
// expected output: "scaly"

var proxy1 = new Proxy(monster1, handler1);
delete proxy1.texture;

assert(monster1.texture === undefined);
// expected output: undefined
