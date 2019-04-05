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

function foo(x, y, z) {
      return [this, arguments.length, x];
}

var f = foo.bind(foo);
arr = f(1, 2, 3);
assert(foo === arr[0]);
assert(3 === arr[1]);
assert(1 === arr[2]);
assert(3 === f.length);
assert("function () { [native code] }" === f.toString());

function bar(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
}

f = bar.bind(bar, 1); 
obj1 = new f();
assert(obj1 instanceof bar);
assert(obj1 instanceof f);

f = bar.bind(bar, 1).bind(bar, 2).bind(bar, 3);
obj2 = new f();
assert(1 === obj2.x);
assert(2 === obj2.y);
assert(3 === obj2.z);
assert(obj2 instanceof bar);
assert(obj2 instanceof f);
