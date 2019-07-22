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

// pass if there is no compile error
function fo() {
  try {} catch(e) {}
  var e;
}

function foo() {
    try {
  } catch(e) {
    try {

    } catch(e2) {
        var e;
    }
  }
}


function F()
{
  e = 100;
  assert(e == 100)
  try
  {
      throw 200;
      assert(false)
  }
  catch(e)
  {
    assert(e == 200)
    var e = 300;
    assert(e == 300)
  }

  assert(e == 100)
}

F()

function F2()
{
  try
  {
      throw 200;
      assert(false)
  }
  catch(e)
  {
    var e;
  }

  assert(e === undefined)
}

F2()

try {
  eval("try {} catch(e) { function e() {} }")
  assert(false)
} catch (e) {
  assert(e instanceof SyntaxError)
}

try {
  eval("try {} catch([e]) { var e }")
  assert(false)
} catch (e) {
  assert(e instanceof SyntaxError)
}

