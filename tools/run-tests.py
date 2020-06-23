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
import time

from argparse import ArgumentParser
from difflib import unified_diff
from glob import glob
from os.path import abspath, basename, dirname, join, relpath
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


def run(args, cwd=None, env=None, stdout=None, checkresult=True, report=False):
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

    proc = Popen(args, cwd=cwd, env=env, stdout=PIPE if report else stdout)

    counter = 0

    while report:
        nextline = proc.stdout.readline()
        if nextline == '' and proc.poll() is not None:
            break
        stdout.write(nextline)
        stdout.flush()
        if counter % 250 == 0:
            print('Ran %d tests..' % (counter))
        counter += 1

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
    run([engine] + sorted(glob(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SunSpider', 'tests', 'sunspider-1.0.2', '*.js'))))


@runner('octane', default=True)
def run_octane(engine, arch):
    max_retry_count = 3
    try_count = 0
    last_error = None
    while try_count < max_retry_count:
        try:
            OCTANE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'octane')

            stdout = run(['/usr/bin/time', '-f', '%M', '-o', 'mem.txt', engine, 'run.js'],
                         cwd=OCTANE_DIR,
                         stdout=PIPE)
            f = open(join(OCTANE_DIR, 'mem.txt'))
            for s in f:
                rss = s.strip("\n")
            mem = int(rss)
            print('Octane maximum resident set size: ' + str(mem) + 'KB')

            if arch == str('x86_64') and mem > 250000:
                raise Exception('Exceed memory consumption')
            if arch == str('x86') and mem > 150000:
                raise Exception('Exceed memory consumption')

            if 'Score' not in stdout:
                raise Exception('no "Score" in stdout')
            return
        except Exception as e:
            last_error = e
            try_count = try_count + 1


    raise last_error


@runner('modifiedVendorTest', default=True)
def run_internal_test(engine, arch):
    INTERNAL_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'ModifiedVendorTest')
    INTERNAL_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'ModifiedVendorTest')

    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-cases.txt'), join(INTERNAL_DIR, 'internal-test-cases.txt'))
    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-driver.py'), join(INTERNAL_DIR, 'driver.py'))

    run(['python', 'driver.py', engine, 'internal-test-cases.txt'],
        cwd=INTERNAL_DIR)


@runner('test262', default=True)
def run_test262(engine, arch):
    TEST262_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'test262')
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    copy(join(TEST262_OVERRIDE_DIR, 'excludelist.orig.xml'), join(TEST262_DIR, 'excludelist.xml'))
    copy(join(TEST262_OVERRIDE_DIR, 'cth.js'), join(TEST262_DIR, 'harness', 'cth.js'))
    copy(join(TEST262_OVERRIDE_DIR, 'testIntl.js'), join(TEST262_DIR, 'harness', 'testIntl.js'))

    copy(join(TEST262_OVERRIDE_DIR, 'parseTestRecord.py'), join(TEST262_DIR, 'tools', 'packaging', 'parseTestRecord.py'))
    copy(join(TEST262_OVERRIDE_DIR, 'test262.py'), join(TEST262_DIR, 'tools', 'packaging', 'test262.py')) # for parallel running (we should re-implement this for es6 suite)

    mode = "--fast_mode" if arch == str('x86') else ""
    stdout = run(['pypy', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         mode,
         '--full-summary'],
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific'},
        stdout=PIPE)

    summary = stdout.split('=== Test262 Summary ===')[1]
    if summary.find('- All tests succeeded') < 0:
        raise Exception('test262 failed')
    print('test262: All tests passed')

@runner('test262-strict', default=True)
def run_test262_strict(engine, arch):
    TEST262_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'test262')
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    copy(join(TEST262_OVERRIDE_DIR, 'excludelist.orig.xml'), join(TEST262_DIR, 'excludelist.xml'))
    copy(join(TEST262_OVERRIDE_DIR, 'test262.py'), join(TEST262_DIR, 'tools', 'packaging', 'test262.py')) # for parallel running (we should re-implement this for es6 suite)

    out = open('test262-strict_out', 'w')

    run(['pypy', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--full-summary',
         '--strict_only'],
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific'},
        stdout=out,
        report=True)

    out.close()

    with open('test262-strict_out', 'r') as out:
        full = out.read()
        summary = full.split('=== Summary ===')[1]
        if summary.find('- All tests succeeded') < 0:
            print(summary)
            raise Exception('test262-strict failed')

        print('test262-strict: All tests passed')


