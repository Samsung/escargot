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

import subprocess
import sys
import os

from argparse import ArgumentParser

PATH = os.path.dirname(os.path.abspath(__file__))

TERM_RED = '\033[1;31m'
TERM_GREEN = '\033[1;32m'
TERM_YELLOW = '\033[1;33m'
TERM_EMPTY = '\033[0m'


def separator_line():
    print("{0}=====================================\n {1}".format(TERM_YELLOW, TERM_EMPTY))


def get_results(filename, no_of_lines=1):
    res = []
    file = open(filename,'r')
    lines = file.readlines()
    last_lines = lines[-no_of_lines:]
    for line in last_lines:
        res.append(line)
    file.close()
    return res


def run_testsuite(suite,engine):
    separator_line()
    print("{0}Running kangax suite(s):\n {1}{2}".format(TERM_YELLOW, 'es6\n es2016plus\n' if suite=='""' else suite, TERM_EMPTY))
    separator_line()

    if engine != 'local':
        engine = engine
    else:
        engine = '../../escargot'

    os.chdir(PATH + '/../../test/kangax/')
    if suite == '""':
        es6res = open("./kangaxES6Res.txt", "w")
        es6plusres = open("./kangaxES6PLUSRes.txt", "w")
        nodeFails = open("./kangaxRunErrors.txt", "w")
        subprocess.call(['node', './escargot.js', engine, 'es6'], stdout = es6res, stderr = nodeFails)
        print("{0}ES6 RESULTS:\n{1}".format(TERM_YELLOW, TERM_EMPTY))
        grepProc = subprocess.Popen(['grep -hr "actual: false" ./kangaxES6Res.txt'], stdout=subprocess.PIPE, shell=True)
        (fails, err) = grepProc.communicate()
        print(''.join(get_results("./kangaxES6Res.txt", 2)))
        print("{0}ES6 Fails:\n{1}".format(TERM_RED, TERM_EMPTY))
        print(''.join(fails))
        print("{0}Errors:\n{1}".format(TERM_RED, TERM_EMPTY))
        es6res.close()
        nodeFails.close()
        with open("./kangaxRunErrors.txt", "r") as lines:
            print(lines.read())
        print('\n')

        nodeFails = open("./kangaxRunErrors.txt", "w")
        subprocess.call(['node', './escargot.js', engine, 'es2016plus'], stdout = es6plusres, stderr = nodeFails)
        print("{0}ES6PLUS RESULTS:\n{1}".format(TERM_YELLOW, TERM_EMPTY))
        grepProc = subprocess.Popen(['grep -hr "actual: false" ./kangaxES6PLUSRes.txt'], stdout=subprocess.PIPE, shell=True)
        (fails, err) = grepProc.communicate()
        print(''.join(get_results("./kangaxES6PLUSRes.txt", 2)))
        print("{0}ES6PLUS FAILS:\n{1}".format(TERM_RED, TERM_EMPTY))
        print(''.join(fails))
        print("{0}Errors:\n{1}".format(TERM_RED, TERM_EMPTY))
        es6plusres.close()
        nodeFails.close()
        with open("./kangaxRunErrors.txt", "r") as lines:
            print(lines.read())
        print('\n')
    else:
        res = open("./kangaxRes.txt", "w")
        nodeFails = open("./kangaxRunErrors.txt", "w")
        subprocess.call(['node', './escargot.js', engine, suite], stdout = res, stderr = nodeFails)
        grepProc = subprocess.Popen(['grep -hr "actual: false" ./kangaxRes.txt'], stdout=subprocess.PIPE, shell=True)
        (fails, err) = grepProc.communicate()
        print("{0}{1} RESULTS:\n\n{2}".format(TERM_YELLOW, suite, TERM_EMPTY))
        print( ''.join(get_results("./kangaxRes.txt", 2)))
        print("{0}FAILS:\n{1}".format(TERM_RED, TERM_EMPTY))
        print(''.join(fails))
        print("{0}Errors:\n{1}".format(TERM_RED, TERM_EMPTY))
        res.close()
        nodeFails.close()
        with open("./kangaxRunErrors.txt", "r") as lines:
            print(lines.read())
        print('\n')


def kangax_setup():
    os.chdir(PATH + '/../../test/')
    if os.path.isdir("./kangax/"):
        print("{0}/test/kangax directory is present\nAssuming correct repository there{1}\n".format(TERM_YELLOW, TERM_EMPTY))
    else:
        print("{0}Update kangax repository\n{1}{2}\n".format(TERM_YELLOW, PATH + '/../../test/' + 'kangax', TERM_EMPTY))
        os.system("git submodule update {0}".format(PATH + '/../../test/kangax'))


    separator_line()
    print("{0}Patching kangax enviroment\n applying patch{1}".format(TERM_YELLOW, TERM_EMPTY))
    os.chdir(PATH + '/../../test/kangax')
    os.system('git apply {0}'.format(PATH + '/escargot.patch'))
    os.chdir(PATH)
    print("{0} adding Escargot runner{1}\n".format(TERM_YELLOW, TERM_EMPTY))
    os.system('cp {0} {1}'.format(PATH + '/escargot.js',PATH + '/../../test/kangax/'))



def main():
    parser = ArgumentParser(description='Kangax runner for the Escargot engine')
    parser.add_argument('--suite', nargs='?', default='""',  choices=['es6', 'es2016plus'], help='Run kangax suite (choices: %(choices)s)')
    parser.add_argument('--engine', nargs='?', default='local', help='Define escargot path. Leave empty for project root ./escargot file')
    args = parser.parse_args()

    separator_line()
    print("{0}KANGAX RUNNER FOR \nESCARGOT REPOSITORY\n{1}".format(TERM_YELLOW, TERM_EMPTY))
    separator_line()
    kangax_setup()
    run_testsuite(args.suite,args.engine)


if __name__ == '__main__':
    main()
