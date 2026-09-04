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
