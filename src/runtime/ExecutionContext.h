/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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

#if ESCARGOT_ENABLE_PROMISE
    FunctionObject* resolveCallee();
#endif

private:
    bool m_inStrictMode;
    Context* m_context;
    ExecutionContext* m_parent;
    LexicalEnvironment* m_lexicalEnvironment;
};
}

#endif
