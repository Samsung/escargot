#!/usr/bin/env python3
"""Measure Escargot and V8 Lite with the monthly fixed-work memory protocol."""

import csv
import datetime
import importlib.util
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys


def write_results(report, data):
    (report / 'measurements.json').write_text(json.dumps(data, indent=2) + '\n')
    fields = ['architecture', 'engine', 'suite', 'repetitions', 'average_rss_kib',
              'peak_rss_kib', 'average_pss_kib', 'average_uss_kib',
              'active_duration_seconds', 'work_signature', 'error']
    with (report / 'measurements.csv').open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        for name, engine in data['engines'].items():
            for suite, result in engine.get('memory', {}).items():
                row = {field: result.get(field, '') for field in fields}
                row.update(architecture=data['architecture'], engine=name, suite=suite)
                writer.writerow(row)


def print_results(data):
    print(f"\n{data['architecture']} fixed-work memory: Escargot vs V8 Lite", flush=True)
    print('Engine       Suite          Average RSS  Peak RSS  Average PSS  Average USS  Active work',
          flush=True)
    for name, engine in data['engines'].items():
        for suite, result in engine.get('memory', {}).items():
            if 'error' in result:
                print(f"{name:12} {suite:14} FAILED: {result['error']}", flush=True)
                continue
            values = [result[key] / 1024 for key in
                      ['average_rss_kib', 'peak_rss_kib', 'average_pss_kib', 'average_uss_kib']]
            print(f'{name:12} {suite:14} {values[0]:10.2f} {values[1]:9.2f}'
                  f' {values[2]:12.2f} {values[3]:12.2f}'
                  f" {result['active_duration_seconds']:10.3f}s", flush=True)
    print('Memory units: MiB. Active work includes loading and natural GC; checkpoint waits are excluded.',
          flush=True)
    print('Peak RSS covers the whole process lifetime. Active duration is not an official benchmark score.',
          flush=True)


