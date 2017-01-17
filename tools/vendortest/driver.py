#!/usr/bin/env python3

import os
import optparse
import sys
import platform
import itertools
import subprocess
import importlib
import json

sys.path.insert(0, './test/vendortest/SpiderMonkey/lib')
from tests import RefTestCase, get_jitflags, get_cpu_count, get_environment_overlay, change_env
from results import ResultsSink
from progressbar import ProgressBar

sys.path.insert(0, './test/vendortest/SpiderMonkey/lib')
if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
    from tasks_unix import run_all_tests
else:
    from tasks_win import run_all_tests

BASE_DIR = os.path.join('tools', 'vendortest')

TIMEOUT_DEFAULT = 60

TEST_SUITES = ["stress", "mozilla"]
SUPPORTED_ARCHS = ["x64", "x86", "arm64", "arm"]
VARIANTS = ["interpreter", "jit"]
MODES = ["debug", "release"]
ENGINES = ["escargot", "jsc", "v8"]

class CommandOptionValues(object):
    def __init__(self, arch, variants, mode, engine, timeout, suite, subpath):
        arch = arch.split(",")
        variants = variants.split(",")
        mode = mode.split(",")
        engine = engine.split(",")
        suite = suite.split(",")
        for m in mode:
            if not (m in MODES):
                raise ValueError("Mode should be between [" +', '.join(MODES) +"] not " + m)
        for v in variants:
            if not (v in VARIANTS):
                raise ValueError("Variants should be between [" +', '.join(VARIANTS) + "] not " + v )
        for a in arch:
            if not (a in SUPPORTED_ARCHS):
                raise ValueError("Architecture should be between [" + ', '.join(SUPPORTED_ARCHS) +"] not " + a)
        for e in engine:
            if not (e in ENGINES):
                raise ValueError("Engine should be between [" + ', '.join(ENGINES) +"] not " + e)
        for s in suite:
            if not (s in TEST_SUITES):
                raise ValueError("TestSuite should be between [" + ', '.join(TEST_SUITES) +"] not " + s)
        if (timeout > 300):
            raise ValueError("Maximum wait time should be less than 300")

        self.suite = suite
        self.timeout = timeout
        self.arch_and_variants_and_mode_and_engine = itertools.product(arch, variants, mode, engine)
        self.subpath = subpath
        self.js_shell = os.path.join("out", "x64", "interpreter", "release", "escargot")
        self.xul_info_src = ':'.join([str(arch), 'Linux', 'false'])

class ArgumentParser(object):
    def __init__(self):
        self._parser = self._create_option_parser()

    def _create_option_parser(self):
        parser = optparse.OptionParser()
        parser.add_option("-a", "--arch",
                help=("The architecture to run tests for %s" % SUPPORTED_ARCHS),
                default="x64")
        parser.add_option("--variants",
                help=("The variants to run tests for %s" % VARIANTS),
                default="interpreter")
        parser.add_option("-m", "--mode",
                help=("The modes to run tests for %s" % MODES),
                default="release")
        parser.add_option("-e", "--engine",
                help=("The engine to run test with %s" % ENGINES),
                default="escargot")
        parser.add_option("-t", "--timeout",
                help=("The time out value for each test case, which should be between" % MODES),
                type=int,
                default=TIMEOUT_DEFAULT)
        parser.add_option("-s", "--suite",
                help=("The test suite to run test for %s" % TEST_SUITES),
                default="stress")
        parser.add_option("-p", "--subpath",
                help=("Run only the tests of which path includes SUBPATH"),
                default="")
        return parser

    def parse(self, args):
        (options, paths) = self._parser.parse_args(args=args)
        suite = options.suite
        timeout = options.timeout
        mode = options.mode
        engine = options.engine
        variants = options.variants
        arch = options.arch
        subpath = options.subpath
        options = CommandOptionValues(arch, variants, mode, engine, timeout, suite, subpath)
        return (paths, options)

class Test(object):
    def __init__(self, path, ignore, ignore_reason, timeout, env=None, flags=None):
        self.path = path
        self.ignore = ignore
        self.ignore_reason = ignore_reason
        self.timeout = timeout
        self.env = env
        self.flags = flags

