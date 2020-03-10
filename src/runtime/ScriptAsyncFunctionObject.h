/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotScriptAsyncFunctionObject__
#define __EscargotScriptAsyncFunctionObject__

#include "runtime/ScriptFunctionObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExecutionPauser.h"

namespace Escargot {

// every async function(normal, arrow function, class...) uses this class
class ScriptAsyncFunctionObject : public ScriptFunctionObject {
public:
    // both thisValue, homeObject are optional
    ScriptAsyncFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, SmallValue thisValue = SmallValue(SmallValue::EmptyValue), Object* homeObject = nullptr);

    virtual bool isScriptAsyncFunctionObject() const override
    {
        return true;
    }

    bool isConstructor() const override
    {
        return false;
    }

    SmallValue thisValue() const
    {
        return m_thisValue;
    }

    virtual Object* homeObject() override
    {
        return m_homeObject;
    }

    friend class FunctionObjectProcessCallGenerator;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-call-thisargument-argumentslist
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget) override;

    // http://www.ecma-international.org/ecma-262/10.0/#await
    static PromiseObject* awaitOperationBeforePause(ExecutionState& state, ExecutionPauser* pauser, const Value& awaitValue, Object* source);

private:
    SmallValue m_thisValue;
    Object* m_homeObject;
};
}

#endif
