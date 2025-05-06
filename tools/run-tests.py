#!/usr/bin/env python3

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
import re, fnmatch

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
        print(out.decode("utf-8"))

    if checkresult and proc.returncode:
        raise Exception('command `%s` exited with %s' % (' '.join(args), proc.returncode))
    return out


def readfile(filename):
    with open(filename, 'r') as f:
        return f.readlines()


@runner('sunspider')
def run_sunspider(engine, arch, extra_arg):
    run([join('.', 'sunspider'),
        '--shell', engine,
        '--suite', 'sunspider-1.0.2'],
        cwd=join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SunSpider'))


@runner('sunspider-js', default=True)
def run_sunspider_js(engine, arch, extra_arg):
    run([engine] + sorted(glob(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SunSpider', 'tests', 'sunspider-1.0.2', '*.js'))))


@runner('octane', default=True)
def run_octane(engine, arch, extra_arg):
    max_retry_count = 5
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

            if 'Score' not in stdout.decode("utf-8"):
                raise Exception('no "Score" in stdout')
            return
        except Exception as e:
            last_error = e
            try_count = try_count + 1


    raise last_error


@runner('octane-loading', default=True)
def run_octane_loading(engine, arch, extra_arg):
    OCTANE_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'octane')
    OCTANE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'octane')
    copy(join(OCTANE_OVERRIDE_DIR, 'runLoading.js'), join(OCTANE_DIR, 'runLoading.js'))

    run([engine, 'runLoading.js'],
         cwd=OCTANE_DIR)


@runner('modifiedVendorTest', default=True)
def run_internal_test(engine, arch, extra_arg):
    INTERNAL_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'ModifiedVendorTest')
    INTERNAL_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'ModifiedVendorTest')

    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-cases.txt'), join(INTERNAL_DIR, 'internal-test-cases.txt'))
    copy(join(INTERNAL_OVERRIDE_DIR, 'internal-test-driver.py'), join(INTERNAL_DIR, 'driver.py'))

    run(['python', 'driver.py', engine, 'internal-test-cases.txt'],
        cwd=INTERNAL_DIR)

def copy_test262_files():
    TEST262_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'test262')
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    copy(join(TEST262_OVERRIDE_DIR, 'excludelist.orig.xml'), join(TEST262_DIR, 'excludelist.xml'))
    copy(join(TEST262_OVERRIDE_DIR, 'cth.js'), join(TEST262_DIR, 'harness', 'cth.js'))
    copy(join(TEST262_OVERRIDE_DIR, 'testIntl.js'), join(TEST262_DIR, 'harness', 'testIntl.js'))
    copy(join(TEST262_OVERRIDE_DIR, 'asyncHelpers.js'), join(TEST262_DIR, 'harness', 'asyncHelpers.js'))

    copy(join(TEST262_OVERRIDE_DIR, 'parseTestRecord.py'), join(TEST262_DIR, 'tools', 'packaging', 'parseTestRecord.py'))
    copy(join(TEST262_OVERRIDE_DIR, 'test262.py'), join(TEST262_DIR, 'tools', 'packaging', 'test262.py')) # for parallel running (we should re-implement this for es6 suite)

