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

(function TestForArray() {
    var iterable = [1, 2, 3, 4, 5];
    var result = 0;

    for (var value of iterable) {
        result += value;
    }

    assert(value === 5);
    assert(result === 15);
})();


(function TestForString() {
    var iterable = "boo";
    var result = "";

    for (var value of iterable) {
        result += value;
    }

    assert(value === "o");
    assert(result === "boo");
})();

(function TestForTypedArray() {
    var iterable = new Uint8Array([0x00, 0xff]);
    var result = 0;

    for (var value of iterable) {
        result += value;
    }

    assert(value === 255);
    assert(result === 255);
})();

(function TestForSet() {
    var iterable = new Set([1, 1, 2, 2, 3, 3]);
    var result = 0;

    for (value of iterable) {
        result += value;
    }

    assert(result === 6);
})();

(function TestForIterableObject() {
    var iterable = {
        [Symbol.iterator] : function() {
            return {
                i: 0,
                next: function() {
                    if (this.i < 3) {
                        return { value: this.i++, done: false };
                    }
                    return { value: undefined, done: true };
                }
            };
        }
    };
    var result = 0;

    for (value of iterable) {
        result += value;
    }

    assert(result === 3);
})();

