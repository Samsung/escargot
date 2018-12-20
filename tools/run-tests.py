#!/usr/bin/env python

# Copyright 2018-present Samsung Electronics Co., Ltd.
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
import traceback
import sys

from argparse import ArgumentParser
from difflib import unified_diff
from glob import glob
from os.path import abspath, dirname, join
from shutil import copy
from subprocess import PIPE, Popen


PROJECT_SOURCE_DIR = dirname(dirname(abspath(__file__)))
DEFAULT_ESCARGOT = join(PROJECT_SOURCE_DIR, 'escargot')


COLOR_RED = '\033[31m'
COLOR_GREEN = '\033[32m'
COLOR_YELLOW = '\033[33m'
COLOR_BLUE = '\033[34m'
COLOR_PURPLE = '\033[35m'
COLOR_RESET = '\033[0m'


RUNNERS = {}
DEFAULT_RUNNERS = []


class runner(object):

    def __init__(self, suite, default=False):
        self.suite = suite
        self.default = default

    def __call__(self, fn):
        RUNNERS[self.suite] = fn
        if self.default:
            DEFAULT_RUNNERS.append(self.suite)
        return fn


def run(args, cwd=None, env=None, stdout=None, checkresult=True):
    if cwd:
        print(COLOR_BLUE + 'cd ' + cwd + ' && \\' + COLOR_RESET)
    if env:
        for var, val in sorted(env.items()):
            print(COLOR_BLUE + var + '=' + val + ' \\' + COLOR_RESET)
    print(COLOR_BLUE + ' '.join(args) + COLOR_RESET)

    if env is not None:
        full_env = dict(os.environ)
        full_env.update(env)
        env = full_env

    proc = Popen(args, cwd=cwd, env=env, stdout=stdout)
    out, _ = proc.communicate()
    if out:
        print(out)

    if checkresult and proc.returncode:
        raise Exception('command `%s` exited with %s' % (' '.join(args), proc.returncode))
    return out


def readfile(filename):
    with open(filename, 'r') as f:
        return f.readlines()


