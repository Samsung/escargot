// keep allocating; soft limit should stop this with a RangeError
var keep = [];
try {
  for (var i = 0; ; i++) { keep.push(new Array(1024).fill(i)); }
} catch (e) {
  print("CAUGHT (should not happen): " + e);
}