class TestReader(object):
    def __init__(self, name, base_dir):
        self.name = name
        self.base_dir = base_dir
        self.mandatory_files = {}

    def list_tests(self, options):
        raise NotImplementedError

    def mandatory_file(self, test):
        raise NotImplementedError

    def output_file(self, a_v_m_e):
        return os.path.join(BASE_DIR, ".".join([self.name, self._gen_output_suffix()]))
        #return os.path.join(BASE_DIR, ".".join([self.name, a_v_m_e[0], a_v_m_e[1], a_v_m_e[2], a_v_m_e[3], self._gen_output_suffix()]))

    def origin_file(self, a_v_m_e):
        return os.path.join(BASE_DIR, ".".join([self.name, self._orig_output_suffix()]))
        #return os.path.join(BASE_DIR, ".".join([self.name, a_v_m_e[0], a_v_m_e[1], a_v_m_e[2], a_v_m_e[3], self._orig_output_suffix()]))

    def _suffix(self):
        return ".js"

    def _gen_output_suffix(self):
        return "gen.txt"

    def _orig_output_suffix(self):
        return "orig.txt"

class StressReader(TestReader):
    def __init__(self, name, base_dir):
        TestReader.__init__(self, name, base_dir)

    def list_tests(self, options):
        tc_stress = os.path.join(BASE_DIR, "jsc.stress.TC")
        tc_list = []
        with open(tc_stress, "r") as f:
            for line in f:
                ignore = line.startswith("//")
                if ignore:
                    idx = line.rfind("//")
                    ignore_reason = line[idx + 2:].strip()
                    tc_list.append(Test(os.path.join(self.base_dir, line[3:idx]), ignore, ignore_reason, options.timeout))
                else:
                    tc_list.append(Test(os.path.join(self.base_dir, line[:-1]), ignore, None, options.timeout))
        return tc_list

    def mandatory_file(self, test):
        return [os.path.join(BASE_DIR, "jsc.stress.base.js")]

class MozillaReader(TestReader):
    def __init__(self, name, base_dir):
        TestReader.__init__(self, name, base_dir)

    def list_tests(self, options):
        import manifest

        if options.js_shell is None:
            xul_tester = manifest.NullXULInfoTester()
        else:
            if options.xul_info_src is None:
                xul_info = manifest.XULInfo.create(options.js_shell)
            else:
                xul_abi, xul_os, xul_debug = options.xul_info_src.split(r':')
                xul_debug = xul_debug.lower() is 'true'
                xul_info = manifest.XULInfo(xul_abi, xul_os, xul_debug)
            xul_tester = manifest.XULInfoTester(xul_info, options.js_shell)

        test_dirs = ["ecma_5", "js1_1", "js1_2", "js1_3", "js1_4", "js1_5",
                    "js1_6", "js1_7","js1_8", "js1_8_1", "js1_8_5"]
        tc_list = []
        ESCARGOT_SKIP = "// escargot-skip:"
        ESCARGOT_TIMEOUT = "// escargot-timeout:"
        ESCARGOT_ENV = "// escargot-env:"
        for test_dir in test_dirs:
            for (path, dir, files) in os.walk(os.path.join(self.base_dir, test_dir)):
                dir.sort()
                files.sort()
                for filename in files:
                    ext = os.path.splitext(filename)[-1]
                    if ext == self._suffix():
                        if filename.find("shell") == -1:
                            filepath = os.path.join(path, filename)
                            with open(filepath, "r") as f:
                                first_line = f.readline()
                                ignore = first_line.startswith(ESCARGOT_SKIP)
                                ignore_reason = None
                                if ignore:
                                    ignore_reason = first_line[len(ESCARGOT_SKIP):].strip()
                                filewise_timeout = first_line.startswith(ESCARGOT_TIMEOUT)
                                timeout = options.timeout
                                if filewise_timeout:
                                    timeout = int(first_line[len(ESCARGOT_TIMEOUT):].strip())
                                filewise_env = first_line.startswith(ESCARGOT_ENV)
                                env = None
                                if filewise_env:
                                    env = json.loads(first_line[len(ESCARGOT_ENV):].strip())
                            test = Test(filepath, ignore, ignore_reason, timeout, env)
                            manifest._parse_test_header(filepath, test, xul_tester)
                            tc_list.append(test)
        return tc_list

    def mandatory_file(self, test):
        base_dir = os.path.dirname(test.path)
        if base_dir in self.mandatory_files:
            return self.mandatory_files[base_dir]
        else:
            if os.path.join(self.base_dir, "ecma_5", "JSON") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "ecma_5/shell.js", "ecma_5/JSON/shell.js"]]
            elif os.path.join(self.base_dir, "ecma_5", "RegExp") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "ecma_5/shell.js", "ecma_5/RegExp/shell.js"]]
            elif os.path.join(self.base_dir, "ecma_5") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "ecma_5/shell.js"]]
            elif os.path.join(self.base_dir, "js1_2", "version120") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_2/version120/shell.js"]]
            elif os.path.join(self.base_dir, "js1_5", "Expressions") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_5/Expressions/shell.js"]]
            elif os.path.join(self.base_dir, "js1_6") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_6/shell.js"]]
            elif os.path.join(self.base_dir, "js1_7") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_7/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_1", "jit") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_1/shell.js", "js1_8_1/jit/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_1", "strict") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_1/shell.js", "js1_8_1/strict/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_1") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_1/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_5", "extensions") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_5/shell.js", "js1_8_5/extensions/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_5", "reflect-parse") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_5/shell.js", "js1_8_5/reflect-parse/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8_5") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8_5/shell.js"]]
            elif os.path.join(self.base_dir, "js1_8") in base_dir:
                self.mandatory_files[base_dir] = [os.path.join(self.base_dir, x) for x in ["shell.js", "js1_8/shell.js"]]
            else:
                return ["test/vendortest/SpiderMonkey/shell.js"]
        return self.mandatory_files[base_dir]