@runner('sunspider')
def run_sunspider(engine, arch):
    run([join('.', 'sunspider'),
        '--shell', engine,
        '--suite', 'sunspider-1.0.2'],
        cwd=join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SunSpider'))


@runner('sunspider-js', default=True)
def run_sunspider_js(engine, arch):
    run([engine] + glob(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SunSpider', 'tests', 'sunspider-1.0.2', '*.js')))


@runner('octane', default=True)
def run_octane(engine, arch):
    OCTANE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'octane')

    stdout = run([engine, 'run.js'],
                 cwd=OCTANE_DIR,
                 stdout=PIPE)
    if 'Score' not in stdout:
        raise Exception('no "Score" in stdout')


@runner('internal', default=True)
def run_internal_test(engine, arch):
    INTERNAL_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver')
    INTERNAL_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'internal')

    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-cases.txt'), join(INTERNAL_DIR, 'internal-test-cases.txt'))
    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-driver.py'), join(INTERNAL_DIR, 'driver.py'))

    run(['python', 'driver.py', engine, 'internal-test-cases.txt'],
        cwd=INTERNAL_DIR)


@runner('test262', default=True)
def run_test262(engine, arch):
    TEST262_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test')
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    copy(join(TEST262_OVERRIDE_DIR, 'excludelist.orig.xml'), join(TEST262_DIR, 'test', 'config', 'excludelist.xml'))
    copy(join(TEST262_OVERRIDE_DIR, 'test262.py'), join(TEST262_DIR, 'tools', 'packaging', 'test262.py'))

    run(['python', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--full-summary'],
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific'})


@runner('test262-master')
def run_test262_master(engine, arch):
    TEST262_HARNESS_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test')
    TEST262_HARNESS_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262-harness-py')

    copy(join(TEST262_HARNESS_OVERRIDE_DIR, 'test262-harness-py-excludelist.xml'), join(TEST262_HARNESS_DIR, 'excludelist.xml'))
    copy(join(TEST262_HARNESS_OVERRIDE_DIR, 'test262-harness-py-test262.py'), join(TEST262_HARNESS_DIR, 'src', 'test262.py'))

    run(['python', join(TEST262_HARNESS_DIR, 'src', 'test262.py'),
         '--command', engine,
         '--tests', join(PROJECT_SOURCE_DIR, 'test', 'test262-master'),
         '--full-summary'])


@runner('spidermonkey', default=True)
def run_spidermonkey(engine, arch):
    SPIDERMONKEY_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver')
    SPIDERMONKEY_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SpiderMonkey')

    run(['nodejs', join(PROJECT_SOURCE_DIR, 'node_modules', '.bin', 'babel'), join(SPIDERMONKEY_DIR, 'ecma_6', 'Promise'), '--out-dir', join(SPIDERMONKEY_DIR, 'ecma_6', 'Promise')])
    copy(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.shell.js'), join(SPIDERMONKEY_DIR, 'shell.js'))
    copy(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.js1_8_1.jit.shell.js'), join(SPIDERMONKEY_DIR, 'js1_8_1', 'jit', 'shell.js'))
    copy(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.ecma_6.shell.js'), join(SPIDERMONKEY_DIR, 'ecma_6', 'shell.js'))
    copy(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.ecma_6.TypedArray.shell.js'), join(SPIDERMONKEY_DIR, 'ecma_6', 'TypedArray', 'shell.js'))
    copy(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.ecma_6.Math.shell.js'), join(SPIDERMONKEY_DIR, 'ecma_6', 'Math', 'shell.js'))

    run([join(SPIDERMONKEY_DIR, 'jstests.py'),
         '--no-progress', '-s',
         '--xul-info', '%s-gcc3:Linux:false' % arch,
         '--exclude-file', join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.excludelist.txt'),
         engine,
         '--output-file', join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.%s.log.txt' % arch),
         '--failure-file', join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.%s.gen.txt' % arch),
         'ecma/', 'ecma_2/', 'ecma_3/', 'ecma_3_1/', 'ecma_5/',
         'ecma_6/Promise', 'ecma_6/TypedArray', 'ecma_6/ArrayBuffer', 'ecma_6/Number', 'ecma_6/Math',
         'js1_1/', 'js1_2/',  'js1_3/', 'js1_4/', 'js1_5/', 'js1_6/', 'js1_7/', 'js1_8/', 'js1_8_1/', 'js1_8_5/',
         'shell/', 'supporting/'],
        env={'LOCALE': 'en_US'})

    orig = sorted(readfile(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.%s.orig.txt' % arch)))
    gen = sorted(readfile(join(SPIDERMONKEY_OVERRIDE_DIR, 'spidermonkey.%s.gen.txt' % arch)))
    diff = list(unified_diff(orig, gen))
    if diff:
        for diffline in diff:
            print(diffline)
        raise Exception('failure files differ')


@runner('jsc-stress', default=True)
def run_jsc_stress(engine, arch):
    JSC_DIR = join('test', 'vendortest', 'driver')

    run([join(JSC_DIR, 'driver.py'),
         '-s', 'stress',
         '-a', arch],
        cwd=PROJECT_SOURCE_DIR,
        env={'PYTHONPATH': '.'})


def _run_jetstream(engine, target_test):
    JETSTREAM_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'jetstream')
    JETSTREAM_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'JetStream-1.1')

    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.CDjsSetup.js'), join(JETSTREAM_DIR, 'CDjsSetup.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.OctaneSetup.js'), join(JETSTREAM_DIR, 'OctaneSetup.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.Octane2Setup.js'), join(JETSTREAM_DIR, 'Octane2Setup.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.SimpleSetup.js'), join(JETSTREAM_DIR, 'SimpleSetup.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.SunSpiderSetup.js'), join(JETSTREAM_DIR, 'SunSpiderSetup.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.cdjs.util.js'), join(JETSTREAM_DIR, 'cdjs', 'util.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.runOnePlan.js'), join(JETSTREAM_DIR, 'runOnePlan.js'))
    copy(join(JETSTREAM_OVERRIDE_DIR, 'jetstream.run.sh'), join(JETSTREAM_DIR, 'run.sh'))

    run([join('.', 'run.sh'), engine, target_test],
        cwd=JETSTREAM_DIR)
    run(['python', join(JETSTREAM_OVERRIDE_DIR, 'parsingResults.py'),
         join(JETSTREAM_OVERRIDE_DIR, 'jetstream-result-raw.res'),
         target_test])
    if 'NaN' in ''.join(readfile(join(JETSTREAM_OVERRIDE_DIR, 'jetstream-result-raw.res'))):
        raise Exception('result contains "NaN"')


@runner('jetstream-only-simple', default=True)
def run_jetstream_only_simple(engine, arch):
    _run_jetstream(engine, 'simple')


@runner('jetstream-only-cdjs', default=True)
def run_jetstream_only_cdjs(engine, arch):
    _run_jetstream(engine, 'cdjs')


@runner('jetstream-only-sunspider')
def run_jetstream_only_sunspider(engine, arch):
    _run_jetstream(engine, 'sunspider')


@runner('jetstream-only-octane')
def run_jetstream_only_octane(engine, arch):
    _run_jetstream(engine, 'octane')


@runner('chakracore', default=True)
def run_chakracore(engine, arch):
    CHAKRACORE_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'chakracore')
    CHAKRACORE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'ChakraCore')

    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.run.sh'), join(CHAKRACORE_DIR, 'run.sh'))
    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.include.js'), join(CHAKRACORE_DIR, 'include.js'))
    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.rlexedirs.xml'), join(CHAKRACORE_DIR, 'rlexedirs.xml'))

    run(['bash', '-c', join('build', 'command_chakracore_%s.sh' % arch)],
         cwd=PROJECT_SOURCE_DIR)


@runner('v8', default=True)
def run_v8(engine, arch):
    V8_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'v8')
    V8_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'v8')

    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.status'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.status'))
    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.js'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.js'))
    copy(join(V8_OVERRIDE_DIR, 'v8.run-tests.py'), join(V8_DIR, 'tools', 'run-tests.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.testsuite.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'testsuite.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.execution.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'execution.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.progress.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'progress.py'))

    arch = {'x86': 'x32', 'x86_64': 'x64'}[arch]

    stdout = run([join(V8_DIR, 'tools', 'run-tests.py'),
                  '--timeout=120',
                  '--quickcheck',
                  '--no-presubmit', '--no-variants',
                  '--arch-and-mode=%s.release' % arch,
                  '--shell-dir', '../../../',
                  '--escargot',
                  '--report',
                  '-p', 'verbose',
                  '--no-sorting',
                  'mjsunit'],
                 stdout=PIPE,
                 checkresult=False)
    # FIXME: V8 test runner tends to exit with 2 (most probably the result of
    # a `self.remaining != 0` in `testrunner.local.execution.Runner.Run()`.
    # Couldn't find out yet why that happens.
    # NOTE: This has also been suppressed in cmake/make-based executors by
    # piping the output of run-test.py into a tee. In case of pipes, the
    # observed exit code is that of the last element of the pipe, which is
    # tee in that case (which is always 0).

    with open(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'v8.%s.mjsunit.gen.txt' % arch), 'w') as msunit_gen_txt:
        msunit_gen_txt.write(stdout)
    run(['diff',
         join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'v8.%s.mjsunit.orig.txt' % arch),
         join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'v8.%s.mjsunit.gen.txt' % arch)])


