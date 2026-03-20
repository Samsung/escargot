/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __DebuggerEscargot__
#define __DebuggerEscargot__

#include "Debugger.h"
#include "DebuggerTcp.h"

#ifdef ESCARGOT_DEBUGGER

#define ESCARGOT_DEBUGGER_VERSION 1
#define ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH 128

namespace Escargot {

class Context;
class Object;
class String;
class ExecutionState;
class ByteCodeBlock;
class InterpretedCodeBlock;

class DebuggerEscargot : public DebuggerTcp {
public:
    // Messages sent by Escargot to the debugger client
    enum {
        ESCARGOT_MESSAGE_VERSION = 0,
        ESCARGOT_MESSAGE_CONFIGURATION = 1,
        ESCARGOT_MESSAGE_CLOSE_CONNECTION = 2,
        ESCARGOT_MESSAGE_RELEASE_FUNCTION = 3,
        ESCARGOT_MESSAGE_PARSE_DONE = 4,
        ESCARGOT_MESSAGE_PARSE_ERROR = 5,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_SOURCE_8BIT = 6,
        ESCARGOT_MESSAGE_SOURCE_8BIT_END = 7,
        ESCARGOT_MESSAGE_SOURCE_16BIT = 8,
        ESCARGOT_MESSAGE_SOURCE_16BIT_END = 9,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_FILE_NAME_8BIT = 10,
        ESCARGOT_MESSAGE_FILE_NAME_8BIT_END = 11,
        ESCARGOT_MESSAGE_FILE_NAME_16BIT = 12,
        ESCARGOT_MESSAGE_FILE_NAME_16BIT_END = 13,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT = 14,
        ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT_END = 15,
        ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT = 16,
        ESCARGOT_MESSAGE_FUNCTION_NAME_16BIT_END = 17,
        ESCARGOT_MESSAGE_BREAKPOINT_LOCATION = 18,
        ESCARGOT_MESSAGE_FUNCTION_PTR = 19,
        ESCARGOT_MESSAGE_BREAKPOINT_HIT = 20,
        ESCARGOT_MESSAGE_EXCEPTION_HIT = 21,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_EVAL_RESULT_8BIT = 22,
        ESCARGOT_MESSAGE_EVAL_RESULT_8BIT_END = 23,
        ESCARGOT_MESSAGE_EVAL_RESULT_16BIT = 24,
        ESCARGOT_MESSAGE_EVAL_RESULT_16BIT_END = 25,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_EVAL_FAILED_8BIT = 26,
        ESCARGOT_MESSAGE_EVAL_FAILED_8BIT_END = 27,
        ESCARGOT_MESSAGE_EVAL_FAILED_16BIT = 28,
        ESCARGOT_MESSAGE_EVAL_FAILED_16BIT_END = 29,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_WATCH_RESULT_8BIT = 30,
        ESCARGOT_MESSAGE_WATCH_RESULT_8BIT_END = 31,
        ESCARGOT_MESSAGE_WATCH_RESULT_16BIT = 32,
        ESCARGOT_MESSAGE_WATCH_RESULT_16BIT_END = 33,
        ESCARGOT_MESSAGE_BACKTRACE_TOTAL = 34,
        ESCARGOT_MESSAGE_BACKTRACE = 35,
        ESCARGOT_MESSAGE_BACKTRACE_END = 36,
        ESCARGOT_MESSAGE_SCOPE_CHAIN = 37,
        ESCARGOT_MESSAGE_SCOPE_CHAIN_END = 38,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_STRING_8BIT = 39,
        ESCARGOT_MESSAGE_STRING_8BIT_END = 40,
        ESCARGOT_MESSAGE_STRING_16BIT = 41,
        ESCARGOT_MESSAGE_STRING_16BIT_END = 42,
        ESCARGOT_MESSAGE_VARIABLE = 43,
        ESCARGOT_MESSAGE_PRINT = 44,
        ESCARGOT_MESSAGE_EXCEPTION = 45,
        ESCARGOT_MESSAGE_EXCEPTION_BACKTRACE = 46,
        ESCARGOT_DEBUGGER_WAIT_FOR_SOURCE = 47,
        ESCARGOT_DEBUGGER_WAITING_AFTER_PENDING = 48,
        ESCARGOT_DEBUGGER_WAIT_FOR_WAIT_EXIT = 49
    };

    // Messages sent by the debugger client to Escargot
    enum {
        ESCARGOT_MESSAGE_FUNCTION_RELEASED = 0,
        ESCARGOT_MESSAGE_UPDATE_BREAKPOINT = 1,
        ESCARGOT_MESSAGE_CONTINUE = 2,
        ESCARGOT_MESSAGE_STEP = 3,
        ESCARGOT_MESSAGE_NEXT = 4,
        ESCARGOT_MESSAGE_FINISH = 5,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_EVAL_8BIT_START = 6,
        ESCARGOT_MESSAGE_EVAL_8BIT = 7,
        ESCARGOT_MESSAGE_EVAL_16BIT_START = 8,
        ESCARGOT_MESSAGE_EVAL_16BIT = 9,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_8BIT_START = 10,
        ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_8BIT = 11,
        ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_16BIT_START = 12,
        ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_16BIT = 13,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_WATCH_8BIT_START = 14,
        ESCARGOT_MESSAGE_WATCH_8BIT = 15,
        ESCARGOT_MESSAGE_WATCH_16BIT_START = 16,
        ESCARGOT_MESSAGE_WATCH_16BIT = 17,
        ESCARGOT_MESSAGE_GET_BACKTRACE = 18,
        ESCARGOT_MESSAGE_GET_SCOPE_CHAIN = 19,
        ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES = 20,
        ESCARGOT_MESSAGE_GET_OBJECT = 21,
        // These four must be in the same order.
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT_START = 22,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT = 23,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT_START = 24,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT = 25,
        ESCARGOT_DEBUGGER_THERE_WAS_NO_SOURCE = 26,
        ESCARGOT_DEBUGGER_PENDING_CONFIG = 27,
        ESCARGOT_DEBUGGER_PENDING_RESUME = 28,
        ESCARGOT_DEBUGGER_WAIT_BEFORE_EXIT = 29,
        ESCARGOT_DEBUGGER_STOP = 30
    };