@runner('test262-nonstrict', default=True)
def run_test262_nonstrict(engine, arch):
    TEST262_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'test262')
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    copy(join(TEST262_OVERRIDE_DIR, 'excludelist.orig.xml'), join(TEST262_DIR, 'excludelist.xml'))
    copy(join(TEST262_OVERRIDE_DIR, 'test262.py'), join(TEST262_DIR, 'tools', 'packaging', 'test262.py')) # for parallel running (we should re-implement this for es6 suite)

    out = open('test262-nonstrict_out', 'w')

    run(['pypy', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--full-summary',
         '--non_strict_only'],
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific'},
        stdout=out,
        report=True)

    out.close()

    with open('test262-nonstrict_out', 'r') as out:
        full = out.read()
        summary = full.split('=== Summary ===')[1]
        if summary.find('- All tests succeeded') < 0:
            print(summary)
            raise Exception('test262-nonstrict failed')

        print('test262-nonstrict: All tests passed')


@runner('spidermonkey', default=True)
def run_spidermonkey(engine, arch):
    SPIDERMONKEY_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'spidermonkey')
    SPIDERMONKEY_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SpiderMonkey')

    run([join(SPIDERMONKEY_DIR, 'jstests.py'),
         '--no-progress', '-s',
         '--timeout', '500',
         '--xul-info', '%s-gcc3:Linux:false' % arch,
         '--exclude-file', join(SPIDERMONKEY_OVERRIDE_DIR, 'excludelist.txt'),
         engine,
         '--output-file', join(SPIDERMONKEY_OVERRIDE_DIR, '%s.log.txt' % arch),
         '--failure-file', join(SPIDERMONKEY_OVERRIDE_DIR, '%s.gen.txt' % arch),
         'non262/Array', 'non262/ArrayBuffer', 'non262/arrow-functions', 'non262/BigInt', 'non262/Boolean', 'non262/class', 'non262/comprehensions', 'non262/DataView', 'non262/Date',
         'non262/destructuring', 'non262/Error', 'non262/eval', 'non262/Exceptions', 'non262/execution-contexts', 'non262/expressions', 'non262/extensions', 'non262/fields', 'non262/Function',
         'non262/GC', 'non262/generators', 'non262/get-set', 'non262/global', 'non262/Intl', 'non262/iterable', 'non262/jit', 'non262/JSON', 'non262/lexical', 'non262/lexical-conventions',
         'non262/lexical-environment', 'non262/literals', 'non262/Map', 'non262/Math', 'non262/misc', 'non262/module', 'non262/Number', 'non262/object', 'non262/operators', 'non262/pipeline',
         'non262/Promise', 'non262/Proxy', 'non262/Reflect', 'non262/reflect-parse', 'non262/RegExp', 'non262/regress', 'non262/Scope', 'non262/Script', 'non262/Set', 'non262/statements',
         'non262/strict', 'non262/String', 'non262/Symbol', 'non262/syntax', 'non262/template-strings', 'non262/TypedArray', 'non262/TypedObject', 'non262/types', 'non262/Unicode', 'non262/WeakMap'],
        env={'LOCALE': 'en_US'})

    orig = sorted(readfile(join(SPIDERMONKEY_OVERRIDE_DIR, '%s.orig.txt' % arch)))
    gen = sorted(readfile(join(SPIDERMONKEY_OVERRIDE_DIR, '%s.gen.txt' % arch)))
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
         '-e', engine,
         '-a', arch],
        cwd=PROJECT_SOURCE_DIR,
        env={'PYTHONPATH': '.'})


def _run_regression_tests(engine, assert_js, files, is_fail):
    fails = 0
    for file in files:
        proc = Popen([engine, assert_js, file], stdout=PIPE)
        out, _ = proc.communicate()

        if is_fail and proc.returncode or not is_fail and not proc.returncode:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))
        else:
            print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, file, COLOR_RESET))
            print(out)

            fails += 1

    return fails


