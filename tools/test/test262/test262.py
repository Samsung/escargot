#!/usr/bin/env python
# Copyright 2009 the Sputnik authors.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# This is derived from sputnik.py, the Sputnik console test runner,
# with elements from packager.py, which is separately
# copyrighted. TODO: Refactor so there is less duplication between
# test262.py and packager.py.


import logging
import optparse
import os
from os import path
import platform
import re
import subprocess
import sys
import tempfile
import time
import xml.dom.minidom
import datetime
import shutil
import json
import stat
import signal
import xml.etree.ElementTree as xmlj
import unicodedata
from collections import Counter


from parseTestRecord import parseTestRecord, stripHeader

from packagerConfig import *

class Alarm(Exception):
    pass

def alarm_handler(signum, frame):
    raise Alarm

signal.signal(signal.SIGALRM, alarm_handler)

class Test262Error(Exception):
  def __init__(self, message):
    self.message = message

def ReportError(s):
  raise Test262Error(s)



if not os.path.exists(EXCLUDED_FILENAME):
    print "Cannot generate (JSON) test262 tests without a file," + \
        " %s, showing which tests have been disabled!" % EXCLUDED_FILENAME
    sys.exit(1)
EXCLUDE_LIST = xml.dom.minidom.parse(EXCLUDED_FILENAME)
EXCLUDE_REASON = EXCLUDE_LIST.getElementsByTagName("reason")
EXCLUDE_LIST = EXCLUDE_LIST.getElementsByTagName("test")
EXCLUDE_LIST = [x.getAttribute("id") for x in EXCLUDE_LIST]
ESCARGOT_LD_PRELOAD = os.environ.get('ESCARGOT_LD_PRELOAD')
if ESCARGOT_LD_PRELOAD is None:
    ESCARGOT_LD_PRELOAD = ""

def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("--command", default=None, help="The command-line to run")
  result.add_option("--tests", default=path.abspath('.'),
                    help="Path to the tests")
  result.add_option("--cat", default=False, action="store_true",
                    help="Print packaged test code that would be run")
  result.add_option("--summary", default=False, action="store_true",
                    help="Print summary after running tests")
  result.add_option("--full-summary", default=False, action="store_true",
                    help="Print summary and test output after running tests")
  result.add_option("--strict_only", default=False, action="store_true",
                    help="Test only strict mode")
  result.add_option("--non_strict_only", default=False, action="store_true",
                    help="Test only non-strict mode")
  result.add_option("--unmarked_default", default="both",
                    help="default mode for tests of unspecified strictness")
  result.add_option("--logname", help="Filename to save stdout to")
  result.add_option("--junitname", help="Filename to save test results in JUnit XML format")
  result.add_option("--loglevel", default="warning",
                    help="sets log level to debug, info, warning, error, or critical")
  result.add_option("--print-handle", default="print", help="Command to print from console")
  result.add_option("--list-includes", default=False, action="store_true",
                    help="List includes required by tests")
  return result


def ValidateOptions(options):
  if not options.command:
    ReportError("A --command must be specified.")
  if not path.exists(options.tests):
    ReportError("Couldn't find test path '%s'" % options.tests)


placeHolderPattern = re.compile(r"\{\{(\w+)\}\}")


def IsWindows():
  p = platform.system()
  return (p == 'Windows') or (p == 'Microsoft')


class TempFile(object):

  def __init__(self, suffix="", prefix="tmp", text=False):
    self.suffix = suffix
    self.prefix = prefix
    self.text = text
    self.fd = None
    self.name = None
    self.is_closed = False
    self.Open()

  def Open(self):
    (self.fd, self.name) = tempfile.mkstemp(
        suffix = self.suffix,
        prefix = self.prefix,
        text = self.text)

  def Write(self, str):
    os.write(self.fd, str)

  def Read(self):
    f = file(self.name)
    result = f.read()
    f.close()
    return result

  def Close(self):
    if not self.is_closed:
      self.is_closed = True
      os.close(self.fd)

  def Dispose(self):
    try:
      self.Close()
      os.unlink(self.name)
    except OSError, e:
      logging.error("Error disposing temp file: %s", str(e))


