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
class InterpretedCodeBlock;
class ExecutionPauser;
class Object;
class Script;
class GeneratorObject;
class FunctionObject;
class NativeFunctionObject;

typedef VectorWithInlineStorage<2, ControlFlowRecord*, GCUtil::gc_malloc_allocator<ControlFlowRecord*>> ControlFlowRecordVector;

struct ExecutionStateRareData : public gc {
    InterpretedCodeBlock* m_codeBlock; // for local eval code
    ExecutionPauser* m_pauseSource;
    ControlFlowRecordVector* m_controlFlowRecordVector;
    size_t m_programCounterWhenItStoppedByYield;

    ExecutionStateRareData()
        : m_codeBlock(nullptr)
        , m_pauseSource(nullptr)
        , m_controlFlowRecordVector(nullptr)
        , m_programCounterWhenItStoppedByYield(SIZE_MAX)
    {
    }

    ControlFlowRecordVector* ensureControlFlowRecordVector()
    {
        if (m_controlFlowRecordVector == nullptr) {
            m_controlFlowRecordVector = new ControlFlowRecordVector;
        }
        return m_controlFlowRecordVector;
    }

    ControlFlowRecordVector* controlFlowRecordVector()
    {
        return m_controlFlowRecordVector;
    }

    void setControlFlowRecordVector(ControlFlowRecordVector* v)
    {
        m_controlFlowRecordVector = v;
    }
};

