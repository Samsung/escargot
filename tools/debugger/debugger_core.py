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

from __future__ import print_function
import argparse
import logging
import re
import select
import struct
import sys

# Escargot debugger configuration
ESCARGOT_DEBUGGER_VERSION = 1

# Messages sent by Escargot to the debugger client.
ESCARGOT_MESSAGE_VERSION = 0
ESCARGOT_MESSAGE_CONFIGURATION = 1
ESCARGOT_MESSAGE_CLOSE_CONNECTION = 2
ESCARGOT_MESSAGE_RELEASE_FUNCTION = 3
ESCARGOT_MESSAGE_PARSE_DONE = 4
ESCARGOT_MESSAGE_PARSE_ERROR = 5
ESCARGOT_MESSAGE_SOURCE_8BIT = 6
ESCARGOT_MESSAGE_SOURCE_8BIT_END = 7
ESCARGOT_MESSAGE_SOURCE_16BIT = 8
ESCARGOT_MESSAGE_SOURCE_16BIT_END = 9
ESCARGOT_MESSAGE_FILE_NAME_8BIT = 10
ESCARGOT_MESSAGE_FILE_NAME_8BIT_END = 11
ESCARGOT_MESSAGE_FILE_NAME_16BIT = 12
ESCARGOT_MESSAGE_FILE_NAME_16BIT_END = 13
ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT = 14
ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END = 15
ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT = 16
ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END = 17
ESCARGOT_MESSAGE_BREAKPOINT_LOCATION = 18
ESCARGOT_MESSAGE_FUNCTION_PTR = 19
ESCARGOT_MESSAGE_BREAKPOINT_HIT = 20
ESCARGOT_MESSAGE_EXCEPTION_HIT = 21


# Messages sent by the debugger client to Escargot.
ESCARGOT_MESSAGE_FUNCTION_RELEASED = 0
ESCARGOT_MESSAGE_UPDATE_BREAKPOINT = 1
ESCARGOT_MESSAGE_STEP = 2
ESCARGOT_MESSAGE_CONTINUE = 3


def arguments_parse():
    parser = argparse.ArgumentParser(description="Escargot debugger client")

    parser.add_argument("address", action="store", nargs="?", default="localhost:6501",
                        help="specify a unique network address for tcp connection (default: %(default)s)")
    parser.add_argument("-v", "--verbose", action="store_true", default=False,
                        help="increase verbosity (default: %(default)s)")
    parser.add_argument("--non-interactive", action="store_true", default=False,
                        help="disable stop when newline is pressed (default: %(default)s)")
    parser.add_argument("--color", action="store_true", default=False,
                        help="enable color highlighting on source commands (default: %(default)s)")
    parser.add_argument("--display", action="store", default=None, type=int,
                        help="set display range")
    parser.add_argument("--exception", action="store", default=None, type=int, choices=[0, 1],
                        help="set exception config, usage 1: [Enable] or 0: [Disable]")
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(format="%(levelname)s: %(message)s", level=logging.DEBUG)
        logging.debug("Debug logging mode: ON")

    return args


class Breakpoint(object):
    def __init__(self, line, offset, function):
        self.line = line
        self.offset = offset
        self.function = function
        self.active_index = -1

    def __str__(self):
        result = self.function.source_name or "<unknown>"
        result += ":%d" % (self.line)
        return result

    def __repr__(self):
        return ("Breakpoint(line:%d, offset:%d, active_index:%d)"
                % (self.line, self.offset, self.active_index))