@runner('regression-tests', default=True)
def run_regression_tests(engine, arch):
    REGRESSION_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'regression-tests')
    REGRESSION_XFAIL_DIR = join(REGRESSION_DIR, 'xfail')
    REGRESSION_ASSERT_JS = join(REGRESSION_DIR, 'assert.js')

    print('Running regression tests:')
    xpass = glob(join(REGRESSION_DIR, 'issue-*.js'))
    xpass_result = _run_regression_tests(engine, REGRESSION_ASSERT_JS, xpass, False)

    print('Running regression tests expected to fail:')
    xfail = glob(join(REGRESSION_XFAIL_DIR, 'issue-*.js'))
    xfail_result = _run_regression_tests(engine, REGRESSION_ASSERT_JS, xfail, True)

    tests_total = len(xpass) + len(xfail)
    fail_total = xfail_result + xpass_result
    print('TOTAL: %d' % (tests_total))
    print('%sPASS : %d%s' % (COLOR_GREEN, tests_total - fail_total, COLOR_RESET))
    print('%sFAIL : %d%s' % (COLOR_RED, fail_total, COLOR_RESET))

    if fail_total > 0:
        raise Exception("Regression tests failed")


def _run_jetstream(engine, target_test):
    JETSTREAM_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'jetstream')
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


@runner('jetstream-only-simple-parallel-1')
def run_jetstream_only_simple_parallel_1(engine, arch):
    _run_jetstream(engine, 'simple-1')


@runner('jetstream-only-simple-parallel-2')
def run_jetstream_only_simple_parallel_2(engine, arch):
    _run_jetstream(engine, 'simple-2')


@runner('jetstream-only-simple-parallel-3')
def run_jetstream_only_simple_parallel_3(engine, arch):
    _run_jetstream(engine, 'simple-3')


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
    CHAKRACORE_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'chakracore')
    CHAKRACORE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'ChakraCore')

    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.run.sh'), join(CHAKRACORE_DIR, 'run.sh'))
    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.include.js'), join(CHAKRACORE_DIR, 'include.js'))
    copy(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.rlexedirs.xml'), join(CHAKRACORE_DIR, 'rlexedirs.xml'))
    for rlexexml in glob(join(CHAKRACORE_OVERRIDE_DIR, '*.rlexe.xml')):
        dirname, _, filename = basename(rlexexml).partition('.')
        copy(rlexexml, join(CHAKRACORE_DIR, dirname, filename))
    for js in [join(CHAKRACORE_DIR, 'DynamicCode', 'eval-nativecodedata.js'),
               join(CHAKRACORE_DIR, 'utf8', 'unicode_digit_as_identifier_should_work.js')]:
        with open(js, 'a') as f:
            f.write("WScript.Echo('PASS');")

    stdout = run(['bash', join(CHAKRACORE_DIR, 'run.sh'), relpath(engine)],
                 stdout=PIPE)
    with open(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'chakracore.%s.gen.txt' % arch), 'w') as gen_txt:
        gen_txt.write(stdout)
    run(['diff',
         join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.%s.orig.txt' % arch),
         join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'chakracore.%s.gen.txt' % arch)])


@runner('v8', default=True)
def run_v8(engine, arch):
    V8_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'v8')
    V8_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'v8')

    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.status'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.status'))
    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.js'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.js'))
    copy(join(V8_OVERRIDE_DIR, 'v8.run-tests.py'), join(V8_DIR, 'tools', 'run-tests.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.testsuite.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'testsuite.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.execution.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'execution.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.progress.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'progress.py'))

    arch = {'x86': 'x32', 'x86_64': 'x64'}[arch]

    if (engine is "escargot"):
        shell_str = "../../../escargot"
    else:
        shell_str = engine

    stdout = run([join(V8_DIR, 'tools', 'run-tests.py'),
                  '--timeout=120',
                  '--quickcheck',
                  '--no-presubmit', '--no-variants',
                  '--arch-and-mode=%s.release' % arch,
                  '--shell', shell_str,
                  '--escargot',
                  '--report',
                  '-p', 'verbose',
                  '--no-sorting',
                  '--no-network',
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

    if '=== All tests succeeded' not in stdout:
        raise Exception('Not all tests succeeded')

@runner('new-es', default=True)
def run_new_es(engine, arch):
    NEW_ES_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'new-es')
    NEW_ES_ASSERT_JS = join(NEW_ES_DIR, 'assert.js')

    print('Running new-es test:')
    files = glob(join(NEW_ES_DIR, '*.js'))
    files.remove(NEW_ES_ASSERT_JS)
    fail_total = 0
    for file in files:
        proc = Popen([engine, NEW_ES_ASSERT_JS, file], stdout=PIPE)
        out, _ = proc.communicate()

        if not proc.returncode:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))
        else:
            print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, file, COLOR_RESET))
            print(out)
            fail_total += 1

    tests_total = len(files)
    print('TOTAL: %d' % (tests_total))
    print('%sPASS : %d%s' % (COLOR_GREEN, tests_total - fail_total, COLOR_RESET))
    print('%sFAIL : %d%s' % (COLOR_RED, fail_total, COLOR_RESET))

    if fail_total > 0:
        raise Exception('new-es tests failed')

