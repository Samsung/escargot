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

#include "parser/ast/Node.h"
#include "runtime/Context.h"

namespace Escargot {

class SandBox : public gc {
    friend class ByteCodeInterpreter;
    friend class ErrorObject;

public:
    explicit SandBox(Context* s);
    ~SandBox();

    struct StackTraceData : public gc {
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

        StackTraceData()
            : srcName(String::emptyString)
            , sourceCode(String::emptyString)
            , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
            , functionName(String::emptyString)
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

    typedef Vector<StackTraceData, GCUtil::gc_malloc_allocator<StackTraceData>> StackTraceDataVector;

    struct SandBoxResult {
        Value result;
        Value error;
        StackTraceDataVector stackTrace;

        SandBoxResult()
            : result(Value::EmptyValue)
            , error(Value::EmptyValue)
        {
        }
    };

    SandBoxResult run(const std::function<Value()>& scriptRunner); // for capsule script executing with try-catch
    SandBoxResult run(Value (*runner)(ExecutionState&, void*), void* data);

    static bool createStackTrace(StackTraceDataVector& stackTraceDataVector, ExecutionState& state, bool stopAtPause = false);

    void throwException(ExecutionState& state, Value exception);
    void rethrowPreviouslyCaughtException(ExecutionState& state, Value exception, const StackTraceDataVector& stackTraceDataVector);

    StackTraceDataVector& stackTraceDataVector()
    {
        return m_stackTraceDataVector;
    }

    Context* context()
    {
        return m_context;
    }

protected:
    void processCatch(const Value& error, SandBoxResult& result);
    void fillStackDataIntoErrorObject(const Value& e);

private:
    Context* m_context;
    SandBox* m_oldSandBox;
    StackTraceDataVector m_stackTraceDataVector;
    Value m_exception; // To avoid accidential GC of exception value
};
} // namespace Escargot

#endif
