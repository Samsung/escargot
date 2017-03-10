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

#ifndef __Escargot_Environment_h
#define __Escargot_Environment_h

#include "runtime/AtomicString.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

class GlobalObject;

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-lexical-environments
class LexicalEnvironment : public gc {
public:
    LexicalEnvironment(EnvironmentRecord* record, LexicalEnvironment* outerEnvironment)
        : m_record(record)
        , m_outerEnvironment(outerEnvironment)
    {
    }
    EnvironmentRecord* record()
    {
        return m_record;
    }

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        LexicalEnvironment* env = this;
        while (env) {
            if (LIKELY(env->record()->hasBinding(state, name).m_index != SIZE_MAX)) {
                return env->record()->deleteBinding(state, name);
            }
            env = env->outerEnvironment();
        }
        return true;
    }

protected:
    EnvironmentRecord* m_record;
    LexicalEnvironment* m_outerEnvironment;
};
}
#endif
