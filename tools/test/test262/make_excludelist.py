#!/usr/bin/env python3

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
import tempfile
import xml.etree.ElementTree as ET
from xml.sax.saxutils import quoteattr

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
        
    proc = Popen(['python3', join(PROJECT_SOURCE_DIR, 'tools', 'run-tests.py'), 
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

def write_exclude_list(contents):
    """Replace the canonical list without leaving a partial XML file."""
    fd, temporary_name = tempfile.mkstemp(prefix='excludelist.', suffix='.xml',
                                          dir=SCRIPT_SOURCE_DIR)
    try:
        with os.fdopen(fd, 'w') as exclude_file:
            exclude_file.write(contents)
        os.replace(temporary_name, DEFAULT_EXCLUDE_LIST)
    except:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise

def failed_test_ids(summary, heading, footer):
    """Return IDs from one Test262 summary section."""
    start = summary.find(heading)
    if start < 0:
        return set()
    start += len(heading)
    end = summary.find(footer, start)
    if end < 0:
        raise RuntimeError('incomplete Test262 summary: missing ' + footer)

    # A negative test that "expected to fail but passed" is also an engine
    # failure, so callers intentionally add it to the exclusion list.
    return set(re.findall(r'^  ([^\n]+?) in (?:strict|non-strict) mode$',
                          summary[start:end], re.MULTILINE))

def test_ids(contents):
    return {test.attrib['id'] for test in ET.fromstring(contents).findall('test')}


def update_exclude_list(previous, template, failed):
    """Keep existing XML text and edit only entries whose membership changes."""
    template_ids = test_ids(template + rear_template())
    wanted = set(failed) | template_ids
    previous_ids = test_ids(previous)
    # Match comments separately so examples inside comments remain untouched.
    entries = re.compile(r'<!--.*?-->|(?:^[ \t]*)?<test\b[^>]*>.*?</test\s*>'
                         r'(?:[ \t]*\r?\n)?', re.MULTILINE | re.DOTALL)

    def keep_entry(match):
        entry = match.group()
        if entry.startswith('<!--'):
            return entry
        return entry if ET.fromstring(entry).attrib['id'] in wanted else ''

    contents = entries.sub(keep_entry, previous)
    additions = []
    # Newly mandatory entries retain the reason supplied by the template.
    for match in entries.finditer(template):
        entry = match.group()
        if entry.startswith('<!--'):
            continue
        if ET.fromstring(entry).attrib['id'] not in previous_ids:
            additions.append(entry.rstrip('\r\n') + '\n')
    for item in sorted(set(failed) - template_ids - previous_ids):
        additions.append('    <test id=' + quoteattr(item)
                         + '><reason>TODO</reason></test>\n')
    if additions:
        end = contents.rfind('</excludeList>')
        prefix = contents[:end]
        if prefix and not prefix.endswith('\n'):
            prefix += '\n'
        contents = prefix + ''.join(additions) + contents[end:]
    if test_ids(contents) != wanted:
        raise RuntimeError('generated exclude list does not match test IDs')
    return contents


def report_changes(previous, generated):
    old, new = test_ids(previous), test_ids(generated)
    for item in sorted(new - old):
        print(COLOR_RED + '+ ' + item + COLOR_RESET)
    for item in sorted(old - new):
        print(COLOR_GREEN + '- ' + item + COLOR_RESET)
    print(COLOR_RED + 'New exclusions: ' + str(len(new - old)) + COLOR_RESET)
    print(COLOR_GREEN + 'Removed exclusions: ' + str(len(old - new)) + COLOR_RESET)


def main():
    parser = ArgumentParser()
    parser.add_argument('--engine', metavar='PATH', default=DEFAULT_ESCARGOT,
                        help='path to the engine to be tested (default: %(default)s)')
    parser.add_argument('--arch', metavar='NAME', choices=['x86', 'x86_64'], default='x86_64',
                        help='architecture the engine was built for (%(choices)s; default: %(default)s)')
    args = parser.parse_args()

    with open(DEFAULT_EXCLUDE_LIST, 'r') as exclude_file:
        previous_list = exclude_file.read()

    try:
        full = run_all_test262(args.engine, args.arch)
        summary_marker = '=== Test262 Summary ==='
        if summary_marker not in full:
            raise RuntimeError('Test262 did not produce a summary; keeping the previous exclude list')
        summary = full.split(summary_marker, 1)[1]

        failed = failed_test_ids(summary, 'Test262 Failed Tests', 'Failed Tests End')
        failed.update(failed_test_ids(summary, 'Test262 Expected to fail but passed',
                                      'Expected to fail End'))
        if not failed and '- All tests succeeded' not in summary:
            raise RuntimeError('Test262 summary has no failure IDs or success marker')

        generated_list = update_exclude_list(previous_list, front_template(), failed)
        write_exclude_list(generated_list)
    except:
        # run_all_test262 temporarily installs a minimal list so it can find
        # every failure. Never leave that temporary list behind on an error.
        write_exclude_list(previous_list)
        raise

    report_changes(previous_list, generated_list)

    print(COLOR_GREEN + 'success: new exclude list generated' + COLOR_RESET)
    sys.exit()


if __name__ == '__main__':
    main()
