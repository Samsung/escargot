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

function checkNativeGetter(builtin) {
    assert(builtin[Symbol.species] === builtin);
}

// Check the property [Symbol.species] in builtin constructors.
checkNativeGetter(RegExp);
checkNativeGetter(Array);
checkNativeGetter(Map);
checkNativeGetter(Set);
checkNativeGetter(ArrayBuffer);
checkNativeGetter(Promise);

checkNativeGetter(Int8Array);
checkNativeGetter(Int16Array);
checkNativeGetter(Int32Array);
checkNativeGetter(Uint8Array);
checkNativeGetter(Uint8ClampedArray);
checkNativeGetter(Uint16Array);
checkNativeGetter(Uint32Array);
checkNativeGetter(Float32Array);
checkNativeGetter(Float64Array);
