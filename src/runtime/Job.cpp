/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "Job.h"
#include "Context.h"
#include "VMInstance.h"
#include "SandBox.h"
#include "runtime/FinalizationRegistryObject.h"

namespace Escargot {

SandBox::SandBoxResult PromiseReactionJob::run()
{
    Context* context = relatedContext();
    ExecutionState state(context);

    if (UNLIKELY(context->vmInstance()->isPromiseHookRegistered())) {
        Object* promiseTarget = m_reaction.m_capability.m_promise;
        PromiseObject* promise = (promiseTarget && promiseTarget->isPromiseObject()) ? promiseTarget->asPromiseObject() : nullptr;
        context->vmInstance()->triggerPromiseHook(state, VMInstance::PromiseHookType::Before, promise, Value());
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger* debugger = state.context()->debugger();
    ExecutionState* activeSavedStackTraceExecutionState = ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE;
    Debugger::SavedStackTraceDataVector* activeSavedStackTrace = nullptr;

    if (debugger != nullptr && m_reaction.m_capability.m_savedStackTrace != nullptr) {
        activeSavedStackTraceExecutionState = debugger->activeSavedStackTraceExecutionState();
        activeSavedStackTrace = debugger->activeSavedStackTrace();
        debugger->setActiveSavedStackTrace(&state, m_reaction.m_capability.m_savedStackTrace);
    }
#endif /* ESCARGOT_DEBUGGER */

    // https://www.ecma-international.org/ecma-262/10.0/#sec-promisereactionjob
    SandBox sandbox(context);
    SandBox::SandBoxResult result = sandbox.run(state, [](ExecutionState& state, void* data) -> Value {
        PromiseReactionJob* self = reinterpret_cast<PromiseReactionJob*>(data);
        /* 25.4.2.1.4 Handler is "Identity" case */
        if (self->m_reaction.m_handler == (Object*)1) {
            Value value[] = { self->m_argument };
            return Object::call(state, self->m_reaction.m_capability.m_resolveFunction, Value(), 1, value);
        }

        /* 25.4.2.1.5 Handler is "Thrower" case */
        if (self->m_reaction.m_handler == (Object*)2) {
            Value value[] = { self->m_argument };
            return Object::call(state, self->m_reaction.m_capability.m_rejectFunction, Value(), 1, value);
        }

        Value res;
        try {
            Value argument = self->m_argument;
            res = Object::call(state, self->m_reaction.m_handler, Value(), 1, &argument);
            // m_reaction.m_capability can be null when there was no result capability when promise.then()
            if (self->m_reaction.m_capability.m_promise != nullptr) {
                Value value[] = { res };
                Object::call(state, self->m_reaction.m_capability.m_resolveFunction, Value(), 1, value);
            }
        } catch (const Value& v) {
            Value reason = v;
            if (self->m_reaction.m_capability.m_rejectFunction) {
                return Object::call(state, self->m_reaction.m_capability.m_rejectFunction, Value(), 1, &reason);
            } else {
                state.throwException(reason);
            }
        }

        return res;
    },
                                                this);

#ifdef ESCARGOT_DEBUGGER
    if (activeSavedStackTraceExecutionState != ESCARGOT_DEBUGGER_NO_STACK_TRACE_RESTORE) {
        debugger->setActiveSavedStackTrace(activeSavedStackTraceExecutionState, activeSavedStackTrace);
    }
#endif /* ESCARGOT_DEBUGGER */

    if (UNLIKELY(context->vmInstance()->isPromiseHookRegistered())) {
        Object* promiseTarget = m_reaction.m_capability.m_promise;
        PromiseObject* promise = (promiseTarget && promiseTarget->isPromiseObject()) ? promiseTarget->asPromiseObject() : nullptr;
        context->vmInstance()->triggerPromiseHook(state, VMInstance::PromiseHookType::After, promise, Value());
    }

    return result;
}

SandBox::SandBoxResult PromiseResolveThenableJob::run()
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-promiseresolvethenablejob
    SandBox sandbox(relatedContext());
    return sandbox.run([](ExecutionState& state, void* data) -> Value {
        PromiseResolveThenableJob* self = reinterpret_cast<PromiseResolveThenableJob*>(data);
        auto strings = &state.context()->staticStrings();
        PromiseReaction::Capability capability = self->m_promise->createResolvingFunctions(state);

        Value result;
        try {
            Value arguments[] = { capability.m_resolveFunction, capability.m_rejectFunction };
            result = Object::call(state, self->m_then, self->m_thenable, 2, arguments);
        } catch (const Value& v) {
            Value reason = v;
            return Object::call(state, capability.m_rejectFunction, Value(), 1, &reason);
        }

        return result;
    },
                       this);
}

SandBox::SandBoxResult CleanupSomeJob::run()
{
    auto oldCallback = m_object->m_cleanupCallback;
    m_object->m_cleanupCallback = m_callback;

    clearStack<1024>();
    GC_gcollect_and_unmap();
    GC_gcollect_and_unmap();
    GC_gcollect_and_unmap();
    m_object->m_cleanupCallback = oldCallback;

    SandBox::SandBoxResult result;
    result.result = Value();
    return result;
}
} // namespace Escargot
