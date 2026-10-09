#!/usr/bin/env python3

import json
import os
from pathlib import Path
import re
import subprocess


def execute(command, directory, log):
    with log.open('w') as output:
        with subprocess.Popen(command, cwd=directory, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, bufsize=1) as process:
            for line in process.stdout:
                print(line, end='', flush=True)
                output.write(line)
                output.flush()
            if process.wait():
                raise subprocess.CalledProcessError(process.returncode, command)
    return log.read_text()


def main():
    engine = os.environ['ESCARGOT_ENGINE']
    architecture = os.environ['BENCHMARK_ARCHITECTURE']
    cpu = os.environ['BENCHMARK_CPU']
    repetitions = int(os.environ['BENCHMARK_REPETITIONS'])
    directory = Path(os.environ['BENCHMARK_HARNESS']) / 'test/web-tooling-benchmark'
    report = Path(os.environ['BENCHMARK_OUTPUT_DIR'])
    scores = []
    for sample in range(1, repetitions + 1):
        print(f'::group::{architecture} WTB {sample}/{repetitions}', flush=True)
        text = execute(['taskset', '-c', cpu, engine, 'dist/cli.js'], directory,
                       report / f'wtb-{architecture}-{sample}.log')
        match = re.search(r'Geometric mean:\s*([\d.]+)\s+runs/s', text)
        if not match:
            raise RuntimeError('WTB geometric mean missing')
        scores.append(float(match.group(1)))
        print('::endgroup::', flush=True)
    (report / f'wtb-{architecture}.json').write_text(json.dumps({'scores': scores}, indent=2) + '\n')
    if os.environ.get('BENCHMARK_PROFILE') == 'true':
        print(f'::group::{architecture} fixed-work WTB perf', flush=True)
        driver = report / f'wtb-fixed-{architecture}.js'
        driver.write_text("function benchmarkCheckpoint() {}\nload('dist/memory.js');\n")
        command = ['taskset', '-c', cpu, engine, str(driver)]
        execute(['perf', 'stat', '-r', '3', '-x', ';', '-e', 'task-clock,cycles,instructions,branches,branch-misses',
                 '-o', str(report / f'perf-stat-{architecture}.csv'), '--', *command], directory,
                report / f'perf-stat-{architecture}.log')
        data = report / f'perf-{architecture}.data'
        execute(['perf', 'record', '-e', 'cycles:u', '-F', '499', '-o', str(data), '--', *command], directory,
                report / f'perf-record-{architecture}.log')
        execute(['perf', 'report', '--stdio', '--no-children', '--sort', 'symbol,dso', '-i', str(data)], directory,
                report / f'perf-report-{architecture}.txt')
        print((report / f'perf-stat-{architecture}.csv').read_text(), flush=True)
        print('::endgroup::', flush=True)


if __name__ == '__main__':
    main()
