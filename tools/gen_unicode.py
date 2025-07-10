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


PARAGRAPH_SEP = "# ================================================\n"
ID_START_TITLE = "# Derived Property: ID_Start\n"
ID_CONTINUE_TITLE = "# Derived Property: ID_Continue\n"


BASIC_PLANE_START = dict()
LONG_RANGE_LENGHT = []
BASIC_PLANE_CONTINUE = dict()


SUPPLEMENTARY_PLANE = dict()
MERGED_ID_START = dict()


TERM_RED = '\033[1;31m'
TERM_GREEN = '\033[1;32m'
TERM_YELLOW = '\033[1;33m'
TERM_EMPTY = '\033[0m'


class UnicodeTable:
    def __init__(self):
        self.header = []
        self.data = []
        self.footer = []

    def add_header(self, header_text):
        self.header.append(header_text)
        self.header.append("")


    def add_footer(self, footer_text):
        self.footer.append(footer_text)
        self.footer.append("")


    def add_table(self, table, table_name, table_type, table_descr):
        self.data.append("/* %s */" % (table_descr))
        self.data.append("const %s identRange%s[%s] = {" % (table_type,table_name,len(table)))
        self.data.append('    ' + ', '.join(map(str, table)))
        self.data.append("};")
        self.data.append("")

    def generate(self):
        with open(os.path.dirname(os.path.abspath(__file__)) + "/../src/parser/UnicodeIdentifierTables.cpp", 'w') as unicode_table_file:
            unicode_table_file.write("\n".join([str(i) for i in self.header]))
            unicode_table_file.write("\n".join([str(i) for i in self.data]))
            unicode_table_file.write("\n".join([str(i) for i in self.footer]))


def create_basic_plane_table():

    # zero_width_non_joiner and zero_width_joiner needs to be added here
    # http://www.ecma-international.org/ecma-262/10.0/#prod-IdentifierName
    # $ and _ are handled directly in the Escargot Lexer
    other_starters = [0x200C,0x200D]
    for key in other_starters:
        BASIC_PLANE_START[key] = 1

    # Create new merged dict
    for k, v in BASIC_PLANE_START.items():
        MERGED_ID_START[k] = v

    # Copy unicodes not present in base
    for continue_key in BASIC_PLANE_CONTINUE.keys():
        if not MERGED_ID_START.has_key(continue_key):
            MERGED_ID_START[continue_key] = BASIC_PLANE_CONTINUE[continue_key]

    # Merge ranges if possible
    # Get collection of keys
    ordered_keys = sorted(MERGED_ID_START.keys())
    for key in range(len(MERGED_ID_START)-1, 1, -1):
        # If key value is equal than
        if ordered_keys[key] == ordered_keys[key-1]+MERGED_ID_START[ordered_keys[key-1]]+1:
            # combine the length of the two
            # refresh key length value with the sum of the sperate values
            # pop next value out from the dict
            new_length = MERGED_ID_START[ordered_keys[key]] + MERGED_ID_START[ordered_keys[key-1]] + 1
            MERGED_ID_START[ordered_keys[key-1]] = new_length
            error = MERGED_ID_START.pop(ordered_keys[key])

    # Create the long range table
    long_length_index = 0
    for key, value in MERGED_ID_START.iteritems():
        if value >= 200:
            LONG_RANGE_LENGHT.append(value)
            new_value = 200+long_length_index
            long_length_index+=1
            MERGED_ID_START[key] = new_value


def unify_non_basic_plane():
    # Merge ranges if possible
    # Get collection of keys
    ordered_keys = sorted(SUPPLEMENTARY_PLANE.keys())
    for key in range(len(SUPPLEMENTARY_PLANE)-1, 1, -1):
        # If key value is equal than
        if ordered_keys[key] == ordered_keys[key-1]+SUPPLEMENTARY_PLANE[ordered_keys[key-1]]+1:
            # Combine the length of the two
            # Refresh key length value with the sum of the sperate values
            # Pop next value out from the dict
            new_length = SUPPLEMENTARY_PLANE[ordered_keys[key]] + SUPPLEMENTARY_PLANE[ordered_keys[key-1]] + 1
            SUPPLEMENTARY_PLANE[ordered_keys[key-1]] = new_length
            error = SUPPLEMENTARY_PLANE.pop(ordered_keys[key])