@runner('test262', default=True)
def run_test262(engine, arch, extra_arg):
    copy_test262_files()
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')
    args = ['python3', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--summary']
    if len(extra_arg["test262_extra_arg"]):
        args.extend(extra_arg["test262_extra_arg"].split(" "))
    stdout = run(args,
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific'},
        stdout=PIPE)

    summary = stdout.decode("utf-8").split('=== Test262 Summary ===')[1]
    if summary.find('- All tests succeeded') < 0:
        raise Exception('test262 failed')
    print('test262: All tests passed')

@runner('test262-strict', default=False)
def run_test262_strict(engine, arch, extra_arg):
    copy_test262_files()
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')
    out = open('test262-strict_out', 'w')

    args = ['python3', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--full-summary',
         '--strict_only'],
    if len(extra_arg["test262_extra_arg"]):
        args.extend(extra_arg["test262_extra_arg"].split(" "))
    run(args,
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


@runner('test262-nonstrict', default=False)
def run_test262_nonstrict(engine, arch, extra_arg):
    copy_test262_files()
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')
    out = open('test262-nonstrict_out', 'w')

    args = ['python3', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
        '--full-summary',
         '--non_strict_only'],
    if len(extra_arg["test262_extra_arg"]):
        args.extend(extra_arg["test262_extra_arg"].split(" "))
    run(args,
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

def compile_test_data_runner(extra_arg):
    if extra_arg["skip_build_test_data_runner"]:
        return
    run(['g++', "./tools/test/test-data-runner/test-data-runner.cpp", "-o", "./tools/test/test-data-runner/test-data-runner", "-g3", "-std=c++11", "-lpthread"],
    cwd=PROJECT_SOURCE_DIR,
    stdout=PIPE)

@runner('test262-dump', default=False)
def run_test262_dump(engine, arch, extra_arg):
    copy_test262_files()
    TEST262_DIR = join(PROJECT_SOURCE_DIR, 'test', 'test262')

    args = ['python3', join('tools', 'packaging', 'test262.py'),
         '--command', engine,
         '--summary']
    if len(extra_arg["test262_extra_arg"]):
        args.extend(extra_arg["test262_extra_arg"].split(" "))
    stdout = run(args,
        cwd=TEST262_DIR,
        env={'TZ': 'US/Pacific', 'ESCARGOT_DUMP262DATA': '1'},
        stdout=PIPE)

    summary = stdout.decode("utf-8").split('=== Test262 Summary ===')[1]
    if summary.find('- All tests succeeded') < 0:
        raise Exception('test262 failed')
    print('test262: All tests passed and dumped')

@runner('test262-dump-run', default=False)
def run_test262_dump_run(engine, arch, extra_arg):
    compile_test_data_runner(extra_arg)

    stdout = run([extra_arg["test_data_runner_path"], "--shell", engine,
                  "--test", "test262", "--test-data", join(PROJECT_SOURCE_DIR, 'test', 'test262', 'test262_data')],
        cwd=join(PROJECT_SOURCE_DIR, 'test', 'test262'),
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')
    print(stdout)
    print('test262-dump-passed')

@runner('spidermonkey', default=True)
def run_spidermonkey(engine, arch, extra_arg):
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

@runner('spidermonkey-dump', default=False)
def run_spidermonkey_dump(engine, arch, extra_arg):
    SPIDERMONKEY_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'spidermonkey')
    SPIDERMONKEY_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'SpiderMonkey')

    log_path = join(SPIDERMONKEY_OVERRIDE_DIR, '%s.log.txt' % arch)
    if not os.path.exists(log_path):
        run_spidermonkey(engine, arch, extra_arg)

    log = sorted(readfile(log_path))
    for idx, x in enumerate(log):
        text = log[idx]
        text = text.split(" ")
        text.pop(0)
        text = " ".join(text)
        text = text.replace(PROJECT_SOURCE_DIR + "/", "")
        text = text.replace("-f ", "")
        text = text.replace("\n", "")
        log[idx] = text

    with open(join(SPIDERMONKEY_OVERRIDE_DIR, '%s.data.txt' % arch), "w") as output:
        for idx, x in enumerate(log):
            output.write(log[idx] + "\n")
            if log[idx].endswith("-n.js"):
                output.write("3\n")
            else:
                output.write("0\n")
            output.write("\n")

@runner('spidermonkey-dump-run', default=False)
def run_spidermonkey_dump_run(engine, arch, extra_arg):
    compile_test_data_runner(extra_arg)

    SPIDERMONKEY_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'spidermonkey')
    stdout = run([extra_arg["test_data_runner_path"], '--test-data', join(SPIDERMONKEY_OVERRIDE_DIR, '%s.data.txt' % arch),
                  "--shell", engine, "--env", "LOCALE=en_US"],
        cwd=PROJECT_SOURCE_DIR,
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')
    print(stdout)
    print('spidermonkey-dump-passed')


@runner('jsc-stress', default=True)
def run_jsc_stress(engine, arch, extra_arg):
    JSC_DIR = join('test', 'vendortest', 'driver')

    run([join(JSC_DIR, 'driver.py'),
         '-s', 'stress',
         '-e', engine,
         '-a', arch],
        cwd=PROJECT_SOURCE_DIR,
        env={'PYTHONPATH': '.'})

@runner('jsc-stress-dump', default=False)
def run_jsc_stress_dump(engine, arch, extra_arg):
    JSC_DIR = join('test', 'vendortest', 'driver')

    log_path = join(JSC_DIR, 'jsc.stress.%s.gen.txt' % arch)
    if not os.path.exists(log_path):
        run_jsc_stress(engine, arch, extra_arg)

    log = readfile(log_path)
    with open(join(JSC_DIR, 'jsc.stress.%s.data.txt' % arch), "w") as output:
        for idx, x in enumerate(log):
            text = log[idx]
            if ".... Success\n" in text:
                stext = text.split(" ")
                output.write(stext[1] + " " + stext[2] + "\n")
                output.write("0\n")
                output.write("\n")

@runner('jsc-stress-dump-run', default=False)
def run_jsc_stress_dump_run(engine, arch, extra_arg):
    JSC_DIR = join('test', 'vendortest', 'driver')
    data_path = join(JSC_DIR, 'jsc.stress.%s.data.txt' % arch)

    compile_test_data_runner(extra_arg)

    stdout = run([extra_arg["test_data_runner_path"], "--shell", engine,
                  "--test-data", data_path, "--env", "GC_FREE_SPACE_DIVISOR=1"],
        cwd=PROJECT_SOURCE_DIR,
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')
    print(stdout)
    print('jsc-stress-dump-passed')

def _run_regression_tests(engine, assert_js, files, is_fail):
    fails = 0
    for file in files:
        proc = Popen([engine, assert_js, file], stdout=PIPE)
        out, _ = proc.communicate()

        if is_fail and proc.returncode or not is_fail and not proc.returncode:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))
        else:
            print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, file, COLOR_RESET))
            print(out.decode("utf-8"))

            fails += 1

    return fails


