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

function assertToString(actual, expected) {
    assert(actual.toString() == expected.toString());
}

var arr1 = [1,2,3];
var arr2 = ['a', 'b', 'c'];

var obj = {
    length: 3,
    0: 'd',
    1: 'e',
    2: 'f'
};

// The Symbol.isConcatSpreadable property should be undefined by default.
assert(arr1[Symbol.isConcatSpreadable] === undefined);
assert(arr2[Symbol.isConcatSpreadable] === undefined);

// If the property is undefined, it should be handled as true in case of Array objects,
// false in case of array-like objects.
var arr_concat1 = arr1.concat(arr2);
assertToString(arr_concat1, [1, 2, 3, 'a', 'b', 'c']);

var obj_concat1 = arr1.concat(obj);
assertToString(obj_concat1, [1, 2, 3, obj]);

// If Symbol.isConcatSpreadable is set to true in an object, that object should be spread out.
arr1[Symbol.isConcatSpreadable] = true;

var arr_concat2 = arr1.concat(arr2);
assertToString(arr_concat2, [1, 2, 3, 'a', 'b', 'c']);

obj[Symbol.isConcatSpreadable] = true;
var obj_concat2 = arr1.concat(obj);
assertToString(obj_concat2, [1, 2, 3, 'd', 'e', 'f']);

// If Symbol.isConcatSpreadable is set to false in an object, that object should not be spread out.
arr1[Symbol.isConcatSpreadable] = false;

var arr_concat3 = arr1.concat(arr2);
assertToString(arr_concat3, [[1, 2, 3], 'a', 'b', 'c']);

arr1[Symbol.isConcatSpreadable] = true;
arr2[Symbol.isConcatSpreadable] = false;

var arr_concat4 = arr1.concat(arr2);
assertToString(arr_concat4, [1, 2, 3, ['a', 'b', 'c']]);

obj[Symbol.isConcatSpreadable] = false;
var obj_concat3 = arr1.concat(obj);
assertToString(obj_concat3, [1, 2, 3, {length: 3, 0: 'd', 1: 'e', 2: 'f'}]);
