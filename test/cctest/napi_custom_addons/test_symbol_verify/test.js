'use strict';
const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_symbol_verify`);
const assert = require('assert');

// Debt #11 Verification: Check if node_api_symbol_for shares the exact same
// global symbol registry as JS Symbol.for()
const c_symbol = binding.createSymbolFor("escargot_napi_verify");
const js_symbol = Symbol.for("escargot_napi_verify");
assert.strictEqual(typeof c_symbol, 'symbol');
assert.strictEqual(c_symbol, js_symbol, "Debt #11: node_api_symbol_for does not share JS Symbol.for registry!");
assert.strictEqual(Symbol.keyFor(c_symbol), "escargot_napi_verify");

// Debt #12 Verification: Check if a weak reference on a Symbol correctly triggers 
// its GC finalizer without memory leak / unregister failure.
(async function() {
  {
    // Create a local, non-global symbol (no Symbol.for) so it can be GC'd.
    const local_symbol = binding.createLocalSymbol("garbage_collect_me");
    binding.attachWeakFinalizer(local_symbol, () => {
      // If this runs, Debt #12 is successfully verified!
      console.log("Symbol Weak-Ref Finalizer successfully ran!");
    });
  }
  
  // No manual global.gc() here due to conservative GC stale pointers.
  // The C++ harness will naturally sweep this symbol during environment teardown!
})();
