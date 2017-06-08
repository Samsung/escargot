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

#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class ExecutionContext;
class Value;

class ControlFlowRecord;

typedef Vector<ControlFlowRecord*, GCUtil::gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    Vector<ControlFlowRecord*, GCUtil::gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>>* m_controlFlowRecord;
    ExecutionState* m_parent;
    ExecutionStateRareData()
    {
        m_controlFlowRecord = nullptr;
        m_parent = nullptr;
    }
};

class ExecutionState : public gc {
    friend class ByteCodeInterpreter;

public:
    ExecutionState(Context* context, ExecutionContext* executionContext = nullptr)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_registerFile(nullptr)
        , m_parent(1)
    {
        volatile int sp;
        m_stackBase = (size_t)&sp;
    }

    ExecutionState(Context* context, ExecutionState* parent, ExecutionContext* executionContext)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_stackBase(parent->stackBase())
        , m_registerFile(nullptr)
        , m_parent((size_t)parent + 1)
    {
    }

    ExecutionState(Context* context, ExecutionState* parent, ExecutionContext* executionContext, Value* registerFile)
        : m_context(context)
        , m_executionContext(executionContext)
        , m_stackBase(parent->stackBase())
        , m_registerFile(registerFile)
        , m_parent((size_t)parent + 1)
    {
    }

    ExecutionState(ExecutionState* parent, ExecutionContext* executionContext)
        : m_context(parent->context())
        , m_executionContext(executionContext)
        , m_stackBase(parent->stackBase())
        , m_registerFile(nullptr)
        , m_parent((size_t)parent + 1)
    {
    }

    Context* context()
    {
        return m_context;
    }

    ExecutionContext* executionContext()
    {
        return m_executionContext;
    }

    size_t stackBase()
    {
        return m_stackBase;
    }

    Value* registerFile()
    {
        return m_registerFile;
    }

    bool inStrictMode()
    {
        return m_executionContext->inStrictMode();
    }

    void throwException(const Value& e);

    ExecutionState* parent();
    ExecutionStateRareData* ensureRareData();

    ExecutionStateRareData* rareData()
    {
        return m_rareData;
    }

protected:
    Context* m_context;
    ExecutionContext* m_executionContext;
    size_t m_stackBase;
    Value* m_registerFile;
    union {
        size_t m_parent;
        ExecutionStateRareData* m_rareData;
    };
};
}

#endif