@runner('regression-tests', default=True)
def run_regression_tests(engine, arch, extra_arg):
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
def run_jetstream_only_simple_parallel_1(engine, arch, extra_arg):
    _run_jetstream(engine, 'simple-1')


@runner('jetstream-only-simple-parallel-2')
def run_jetstream_only_simple_parallel_2(engine, arch, extra_arg):
    _run_jetstream(engine, 'simple-2')


@runner('jetstream-only-simple-parallel-3')
def run_jetstream_only_simple_parallel_3(engine, arch, extra_arg):
    _run_jetstream(engine, 'simple-3')


@runner('jetstream-only-simple', default=True)
def run_jetstream_only_simple(engine, arch, extra_arg):
    _run_jetstream(engine, 'simple')


@runner('jetstream-only-cdjs', default=True)
def run_jetstream_only_cdjs(engine, arch, extra_arg):
    _run_jetstream(engine, 'cdjs')


@runner('jetstream-only-sunspider')
def run_jetstream_only_sunspider(engine, arch, extra_arg):
    _run_jetstream(engine, 'sunspider')


@runner('jetstream-only-octane')
def run_jetstream_only_octane(engine, arch, extra_arg):
    _run_jetstream(engine, 'octane')


@runner('chakracore', default=True)
def run_chakracore(engine, arch, extra_arg):
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
        result = stdout.decode("utf-8")
        gen_txt.write(result)
    run(['diff',
         join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.%s.orig.txt' % arch),
         join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'driver', 'chakracore.%s.gen.txt' % arch)])

