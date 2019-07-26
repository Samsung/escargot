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

var assertCount = 0;
function assertAndCount(e) {
    assert(e)
    assertCount++;
}

var count = 0;

for (let x = 0; x < 5;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
    continue;
  }
}
assertAndCount(count == 10)

count = 0;
for (let x = 0; x < 5;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
    try {
      continue;
    } catch(e) {}
  }
}
assertAndCount(count == 10)


function fun() {

  count = 0;
  for (let x = 0; x < 5;) {
    x++;
    for (let y = 0; y < 2;) {
      y++;
      count++;
      continue;
    }
  }
  assertAndCount(count == 10)

  count = 0;
  for (let x = 0; x < 5;) {
    x++;
    for (let y = 0; y < 2;) {
      y++;
      count++;
      try {
          continue;
      } catch(e) {}
    }
  }
  assertAndCount(count == 10)
}

fun();


count = 0;
for (let x = 0; x < 5;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++; function capture() { y }
    continue;
  }
}
assertAndCount(count == 10)

count = 0;
for (let x = 0; x < 5;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++; function capture() { y }
    try {
      continue;
    } catch(e) {}
  }
}
assertAndCount(count == 10)


function fun2() {

  count = 0;
  for (let x = 0; x < 5;) {
    x++;
    for (let y = 0; y < 2;) {
      y++;
      count++; function capture() { y }
      continue;
    }
  }
  assertAndCount(count == 10)

  count = 0;
  for (let x = 0; x < 5;) {
    x++;
    for (let y = 0; y < 2;) {
      y++;
      count++; function capture() { y }
      try {
          continue;
      } catch(e) {}
    }
  }
  assertAndCount(count == 10)
}

fun2();

count = 0;
for (let x = 0; x < 5;) {
  x++;
  for (let y = 0; y < 2;) {
    y++;
    count++;
    try {
      break;
    } catch(e) {}
  }
}
assertAndCount(count == 5)


assert(assertCount == 9)