class EscargotFunction(object):
    # pylint: disable=too-many-instance-attributes,too-many-arguments
    def __init__(self, byte_code_ptr, source, source_name, name, locations):
        self.byte_code_ptr = byte_code_ptr
        self.source = re.split("\r\n|[\r\n]", source)
        self.source_name = source_name
        self.name = name
        self.lines = {}
        self.offsets = {}
        self.line = locations[0][0]
        self.first_breakpoint_line = locations[0][0]
        self.first_breakpoint_offset = locations[0][1]

        if len(self.source) > 1 and not self.source[-1]:
            self.source.pop()

        for location in locations:
            breakpoint = Breakpoint(location[0], location[1], self)
            self.lines[location[0]] = breakpoint
            self.offsets[location[1]] = breakpoint

    def __repr__(self):
        result = ("Function(byte_code_ptr:0x%x, source_name:%r, name:%r { "
                  % (self.byte_code_ptr, self.source_name, self.name))

        result += ','.join([str(breakpoint) for breakpoint in self.lines.values()])

        return result + " })"


class Multimap(object):

    def __init__(self):
        self.map = {}

    def get(self, key):
        if key in self.map:
            return self.map[key]
        return []

    def insert(self, key, value):
        if key in self.map:
            self.map[key].append(value)
        else:
            self.map[key] = [value]

    def delete(self, key, value):
        items = self.map[key]

        if len(items) == 1:
            del self.map[key]
        else:
            del items[items.index(value)]

    def __repr__(self):
        return "Multimap(%r)" % (self.map)


class DebuggerAction(object):
    END = 0
    WAIT = 1
    TEXT = 2
    PROMPT = 3

    def __init__(self, action_type, action_text):
        self.action_type = action_type
        self.action_text = action_text

    def get_type(self):
        return self.action_type

    def get_text(self):
        return self.action_text


class Debugger(object):
    # pylint: disable=too-many-instance-attributes,too-many-statements,too-many-public-methods,no-self-use,redefined-variable-type
    def __init__(self, channel):
        self.prompt = False
        self.function_list = {}
        self.source = ''
        self.source_name = ''
        self.exception_string = ''
        self.frame_index = 0
        self.scope_vars = ""
        self.scopes = ""
        self.client_sources = []
        self.last_breakpoint_hit = None
        self.next_breakpoint_index = 0
        self.active_breakpoint_list = {}
        self.line_list = Multimap()
        self.display = 0
        self.green = ''
        self.red = ''
        self.yellow = ''
        self.green_bg = ''
        self.yellow_bg = ''
        self.blue = ''
        self.nocolor = ''
        self.src_offset = 0
        self.src_offset_diff = 0
        self.non_interactive = False
        self.current_out = b""
        self.current_log = b""
        self.channel = channel

        # The server will send the version message after connection established
        # type [1]
        # littleEndian [1]
        # version [4]
        result = self.channel.connect()

        if len(result) != 6 or ord(result[0]) != ESCARGOT_MESSAGE_VERSION:
            raise Exception("Unexpected version info")

        self.little_endian = ord(result[1]) != 0

        if self.little_endian:
            self.byte_order = "<"
            logging.debug("Little-endian machine")
        else:
            self.byte_order = ">"
            logging.debug("Big-endian machine")

        self.version = struct.unpack(self.byte_order + 'I', result[2:6])[0]
        if self.version != ESCARGOT_DEBUGGER_VERSION:
            raise Exception("Incorrect debugger version from target: %d expected: %d" %
                            (self.version, ESCARGOT_DEBUGGER_VERSION))

        result = self.channel.get_message(True)

        print("Connection created!!!")

        self.pointer_size = ord(result[1])

        if self.pointer_size == 8:
            self.pointer_format = "Q"
        elif self.pointer_size == 4:
            self.pointer_format = "I"
        else:
            raise Exception("Unsupported pointer size: %d" % (self.pointer_size))

        self.idx_format = "I"

