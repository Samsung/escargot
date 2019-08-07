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

// Test for issue code
var test_array = new Int8Array(new Int16Array(777))
assert(test_array.length == 777)

// Test for Values
test_array = new Int8Array(new Int16Array([-1,16,32,64]))
assert(test_array.toString() == "-1,16,32,64")
test_array = new Int16Array(new Int8Array([-1,16,32,64]))
assert(test_array.toString() == "-1,16,32,64")

// Overflow
test_array = new Int8Array(new Int16Array([-1,16,32,64,128]))
assert(test_array.toString() == "-1,16,32,64,-128")
test_array = new Uint8Array(new Int16Array([-1,16,32,64,256]))
assert(test_array.toString() == "255,16,32,64,0")

// Signed to Unsigned
test_array = new Uint8Array(new Int16Array([-1,16,32,64]))
assert(test_array.toString() == "255,16,32,64")
