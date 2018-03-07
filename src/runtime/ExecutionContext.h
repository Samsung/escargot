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

#ifndef __EscargotExcutionContext__
#define __EscargotExcutionContext__

namespace Escargot {

class Context;
class LexicalEnvironment;
class EnvironmentRecord;
class Value;

class ExecutionContext : public gc {
    friend class FunctionObject;
    friend class ByteCodeInterpreter;
    friend class SandBox;
    friend class Script;

public:
    ExecutionContext(Context* context, ExecutionContext* parent = nullptr, LexicalEnvironment* lexicalEnvironment = nullptr, bool inStrictMode = false)
        : m_inStrictMode(inStrictMode)
        , m_context(context)
        , m_parent(parent)
        , m_lexicalEnvironment(lexicalEnvironment)
    {
    }

    Context* context()
    {
        return m_context;
    }

    ExecutionContext* parent()
    {
        return m_parent;
    }

    LexicalEnvironment* lexicalEnvironment()
    {
        return m_lexicalEnvironment;
    }

    bool inStrictMode()
    {
        return m_inStrictMode;
    }

    FunctionObject* resolveCallee();

private:
    bool m_inStrictMode;
    Context* m_context;
    ExecutionContext* m_parent;
    LexicalEnvironment* m_lexicalEnvironment;
};
}

#endif
