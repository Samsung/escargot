//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/* ES6 compatible test */

print("Test 1: Math.E");
var obj = Object.create(Math);
obj.E = "foo";
assert(obj.E === 2.718281828459045);

print("Test 2: function length");
function myFunc(a, b, c, d) {}
obj = Object.create(myFunc);
obj.length = "foo";
assert(obj.length === 4);

print("Test 3: Regular expression properties");
var regExp = new RegExp("/abc/g");
obj = Object.create(regExp);
obj.global = "foo";
try {
  obj.global;
} catch (e) {
  assert(e instanceof TypeError);
}

obj.lastIndex = "foo";
print(obj.lastIndex === "foo");

print("Test 4: String length");
obj = Object.create(new String("test"));
obj.length = "foo";
assert(obj.length === 4);
