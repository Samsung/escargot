// infinite loop that also tries to swallow the termination
var n = 0;
try {
  while (true) { n++; }
} catch (e) {
  print("CAUGHT (should not happen): " + e);
} finally {
  print("finally ran, n=" + n);
}
print("after loop (should not happen)");
