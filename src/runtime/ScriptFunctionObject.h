/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotScriptFunctionObject__
#define __EscargotScriptFunctionObject__

#include "runtime/FunctionObject.h"

namespace Escargot {

class ScriptFunctionObject : public FunctionObject {
    friend class Script;
    friend class Interpreter;
    friend class InterpreterSlowPath;
    friend class FunctionObjectProcessCallGenerator;
    friend class Global;

public:
    enum ConstructorKind {
        Base,
        Derived,
    };

    enum ThisMode {
        Lexical,
        Strict,
        Global,
    };

    ScriptFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, bool isConstructor, bool isGenerator);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    virtual bool isScriptFunctionObject() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        return true;
    }

    virtual bool isGenerator() const override
    {
        return interpretedCodeBlock()->isGenerator();
    }

    InterpretedCodeBlock* interpretedCodeBlock() const
    {
        ASSERT(m_codeBlock->isInterpretedCodeBlock());
        return static_cast<InterpretedCodeBlock*>(m_codeBlock.valueWithBase(reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX)));
    }

    ALWAYS_INLINE InterpretedCodeBlock* interpretedCodeBlockWithBase(uintptr_t base) const
    {
        CodeBlock* block = m_codeBlock.valueWithBase(base);
        ASSERT(block->isInterpretedCodeBlock());
        return static_cast<InterpretedCodeBlock*>(block);
    }

    ConstructorKind constructorKind()
    {
        ASSERT(isConstructor());
        return interpretedCodeBlock()->isDerivedClassConstructor() ? ConstructorKind::Derived : ConstructorKind::Base;
    }

    ThisMode thisMode()
    {
        InterpretedCodeBlock* codeBlock = interpretedCodeBlock();
        if (codeBlock->isArrowFunctionExpression()) {
            if (UNLIKELY(codeBlock->isOneExpressionOnlyVirtualArrowFunctionExpression() || codeBlock->isFunctionBodyOnlyVirtualArrowFunctionExpression())) {
                return ThisMode::Global;
            }
            return ThisMode::Lexical;
        } else if (codeBlock->isStrict()) {
            return ThisMode::Strict;
        } else {
            return ThisMode::Global;
        }
    }

protected:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    static inline void fillCompressedGCDescriptor(GC_word* desc)
    {
        FunctionObject::fillCompressedGCDescriptor(desc);
        GC_set_bit(desc, offsetof(ScriptFunctionObject, m_outerEnvironment) / 4);
    }
#endif
    ScriptFunctionObject()
        : FunctionObject()
        , m_outerEnvironment(nullptr)
    {
        // dummy default constructor
        // only called by Global::initialize to set tag value
    }

    ScriptFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, size_t defaultPropertyCount);

    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-call-thisargument-argumentslist
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;
    virtual void callConstructor(ExecutionState& state, Object* receiver, const size_t argc, Value* argv, Object* newTarget) override;

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment.getWithBase(reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX)).unwrap();
    }

    ALWAYS_INLINE Optional<LexicalEnvironment*> outerEnvironmentWithBase(uintptr_t base) const
    {
        return m_outerEnvironment.getWithBase(base);
    }

    void generateArgumentsObject(ExecutionState& state, size_t argc, Value* argv, FunctionEnvironmentRecord* environmentRecordWillArgumentsObjectBeLocatedIn, Optional<Value*> stackStorage, bool isMapped);
    void generateByteCodeBlock(ExecutionState& state);

    static inline void fillGCDescriptor(GC_word* desc)
    {
        FunctionObject::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(ScriptFunctionObject, m_outerEnvironment));
    }

    CompressibleHeapPointer<LexicalEnvironment> m_outerEnvironment;
};
} // namespace Escargot

#endif
