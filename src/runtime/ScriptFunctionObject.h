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

    virtual bool isGenerator() const override
    {
        return interpretedCodeBlock()->isGenerator();
    }

    InterpretedCodeBlock* interpretedCodeBlock() const
    {
        ASSERT(m_codeBlock->isInterpretedCodeBlock());
        return (InterpretedCodeBlock*)m_codeBlock;
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

    virtual bool isScriptFunctionObject() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        return true;
    }

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    void generateArgumentsObject(ExecutionState& state, size_t argc, Value* argv, FunctionEnvironmentRecord* environmentRecordWillArgumentsObjectBeLocatedIn, Value* stackStorage, bool isMapped);
    void generateByteCodeBlock(ExecutionState& state);

    static inline void fillGCDescriptor(GC_word* desc)
    {
        FunctionObject::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(ScriptFunctionObject, m_outerEnvironment));
    }

    LexicalEnvironment* m_outerEnvironment;
};
} // namespace Escargot

#endif
