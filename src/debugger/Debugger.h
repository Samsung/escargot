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

#include "util/Vector.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

#define ESCARGOT_DEBUGGER_MAX_STACK_TRACE_LENGTH 8

/* WebSocket max length encoded in one byte. */
#define ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH 125
#define ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY 10
#define ESCARGOT_DEBUGGER_IN_WAIT_MODE (nullptr)
#define ESCARGOT_DEBUGGER_IN_EVAL_MODE (reinterpret_cast<ExecutionState*>(0x1))
#define ESCARGOT_DEBUGGER_ALWAYS_STOP (reinterpret_cast<ExecutionState*>(0x2))
#define ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE (reinterpret_cast<ExecutionState*>(0x1))

#define ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT 0x80
#define ESCARGOT_DEBUGGER_WEBSOCKET_TEXT_FRAME 1
#define ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME 2
#define ESCARGOT_DEBUGGER_WEBSOCKET_CLOSE_FRAME 8
#define ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK 0x0f
#define ESCARGOT_DEBUGGER_WEBSOCKET_LENGTH_MASK 0x7f
#define ESCARGOT_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX 125
#define ESCARGOT_DEBUGGER_WEBSOCKET_MASK_BIT 0x80

class Context;
class Object;
class String;
class ExecutionState;
class ByteCodeBlock;
class InterpretedCodeBlock;

class Debugger : public gc {
public:
    // The following code is the same as in EscargotPublic.h
    class WeakCodeRef;

    struct BreakpointLocation {
        BreakpointLocation(uint32_t line, uint32_t offset)
            : line(line)
            , offset(offset)
        {
        }

        uint32_t line; // source code line
        uint32_t offset; // bytecode offset
    };

    typedef std::vector<BreakpointLocation> BreakpointLocationVector;

    struct BreakpointLocationsInfo {
        BreakpointLocationsInfo(WeakCodeRef* weakCodeRef)
            : weakCodeRef(weakCodeRef)
        {
        }

        // The codeRef is a unique id which is not garbage collected
        // to avoid keeping script / function code in the memory.
        WeakCodeRef* weakCodeRef;
        BreakpointLocationVector breakpointLocations;
    };

    // End of the code from EscargotPublic.h

    struct SavedStackTraceData : public gc {
        ByteCodeBlock* byteCodeBlock;
        uint32_t line;
        uint32_t column;

        SavedStackTraceData(ByteCodeBlock* byteCodeBlock, uint32_t line, uint32_t column)
            : byteCodeBlock(byteCodeBlock)
            , line(line)
            , column(column)
        {
        }
    };

    typedef Vector<SavedStackTraceData, GCUtil::gc_malloc_allocator<SavedStackTraceData>> SavedStackTraceDataVector;

    bool inDebuggingCodeMode() const
    {
        return m_inDebuggingCodeMode;
    }

    void setInDebuggingCodeMode(bool mode)
    {
        m_inDebuggingCodeMode = mode;
    }

    void appendBreakpointLocations(BreakpointLocationsInfo* breakpointLocations)
    {
        m_breakpointLocationsVector.push_back(breakpointLocations);
    }

    void clearParsingData()
    {
        for (size_t i = 0; i < m_breakpointLocationsVector.size(); i++) {
            // delete each shared Debugger::BreakpointLocationsInfo here
            delete m_breakpointLocationsVector[i];
        }
        m_breakpointLocationsVector.clear();
    }

    void setActiveSavedStackTrace(ExecutionState* state, SavedStackTraceDataVector* trace)
    {
        m_activeSavedStackTraceExecutionState = state;
        m_activeSavedStackTrace = trace;
    }

    ExecutionState* activeSavedStackTraceExecutionState()
    {
        return m_activeSavedStackTraceExecutionState;
    }

    SavedStackTraceDataVector* activeSavedStackTrace()
    {
        return m_activeSavedStackTrace;
    }

    inline void processDisabledBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
    {
        if (m_stopState != ESCARGOT_DEBUGGER_ALWAYS_STOP && m_stopState != state) {
            m_delay--;
            if (m_delay == 0) {
                processEvents(state, byteCodeBlock);
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

    void setStopState(ExecutionState* stopState)
    {
        m_stopState = stopState;
    }

    virtual void init(const char* options, Context* context) = 0;
    virtual void parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error = nullptr) = 0;
    virtual void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) = 0;
    virtual void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) = 0;
    virtual void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) = 0;
    virtual void consoleOut(String* output) = 0;
    virtual String* getClientSource(String** sourceName) = 0;
    virtual bool getWaitBeforeExitClient() = 0;
    virtual void deleteClient() {}
    virtual bool skipSourceCode(String* srcName) const
    {
        return false;
    }

    static SavedStackTraceDataVector* saveStackTrace(ExecutionState& state);

    void pumpDebuggerEvents(ExecutionState* state);
    static void createDebugger(const char* options, Context* context);

    void enable(Context* context);

    Vector<Object*, GCUtil::gc_malloc_allocator<Object*>> m_activeObjects;

protected:
    Debugger()
        : m_delay(ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY)
        , m_stopState(nullptr)
        , m_context(nullptr)
        , m_activeSavedStackTraceExecutionState(nullptr)
        , m_activeSavedStackTrace(nullptr)
        , m_inDebuggingCodeMode(false)
    {
    }

    bool enabled()
    {
        return m_context != nullptr;
    }

    void disable();

    virtual bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) = 0;

    uint32_t m_delay;
    ExecutionState* m_stopState;
    std::vector<BreakpointLocationsInfo*> m_breakpointLocationsVector;
    Vector<uintptr_t, GCUtil::gc_malloc_atomic_allocator<uintptr_t>> m_releasedFunctions;

private:
    Context* m_context;
    ExecutionState* m_activeSavedStackTraceExecutionState;
    SavedStackTraceDataVector* m_activeSavedStackTrace;

    // represent that every created InterpretedCodeBlock and its ByteCode should be marked with debugging feature
    // ByteCode should contain debugging code (breakpoint)
    bool m_inDebuggingCodeMode;
};

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