class TestResult(object):

  def __init__(self, exit_code, stdout, stderr, case):
    self.exit_code = exit_code
    self.stdout = stdout
    self.stderr = stderr
    self.case = case

  def ReportOutcome(self, long_format):
    name = self.case.GetName()
    mode = self.case.GetMode()
    if self.HasUnexpectedOutcome():
      if self.case.IsNegative():
        print "=== %s was expected to fail in %s, but didn't ===" % (name, mode)
        print "--- expected error: %s ---\n" % self.case.GetNegative()
      else:
        if long_format:
          print "=== %s failed in %s ===" % (name, mode)
        else:
          print "%s in %s: " % (name, mode)
      self.WriteOutput(sys.stdout)
      if long_format:
        print "==="
    elif self.case.IsNegative():
      print "%s failed in %s as expected" % (name, mode)
    else:
      print "%s passed in %s" % (name, mode)

  def WriteOutput(self, target):
    out = self.stdout.strip()
    if len(out) > 0:
       target.write("--- output --- \n %s" % out)
    err = self.stderr.strip()
    if len(err) > 0:
       target.write("--- errors ---  \n %s" % err)

  # This is a way to make the output from the "whitespace" tests into valid XML
  def SafeFormat(self, msg):
    try:
      msg = msg.encode(encoding='ascii', errors='strict')
      msg = msg.replace('\u000Bx', '?')
      msg = msg.replace('\u000Cx', '?')
    except:
      return 'Output contained invalid characters'

  def XmlAssemble(self, result):
    test_name = self.case.GetName()
    test_mode = self.case.GetMode()
    testCaseElement = xmlj.Element("testcase")
    testpath = self.TestPathManipulation(test_name)
    testCaseElement.attrib["classname"] = "%s.%s" % (testpath[0] , testpath[1])
    testCaseElement.attrib["name"] = "%s %s" % (testpath[2].replace('.','_') , test_mode)
    if self.HasUnexpectedOutcome():
      failureElement = xmlj.Element("failure")
      out = self.stdout.strip().decode('utf-8')
      err = self.stderr.strip().decode('utf-8')
      if len(out) > 0:
        failureElement.text = self.SafeFormat(out)
      if len(err) > 0:
        failureElement.text = self.SafeFormat(err)
      testCaseElement.append(failureElement)
    return testCaseElement

  def TestPathManipulation(self, test_name):
    testdirlist = test_name.split('/')
    testcase = testdirlist.pop()
    testclass = testdirlist.pop()
    testclass = testclass.replace('.','_')
    if len(testdirlist) >= 1:
       testpackage = testdirlist.pop(0)
    else:
       testpackage = testclass
    return(testpackage,testclass,testcase)

  def HasFailed(self):
    return self.exit_code != 0

  def AsyncHasFailed(self):
    return 'Test262:AsyncTestComplete' not in self.stdout

  def HasUnexpectedOutcome(self):
    if self.case.IsNegative():
       return not (self.HasFailed() and self.case.NegativeMatch(self.GetErrorOutput()))
    elif self.case.IsAsyncTest():
       return self.AsyncHasFailed() or self.HasFailed()
    else:
       return self.HasFailed()

  def GetErrorOutput(self):
    if len(self.stderr) != 0:
      return self.stderr
    return self.stdout


