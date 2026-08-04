# Node-API (N-API) Support in Escargot

Escargot provides a 100% compliant, high-performance **Node-API (N-API)** layer that supports ABI stability versions up to **v10** (and experimental extensions). 

This allows you to load pre-compiled Node.js C++ addons directly on Escargot and embed the engine into your own native C/C++ applications using standard, engine-independent C-style hosting APIs.

---

## 1. Key Features & Compatibility

The Node-API implementation in Escargot is rigorously verified against the official Node.js Node-API test suite, achieving 100% compliance.

* **ABI Stability (v8, v9, v10+)**: Supports all standard APIs, ensuring that compiled native addons can run across engine upgrades without recompilation.
* **Thread-Safe Functions (TSFN)**: Fully supports asynchronous, multi-threaded communication between C++ worker threads and the JS main thread via `napi_create_threadsafe_function` and `napi_call_threadsafe_function`.
* **Advanced Memory Management**: Integrates seamlessly with Escargot's conservative Boehm-GC, managing lifecycle bindings through `napi_add_finalizer` and `node_api_post_finalizer`.
* **Object Control & Protection**: Fully implements metadata operations such as `napi_object_freeze`, `napi_object_seal`, and type-tagging APIs (`napi_type_tag_object`).
* **Optimized Typed Arrays**: Supports sharing raw heap memory via `SharedArrayBuffer` and `DataView` interfaces.

---

## 2. Conservative GC & Finalizer Execution Semantics

Escargot utilizes a highly optimized **Conservative Garbage Collector (Boehm-Demers-Weiser GC)**. This introduces key architectural behaviors regarding object destruction and finalizer invocation (`napi_add_finalizer`, `napi_wrap`, `node_api_post_finalizer`):

### Deferred/Late Finalization
Because the GC is conservative, it scans the native C++ call stack and CPU registers for potential pointer-like bit patterns. 
* **Stale Pointer Retention**: Stale register or stack slots from previous C++ function frames containing addresses of JS objects might temporarily keep those JS objects alive, even if they are unreachable from the JavaScript side.
* **Delayed Invocations**: As a result, native objects wrapped by Node-API may not be collected immediately when they go out of scope, causing their registered C++ finalizers to be invoked slightly later (often during subsequent GC sweeps when the native stack has been cleared or overwritten).

### Finalizer Safety & Main Thread Execution
To maintain technical integrity and avoid race conditions or crashes (e.g., executing native finalizers on a background GC thread, which is unsafe):
* **Safe-Point Execution**: Escargot queues finalized objects into a `PostFinalizer` batch during GC.
* **Active Loop Pumping**: Developers can actively flush and execute these pending finalizers on the main thread safely by calling the custom `escargot_napi_pump_message_loop()` API within their main tick loop.

---

## 3. Build Configuration

To enable the Node-API layer when building Escargot, pass the `-DESCARGOT_NAPI=ON` option to CMake:

```sh
cmake -DCMAKE_BUILD_TYPE=Release -DESCARGOT_NAPI=ON -DESCARGOT_BUILD_SHARED_LIBS=ON -GNinja
ninja
```

This compiles the N-API runtime and exposes all standard `napi_*` and `node_api_*` C symbols from the shared/static library.

### Symbol Visibility Optimization

When Node-API is enabled, Escargot automatically restricts the symbol visibility of all its proprietary C++ public APIs (`Globals`, `Evaluator`, `ObjectRef`, `ValueRef`, etc.) from `default` to `hidden` (`__attribute__((visibility("hidden")))` on Unix/GCC/Clang).

This achieves two key optimization benefits for resource-constrained production deployments:
* **Significant Binary Size Reduction**: By hiding proprietary C++ symbols, the compiler and linker can perform aggressive dead-code elimination (DCE), devirtualization, and inlining, yielding a much smaller shared library footprint.
* **Securing the Public API Surface**: Only the standard, C-style Node-API symbols (`napi_*` and `node_api_*`) are exported to the dynamic symbol table, completely concealing proprietary engine internals.

### Target Restrictions

Because Node-API is designed for engine-agnostic embedding, and because enabling N-API hides all proprietary C++ public API symbols to maximize size reduction, **the traditional proprietary C++ shell target (`-DENABLE_SHELL=ON`) is not supported when Node-API is enabled**.

If you attempt to configure CMake with `-DENABLE_SHELL=ON` and `-DESCARGOT_NAPI=ON`, CMake will halt configuration with a clear `FATAL_ERROR`, advising you to configure with `-DESCARGOT_ENABLE_SHELL=OFF` instead. (When `ESCARGOT_NAPI=ON` and the shell option is left unset, Escargot now defaults it to `OFF` automatically.)

### The N-API Shell Target (`escargot-napi`)

