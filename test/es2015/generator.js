/* Copyright JS Foundation and other contributors, http://js.foundation
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

function* generator1(i) {
  yield i;
  yield i + 10;
}

var gen = generator1(10);

assert(gen.next().value === 10);
assert(gen.next().value === 20);

// Simple example
function* idMaker() {
  var index = 0;
  while (index < index+1)
    yield index++;
}

var gen = idMaker();

for (var i = 0; i < 10; i++) {
  assert(gen.next().value === i);
}

var console = {log:print};

// with yield*
function* anotherGenerator(i) {
  yield i + 1;
  yield i + 2;
  yield i + 3;
}

function* generator(i) {
  yield i;
  yield* anotherGenerator(i);
  yield i + 10;
}

var gen = generator(10);

assert(gen.next().value === 10);
assert(gen.next().value === 11);
assert(gen.next().value === 12);
assert(gen.next().value === 13);
assert(gen.next().value === 20);

// Passing arguments into Generators
var counter = 0;
function logger (a,b) {
  a;
  b;
  counter++;
}

function* logGenerator() {
  counter++;
  logger(1, yield);
  assert(counter === 2);
  counter++;
  logger(2, yield);
  assert(counter === 4);
  counter++;
  logger(3, yield);
  assert(counter === 6);
  counter++;
}

var gen = logGenerator();

gen.next();
assert(counter === 1);
gen.next('pretzel');
assert(counter === 3);
gen.next('california');
assert(counter === 5);
gen.next('mayonnaise');
assert(counter === 7);

// Return statement in a generator
function* yieldAndReturn() {
  yield "Y";
  return "R";
  yield "unreachable";
}

var gen = yieldAndReturn()
assert(gen.next().value === "Y");
assert(gen.next().value === "R");
assert(gen.next().done === true);

// Generator as an object property
var someObj = {
  *generator () {
    yield 'a';
    yield 'b';
  }
}

var gen = someObj.generator();
assert(gen.next().value === 'a');
assert(gen.next().value === 'b');
assert(gen.next().done === true);

// Generator as an object methodSection
{
  class Foo {
    *generator () {
      yield 1;
      yield 2;
      yield 3;
    }
  }

  var f = new Foo ();
  var gen = f.generator();

  assert(gen.next().value === 1);
  assert(gen.next().value === 2);
  assert(gen.next().value === 3);
  assert(gen.next().done === true);
}

{
  // Generator as a computed property
  class Foo {
    *[Symbol.iterator] () {
      yield 1;
      yield 2;
    }
  }

  var array = Array.from(new Foo);
  assert(array.length === 2);
  assert(array[0] === 1);
  assert(array[1] === 2);
}

var SomeObj = {
  *[Symbol.iterator] () {
    yield 'a';
    yield 'b';
  }
}

array = Array.from(SomeObj);
assert(array.length === 2);
assert(array[0] === 'a');
assert(array[1] === 'b');

// Generators are not constructable
try {
  function* f() {}
  var obj = new f;
  assert(false);
} catch (e) {
  assert (e instanceof TypeError);
}

// Generator defined in an expression
var foo = function* () {
  yield 10;
  yield 20;
};

var bar = foo();
assert(bar.next().value);

// Delegating to another generator
function* g1() {
  yield 2;
  yield 3;
  yield 4;
}

function* g2() {
  yield 1;
  yield* g1();
  yield 5;
}

var iterator = g2();
assert(iterator.next().value === 1);
assert(iterator.next().value === 2);
assert(iterator.next().value === 3);
assert(iterator.next().value === 4);
assert(iterator.next().value === 5);
assert(iterator.next().done === true);

// Other Iterable objects
function* g3() {
  yield* [1, 2];
  yield* '34';
  yield* Array.from(arguments);
}

var iterator = g3(5, 6);

assert(iterator.next().value === 1); // {value: 1, done: false}
assert(iterator.next().value === 2); // {value: 2, done: false}
assert(iterator.next().value === "3"); // {value: "3", done: false}
assert(iterator.next().value === "4"); // {value: "4", done: false}
assert(iterator.next().value === 5); // {value: 5, done: false}
assert(iterator.next().value === 6); // {value: 6, done: false}
assert(iterator.next().done === true); // {value: undefined, done: true}

// The value of yield* expression itself
function* g4() {
  yield* [1, 2, 3];
  return 'foo';
}

var result;

function* g5() {
  result = yield* g4();
}

var iterator = g5();

assert(iterator.next().value === 1);
assert(iterator.next().value === 2);
assert(iterator.next().value === 3);
assert(iterator.next().done === true);
assert(result === 'foo');

// %GeneratorPrototype%.return
function* g6() {
  yield* [1, 2, 3];
  return 'foo';
}

var iterator = g6();

assert(iterator.next().value === 1);
assert(iterator.return().done === true);
assert(iterator.next().done === true);

var iterator = g6();
assert(iterator.next().value === 1);
assert(iterator.return(5).value === 5);
assert(iterator.next().done === true);

// %GeneratorPrototype%.throw
var iterator = g6();
assert(iterator.next().value === 1);
try {
  iterator.throw();
  assert(false);
} catch (e) {
  assert (e === undefined);
}
assert(iterator.next().done === true);

var iterator = g6();
assert(iterator.next().value === 1);
try {
  iterator.throw(5);
  assert(false);
} catch (e) {
  assert (e === 5);
}
assert(iterator.next().done === true);
