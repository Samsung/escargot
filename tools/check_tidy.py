#!/usr/bin/env python

# Copyright 2015-present Samsung Electronics Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

import os
import subprocess
import sys

from argparse import ArgumentParser
from difflib import unified_diff
#from check_license import CheckLicenser
from os.path import abspath, dirname, join, relpath, splitext

DEFAULT_DIR = dirname(dirname(abspath(__file__)))

TERM_RED = '\033[1;31m'
TERM_GREEN = '\033[1;32m'
TERM_YELLOW = '\033[1;33m'
TERM_PURPLE = '\033[35m'
TERM_EMPTY = '\033[0m'


clang_format_exts = ['.cpp', '.h']
skip_dirs = ['build', 'CMakeFiles', 'docs', 'out', 'test', 'tools', 'double_conversion', 'GCutil', 'lz4', 'rapidjson', 'windows', '.git']
skip_files = []


class Stats(object):
    def __init__(self):
        self.files = 0
        self.lines = 0
        self.empty_lines = 0
        self.errors = 0


def is_checked_by_clang(file):
    _, ext = splitext(file)
    return ext in clang_format_exts and file not in skip_files


def check_tidy(src_dir, update, clang_format, stats):
    print('processing directory: %s' % src_dir)

    for dirpath, _, filenames in os.walk(src_dir):
        if any(d in relpath(dirpath, src_dir) for d in skip_dirs):
            continue

        for file in [join(dirpath, name) for name in filenames if is_checked_by_clang(name)]:
            def report_error(msg, line=None):
                print('%s%s:%s %s%s' % (TERM_YELLOW, file, '%d:' % line if line else '', msg, TERM_EMPTY))
                stats.errors += 1

            with open(file, 'r') as f:
                original = f.readlines()
            formatted = subprocess.check_output([clang_format, '-style=file', file])

            if update:
                with open(file, 'w') as f:
                    f.write(formatted)

            stats.files += 1
            stats.lines += len(original)

            for lineno, line in enumerate(original):
                lineno += 1

                if '\t' in line:
                    report_error('TAB character', lineno)
                if '\r' in line:
                    report_error('CR character', lineno)
                if line.endswith(' \n') or line.endswith('\t\n'):
                    report_error('trailing whitespace', lineno)
                if not line.endswith('\n'):
                    report_error('line ends without NEW LINE character', lineno)

                if not line.strip():
                    stats.empty_lines += 1

            diff = list(unified_diff(original, formatted.splitlines(True)))
            if diff:
                report_error('format error')
                for diffline in diff:
                    print(diffline, end='')

            # if not CheckLicenser.check(file):
            #     report_error('incorrect license')


def main():
    parser = ArgumentParser(description='Escargot Source Format Checker and Updater')
    parser.add_argument('--clang-format', metavar='PATH', default='clang-format-3.9',
                        help='path to clang-format (default: %(default)s)')
    parser.add_argument('--update', action='store_true',
                        help='reformat files')
    parser.add_argument('--dir', metavar='PATH', default=DEFAULT_DIR,
                        help='directory to process (default: %(default)s)')
    args = parser.parse_args()

    stats = Stats()

    check_tidy(args.dir, args.update, args.clang_format, stats)

    print()
    print('* Total number of files: %d' % stats.files)
    print('* Total lines of code: %d' % stats.lines)
    print('* Total non-empty lines of code: %d' % (stats.lines - stats.empty_lines))
    print('%s* Total number of errors: %d%s' % (TERM_RED if stats.errors else TERM_GREEN,
                                                stats.errors,
                                                TERM_EMPTY))

    if args.update:
        print()
        print('All files reformatted, check for changes with `git diff`.');

    sys.exit(1 if stats.errors else 0)


if __name__ == '__main__':
    main()
