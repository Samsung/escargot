/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifdef ESCARGOT_ENABLE_PROMISE

#include "Escargot.h"
#include "Job.h"
#include "Context.h"
#include "FunctionObject.h"
#include "SandBox.h"

namespace Escargot {

SandBox::SandBoxResult PromiseReactionJob::run(ExecutionState& state)
{
    SandBox sandbox(state.context());
    return sandbox.run([&]() -> Value {
        /* 25.4.2.1.4 Handler is "Identity" case */
        if (m_reaction.m_handler == (FunctionObject*)1) {
            Value value[] = { m_argument };
            return FunctionObject::call(state, m_reaction.m_capability.m_resolveFunction, Value(), 1, value);
        }

        /* 25.4.2.1.5 Handler is "Thrower" case */
        if (m_reaction.m_handler == (FunctionObject*)2) {
            Value value[] = { m_argument };
            return FunctionObject::call(state, m_reaction.m_capability.m_rejectFunction, Value(), 1, value);
        }

        SandBox sb(state.context());
        auto res = sb.run([&]() -> Value {
            Value arguments[] = { m_argument };
            Value res = FunctionObject::call(state, m_reaction.m_handler, Value(), 1, arguments);
            Value value[] = { res };
            return FunctionObject::call(state, m_reaction.m_capability.m_resolveFunction, Value(), 1, value);
        });
        if (!res.error.isEmpty()) {
            Value reason[] = { res.error };
            return FunctionObject::call(state, m_reaction.m_capability.m_rejectFunction, Value(), 1, reason);
        }
        return res.result;
    });
}

SandBox::SandBoxResult PromiseResolveThenableJob::run(ExecutionState& state)
{
    SandBox sandbox(state.context());
    return sandbox.run([&]() -> Value {
        auto strings = &state.context()->staticStrings();
        PromiseReaction::Capability capability = m_promise->createResolvingFunctions(state);

        SandBox sb(state.context());
        auto res = sb.run([&]() -> Value {
            Value arguments[] = { capability.m_resolveFunction, capability.m_rejectFunction };
            Value thenCallResult = FunctionObject::call(state, m_then, m_thenable, 2, arguments);
            Value value[] = { thenCallResult };
            return Value();
        });
        if (!res.error.isEmpty()) {
            Object* alreadyResolved = PromiseObject::resolvingFunctionAlreadyResolved(state, capability.m_resolveFunction);
            if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).asBoolean())
                return Value();
            alreadyResolved->setThrowsException(state, strings->value, Value(true), alreadyResolved);

            Value reason[] = { res.error };
            return FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, reason);
        }
        return Value();
    });
}
}

#endif
