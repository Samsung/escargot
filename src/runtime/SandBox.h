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
        String* src;
        String* sourceCode;
        ExtendedNodeLOC loc;
        StackTraceData()
            : src(String::emptyString)
            , sourceCode(String::emptyString)
            , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        {
        }
    };

    typedef Vector<std::pair<ExecutionState*, StackTraceData>, GCUtil::gc_malloc_allocator<std::pair<ExecutionState*, StackTraceData>>> StackTraceDataVector;

    struct SandBoxResult {
        Value result;
        Value error;
        Vector<StackTraceData, GCUtil::gc_malloc_allocator<StackTraceData>> stackTraceData;
        SandBoxResult()
            : result(Value::EmptyValue)
            , error(Value::EmptyValue)
        {
        }
    };

    SandBoxResult run(const std::function<Value()>& scriptRunner); // for capsule script executing with try-catch
    SandBoxResult run(Value (*runner)(ExecutionState&, void*), void* data);
    void throwException(ExecutionState& state, Value exception);

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
    StackTraceDataVector m_stackTraceData;
    Value m_exception; // To avoid accidential GC of exception value
};
}

#endif