class Driver(object):
    def main(self):
        args = sys.argv[1:]

        parser = ArgumentParser()
        (path, options) = parser.parse(args)

        a_v_m_es = []
        for a_v_m_e in options.arch_and_variants_and_mode_and_engine:
            a_v_m_es.append(a_v_m_e)

        module = importlib.import_module("driver")
        instances = []
        for suite in options.suite:
            if suite == "stress":
                class_ = getattr(module, "StressReader")
                instances.append(class_("jsc.stress", os.path.join("test", "vendortest", "JavaScriptCore", "stress")))
            elif suite == "mozilla":
                class_ = getattr(module, "MozillaReader")
                instances.append(class_("spidermonkey", os.path.join("test", "vendortest", "SpiderMonkey")))

        def log(f, str):
            print(str)
            f.write(str + '\n')

        for a_v_m_e in a_v_m_es:
            if "escargot" in a_v_m_e[3]:
                shell = os.path.join(".", "escargot")
                #shell = os.path.join("out", "linux", a_v_m_e[0], a_v_m_e[1], a_v_m_e[2], "escargot")
                #build_cmd = ['make', '.'.join([a_v_m_e[0], a_v_m_e[1], a_v_m_e[2]]), '-j']
                #subprocess.check_call(build_cmd)
            elif "jsc" in a_v_m_e[3]:
                shell = os.path.join("test", "bin", a_v_m_e[0], "jsc", "baseline", "jsc")
            elif "v8" in a_v_m_e[3]:
                shell = os.path.join("test", "bin", a_v_m_e[0], "v8", "d8")
            index = 0
            total = 0
            succ = 0
            fail = 0
            ignore = 0
            timeout = 0
            for instance in instances:
                with open(instance.output_file(a_v_m_e), 'w') as f:
                    for tc in instance.list_tests(options):
                        index += 1
                        try:
                            if options.subpath not in tc.path:
                                continue
                            total += 1

                            command = [shell]
                            command = command + instance.mandatory_file(tc)
                            command.append(tc.path)

                            command_print = []
                            command_print = command_print + instance.mandatory_file(tc)
                            command_print.append(tc.path)

                            if tc.ignore:
                                ignore += 1
                                log(f, '[' + str(index) + '] ' + ' '.join(command_print) + " .... Excluded (" + tc.ignore_reason + ")")
                                continue
                            output = subprocess.check_output(command, timeout=tc.timeout, env=tc.env)
                            succ += 1
                            log(f, '[' + str(index) + '] ' + ' '.join(command_print) + " .... Success")
                        except subprocess.TimeoutExpired as e:
                            timeout += 1
                            log(f, '[' + str(index) + '] ' + ' '.join(command_print) + " .... Timeout")
                        except subprocess.CalledProcessError as e:
                            fail += 1
                            log(f, '[' + str(index) + '] ' + ' '.join(command_print) + " .... Fail (" + e.output.decode('cp1252')[:-1] + ")")
                    log(f, 'total : ' + str(total))
                    log(f, 'succ : ' + str(succ))
                    log(f, 'fail : ' + str(fail))
                    log(f, 'ignore : ' + str(ignore))
                    log(f, 'timeout : ' + str(timeout))
                if len(options.subpath) == 0:
                    try:
                        subprocess.check_output(["diff", instance.origin_file(a_v_m_e), instance.output_file(a_v_m_e)])
                    except subprocess.CalledProcessError as e:
                        print(e.output.decode('cp1252')[:-1])
                        return 1
        return 0

if __name__ == "__main__":
    sys.exit(Driver().main())
