// base_mem.js - single-pass overlay for Octane's base.js, for use as a
// CI memory-stability metric.
//
// Background: Octane's default measurement mode is time-boxed - see
// base.js's RunSingleBenchmark() -> Measure(), which loops
// `while (elapsed < 1000)` (or `i < benchmark.deterministicIterations` in
// doDeterministic mode) and keeps re-invoking itself until
// `benchmark.minIterations` total runs have accumulated. That ties every
// suite's iteration count to engine speed and per-benchmark tuning knobs,
// which is fatal for a suite that permanently retains per-iteration data:
// CodeLoad's cacheBust() gives every eval a fresh, unique global
// identifier, so each eval's sloppy-mode top-level `var` becomes a new
// non-configurable property on the global object - and non-configurable
// properties can never be deleted, per spec. That pins the whole compiled
// CodeBlock tree for that eval forever. A faster engine therefore runs
// more CodeLoad iterations per 1s window and retains proportionally more
// memory with zero behavior change, which looks like a memory regression
// in CI. (See memory note: escargot-codeload-retention-scales-with-speed.)
// The same run-count-tied-to-speed problem applies to every other suite
// too, e.g. PdfJS (`new Benchmark("PdfJS", false, false, 24, ...)`).
//
// Fix: replace base.js's whole time-boxed/deterministic/minIterations
// loop with the simplest possible one - run each benchmark's Setup/run/
// TearDown exactly once, then move on. No scaling knobs, no per-engine
// tuning: every run of every suite does the exact same, fixed amount of
// work, so RSS/heap-size is comparable run-to-run and usable as a CI
// stability signal instead of a confounded one.
//
// Usage: load this file (order relative to the benchmark files doesn't
// matter - it patches BenchmarkSuite.prototype directly). See
// run_mem_ci.js.

load('base.js');

BenchmarkSuite.prototype.RunSingleBenchmark = function(benchmark, data) {
  var start = new Date();
  benchmark.run();
  var elapsed = new Date() - start;
  var rms = (benchmark.rmsResult != null) ? benchmark.rmsResult() : 0;
  this.NotifyStep(new BenchmarkResult(benchmark, elapsed * 1000, rms));
  return null; // always "done after one run" - never loop, never retry.
};
