/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotScriptClassConstructorFunctionObject__
#define __EscargotScriptClassConstructorFunctionObject__

#include "runtime/ScriptFunctionObject.h"

namespace Escargot {

class ScriptClassConstructorFunctionObject : public ScriptFunctionObject {
public:
    ScriptClassConstructorFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, Object* homeObject, String* sourceSrc);

    friend class FunctionObjectProcessCallGenerator;
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget) override;

    bool isConstructor() const override
    {
        return true;
    }

    virtual bool isScriptClassConstructorFunctionObject() const override
    {
        return true;
    }

    virtual Object* homeObject() override
    {
        return m_homeObject;
    }

    String* sourceSrc()
    {
        return m_sourceSrc;
    }

private:
    virtual size_t functionPrototypeIndex() override
    {
        return ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1;
    }

    Object* m_homeObject;
    String* m_sourceSrc;
};
}

#endif
