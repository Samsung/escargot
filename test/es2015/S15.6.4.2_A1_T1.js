// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype.valueOf() returns this boolean value
esid: sec-boolean.prototype.valueof
description: calling with argument
---*/

//CHECK#1
if (Boolean.prototype.toString() !== "false") {
    assertNotReached('#1: Boolean.prototype.toString() === "false"');
}

//CHECK#2
if ((new Boolean()).toString() !== "false") {
    assertNotReached('#2: (new Boolean()).toString() === "false"');
}

//CHECK#3
if ((new Boolean(false)).toString() !== "false") {
    assertNotReached('#3: (new Boolean(false)).toString() === "false"');
}

//CHECK#4
if ((new Boolean(true)).toString() !== "true") {
    assertNotReached('#4: (new Boolean(true)).toString() === "true"');
}

//CHECK#5
if ((new Boolean(1)).toString() !== "true") {
    assertNotReached('#5: (new Boolean(1)).toString() === "true"');
}

//CHECK#6
if ((new Boolean(0)).toString() !== "false") {
    assertNotReached('#6: (new Boolean(0)).toString() === "false"');
}

//CHECK#7
if ((new Boolean(new Object())).toString() !== "true") {
    assertNotReached('#7: (new Boolean(new Object())).toString() === "true"');
}
