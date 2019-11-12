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
class CodeBlock;
class NativeFunctionObject;
class GeneratorObject;
class ExecutionPauser;

typedef Vector<ControlFlowRecord*, GCUtil::gc_malloc_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    ControlFlowRecordVector* m_controlFlowRecord;
    ExecutionState* m_parent;
    CodeBlock* m_codeBlock;
    ExecutionPauser* m_pauseSource;
    size_t m_programCounterWhenItStoppedByYield;

    ExecutionStateRareData()
    {
        m_codeBlock = nullptr;
        m_controlFlowRecord = nullptr;
        m_pauseSource = nullptr;
        m_parent = nullptr;
        m_programCounterWhenItStoppedByYield = SIZE_MAX;
    }
};

class ExecutionState : public gc {
    friend class FunctionObject;
    friend class ByteCodeInterpreter;
    friend class ExecutionStateProgramCounterBinder;
    friend class FunctionObjectProcessCallGenerator;
    friend class SandBox;
    friend class Script;
    friend class GeneratorObject;

public:
    ExecutionState(Context* context)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_programCounter(nullptr)
        , m_argc(0)
        , m_argv(nullptr)
        , m_parent(1)
        , m_inStrictMode(false)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
    {
        volatile int sp;
        m_stackBase = (size_t)&sp;
    }

    ExecutionState(Context* context, size_t stackBase)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_stackBase(stackBase)
        , m_programCounter(nullptr)
        , m_argc(0)
        , m_argv(nullptr)
        , m_parent(1)
        , m_inStrictMode(false)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
    {
    }

    ALWAYS_INLINE ExecutionState(ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
        : m_context(parent->context())
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackBase(parent->stackBase())
        , m_programCounter(nullptr)
        , m_argc(parent->argc())
        , m_argv(parent->argv())
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackBase(parent->stackBase())
        , m_programCounter(nullptr)
        , m_argc(argc)
        , m_argv(argv)
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, NativeFunctionObject* callee, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_stackBase(parent->stackBase())
        , m_calledNativeFunctionObject(callee)
        , m_argc(argc)
        , m_argv(argv)
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(true)
    {
    }

    enum ForGenerator {
        OnlyForGenerator
    };
    ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode, ForGenerator)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackBase(0)
        , m_programCounter(nullptr)
        , m_argc(argc)
        , m_argv(argv)
        , m_parent((size_t)parent + 1)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
    {
    }

    Context* context()
    {
        return m_context;
    }

    LexicalEnvironment* lexicalEnvironment()
    {
        return m_lexicalEnvironment;
    }

    void setLexicalEnvironment(LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
    {
        m_lexicalEnvironment = lexicalEnvironment;
        m_inStrictMode = inStrictMode;
    }

    size_t stackBase()
    {
        return m_stackBase;
    }

    void throwException(const Value& e);

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

    ExecutionState* parent()
    {
        if (!hasRareData()) {
            return (ExecutionState*)(m_parent - 1);
        }
        return rareData()->m_parent;
    }

    ExecutionPauser* pauseSource()
    {
        return rareData()->m_pauseSource;
    }

    void setPauseSource(ExecutionPauser* pauseSource)
    {
        ensureRareData()->m_pauseSource = pauseSource;
    }

    void setParent(ExecutionState* parent)
    {
        if (parent) {
            m_stackBase = parent->stackBase();
        }
        if (hasRareData()) {
            rareData()->m_parent = parent;
        } else {
            ASSERT(m_parent & 1);
            m_parent = (size_t)parent + 1;
        }
    }

    bool inStrictMode()
    {
        return m_inStrictMode;
    }

    // callee is pauser && isNotInEvalCode
    bool inPauserScope();

    FunctionObject* resolveCallee();

    bool isLocalEvalCode()
    {
        // evalcode has codeBlock now.
        return codeBlock() != nullptr;
    }

    CodeBlock* codeBlock()
    {
        if (hasRareData()) {
            return rareData()->m_codeBlock;
        }
        return nullptr;
    }

    size_t argc()
    {
        return m_argc;
    }

    Value* argv()
    {
        return m_argv;
    }

    LexicalEnvironment* mostNearestHeapAllocatedLexicalEnvironment();

    // http://www.ecma-international.org/ecma-262/6.0/#sec-getnewtarget
    Object* getNewTarget();
    // http://www.ecma-international.org/ecma-262/6.0/#sec-getthisenvironment
    EnvironmentRecord* getThisEnvironment();
    Value makeSuperPropertyReference();
    Value getSuperConstructor();

private:
    Context* m_context;
    LexicalEnvironment* m_lexicalEnvironment;
    size_t m_stackBase;
    union {
        size_t* m_programCounter;
        NativeFunctionObject* m_calledNativeFunctionObject;
    };
    size_t m_argc;
    Value* m_argv;

    union {
        size_t m_parent;
        ExecutionStateRareData* m_rareData;
    };

    bool m_inStrictMode;
    bool m_inTryStatement;
    bool m_isNativeFunctionObjectExecutionContext;
};
}

#endif
