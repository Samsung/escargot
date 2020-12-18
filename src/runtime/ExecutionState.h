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

#ifndef __EscargotExecutionState__
#define __EscargotExecutionState__

#include "util/Vector.h"

namespace Escargot {

class Context;
class ControlFlowRecord;
class LexicalEnvironment;
class EnvironmentRecord;
class Value;
class CodeBlock;
class ExecutionPauser;
class Object;
class GeneratorObject;
class FunctionObject;
class NativeFunctionObject;

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
        , m_parent(0)
        , m_hasRareData(false)
        , m_inStrictMode(false)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_argc(0)
        , m_argv(nullptr)
    {
        volatile int sp;
        m_stackLimit = (size_t)&sp;

#ifdef STACK_GROWS_DOWN
        m_stackLimit = m_stackLimit - STACK_LIMIT_FROM_BASE;
#else
        m_stackLimit = m_stackLimit + STACK_LIMIT_FROM_BASE;
#endif
    }

    ALWAYS_INLINE ExecutionState(ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
        : m_context(parent->context())
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackLimit(parent->stackLimit())
        , m_programCounter(nullptr)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_argc(parent->argc())
        , m_argv(parent->argv())
    {
    }

    ExecutionState(Context* context, size_t stackLimit)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_stackLimit(stackLimit)
        , m_programCounter(nullptr)
        , m_parent(0)
        , m_hasRareData(false)
        , m_inStrictMode(false)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_argc(0)
        , m_argv(nullptr)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackLimit(parent->stackLimit())
        , m_programCounter(nullptr)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_argc(argc)
        , m_argv(argv)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, NativeFunctionObject* callee, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_stackLimit(parent->stackLimit())
        , m_calledNativeFunctionObject(callee)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(true)
        , m_argc(argc)
        , m_argv(argv)
    {
    }

    enum ForPauserType {
        ForPauser
    };
    ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode, ForPauserType)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_stackLimit(0)
        , m_programCounter(nullptr)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_inTryStatement(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_argc(argc)
        , m_argv(argv)
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

    size_t stackLimit()
    {
        return m_stackLimit;
    }

    void throwException(const Value& e);

    ExecutionStateRareData* ensureRareData();

    bool hasRareData()
    {
        return m_hasRareData;
    }

    ExecutionStateRareData* rareData()
    {
        ASSERT(hasRareData());
        return m_rareData;
    }

    ExecutionState* parent()
    {
        if (!hasRareData()) {
            return m_parent;
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
        ASSERT(parent != this);
        if (parent) {
            m_stackLimit = parent->stackLimit();
        }
        if (hasRareData()) {
            rareData()->m_parent = parent;
        } else {
            m_parent = parent;
        }
    }

    bool inStrictMode()
    {
        return m_inStrictMode;
    }

    bool inTryStatement()
    {
        return m_inTryStatement;
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

    LexicalEnvironment* mostNearestFunctionLexicalEnvironment();
    LexicalEnvironment* mostNearestHeapAllocatedLexicalEnvironment();

    // http://www.ecma-international.org/ecma-262/6.0/#sec-getnewtarget
    Object* getNewTarget();
    // http://www.ecma-international.org/ecma-262/6.0/#sec-getthisenvironment
    EnvironmentRecord* getThisEnvironment();
    Value makeSuperPropertyReference();
    Value getSuperConstructor();

    ExecutionPauser* executionPauser();

private:
    Context* m_context;
    LexicalEnvironment* m_lexicalEnvironment;
    size_t m_stackLimit;
    union {
        size_t* m_programCounter;
        NativeFunctionObject* m_calledNativeFunctionObject;
    };

    union {
        ExecutionState* m_parent;
        ExecutionStateRareData* m_rareData;
    };

    bool m_hasRareData : 1;
    bool m_inStrictMode : 1;
    bool m_inTryStatement : 1;
    bool m_isNativeFunctionObjectExecutionContext : 1;
#ifdef ESCARGOT_32
    size_t m_argc : 28;
#else
    size_t m_argc : 60;
#endif
    Value* m_argv;
};
} // namespace Escargot

#endif
