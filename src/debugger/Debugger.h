/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#define ESCARGOT_DEBUGGER_ALWAYS_STOP ((ExecutionState*)0x1)

class ExecutionState;
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
    };

    // Messages sent by the debugger client to Escargot
    enum {
        ESCARGOT_MESSAGE_FUNCTION_RELEASED = 0,
        ESCARGOT_MESSAGE_UPDATE_BREAKPOINT = 1,
        ESCARGOT_MESSAGE_CONTINUE = 2,
        ESCARGOT_MESSAGE_STEP = 3,
        ESCARGOT_MESSAGE_NEXT = 4,
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

    inline void processDisabledBreakpoint(void* byteCodeStart, uint32_t offset, ExecutionState* state)
    {
        if (m_stopState != ESCARGOT_DEBUGGER_ALWAYS_STOP && m_stopState != state) {
            m_delay = (uint8_t)(m_delay - 1);
            if (m_delay == 0) {
                processIncomingMessages(state);
            }
        }

        if (m_stopState == ESCARGOT_DEBUGGER_ALWAYS_STOP || m_stopState == state) {
            stopAtBreakpoint(byteCodeStart, offset, state);
        }
    }

    inline void beforeReturn(ExecutionState* state)
    {
        if (m_stopState == state) {
            // Stop at the next breakpoint if a "next" operation targets the current function
            m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;
        }
    }

    void sendType(uint8_t type);
    void sendString(uint8_t type, StringView* string);
    void sendPointer(uint8_t type, const void* ptr);
    void sendFunctionInfo(InterpretedCodeBlock* codeBlock);
    void sendBreakpointLocations(std::vector<Debugger::BreakpointLocation>& locations);
    void stopAtBreakpoint(void* byteCodeStart, uint32_t offset, ExecutionState* state);
    void releaseFunction(const void* ptr);

protected:
    Debugger()
        : m_enabled(false)
        , m_delay(ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY)
        , m_stopState(ESCARGOT_DEBUGGER_ALWAYS_STOP)
    {
    }

    virtual bool init(const char* options) = 0;
    virtual bool send(uint8_t type, const void* buffer, size_t length) = 0;
    virtual bool receive(uint8_t* buffer, size_t& length) = 0;
    virtual void close(void) = 0;

    bool processIncomingMessages(ExecutionState* state);

    bool* m_debuggerEnabled;
    bool m_enabled;

private:
    // Packed structure definitions to reduce network traffic

    struct MessageVersion {
        uint8_t littleEndian;
        uint8_t version[sizeof(uint32_t)];
    };

    struct MessageConfiguration {
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

    uint8_t m_delay;
    ExecutionState* m_stopState;
    Vector<uintptr_t, GCUtil::gc_malloc_atomic_allocator<uintptr_t>> m_releasedFunctions;
};

Debugger* createDebugger(const char* options, bool* debuggerEnabled);
}
#endif /* ESCARGOT_DEBUGGER */

#endif
