#!/usr/bin/env python

import sys
from multiprocessing import Pool
import multiprocessing
import subprocess

def CaseRunner(Case):
  Case["CommandLine"] = Case["CommandLine"] + Case["TestCaseCommandline"].split()
  Process = subprocess.Popen(Case["CommandLine"], shell = False, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  ResultCode = Process.wait()

  Out = ""
  for line in iter(Process.stdout.readline,''):
    Out = Out + line

  Err = ""
  for line in iter(Process.stderr.readline,''):
    Err = Err + line


  if ResultCode == 0:
    ResultCode = True
    print(Case["TestCaseCommandline"] + " - success")
  else:
    ResultCode = False
    print(Case["TestCaseCommandline"] + " fail")

  return { "Index": Case["Index"], "Result": ResultCode, "Case": Case["TestCaseCommandline"], "Out": Out, "Err": Err, "CommandLine": ' '.join(Case["CommandLine"]) }

def Main(JSShellFile, TestDataFile):
  TestCases = []
  with open(TestDataFile) as f:
    TestCases = f.read().splitlines()
  TestPool = Pool(processes=multiprocessing.cpu_count())

  CasesV = []
  for Case in TestCases:
    C = Case
    Case = {}
    Case["CommandLine"] = [JSShellFile, "util.js"]
    Case["TestCaseCommandline"] = C
    Case["Index"] = len(CasesV)
    CasesV.append(Case)

  CasesResultTemp = TestPool.imap_unordered(CaseRunner, CasesV)

  Result = [None] * len(TestCases)

  for c in CasesResultTemp:
    Result[c["Index"]] = c

  #print(Result)
  print("result..............................")
  FinalResult = True
  SCount = 0
  FCount = 0
  for t in Result:
    Txt = t["Case"] + " - "
    if t["Result"]:
      Txt = Txt + "success"
      SCount = SCount + 1
      print(Txt)
    else:
      Txt = Txt + "fail"
      FCount = FCount + 1
      FinalResult = False
      print(Txt)
      print("command line")
      print(t["CommandLine"])
      print("stdout")
      print(t["Out"])
      print("stderr")
      print(t["Err"])

  print("Ran " + str(SCount + FCount) + " Tests")
  print("Success case number: " + str(SCount))
  print("Fail case number: " + str(FCount))
  if FCount == 0:
    print("SUCCESS")
    sys.exit(0)
  else:
    print("FAIL")
    sys.exit(1)

if __name__ == '__main__':
  #print(str(sys.argv))
  JSShellFile = sys.argv[1]
  TestDataFile = sys.argv[2]
  print("starting test with " + JSShellFile + " and " + TestDataFile)
  Main(JSShellFile, TestDataFile)
