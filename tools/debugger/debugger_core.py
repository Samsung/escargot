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
ESCARGOT_MESSAGE_EVAL_RESULT_8BIT = 22
ESCARGOT_MESSAGE_EVAL_RESULT_8BIT_END = 23
ESCARGOT_MESSAGE_EVAL_RESULT_16BIT = 24
ESCARGOT_MESSAGE_EVAL_RESULT_16BIT_END = 25
ESCARGOT_MESSAGE_EVAL_FAILED_8BIT = 26
ESCARGOT_MESSAGE_EVAL_FAILED_8BIT_END = 27
ESCARGOT_MESSAGE_EVAL_FAILED_16BIT = 28
ESCARGOT_MESSAGE_EVAL_FAILED_16BIT_END = 29
ESCARGOT_MESSAGE_BACKTRACE_TOTAL = 30
ESCARGOT_MESSAGE_BACKTRACE = 31
ESCARGOT_MESSAGE_BACKTRACE_END = 32
ESCARGOT_MESSAGE_SCOPE_CHAIN = 33
ESCARGOT_MESSAGE_SCOPE_CHAIN_END = 34
ESCARGOT_MESSAGE_STRING_8BIT = 35
ESCARGOT_MESSAGE_STRING_8BIT_END = 36
ESCARGOT_MESSAGE_STRING_16BIT = 37
ESCARGOT_MESSAGE_STRING_16BIT_END = 38
ESCARGOT_MESSAGE_VARIABLE = 39
ESCARGOT_MESSAGE_PRINT = 40
ESCARGOT_MESSAGE_EXCEPTION = 41
ESCARGOT_MESSAGE_EXCEPTION_BACKTRACE = 42
ESCARGOT_DEBUGGER_WAIT_FOR_SOURCE = 43
ESCARGOT_DEBUGGER_WAITING_AFTER_PENDING = 44


# Messages sent by the debugger client to Escargot.
ESCARGOT_MESSAGE_FUNCTION_RELEASED = 0
ESCARGOT_MESSAGE_UPDATE_BREAKPOINT = 1
ESCARGOT_MESSAGE_CONTINUE = 2
ESCARGOT_MESSAGE_STEP = 3
ESCARGOT_MESSAGE_NEXT = 4
ESCARGOT_MESSAGE_FINISH = 5
ESCARGOT_MESSAGE_EVAL_8BIT_START = 6
ESCARGOT_MESSAGE_EVAL_8BIT = 7
ESCARGOT_MESSAGE_EVAL_16BIT_START = 8
ESCARGOT_MESSAGE_EVAL_16BIT = 9
ESCARGOT_MESSAGE_GET_BACKTRACE = 10
ESCARGOT_MESSAGE_GET_SCOPE_CHAIN = 11
ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES = 12
ESCARGOT_MESSAGE_GET_OBJECT = 13
ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT_START = 14
ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT = 15
ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT_START = 16
ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT = 17
ESCARGOT_DEBUGGER_THERE_WAS_NO_SOURCE = 18
ESCARGOT_DEBUGGER_PENDING_CONFIG = 19
ESCARGOT_DEBUGGER_PENDING_RESUME = 20


# Environment record types
ESCARGOT_RECORD_GLOBAL_ENVIRONMENT = 0
ESCARGOT_RECORD_FUNCTION_ENVIRONMENT = 1
ESCARGOT_RECORD_DECLARATIVE_ENVIRONMENT = 2
ESCARGOT_RECORD_OBJECT_ENVIRONMENT = 3
ESCARGOT_RECORD_MODULE_ENVIRONMENT = 4
ESCARGOT_RECORD_UNKNOWN_ENVIRONMENT = 5


