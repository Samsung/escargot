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

function  reportCompare(expectedResult, test)
{
  assert(expectedResult == test)
}

reportCompare(
  2,
  String.prototype.replace.length
);

reportCompare(
  "321",
  String.prototype.replace.call(123, "123", "321")
);

reportCompare(
  "ok",
  "ok".replace()
);

reportCompare(
  "undefined**",
  "***".replace("*")
);

reportCompare(
  "xnullz",
  "xyz".replace("y", null)
);

reportCompare(
  "x123",
  "xyz".replace("yz", 123)
);

reportCompare(
  "/x/g/x/g/x/g",
  "xxx".replace(/x/g, /x/g)
);

reportCompare(
  "ok",
  "undefined".replace(undefined, "ok")
);

reportCompare(
  "ok",
  "null".replace(null, "ok")
);

reportCompare(
  "ok",
  "123".replace(123, "ok")
);

reportCompare(
  "xzyxyz",
  "xyzxyz".replace("yz", "zy")
);

reportCompare(
  "ok",
  "(xyz)".replace("(xyz)", "ok")
);

reportCompare(
  "*$&yzxyz",
  "xyzxyz".replace("x", "*$$&")
);

reportCompare(
  "xy*z*",
  "xyz".replace("z", "*$&*")
);

reportCompare(
  "xyxyzxyz",
  "xyzxyzxyz".replace("zxy", "$`")
);

reportCompare(
  "zxyzxyzzxyz",
  "xyzxyz".replace("xy", "$'xyz")
);

reportCompare(
  "$",
  "xyzxyz".replace("xyzxyz", "$")
);

reportCompare(
  "x$0$00xyz",
  "xyzxyz".replace("yz", "$0$00")
);

// Result for $1/$01 .. $99 is implementation-defined if searchValue is no
// regular expression. $+ is a non-standard Mozilla extension.

reportCompare(
  "$!$\"$-1$*$#$.$xyz$$",
  "xyzxyz$$".replace("xyz", "$!$\"$-1$*$#$.$")
);

reportCompare(
  "$$$&$$$&$&",
  "$$$&".replace("$$", "$$$$$$&$&$$&")
);

reportCompare(
  "yxx",
  "xxx".replace(/x/, "y")
);

reportCompare(
  "yyy",
  "xxx".replace(/x/g, "y")
);

rex = /x/, rex.lastIndex = 1;
reportCompare(
  "yxx1",
  "xxx".replace(rex, "y") + rex.lastIndex
);

rex = /x/g, rex.lastIndex = 1;
reportCompare(
  "yyy0",
  "xxx".replace(rex, "y") + rex.lastIndex
);

rex = /y/, rex.lastIndex = 1;
reportCompare(
  "xxx1",
  "xxx".replace(rex, "y") + rex.lastIndex
);

rex = /y/g, rex.lastIndex = 1;
reportCompare(
  "xxx0",
  "xxx".replace(rex, "y") + rex.lastIndex
);

rex = /x?/, rex.lastIndex = 1;
reportCompare(
  "(x)xx1",
  "xxx".replace(rex, "($&)") + rex.lastIndex
);

rex = /x?/g, rex.lastIndex = 1;
reportCompare(
  "(x)(x)(x)()0",
  "xxx".replace(rex, "($&)") + rex.lastIndex
);

rex = /y?/, rex.lastIndex = 1;
reportCompare(
  "()xxx1",
  "xxx".replace(rex, "($&)") + rex.lastIndex
);

rex = /y?/g, rex.lastIndex = 1;
reportCompare(
  "()x()x()x()0",
  "xxx".replace(rex, "($&)") + rex.lastIndex
);

reportCompare(
  "xy$0xy$zxy$zxyz$zxyz",
  "xyzxyzxyz".replace(/zxy/, "$0$`$$$&$$$'$")
);

reportCompare(
  "xy$0xy$zxy$zxyz$$0xyzxy$zxy$z$z",
  "xyzxyzxyz".replace(/zxy/g, "$0$`$$$&$$$'$")
);

reportCompare(
  "xyxyxyzxyxyxyz",
  "xyzxyz".replace(/(((x)(y)()()))()()()(z)/g, "$01$2$3$04$5$6$7$8$09$10")
);

