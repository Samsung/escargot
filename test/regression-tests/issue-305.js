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

var args = [undefined, null, [], true, false, 3.14];
var expectedFloatType = [NaN ,0, 0, 1, 0, true];
var expectedIntegralTyped = [0 ,0, 0, 1, 0, 3];

var constructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];

function test(constructor) {

  var typedArray = new constructor(args);

  for (var i = 0 ; i < typedArray.length; i++) {
    if (constructor === Float32Array || constructor === Float64Array) {
        if (i == 0) {
            assert(isNaN(expectedFloatType[i]) === isNaN(typedArray[i]));
        } else if (i == 5) {
            assert(expectedFloatType[i] === Math.abs(typedArray[5] - 3.14) < 1e-6);
        } else {
            assert(expectedFloatType[i] === typedArray[i]);
        }
    } else {
        assert(expectedIntegralTyped[i] === typedArray[i]);
    }
  }
}

for (var constructor of constructors) {
  test(constructor);
}
