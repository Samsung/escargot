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

struct StackTraceDataOnStack : public gc {
    String* srcName;
    String* sourceCode;
    ExtendedNodeLOC loc;
    String* functionName;

#ifdef ESCARGOT_DEBUGGER
    uint32_t executionStateDepth;
#endif /* ESCARGOT_DEBUGGER */
    Optional<FunctionObject*> callee;
    bool isFunction;
    bool isConstructor;
    bool isAssociatedWithJavaScriptCode;
    bool isEval;

    StackTraceDataOnStack()
        : srcName(String::emptyString())
        , sourceCode(String::emptyString())
        , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        , functionName(String::emptyString())
#ifdef ESCARGOT_DEBUGGER
        , executionStateDepth(0)
#endif /* ESCARGOT_DEBUGGER */
        , callee(nullptr)
        , isFunction(false)
        , isConstructor(false)
        , isAssociatedWithJavaScriptCode(false)
        , isEval(false)
    {
    }
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
    static StackTraceData* create(SandBox* sandBox);
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
    void rethrowPreviouslyCaughtException(ExecutionState& state, Value exception, StackTraceDataOnStackVector&& stackTraceDataVector);

    StackTraceDataOnStackVector& stackTraceDataVector()
    {
        return m_stackTraceDataVector;
    }

    Value exception() const
    {
        return m_exception;
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
    Value m_exception; // To avoid accidential GC of exception value
};
} // namespace Escargot

#endif
