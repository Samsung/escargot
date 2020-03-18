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

#ifndef __EscargotScriptClassMethodFunctionObject__
#define __EscargotScriptClassMethodFunctionObject__

#include "runtime/ScriptFunctionObject.h"

namespace Escargot {

// {method, get, set} of object literal also uses this class
class ScriptClassMethodFunctionObject : public ScriptFunctionObject {
public:
    ScriptClassMethodFunctionObject(ExecutionState& state, Object* proto, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, Object* homeObject)
        : ScriptFunctionObject(state, proto, codeBlock, outerEnvironment, false, codeBlock->isGenerator(), codeBlock->isAsync())
        , m_homeObject(homeObject)
    {
    }

    bool isConstructor() const override
    {
        return false;
    }

    virtual Object* homeObject() override
    {
        return m_homeObject;
    }

private:
    Object* m_homeObject;
};
}

#endif
