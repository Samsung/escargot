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

function isSyntaxError(str) {
  try {
    eval(str);
    assert(false);
  } catch (e) {
    assert(e instanceof SyntaxError);
  }
}

// Array destructuring

// Nested array destructuring
var array = [1, 2, [3, [4]], 5, 6, , 7];

var [a, b, [c, [d]], ...e] = array;
assert(a === 1);
assert(b === 2);
assert(c === 3);
assert(d === 4);
assert(e[0] === 5);
assert(e[1] === 6);
assert(e[2] === undefined);
assert(e[3] === 7);

// Basic variable assignment
var foo = ['one', 'two', 'three'];

var [one, two, three] = foo;
assert(one === 'one');
assert(two === 'two');
assert(three === 'three');

// Assignment separate from declaration
var a, b;

[a, b] = [1, 2];
assert(a === 1);
assert(b === 2);

// Array destructuring with defaults
[a = 6, b = 7, c = 8] = [1, , undefined];
assert(a === 1);
assert(b === 7);
assert(c === 8);

// Swapping Variables
var a = 1;
var b = 3;

[a, b] = [b, a];

assert(a === 3);
assert(b === 1);

// Function default argument
function defaultArrayArgument ([a,b,c] = [1,2,3]) {
  return a + b + c
}

assert(defaultArrayArgument() === 6);
assert(defaultArrayArgument([4, 5, 6]) === 15);

// Array destructuring syntax check
isSyntaxError("var [x, ...y , x]");
isSyntaxError("var [x, x]");
isSyntaxError("(function ([x, x]) {})");
isSyntaxError("(function ([x, y], x) {})");

// Object destructuring

// Basic assignment
var o = {p: 42, q: true};
var {p, q} = o;

assert(p === 42);
assert(q === true);


// Assignment without declaration
var a, b;

({a, b} = {a: 1, b: 2});

assert(a === 1);
assert(b === 2);

// Assingment to new variable names
var o = {p: 42, q: true};
var {p: foo, q: bar} = o;

assert(foo === 42);
assert(bar === true);

// Default values
var {a = 10, b = 5} = {a: 3};

assert(a === 3);
assert(b === 5);

// Aliasing
var {a: aa = 10, b: bb = 5} = {a: 3};
assert(aa === 3);
assert(bb === 5);

// Function parameters default value
function f({size = 'big', coords = {x: 0, y: 0}, radius = 25} = {}) {
    assert(size === 'big');
    assert(coords.x === 0);
    assert(coords.y === 0);
    assert(radius === 25);
}

function g({size = 'big', coords = {x: 0, y: 0}, radius = 25} = {}) {
    assert(size === 'big');
    assert(coords.x === 18);
    assert(coords.y === 30);
    assert(radius === 30);
}

f();
g({
  coords: {x: 18, y: 30},
  radius: 30
});


// Nested object and array destructuring
var metadata = {
  title: 'Scratchpad',
  translations: [
    {
      locale: 'de',
      localization_tags: [],
      last_edit: '2014-04-14T08:43:37',
      url: '/de/docs/Tools/Scratchpad',
      title: 'JavaScript-Umgebung'
    }
  ],
  url: '/en-US/docs/Tools/Scratchpad'
};

var {
  title: englishTitle, // rename
  translations: [
    {
       title: localeTitle, // rename
    },
  ],
} = metadata;

assert(englishTitle === 'Scratchpad');
assert(localeTitle === 'JavaScript-Umgebung');

// For of iteration and destructuring
var people = [
  {
    name: 'Mike Smith',
    family: {
      mother: 'Jane Smith',
      father: 'Harry Smith',
      sister: 'Samantha Smith'
    },
    age: 35
  },
  {
    name: 'Tom Jones',
    family: {
      mother: 'Norah Jones',
      father: 'Richard Jones',
      brother: 'Howard Jones'
    },
    age: 25
  }
];

var idx = 0;
for (var {name: n, family: {father: f}} of people) {
  if (idx === 0) {
    assert(n === 'Mike Smith');
    assert(f === 'Harry Smith');
  } else {
    assert(n === 'Tom Jones');
    assert(f === 'Richard Jones');
  }
  idx++;
}

// Unpacking fields from objects passed as function parameterSection
function userId({id}) {
  return id;
}

function whois({displayName, fullName: {firstName: name}}) {
  assert(displayName === 'jdoe');
  assert(name === 'John');
}

var user = {
  id: 42,
  displayName: 'jdoe',
  fullName: {
      firstName: 'John',
      lastName: 'Doe'
  }
};

assert(userId(user) === 42);
whois(user);

// Computed object property names and destructuring
var key = 'z';
var {[key]: foo} = {z: 'bar'};

assert(foo === 'bar');

// Invalid JavaScript identifier as a property name
var foo = { 'fizz-buzz': true };
var { 'fizz-buzz': fizzBuzz } = foo;

assert(fizzBuzz === true);

// Combined Array and Object Destructuring
var props = [
  { id: 1, name: 'Fizz'},
  { id: 2, name: 'Buzz'},
  { id: 3, name: 'FizzBuzz'}
];

var [,, { name }] = props;

assert(name === 'FizzBuzz');

// Object destructuring syntax check
isSyntaxError("(function ({x, x}) {})");
isSyntaxError("(function ({x, y}, x) {})");
isSyntaxError("(function ({x : a, y : a}, b) {})");