# Variable types
ESCARGOT_VARIABLE_END = 0
ESCARGOT_VARIABLE_UNACCESSIBLE = 1
ESCARGOT_VARIABLE_UNDEFINED = 2
ESCARGOT_VARIABLE_NULL = 3
ESCARGOT_VARIABLE_TRUE = 4
ESCARGOT_VARIABLE_FALSE = 5
ESCARGOT_VARIABLE_NUMBER = 6
ESCARGOT_VARIABLE_STRING = 7
ESCARGOT_VARIABLE_SYMBOL = 8
ESCARGOT_VARIABLE_BIGINT = 9
ESCARGOT_VARIABLE_OBJECT = 10
ESCARGOT_VARIABLE_ARRAY = 11
ESCARGOT_VARIABLE_FUNCTION = 12
ESCARGOT_VARIABLE_LONG_NAME = 0x40
ESCARGOT_VARIABLE_LONG_VALUE = 0x80


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
    parser.add_argument("--client-source", action="store", default=[], type=str, nargs="+",
                        help="specify a javascript source file to execute")
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(format="%(levelname)s: %(message)s", level=logging.DEBUG)
        logging.debug("Debug logging mode: ON")

    return args


def _parse_int(value):
    try:
        index = int(value)
        if index < 0:
             return "Error: A non negative integer number expected"
        return index

    except ValueError as val_errno:
        return "Error: Non negative integer number expected, %s\n" % (val_errno)


class Breakpoint(object):
    def __init__(self, line, offset, function):
        self.line = line
        self.offset = offset
        self.function = function
        self.active_index = -1

    def __str__(self):
        result = self.function.source_name or "<unknown>"
        result += ":%d" % (self.line)

        if self.function.is_func:
            result += " (in "
            result += self.function.name or "function"
            result += "() at line:%d, col:%d)" % (self.function.line, self.function.column)
        return result

    def __repr__(self):
        return ("Breakpoint(line:%d, offset:%d, active_index:%d)"
                % (self.line, self.offset, self.active_index))

class PendingBreakpoint(object):
    def __init__(self, line=None, source_name=None, function=None):
        self.function = function
        self.line = line
        self.source_name = source_name

        self.index = -1

    def __str__(self):
        result = self.source_name or ""
        if self.line:
            result += ":%d" % (self.line)
        else:
            result += "%s()" % (self.function)
        return result