@runner('chakracore-dump', default=False)
def run_chakracore_dump(engine, arch, extra_arg):
    CHAKRACORE_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'chakracore')
    CHAKRACORE_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'ChakraCore')

    log_path = join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.%s.orig.txt' % arch)

    driver_file = "tools/test/chakracore/chakracore.include.js"
    log = readfile(log_path)
    with open(join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.%s.data.txt' % arch), "w") as output:
        for idx, x in enumerate(log):
            if ".......... Pass\n" in x:
                segs = x.split(" ")
                kind = segs[0]
                kind = kind[1:]
                kind = kind[:-1]
                tc = join('test', 'vendortest', 'ChakraCore', kind, segs[1])
                output.write(driver_file + " " + tc + "\n")
                output.write("0\n")

                baseline = tc[:-2] + "baseline"
                try:
                    # some test has invalid unicode
                    tc_contents = "".join(readfile(tc))
                except:
                    tc_contents = ""
                if os.path.exists(baseline) and "testRunner" not in tc_contents:
                    baseline = baseline + "\n"
                else:
                    baseline = "\n"

                output.write(baseline)

@runner('chakracore-dump-run', default=False)
def run_chakracore_dump_run(engine, arch, extra_arg):
    compile_test_data_runner(extra_arg)

    CHAKRACORE_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'chakracore')
    stdout = run([extra_arg["test_data_runner_path"], "--shell", engine,
                "--test-data", join(CHAKRACORE_OVERRIDE_DIR, 'chakracore.%s.data.txt' % arch),
                "--env", "GC_FREE_SPACE_DIVISOR=1 TZ=US/Pacific",
                "--treat-global-tostring-as-object"],
        cwd=join(PROJECT_SOURCE_DIR),
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')

    print('chakracore-dump-passed')

@runner('v8', default=True)
def run_v8(engine, arch, extra_arg):
    V8_OVERRIDE_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'v8')
    V8_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'v8')

    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.status'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.status'))
    copy(join(V8_OVERRIDE_DIR, 'v8.mjsunit.js'), join(V8_DIR, 'test', 'mjsunit', 'mjsunit.js'))
    copy(join(V8_OVERRIDE_DIR, 'v8.run-tests.py'), join(V8_DIR, 'tools', 'run-tests.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.testsuite.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'testsuite.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.execution.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'execution.py'))
    copy(join(V8_OVERRIDE_DIR, 'v8.progress.py'), join(V8_DIR, 'tools', 'testrunner', 'local', 'progress.py'))

    arch = {'x86': 'x32', 'x86_64': 'x64'}[arch]

    if (engine == "escargot"):
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
                 env={'GC_FREE_SPACE_DIVISOR': '1'},
                 checkresult=False)
    # FIXME: V8 test runner tends to exit with 2 (most probably the result of
    # a `self.remaining != 0` in `testrunner.local.execution.Runner.Run()`.
    # Couldn't find out yet why that happens.
    # NOTE: This has also been suppressed in cmake/make-based executors by
    # piping the output of run-test.py into a tee. In case of pipes, the
    # observed exit code is that of the last element of the pipe, which is
    # tee in that case (which is always 0).

    if '=== All tests succeeded' not in stdout.decode("utf-8"):
        raise Exception('Not all tests succeeded')

    return stdout


@runner('v8-dump', default=False)
def run_v8_dump(engine, arch, extra_arg):
    stdout = run_v8(engine, arch, extra_arg)
    stdout = stdout.decode("utf-8").split("\n")

    TOOL_V8_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'v8')
    STATUS_FILE_CONTENTS = readfile(join(TOOL_V8_DIR, 'v8.mjsunit.status'))

    driver_path = "tools/test/v8/v8.mjsunit.js";
    with open(join(TOOL_V8_DIR, '%s.data.txt' % arch), "w") as output:
        for line in stdout:
            if "Done running " in line:
                casename = line.split(" ")[2].split(":")[0]
                skipped = False
                for status_file_content in STATUS_FILE_CONTENTS:
                    if casename[8:] in status_file_content:
                        if "PASS" not in status_file_content or "FAIL" in status_file_content:
                            skipped = True
                if not skipped and "bugs/" not in casename:
                    print("TEST " + casename)
                    filename = "test/vendortest/v8/test/" + casename + ".js"
                    output.write(driver_path)
                    output.write(" ")
                    output.write(filename)
                    output.write("\n")
                    output.write("0\n")
                    output.write("\n")
                else:
                    print("SKIP " + casename)

