#!/usr/bin/env python3

import sys
import os

MAX_UCS2 = 0xFFFF

g_upper_case_data = [0] * (MAX_UCS2 + 1)

def to_hex(val, width=4):
    return f"0x{val:0{width}X}".lower()

def ucs2_to_upper(i):
    if i > MAX_UCS2:
        return i
    if g_upper_case_data[i] != 0:
        return g_upper_case_data[i]
    return i

def from_char_code(code):
    if code < 0x10000:
        return [code]
    else:
        h = 0xD800 + ((code - 0x10000) >> 10)
        l = 0xDC00 + ((code - 0x10000) & 1023)
        return [h, l]

def canonicalize(ch):
    """ES 6.0, 21.2.2.8.2 Steps 3"""
    u = from_char_code(ch)
    
    u = [ucs2_to_upper(c) for c in u]

    if len(u) > 1:
        return ch

    cu = u[0]
    if ch >= 128 and cu < 128:
        return ch
    return cu

def create_ucs2_canonical_groups():
    grouped_canonically = {} # dict: char16_t -> list of char16_t

    for i in range(MAX_UCS2 + 1):
        ch = canonicalize(i)
        if ch not in grouped_canonically:
            grouped_canonically[ch] = []
        grouped_canonically[ch].append(i)
    
    return grouped_canonically

def create_tables(fp, prefix, max_value, canonical_groups):
    prefix_lower = prefix.lower()
    
    type_info = [""] * (max_value + 1)
    character_set_info = []

    sorted_keys = sorted(canonical_groups.keys())

    for cu in sorted_keys:
        characters = canonical_groups[cu]
        
        if len(characters) == 1:
            type_info[characters[0]] = "CanonicalizeUnique:0"
            continue
        
        characters.sort()

        if len(characters) > 2:
            idx = len(character_set_info)
            for char_code in characters:
                type_info[char_code] = f"CanonicalizeSet:{idx}"
            character_set_info.append(characters)
            continue
        
        lo = characters[0]
        hi = characters[1]
        delta = hi - lo
        
        if delta == 1:
            if lo & 1:
                t = "CanonicalizeAlternatingUnaligned:0"
            else:
                t = "CanonicalizeAlternatingAligned:0"
            type_info[lo] = t
            type_info[hi] = t
        else:
            type_info[lo] = f"CanonicalizeRangeLo:{delta}"
            type_info[hi] = f"CanonicalizeRangeHi:{delta}"

    range_info = []
    end = 0
    while end <= max_value:
        begin = end
        current_type = type_info[end]
        
        while end < max_value and type_info[end + 1] == current_type:
            end += 1
        
        range_info.append({
            'begin': begin,
            'end': end,
            'type': current_type
        })
        end += 1

    character_set_var_name = prefix_lower + "CharacterSet"
    for i, char_set in enumerate(character_set_info):
        chars_str = ""
        for j in char_set:
            chars_str += to_hex(j) + ", "
        
        fp.write(f"constexpr char32_t {character_set_var_name}{i}[] = {{ {chars_str}0 }};\n")
    
    fp.write("\n")

    canonicalization_sets_var_name = prefix_lower + "CanonicalizationSets"
    fp.write(f"static constinit const size_t {canonicalization_sets_var_name} = {len(character_set_info)};\n")
    fp.write(f"constinit const char32_t* {prefix_lower}CharacterSetInfo[{canonicalization_sets_var_name}] = {{\n")
    
    for i in range(len(character_set_info)):
        fp.write(f"    {character_set_var_name}{i},\n")
    fp.write("};\n\n")

    canonicalization_ranges_var_name = prefix_lower + "CanonicalizationRanges"
    fp.write(f"constinit const size_t {canonicalization_ranges_var_name} = {len(range_info)};\n")
    fp.write(f"constinit const CanonicalizationRange {prefix_lower}RangeInfo[{canonicalization_ranges_var_name}] = {{\n")

    for info in range_info:
        type_str = info['type']
        if ':' in type_str:
            type_name, val_str = type_str.split(':')
            val = int(val_str)
        else:
            type_name = type_str
            val = 0
            
        fp.write(f"    {{ {to_hex(info['begin'])}, {to_hex(info['end'])}, {to_hex(val)}, {type_name} }},\n")
        
    fp.write("};\n\n")

def main():
    if len(sys.argv) < 3:
        print(f"Usage: python {sys.argv[0]} <UnicodeData.txt> <Output.cpp>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line: continue
            
            parts = line.split(';')
            if len(parts) < 13: continue

            try:
                code_point = int(parts[0], 16)
            except ValueError:
                continue

            if code_point <= MAX_UCS2:
                upper_mapping = parts[12].strip()
                if upper_mapping:
                    g_upper_case_data[code_point] = int(upper_mapping, 16)

    with open(output_path, 'w', encoding='utf-8') as fp:
        head = """/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
// DO NOT EDIT! - this file autogenerated by generateYarrCanonicalizeUCS2.py

#include "WTFBridge.h"
#include "YarrCanonicalize.h"

namespace JSC { namespace Yarr {
"""
        fp.write(head)

        canonical_groups = create_ucs2_canonical_groups()
        create_tables(fp, "ucs2", MAX_UCS2, canonical_groups)

        tail = """
} } // JSC::Yarr
"""
        fp.write(tail)

if __name__ == "__main__":
    main()
