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

// Redeclaration
var redeclarations = [ "const a; const a;",
                       "var a; const a;",
                       "const a; var a;",
                       "const a; { var a; }",
                       "const a; { eval ('var a;') }",
                       "switch(1) { case 0: const foo; break; case 1: const foo; break; }",
                     ];

redeclarations.forEach(function(e) {
  assertThrows(e, SyntaxError);
});

assertThrows("const b;", SyntaxError);

// Single-statement context
var singleStatements = [ "for (;;) const a;",
                         "if (true) const a;",
                         "while (true) const a;",
                         "do const a; while(false);"
                       ];

singleStatements.forEach(function(e) {
  assertThrows(e, SyntaxError);
});

const MY_FAV = 7;

try {
  MY_FAV = 20;
  assert(false);
} catch (e) {
  assert (e instanceof TypeError);
}

assert(MY_FAV === 7);

if (MY_FAV === 7) {
    let MY_FAV = 20;
    assert (MY_FAV === 20);
}

assert(MY_FAV === 7);

const MY_OBJECT = {'key': 'value'};

try {
  MY_OBJECT = {'OTHER_KEY': 'value'};
  assert(false);
} catch (e) {
  assert (e instanceof TypeError);
}

MY_OBJECT.key = 'otherValue';
assert(MY_OBJECT.key === 'otherValue');

const MY_ARRAY = [];
MY_ARRAY.push('A');
assert(MY_ARRAY[0] === 'A');

try {
  MY_ARRAY = ['B'];
  assert(false);
} catch (e) {
  assert (e instanceof TypeError);
}

// Test for statement
var array = [0, 1, 2];
let n = 0;
for (const i = n; ; n++ ) {
  assert(i === 0);
  assert(array[n] === n);

  try {
    i = 7;
    assert(false);
  } catch (e) {
    assert(e instanceof TypeError);
  }

  if (n === array.length - 1) {
    break;
  }
}

try {
  print(i);
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}

// Test for of statement
for (const a of array) {
  assert(array.includes(a));
  try {
    a = 7;
    assert(false);
  } catch (e) {
    assert(e instanceof TypeError);
  }
}

try {
  print(a);
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}
