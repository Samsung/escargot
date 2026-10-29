// each worker thread gets its own GC heap; check the main thread's heap is untouched
function mb(x) { return (x / 1048576).toFixed(1); }
var before = heapSize();
evalOnThreadAndWait("var k=[]; for (var i=0;i<300000;i++) k.push({i:i, s:'x'+i}); print('worker heap MB: ' + (heapSize()/1048576).toFixed(1));", 2);
gc();
print("main heap MB before=" + mb(before) + " after workers exit=" + mb(heapSize()));
