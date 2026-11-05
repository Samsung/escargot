#!/usr/bin/env python3

import argparse
import contextlib
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def run(command, *, cwd=None, env=None, log=None):
    print('+ ' + shlex.join(map(str, command)), flush=True)
    with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors='replace',
                          bufsize=1, start_new_session=True) as process:
        try:
            with open(log, 'a') if log else contextlib.nullcontext() as output:
                for line in process.stdout:
                    print(line, end='', flush=True)
                    if output:
                        output.write(line)
                        output.flush()
            if process.wait():
                raise subprocess.CalledProcessError(process.returncode, command)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()


def revision(path):
    return subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()


def inspect_binary(engine, architecture, report, tool_prefix=''):
    def output(tool, *args):
        return subprocess.check_output([tool_prefix + tool, *map(str, args)], text=True)

    section_output = output('size', '-A', engine)
    (report / f'sections-{architecture}.txt').write_text(section_output)
    sections = {}
    for line in section_output.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1].isdigit():
            sections[fields[0]] = int(fields[1])
    symbols_output = output('readelf', '-sW', engine)
    functions = {}
    for line in symbols_output.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[3] == 'FUNC' and fields[6].isdigit():
            value = int(fields[1], 16)
            size = int(fields[2], 16 if fields[2].startswith('0x') else 10)
            functions[fields[7]] = {'value': value, 'size_bytes': size}
    matches = [(name, data) for name, data in functions.items()
               if name.startswith('_ZN8Escargot11Interpreter9interpretE')]
    if len(matches) != 1:
        raise RuntimeError('Expected one interpreter function for binary inspection')
    name, interpreter = matches[0]
    interpreter = dict(interpreter, symbol=name)
    interpreter['instruction_state'] = ('Thumb' if interpreter['value'] & 1 else 'ARM') if architecture == 'arm32' else 'AArch64'
    (report / f'interpreter-{architecture}.disasm').write_text(
        output('objdump', '-d', '--disassemble=' + name, engine))
    stripped = report / f'escargot-{architecture}.stripped'
    subprocess.check_call([tool_prefix + 'strip', '--strip-all', '-o', str(stripped), str(engine)])
    counts = {}
    if architecture == 'arm32':
        for function in functions.values():
            state = 'Thumb' if function['value'] & 1 else 'ARM'
            counts[state] = counts.get(state, 0) + 1
    result = {'file_bytes': engine.stat().st_size,
              'stripped_file_bytes': stripped.stat().st_size,
              'stripped_sha256': hashlib.sha256(stripped.read_bytes()).hexdigest(),
              'section_bytes': sections, 'interpreter': interpreter,
              'defined_function_states': counts}
    print(f"{architecture}: stripped={result['stripped_file_bytes']} bytes, "
          f".text={sections['.text']} bytes, interpreter={interpreter['size_bytes']} bytes "
          f"({interpreter['instruction_state']})", flush=True)
    return result