    // Environment record types
    enum {
        ESCARGOT_RECORD_GLOBAL_ENVIRONMENT = 0,
        ESCARGOT_RECORD_FUNCTION_ENVIRONMENT = 1,
        ESCARGOT_RECORD_DECLARATIVE_ENVIRONMENT = 2,
        ESCARGOT_RECORD_OBJECT_ENVIRONMENT = 3,
        ESCARGOT_RECORD_MODULE_ENVIRONMENT = 4,
        ESCARGOT_RECORD_UNKNOWN_ENVIRONMENT = 5,
    };

    // Variable types
    enum {
        ESCARGOT_VARIABLE_END = 0,
        ESCARGOT_VARIABLE_UNACCESSIBLE = 1,
        ESCARGOT_VARIABLE_UNDEFINED = 2,
        ESCARGOT_VARIABLE_NULL = 3,
        ESCARGOT_VARIABLE_TRUE = 4,
        ESCARGOT_VARIABLE_FALSE = 5,
        ESCARGOT_VARIABLE_NUMBER = 6,
        ESCARGOT_VARIABLE_STRING = 7,
        ESCARGOT_VARIABLE_SYMBOL = 8,
        ESCARGOT_VARIABLE_BIGINT = 9,
        // Only object types should be defined after this point.
        ESCARGOT_VARIABLE_OBJECT = 10,
        ESCARGOT_VARIABLE_ARRAY = 11,
        ESCARGOT_VARIABLE_FUNCTION = 12,
        ESCARGOT_VARIABLE_TYPE_MASK = 0x3f,
        ESCARGOT_VARIABLE_LONG_NAME = 0x40,
        ESCARGOT_VARIABLE_LONG_VALUE = 0x80,
    };

    inline bool pendingWait(void)
    {
        return m_pendingWait;
    }

    inline bool connected(void)
    {
        return enabled();
    }

    void sendType(uint8_t type);
    void sendSubtype(uint8_t type, uint8_t subType);
    void sendString(uint8_t type, const String* string);
    void sendPointer(uint8_t type, const void* ptr);

    void init(const char* options, Context* context) override;
    void parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error = nullptr) override;
    void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) override;
    void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) override;
    void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) override;
    void consoleOut(String* output) override;
    String* getClientSource(String** sourceName) override;
    bool getWaitBeforeExitClient() override;

    void sendBacktraceInfo(uint8_t type, ByteCodeBlock* byteCodeBlock, uint32_t line, uint32_t column, uint32_t executionStateDepth);
    void sendVariableObjectInfo(uint8_t subType, Object* object);
    void waitForResolvingPendingBreakpoints();

    DebuggerEscargot(EscargotSocket socket, String* skipSource)
        : DebuggerTcp(socket, skipSource, ESCARGOT_WS_HEADER_BASE_SIZE + ESCARGOT_WS_MASK_SIZE + ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH, ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME)
        , m_exitClient(false)
        , m_pendingWait(false)
        , m_waitForResume(false)
        , m_watchEval(false)
        , m_clientSourceData(nullptr)
        , m_clientSourceName(nullptr)
    {
    }

    bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) override;

private:
    // Packed structure definitions to reduce network traffic

    struct MessageVersion {
        uint8_t littleEndian;
        uint8_t version[sizeof(uint32_t)];
    };

    struct MessageConfiguration {
        uint8_t maxMessageSize;
        uint8_t pointerSize;
    };

    struct FunctionInfo {
        uint8_t byteCodeStart[sizeof(void*)];
        uint8_t startLine[sizeof(uint32_t)];
        uint8_t startColumn[sizeof(uint32_t)];
    };

    struct BreakpointOffset {
        uint8_t byteCodeStart[sizeof(void*)];
        uint8_t offset[sizeof(uint32_t)];
    };

    struct BacktraceInfo {
        uint8_t byteCode[sizeof(void*)];
        uint8_t line[sizeof(uint32_t)];
        uint8_t column[sizeof(uint32_t)];
        uint8_t executionStateDepth[sizeof(uint32_t)];
    };

    struct VariableObjectInfo {
        uint8_t subType;
        uint8_t index[sizeof(uint32_t)];
    };

    bool doEval(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, uint8_t* buffer, size_t length);
    void getBacktrace(ExecutionState* state, uint32_t minDepth, uint32_t maxDepth, bool getTotal);
    void getScopeChain(ExecutionState* state, uint32_t stateIndex);
    void getScopeVariables(ExecutionState* state, uint32_t stateIndex, uint32_t index);

    bool m_exitClient : 1;
    bool m_pendingWait : 1;
    bool m_waitForResume : 1;
    bool m_watchEval : 1;
    String* m_clientSourceData;
    String* m_clientSourceName;
};

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
