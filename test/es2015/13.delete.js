//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var scenarios = [
 ["obj.foo", false],       // obj is configurable false
 ["'hello'[0]", false],
 ["'hello'.length", false],
 ["reg.source", true],
 ["reg.global", true],
 ["reg.lastIndex", false],
];

(function Test1(x) {
    var str = "delete configurable false property";

    var obj = new Object();
    Object.defineProperty(obj, "foo", { configurable: false, value: 20 });

    var reg = /foo/;

    for (var i = 0; i < scenarios.length; ++i) {
        var r = eval("delete " + scenarios[i][0]);
        assert(scenarios[i][1] === r);
    }
})();