@runner('v8-dump-run', default=False)
def run_v8_dump_run(engine, arch, extra_arg):
    compile_test_data_runner(extra_arg)

    TOOL_V8_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'v8')
    stdout = run([extra_arg["test_data_runner_path"], "--shell", engine,
                "--test-data", join(TOOL_V8_DIR, '%s.data.txt' % arch), "--env", "GC_FREE_SPACE_DIVISOR=1"],
        cwd=join(PROJECT_SOURCE_DIR),
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')

    print('v8-dump-passed')

@runner('new-es', default=True)
def run_new_es(engine, arch, extra_arg):
    NEW_ES_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'new-es')
    NEW_ES_ASSERT_JS = join(NEW_ES_DIR, 'assert.js')

    print('Running new-es test:')
    files = glob(join(NEW_ES_DIR, '*.js'))
    files.remove(NEW_ES_ASSERT_JS)
    fail_total = 0
    fail_files = []
    for file in files:
        proc = Popen([engine, NEW_ES_ASSERT_JS, file], stdout=PIPE)
        out, _ = proc.communicate()

        if not proc.returncode:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))
        else:
            print('%sFAIL(%d): %s%s' % (COLOR_RED, proc.returncode, file, COLOR_RESET))
            print(out.decode("utf-8"))
            fail_total += 1
            fail_files.append(file)


    tests_total = len(files)
    print('TOTAL: %d' % (tests_total))
    print('%sPASS : %d%s' % (COLOR_GREEN, tests_total - fail_total, COLOR_RESET))
    print('%sFAIL : %d%s' % (COLOR_RED, fail_total, COLOR_RESET))
    for fail in fail_files:
        print('%sFAIL : %s%s' % (COLOR_RED, fail, COLOR_RESET))

    if fail_total > 0:
        raise Exception('new-es tests failed')

@runner('intl', default=True)
def run_intl(engine, arch, extra_arg):
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
            print(out.decode("utf-8"))
            fails += 1

    if fails > 0:
        raise Exception('Intl tests failed')

@runner('escargot-test-dump', default=False)
def run_escargot_test_dump(engine, arch, extra_arg):
    NEW_ES_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'new-es')
    NEW_ES_ASSERT_JS = join(NEW_ES_DIR, 'assert.js')

    newes = glob(join(NEW_ES_DIR, '*.js'))
    newes.remove(NEW_ES_ASSERT_JS)

    REGRESSION_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'regression-tests')
    REGRESSION_XFAIL_DIR = join(REGRESSION_DIR, 'xfail')
    REGRESSION_ASSERT_JS = join(REGRESSION_DIR, 'assert.js')
    xpass = glob(join(REGRESSION_DIR, 'issue-*.js'))
    xfail = glob(join(REGRESSION_XFAIL_DIR, 'issue-*.js'))

    INTL_DIR = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'intl')
    INTL_ASSERT_JS = join(INTL_DIR, 'assert.js')

    intl = glob(join(INTL_DIR, '*.js'))
    intl.remove(INTL_ASSERT_JS)

    with open(join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'data.txt'), "w") as output:
        for file in newes:
            output.write(NEW_ES_ASSERT_JS.replace(PROJECT_SOURCE_DIR + "/", "") + " "
                         + file.replace(PROJECT_SOURCE_DIR + "/", "") + " " + "\n")
            output.write("0\n")
            output.write("\n")

        for file in xpass:
            output.write(REGRESSION_ASSERT_JS.replace(PROJECT_SOURCE_DIR + "/", "") + " "
                         + file.replace(PROJECT_SOURCE_DIR + "/", "") + " " + "\n")
            output.write("0\n")
            output.write("\n")

        for file in xfail:
            output.write(INTL_ASSERT_JS.replace(PROJECT_SOURCE_DIR + "/", "") + " "
                         + file.replace(PROJECT_SOURCE_DIR + "/", "") + " " + "\n")
            output.write("3\n")
            output.write("\n")

        for file in intl:
            output.write(INTL_ASSERT_JS.replace(PROJECT_SOURCE_DIR + "/", "") + " "
                         + file.replace(PROJECT_SOURCE_DIR + "/", "") + " " + "\n")
            output.write("0\n")
            output.write("\n")

@runner('escargot-test-dump-run', default=False)
def run_escargot_test_dump_run(engine, arch, extra_arg):
    compile_test_data_runner(extra_arg)

    stdout = run([extra_arg["test_data_runner_path"], "--shell", engine,
                  "--test-data", join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'Escargot', 'data.txt')],
        cwd=join(PROJECT_SOURCE_DIR),
        stdout=PIPE)

    stdout = stdout.decode("utf-8")
    if stdout.find("Passed") < 0:
        raise Exception('failed')
    print(stdout)
    print('escargot-dump-passed')

