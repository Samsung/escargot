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

function assert(expression) {
  if (!expression) {
    throw new Error("Assertion failed");
  }
}

function assertThrows(code, errorType) {
    try {
        if (typeof code === 'function') {
            code();
        } else {
            eval(code);
        }
    } catch (e) {
        if (errorType && !(e instanceof errorType)) {
            throw new Error("Expected exception has failed");
        }
        return;
    }
    throw new Error("Did not throw exception");
}