def main():
    parser = ArgumentParser(description='Escargot Test Suite Runner')
    parser.add_argument('--engine', metavar='PATH', default=DEFAULT_ESCARGOT,
                        help='path to the engine to be tested (default: %(default)s)')
    parser.add_argument('--arch', metavar='NAME', choices=['x86', 'x86_64'], default='x86_64',
                        help='architecture the engine was built for (%(choices)s; default: %(default)s)')
    parser.add_argument('suite', metavar='SUITE', nargs='*', default=sorted(DEFAULT_RUNNERS),
                        help='test suite to run (%s; default: %s)' % (', '.join(sorted(RUNNERS.keys())), ' '.join(sorted(DEFAULT_RUNNERS))))
    args = parser.parse_args()

    for suite in args.suite:
        if suite not in RUNNERS:
            parser.error('invalid test suite: %s' % suite)

    success, fail = [], []

    for suite in args.suite:
        print(COLOR_PURPLE + 'running test suite: ' + suite + COLOR_RESET)
        try:
            RUNNERS[suite](args.engine, args.arch)
            success += [suite]
        except Exception as e:
            print('\n'.join(COLOR_YELLOW + line + COLOR_RESET for line in traceback.format_exc().splitlines()))
            fail += [suite]

    if success:
        print(COLOR_GREEN + sys.argv[0] + ': success: ' + ', '.join(success) + COLOR_RESET)
    sys.exit(COLOR_RED + sys.argv[0] + ': fail: ' + ', '.join(fail) + COLOR_RESET if fail else None)


if __name__ == '__main__':
    main()