rex = RegExp(
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()(y)");
reportCompare(
  "x(y)z",
  "xyz".replace(rex, "($99)")
);

rex = RegExp(
  "()()()()()()()()()(x)" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()(y)");
reportCompare(
  "(x0)z",
  "xyz".replace(rex, "($100)")
);

reportCompare(
  "xyz(XYZ)",
  "xyzXYZ".replace(/XYZ/g, "($&)")
);

reportCompare(
  "(xyz)(XYZ)",
  "xyzXYZ".replace(/xYz/gi, "($&)")
);

reportCompare(
  "xyz\rxyz\n",
  "xyz\rxyz\n".replace(/xyz$/g, "($&)")
);

reportCompare(
  "(xyz)\r(xyz)\n",
  "xyz\rxyz\n".replace(/xyz$/gm, "($&)")
);

f = function () { return "failure" };

reportCompare(
  "ok",
  "ok".replace("x", f)
);

reportCompare(
  "ok",
  "ok".replace(/(?=k)ok/, f)
);

reportCompare(
  "ok",
  "ok".replace(/(?!)ok/, f)
);

reportCompare(
  "ok",
  "ok".replace(/ok(?!$)/, f)
);

f = function (sub, offs, str) {
  return ["", sub, typeof sub, offs, typeof offs, str, typeof str, ""]
    .join("|");
};

reportCompare(
  "x|y|string|1|number|xyz|string|z",
  "xyz".replace("y", f)
);

reportCompare(
  "x|(y)|string|1|number|x(y)z|string|z",
  "x(y)z".replace("(y)", f)
);

reportCompare(
  "x|y*|string|1|number|xy*z|string|z",
  "xy*z".replace("y*", f)
);

reportCompare(
  "12|3|string|2|number|12345|string|45",
  String.prototype.replace.call(1.2345e4, 3, f)
);

reportCompare(
  "|x|string|0|number|xxx|string|xx",
  "xxx".replace(/^x/g, f)
);

reportCompare(
  "xx|x|string|2|number|xxx|string|",
  "xxx".replace(/x$/g, f)
);

f = function (sub, paren, offs, str) {
  return ["", sub, typeof sub, paren, typeof paren, offs, typeof offs,
    str, typeof str, ""].join("|");
};

reportCompare(
  "xy|z|string|z|string|2|number|xyz|string|",
  "xyz".replace(/(z)/g, f)
);

reportCompare(
  "xyz||string||string|3|number|xyz|string|",
  "xyz".replace(/($)/g, f)
);

reportCompare(
  "|xy|string|y|string|0|number|xyz|string|z",
  "xyz".replace(/(?:x)(y)/g, f)
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|yz",
  "xyz".replace(/((?=xy)x)/g, f)
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|yz",
  "xyz".replace(/(x(?=y))/g, f)
);

reportCompare(
  "x|y|string|y|string|1|number|xyz|string|z",
  "xyz".replace(/((?!x)y)/g, f)
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|" +
    "|y|string||undefined|1|number|xyz|string|z",
  "xyz".replace(/y|(x)/g, f)
);

reportCompare(
  "xy|z|string||string|2|number|xyz|string|",
  "xyz".replace(/(z?)z/, f)
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/(z)?z/, f)
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/(z)?\1z/, f)
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/\1(z)?z/, f)
);

reportCompare(
  "xy|z|string||string|2|number|xyz|string|",
  "xyz".replace(/(z?\1)z/, f)
);

f = function (sub, paren1, paren2, offs, str) {
  return ["", sub, typeof sub, paren1, typeof paren1, paren2, typeof paren2,
    offs, typeof offs, str, typeof str, ""].join("|");
};

reportCompare(
  "x|y|string|y|string||undefined|1|number|xyz|string|z",
  "xyz".replace(/(y)(\1)?/, f)
);

reportCompare(
  "x|yy|string|y|string|y|string|1|number|xyyz|string|z",
  "xyyz".replace(/(y)(\1)?/g, f)
);

reportCompare(
  "x|y|string|y|string||undefined|1|number|xyyz|string|" +
    "|y|string|y|string||undefined|2|number|xyyz|string|z",
  "xyyz".replace(/(y)(\1)??/g, f)
);

reportCompare(
  "x|y|string|y|string|y|string|1|number|xyz|string|z",
  "xyz".replace(/(?=(y))(\1)?/, f)
);

reportCompare(
  "xyy|z|string||undefined||string|3|number|xyyz|string|",
  "xyyz".replace(/(?!(y)y)(\1)z/, f)
);

rex = RegExp(
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()(z)?(y)");
a = ["sub"];
for (i = 1; i <= 102; ++i)
  a[i] = "p" + i;
a[103] = "offs";
a[104] = "str";
a[105] = "return ['', sub, typeof sub, offs, typeof offs, str, typeof str, " +
  "p100, typeof p100, p101, typeof p101, p102, typeof p102, ''].join('|');";
f = Function.apply(null, a);
reportCompare(
  "x|y|string|1|number|xyz|string||string||undefined|y|string|z",
  "xyz".replace(rex, f)
);

reportCompare(
  "undefined",
  "".replace(/.*/g, function () {})
);

reportCompare(
  "nullxnullynullznull",
  "xyz".replace(/.??/g, function () { return null; })
);

reportCompare(
  "111",
  "xyz".replace(/./g, function () { return 1; })
);
