// run_mem_ci.js - like run_mem.js, but runs every benchmark exactly once
// (via base_mem.js's RunSingleBenchmark override) instead of Octane's
// default time-boxed/deterministic loop, so RSS_KB is a stable CI metric
// instead of one confounded by how many iterations a faster/slower
// engine happened to fit into a 1-second window. See base_mem.js for the
// rationale.

var base_dir = '';
load(base_dir + 'base_mem.js');
load(base_dir + 'richards.js');
load(base_dir + 'deltablue.js');
load(base_dir + 'crypto.js');
load(base_dir + 'raytrace.js');
load(base_dir + 'earley-boyer.js');
load(base_dir + 'regexp.js');
load(base_dir + 'splay.js');
load(base_dir + 'navier-stokes.js');
load(base_dir + 'pdfjs.js');
load(base_dir + 'mandreel.js');
load(base_dir + 'gbemu-part1.js');
load(base_dir + 'gbemu-part2.js');
load(base_dir + 'code-load.js');
load(base_dir + 'box2d.js');
load(base_dir + 'zlib.js');
load(base_dir + 'zlib-data.js');
load(base_dir + 'typescript.js');
load(base_dir + 'typescript-input.js');
load(base_dir + 'typescript-compiler.js');

var success = true;

// processMemoryUsage() is a native Escargot::Shell test hook (guarded by
// ESCARGOT_ENABLE_TEST, see src/shell/Shell.cpp) that isn't compiled into
// plain release/deploy builds - and this driver is meant to run on exactly
// those. Neither caller of this file actually parses the RSS_KB/BASE_RSS_KB/
// FINAL_RSS_KB lines below (tools/run-tests.py's octane-memory gate reads
// peak RSS from `/usr/bin/time -f %M` externally, and
// .github/workflows/performance-benchmark.yml samples VmRSS from
// /proc/$pid/status externally); they're only for a human skimming the raw
// log. So read the same /proc/self/status VmRSS line ourselves, in JS, via
// the always-available `read()` builtin, instead of depending on a
// test-only native function.
//
// /proc/self/status is Linux-only - both current callers of this driver
// only invoke it there, but read() throws (not just returns null) when the
// file can't be opened, so guard it anyway rather than let a future
// Darwin/Windows run die on an uncaught exception. Match the old native
// processMemoryUsage()'s failure behavior: return -1.
function processMemoryUsage() {
  try {
    var match = /VmRSS:\s*(\d+) kB/.exec(read('/proc/self/status'));
    return match ? parseInt(match[1], 10) * 1024 : -1;
  } catch (e) {
    return -1;
  }
}

function PrintResult(name, result) {
  gc(); gc();
  print(name + ': ' + result + ' | RSS_KB ' + Math.round(processMemoryUsage() / 1024));
}

function PrintError(name, error) {
  print(name + ': ' + error);
  success = false;
}

function PrintScore(score) {
  gc(); gc();
  print('----');
  print('Score (version ' + BenchmarkSuite.version + '): ' + score);
  print('FINAL_RSS_KB ' + Math.round(processMemoryUsage() / 1024));
}

gc();
print('BASE_RSS_KB ' + Math.round(processMemoryUsage() / 1024));

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError,
                           NotifyScore: PrintScore });
