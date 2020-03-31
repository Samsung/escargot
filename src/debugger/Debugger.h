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

#ifndef __Debugger__
#define __Debugger__

#include "util/Util.h"
#include "util/Vector.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

/* WebSocket max length encoded in one byte. */
#define ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH 125
#define ESCARGOT_DEBUGGER_VERSION 1
#define ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY 10
#define ESCARGOT_DEBUGGER_IN_WAIT_MODE (nullptr)
#define ESCARGOT_DEBUGGER_IN_EVAL_MODE ((ExecutionState*)0x1)
#define ESCARGOT_DEBUGGER_ALWAYS_STOP ((ExecutionState*)0x2)
#define ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH 128

class ExecutionState;
class ByteCodeBlock;
class InterpretedCodeBlock;

class Debugger : public gc {
    friend Debugger* createDebugger(const char* options, bool* debuggerEnabled);

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
        ESCARGOT_MESSAGE_BACKTRACE_TOTAL = 30,
        ESCARGOT_MESSAGE_BACKTRACE = 31,
        ESCARGOT_MESSAGE_BACKTRACE_END = 32,
        ESCARGOT_MESSAGE_SCOPE_CHAIN = 33,
        ESCARGOT_MESSAGE_SCOPE_CHAIN_END = 34,
        ESCARGOT_MESSAGE_VARIABLE = 35,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_VARIABLE_8BIT = 36,
        ESCARGOT_MESSAGE_VARIABLE_8BIT_END = 37,
        ESCARGOT_MESSAGE_VARIABLE_16BIT = 38,
        ESCARGOT_MESSAGE_VARIABLE_16BIT_END = 39,
    };

    // Messages sent by the debugger client to Escargot
    enum {
        ESCARGOT_MESSAGE_FUNCTION_RELEASED = 0,
        ESCARGOT_MESSAGE_UPDATE_BREAKPOINT = 1,
        ESCARGOT_MESSAGE_CONTINUE = 2,
        ESCARGOT_MESSAGE_STEP = 3,
        ESCARGOT_MESSAGE_NEXT = 4,
        // These four must be in the same order.
        ESCARGOT_MESSAGE_EVAL_8BIT_START = 5,
        ESCARGOT_MESSAGE_EVAL_8BIT = 6,
        ESCARGOT_MESSAGE_EVAL_16BIT_START = 7,
        ESCARGOT_MESSAGE_EVAL_16BIT = 8,
        ESCARGOT_MESSAGE_GET_BACKTRACE = 9,
        ESCARGOT_MESSAGE_GET_SCOPE_CHAIN = 10,
        ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES = 11,
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
        ESCARGOT_VARIABLE_OBJECT = 8,
        ESCARGOT_VARIABLE_ARRAY = 9,
        ESCARGOT_VARIABLE_FUNCTION = 10,
        ESCARGOT_VARIABLE_LONG_NAME = 0x40,
        ESCARGOT_VARIABLE_LONG_VALUE = 0x80,
    };

    struct BreakpointLocation {
        BreakpointLocation(uint32_t line, uint32_t offset)
            : line(line)
            , offset(offset)
        {
        }

        uint32_t line;
        uint32_t offset;
    };

    bool enabled()
    {
        return m_enabled;
    }

    bool computeLocation(void)
    {
        return m_computeLocation;
    }

    void setComputeLocation(bool value)
    {
        m_computeLocation = value;
    }

    inline void processDisabledBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
    {
        if (m_stopState != ESCARGOT_DEBUGGER_ALWAYS_STOP && m_stopState != state) {
            m_delay = (uint8_t)(m_delay - 1);
            if (m_delay == 0) {
                processIncomingMessages(state, byteCodeBlock);
            }
        }

        if (m_stopState == ESCARGOT_DEBUGGER_ALWAYS_STOP || m_stopState == state) {
            stopAtBreakpoint(byteCodeBlock, offset, state);
        }
    }

    static inline void updateStopState(Debugger* debugger, ExecutionState* state, ExecutionState* newState)
    {
        if (debugger != nullptr && debugger->m_stopState == state) {
            // Stop at the next breakpoint if a "next" operation targets the current function
            debugger->m_stopState = newState;
        }
    }

    void sendType(uint8_t type);
    void sendSubtype(uint8_t type, uint8_t subType);
    void sendString(uint8_t type, StringView* string);
    void sendPointer(uint8_t type, const void* ptr);
    void sendFunctionInfo(InterpretedCodeBlock* codeBlock);
    void sendBreakpointLocations(std::vector<Debugger::BreakpointLocation>& locations);
    void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state);
    void releaseFunction(const void* ptr);

protected:
    Debugger()
        : m_enabled(false)
        , m_delay(ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY)
        , m_computeLocation(false)
        , m_stopState(ESCARGOT_DEBUGGER_ALWAYS_STOP)
    {
    }

    virtual bool init(const char* options) = 0;
    virtual bool send(uint8_t type, const void* buffer, size_t length) = 0;
    virtual bool receive(uint8_t* buffer, size_t& length) = 0;
    virtual void close(void) = 0;

    bool processIncomingMessages(ExecutionState* state, ByteCodeBlock* byteCodeBlock);
    bool doEval(ExecutionState* state, ByteCodeBlock* byteCodeBlock, uint8_t* buffer, size_t length);
    void getBacktrace(ExecutionState* state, uint32_t minDepth, uint32_t maxDepth, bool getTotal);
    void getScopeChain(ExecutionState* state);
    void getScopeVariables(ExecutionState* state, uint32_t index);

    bool* m_debuggerEnabled;
    bool m_enabled;

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
    };

    uint8_t m_delay;
    bool m_computeLocation;
    ExecutionState* m_stopState;
    Vector<uintptr_t, GCUtil::gc_malloc_atomic_allocator<uintptr_t>> m_releasedFunctions;
};

Debugger* createDebugger(const char* options, bool* debuggerEnabled);
}
#endif /* ESCARGOT_DEBUGGER */

#endif
