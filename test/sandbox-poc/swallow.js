// tries every trick to swallow the termination
var n = 0;
function spin() { while (true) { n++; } }
try {
  try { spin(); } catch (e) { print("inner CAUGHT (bad): " + e); }
} catch (e) {
  print("outer CAUGHT (bad): " + e);
} finally {
  try { while (true) { n++; } } catch (e) { print("finally CAUGHT (bad): " + e); }
}
print("after (bad)");
