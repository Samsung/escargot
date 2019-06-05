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

namespace Escargot {

class Context;
class ControlFlowRecord;
class LexicalEnvironment;
class EnvironmentRecord;
class Value;

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
    friend class FunctionObject;
    friend class ByteCodeInterpreter;
    friend class SandBox;
    friend class Script;

public:
    ExecutionState(Context* context)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_generatorTarget(nullptr)
        , m_registerFile(nullptr)
        , m_parent(1)
        , m_inStrictMode(false)
        , m_onGoingClassConstruction(false)
        , m_onGoingSuperCall(false)
    {
        volatile int sp;
        m_stackBase = (size_t)&sp;
    }

    ExecutionState(ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode, Value* registerFile = nullptr)
        : m_context(parent->context())
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_generatorTarget(nullptr)
        , m_stackBase(parent->stackBase())
        , m_registerFile(registerFile)
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_onGoingClassConstruction(false)
        , m_onGoingSuperCall(false)
    {
    }

    ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode, Value* registerFile = nullptr)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_generatorTarget(nullptr)
        , m_stackBase(parent->stackBase())
        , m_registerFile(registerFile)
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_onGoingClassConstruction(false)
        , m_onGoingSuperCall(false)
    {
    }

    Context* context()
    {
        return m_context;
    }

    size_t stackBase()
    {
        return m_stackBase;
    }

    Value* registerFile()
    {
        return m_registerFile;
    }

    void throwException(const Value& e);

    ExecutionState* parent();
    ExecutionStateRareData* ensureRareData();

    ExecutionStateRareData* rareData()
    {
        return m_rareData;
    }

    LexicalEnvironment* lexicalEnvironment()
    {
        return m_lexicalEnvironment;
    }

    Object* generatorTarget()
    {
        return m_generatorTarget;
    }

    void setGeneratorTarget(Object* target)
    {
        ASSERT(target != nullptr);
        m_generatorTarget = target;
    }

    bool inStrictMode()
    {
        return m_inStrictMode;
    }

    bool isOnGoingClassConstruction()
    {
        return m_onGoingClassConstruction;
    }

    bool isOnGoingSuperCall()
    {
        return m_onGoingSuperCall;
    }

    void setParent(ExecutionState* parent)
    {
        ASSERT(m_parent == 1);
        m_parent = (size_t)parent + 1;
    }

    void setLexicalEnvironment(LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
    {
        ASSERT(m_lexicalEnvironment == nullptr && m_inStrictMode == false);
        m_lexicalEnvironment = lexicalEnvironment;
        m_inStrictMode = inStrictMode;
    }

    void setOnGoingClassConstruction(bool startClassConstruction)
    {
        m_onGoingClassConstruction = startClassConstruction;
    }

    void setOnGoingSuperCall(bool onGoingSuperCall)
    {
        m_onGoingSuperCall = onGoingSuperCall;
    }

    FunctionObject* resolveCallee();
    Value getNewTarget();
    EnvironmentRecord* getThisEnvironment();
    Value makeSuperPropertyReference(ExecutionState& state);
    Value getSuperConstructor(ExecutionState& state);

private:
    Context* m_context;
    LexicalEnvironment* m_lexicalEnvironment;
    Object* m_generatorTarget;
    size_t m_stackBase;
    Value* m_registerFile;
    union {
        size_t m_parent;
        ExecutionStateRareData* m_rareData;
    };

    bool m_inStrictMode : 1;
    bool m_onGoingClassConstruction : 1;
    bool m_onGoingSuperCall : 1;
};
}

#endif