def run_experiment(harness, report, data):
    architecture = data['architecture']
    repetitions = int(os.environ.get('BENCHMARK_REPETITIONS', '1'))
    if repetitions < 1:
        raise ValueError('BENCHMARK_REPETITIONS must be positive')
    cpu = int(os.environ['BENCHMARK_CPU'])
    allowed = sorted(os.sched_getaffinity(0))
    if cpu not in allowed:
        raise RuntimeError(f'Engine CPU {cpu} is outside allowed affinity {allowed}')
    monitor_cpu = next((candidate for candidate in allowed if candidate != cpu), None)
    if monitor_cpu is None:
        raise RuntimeError('Memory measurements require a separate allowed monitor CPU')

    os.environ.setdefault('GITHUB_WORKSPACE', str(harness))
    sys.path.insert(0, str(harness / 'tools'))
    spec = importlib.util.spec_from_file_location('monthly_engine_benchmark',
                                                harness / 'tools/monthly-engine-benchmark.py')
    monthly = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(monthly)
    monthly.ROOT = harness
    monthly.REPORT = report
    from engine_memory_benchmark import METHOD, aggregate, measure_memory

    data.update(cpu=cpu, monitor_cpu=monitor_cpu, repetitions=repetitions,
                memory_method=METHOD, execution_mode='native',
                pointer_bits=32 if architecture == 'arm32' else 64,
                protocol={'sample_interval_ms': 10,
                          'average_window': 'baseline to workload_end, including loading and natural GC',
                          'average_method': 'time-weighted smaps_rollup; checkpoint waits excluded',
                          'gc_policy': 'natural GC; two explicit GC calls after workload_end',
                          'peak_method': 'whole-process GNU time/wait4 ru_maxrss',
                          'uss_definition': 'Private_Clean + Private_Dirty'})
    if architecture == 'arm32':
        loader = Path(os.environ['BENCHMARK_RUNTIME_LOADER'])
        runtime_dir = Path(os.environ['BENCHMARK_RUNTIME_DIRECTORY'])
        if not loader.is_file() or not runtime_dir.is_dir():
            raise RuntimeError('Matching ARM32 loader and runtime directory are required')
        runtime = {'source': 'ARM32 Ubuntu container runtime supplied by the orchestrator',
                   'loader': str(loader), 'loader_sha256': monthly.sha256(loader),
                   'directory': str(runtime_dir)}
    else:
        loader, runtime_dir, runtime = monthly.install_glibc()
    data['runtime'] = runtime
    d8, metadata = monthly.install_d8(loader, runtime_dir)
    d8 = list(map(str, d8))
    # The provided ARM32 launcher execs the native shell with its matching runtime.
    engine = Path(os.environ['ESCARGOT_ENGINE']).resolve()
    if architecture == 'arm32':
        escargot = [str(engine)]
    else:
        libraries = f'{runtime_dir}:/usr/icu78-64/lib:{monthly.SYSTEM_LIBRARY_PATH}'
        escargot = [str(loader), '--library-path', libraries, str(engine)]
    engines = [('escargot', escargot, 'escargot'),
               ('d8_lite', [*d8, '--lite-mode', '--expose-gc'], 'd8_jitless')]
    data['engines'] = {'escargot': {'command': escargot,
                                  'launcher_sha256': monthly.sha256(engine), 'memory': {}},
                       'd8_lite': {**metadata, 'command': engines[1][1],
                                   'flags': ['--lite-mode', '--expose-gc'], 'memory': {}}}
    try:
        data['escargot_source_revision'] = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], text=True).strip()
    except subprocess.CalledProcessError:
        data['escargot_source_revision'] = None
    write_results(report, data)

    wtb = harness / 'test/web-tooling-benchmark'
    if not (wtb / 'dist/memory.js').is_file():
        wtb = monthly.build_wtb()
    drivers = monthly.build_memory_drivers(wtb)
    data['memory_driver_sha256'] = {suite: monthly.sha256(driver)
                                    for suite, (_, driver, _, _) in drivers.items()}
    data['wtb_memory_bundle_sha256'] = monthly.sha256(wtb / 'dist/memory.js')
    data['monitor_sha256'] = monthly.sha256(harness / 'tools/engine_memory_benchmark.py')
    signatures = {}
    failures = []
    for repetition in range(repetitions):
        ordered = engines if repetition % 2 == 0 else engines[::-1]
        for name, command, bridge in ordered:
            for suite, (directory, driver, count, iterations) in drivers.items():
                label = f'{name}-{suite}-{architecture}-{repetition + 1}'
                print(f'::group::{label}', flush=True)
                memory = data['engines'][name]['memory']
                result = memory.setdefault(suite, {'samples': []})
                try:
                    # V8 uses d8's readline checkpoint bridge.
                    sample = measure_memory(label, bridge, command, directory, os.environ,
                                            driver, report, cpu, monitor_cpu)
                    work = sample['work_counts']
                    if len(work) != count or any(value != iterations for value in work.values()):
                        raise RuntimeError(f'Incorrect fixed-work counts: {work}')
                    signature = sample['work_signature']
                    if suite in signatures and signatures[suite] != signature:
                        raise RuntimeError('Work counts differ between engines or repetitions')
                    signatures[suite] = signature
                    samples = [*result['samples'], sample]
                    updated = aggregate(samples)
                    durations = [value['active_duration_seconds'] for value in samples]
                    updated.update(active_duration_seconds=statistics.median(durations),
                                   active_duration_seconds_range=[min(durations), max(durations)],
                                   work_signature=signature)
                    if 'error' in result:
                        updated['error'] = result['error']
                    memory[suite] = updated
                    print(f"{label}: average RSS {sample['average_rss_kib'] / 1024:.2f} MiB; "
                          f"peak {sample['peak_rss_kib'] / 1024:.2f} MiB; "
                          f"active work {sample['active_duration_seconds']:.3f}s", flush=True)
                except Exception as error:
                    result['error'] = str(error)
                    failures.append(label)
                    print(f'::error::{label}: {error}', flush=True)
                finally:
                    write_results(report, data)
                    print('::endgroup::', flush=True)
    data['memory_validated'] = not failures
    data['failures'] = failures
    print_results(data)
    if failures:
        raise RuntimeError('Incomplete memory experiment: ' + ', '.join(failures))


def main():
    architecture = os.environ['BENCHMARK_ARCHITECTURE']
    if architecture not in ('arm32', 'arm64'):
        raise ValueError('BENCHMARK_ARCHITECTURE must be arm32 or arm64')
    harness = Path(os.environ['BENCHMARK_HARNESS']).resolve()
    report = Path(os.environ['BENCHMARK_OUTPUT_DIR']).resolve() / f'v8-lite-{architecture}'
    report.mkdir(parents=True, exist_ok=True)
    data = {'architecture': architecture, 'host_architecture': platform.machine(),
            'measured_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'engines': {}, 'memory_validated': False}
    try:
        run_experiment(harness, report, data)
    except Exception as error:
        data['error'] = str(error)
        raise
    finally:
        write_results(report, data)
        print(f'Experiment data: {report}', flush=True)


if __name__ == '__main__':
    main()
