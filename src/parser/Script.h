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
