# Sandbox PoC: host interrupt and heap limits

Proof of concept for running untrusted script on Escargot with the two
controls a sandbox host needs and the engine did not have:

- `VMInstanceRef::requestInterrupt()` — callable from any thread. The script
  receives an uncatchable `RangeError` at its next loop back-edge (`Jump`) or
  call (`Call`, `CallWithReceiver`) and the error surfaces to the host as a
  normal evaluation failure. `clearInterrupt()` re-arms the VM.
- `VMInstanceRef::setHeapSoftLimit(bytes)` — checked at the same poll points
  (every 1024 polls). When the live heap of the running thread is still above
  the limit after a collection, the script receives the same kind of
  uncatchable `RangeError`.
- `Memory::setMaxHeapSize(bytes)` — bdwgc's hard limit. With
  `ESCARGOT_THREADING` each thread owns its own GC heap, so this applies to
  the calling thread only. Exceeding it is fatal (abort), so it is the
  backstop behind the soft limit, not the first line.

"Uncatchable" is implemented by a `terminating` flag on the VM: once set,
every `try` on the way out treats the exception as if it had no `catch`
clause (`finally` still runs, but any script in it that loops or calls
polls again and is terminated too).

## Shell flags

```
escargot --interrupt-after-ms=300  test/sandbox-poc/swallow.js
escargot --heap-soft-limit-mb=32   test/sandbox-poc/alloc.js
escargot --heap-hard-limit-mb=64   test/sandbox-poc/alloc.js      # aborts
escargot test/sandbox-poc/threads.js                               # needs -DESCARGOT_TEST=ON
```

`heapSize()` is added to the shell globals and returns the live GC heap of
the calling thread.

## Results (Linux x86-64, Release, ICU off, 2026-10-29)

| script | flags | result | wall | max RSS |
|---|---|---|---|---|
| `swallow.js` (nested try/catch/finally around `while(true)`) | `--interrupt-after-ms=300` | `Uncaught RangeError: Execution interrupted by host`, no catch ran | 0.31 s | 10 MB |
| `alloc.js` (push arrays forever) | `--heap-soft-limit-mb=32` | `Uncaught RangeError: Heap soft limit exceeded (33636352 bytes live, limit 33554432)` | 0.21 s | 41 MB |
| `alloc.js` | `--heap-hard-limit-mb=64` | bdwgc out-of-memory, abort | 0.36 s | 73 MB |
| `alloc.js` | no limit, interrupt at 5 s | heap grew to 486 MB before the interrupt landed | 5.8 s | 486 MB |
| `ok.js` (100M-iteration loop) | none | 2.43 s avg of 3 | | 10 MB |
| `ok.js` | soft limit + armed interrupt (poll path active) | 2.51 s avg of 3, about 3% slower | | 10 MB |
| `threads.js` (2 worker threads each build 300k objects, `-DESCARGOT_TEST=ON`) | none | each worker reports 21.7 MB live heap; main thread's heap stays at 0.5 MB before and after, so heaps are per thread | 0.36 s | 48 MB peak |

Not measured: the cost of the inactive check against a pristine build, and
`clearInterrupt()` followed by reuse of the same VM.