@runner('wasm-js', default=False)
def run_wasm_js(engine, arch, extra_arg):
    WASM_TEST_ROOT = join(PROJECT_SOURCE_DIR, 'test', 'vendortest', 'wasm-js')
    WASM_TEST_DIR = join(WASM_TEST_ROOT, 'tests')
    WASM_TEST_MJS = join(WASM_TEST_ROOT, 'mjsunit.js')
    WASM_TEST_HARNESS = join(WASM_TEST_ROOT, 'testharness.js')

    EXCLUDE_LIST_FILE = join(PROJECT_SOURCE_DIR, 'tools', 'test', 'wasm-js', 'exclude_list.txt')
    exclude_list = []
    with open(EXCLUDE_LIST_FILE) as f:
        exclude_list = f.read().replace('\n', ' ').split()

    files = []
    for root, dirnames, filenames in os.walk(join(WASM_TEST_DIR)):
        if "proposals" in root:
            # skip proposals test
            continue
        for filename in fnmatch.filter(filenames, '*.any.js'):
            full_path = join(root, filename)
            rel_path = full_path[len(WASM_TEST_DIR)+1:]
            if rel_path in exclude_list:
                continue
            files.append(join(root, filename))
    files = sorted(files)

    WPT_ROOT = "/wasm/jsapi/"
    META_SCRIPT_REGEXP = re.compile(r"META:\s*script=(.*)")
    fail_total = 0
    fail_files = []
    for file in files:
        source = ""
        with open(file) as f:
            source = f.read()

        script_files = []
        for script in META_SCRIPT_REGEXP.findall(source):
            if (script.startswith(WPT_ROOT)):
                script = join(WASM_TEST_DIR, script[len(WPT_ROOT):])
            else:
                script = join(os.path.dirname(file), script)
            script_files.append(script)

        script_files.append(file)

        stdout = run([engine, WASM_TEST_MJS, WASM_TEST_HARNESS] + script_files, stdout=PIPE)
        out = stdout.decode("utf-8")

        if out.find("FAIL") >= 0:
            fail_total += 1
            fail_files.append(file)
        else:
            print('%sOK: %s%s' % (COLOR_GREEN, file, COLOR_RESET))

    tests_total = len(files)
    print('TOTAL: %d' % (tests_total))
    print('%sPASS : %d%s' % (COLOR_GREEN, tests_total - fail_total, COLOR_RESET))
    print('%sFAIL : %d%s' % (COLOR_RED, fail_total, COLOR_RESET))
    for fail in fail_files:
        print('%sFAIL : %s%s' % (COLOR_RED, fail, COLOR_RESET))

    if fail_total > 0:
        raise Exception('wasm-js tests failed')

@runner('cctest', default=False)
def run_cctest(engine, arch, extra_arg):
    proc = Popen([engine], stdout=PIPE)
    out, _ = proc.communicate()

    outStr = out.decode("utf-8")
    print(outStr)
    result = outStr.lower()
    if 'fail' in result:
        raise Exception('Not all tests succeeded')

@runner('debugger-server-source', default=True)
def run_escargot_debugger(engine, arch, extra_arg):
    ESCARGOT_DEBUGGER_TEST_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'tests')
    ESCARGOT_DEBUGGER_CLIENT = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger.py')
    ESCARGOT_DEBUGGER_TESTER = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger_tester.sh')
    print('Running Escargot-Debugger-Server-Source test:')
    fails = 0
    proc = Popen(['chmod', '+x', ESCARGOT_DEBUGGER_TESTER], stdout=PIPE)
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
def run_escargot_debugger2(engine, arch, extra_arg):
    ESCARGOT_DEBUGGER_TEST_DIR = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'tests')
    ESCARGOT_DEBUGGER_CLIENT = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger.py')
    ESCARGOT_DEBUGGER_TESTER = join(PROJECT_SOURCE_DIR, 'tools', 'debugger', 'debugger_tester.sh')
    print('Running Escargot-Debugger-Client-Source test:')
    fails = 0
    proc = Popen(['chmod', '+x', ESCARGOT_DEBUGGER_TESTER], stdout=PIPE)
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

