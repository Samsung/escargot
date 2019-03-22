var a = [1, 2];
var b = [5, 6];
var c = [9, 10];
var d = [0, ...a, 3, 4, ...b, 7, 8, ...c];

for (var i = 0; i < d.length; i++) {
  assert(d[i] === i);
}

var str = "foobar";
var e = [...str];

for (var i = 0; i < e.length; i++) {
  assert(e[i] === str[i]);
}

function f(a, b, c, d, e, f, g) {
  return a + b + c + d + e + f;
}

var args = [1, 2, 3, 4, 5, 6];
assert(f(10, ...args) === 25);

with ({}) {
  assert(f(0, ...args) === 15)
}

var str = ["5", "6"];
assert(eval(...str) === 5);

var g = eval;
assert(g(...str) === 5);

var array = [1, 2, 3];
var newArray = new Array(...array);
for (var i = 0; i < array.length; i++) {
    assert (array[i] == newArray[i]);
}
