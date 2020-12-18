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

#ifndef __EscargotScriptGeneratorFunctionObject__
#define __EscargotScriptGeneratorFunctionObject__

#include "runtime/ScriptFunctionObject.h"

namespace Escargot {

// every generator(normal, arrow function, class...) uses this class
class ScriptGeneratorFunctionObject : public ScriptFunctionObject {
public:
    // both thisValue, homeObject are optional
    ScriptGeneratorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, EncodedValue thisValue = EncodedValue(EncodedValue::EmptyValue), Object* homeObject = nullptr)
        : ScriptFunctionObject(state, proto, codeBlock, outerEnvironment, false, true, false)
        , m_thisValue(thisValue)
        , m_homeObject(homeObject)
    {
    }

    virtual bool isScriptGeneratorFunctionObject() const override
    {
        return true;
    }

    bool isConstructor() const override
    {
        return false;
    }

    EncodedValue thisValue() const
    {
        return m_thisValue;
    }

    virtual Object* homeObject() override
    {
        return m_homeObject;
    }

    virtual Object* createFunctionPrototypeObject(ExecutionState& state) override;

    friend class FunctionObjectProcessCallGenerator;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-call-thisargument-argumentslist
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
    virtual Value construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget) override;

private:
    EncodedValue m_thisValue;
    Object* m_homeObject;
};
} // namespace Escargot

#endif
