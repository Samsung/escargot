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

function FunctionWithInnerArrowFunctionWithBlock(array) {
  return array.map((row) =>
    row.map((col) => {
      return row + col;
    })
  )
}

function FunctionWithArrowFunctionsWithoutBlocks(array) {
  return array.map((row) => row.map((col) => (row + col)))
}

var array1 = [[1,2,3], [4,5,6], [7,8,9]];
var expected = '[["1,2,31","1,2,32","1,2,33"],["4,5,64","4,5,65","4,5,66"],["7,8,97","7,8,98","7,8,99"]]';

assert(JSON.stringify(FunctionWithInnerArrowFunctionWithBlock(array1)) === expected);
assert(JSON.stringify(FunctionWithArrowFunctionsWithoutBlocks(array1)) === expected);
