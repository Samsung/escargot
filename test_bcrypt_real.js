'use strict';

const printLog = (msg) => {
  if (typeof globalThis.print === 'function') {
    globalThis.print(msg);
  } else {
    // fallback
  }
};

const assert = {
  strictEqual: (actual, expected, msg) => {
    if (actual !== expected) {
      throw new Error(`Assertion failed: expected ${expected}, got ${actual}. ${msg || ''}`);
    }
  }
};

// 1. Load precompiled bcrypt native addon
const addonKey = 'bcrypt_lib';
printLog(`Loading bcrypt binary from N-API key: ${addonKey}`);
const bindings = globalThis.__napi_load_addon(addonKey);

if (!bindings || !bindings.gen_salt_sync) {
  printLog("Failed to load bcrypt bindings or methods are missing.");
  process.exit(1);
}

printLog("bcrypt binary loaded successfully!");

// 2. Prepare 16-byte seed buffer
const seed = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);

// 3. Verify Synchronous Flow (Salt -> Encrypt -> Compare)
printLog("Starting synchronous bcrypt flow...");
const saltSync = bindings.gen_salt_sync('b', 10, seed);
printLog(`[Sync] Generated Salt: ${saltSync}`);

const hashSync = bindings.encrypt_sync('EscargotSecurity123', saltSync);
printLog(`[Sync] Encrypted Hash: ${hashSync}`);

const compareSyncMatch = bindings.compare_sync('EscargotSecurity123', hashSync);
const compareSyncMismatch = bindings.compare_sync('WrongPassword', hashSync);

assert.strictEqual(compareSyncMatch, true, "Sync match check");
assert.strictEqual(compareSyncMismatch, false, "Sync mismatch check");
printLog("Synchronous bcrypt flow PASSED successfully!");

// 4. Verify Asynchronous Flow (Thread-Pool concurrency)
printLog("Starting asynchronous thread-pool bcrypt flow...");

let asyncCompleted = false;

bindings.gen_salt('b', 10, seed, (err, saltAsync) => {
  if (err) {
    printLog(`[Async] gen_salt error: ${err}`);
    process.exit(1);
  }
  printLog(`[Async] Generated Salt: ${saltAsync}`);

  bindings.encrypt('EscargotSecurity123', saltAsync, (err, hashAsync) => {
    if (err) {
      printLog(`[Async] encrypt error: ${err}`);
      process.exit(1);
    }
    printLog(`[Async] Encrypted Hash: ${hashAsync}`);

    bindings.compare('EscargotSecurity123', hashAsync, (err, isMatch) => {
      if (err) {
        printLog(`[Async] compare error: ${err}`);
        process.exit(1);
      }
      printLog(`[Async] Compare Match Result: ${isMatch}`);
      assert.strictEqual(isMatch, true, "Async match check");

      bindings.compare('WrongPassword', hashAsync, (err, isMismatch) => {
        if (err) {
          printLog(`[Async] compare mismatch error: ${err}`);
          process.exit(1);
        }
        printLog(`[Async] Compare Mismatch Result: ${isMismatch}`);
        assert.strictEqual(isMismatch, false, "Async mismatch check");

        printLog("Asynchronous thread-pool bcrypt flow PASSED successfully!");
        printLog("TEST_SUCCESS");
        asyncCompleted = true;
      });
    });
  });
});

// Await the asynchronous worker thread responses by holding the event loop artificially
(async function holdLoop() {
  while (!asyncCompleted) {
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
})();
