/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotScriptVirtualArrowFunctionObject__
#define __EscargotScriptVirtualArrowFunctionObject__

#include "runtime/ScriptFunctionObject.h"

namespace Escargot {

class ScriptVirtualArrowFunctionObject : public ScriptFunctionObject {
    friend class ScriptVirtualArrowFunctionObjectPrototypeSetter;

public:
    ScriptVirtualArrowFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment)
        : ScriptFunctionObject(state, proto, codeBlock, outerEnvironment, false, codeBlock->isGenerator())
    {
        m_prototype = nullptr;
    }

    virtual bool isScriptVirtualArrowFunctionObject() const override
    {
        return true;
    }

    friend class FunctionObjectProcessCallGenerator;
    Value call(ExecutionState& state, const Value& thisValue, Object* homeObject);

    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override;
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;

    bool isConstructor() const override
    {
        return false;
    }

    virtual Object* homeObject() override
    {
        ASSERT(!hasRareData());
        return m_prototype;
    }
};
} // namespace Escargot

#endif