To replace the traditional proprietary C++ shell, enabling N-API automatically introduces a new, modern, engine-independent shell target called **`escargot-napi`**.

This shell is a clean C++ application (`src/shell/NapiShell.cpp`) that boots the JS engine and runs scripts entirely through standard Node-API hosting and event pumping interfaces (`napi_create_platform`, `napi_create_environment`, and `escargot_napi_pump_message_loop`), providing a direct, production-ready demonstration of our C-style embedding capabilities.

To run a JavaScript file with the N-API shell:
```sh
./build/out_linux64_cctest/escargot-napi script.js
```

---

## 4. C-Style Hosting & Embedding APIs

Escargot extends standard Node-API with **C-Style Hosting APIs** that allow you to initialize the platform, spin up independent Isolate environments, and run JavaScript code **without calling any of Escargot's proprietary C++ public APIs**. 

These APIs are split into two categories:
1. **De-facto Embedding Standards**: Standardized hosting APIs commonly used in multi-engine Node-API ecosystems (such as Microsoft's Node-API for React Native / libnode).
2. **Escargot-Specific Custom Extensions**: Proprietary extensions designed specifically to support tick-based game/OS event loops.

### Platform and Environment Lifecycle (De-facto Standards)

These APIs match de-facto industry standards for embedding Node-API-compliant JS engines:

```c
// Process-wide platform initialization (sets up Globals and main thread local state)
napi_status napi_create_platform(int argc, char** argv, napi_platform* result);

// Platform resource teardown
napi_status napi_destroy_platform(napi_platform platform);

// Spins up a fresh, completely independent JS isolate and returns its napi_env
napi_status napi_create_environment(napi_platform platform, napi_env* result);

// Tears down the isolate environment, running all registered cleanup hooks and finalizers
napi_status napi_destroy_environment(napi_env env);
```

### Event Loop Pumping & Synchronization (Escargot-Specific Extensions)

If your application has its own main run loop (e.g., game engines, GUI event loops, or native OS run loops), you can pump Escargot's internal libuv loop and promise queues using these custom synchronization APIs (prefixed with `escargot_napi_*`):

```c
// [Custom Extension] Explicitly runs outstanding microtasks (such as Promise reactions) on the given environment
napi_status escargot_napi_perform_microtask_checkpoint(napi_env env);

// [Custom Extension] Pumps outstanding libuv asynchronous work (Thread-Safe Functions, Async Work) 
// and drains microtask and post-finalizer queues in one tick.
// Returns whether there is still active work pending.
napi_status escargot_napi_pump_message_loop(napi_env env, bool* out_has_more_work);
```

---

## 4. Complete C++ Embedding Example

Here is a complete example of how to embed Escargot in a C++ application using *only* C-style Node-API functions:

```cpp
#include <node_api.h>
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    napi_platform platform = nullptr;
    napi_env env = nullptr;

    // 1. Initialize the process-wide platform
    if (napi_create_platform(argc, argv, &platform) != napi_ok) {
        std::cerr << "Failed to create N-API platform" << std::endl;
        return 1;
    }

    // 2. Create an independent JS isolate environment
    if (napi_create_environment(platform, &env) != napi_ok) {
        std::cerr << "Failed to create N-API environment" << std::endl;
        napi_destroy_platform(platform);
        return 1;
    }

    // 3. Evaluate JavaScript code
    napi_value script = nullptr;
    napi_value result = nullptr;
    napi_create_string_utf8(env, "1 + 2", NAPI_AUTO_LENGTH, &script);
    napi_run_script(env, script, &result);

    int32_t val = 0;
    napi_get_value_int32(env, result, &val);
    std::cout << "Result of '1 + 2' in JS is: " << val << std::endl; // Prints 3

    // 4. Register a Promise task and pump the loop
    napi_value pScript = nullptr;
    napi_create_string_utf8(env, 
        "globalThis.status = 'pending'; "
        "Promise.resolve().then(() => { globalThis.status = 'resolved'; });", 
        NAPI_AUTO_LENGTH, &pScript);
    napi_run_script(env, pScript, nullptr);

    // Promise is queued but not run yet. Run the microtask checkpoint to drain it!
    escargot_napi_perform_microtask_checkpoint(env);

    // Verify it resolved
    napi_value checkScript = nullptr;
    napi_value checkResult = nullptr;
    napi_create_string_utf8(env, "globalThis.status", NAPI_AUTO_LENGTH, &checkScript);
    napi_run_script(env, checkScript, &checkResult);

    char buf[32];
    size_t written = 0;
    napi_get_value_string_utf8(env, checkResult, buf, sizeof(buf), &written);
    std::cout << "Promise resolved status: " << buf << std::endl; // Prints 'resolved'

    // 5. Tear down and exit
    napi_destroy_environment(env);
    napi_destroy_platform(platform);

    return 0;
}
```