class ExecutionState : public gc {
    friend class Interpreter;
    friend class InterpreterSlowPath;
    friend class ExecutionPauser;
    friend class SandBox;
    friend class VMInstance;
    friend class StackOverflowDisabler;
    friend struct OpcodeTable;

public:
    ALWAYS_INLINE ExecutionState(ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
        : m_context(parent->context())
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_programCounter(nullptr)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_inExecutionStopState(false)
#if defined(ENABLE_EXTENDED_API)
        , m_onTry(false)
        , m_onCatch(false)
        , m_onFinally(false)
#endif
#if defined(ENABLE_TCO)
        , m_inTCO(false)
#endif
        , m_argc(parent->argc())
        , m_argv(parent->argv())
    {
    }

    ExecutionState(Context* context)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_programCounter(nullptr)
        , m_parent(nullptr)
        , m_hasRareData(false)
        , m_inStrictMode(false)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_inExecutionStopState(false)
#if defined(ENABLE_EXTENDED_API)
        , m_onTry(false)
        , m_onCatch(false)
        , m_onFinally(false)
#endif
#if defined(ENABLE_TCO)
        , m_inTCO(false)
#endif
        , m_argc(0)
        , m_argv(nullptr)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(lexicalEnvironment)
        , m_programCounter(nullptr)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_isNativeFunctionObjectExecutionContext(false)
        , m_inExecutionStopState(false)
#if defined(ENABLE_EXTENDED_API)
        , m_onTry(false)
        , m_onCatch(false)
        , m_onFinally(false)
#endif
#if defined(ENABLE_TCO)
        , m_inTCO(false)
#endif
        , m_argc(argc)
        , m_argv(argv)
    {
    }

    ALWAYS_INLINE ExecutionState(Context* context, ExecutionState* parent, NativeFunctionObject* callee, size_t argc, Value* argv, bool inStrictMode)
        : m_context(context)
        , m_lexicalEnvironment(nullptr)
        , m_calledNativeFunctionObject(callee)
        , m_parent(parent)
        , m_hasRareData(false)
        , m_inStrictMode(inStrictMode)
        , m_isNativeFunctionObjectExecutionContext(true)
        , m_inExecutionStopState(false)
#if defined(ENABLE_EXTENDED_API)
        , m_onTry(false)
        , m_onCatch(false)
        , m_onFinally(false)
#endif
#if defined(ENABLE_TCO)
        , m_inTCO(false)
#endif
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

    void throwException(const Value& e);

    bool hasRareData()
    {
        return m_hasRareData;
    }

    ExecutionStateRareData* rareData()
    {
        ASSERT(hasRareData());
        return reinterpret_cast<ExecutionStateRareData*>(reinterpret_cast<uint8_t*>(this) + sizeof(ExecutionState));
    }

    ExecutionState* parent()
    {
        return m_parent;
    }

    Optional<ExecutionPauser*> pauseSource()
    {
        return hasRareData() ? rareData()->m_pauseSource : nullptr;
    }

    void setPauseSource(ExecutionPauser* pauseSource)
    {
        rareData()->m_pauseSource = pauseSource;
    }

    void setParent(ExecutionState* parent)
    {
        ASSERT(parent != this);
        m_parent = parent;
    }

    bool inStrictMode() const
    {
        return m_inStrictMode;
    }

    bool inExecutionStopState() const
    {
        return m_inExecutionStopState;
    }

#if defined(ENABLE_EXTENDED_API)
    bool onTry() const
    {
        return m_onTry;
    }

    bool onCatch() const
    {
        return m_onCatch;
    }

    bool onFinally() const
    {
        return m_onFinally;
    }
#endif

#if defined(ENABLE_TCO)
    bool inTCO() const
    {
        return m_inTCO;
    }

    void initTCOWithBuffer(Value* argv)
    {
        // initialize arguments buffer for tail call
        ASSERT(!m_inTCO);
        m_argv = argv;
        m_inTCO = true;
    }
#endif

    bool isNativeFunctionObjectExecutionContext() const
    {
        return m_isNativeFunctionObjectExecutionContext;
    }

    // callee is pauser && isNotInEvalCode
    bool inPauserScope();

    FunctionObject* resolveCallee();

    Optional<Script*> resolveOuterScript();

    bool isLocalEvalCode()
    {
        // evalcode has codeBlock now.
        return codeBlock();
    }

    Optional<InterpretedCodeBlock*> codeBlock()
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
    Optional<LexicalEnvironment*> mostNearestHeapAllocatedLexicalEnvironment();

    Optional<Object*> mostNearestHomeObject();
    static Object* convertHomeObjectIntoPrivateMemberContextObject(Object* o);
    Object* findPrivateMemberContextObject();

    // http://www.ecma-international.org/ecma-262/6.0/#sec-getnewtarget
    Object* getNewTarget();
    // http://www.ecma-international.org/ecma-262/6.0/#sec-getthisenvironment
    EnvironmentRecord* getThisEnvironment();
    Value thisValue();
    Value makeSuperPropertyReference();
    Value getSuperConstructor();

    ExecutionPauser* executionPauser();

protected:
    // create a dummy ExecutionState for initialization of Escargot engine
    ExecutionState();

    Context* m_context;
    LexicalEnvironment* m_lexicalEnvironment;
    union {
        size_t* m_programCounter;
        NativeFunctionObject* m_calledNativeFunctionObject;
    };

    ExecutionState* m_parent;

    bool m_hasRareData : 1;
    bool m_inStrictMode : 1;
    bool m_isNativeFunctionObjectExecutionContext : 1;
    bool m_inExecutionStopState : 1;
#if defined(ENABLE_EXTENDED_API)
    bool m_onTry : 1;
    bool m_onCatch : 1;
    bool m_onFinally : 1;
#endif
#if defined(ENABLE_TCO)
    bool m_inTCO : 1;
#endif

#ifdef ESCARGOT_32
    size_t m_argc : 24;
#else
    size_t m_argc : 56;
#endif
    Value* m_argv;
};

class ExtendedExecutionState : public ExecutionState {
public:
    ExtendedExecutionState(Context* context)
        : ExecutionState(context)
    {
        m_hasRareData = true;
    }

    ExtendedExecutionState(ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, bool inStrictMode)
        : ExecutionState(parent, lexicalEnvironment, inStrictMode)
    {
        m_hasRareData = true;
    }

    ExtendedExecutionState(Context* context, ExecutionState* parent, LexicalEnvironment* lexicalEnvironment, size_t argc, Value* argv, bool inStrictMode)
        : ExecutionState(context, parent, lexicalEnvironment, argc, argv, inStrictMode)
    {
        m_hasRareData = true;
    }

private:
    ExecutionStateRareData m_rareData;
};
} // namespace Escargot

#endif
