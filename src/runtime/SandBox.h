/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotSandBox__
#define __EscargotSandBox__

#include "parser/ast/Node.h"
#include "runtime/Context.h"

namespace Escargot {

class SandBox {
    MAKE_STACK_ALLOCATED();
    friend class ByteCodeInterpreter;

public:
    SandBox(Context* s)
    {
        m_context = s;
        m_context->m_sandBoxStack.pushBack(this);
    }

    ~SandBox()
    {
        ASSERT(m_context->m_sandBoxStack.back() == this);
        m_context->m_sandBoxStack.pop_back();
    }

    struct StackTraceData : public gc {
        String* fileName;
        ExtendedNodeLOC loc;
        StackTraceData()
            : fileName(String::emptyString)
            , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        {
        }
    };

    typedef Vector<std::pair<ExecutionContext*, StackTraceData>, GCUtil::gc_malloc_allocator<std::pair<ExecutionContext*, StackTraceData>>> StackTraceDataVector;

    struct SandBoxResult {
        Value result;
        Value error;
        String* msgStr;
        Vector<StackTraceData, GCUtil::gc_malloc_allocator<StackTraceData>> stackTraceData;
        SandBoxResult()
            : result(Value::EmptyValue)
            , error(Value::EmptyValue)
            , msgStr(String::emptyString)
        {
        }
    };

    SandBoxResult run(const std::function<Value()>& scriptRunner); // for capsule script executing with try-catch
    void throwException(ExecutionState& state, Value exception);

protected:
    Context* m_context;
    StackTraceDataVector m_stackTraceData;
    Value m_exception; // To avoid accidential GC of exception value
};
}

#endif
