// Minimal, in-process CommonJS + `assert`/`common` compatibility layer used
// to run the real, unmodified Node.js test/js-native-api/*/test.js files
// against Escargot's N-API implementation (see test/cctest/testnapi_suite.cpp).
//
// This file is evaluated once per NapiEnv, as a plain top-level script (via
// ScriptParserRef::initializeScript), directly in the global scope - so
// everything declared here with `var`/`function` becomes a global, which is
// intentional: it's how `gc`, `queueMicrotask`, `setImmediate`, `process`
// etc. get installed for the test file to see, and how the C++ driver finds
// its entrypoints (`__runTest`, `__finishTest`, `__pumpOnce`).
//
// Three native hooks are expected to already be installed as globals by the
// C++ side before this file runs: __gc(), __read_file(absPath) and
// __napi_load_addon(name). See installNativeHooks() in testnapi_suite.cpp.

(function () {
    'use strict';

    // ------------------------------------------------------------------
    // path helpers (no `path` module available - these are the only two
    // operations the require() resolver below needs)
    // ------------------------------------------------------------------

    function dirnameOf(p) {
        var idx = p.lastIndexOf('/');
        return idx >= 0 ? p.slice(0, idx) : '.';
    }

    // resolves `rel` (which may contain '.'/'..' segments) against `baseDir`,
    // both using '/' separators; always returns an absolute path
    function resolvePath(baseDir, rel) {
        var combined = rel.charAt(0) === '/' ? rel : baseDir + '/' + rel;
        var parts = combined.split('/');
        var out = [];
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i];
            if (part === '' || part === '.') {
                continue;
            }
            if (part === '..') {
                out.pop();
            } else {
                out.push(part);
            }
        }
        return '/' + out.join('/');
    }

    // ------------------------------------------------------------------
    // assert shim (subset used by the target test dirs: strictEqual,
    // notStrictEqual, deepStrictEqual, throws, ok, match, bare assert())
    // ------------------------------------------------------------------

    function AssertionError(message) {
        var err = new Error(message);
        err.name = 'AssertionError';
        return err;
    }

    function inspect(v) {
        try {
            if (typeof v === 'string') {
                return JSON.stringify(v);
            }
            if (typeof v === 'bigint') {
                return String(v) + 'n';
            }
            if (typeof v === 'symbol' || typeof v === 'function') {
                return String(v);
            }
            return JSON.stringify(v);
        } catch (e) {
            return String(v);
        }
    }

    function assert(value, message) {
        if (!value) {
            throw AssertionError(message || ('The expression evaluated to a falsy value:\n\n' + inspect(value)));
        }
    }

    function strictEqual(actual, expected, message) {
        if (!Object.is(actual, expected)) {
            throw AssertionError(message ||
                ('Expected values to be strictly equal:\n' + inspect(actual) + ' !== ' + inspect(expected)));
        }
    }

    function notStrictEqual(actual, expected, message) {
        if (Object.is(actual, expected)) {
            throw AssertionError(message ||
                ('Expected "actual" to be strictly unequal to:\n\n' + inspect(actual)));
        }
    }

    function isTypedArray(v) {
        return ArrayBuffer.isView(v) && !(v instanceof DataView);
    }

    function deepStrictEqualInner(a, b, seen) {
        if (Object.is(a, b)) {
            return true;
        }
        if (typeof a !== typeof b) {
            return false;
        }
        if (a === null || b === null) {
            return a === b;
        }
        if (typeof a !== 'object') {
            // primitives not handled by Object.is above (e.g. 1 vs 1 already
            // caught) really are unequal at this point
            return false;
        }

        // both non-null objects
        if (Object.getPrototypeOf(a) !== Object.getPrototypeOf(b)) {
            return false;
        }

        if (Array.isArray(a)) {
            if (!Array.isArray(b) || a.length !== b.length) {
                return false;
            }
            for (var i = 0; i < a.length; i++) {
                if (!deepStrictEqualInner(a[i], b[i], seen)) {
                    return false;
                }
            }
            return true;
        }

        if (isTypedArray(a) || isTypedArray(b)) {
            if (!isTypedArray(a) || !isTypedArray(b)) {
                return false;
            }
            if (a.constructor !== b.constructor || a.length !== b.length) {
                return false;
            }
            for (var j = 0; j < a.length; j++) {
                if (!Object.is(a[j], b[j])) {
                    return false;
                }
            }
            return true;
        }

        if (a instanceof DataView) {
            if (!(b instanceof DataView) || a.byteLength !== b.byteLength) {
                return false;
            }
            for (var k = 0; k < a.byteLength; k++) {
                if (a.getUint8(k) !== b.getUint8(k)) {
                    return false;
                }
            }
            return true;
        }

        if (a instanceof RegExp) {
            return b instanceof RegExp && a.source === b.source && a.flags === b.flags;
        }

        if (a instanceof Date) {
            return b instanceof Date && Object.is(a.getTime(), b.getTime());
        }

        // plain-object-ish fallback: compare own enumerable string keys
        var aKeys = Object.keys(a).sort();
        var bKeys = Object.keys(b).sort();
        if (aKeys.length !== bKeys.length) {
            return false;
        }
        for (var m = 0; m < aKeys.length; m++) {
            if (aKeys[m] !== bKeys[m]) {
                return false;
            }
        }
        for (var n = 0; n < aKeys.length; n++) {
            if (!deepStrictEqualInner(a[aKeys[n]], b[aKeys[n]], seen)) {
                return false;
            }
        }
        return true;
    }

    function deepStrictEqual(actual, expected, message) {
        if (!deepStrictEqualInner(actual, expected, null)) {
            throw AssertionError(message ||
                ('Expected values to be strictly deep-equal:\n' + inspect(actual) + '\nvs\n' + inspect(expected)));
        }
    }

    function isErrorConstructorLike(fn) {
        if (typeof fn !== 'function') {
            return false;
        }
        if (fn === Error) {
            return true;
        }
        var proto = fn.prototype;
        while (proto) {
            if (proto === Error.prototype) {
                return true;
            }
            proto = Object.getPrototypeOf(proto);
        }
        return false;
    }

    function matchErrorAgainstObject(err, expected) {
        for (var key in expected) {
            if (!Object.prototype.hasOwnProperty.call(expected, key)) {
                continue;
            }
            var expectedVal = expected[key];
            var actualVal = err ? err[key] : undefined;
            if (expectedVal instanceof RegExp) {
                if (!expectedVal.test(String(actualVal))) {
                    throw AssertionError('Expected property "' + key + '" (' + inspect(actualVal) +
                        ') to match ' + expectedVal);
                }
            } else if (!Object.is(actualVal, expectedVal)) {
                throw AssertionError('Expected property "' + key + '" to strictly equal ' +
                    inspect(expectedVal) + ' but got ' + inspect(actualVal));
            }
        }
    }

    function throwsImpl(shouldThrow, fn, expected, message) {
        var threw = false;
        var error;
        try {
            fn();
        } catch (e) {
            threw = true;
            error = e;
        }

        if (!shouldThrow) {
            if (threw) {
                throw AssertionError(message || ('Got unwanted exception: ' + inspect(error && error.message)));
            }
            return;
        }

        if (!threw) {
            throw AssertionError(message || 'Missing expected exception');
        }

        if (expected === undefined || expected === null) {
            return;
        }

        if (expected instanceof RegExp) {
            // Node's assert.throws tests a RegExp `expected` against
            // `String(error)` (i.e. Error.prototype.toString(), "Name: message"),
            // not against `error.message` alone.
            var stringified = String(error);
            if (!expected.test(stringified)) {
                throw AssertionError(message ||
                    ('Expected error to match ' + expected + ' but got ' + inspect(stringified)));
            }
            return;
        }

        if (typeof expected === 'function') {
            if (isErrorConstructorLike(expected)) {
                if (!(error instanceof expected)) {
                    throw AssertionError(message ||
                        ('Expected error to be instance of ' + (expected.name || expected) +
                            ' but got ' + inspect(error)));
                }
                return;
            }
            // validation predicate
            var predicateResult = expected(error);
            if (predicateResult === false) {
                throw AssertionError(message || 'Validation function on thrown error returned false');
            }
            return;
        }

        if (typeof expected === 'object') {
            matchErrorAgainstObject(error, expected);
            return;
        }

        throw new TypeError('Unsupported "expected" argument to assert.throws/doesNotThrow');
    }

    function throwsFn(fn, expected, message) {
        throwsImpl(true, fn, expected, message);
    }

    function doesNotThrow(fn, expected, message) {
        throwsImpl(false, fn, expected, message);
    }

    function ok(value, message) {
        assert(value, message);
    }

    function match(str, regex, message) {
        if (!(regex instanceof RegExp)) {
            throw new TypeError('"regex" must be a RegExp');
        }
        if (!regex.test(str)) {
            throw AssertionError(message ||
                ('The input did not match the regular expression ' + regex + '. Input:\n\n' + inspect(str)));
        }
    }

    function fail(message) {
        throw AssertionError(message || 'Failed');
    }

    assert.strictEqual = strictEqual;
    assert.notStrictEqual = notStrictEqual;
    assert.deepStrictEqual = deepStrictEqual;
    assert.throws = throwsFn;
    assert.doesNotThrow = doesNotThrow;
    assert.ok = ok;
    assert.match = match;
    assert.fail = fail;
    // easy self-loop, some tests do `assert.equal`/`assert.notEqual` too;
    // not in the documented "used" list but cheap and harmless to alias.
    assert.equal = strictEqual;
    assert.notEqual = notStrictEqual;
    // assert.ifError(value): passes for null/undefined, otherwise rethrows the
    // value (an Error) or fails - used by node-api child-process tests
    // (test_async/test_fatal) to assert spawnSync produced no launch error.
    assert.ifError = function (value) {
        if (value !== null && value !== undefined) {
            if (value instanceof Error) {
                throw value;
            }
            throw AssertionError('ifError got unwanted exception: ' + inspect(value));
        }
    };

    // ------------------------------------------------------------------
    // common shim (buildType, mustCall, mustCallAtLeast, mustNotCall,
    // nodeProcessAborted)
    // ------------------------------------------------------------------

    var mustCallChecks = [];

    function noop() {}

    function mustCallInner(fn, criteria, field) {
        if (typeof fn === 'number') {
            criteria = fn;
            fn = undefined;
        }
        if (criteria === undefined) {
            criteria = 1;
        }
        if (fn === undefined) {
            fn = noop;
        }
        if (typeof criteria !== 'number') {
            throw new TypeError('Invalid ' + field + ' value: ' + criteria);
        }

        var context = { actual: 0, name: fn.name || '<anonymous>' };
        context[field] = criteria;
        mustCallChecks.push(context);

        var wrapper = function () {
            context.actual++;
            return fn.apply(this, arguments);
        };
        return wrapper;
    }

    function mustCall(fn, exact) {
        return mustCallInner(fn, exact, 'exact');
    }

    function mustCallAtLeast(fn, minimum) {
        return mustCallInner(fn, minimum, 'minimum');
    }

    function mustNotCall(msg) {
        return function () {
            var args = [];
            for (var i = 0; i < arguments.length; i++) {
                args.push(inspect(arguments[i]));
            }
            var argsInfo = args.length > 0 ? ('\ncalled with arguments: ' + args.join(', ')) : '';
            throw AssertionError((msg || 'function should not have been called') + argsInfo);
        };
    }

    function checkMustCalls() {
        var failed = [];
        for (var i = 0; i < mustCallChecks.length; i++) {
            var c = mustCallChecks[i];
            if ('minimum' in c) {
                if (c.actual < c.minimum) {
                    failed.push('Mismatched ' + c.name + ' function calls. Expected at least ' +
                        c.minimum + ', actual ' + c.actual + '.');
                }
            } else if (c.actual !== c.exact) {
                failed.push('Mismatched ' + c.name + ' function calls. Expected exactly ' +
                    c.exact + ', actual ' + c.actual + '.');
            }
        }
        if (failed.length) {
            throw new Error(failed.join('\n'));
        }
    }

    // matches Node's own test/common/index.js nodeProcessAborted (Linux/non-
    // Windows/non-SunOS branch): a signal-killed child (spawnSync sets
    // `status: null, signal: 'SIGxxx'` in that case) is "aborted" iff the
    // signal is one of these three; otherwise fall back to checking the
    // (non-signal) exit code against the "aborted" range a compiler's
    // abort()/V8 fatal-error exit can produce.
    function nodeProcessAborted(exitCode, signal) {
        var expectedSignals = ['SIGILL', 'SIGTRAP', 'SIGABRT'];
        var expectedExitCodes = [132, 133, 134];
        if (signal !== null && signal !== undefined) {
            return expectedSignals.indexOf(signal) !== -1;
        }
        return expectedExitCodes.indexOf(exitCode) !== -1;
    }

    var common = {
        buildType: 'Release',
        mustCall: mustCall,
        mustCallAtLeast: mustCallAtLeast,
        mustNotCall: mustNotCall,
        nodeProcessAborted: nodeProcessAborted
    };

    // ------------------------------------------------------------------
    // common/gc shim ({ gcUntil })
    // ------------------------------------------------------------------

    function gcUntil(name, condition, maxCount) {
        if (maxCount === undefined) {
            maxCount = 10;
        }
        return new Promise(function (resolve, reject) {
            var count = 0;
            function step() {
                if (condition()) {
                    resolve();
                    return;
                }
                count++;
                if (count >= maxCount) {
                    reject(new Error('Test ' + name + ' failed'));
                    return;
                }
                setImmediate(function () {
                    __gc();
                    step();
                });
            }
            step();
        });
    }

    var commonGc = { gcUntil: gcUntil };

    // ------------------------------------------------------------------
    // globals: gc, queueMicrotask, setImmediate/setTimeout family, process
    // ------------------------------------------------------------------

    globalThis.gc = function () {
        __gc();
    };

    // Node exposes both `global` and `globalThis` pointing at the same object
    if (typeof globalThis.global === 'undefined') {
        globalThis.global = globalThis;
    }

    if (typeof globalThis.queueMicrotask !== 'function') {
        globalThis.queueMicrotask = function (cb) {
            Promise.resolve().then(function () {
                cb();
            });
        };
    }

    globalThis.setImmediate = function (cb) {
        var args = Array.prototype.slice.call(arguments, 1);
        __uv_timer_start(cb, 0, ...args);
        return 0; // dummy id
    };
    globalThis.clearImmediate = function (id) {};
    globalThis.setTimeout = function (cb) {
        var args = Array.prototype.slice.call(arguments, 2);
        var delayMs = arguments[1] || 0;
        __uv_timer_start(cb, delayMs, ...args);
        return 0; // dummy id
    };
    globalThis.clearTimeout = function (id) {};
    globalThis.setInterval = function () { return 0; };
    globalThis.clearInterval = function () {};

    // OS-backed libuv timers natively handle setImmediate/setTimeout via __uv_timer_start.
    // __pumpOnce no longer needs to artificially dispatch JS-queued immediates.
    globalThis.__pumpOnce = function () {
        return false;
    };

    if (typeof globalThis.console !== 'object' || globalThis.console === null) {
        // map console to globalThis.print to surface outputs in --napi-run mode.
        var p = typeof globalThis.print === 'function' ? globalThis.print : function() {};
        globalThis.console = {
            log: p,
            info: p,
            warn: p,
            error: p
        };
    }

    // ------------------------------------------------------------------
    // process.on('uncaughtException', ...): a minimal EventEmitter-ish
    // registry (only the 'uncaughtException' event is actually needed by any
    // target test.js). See __routeUncaughtException below, called from the
    // C++ driver (testnapi_suite.cpp) whenever a job/immediate/top-level
    // script throws, or a native finalizer left a pending exception that
    // never surfaced as a normal JS throw (its own napi_call_function
    // already reported it via a returned status, e.g.
    // test_reference/test_finalizer.js's createExternalWithJsFinalize).
    // ------------------------------------------------------------------

    var uncaughtExceptionHandlers = [];

    function processOn(event, handler) {
        if (event === 'uncaughtException') {
            uncaughtExceptionHandlers.push(handler);
        }
        return process;
    }

    // process.once('uncaughtException', ...): like processOn but the handler
    // removes itself right before running, so it fires at most once (used by
    // test_callback_scope's throw-from-callback-scope case).
    function processOnce(event, handler) {
        if (event === 'uncaughtException') {
            var wrapper = function (err) {
                var idx = uncaughtExceptionHandlers.indexOf(wrapper);
                if (idx >= 0) {
                    uncaughtExceptionHandlers.splice(idx, 1);
                }
                return handler(err);
            };
            uncaughtExceptionHandlers.push(wrapper);
        }
        return process;
    }

    // Routes a thrown value to every registered 'uncaughtException' handler,
    // in registration order, same as Node itself invoking them all for one
    // exception. If none are registered, rethrows `err` so the caller (the
    // C++ driver) sees it and fails the test exactly as an actually-unhandled
    // exception should. A handler that itself throws is likewise left to
    // propagate (real Node also treats that as fatal, not silently retried).
    globalThis.__routeUncaughtException = function (err) {
        if (uncaughtExceptionHandlers.length === 0) {
            throw err;
        }
        var handlers = uncaughtExceptionHandlers.slice();
        for (var i = 0; i < handlers.length; i++) {
            handlers[i](err);
        }
    };

    // The absolute path to the running cctest binary (captured at process
    // startup - testapi.cpp's argv[0]/readlink("/proc/self/exe"), see
    // testnapi_suite.cpp's ResolveCctestBinaryPath), and any extra CLI
    // argv (role/arg strings) this particular run was given (empty for a
    // normal, gtest-driven NapiSuite.* TEST; non-empty when this same binary
    // re-invoked itself in single-test CLI mode - see __spawn_sync below).
    var execPathValue = (typeof __napi_exec_path === 'function') ? __napi_exec_path() : 'escargot';
    var cliExtraArgv = (typeof __napi_cli_extra_argv === 'function') ? __napi_cli_extra_argv() : [];

    globalThis.process = {
        // argv[1] (the running script's own path) is filled in by __runTest
        // below, once the target test.js's absolute path is known.
        argv: [execPathValue, ''].concat(cliExtraArgv),
        execPath: execPathValue,
        platform: 'linux',
        arch: 'x64',
        version: 'v20.0.0',
        versions: {},
        env: {},
        on: processOn,
        once: processOnce,
        exit: function () {},
        cwd: function () {
            return '.';
        },
        nextTick: (function() {
            var nextTickQueue = [];
            var nextTickScheduled = false;

            function drainNextTicks() {
                nextTickScheduled = false;
                // Drain everything in the queue synchronously, mirroring Node's exact nextTick behavior
                var batch = nextTickQueue.slice();
                nextTickQueue = [];
                for (var i = 0; i < batch.length; i++) {
                    var item = batch[i];
                    item.cb.apply(undefined, item.args);
                }
                // If new nextTicks were queued recursively during the drain, schedule them again
                if (nextTickQueue.length > 0 && !nextTickScheduled) {
                    nextTickScheduled = true;
                    queueMicrotask(drainNextTicks);
                }
            }

            return function(cb) {
                var args = Array.prototype.slice.call(arguments, 1);
                nextTickQueue.push({ cb: cb, args: args });
                if (!nextTickScheduled) {
                    nextTickScheduled = true;
                    // Enqueue at the very front of the microtask queue
                    queueMicrotask(drainNextTicks);
                }
            };
        })(),
        // identity-only placeholders: spawnSync's `options.stdio` (below)
        // only ever compares these by reference/checks for the literal
        // string 'pipe', never actually reads/writes through them.
        stdin: {},
        stdout: {},
        stderr: {}
    };

    // ------------------------------------------------------------------
    // child_process.spawnSync: re-invokes *this same* cctest binary in the
    // single-test CLI mode implemented in testnapi_suite.cpp
    // (RunNapiSingleTestCli)/testapi.cpp (`--napi-run <abs_test_js> [args...]`),
    // via the native __spawn_sync(command, execArgv, options) hook
    // (fork()+execvp()+waitpid(), testnapi_suite.cpp). Supports exactly the
    // shape the target test.js files actually use:
    // `spawnSync(process.execPath, ['--expose-gc'?, __filename, role, ...])`.
    // ------------------------------------------------------------------

    function stripNodeFlags(args) {
        // this harness has no flags to consume (--expose-gc is a no-op here:
        // global.gc() is always available) - only the __filename + role/args
        // that follow matter to --napi-run.
        var out = [];
        for (var i = 0; i < args.length; i++) {
            if (String(args[i]).charAt(0) !== '-') {
                out.push(args[i]);
            }
        }
        return out;
    }

    function resolveStdioCapture(options) {
        var stdio = options && options.stdio;
        if (!stdio) {
            return { stdout: true, stderr: true }; // Node's own spawnSync default: pipe both
        }
        return {
            stdout: stdio[1] === 'pipe',
            stderr: stdio[2] === 'pipe'
        };
    }

    function spawnSync(command, args, options) {
        var positional = stripNodeFlags(args || []);
        // positional[0] is the target test.js's own __filename; anything
        // after it (role, extra args) is threaded straight through.
        var execArgv = ['--napi-run'].concat(positional);
        var capture = resolveStdioCapture(options);
        var result = __spawn_sync(command, execArgv, capture);
        return {
            pid: result.pid,
            status: result.status,
            signal: result.signal,
            stdout: result.stdout,
            stderr: result.stderr,
            error: result.error || null
        };
    }

    // child_process.fork(modulePath[, args][, options]): Node runs modulePath
    // as a new Node child with an IPC channel. Here the child is the same
    // cctest binary re-invoked in --napi-run mode (identical bridge to
    // spawnSync), run to completion synchronously by __spawn_sync; we then
    // surface a minimal ChildProcess whose 'exit'/'close' events fire on the
    // next microtask (after the caller has attached its handlers). The target
    // tests only observe the child's exit code via child.on('close', ...), so
    // IPC/streaming is stubbed. Signature tolerates fork(path), fork(path,
    // args), fork(path, options), fork(path, args, options).
    function fork(modulePath, args, options) {
        if (args && !Array.isArray(args)) {
            options = args;
            args = [];
        }
        var childArgs = [modulePath].concat(args || []);
        var execArgv = ['--napi-run'].concat(childArgs);
        var result = __spawn_sync(process.execPath, execArgv, { stdout: false, stderr: false });

        var handlers = { exit: [], close: [], error: [], message: [], disconnect: [] };
        function on(event, cb) {
            if (handlers[event]) {
                handlers[event].push(cb);
            }
            return child;
        }
        var child = {
            pid: result.pid,
            connected: true,
            on: on,
            once: on, // fired at most once here anyway (single exit)
            send: function () { return true; },
            disconnect: function () { child.connected = false; },
            kill: function () {},
            unref: function () {},
            ref: function () {}
        };
        if (result.error) {
            queueMicrotask(function () {
                for (var i = 0; i < handlers.error.length; i++) {
                    handlers.error[i](result.error);
                }
            });
            return child;
        }
        var code = result.signal ? null : result.status;
        var signal = result.signal || null;
        queueMicrotask(function () {
            child.connected = false;
            var i;
            for (i = 0; i < handlers.exit.length; i++) {
                handlers.exit[i](code, signal);
            }
            for (i = 0; i < handlers.close.length; i++) {
                handlers.close[i](code, signal);
            }
        });
        return child;
    }

    var childProcessModule = { spawnSync: spawnSync, fork: fork };

    // Minimal `vm` module: runInNewContext(code) evaluates `code` in a fresh
    // JS context with its own global (backed by the native
    // __vm_run_in_new_context hook - testnapi_suite.cpp). The optional
    // sandbox/options args real Node supports aren't used by the target tests
    // (test_make_callback), so they're accepted and ignored.
    var vmModule = {
        runInNewContext: function (code) {
            return __vm_run_in_new_context(String(code));
        }
    };

    // ------------------------------------------------------------------
    // CommonJS require()/module wrapper
    // ------------------------------------------------------------------

    var moduleCache = Object.create(null);
    // sentinel "main module" - always distinct from every test module object,
    // so `if (module !== require.main)` (used by e.g. test_instance_data) always
    // takes the "required as a module" branch instead of a
    // worker_threads-style self-respawn branch (this harness has no
    // worker_threads shim). child_process-based self-respawn (spawnSync +
    // `process.argv[2] === 'child'`, e.g. test_finalizer/test_fatal_finalize.js)
    // *is* supported - see child_process/spawnSync below.
    var requireMainSentinel = { exports: {}, __isHarnessMainSentinel: true };

    function isAddonRequest(id) {
        return id.indexOf('/build/') !== -1 || (id.charAt(0) !== '.' && id.charAt(0) !== '/');
    }

    function addonBasename(id) {
        var parts = id.split('/');
        return parts[parts.length - 1];
    }

    // "<requiring-dir-basename>/<addon-basename>" - lets the C++ side
    // (NativeLoadAddon, testnapi_suite.cpp) disambiguate two different test
    // directories that happen to `require()` a same-named addon backed by a
    // *different* .so (e.g. test_reference/test_finalizer.js vs
    // test_finalizer/test_fatal_finalize.js, both requiring "test_finalizer"),
    // while staying a no-op for every other, unambiguous addon (NativeLoadAddon
    // falls back to the bare basename if no qualified entry matches).
    function qualifiedAddonName(requiringDirname, id) {
        var dirParts = requiringDirname.split('/');
        var dirBase = dirParts[dirParts.length - 1];
        return dirBase + '/' + addonBasename(id);
    }

    function loadJsModule(absPath) {
        var cached = moduleCache[absPath];
        if (cached) {
            return cached.exports;
        }

        var source = __read_file(absPath);
        var dirname = dirnameOf(absPath);
        var module = { exports: {}, id: absPath, filename: absPath, loaded: false };
        moduleCache[absPath] = module;

        var req = makeRequire(dirname);
        var wrapper = new Function('exports', 'require', 'module', '__filename', '__dirname', source);
        wrapper.call(module.exports, module.exports, req, module, absPath, dirname);

        module.loaded = true;
        return module.exports;
    }

    function makeRequire(dirname) {
        function req(id) {
            if (id === '../../common' || id === '../common' || /(^|\/)common$/.test(id)) {
                return common;
            }
            if (/(^|\/)common\/gc$/.test(id)) {
                return commonGc;
            }
            if (id === 'assert') {
                return assert;
            }
            if (id === 'child_process') {
                return childProcessModule;
            }
            if (id === 'process') {
                return globalThis.process;
            }
            if (id === 'vm') {
                return vmModule;
            }
            if (isAddonRequest(id)) {
                return __napi_load_addon(qualifiedAddonName(dirname, id));
            }
            // relative .js (or extension-less) file, resolved against the
            // requiring module's directory
            var resolved = resolvePath(dirname, id);
            if (!/\.js$/.test(resolved)) {
                resolved += '.js';
            }
            return loadJsModule(resolved);
        }
        req.main = requireMainSentinel;
        req.resolve = function (id) {
            return id;
        };
        return req;
    }

    // ------------------------------------------------------------------
    // entrypoints called from C++ (testnapi_suite.cpp)
    // ------------------------------------------------------------------

    // runs one target test file end-to-end (require wiring, module wrapper);
    // mirrors loadJsModule but never cache-hits (a fresh call per gtest TEST,
    // on top of a fresh NapiEnv/global scope, so caching across calls never
    // actually matters - kept separate mainly for clarity at the call site).
    globalThis.__runTest = function (absPath) {
        mustCallChecks.length = 0;
        // process.argv[1] is conventionally the running script's own path
        // (Node); required for e.g. spawnSync(process.execPath, [__filename, ...])
        // self-respawn to pass __filename straight through.
        process.argv[1] = absPath;
        return loadJsModule(absPath);
    };

    // checks the common.mustCall()/mustCallAtLeast() registry accumulated by
    // the just-run test file; throws (failing the gtest) if unmet
    globalThis.__finishTest = function () {
        checkMustCalls();
    };
}());