def build(source, work, architecture, report):
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DESCARGOT_DEPLOY=ON',
               '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
               '-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++',
               '-DESCARGOT_THREADING=ON', '-DESCARGOT_TCO=ON',
               '-DESCARGOT_TEMPORAL=ON', '-DESCARGOT_ENABLE_SHELL=ON']
    env = os.environ.copy()
    if architecture == 'arm32':
        import struct
        if struct.calcsize('P') != 4:
            raise RuntimeError('Expected ARM32 userland')
        options += ['-DESCARGOT_ARCH=arm', '-DCMAKE_SYSTEM_PROCESSOR=arm']
    elif Path('/usr/icu78-64').is_dir():
        env.update(LDFLAGS='-L/usr/icu78-64/lib -Wl,-rpath=/usr/icu78-64/lib',
                   PKG_CONFIG_PATH='/usr/icu78-64/lib/pkgconfig', LD_LIBRARY_PATH='/usr/icu78-64/lib')
    directory = work / f'build-{architecture}'
    log = report / f'build-{architecture}.log'
    print(f'::group::Build {architecture}', flush=True)
    run(['gcc', '--version'], log=log)
    run(['cmake', '-S', str(source), '-B', str(directory), '-GNinja', *options], env=env, log=log)
    run(['cmake', '--build', str(directory), '--target', 'escargot', '--parallel', '8'], env=env, log=log)
    engine = directory / 'escargot'
    run(['file', str(engine)], log=log)
    run(['readelf', '-h', str(engine)], log=log)
    sizes = inspect_binary(engine, architecture, report)
    commands = json.loads((directory / 'compile_commands.json').read_text())
    shutil.copy2(directory / 'compile_commands.json', report / f'compile-commands-{architecture}.json')
    mode_counts = {}
    for entry in commands:
        flags = [arg for arg in shlex.split(entry['command']) if arg in ('-marm', '-mthumb')]
        mode = flags[-1] if flags else 'toolchain-default'
        mode_counts[mode] = mode_counts.get(mode, 0) + 1
    (report / f'build-{architecture}.json').write_text(json.dumps({
        'architecture': architecture, 'cmake_options': options,
        'binary_sha256': hashlib.sha256(engine.read_bytes()).hexdigest(),
        'binary_sizes': sizes, 'compile_mode_counts': mode_counts,
    }, indent=2) + '\n')
    print('::endgroup::', flush=True)
    return engine, env


@contextlib.contextmanager
def host_lock():
    directory = Path.home() / '.cache/escargot-locks'
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    with (directory / 'performance-host.lock').open('a') as lock:
        deadline = time.monotonic() + 3600
        print('Waiting for exclusive access to the performance host', flush=True)
        while True:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise TimeoutError('Performance host remained busy for one hour')
                time.sleep(10)
        yield


def prepare_perf(report):
    if not shutil.which('perf'):
        packages = ['linux-perf']
        if 'ID=ubuntu' in Path('/etc/os-release').read_text():
            packages = ['linux-tools-common', f'linux-tools-{platform.release()}']
        run(['sudo', '-n', 'apt-get', 'update'], log=report / 'perf-setup.log')
        run(['sudo', '-n', 'apt-get', 'install', '-y', *packages], log=report / 'perf-setup.log')
    command = ['perf', 'stat', '-e', 'cycles,instructions,branches,branch-misses', '--', 'true']
    probe = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if probe.returncode:
        print(probe.stdout, flush=True)
        run(['sudo', '-n', 'sysctl', '-w', 'kernel.perf_event_paranoid=-1'], log=report / 'perf-setup.log')
        run(command, log=report / 'perf-setup.log')


def measure(engine, architecture, env, source, harness, work, report, cpu, repetitions, experiment, profile):
    run([str(engine), '-e', "if (typeof Temporal !== 'object' || typeof Temporal.Instant !== 'function') throw new Error('Temporal is unavailable'); print('Temporal enabled');"],
        env=env, log=report / f'temporal-{architecture}.log')
    if experiment.startswith('bench:'):
        root = harness
        script = harness / experiment.removeprefix('bench:')
    else:
        root = source
        script = source / experiment if experiment else harness / 'tools/arm-benchmark-suites.py'
    script = script.resolve()
    if experiment and (not script.is_relative_to(root.resolve()) or not script.is_file()):
        raise ValueError('Benchmark file must be an existing file inside the selected checkout')
    env = dict(env, ESCARGOT_ENGINE=str(engine), BENCHMARK_ARCHITECTURE=architecture,
               BENCHMARK_CPU=str(cpu), BENCHMARK_REPETITIONS=str(repetitions),
               BENCHMARK_HARNESS=str(harness), BENCHMARK_OUTPUT_DIR=str(report),
               BENCHMARK_PROFILE='true' if profile else 'false')
    (report / f'benchmark-{architecture}.json').write_text(json.dumps({
        'script': experiment or 'tools/arm-benchmark-suites.py',
        'script_sha256': hashlib.sha256(script.read_bytes()).hexdigest(),
        'cpu': cpu, 'repetitions': repetitions, 'profile': profile,
        'scores_only': env.get('BENCHMARK_SCORES_ONLY') == 'true',
    }, indent=2) + '\n')
    run([sys.executable, '-u', str(script)], cwd=source, env=env, log=report / f'run-{architecture}.log')


