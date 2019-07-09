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

(function TestToLocaleStringCalls() {
  let log = [];
  let pushArgs = (label) => (...args) => log.push(label, args);

  let NumberToLocaleString = Number.prototype.toLocaleString;
  let StringToLocaleString = String.prototype.toLocaleString;
  let ObjectToLocaleString = Object.prototype.toLocaleString;
  Number.prototype.toLocaleString = pushArgs("Number");
  String.prototype.toLocaleString = pushArgs("String");
  Object.prototype.toLocaleString = pushArgs("Object");

  [42, "foo", {}].toLocaleString();

  let expected = ["Number", [], "String", [], "Object", []];
  for (let i = 0; i < log.length; i++) {
    print(expected[i] == log[i]);
  }
})();