@runner('intl', default=True)
def run_intl(engine, arch):
    INTL_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'intl')
    INTL_ASSERT_JS = join(INTL_DIR, 'assert.js')

    print('Running Intl test:')
    files = glob(join(INTL_DIR, '*.js'))
    files.remove(INTL_ASSERT_JS)
    fails = 0
    for file in files:
        proc = Popen([engine, INTL_ASSERT_JS, file], stdout=PIPE)
        out, _ = proc.communicate()

        if not proc.returncode:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))
        else:
            print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, file, COLOR_RESET))
            print(out)
            fails += 1

    if fails > 0:
        raise Exception('Intl tests failed')

@runner('cctest', default=False)
def run_cctest(engine, arch):
    if engine is "escargot":
        engine = "cctest"
    proc = Popen([engine], stdout=PIPE)
    stdout, _ = proc.communicate()
    print(stdout)
    if '  PASSED  ' not in stdout:
        raise Exception('Not all tests succeeded')

@runner('debugger-server-source', default=True)
def run_escargot_debugger(engine, arch):
    ESCARGOT_DEBUGGER_TEST_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'tests')
    ESCARGOT_DEBUGGER_CLIENT = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger.py')
    ESCARGOT_DEBUGGER_TESTER = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger_tester.sh')
    print('Running Escargot-Debugger-Server-Source test:')
    fails = 0
    proc = Popen(['chmod', '+x', ESCARGOT_DEBUGGER_TESTER],stdout=PIPE)
    for files in os.listdir(ESCARGOT_DEBUGGER_TEST_DIR):
        if files.endswith(".cmd"):
            test_case, _ = os.path.splitext(files)
            test_case_path = os.path.join(ESCARGOT_DEBUGGER_TEST_DIR, test_case)
            proc = Popen([ESCARGOT_DEBUGGER_TESTER, engine, os.path.relpath(test_case_path, PROJECT_SOURCE_DIR), ESCARGOT_DEBUGGER_CLIENT, "0"])
            proc.communicate()
            if not proc.returncode:
                 print('%sOK: %s%s' % (COLOR_GREEN, test_case_path, COLOR_RESET))
            else:
                 print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, test_case_path, COLOR_RESET))
                 fails += 1
    if fails > 0:
        raise Exception('Escargot-Debugger-Server-Source tests failed')

@runner('debugger-client-source', default=True)
def run_escargot_debugger2(engine, arch):
    ESCARGOT_DEBUGGER_TEST_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'tests')
    ESCARGOT_DEBUGGER_CLIENT = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger.py')
    ESCARGOT_DEBUGGER_TESTER = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger_tester.sh')
    print('Running Escargot-Debugger-Client-Source test:')
    fails = 0
    proc = Popen(['chmod', '+x', ESCARGOT_DEBUGGER_TESTER],stdout=PIPE)
    for files in os.listdir(ESCARGOT_DEBUGGER_TEST_DIR):
        if files.endswith(".cmd"):
            test_case, _ = os.path.splitext(files)
            test_case_path = os.path.join(ESCARGOT_DEBUGGER_TEST_DIR, test_case)
            proc = Popen([ESCARGOT_DEBUGGER_TESTER, engine, os.path.relpath(test_case_path, PROJECT_SOURCE_DIR), ESCARGOT_DEBUGGER_CLIENT, "1"])
            proc.communicate()
            if not proc.returncode:
                 print('%sOK: %s%s' % (COLOR_GREEN, test_case_path, COLOR_RESET))
            else:
                 print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, test_case_path, COLOR_RESET))
                 fails += 1
    if fails > 0:
        raise Exception('Escargot-Debugger-Client-Source tests failed')

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
