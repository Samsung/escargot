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

#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class InterpretedCodeBlock;
class Context;

class Script : public gc {
    friend class ScriptParser;
    friend class GlobalObject;
    Script(String* fileName, String* src)
        : m_fileName(fileName)
        , m_src(src)
        , m_topCodeBlock(nullptr)
    {
    }

public:
    struct ScriptSandboxExecuteResult {
        MAKE_STACK_ALLOCATED();
        Value result;
        String* msgStr;
        struct Error {
            Value errorValue;
            struct StackTrace {
                String* fileName;
                size_t line;
                size_t column;
            };
            Vector<StackTrace, GCUtil::gc_malloc_ignore_off_page_allocator<StackTrace>> stackTrace;
        } error;
    };
    Value execute(ExecutionState& state, bool isEvalMode = false, bool needNewEnv = false, bool isOnGlobal = false);
    ScriptSandboxExecuteResult sandboxExecute(ExecutionState& state); // execute using sandbox
    String* fileName()
    {
        return m_fileName;
    }

    InterpretedCodeBlock* topCodeBlock()
    {
        return m_topCodeBlock;
    }

protected:
    Value executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isEvalMode = false, bool needNewEnv = false);
    String* m_fileName;
    String* m_src;
    InterpretedCodeBlock* m_topCodeBlock;
};
}

#endif