class TestCase(object):

  def __init__(self, suite, name, full_path, strict_mode):
    self.suite = suite
    self.name = name
    self.full_path = full_path
    self.strict_mode = strict_mode
    f = open(self.full_path)
    self.contents = f.read()
    self.source = self.contents
    f.close()
    testRecord = parseTestRecord(self.contents, name)
    self.test = testRecord["test"]
    del testRecord["test"]
    del testRecord["header"]
    testRecord.pop("commentary", None)    # do not throw if missing
    self.testRecord = testRecord;

    self.validate()

  def NegativeMatch(self, stderr):
    negative_data = self.GetNegative()
    if type(negative_data) is dict:
        neg = negative_data["type"]
    else:
        neg = re.compile(negative_data)
    return re.search(neg, stderr)

  def GetNegative(self):
    return self.testRecord['negative']

  def GetName(self):
    return path.join(*self.name)

  def GetMode(self):
    if self.strict_mode:
      return "strict mode"
    else:
      return "non-strict mode"

  def GetPath(self):
    return self.name

  def IsNegative(self):
    return 'negative' in self.testRecord

  def IsOnlyStrict(self):
    return 'onlyStrict' in self.testRecord

  def IsNoStrict(self):
    return 'noStrict' in self.testRecord or self.IsRaw()

  def IsRaw(self):
    return 'raw' in self.testRecord

  def IsAsyncTest(self):
    return '$DONE' in self.test

  def GetIncludeList(self):
    if self.testRecord.get('includes'):
      return self.testRecord['includes']
    return []

  def GetAdditionalIncludes(self):
    return '\n'.join([self.suite.GetInclude(include) for include in self.GetIncludeList()])

  def GetDriverSource(self):
    source = self.suite.GetInclude("sta.js") + \
        self.suite.GetInclude("cth.js") + \
        self.suite.GetInclude("assert.js")

    if self.IsAsyncTest():
      source = source + \
               self.suite.GetInclude("timer.js") + \
               self.suite.GetInclude("doneprintHandle.js").replace('print', self.suite.print_handle)

    source = source + \
        self.GetAdditionalIncludes() + \
        '\n'
    return source

  def GetSource(self):
    if self.IsRaw():
        return self.source

    source = self.source
    # if test is hashbang test
    if len(source) > 1 and (source[0] == '#' or source[0] == '\\'):
      lines = source.splitlines()
      if self.strict_mode:
        lines.insert(1, 'var strict_mode = true;')
        lines.insert(1, '"use strict";')
      return "\n".join(lines)

    if self.strict_mode:
      source = '"use strict";\nvar strict_mode = true;\n' + source
    else:
      # add comment line so line numbers match in both strict and non-strict version
      source =  '//"no strict";\nvar strict_mode = false;\n' + source

    return source

  def InstantiateTemplate(self, template, params):
    def GetParameter(match):
      key = match.group(1)
      return params.get(key, match.group(0))
    return placeHolderPattern.sub(GetParameter, template)

  def Execute(self, command):
    if IsWindows():
      args = '%s' % command
    else:
      args = command.split(" ")
    stdout = TempFile(prefix="test262-out-")
    stderr = TempFile(prefix="test262-err-")
    try:
      logging.info("exec: %s", str(args))

      my_env = os.environ.copy()
      my_env["LD_PRELOAD"] = ESCARGOT_LD_PRELOAD

      process = subprocess.Popen(
        args,
        shell = IsWindows(),
        stdout = stdout.fd,
        stderr = stderr.fd,
        env = my_env
      )

      signal.alarm(120)  # raise Alarm in 2 minutes

      try:
        code = process.wait()
        signal.alarm(0)  # reset the alarm
      except Alarm:
        process.kill()
        process.wait()
        code = -1

      # code = process.wait()
      out = stdout.Read()
      err = stderr.Read()
    finally:
      stdout.Dispose()
      stderr.Dispose()
    return (code, out, err)

  def RunTestIn(self, command_template, driver_tmp, tmp):
    driver_source = self.GetDriverSource()
    driver_tmp.Write(driver_source)
    driver_tmp.Close()

    source = self.GetSource()
    is_module = False

    if "flags" in self.testRecord:
      is_module = "module" in self.testRecord["flags"]

    if is_module:
      is_module = "--module "
    else:
      is_module = ""

    tmp.Write(source)
    tmp.Close()

    command = self.InstantiateTemplate(command_template, {
      'driver_path': driver_tmp.name,
      'path': tmp.name,
      'test_case_path': self.full_path,
      'is_module': is_module
    })

    (code, out, err) = self.Execute(command)
    return TestResult(code, out, err, self)

  def Run(self, command_template):
    driver_tmp = TempFile(suffix=".js", prefix="test262-", text=True)
    tmp = TempFile(suffix=".js", prefix="test262-", text=True)
    try:
      result = self.RunTestIn(command_template, driver_tmp, tmp)
    finally:
      driver_tmp.Dispose()
      tmp.Dispose()
    return result

  def Print(self):
    print self.GetSource()

  def validate(self):
    flags = self.testRecord.get("flags")

    if not flags:
        return

    if 'raw' in flags:
        if 'noStrict' in flags:
            raise TypeError("The `raw` flag implies the `noStrict` flag")
        elif 'onlyStrict' in flags:
            raise TypeError(
                "The `raw` flag is incompatible with the `onlyStrict` flag")
        elif len(self.GetIncludeList()) > 0:
            raise TypeError(
                "The `raw` flag is incompatible with the `includes` tag")

class ProgressIndicator(object):

  def __init__(self, count):
    self.count = count
    self.succeeded = 0
    self.failed = 0
    self.failed_tests = []

  def HasRun(self, result):
#result.ReportOutcome(True)
    if result.HasUnexpectedOutcome():
      self.failed += 1
      self.failed_tests.append(result)
    else:
      self.succeeded += 1


def MakePlural(n):
  if (n == 1):
    return (n, "")
  else:
    return (n, "s")

def PercentFormat(partial, total):
  return "%i test%s (%.1f%%)" % (MakePlural(partial) +
                                 ((100.0 * partial)/total,))

def CaseRunner(case):
  #print(case.GetName())
  result = case.Run(case.command_template)
  return [case.index, result]

