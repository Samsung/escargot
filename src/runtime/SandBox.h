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

#ifndef __EscargotSandBox__
#define __EscargotSandBox__

#include "runtime/Context.h"

namespace Escargot {

struct ExtendedNodeLOC;
class ScriptSource;

struct StackTraceDataOnStack : public gc {
    String* srcName;
    String* sourceCode;
    Optional<ScriptSource*> scriptSource;
    ExtendedNodeLOC loc;
    String* functionName;

#ifdef ESCARGOT_DEBUGGER
    uint32_t executionStateDepth;
    ExecutionState* executionState;
    LexicalEnvironment* lexicalEnvironment;
#endif /* ESCARGOT_DEBUGGER */
    Optional<FunctionObject*> callee;
    bool isFunction;
    bool isConstructor;
    bool isAssociatedWithJavaScriptCode;
    bool isEval;

    StackTraceDataOnStack()
        : srcName(String::emptyString())
        , sourceCode(String::emptyString())
        , scriptSource(nullptr)
        , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        , functionName(String::emptyString())
#ifdef ESCARGOT_DEBUGGER
        , executionStateDepth(0)
        , executionState(nullptr)
        , lexicalEnvironment(nullptr)
#endif /* ESCARGOT_DEBUGGER */
        , callee(nullptr)
        , isFunction(false)
        , isConstructor(false)
        , isAssociatedWithJavaScriptCode(false)
        , isEval(false)
    {
    }

    String* materializedSourceCode() const;
};

typedef Vector<StackTraceDataOnStack, GCUtil::gc_malloc_allocator<StackTraceDataOnStack>> StackTraceDataOnStackVector;

struct StackTraceGCData {
    union {
        ByteCodeBlock* byteCodeBlock;
        String* infoString;
    };
};

struct StackTraceNonGCData {
    size_t byteCodePosition;
};

struct StackTraceData : public gc {
    TightVector<StackTraceGCData, GCUtil::gc_malloc_allocator<StackTraceGCData>> gcValues;
    TightVector<StackTraceNonGCData, GCUtil::gc_malloc_atomic_allocator<StackTraceNonGCData>> nonGCValues;
    Value exception;

    void buildStackTrace(Context* context, StringBuilder& builder);
    static StackTraceData* create(SandBox* sandBox, const Value& exception);
    static StackTraceData* create(StackTraceDataOnStackVector& data);

private:
    StackTraceData() {}
};

class SandBox : public gc {
    friend class InterpreterSlowPath;
    friend class ErrorObject;

public:
    explicit SandBox(Context* s);
    ~SandBox();

    struct SandBoxResult {
        Value result;
        Value error;
        StackTraceDataOnStackVector stackTrace;

        SandBoxResult()
            : result(Value::EmptyValue)
            , error(Value::EmptyValue)
        {
        }
    };

    SandBoxResult run(Value (*runner)(ExecutionState&, void*), void* data);
    SandBoxResult run(ExecutionState& parentState, Value (*runner)(ExecutionState&, void*), void* data);

    static bool createStackTrace(StackTraceDataOnStackVector& stackTraceDataVector, ExecutionState& state, bool stopAtPause = false);

    void throwException(ExecutionState& state, const Value& exception);
    // Throws `exception` again from a try/finally frame that caught it, restoring the trace of the
    // original throw (and the StackTraceData already built from it, if any) instead of recording
    // the rethrow site -- the only trace recorded fresh is for a frame that has nothing saved.
    void rethrowPreviouslyCaughtException(ExecutionState& state, Value exception, StackTraceDataOnStackVector&& stackTraceDataVector, Optional<StackTraceData*> materializedStackTraceData);

    StackTraceDataOnStackVector& stackTraceDataVector()
    {
        return m_stackTraceDataVector;
    }

    // The StackTraceData built from the current trace for `exception`, if one has been attached
    // to it already; a frame that saves the trace for a later rethrow saves this with it.
    Optional<StackTraceData*> materializedStackTraceData(const Value& exception)
    {
        if (m_materializedStackTraceData && m_materializedStackTraceData->exception == exception) {
            return m_materializedStackTraceData;
        }
        return NullOption;
    }

    Context* context() const
    {
        return m_context;
    }

protected:
    void processCatch(const Value& error, SandBoxResult& result);
    void fillStackDataIntoErrorObject(const Value& e);

private:
    Context* m_context;
    SandBox* m_oldSandBox;
    StackTraceDataOnStackVector m_stackTraceDataVector;
    // The StackTraceData most recently built from m_stackTraceDataVector and attached to the
    // ErrorObject it names. An exception unwinding through nested try/finally blocks is caught
    // and rethrown once per block and every catch re-attaches the trace; reusing this keeps that
    // from rebuilding a copy of the whole trace per block. Valid only for the current vector:
    // reset whenever the vector is rebuilt.
    Optional<StackTraceData*> m_materializedStackTraceData;
};
} // namespace Escargot

#endif
