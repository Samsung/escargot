#!/usr/bin/env python

# Copyright 2020-present Samsung Electronics Co., Ltd.
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



import os
import re
import traceback
import sys
import subprocess

from argparse import ArgumentParser
from os.path import abspath, dirname, join, isfile
from shutil import copy
from subprocess import PIPE, Popen


PROJECT_SOURCE_DIR = dirname(dirname(dirname(dirname(abspath(__file__)))))
SCRIPT_SOURCE_DIR = dirname(abspath(__file__))
DEFAULT_ESCARGOT = join(PROJECT_SOURCE_DIR, 'escargot')
DEFAULT_EXCLUDE_LIST = join(SCRIPT_SOURCE_DIR, 'excludelist.orig.xml')

COLOR_RED = '\033[31m'
COLOR_GREEN = '\033[32m'
COLOR_RESET = '\033[0m'

def run_all_test262(engine, arch):
    template_file = open(join(SCRIPT_SOURCE_DIR, 'template.xml'), 'r')
    empty_list = template_file.read()
    template_file.close()

    exclude_file = open(DEFAULT_EXCLUDE_LIST, 'w')
    exclude_file.write(empty_list)
    exclude_file.write(rear_template())
    exclude_file.close()

    os.environ['GC_FREE_SPACE_DIVISOR'] = '1'
    if arch == 'x86':
        if not isfile(join(PROJECT_SOURCE_DIR, 'backtrace-hooking-32.so')):
            subprocess.call(["gcc", "-shared", "-m32", 
            "-fPIC", "-o", join(PROJECT_SOURCE_DIR, 'backtrace-hooking-32.so'), join(SCRIPT_SOURCE_DIR, 'backtrace-hooking.c')])

        if isfile(join(PROJECT_SOURCE_DIR, 'backtrace-hooking-32.so')):
            os.environ['ESCARGOT_LD_PRELOAD'] = join(PROJECT_SOURCE_DIR, 'backtrace-hooking-32.so')
    else:
        if not isfile(join(PROJECT_SOURCE_DIR, 'backtrace-hooking-64.so')):
            subprocess.call(["gcc", "-shared", "-fPIC", 
            "-o", join(PROJECT_SOURCE_DIR, 'backtrace-hooking-64.so'), join(SCRIPT_SOURCE_DIR, 'backtrace-hooking.c')])

        if isfile(join(PROJECT_SOURCE_DIR, 'backtrace-hooking-64.so')):
            os.environ['ESCARGOT_LD_PRELOAD'] = join(PROJECT_SOURCE_DIR, 'backtrace-hooking-64.so')
        
    proc = Popen(['pypy', join(PROJECT_SOURCE_DIR, 'tools', 'run-tests.py'), 
        '--arch=%s' % arch, '--engine', engine, 'test262'],
        stdout=PIPE, stderr=PIPE)
    out, _ = proc.communicate()

    if not out:
        raise Exception('test262 run with empty exclude list returns no result')

    return out.decode('utf-8')

def front_template():
    template_file = open(join(SCRIPT_SOURCE_DIR, 'template.xml'), 'r')
    template = str(template_file.read())
    template_file.close()
    return template

def rear_template():
    return str('</excludeList>')

def main():
    parser = ArgumentParser()
    parser.add_argument('--engine', metavar='PATH', default=DEFAULT_ESCARGOT,
                        help='path to the engine to be tested (default: %(default)s)')
    parser.add_argument('--arch', metavar='NAME', choices=['x86', 'x86_64'], default='x86_64',
                        help='architecture the engine was built for (%(choices)s; default: %(default)s)')
    args = parser.parse_args()

    full = run_all_test262(args.engine, args.arch)
    if full.find('- All tests succeeded') >= 0:
        sys.exit(COLOR_RED + 'already passed all test262 tcs' + COLOR_RESET)

    with open(DEFAULT_EXCLUDE_LIST, 'w') as out_file:
        summary = full.split('=== Test262 Summary ===')[1]

        out_list = []
        if summary.find('Test262 Failed Tests') > 0:
            failed = summary.split('Test262 Failed Tests')[1]
            failed = failed.split('Failed Tests End')[0]
            failed = failed.replace(' in strict mode', '')
            failed = failed.replace(' in non-strict mode', '')
            failed = failed.split()
            failed = sorted(set(failed))

            for item in failed:
                list_item = '    <test id="' + item + '"><reason>TODO</reason></test>\n';
                out_list.append(list_item)

        if summary.find('Test262 Expected to fail but passed') > 0:
            failed = summary.split('Test262 Expected to fail but passed')[1]
            failed = failed.split('Expected to fail End')[0]
            failed = failed.replace(' in strict mode', '')
            failed = failed.replace(' in non-strict mode', '')
            failed = failed.split()
            failed = sorted(set(failed))

            for item in failed:
                list_item = '    <test id="' + item + '"><reason>TODO</reason></test>\n';
                out_list.append(list_item)
    
        out_list = sorted(set(out_list))
        out_file.write(front_template())
        for item in out_list:
            out_file.write(item)
        out_file.write(rear_template())

    numstat = subprocess.check_output(["git", "diff", "--numstat", DEFAULT_EXCLUDE_LIST]).decode('utf-8').split("\t")
    lines = sorted(re.findall(r'^[+|-][^+|-].*', subprocess.check_output(["git", "diff", "--unified=0", DEFAULT_EXCLUDE_LIST]).decode('utf-8'), re.MULTILINE), key=lambda x:x[:1])

    for i in lines:
        if i[0] == "+":
            print(COLOR_RED + i + COLOR_RESET)
        else:
            print(COLOR_GREEN + i + COLOR_RESET)

    if len(numstat) > 2:
        print(COLOR_RED + "Failed tests: " + numstat[0] + COLOR_RESET)
        print(COLOR_GREEN + "New successful tests: " + numstat[1] + COLOR_RESET)

    print(COLOR_GREEN + 'success: new exclude list generated' + COLOR_RESET)
    sys.exit()


if __name__ == '__main__':
    main()
