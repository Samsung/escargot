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

// Scoping rules
function varTest() {
  var x = 1;
  if (true) {
    var x = 2;
    assert(x === 2);
  }
  assert(x === 2);
}

function letTest() {
  let x = 1;
  if (true) {
    let x = 2;
    assert(x === 2);
  }
  assert(x === 1);
}

varTest();
letTest();

var x = 'global';
let y = 'global';
assert (this.x === 'global');
assert(this.y === undefined);

// Emulating private members

var console = {log:print};

var Thing;

{
  let privateScope = new WeakMap();
  let counter = 0;

  Thing = function() {
    this.someProperty = 'foo';

    privateScope.set(this, {
      hidden: ++counter,
    });
  };

  Thing.prototype.showPublic = function() {
    return this.someProperty;
  };

  Thing.prototype.showPrivate = function() {
    return privateScope.get(this).hidden;
  };
}

assert(typeof privateScope === "undefined");

var thing = new Thing();

assert(thing.someProperty === "foo");
assert(thing.showPublic() === "foo");
assert(thing.showPrivate() === 1);

// Redeclaration
var redeclarations = [ "let a; let a;",
                       "var a; let a;",
                       "let a; var a;",
                       "let a; { var a; }",
                       "let a; { eval ('var a;') }",
                       "switch(1) { case 0: let foo; break; case 1: let foo; break; }",
                     ];

redeclarations.forEach(function(e) {
  assertThrows(e, SyntaxError);
});

// Single-statement context
var singleStatements = [ "for (;;) let a;",
                         "if (true) let a;",
                         "while (true) let a;",
                         "do let a; while(false);"
                       ];

singleStatements.forEach(function(e) {
  assertThrows(e, SyntaxError);
});


switch(1) {
  case 0: {
    let foo;
    break;
  }
  case 1: {
    let foo;
    break;
  }
}

try {
  print(foo);
  assert(false);
} catch (e) {
  assert (e instanceof ReferenceError);
}

if (true) {
  let localBlockVariable = 5;
}

try {
  print(localBlockVariable);
  assert(false);
} catch (e) {
  assert (e instanceof ReferenceError);
}

// Test temporal dead zone
var tdz = [ "assert(bar === undefined); print(foo); assert (false); var bar = 1; let foo = 2;",
            "assert((typeof undeclaredVariable) === 'undefined'); print(typeof i); assert(false); let i = 10;",
            "var foo = 33; if (true) { let foo = (foo + 55); }",
            "function go(n) { for (let n of n.a) {  assert (false); } }; go({a: [1, 2, 3]});",
];

tdz.forEach(function(e) {
  assertThrows(e, ReferenceError);
});

// Other situations
var a = 1;
var b = 2;

if (a === 1) {
  var a = 11;
  let b = 22;

  assert(a === 11);
  assert(b === 22);
}

assert(a === 11);
assert(b === 2);

// Test for statement
var array = [0, 1, 2];
for (let i = 0; i < array.length; i++) {
  assert(array[i] === i);
}

try {
  print(i);
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}

// Test for of statement

for (let c of array) {
  try {
    assert(array.includes(c));
  } catch (e) {
    assert(e instanceof ReferenceError);
  }
  let c = 7;
  assert(c === 7);
}

try {
  print(c);
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}

// Test per-iteration block
let functionArray = [];
for (let i = 0; i < 5; i++) {
    let local = i + 2;
    functionArray[i] = function () { return [i, local] };
}

for (let i = 0; i < functionArray.length; i++) {
    let result = functionArray[i]();
    assert(result[0] === i);
    assert(result[1] === i + 2);
}
