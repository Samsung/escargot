// finishes well under both limits: must not be affected; long enough to time
var s = 0; for (var i = 0; i < 100000000; i++) { s += i & 7; }
print("sum=" + s);