@runner('web-tooling-benchmark', default=False)
def run_web_tooling_benchmark(engine, arch, extra_arg):
    WEB_TOOLING_DIR = join(PROJECT_SOURCE_DIR, 'test', 'web-tooling-benchmark')
    WEB_TOOLING_SRC_DIR = join(WEB_TOOLING_DIR, 'src')

    suite_file = join(WEB_TOOLING_SRC_DIR, 'suite.js')
    with open(suite_file, "r") as f:
        lines = f.readlines()
    with open(suite_file, "w") as f:
        for line in lines:
            # minimize sample count to 1
            f.write(re.sub(r'minSamples: 20', 'minSamples: 1', line))

    flag_file = join(WEB_TOOLING_SRC_DIR, 'cli-flags-helper.js')
    with open(flag_file, "r") as f:
        lines = f.readlines()
    with open(flag_file, "w") as f:
        for line in lines:
            # exclude long running tests
            if "babel" not in line and "babylon" not in line and "chai" not in line:
                f.write(line)

    proc = Popen(['npx', 'npm@6', 'install'], cwd=WEB_TOOLING_DIR, stdout=PIPE)
    proc.wait()

    run([engine, 'dist/cli.js'], cwd=WEB_TOOLING_DIR)

@runner('dump-all', default=False)
def run_dump_all(engine, arch, extra_arg):
    for test in RUNNERS:
        if test.endswith("-dump"):
            RUNNERS[test](engine, arch, extra_arg)

@runner('dump-run-all', default=False)
def run_dump_run_all(engine, arch, extra_arg):
    for test in RUNNERS:
        if test.endswith("-dump-run"):
            RUNNERS[test](engine, arch, extra_arg)

def main():
    parser = ArgumentParser(description='Escargot Test Suite Runner')
    parser.add_argument('--engine', metavar='PATH', default=DEFAULT_ESCARGOT,
                        help='path to the engine to be tested (default: %(default)s)')
    parser.add_argument('--arch', metavar='NAME', choices=['x86', 'x86_64'], default='x86_64',
                        help='architecture the engine was built for (%(choices)s; default: %(default)s)')
    parser.add_argument('--test262-extra-arg', default='',
                        help='extra argument variable to test262')
    parser.add_argument('--test-data-runner-path', metavar='PATH', default=(PROJECT_SOURCE_DIR + '/tools/test/test-data-runner/test-data-runner'),
                        help='test-data-runner executable path')
    parser.add_argument('--skip-build-test-data-runner', default=False, action="store_true",
                        help='Skip build test-data-runner executable')
    parser.add_argument('suite', metavar='SUITE', nargs='*', default=sorted(DEFAULT_RUNNERS),
                        help='test suite to run (%s; default: %s)' % (', '.join(sorted(RUNNERS.keys())), ' '.join(sorted(DEFAULT_RUNNERS))))
    args = parser.parse_args()

    for suite in args.suite:
        if suite not in RUNNERS:
            parser.error('invalid test suite: %s' % suite)

    success, fail = [], []
    extra_arg = {
        'test262_extra_arg' : args.test262_extra_arg,
        'skip_build_test_data_runner' : args.skip_build_test_data_runner,
        'test_data_runner_path' : args.test_data_runner_path
                 }
    for suite in args.suite:
        print(COLOR_PURPLE + 'running test suite: ' + suite + COLOR_RESET)
        try:
            RUNNERS[suite](args.engine, args.arch, extra_arg)
            success += [suite]
        except Exception as e:
            print('\n'.join(COLOR_YELLOW + line + COLOR_RESET for line in traceback.format_exc().splitlines()))
            fail += [suite]

    if success:
        print(COLOR_GREEN + sys.argv[0] + ': success: ' + ', '.join(success) + COLOR_RESET)
    sys.exit(COLOR_RED + sys.argv[0] + ': fail: ' + ', '.join(fail) + COLOR_RESET if fail else None)


if __name__ == '__main__':
    main()