#        logging.debug("Compressed pointer size: %d", self.cp_size)

    def __del__(self):
        if self.channel is not None:
            self.channel.close()

    def encode8(self, string):
        return string.encode("latin1")

    def encode16(self, string):
        return string.encode("UTF-16LE" if self.little_endian else "UTF-16BE", "namereplace")

    # pylint: disable=too-many-branches,too-many-locals,too-many-statements,too-many-return-statements
    def process_messages(self):
        result = ""
        while True:
            data = self.channel.get_message(False)
            if not self.non_interactive:
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    sys.stdin.readline()
                    self.stop()

            if data == b'':
                action_type = DebuggerAction.PROMPT if self.prompt else DebuggerAction.WAIT
                return DebuggerAction(action_type, "")

            if not data:  # Break the while loop if there is no more data.
                return DebuggerAction(DebuggerAction.END, "")

            buffer_type = ord(data[0])
            buffer_size = len(data) - 1

            logging.debug("Main buffer type: %d, message size: %d", buffer_type, buffer_size)

            if buffer_type in [ESCARGOT_MESSAGE_SOURCE_8BIT,
                               ESCARGOT_MESSAGE_SOURCE_8BIT_END,
                               ESCARGOT_MESSAGE_SOURCE_16BIT,
                               ESCARGOT_MESSAGE_SOURCE_16BIT_END]:
                result = self._parse_source(data)
                if result:
                    return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == ESCARGOT_MESSAGE_CLOSE_CONNECTION:
                return DebuggerAction(DebuggerAction.END, "")

            elif buffer_type == ESCARGOT_MESSAGE_RELEASE_FUNCTION:
                self._release_function(data)

            elif buffer_type == ESCARGOT_MESSAGE_BREAKPOINT_HIT:
                breakpoint_data = struct.unpack(self.byte_order + self.pointer_format + self.idx_format, data[1:])

                breakpoint = self._get_breakpoint(breakpoint_data)
                self.last_breakpoint_hit = breakpoint[0]

                if breakpoint[1]:
                    breakpoint_info = "at"
                else:
                    breakpoint_info = "around"

                if breakpoint[0].active_index >= 0:
                    breakpoint_info += " breakpoint:%s%d%s" % (self.red, breakpoint[0].active_index, self.nocolor)

                result += "Stopped %s %s\n" % (breakpoint_info, breakpoint[0])

                if self.display > 0:
                    result += self.print_source(self.display, self.src_offset)

                self.prompt = True
                return DebuggerAction(DebuggerAction.TEXT, result)

            else:
                raise Exception("Unknown message")

    def set_colors(self):
        self.nocolor = '\033[0m'
        self.green = '\033[92m'
        self.red = '\033[31m'
        self.yellow = '\033[93m'
        self.green_bg = '\033[42m\033[30m'
        self.yellow_bg = '\033[43m\033[30m'
        self.blue = '\033[94m'

    def _exec_command(self, command_id):
        message = struct.pack(self.byte_order + "BB",
                              1,
                              command_id)
        self.channel.send_message(self.byte_order, message)

    def do_continue(self):
        self.prompt = False
        self._exec_command(ESCARGOT_MESSAGE_CONTINUE)

    def step(self):
        self.prompt = False
        self._exec_command(ESCARGOT_MESSAGE_STEP)

    def set_break(self, args):
        if not args:
            return "Error: Breakpoint index expected"

        if ':' in args:
            try:
                if int(args.split(':', 1)[1]) <= 0:
                    return "Error: Positive breakpoint index expected"

                return self._set_breakpoint(args, False)

            except ValueError as val_errno:
                return "Error: Positive breakpoint index expected: %s" % (val_errno)

        return self._set_breakpoint(args, False)

    def delete(self, args):
        if not args:
            return "Error: Breakpoint index expected\n" \
                   "Delete the given breakpoint, use 'delete all|active|pending' " \
                   "to clear all the given breakpoints\n "
        elif args == 'all':
            for i in self.active_breakpoint_list.values():
                breakpoint = self.active_breakpoint_list[i.active_index]
                del self.active_breakpoint_list[i.active_index]
                breakpoint.active_index = -1
                self._send_breakpoint(breakpoint)
            return ""

        try:
            breakpoint_index = int(args)
        except ValueError as val_errno:
            return "Error: Integer number expected, %s\n" % (val_errno)

        if breakpoint_index in self.active_breakpoint_list:
            breakpoint = self.active_breakpoint_list[breakpoint_index]
            del self.active_breakpoint_list[breakpoint_index]
            breakpoint.active_index = -1
            self._send_breakpoint(breakpoint)
            return "Breakpoint %d deleted\n" % (breakpoint_index)

        return "Error: Breakpoint %d not found\n" % (breakpoint_index)

    def breakpoint_list(self):
        result = ""
        if self.active_breakpoint_list:
            result += "=== %sActive breakpoints%s ===\n" % (self.green_bg, self.nocolor)
            for breakpoint in self.active_breakpoint_list.values():
                result += " %d: %s\n" % (breakpoint.active_index, breakpoint)

        if result == "":
            result = "No breakpoints\n"

        return result

    def print_source(self, line_num, offset):
        msg = ""
        last_bp = self.last_breakpoint_hit

        if not last_bp:
            return ""

        lines = last_bp.function.source
        if last_bp.function.source_name:
            msg += "Source: %s\n" % (last_bp.function.source_name)

        if line_num == 0:
            start = 0
            end = len(last_bp.function.source)
        else:
            start = max(last_bp.line - line_num, 0)
            end = min(last_bp.line + line_num - 1, len(last_bp.function.source))
            if offset:
                if start + offset < 0:
                    self.src_offset += self.src_offset_diff
                    offset += self.src_offset_diff
                elif end + offset > len(last_bp.function.source):
                    self.src_offset -= self.src_offset_diff
                    offset -= self.src_offset_diff

                start = max(start + offset, 0)
                end = min(end + offset, len(last_bp.function.source))

        for i in range(start, end):
            if i == last_bp.line - 1:
                msg += "%s%4d%s %s>%s %s\n" % (self.green, i + 1, self.nocolor, self.red, self.nocolor, lines[i])
            else:
                msg += "%s%4d%s   %s\n" % (self.green, i + 1, self.nocolor, lines[i])

        return msg

    # pylint: disable=too-many-branches,too-many-locals,too-many-statements
    def _parse_source(self, data):
        source = ""
        source_name = ""
        name = ""
        locations = []

        result = ""
        function_list = []

        while True:
            if data is None:
                return "Error: connection lost during source code receiving"

            buffer_type = ord(data[0])
            buffer_size = len(data) - 1

            logging.debug("Parser buffer type: %d, message size: %d", buffer_type, buffer_size)

            if buffer_type == ESCARGOT_MESSAGE_PARSE_DONE:
                break

            elif buffer_type == ESCARGOT_MESSAGE_PARSE_ERROR:
                logging.error("Syntax error found")
                return ""

            elif buffer_type in [ESCARGOT_MESSAGE_SOURCE_8BIT, ESCARGOT_MESSAGE_SOURCE_8BIT_END]:
                source += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_SOURCE_8BIT_END:
                    source = self.encode8(source)

            elif buffer_type in [ESCARGOT_MESSAGE_SOURCE_16BIT, ESCARGOT_MESSAGE_SOURCE_16BIT_END]:
                source += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_SOURCE_16BIT_END:
                    source = self.encode16(source)

            elif buffer_type in [ESCARGOT_MESSAGE_FILE_NAME_8BIT, ESCARGOT_MESSAGE_FILE_NAME_8BIT_END]:
                source_name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FILE_NAME_8BIT_END:
                    source_name = self.encode8(source_name)

            elif buffer_type in [ESCARGOT_MESSAGE_FILE_NAME_16BIT, ESCARGOT_MESSAGE_FILE_NAME_16BIT_END]:
                source_name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FILE_NAME_16BIT_END:
                    source_name = self.encode16(source_name)

            elif buffer_type in [ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT, ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END]:
                name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END:
                    name = self.encode8(name)

            elif buffer_type in [ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT, ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END]:
                name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END:
                    name = self.encode16(name)

            elif buffer_type == ESCARGOT_MESSAGE_BREAKPOINT_LOCATION:
                logging.debug("Breakpoint %s received", source_name)

                buffer_pos = 1
                while buffer_size > 0:
                    location = struct.unpack(self.byte_order + self.idx_format + self.idx_format,
                                             data[buffer_pos: buffer_pos + 8])
                    locations.append(location)
                    buffer_pos += 8
                    buffer_size -= 8

            elif buffer_type == ESCARGOT_MESSAGE_FUNCTION_PTR:
                byte_code_ptr = struct.unpack(self.byte_order + self.pointer_format, data[1:])
                logging.debug("Pointer %s received %d", source_name, byte_code_ptr[0])

                function_list.append(EscargotFunction(byte_code_ptr[0], source, source_name, name, locations))

                name = ""
                locations = []

            else:
                logging.error("Parser error!")
                raise Exception("Unexpected message")

            data = self.channel.get_message(True)

        # Only append these functions if parsing is successful
        for function in function_list:
            for line, breakpoint in function.lines.items():
                self.line_list.insert(line, breakpoint)
            self.function_list[function.byte_code_ptr] = function

        return result

    def _release_function(self, data):
        byte_code_ptr = struct.unpack(self.byte_order + self.pointer_format, data[1:])[0]

        function = self.function_list[byte_code_ptr]

        for line, breakpoint in function.lines.items():
            self.line_list.delete(line, breakpoint)
            if breakpoint.active_index >= 0:
                del self.active_breakpoint_list[breakpoint.active_index]

        del self.function_list[byte_code_ptr]

        message = struct.pack(self.byte_order + "BB" + self.pointer_format,
                              1 + self.pointer_size,
                              ESCARGOT_MESSAGE_FUNCTION_RELEASED,
                              byte_code_ptr)
        self.channel.send_message(self.byte_order, message)
        logging.debug("Function {0x%x} byte-code released", byte_code_ptr)

    def _send_breakpoint(self, breakpoint):
        message = struct.pack(self.byte_order + "BBB" + self.pointer_format + self.idx_format,
                              1 + 1 + self.pointer_size + 4,
                              ESCARGOT_MESSAGE_UPDATE_BREAKPOINT,
                              int(breakpoint.active_index >= 0),
                              breakpoint.function.byte_code_ptr,
                              breakpoint.offset)
        self.channel.send_message(self.byte_order, message)

    def _get_breakpoint(self, breakpoint_data):
        function = self.function_list[breakpoint_data[0]]
        offset = breakpoint_data[1]

        if offset in function.offsets:
            return (function.offsets[offset], True)

        if offset < function.first_breakpoint_offset:
            return (function.offsets[function.first_breakpoint_offset], False)

        nearest_offset = -1

        for current_offset in function.offsets:
            if current_offset <= offset and current_offset > nearest_offset:
                nearest_offset = current_offset

        return (function.offsets[nearest_offset], False)

    def _enable_breakpoint(self, breakpoint):
        if breakpoint.active_index < 0:
            self.next_breakpoint_index += 1
            self.active_breakpoint_list[self.next_breakpoint_index] = breakpoint
            breakpoint.active_index = self.next_breakpoint_index
            self._send_breakpoint(breakpoint)

        return "%sBreakpoint %d%s at %s\n" % (self.green,
                                              breakpoint.active_index,
                                              self.nocolor,
                                              breakpoint)

    def _set_breakpoint(self, string, pending):
        line = re.match("(.*):(\\d+)$", string)
        result = ""

        if line:
            source_name = line.group(1)
            new_line = int(line.group(2))

            for breakpoint in self.line_list.get(new_line):
                func_source = breakpoint.function.source_name
                if (source_name == func_source or
                        func_source.endswith("/" + source_name) or
                        func_source.endswith("\\" + source_name)):

                    result += self._enable_breakpoint(breakpoint)

        else:
            functions_to_enable = []
            for function in self.function_list.values():
                if function.name == string:
                    functions_to_enable.append(function)

            functions_to_enable.sort(key=lambda x: x.line)

            for function in functions_to_enable:
                result += self._enable_breakpoint(function.lines[function.first_breakpoint_line])

        return result