class TestSuite(object):

  def __init__(self, root, strict_only, non_strict_only, unmarked_default, print_handle):
    # TODO: derive from packagerConfig.py
    self.test_root = path.join(root, 'test')
    self.lib_root = path.join(root, 'harness')
    self.strict_only = strict_only
    self.non_strict_only = non_strict_only
    self.unmarked_default = unmarked_default
    self.print_handle = print_handle
    self.include_cache = { }
    self.total_test_number = 0

  def Validate(self):
    if not path.exists(self.test_root):
      ReportError("No test repository found")
    if not path.exists(self.lib_root):
      ReportError("No test library found")

  def IsHidden(self, path):
    return path.startswith('.') or path == 'CVS'

  def IsTestCase(self, path):
    return path.endswith('.js') and "_FIXTURE" not in path

  def ShouldRun(self, rel_path, tests):
    if len(tests) == 0:
      return True
    for test in tests:
      if test in rel_path:
        return True
    return False

  def GetInclude(self, name):
    if not name in self.include_cache:
      static = path.join(self.lib_root, name)
      if path.exists(static):
        f = open(static)
        contents = stripHeader(f.read())
        contents = re.sub(r'\r\n', '\n', contents)
        self.include_cache[name] = contents + "\n"
        f.close()
      else:
        ReportError("Can't find: " + static)
    return self.include_cache[name]

  def EnumerateTests(self, tests):
    logging.info("Listing tests in %s", self.test_root)
    cases = []
    for root, dirs, files in os.walk(self.test_root):
      for f in [x for x in dirs if self.IsHidden(x)]:
        dirs.remove(f)
      dirs.sort()
      for f in sorted(files):
        if self.IsTestCase(f):
          skip = False
          full_path = path.join(root, f)
          if full_path.startswith(self.test_root):
            rel_path = full_path[len(self.test_root)+1:]
          else:
            logging.warning("Unexpected path %s", full_path)
            rel_path = full_path
          if self.ShouldRun(rel_path, tests):
            basename = path.basename(full_path)[:-3]
            name = rel_path.split(path.sep)[:-1] + [basename]
            file_path = rel_path[0:rel_path.rindex('.')]
            if file_path in EXCLUDE_LIST:
              #print 'Excluded: ' + rel_path
              skip = True
            elif EXCLUDE_LIST.count(basename) >= 1:
              #print 'Excluded: ' + basename
              skip = True

            if not self.non_strict_only:
              strict_case = TestCase(self, name, full_path, True)
              if not strict_case.IsNoStrict():
                if strict_case.IsOnlyStrict() or \
                      self.unmarked_default in ['both', 'strict']:
                  self.total_test_number += 1
                  if not skip:
                    cases.append(strict_case)
            if not self.strict_only:
              non_strict_case = TestCase(self, name, full_path, False)
              if not non_strict_case.IsOnlyStrict():
                if non_strict_case.IsNoStrict() or \
                      self.unmarked_default in ['both', 'non_strict']:
                  self.total_test_number += 1
                  if not skip:
                    cases.append(non_strict_case)
    logging.info("Done listing tests")
    return cases


  def PrintSummary(self, progress, logfile):

    def write(s):
      if logfile:
        self.logf.write(s + "\n")
      print s

    print
    write("=== Test262 Summary ===");
    count = progress.count
    succeeded = progress.succeeded
    failed = progress.failed
    write(" - Ran %i test%s (total %i tests)" % (MakePlural(count) + (self.total_test_number,)))
    if progress.failed == 0:
      write(" - All tests succeeded")
      write(" - Total Passed " + PercentFormat(count, self.total_test_number))
    else:
      write(" - Passed " + PercentFormat(succeeded, count))
      write(" - Failed " + PercentFormat(failed, count))
      positive = [c for c in progress.failed_tests if not c.case.IsNegative()]
      negative = [c for c in progress.failed_tests if c.case.IsNegative()]
      if len(positive) > 0:
        print
        write("Test262 Failed Tests")
        for result in positive:
          write("  %s in %s" % (result.case.GetName(), result.case.GetMode()))
        write("Failed Tests End")
      if len(negative) > 0:
        print
        write("Test262 Expected to fail but passed")
        for result in negative:
          write("  %s in %s" % (result.case.GetName(), result.case.GetMode()))
        write("Expected to fail End")

  def PrintFailureOutput(self, progress, logfile):
    for result in progress.failed_tests:
      if logfile:
        self.WriteLog(result)
      print
      result.ReportOutcome(False)

  def Run(self, command_template, tests, print_summary, full_summary, logname, junitfile):
    if not "{{path}}" in command_template:
      command_template += " {{driver_path}} {{is_module}}--filename-as={{test_case_path}} {{path}}"
    cases = self.EnumerateTests(tests)
    if len(cases) == 0:
      ReportError("No tests to run")
    progress = ProgressIndicator(len(cases))
    if logname:
      self.logf = open(logname, "w")
    if junitfile:
      self.outfile = open(junitfile, "w")
      TestSuitesElement = xmlj.Element("testsuites")
      TestSuiteElement = xmlj.Element("testsuite")
      TestSuitesElement.append(TestSuiteElement)
      TestSuiteElement.attrib["name "] = "test262"
      for x in range(len(EXCLUDE_LIST)):
        if self.ShouldRun (unicode(EXCLUDE_LIST[x].encode('utf-8','ignore')), tests):
          SkipCaseElement = xmlj.Element("testcase")
          SkipCaseElement.attrib["classname"] = unicode(EXCLUDE_LIST[x]).encode('utf-8','ignore')
          SkipCaseElement.attrib["name"] = unicode(EXCLUDE_LIST[x]).encode('utf-8','ignore')
          SkipElement = xmlj.Element("skipped")
          SkipElement.attrib["message"] = unicode(EXCLUDE_REASON[x].firstChild.nodeValue)
          SkipCaseElement.append(SkipElement)
          TestSuiteElement.append(SkipCaseElement)

    from multiprocessing import Pool
    import multiprocessing
    pool = Pool(processes=multiprocessing.cpu_count())

    casesV = []
    for case in cases:
        case.command_template = command_template
        case.index = cases.index(case)
        casesV.append(case)

    casesResult = pool.imap_unordered(CaseRunner, casesV)

    resultTemp = [None] * len(cases)

    for cr in casesResult:
      resultTemp[cr[0]] = cr[1]

    for case in cases:
      #result = case.Run(command_template)
      result = resultTemp[cases.index(case)]

      if junitfile:
        TestCaseElement = result.XmlAssemble(result)
        TestSuiteElement.append(TestCaseElement)
        if case == cases[len(cases)-1]:
             xmlj.ElementTree(TestSuitesElement).write(junitfile, "UTF-8")
      if logname:
        self.WriteLog(result)
      progress.HasRun(result)

    pool.close()
    pool.join()

    if print_summary:
      self.PrintSummary(progress, logname)
    '''
      if full_summary:
        self.PrintFailureOutput(progress, logname)
      else:
        print
        print "Use --full-summary to see output from failed tests"
    '''
    print
    return progress.failed

  def WriteLog(self, result):
    name = result.case.GetName()
    mode = result.case.GetMode()
    if result.HasUnexpectedOutcome():
      if result.case.IsNegative():
          self.logf.write("=== %s was expected to fail in %s, but didn't === \n" % (name, mode))
          self.logf.write("--- expected error: %s ---\n" % result.case.GetNegative())
          result.WriteOutput(self.logf)
      else:
          self.logf.write("=== %s failed in %s === \n" % (name, mode))
          result.WriteOutput(self.logf)
      self.logf.write("===\n")
    elif result.case.IsNegative():
      self.logf.write("%s failed in %s as expected \n" % (name, mode))
    else:
      self.logf.write("%s passed in %s \n" % (name, mode))

  def Print(self, tests):
    cases = self.EnumerateTests(tests)
    if len(cases) > 0:
      cases[0].Print()

  def ListIncludes(self, tests):
    cases = self.EnumerateTests(tests)
    includes_dict = Counter()
    for case in cases:
      includes = case.GetIncludeList()
      includes_dict.update(includes)

    print includes_dict


def Main():
  code = 0
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  ValidateOptions(options)
  test_suite = TestSuite(options.tests,
                         options.strict_only,
                         options.non_strict_only,
                         options.unmarked_default,
			 options.print_handle)
  test_suite.Validate()
  if options.loglevel == 'debug':
    logging.basicConfig(level=logging.DEBUG)
  elif options.loglevel == 'info':
    logging.basicConfig(level=logging.INFO)
  elif options.loglevel == 'warning':
    logging.basicConfig(level=logging.WARNING)
  elif options.loglevel == 'error':
    logging.basicConfig(level=logging.ERROR)
  elif options.loglevel == 'critical':
    logging.basicConfig(level=logging.CRITICAL)
  if options.cat:
    test_suite.Print(args)
  elif options.list_includes:
    test_suite.ListIncludes(args)
  else:
    code = test_suite.Run(options.command, args,
                          options.summary or options.full_summary,
                          options.full_summary,
                          options.logname,
                          options.junitname)
  return code

if __name__ == '__main__':
  try:
    code = Main()
    sys.exit(code)
  except Test262Error, e:
    print "Error: %s" % e.message
    sys.exit(1)
