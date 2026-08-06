'use strict';

const printLog = (msg) => {
  if (typeof globalThis.print === 'function') {
    globalThis.print(msg);
  } else {
    // fallback
  }
};

// 1. Load the compiled node-sqlite3 binary directly via our N-API custom loader key
const addonKey = 'node_sqlite3';
printLog(`Loading sqlite3 binary from N-API key: ${addonKey}`);
const binding = globalThis.__napi_load_addon(addonKey);

if (!binding || !binding.Database) {
  printLog("Failed to load binding or binding.Database is undefined.");
  process.exit(1);
}

printLog("sqlite3 binary loaded successfully!");

// 2. Instantiate in-memory Database
const db = new binding.Database(':memory:', (err) => {
  if (err) {
    printLog(`Database open error: ${err}`);
    process.exit(1);
  }
  printLog("In-memory database opened successfully!");

  // 3. Create schema
  db.exec('CREATE TABLE students (id INT, name TEXT);', (err) => {
    if (err) {
      printLog(`Create table error: ${err}`);
      process.exit(1);
    }
    printLog("Table 'students' created successfully!");

    // 4. Insert real records
    db.exec('INSERT INTO students VALUES (1, "Escargot N-API"); INSERT INTO students VALUES (2, "ABI Stability Core");', (err) => {
      if (err) {
        printLog(`Insert error: ${err}`);
        process.exit(1);
      }
      printLog("Records inserted successfully!");

      // 5. Close database safely
      db.close((err) => {
        if (err) {
          printLog(`Database close error: ${err}`);
          process.exit(1);
        }
        printLog("Database closed successfully! All N-API real-world assertions PASSED.");
        printLog("TEST_SUCCESS");
      });
    });
  });
});