class EscargotFunction(object):
    # pylint: disable=too-many-instance-attributes,too-many-arguments
    def __init__(self, is_func, function_info, source, source_name, name, locations):
        self.is_func = is_func
        self.byte_code_ptr = function_info[0]
        self.source = re.split("\r\n|[\r\n]", source)
        self.source_name = source_name
        self.name = name
        self.lines = {}
        self.offsets = {}
        self.line = function_info[1]
        self.column = function_info[2]
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
        self.no_scope = 4294967295
        self.client_sources = []
        self.last_breakpoint_hit = None
        self.next_breakpoint_index = 0
        self.active_breakpoint_list = {}
        self.pending_breakpoint_list = {}
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

        self.max_message_size = ord(result[1])
        self.pointer_size = ord(result[2])

        if self.pointer_size == 8:
            self.pointer_format = "Q"
        elif self.pointer_size == 4:
            self.pointer_format = "I"
        else:
            raise Exception("Unsupported pointer size: %d" % (self.pointer_size))

        self.idx_format = "I"

        logging.debug("Pointer size: %d", self.pointer_size)

    def __del__(self):
        if self.channel is not None:
            self.channel.close()

    def decode8(self, string):
        return string.decode("latin1")

    def decode16(self, string):
        return string.decode("UTF-16LE" if self.little_endian else "UTF-16BE", "namereplace")

    # pylint: disable=too-many-branches,too-many-locals,too-many-statements,too-many-return-statements
    def process_messages(self):
        result = ""
        while True:
            data = self.channel.get_message(False)
            if not self.non_interactive:
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    sys.stdin.readline()
                    self.step()

            if data == b'':
                action_type = DebuggerAction.PROMPT if self.prompt else DebuggerAction.WAIT
                return DebuggerAction(action_type, "")

            if not data:  # Break the while loop if there is no more data.
                return DebuggerAction(DebuggerAction.END, "")

            buffer_type = ord(data[0])
            buffer_size = len(data) - 1

            logging.debug("Main buffer type: %d, message size: %d", buffer_type, buffer_size)

            if buffer_type in [ESCARGOT_MESSAGE_PARSE_ERROR,
                               ESCARGOT_MESSAGE_SOURCE_8BIT,
                               ESCARGOT_MESSAGE_SOURCE_8BIT_END,
                               ESCARGOT_MESSAGE_SOURCE_16BIT,
                               ESCARGOT_MESSAGE_SOURCE_16BIT_END]:
                result = self._parse_source(data)
                if result:
                    return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == ESCARGOT_DEBUGGER_WAITING_AFTER_PENDING:
                self._exec_command(ESCARGOT_DEBUGGER_PENDING_RESUME)

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

            elif buffer_type == ESCARGOT_MESSAGE_BACKTRACE_TOTAL:
                total = struct.unpack(self.byte_order + self.idx_format, data[1:])[0]
                result += "Total number of frames: %d\n" % (total)
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type in [ESCARGOT_MESSAGE_EVAL_RESULT_8BIT,
                                 ESCARGOT_MESSAGE_EVAL_RESULT_8BIT_END,
                                 ESCARGOT_MESSAGE_EVAL_RESULT_16BIT,
                                 ESCARGOT_MESSAGE_EVAL_RESULT_16BIT_END]:
                self.prompt = True
                return DebuggerAction(DebuggerAction.TEXT, self._receive_string(ESCARGOT_MESSAGE_EVAL_RESULT_8BIT, data));

            elif buffer_type in [ESCARGOT_MESSAGE_EVAL_FAILED_8BIT,
                                 ESCARGOT_MESSAGE_EVAL_FAILED_8BIT_END,
                                 ESCARGOT_MESSAGE_EVAL_FAILED_16BIT,
                                 ESCARGOT_MESSAGE_EVAL_FAILED_16BIT_END]:
                self.prompt = True
                result = self._receive_string(ESCARGOT_MESSAGE_EVAL_FAILED_8BIT, data);
                return DebuggerAction(DebuggerAction.TEXT, "%sException: %s%s" % (self.red, result, self.nocolor));

            elif buffer_type in [ESCARGOT_MESSAGE_BACKTRACE,
                                 ESCARGOT_MESSAGE_EXCEPTION_BACKTRACE]:
                backtrace_info = struct.unpack(self.byte_order + self.pointer_format + self.idx_format + self.idx_format + self.idx_format, data[1:])
                function = self.function_list.get(backtrace_info[0])

                depth = ""
                if backtrace_info[3] != self.no_scope:
                    depth = " [depth:%d]" % backtrace_info[3]

                if function is not None:
                    result = "%s:%d:%d%s" % (function.source_name, backtrace_info[1], backtrace_info[2], depth)
                    if function.name != "":
                        result += " (in %s)" % (function.name)
                else:
                    result = "unknown dynamic function:%d:%d%s" % (backtrace_info[1], backtrace_info[2], depth)

                if buffer_type == ESCARGOT_MESSAGE_BACKTRACE:
                    return DebuggerAction(DebuggerAction.TEXT, result + "\n")
                return DebuggerAction(DebuggerAction.TEXT, "%s%s%s\n" % (self.red, result, self.nocolor))

            elif buffer_type == ESCARGOT_MESSAGE_BACKTRACE_END:
                self.prompt = True
                return DebuggerAction(DebuggerAction.WAIT, "")

            elif buffer_type in [ESCARGOT_MESSAGE_SCOPE_CHAIN,
                                 ESCARGOT_MESSAGE_SCOPE_CHAIN_END]:
                scope_chain = ""

                while buffer_type == ESCARGOT_MESSAGE_SCOPE_CHAIN:
                    scope_chain += data[1:]
                    data = self.channel.get_message(True)
                    buffer_type = ord(data[0])

                if buffer_type != ESCARGOT_MESSAGE_SCOPE_CHAIN_END:
                    raise Exception("Unexpected message")

                scope_chain += data[1:]
                result = ""

                for env_type in scope_chain:
                    env_type = ord(env_type)
                    if env_type == ESCARGOT_RECORD_GLOBAL_ENVIRONMENT:
                        result += "Global Environment\n"
                    elif env_type == ESCARGOT_RECORD_FUNCTION_ENVIRONMENT:
                        result += "Function Environment\n"
                    elif env_type == ESCARGOT_RECORD_DECLARATIVE_ENVIRONMENT:
                        result += "Declarative Environment\n"
                    elif env_type == ESCARGOT_RECORD_OBJECT_ENVIRONMENT:
                        result += "Object Environment\n"
                    elif env_type == ESCARGOT_RECORD_MODULE_ENVIRONMENT:
                        result += "Module Environment\n"
                    else:
                        result += "Unknown Environment\n"

                self.prompt = True
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == ESCARGOT_MESSAGE_VARIABLE:
                variable_full_type = ord(data[1])
                variable_type = variable_full_type & 0x3f
                variable_has_value = False

                if variable_type == ESCARGOT_VARIABLE_END:
                    self.prompt = True
                    return DebuggerAction(DebuggerAction.WAIT, "")
                elif variable_type == ESCARGOT_VARIABLE_UNACCESSIBLE:
                    value_str = "unaccessible"
                elif variable_type == ESCARGOT_VARIABLE_UNDEFINED:
                    value_str = "undefined"
                elif variable_type == ESCARGOT_VARIABLE_NULL:
                    value_str = "null"
                elif variable_type == ESCARGOT_VARIABLE_TRUE:
                    value_str = "boolean: true"
                elif variable_type == ESCARGOT_VARIABLE_FALSE:
                    value_str = "boolean: false"
                elif variable_type == ESCARGOT_VARIABLE_NUMBER:
                    value_str = "number: "
                    variable_has_value = True
                elif variable_type == ESCARGOT_VARIABLE_STRING:
                    value_str = "string: "
                    variable_has_value = True
                elif variable_type == ESCARGOT_VARIABLE_SYMBOL:
                    value_str = "symbol: "
                    variable_has_value = True
                elif variable_type == ESCARGOT_VARIABLE_BIGINT:
                    value_str = "bigint: "
                    variable_has_value = True
                elif variable_type == ESCARGOT_VARIABLE_OBJECT:
                    value_str = "object"
                elif variable_type == ESCARGOT_VARIABLE_ARRAY:
                    value_str = "array"
                elif variable_type == ESCARGOT_VARIABLE_FUNCTION:
                    value_str = "function"

                name = self._receive_string(ESCARGOT_MESSAGE_STRING_8BIT, self.channel.get_message(True));
                if variable_full_type & ESCARGOT_VARIABLE_LONG_NAME != 0:
                    name += "..."

                if variable_has_value:
                    value_str += self._receive_string(ESCARGOT_MESSAGE_STRING_8BIT, self.channel.get_message(True));
                    if variable_full_type & ESCARGOT_VARIABLE_LONG_VALUE:
                        value_str += "..."
                elif variable_type >= ESCARGOT_VARIABLE_OBJECT:
                    value_str += " [id: %d]" % (struct.unpack(self.byte_order + self.idx_format, data[2:])[0])

                return DebuggerAction(DebuggerAction.TEXT, "%s: %s\n" % (name, value_str))

            elif buffer_type == ESCARGOT_MESSAGE_PRINT:
                printMessage ="Print: %s\n" % (self._receive_string(ESCARGOT_MESSAGE_STRING_8BIT, self.channel.get_message(True)))
                return DebuggerAction(DebuggerAction.TEXT, printMessage);

            elif buffer_type == ESCARGOT_MESSAGE_EXCEPTION:
                exceptionMessage ="%sException: %s%s\n" % (self.red, self._receive_string(ESCARGOT_MESSAGE_STRING_8BIT, self.channel.get_message(True)), self.nocolor)
                return DebuggerAction(DebuggerAction.TEXT, exceptionMessage);
            elif buffer_type == ESCARGOT_DEBUGGER_WAIT_FOR_SOURCE:
                self.send_client_source()
            else:
                raise Exception("Unknown message: %d" % (buffer_type))

    def set_colors(self):
        self.nocolor = '\033[0m'
        self.green = '\033[92m'
        self.red = '\033[31m'
        self.yellow = '\033[93m'
        self.green_bg = '\033[42m\033[30m'
        self.yellow_bg = '\033[43m\033[30m'
        self.blue = '\033[94m'

    def store_client_sources(self, args):
        self.client_sources = args

    def send_client_source(self):
        if not self.client_sources:
            self._exec_command(ESCARGOT_DEBUGGER_THERE_WAS_NO_SOURCE)
            return
        path = self.client_sources.pop(0)
        if not path.endswith('.js'):
            sys.exit("Error: Javascript file expected!")
            return
        with open(path, 'r') as src_file:
            content = path + '\0'+ src_file.read()
        self._send_string(content, ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT_START)

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

    def next(self):
        self.prompt = False
        self._exec_command(ESCARGOT_MESSAGE_NEXT)

    def finish(self):
        self.prompt = False
        self._exec_command(ESCARGOT_MESSAGE_FINISH)

    def quit(self):
        self.prompt = False
        self._exec_command(ESCARGOT_MESSAGE_CONTINUE)

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
                self.delete_active()
                self.delete_pending()
                return ""
        elif args == "pending":
                self.delete_pending()
                return ""
        elif args == "active":
                self.delete_active()
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
        elif breakpoint_index in self.pending_breakpoint_list:
            del self.pending_breakpoint_list[breakpoint_index]
            if not self.pending_breakpoint_list:
                self._send_pending_config(0)
            return "Pending breakpoint %d deleted\n" % (breakpoint_index)

        return "Error: Breakpoint %d not found\n" % (breakpoint_index)

    def breakpoint_list(self):
        result = ""
        if self.active_breakpoint_list:
            result += "=== %sActive breakpoints%s ===\n" % (self.green_bg, self.nocolor)
            for breakpoint in self.active_breakpoint_list.values():
                result += " %d: %s\n" % (breakpoint.active_index, breakpoint)

        if self.pending_breakpoint_list:
            result += "=== %sPending breakpoints%s ===\n" % (self.yellow_bg, self.nocolor)
            for breakpoint in self.pending_breakpoint_list.values():
                result += " %d: %s (pending)\n" % (breakpoint.index, breakpoint)

        if not self.active_breakpoint_list and not self.pending_breakpoint_list:
            result += "No breakpoints\n"

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

    def eval(self, code):
        self._send_string(code, ESCARGOT_MESSAGE_EVAL_8BIT_START)
        self.prompt = False

    def backtrace(self, args):
        max_depth = 0
        min_depth = 0
        get_total = 0

        if args:
            args = args.split(" ")
            try:
                if "t" in args:
                    get_total = 1
                    args.remove("t")

                if len(args) >= 2:
                    min_depth = int(args[0])
                    max_depth = int(args[1])
                    if max_depth <= 0 or min_depth < 0:
                        return "Error: Positive integer number expected\n"
                    if min_depth > max_depth:
                        return "Error: Start depth needs to be lower than or equal to max depth\n"
                elif len(args) >= 1:
                    max_depth = int(args[0])
                    if max_depth <= 0:
                        return "Error: Positive integer number expected\n"

            except ValueError as val_errno:
                return "Error: Positive integer number expected, %s\n" % (val_errno)

        self.frame_index = min_depth

        message = struct.pack(self.byte_order + "BB" + self.idx_format + self.idx_format + "B",
                              1 + 4 + 4 + 1,
                              ESCARGOT_MESSAGE_GET_BACKTRACE,
                              min_depth,
                              max_depth,
                              get_total)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    def scope_chain(self, args):
        index = 0
        if args != "":
            index = _parse_int(args)

        message = struct.pack(self.byte_order + "BB" + self.idx_format,
                              1 + 4,
                              ESCARGOT_MESSAGE_GET_SCOPE_CHAIN,
                              index)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    def scope_variables(self, args):
        args = args.split(" ", 1)
        stateIndex = 0
        index = 0

        if len(args) == 1:
            if args[0] != "":
                index = _parse_int(args[0])
        else:
            stateIndex = _parse_int(args[0])
            index = _parse_int(args[1])

        message = struct.pack(self.byte_order + "BB" + self.idx_format + self.idx_format,
                              1 + 4 + 4,
                              ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES,
                              stateIndex,
                              index)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    def object(self, args):
        index = _parse_int(args)

        message = struct.pack(self.byte_order + "BB" + self.idx_format,
                              1 + 4,
                              ESCARGOT_MESSAGE_GET_OBJECT,
                              index)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    # pylint: disable=too-many-branches,too-many-locals,too-many-statements
    def _parse_source(self, data):
        source = ""
        source_name = ""
        is_func = False
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
                logging.debug("Syntax error encountered")
                error_str = self._receive_string(ESCARGOT_MESSAGE_STRING_8BIT, self.channel.get_message(True))
                return "%sSyntaxError: %s%s\n" % (self.red, error_str, self.nocolor)

            elif buffer_type in [ESCARGOT_MESSAGE_SOURCE_8BIT, ESCARGOT_MESSAGE_SOURCE_8BIT_END]:
                source += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_SOURCE_8BIT_END:
                    source = self.decode8(source)

            elif buffer_type in [ESCARGOT_MESSAGE_SOURCE_16BIT, ESCARGOT_MESSAGE_SOURCE_16BIT_END]:
                source += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_SOURCE_16BIT_END:
                    source = self.decode16(source)

            elif buffer_type in [ESCARGOT_MESSAGE_FILE_NAME_8BIT, ESCARGOT_MESSAGE_FILE_NAME_8BIT_END]:
                source_name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FILE_NAME_8BIT_END:
                    source_name = self.decode8(source_name)

            elif buffer_type in [ESCARGOT_MESSAGE_FILE_NAME_16BIT, ESCARGOT_MESSAGE_FILE_NAME_16BIT_END]:
                source_name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FILE_NAME_16BIT_END:
                    source_name = self.decode16(source_name)

            elif buffer_type in [ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT, ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END]:
                name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END:
                    name = self.decode8(name)

            elif buffer_type in [ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT, ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END]:
                name += data[1:]
                if buffer_type == ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END:
                    name = self.decode16(name)

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
                function_info = struct.unpack(self.byte_order + self.pointer_format + self.idx_format + self.idx_format, data[1:])
                logging.debug("Pointer %s received %d", source_name, function_info[0])

                function_list.append(EscargotFunction(is_func, function_info, source, source_name, name, locations))

                is_func = True
                name = ""
                locations = []

            elif buffer_type == ESCARGOT_MESSAGE_RELEASE_FUNCTION:
                self._release_function(data)

            else:
                logging.error("Parser error!")
                raise Exception("Unexpected message: %d" % (buffer_type))

            data = self.channel.get_message(True)

        # Only append these functions if parsing is successful
        for function in function_list:
            for line, breakpoint in function.lines.items():
                self.line_list.insert(line, breakpoint)
            self.function_list[function.byte_code_ptr] = function

        # Try to set the pending breakpoints
        if self.pending_breakpoint_list:
            logging.debug("Pending breakpoints available")
            bp_list = self.pending_breakpoint_list

            for breakpoint_index, breakpoint in bp_list.items():
                source_lines = 0
                for src in function_list:
                    if src.name == breakpoint.source_name:
                        source_lines = len(src.source)
                        break
                if breakpoint.line:
                    if breakpoint.line <= source_lines:
                        command = src.source_name + ":" + str(breakpoint.line)
                        set_result = self._set_breakpoint(command, True)
                        if set_result:
                            result += set_result
                            del bp_list[breakpoint_index]
                elif breakpoint.function:
                    command = breakpoint.function
                    set_result = self._set_breakpoint(command, True)
                    if set_result:
                        result += set_result
                        del bp_list[breakpoint_index]
            if not bp_list:
                 self._send_pending_config(0)
        return result

    def _release_function(self, data):
        byte_code_ptr = struct.unpack(self.byte_order + self.pointer_format, data[1:])[0]

        function = self.function_list.get(byte_code_ptr)
        if function is None:
            return

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

    def _send_pending_config(self, enable):
        message = struct.pack(self.byte_order + "BBB",
                              1 + 1,
                              ESCARGOT_DEBUGGER_PENDING_CONFIG,
                              enable)
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
        if isinstance(breakpoint, PendingBreakpoint):
            if self.breakpoint_pending_exists(breakpoint):
                return "%sPending breakpoint%s already exists\n" % (self.yellow, self.nocolor)

            self.next_breakpoint_index += 1
            breakpoint.index = self.next_breakpoint_index
            self.pending_breakpoint_list[self.next_breakpoint_index] = breakpoint
            return ("%sPending breakpoint %d%s at %s\n" % (self.yellow,
                                                           breakpoint.index,
                                                           self.nocolor,
                                                           breakpoint))
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

        if not result and not pending:
            print("No breakpoint found, do you want to add a %spending breakpoint%s? (y or [n])" % \
                  (self.yellow, self.nocolor))

            ans = sys.stdin.readline()
            if ans in ['yes\n', 'y\n']:
                if not self.pending_breakpoint_list:
                    self._send_pending_config(1)

                if line:
                    breakpoint = PendingBreakpoint(int(line.group(2)), line.group(1))
                else:
                    breakpoint = PendingBreakpoint(function=string)
                result += self._enable_breakpoint(breakpoint)

        return result

    def _send_string(self, args, message_type):
        # 1: length of type byte
        # 4: length of an uint32 value
        message_header = 1 + 4

        if not isinstance(args, unicode):
            try:
                args = args.decode("ascii")
            except UnicodeDecodeError:
                args = args.decode("utf-8")

        try:
            args = args.encode("latin1")
        except UnicodeEncodeError:
            args = args.encode("UTF-16LE" if self.little_endian else "UTF-16BE", "namereplace")
            message_type += 2

        size = len(args)
        max_fragment = min(self.max_message_size - message_header, size)

        message = struct.pack(self.byte_order + "BBI",
                              max_fragment + message_header,
                              message_type,
                              size)

        if size == max_fragment:
            self.channel.send_message(self.byte_order, message + args)
            return

        self.channel.send_message(self.byte_order, message + args[0:max_fragment])
        offset = max_fragment

        # 1: length of type byte
        message_header = 1
        message_type += 1

        max_fragment = self.max_message_size - message_header
        while offset < size:
            next_fragment = min(max_fragment, size - offset)

            message = struct.pack(self.byte_order + "BB",
                                  next_fragment + message_header,
                                  message_type)

            prev_offset = offset
            offset += next_fragment

            self.channel.send_message(self.byte_order, message + args[prev_offset:offset])

    def _receive_string(self, message_type, data):
        result = b'';
        buffer_type = ord(data[0])
        end_type = message_type + 1;

        if buffer_type > end_type:
            end_type += 2

        while buffer_type == end_type - 1:
            result += data[1:]
            data = self.channel.get_message(True)
            buffer_type = ord(data[0])

        if buffer_type != end_type:
            raise Exception("Unexpected message")

        result += data[1:]

        if end_type == message_type + 1:
            result = self.decode8(result)
        else:
            result = self.decode16(result)

        return result;

    def delete_active(self):
        for i in self.active_breakpoint_list.values():
            breakpoint = self.active_breakpoint_list[i.active_index]
            del self.active_breakpoint_list[i.active_index]
            breakpoint.active_index = -1
            self._send_breakpoint(breakpoint)

    def delete_pending(self):
        if self.pending_breakpoint_list:
            self.pending_breakpoint_list.clear()
            self._send_pending_config(0)

    def breakpoint_pending_exists(self, breakpoint):
        for existing_bp in self.pending_breakpoint_list.values():
            if (breakpoint.line and existing_bp.source_name == breakpoint.source_name and \
                existing_bp.line == breakpoint.line) \
                or (not breakpoint.line and existing_bp.function == breakpoint.function):
                    return True

        return False
