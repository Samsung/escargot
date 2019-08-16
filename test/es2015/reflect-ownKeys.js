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

var sym = Symbol.for('comet');
var sym2 = Symbol.for('meteor');
var obj = {
    [sym2]: 0, 'str': 0, '773': 0, '0': 0,
    [sym]: 0, '-1': 0, '8': 0, "": 0, 'second str': 0};

var expects = ["0", "8", "773", "str", "-1", "", "second str", sym2, sym];
var keys = Reflect.ownKeys(obj);

for(var i = 0; i < keys.length; i++){
    assert(keys[i] === expects[i])
}
