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

import subprocess
import os

BDWGC_LOGFILE="bdwgcUsage.dat"
GNUPLOT_DISPLAY_STYLE="lines"

def make_plot_subcmd(col, name):
    return '\"%s\" using 1:%d title \"%s (KB)\" with %s' % (BDWGC_LOGFILE, col, name, GNUPLOT_DISPLAY_STYLE)

def draw_bdwgc_plot():
    if not os.path.exists(BDWGC_LOGFILE):
        print 'Cannot draw plot! No input file %s' % BDWGC_LOGFILE
        print 'Please re-build escargot with `-DPROFILE_BDWGC` and run again.'
        exit(1)

    plot_cmd = 'plot '
    plot_cmd += make_plot_subcmd(2, "Peak RSS")
    plot_cmd += ', '
    plot_cmd += make_plot_subcmd(3, "Total Heap")
    plot_cmd += ', '
    plot_cmd += make_plot_subcmd(4, "Marked Heap")

    print "gnuplot -p -e '%s'" % plot_cmd
    print
    print "See '%s' to see raw data." % BDWGC_LOGFILE

    subprocess.call(['gnuplot', '-p', '-e', plot_cmd])

if __name__ == '__main__':
    draw_bdwgc_plot()
