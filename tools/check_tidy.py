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

import sys
import os
import fileinput

#from check_license import CheckLicenser
import os.path as fs
import subprocess

TERM_RED = "\033[1;31m"
TERM_YELLOW = "\033[1;33m"
TERM_GREEN = "\033[1;32m"
TERM_BLUE = "\033[1;34m"
TERM_EMPTY = "\033[0m"


count_err = 0

interesting_exts = ['.cpp', '.h', '.js', '.py', '.sh', '.cmake']
clang_format_exts = ['.cpp', '.h']
skip_dirs = ['deps', 'build', 'third_party', 'out', 'tools', '.git', 'test', 'CMakeFiles']
skip_files = []


def report_error_name_line(name, line, msg):
    global count_err
    if line is None:
        print("%s: %s" % (name, msg))
    else:
        print("%s:%d: %s" % (name, line, msg))
    count_err += 1


def report_error(msg):
    report_error_name_line(fileinput.filename(), fileinput.filelineno(), msg)


def is_interesting(file):
    _, ext = fs.splitext(file)
    return ext in interesting_exts and file not in skip_files


def is_checked_by_clang(file):
    _, ext = fs.splitext(file)
    return ext in clang_format_exts and file not in skip_files


def check_tidy(src_dir, update=False):
    print src_dir
    count_lines = 0
    count_empty_lines = 0

    option = ''
    if update:
        print("All files will be fomatted. Check the change: git diff");
        option = '-i'

    for (dirpath, dirnames, filenames) in os.walk(src_dir):
        if any(d in fs.relpath(dirpath, src_dir) for d in skip_dirs):
            continue

        files = [fs.join(dirpath, name) for name in filenames
                 if is_checked_by_clang(name)]

        if not files:
            continue

        for file in files:
            if is_checked_by_clang(file):
                if update:
                    formatted = subprocess.check_output(['clang-format-3.8',
                        '-style=file', option, file])
                else:
                    formatted = subprocess.check_output(['clang-format-3.8',
                        '-style=file', file])
                    f = open(file + '.formatted', 'w')
                    f.write(formatted)
                    f.close()
                    if subprocess.call(['diff'] + [file, file+'.formatted']) != 0:
                        print(file + '\n')
                    os.remove(file + '.formatted')

        for line in fileinput.input(files):
            if '\t' in line:
                report_error('TAB character')
            if '\r' in line:
                report_error('CR character')
            if line.endswith(' \n') or line.endswith('\t\n'):
                report_error('trailing whitespace')
            if not line.endswith('\n'):
                report_error('line ends without NEW LINE character')

#            if fileinput.isfirstline():
#                if not CheckLicenser.check(fileinput.filename()):
#                    report_error_name_line(fileinput.filename(),
#                                           None,
#                                       'incorrect license')

            count_lines += 1
            if not line.strip():
                count_empty_lines += 1

    print "* total lines of code: %d" % count_lines
    print ("* total non-blank lines of code: %d"
           % (count_lines - count_empty_lines))
    print "%s* total errors: %d%s" % (TERM_RED if count_err > 0 else TERM_GREEN,
                                      count_err,
                                      TERM_EMPTY)
    print

    return count_err == 0

if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == 'update':
        check_tidy("./", True)
    else:
        check_tidy("./")