def main():
    def terminate(*_):
        raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, terminate)
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-arm32', action='store_true')
    args = parser.parse_args()
    if args.build_arm32:
        build(Path('/work/source'), Path('/work'), 'arm32', Path('/report'))
        return
    if platform.machine() != 'aarch64':
        raise RuntimeError('Expected native ARM64 host')
    repetitions = int(os.environ['REPETITIONS'])
    if repetitions not in (1, 3, 5):
        raise ValueError('Invalid repetition count')
    harness = Path(__file__).resolve().parent.parent
    report = Path(tempfile.mkdtemp(prefix='escargot-arm-report.', dir=os.environ['RUNNER_TEMP']))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'report_directory={report}\n')
    cpu = min(os.sched_getaffinity(0))
    profile = os.environ.get('BENCHMARK_PROFILE') == 'true'
    experiment = os.environ.get('BENCHMARK_FILE', '')
    arm32_only = os.environ.get('BENCHMARK_ARM32_ONLY') == 'true'
    scores_only = os.environ.get('BENCHMARK_SCORES_ONLY') == 'true'
    build_only = os.environ.get('BENCHMARK_BUILD_ONLY') == 'true'
    (report / 'metadata.json').write_text(json.dumps({
        'repository': os.environ['SOURCE_REPOSITORY'], 'revision': os.environ['SOURCE_REVISION'],
        'harness_revision': revision(harness), 'runner': os.environ['RUNNER_NAME'],
        'cpu': cpu, 'architecture_order': ['arm32'] if arm32_only else ['arm32', 'arm64'], 'wtb_min_samples': 3,
        'repetitions': repetitions, 'profile': profile, 'scores_only': scores_only,
        'build_only': build_only,
    }, indent=2) + '\n')
    if experiment == 'bench:tools/diagnose-performance-host.py':
        run([sys.executable, str(harness / 'tools/diagnose-performance-host.py')],
            log=report / 'host-lock-diagnosis.json')
        return
    container = f"escargot-arm-benchmark-{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}"
    with host_lock(), tempfile.TemporaryDirectory(prefix='escargot-arm-benchmark.', dir=os.environ['RUNNER_TEMP']) as temporary:
        work = Path(temporary)
        source = work / 'source'
        try:
            print((report / 'metadata.json').read_text(), flush=True)
            run(['lscpu'], log=report / 'host.log')
            run(['git', 'init', str(source)])
            run(['git', '-C', str(source), 'remote', 'add', 'origin', f"https://github.com/{os.environ['SOURCE_REPOSITORY']}.git"])
            run(['git', '-C', str(source), 'fetch', '--depth', '1', 'origin', os.environ['SOURCE_REVISION']])
            run(['git', '-C', str(source), 'checkout', '--detach', 'FETCH_HEAD'])
            if revision(source) != os.environ['SOURCE_REVISION']:
                raise RuntimeError('Unexpected source revision')
            run(['git', '-C', str(source), 'submodule', 'update', '--init', '--jobs', '8', 'third_party/GCutil', 'third_party/walrus'])
            run(['git', '-C', str(harness), 'submodule', 'update', '--init', '--jobs', '8', 'test/vendortest', 'test/octane', 'test/web-tooling-benchmark'])
            sys.path.insert(0, str(harness / 'tools'))
            spec = importlib.util.spec_from_file_location('monthly', harness / 'tools/monthly-engine-benchmark.py')
            monthly = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(monthly)
            # Share the exact generated inputs across source revisions and
            # runner workspaces. Webpack embeds workspace-dependent paths.
            wtb = harness / 'test/web-tooling-benchmark'
            identity = hashlib.sha256((revision(wtb) + hashlib.sha256(
                (harness / 'tools/monthly-engine-benchmark.py').read_bytes()).hexdigest()
                + subprocess.check_output(['node', '--version'], text=True)).encode()).hexdigest()
            bundles = Path.home() / '.cache/escargot-benchmark-inputs' / identity
            if not bundles.is_dir():
                monthly.build_wtb()
                bundles.parent.mkdir(parents=True, exist_ok=True)
                staging = Path(tempfile.mkdtemp(prefix='inputs.', dir=bundles.parent))
                shutil.copytree(wtb / 'dist', staging / 'dist')
                staging.rename(bundles)
            shutil.copytree(bundles / 'dist', wtb / 'dist', dirs_exist_ok=True)
            (report / 'benchmark-inputs.json').write_text(json.dumps({
                'bundle_identity': identity,
                'wtb_revision': revision(wtb),
                'octane_revision': revision(harness / 'test/octane'),
                'vendortest_revision': revision(harness / 'test/vendortest'),
                'wtb_cli_sha256': hashlib.sha256((wtb / 'dist/cli.js').read_bytes()).hexdigest(),
                'wtb_memory_sha256': hashlib.sha256((wtb / 'dist/memory.js').read_bytes()).hexdigest(),
            }, indent=2) + '\n')
            if profile:
                prepare_perf(report)
            run(['docker', 'pull', '--platform', 'linux/arm/v7', 'arm32v7/ubuntu:24.04'])
            image = subprocess.check_output(['docker', 'image', 'inspect', 'arm32v7/ubuntu:24.04', '--format', '{{index .RepoDigests 0}}'], text=True).strip()
            (report / 'arm32-image.txt').write_text(image + '\n')
            install = '''set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends ca-certificates cmake ninja-build pkg-config libicu-dev gcc g++ make python3 file binutils util-linux
exec setpriv --reuid="$BENCHMARK_UID" --regid="$BENCHMARK_GID" --clear-groups python3 -u /bench/tools/run-arm-development-benchmark.py --build-arm32
'''
            run(['docker', 'run', '--name', container, '--platform', 'linux/arm/v7',
                 '-v', f'{work}:/work', '-v', f'{harness}:/bench:ro', '-v', f'{report}:/report',
                 '-w', '/work', '-e', f'BENCHMARK_UID={os.getuid()}', '-e', f'BENCHMARK_GID={os.getgid()}',
                 image, 'bash', '-c', install], log=report / 'arm32-container.log')
            if build_only and arm32_only:
                return
            # Execute the ARM32 binary natively on the host with its container's
            # loader and libraries. Host perf can then measure the actual engine.
            runtime = work / 'runtime-arm32'
            runtime.mkdir()
            run(['docker', 'cp', '-L', f'{container}:/usr/lib/arm-linux-gnueabihf/.', str(runtime)])
            run(['docker', 'cp', '-L', f'{container}:/lib/ld-linux-armhf.so.3', str(runtime / 'ld-linux-armhf.so.3')])
            launcher = work / 'run-arm32-engine'
            engine32 = work / 'build-arm32/escargot'
            launcher.write_text('#!/bin/sh\nexec env LD_LIBRARY_PATH=' + shlex.quote(str(runtime)) + ' '
                                + shlex.join([str(runtime / 'ld-linux-armhf.so.3'), '--library-path', str(runtime), str(engine32)]) + ' "$@"\n')
            launcher.chmod(0o755)
            if profile:
                shutil.copy2(engine32, report / 'escargot-arm32')
            env32 = dict(os.environ, BENCHMARK_RUNTIME_DIRECTORY=str(runtime),
                         BENCHMARK_RUNTIME_LOADER=str(runtime / 'ld-linux-armhf.so.3'))
            if not build_only:
                measure(launcher, 'arm32', env32, source, harness, work, report, cpu, repetitions, experiment, profile)
            if arm32_only:
                return
            engine64, env64 = build(source, work, 'arm64', report)
            if profile:
                shutil.copy2(engine64, report / 'escargot-arm64')
            if not build_only:
                measure(engine64, 'arm64', env64, source, harness, work, report, cpu, repetitions, experiment, profile)
        except BaseException as error:
            (report / 'failure.txt').write_text(f'{type(error).__name__}: {error}\n')
            raise
        finally:
            subprocess.run(['docker', 'rm', '-f', container], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
