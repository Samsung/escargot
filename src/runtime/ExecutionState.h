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

#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "runtime/ExecutionContext.h"

namespace Escargot {

class ExecutionContext;
class Value;

class ControlFlowRecord;

typedef Vector<ControlFlowRecord*, GCUtil::gc_malloc_ignore_off_page_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    size_t m_controlFlowDepth;
    ControlFlowRecord* m_currentControlFlowRecord;
    ExecutionState* m_parent;
    ExecutionStateRareData()
    {
        m_controlFlowDepth = 0;
        m_currentControlFlowRecord = nullptr;
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

    bool hasRareData()
    {
        return (m_parent & 1) == 0;
    }

    ExecutionStateRareData* rareData()
    {
        ASSERT(hasRareData());
        return m_rareData;
    }

private:
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
