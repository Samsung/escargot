#!/usr/bin/env python3
"""Read lock owners and their CI identifiers without changing host state."""
import json
import os
from pathlib import Path
import subprocess
import time


def describe_process(pid):
    root = Path('/proc') / str(pid)
    result = {'pid': pid}
    try:
        status = dict(line.split(':', 1) for line in (root / 'status').read_text().splitlines() if ':' in line)
        result.update({key: status[key].strip() for key in ('Name', 'State', 'PPid', 'Uid', 'Threads') if key in status})
        result['summary'] = subprocess.check_output(
            ['ps', '-p', str(pid), '-o', 'pid=,ppid=,etimes=,pcpu=,comm='], text=True).strip()
        result['executable'] = os.readlink(root / 'exe')
        result['cwd'] = os.readlink(root / 'cwd')
        # Only emit specific non-secret CI identifiers, never full environments.
        allowed = {'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_JOB', 'GITHUB_WORKFLOW', 'GITHUB_REPOSITORY', 'RUNNER_TEMP', 'SOURCE_REVISION'}
        entries = (root / 'environ').read_bytes().split(b'\0')
        result['ci'] = {key.decode(): value.decode(errors='replace') for entry in entries
                        if b'=' in entry for key, value in [entry.split(b'=', 1)]
                        if key.decode(errors='replace') in allowed}
    except (OSError, subprocess.CalledProcessError) as error:
        result['read_error'] = type(error).__name__
    return result



def benchmark_progress(process):
    ci = process.get('ci', {})
    if ci.get('GITHUB_REPOSITORY') != 'Samsung/escargot' or not ci.get('SOURCE_REVISION'):
        return None
    root = Path(ci.get('RUNNER_TEMP', '/nonexistent'))
    metadata_files = sorted(root.glob('escargot-arm-report.*/metadata.json'),
                            key=lambda path: path.stat().st_mtime, reverse=True)
    for path in metadata_files:
        try:
            metadata = json.loads(path.read_text())
            if metadata.get('revision') != ci['SOURCE_REVISION']:
                continue
            directory = path.parent
            result_files = list(directory.glob('adaptive-arithmetic-*/results.json'))
            if not result_files and not (directory / 'build-arm32.log').exists():
                continue
            result = {'directory': str(directory), 'architectures': {}}
            for result_file in result_files:
                data = json.loads(result_file.read_text())
                result['architectures'][data['architecture']] = {
                    'validated': data.get('validated', False),
                    'scores': {label: {suite: [sample.get('score', sample.get('score_runs_per_second'))
                                              for sample in samples]
                                       for suite, samples in suites.items()}
                               for label, suites in data.get('scores', {}).items()},
                    'memory_samples': {label: {suite: len(samples) for suite, samples in suites.items()}
                                       for label, suites in data.get('memory', {}).items()},
                }
            logs = sorted(directory.rglob('*.log'), key=lambda item: item.stat().st_mtime, reverse=True)
            result['latest_logs'] = [{'name': str(item.relative_to(directory)),
                                      'age_seconds': round(time.time() - item.stat().st_mtime)}
                                     for item in logs[:3]]
            return result
        except (OSError, ValueError):
            continue
    return None


def main():
    path = Path.home() / '.cache/escargot-locks/performance-host.lock'
    data = {'lock_path': str(path), 'exists': path.exists(), 'lock_records': [], 'openers': []}
    if path.exists():
        target = path.stat()
        identity = f'{os.major(target.st_dev):02x}:{os.minor(target.st_dev):02x}:{target.st_ino}'
        data['lock_identity'] = identity
        for line in Path('/proc/locks').read_text().splitlines():
            if identity in line.split():
                data['lock_records'].append(line)
        # Include inherited descriptors even if their original lock owner exited.
        for process in Path('/proc').iterdir():
            if not process.name.isdecimal():
                continue
            try:
                descriptors = list((process / 'fd').iterdir())
            except OSError:
                continue
            matches = []
            for descriptor in descriptors:
                try:
                    info = descriptor.stat()
                    if info.st_dev == target.st_dev and info.st_ino == target.st_ino:
                        matches.append(descriptor.name)
                except OSError:
                    pass
            if matches:
                details = describe_process(int(process.name))
                details['lock_fds'] = matches
                details['benchmark_progress'] = benchmark_progress(details)
                data['openers'].append(details)
    print(json.dumps(data, indent=2), flush=True)


if __name__ == '__main__':
    main()
