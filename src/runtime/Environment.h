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

private:
    EnvironmentRecord* m_record;
    LexicalEnvironment* m_outerEnvironment;
};
}
#endif
