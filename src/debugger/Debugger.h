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
#define ESCARGOT_DEBUGGER_VERSION 1
#define ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY 10
#define ESCARGOT_DEBUGGER_IN_WAIT_MODE (nullptr)
#define ESCARGOT_DEBUGGER_IN_EVAL_MODE (reinterpret_cast<ExecutionState*>(0x1))
#define ESCARGOT_DEBUGGER_ALWAYS_STOP (reinterpret_cast<ExecutionState*>(0x2))
#define ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE (reinterpret_cast<ExecutionState*>(0x1))
#define ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH 128

class Context;
class Object;
class String;
class ExecutionState;
class ByteCodeBlock;
class InterpretedCodeBlock;

class Debugger : public gc {
public:
    // The following code is the sam as in EscargotPublic.h
    class WeakCodeRef;

    struct BreakpointLocation {
        BreakpointLocation(uint32_t line, uint32_t offset)
            : line(line)
            , offset(offset)
        {
        }

        uint32_t line;
        uint32_t offset;
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

    static void createDebuggerRemote(const char* options, Context* context);

    virtual void parseCompleted(String* source, String* srcName, String* error = nullptr) = 0;
    virtual void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) = 0;
    virtual void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) = 0;
    virtual void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) = 0;
    virtual void consoleOut(String* output) = 0;
    virtual String* getClientSource(String** sourceName) = 0;
    virtual bool getWaitBeforeExitClient() = 0;
    virtual bool skipSourceCode(String* srcName) const
    {
        return false;
    }

    static SavedStackTraceDataVector* saveStackTrace(ExecutionState& state);

    void pumpDebuggerEvents(ExecutionState* state);

protected:
    Debugger()
        : m_delay(ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY)
        , m_stopState(ESCARGOT_DEBUGGER_ALWAYS_STOP)
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

    void enable(Context* context);
    void disable();

    virtual bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) = 0;

    uint32_t m_delay;
    ExecutionState* m_stopState;
    std::vector<BreakpointLocationsInfo*> m_breakpointLocationsVector;

private:
    Context* m_context;
    ExecutionState* m_activeSavedStackTraceExecutionState;
    SavedStackTraceDataVector* m_activeSavedStackTrace;

    // represent that every created InterpretedCodeBlock and its ByteCode should be marked with debugging feature
    // ByteCode should contain debugging code (breakpoint)
    bool m_inDebuggingCodeMode;
};

class DebuggerRemote : public Debugger {
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
        // These four must be in the same order.
        ESCARGOT_MESSAGE_STRING_8BIT = 35,
        ESCARGOT_MESSAGE_STRING_8BIT_END = 36,
        ESCARGOT_MESSAGE_STRING_16BIT = 37,
        ESCARGOT_MESSAGE_STRING_16BIT_END = 38,
        ESCARGOT_MESSAGE_VARIABLE = 39,
        ESCARGOT_MESSAGE_PRINT = 40,
        ESCARGOT_MESSAGE_EXCEPTION = 41,
        ESCARGOT_MESSAGE_EXCEPTION_BACKTRACE = 42,
        ESCARGOT_DEBUGGER_WAIT_FOR_SOURCE = 43,
        ESCARGOT_DEBUGGER_WAITING_AFTER_PENDING = 44,
        ESCARGOT_DEBUGGER_WAIT_FOR_WAIT_EXIT = 45,
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
        ESCARGOT_MESSAGE_GET_BACKTRACE = 14,
        ESCARGOT_MESSAGE_GET_SCOPE_CHAIN = 15,
        ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES = 16,
        ESCARGOT_MESSAGE_GET_OBJECT = 17,
        // These four must be in the same order.
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT_START = 18,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT = 19,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT_START = 20,
        ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT = 21,
        ESCARGOT_DEBUGGER_THERE_WAS_NO_SOURCE = 22,
        ESCARGOT_DEBUGGER_PENDING_CONFIG = 23,
        ESCARGOT_DEBUGGER_PENDING_RESUME = 24,
        ESCARGOT_DEBUGGER_WAIT_BEFORE_EXIT = 25,
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
    void sendString(uint8_t type, String* string);
    void sendPointer(uint8_t type, const void* ptr);

    virtual void init(const char* options, Context* context) = 0;
    virtual void parseCompleted(String* source, String* srcName, String* error = nullptr) override;
    virtual void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) override;
    virtual void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) override;
    virtual void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) override;
    virtual void consoleOut(String* output) override;
    virtual String* getClientSource(String** sourceName) override;
    virtual bool getWaitBeforeExitClient() override;

    void sendBacktraceInfo(uint8_t type, ByteCodeBlock* byteCodeBlock, uint32_t line, uint32_t column, uint32_t executionStateDepth);
    void sendVariableObjectInfo(uint8_t subType, Object* object);
    void waitForResolvingPendingBreakpoints();

protected:
    DebuggerRemote()
        : m_exitClient(false)
        , m_pendingWait(false)
        , m_waitForResume(false)
        , m_clientSourceData(nullptr)
        , m_clientSourceName(nullptr)
    {
    }

    virtual bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) override;

    virtual bool send(uint8_t type, const void* buffer, size_t length) = 0;
    virtual bool receive(uint8_t* buffer, size_t& length) = 0;
    virtual bool isThereAnyEvent() = 0;
    virtual void close(void) = 0;

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

    uint32_t appendToActiveObjects(Object* object);
    bool doEval(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, uint8_t* buffer, size_t length);
    void getBacktrace(ExecutionState* state, uint32_t minDepth, uint32_t maxDepth, bool getTotal);
    void getScopeChain(ExecutionState* state, uint32_t stateIndex);
    void getScopeVariables(ExecutionState* state, uint32_t stateIndex, uint32_t index);

    bool m_exitClient : 1;
    bool m_pendingWait : 1;
    bool m_waitForResume : 1;
    String* m_clientSourceData;
    String* m_clientSourceName;

    Vector<uintptr_t, GCUtil::gc_malloc_atomic_allocator<uintptr_t>> m_releasedFunctions;
    Vector<Object*, GCUtil::gc_malloc_allocator<Object*>> m_activeObjects;
};

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