def generate_ranges(file):
    bigger = 0
    long_range_index = 0
    with open(file, "r") as fp:
        line = fp.readline()
        while line:
            line = fp.readline()
            if (line == PARAGRAPH_SEP):
                # Read twice to skip empty line
                paragraph_title = fp.readline()
                paragraph_title = fp.readline()
                if (paragraph_title == ID_START_TITLE) or (paragraph_title == ID_CONTINUE_TITLE):
                    long_range_index = 0
                    # Skip 8 lines, containing definition of ID_START
                    # or skip 9 in case of ID_CONTINUE
                    skip = 8
                    if (paragraph_title == ID_CONTINUE_TITLE):
                        skip = 9

                    for _ in range(skip):
                        fp.readline()
                    unicode_range_line = fp.readline()
                    while (unicode_range_line != "\n"):
                        sub_lines = ""
                        ranges = ""
                        # Cut out ranges
                        sub_lines = unicode_range_line.split(';')
                        # Cut ranges to check length
                        # Convert into int
                        ranges = sub_lines[0].split('..')
                        start = (int(ranges[0],16))

                        if (len(ranges) == 2):
                            length = (int(ranges[1],16) - int(ranges[0],16))
                        else:
                            length = 1

                        # Basic plane for Escargot's uint16_t
                        # Skip the ascii part as it is handled directly
                        # in the Lexer.cpp
                        if (128 < start) and (start <= 65535):
                            if skip == 8:
                                if not BASIC_PLANE_START.has_key(start):
                                    BASIC_PLANE_START[start] = length
                            else:
                                if not BASIC_PLANE_CONTINUE.has_key(start):
                                    BASIC_PLANE_CONTINUE[start] = length
                        # Supplementary Plane data
                        elif start > 65535:
                            SUPPLEMENTARY_PLANE[start] = length
                        unicode_range_line = fp.readline()


def main():
    print("\n%sAutomated Unicode Identifier Table Generator for Escargot %s\n" % (TERM_GREEN,TERM_EMPTY))
    parser = ArgumentParser(description='Kangax runner for the Escargot engine')
    parser.add_argument('--derived_core_properties', metavar='FILE', action='store', required=True, help='specify the unicode data file')
    args = parser.parse_args()

    if not os.path.isfile(args.derived_core_properties):
        parser.error("\n\n%sArgument file is non-existent!%s\nGiven: %s\n" % (TERM_RED,TERM_EMPTY,args.derived_core_properties))

    # Process Derived File
    print("%s..Processing Derived Core Properties file %s\n" % (TERM_YELLOW,TERM_EMPTY))
    generate_ranges(args.derived_core_properties)

    # Porcess data for the basic plane
    # and unify the supplementary
    print("%s..Calculating proper ranges %s\n" % (TERM_YELLOW,TERM_EMPTY))
    create_basic_plane_table()
    unify_non_basic_plane()

    # Start collecting output data
    print("%s..Collecting output data %s\n" % (TERM_YELLOW,TERM_EMPTY))
    generated_data = UnicodeTable()
    licence = "/* * Copyright (c) 2020-present Samsung Electronics Co., Ltd\n *\n *  This library is free software; you can redistribute it and/or\n *  modify it under the terms of the GNU Lesser General Public\n *  License as published by the Free Software Foundation; either\n *  version 2.1 of the License, or (at your option) any later version.\n *\n *  This library is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n *  Lesser General Public License for more details.\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; if not, write to the Free Software\n *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301\n *  USA\n */\n"
    header = "/* This file is automatically generated by the %s script\n * from https://www.unicode.org/Public/<version>/ucd/DerivedCoreProperties.txt\n * DO NOT EDIT!\n */" % (os.path.basename(__file__))
    ifdef_namespace ="#include \"UnicodeIdentifierTables.h\"\n\nnamespace Escargot {\nnamespace EscargotLexer {\n"
    footer_namespace = "const uint16_t basic_plane_length = sizeof(identRangeStart);\nconst uint16_t supplementary_plane_length = sizeof(identRangeStartSupplementaryPlane);\n} // namespace EscargotLexer\n} // namespace Escargot"

    # Append Generated Warning, licence, ifdef
    generated_data.add_header(licence)
    generated_data.add_header(header)
    generated_data.add_header(ifdef_namespace)

    # Fetch data for the basic plane
    keys = sorted(MERGED_ID_START.keys())
    values = []
    for key in keys:
        values.append(MERGED_ID_START[key])

    # Append basic plane values
    generated_data.add_table(keys,"Start","uint16_t","Starting codepoints of identifier ranges.")
    generated_data.add_table(values,"Length","uint16_t","Lengths of identifier ranges.")
    generated_data.add_table(LONG_RANGE_LENGHT,"LongLength","uint16_t","Lengths of identifier ranges greater than LEXER IDENT_RANGE_LONG.")

    # Fetch data for the supplementary plane
    keys = sorted(SUPPLEMENTARY_PLANE.keys())
    values = []
    for key in keys:
        values.append(SUPPLEMENTARY_PLANE[key])

    # Append suppelmentary values
    generated_data.add_table(keys,"StartSupplementaryPlane","uint32_t","Identifier starting codepoints for the supplementary plane")
    generated_data.add_table(values,"LengthSupplementaryPlane","uint16_t","Lengths of identifier ranges for the supplementary plane")
    # Append footer
    generated_data.add_footer(footer_namespace)

    # Writeout data
    print("%s..Writing out data %s\n" % (TERM_YELLOW,TERM_EMPTY))
    generated_data.generate()
    print("\n%sTables generated into src/parser/UnicodeIdentifierTables.cpp %s\n" % (TERM_GREEN,TERM_EMPTY))

if __name__ == '__main__':
    main()
