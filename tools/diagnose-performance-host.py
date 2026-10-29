#!/usr/bin/env python3
"""Read lock owners and their CI identifiers without changing host state."""
import json
import os
from pathlib import Path
import subprocess


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
        allowed = {'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_JOB', 'GITHUB_WORKFLOW', 'GITHUB_REPOSITORY'}
        entries = (root / 'environ').read_bytes().split(b'\0')
        result['ci'] = {key.decode(): value.decode(errors='replace') for entry in entries
                        if b'=' in entry for key, value in [entry.split(b'=', 1)]
                        if key.decode(errors='replace') in allowed}
    except (OSError, subprocess.CalledProcessError) as error:
        result['read_error'] = type(error).__name__
    return result


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
                data['openers'].append(details)
    print(json.dumps(data, indent=2), flush=True)


if __name__ == '__main__':
    main()
