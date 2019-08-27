// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype.valueOf() returns this boolean value
esid: sec-boolean.prototype.valueof
description: calling with argument
---*/

//CHECK#1
if (typeof Boolean.prototype !== "object") {
    assertNotReached('#1: typeof Boolean.prototype === "object"');
}

//CHECK#2
if (Boolean.prototype != false) {
    assertNotReached('#2: Boolean.prototype == false');
}

delete Boolean.prototype.toString;

if (Boolean.prototype.toString() !== "[object Boolean]") {
    assertNotReached('#3: The [[Class]] property of the Boolean prototype object is set to "Boolean"');
}
