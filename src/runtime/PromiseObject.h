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

#ifndef __EscargotPromiseObject__
#define __EscargotPromiseObject__

#include "runtime/Object.h"
#include "debugger/Debugger.h"

namespace Escargot {

class PromiseObject;

struct PromiseReaction {
public:
    struct Capability {
    public:
        Capability()
            : m_promise(nullptr)
            , m_resolveFunction(nullptr)
            , m_rejectFunction(nullptr)
#ifdef ESCARGOT_DEBUGGER
            , m_savedStackTrace(nullptr)
#endif /* ESCARGOT_DEBUGGER */
        {
        }

        Capability(Object* promise, Object* resolveFunction, Object* rejectFunction)
            : m_promise(promise)
            , m_resolveFunction(resolveFunction)
            , m_rejectFunction(rejectFunction)
#ifdef ESCARGOT_DEBUGGER
            , m_savedStackTrace(nullptr)
#endif /* ESCARGOT_DEBUGGER */
        {
        }

        Object* m_promise;
        Object* m_resolveFunction;
        Object* m_rejectFunction;
#ifdef ESCARGOT_DEBUGGER
        Debugger::SavedStackTraceDataVector* m_savedStackTrace;
#endif /* ESCARGOT_DEBUGGER */
    };

    PromiseReaction()
        : m_capability()
        , m_handler(nullptr)
    {
    }

    PromiseReaction(Object* handler, const Capability& capability)
        : m_capability(capability)
        , m_handler(handler)
    {
    }


    Capability m_capability;
    Object* m_handler;
};

class PromiseObject : public Object {
public:
    enum PromiseState : size_t {
        Pending,
        FulFilled,
        Rejected
    };

    enum BuiltinFunctionSlot : size_t {
        Promise = 0,
        AlreadyResolved = 1,
        Capability = 0,
        ValueOrReason = 0,
        Constructor = 0,
        OnFinally = 1,
        AlreadyCalled = 0,
        Index = 1,
        Values = 2,
        Errors = 2,
        Resolve = 3, // TODO merge to Capability
        Reject = 4, // TODO merge to Capability
        RemainingElements = 5,
    };

    explicit PromiseObject(ExecutionState& state);
    explicit PromiseObject(ExecutionState& state, Object* proto);

    virtual bool isPromiseObject() const
    {
        return true;
    }

    void fulfill(ExecutionState& state, Value value);
    void reject(ExecutionState& state, Value reason);

    typedef Vector<PromiseReaction, GCUtil::gc_malloc_allocator<PromiseReaction> > Reactions;
    void triggerPromiseReactions(ExecutionState& state, Reactions& reactions);

    void appendReaction(Object* onFulfilled, Object* onRejected, PromiseReaction::Capability& capability)
    {
        m_fulfillReactions.push_back(PromiseReaction(onFulfilled, capability));
        m_rejectReactions.push_back(PromiseReaction(onRejected, capability));
    }

    PromiseReaction::Capability createResolvingFunctions(ExecutionState& state);
    PromiseReaction::Capability newPromiseResultCapability(ExecutionState& state);

    PromiseState state() { return m_state; }
    Value promiseResult()
    {
        return m_promiseResult;
    }

    Object* then(ExecutionState& state, Value handler);
    Object* catchOperation(ExecutionState& state, Value handler);
    // http://www.ecma-international.org/ecma-262/10.0/#sec-performpromisethen
    // You can get return value when you give resultCapability
    Optional<Object*> then(ExecutionState& state, Value onFulfilled, Value onRejected, Optional<PromiseReaction::Capability> resultCapability = Optional<PromiseReaction::Capability>());

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static PromiseReaction::Capability newPromiseCapability(ExecutionState& state, Object* constructor, const Value& parentPromise = Value());
    static Value getCapabilitiesExecutorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    // http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve
    // The abstract operation PromiseResolve, given a constructor and a value, returns a new promise resolved with that value.
    static Object* promiseResolve(ExecutionState& state, Object* C, const Value& x);
    static Value promiseAllResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    static Value promiseThenFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    static Value promiseCatchFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

    // https://tc39.es/ecma262/#sec-promise.allsettled-resolve-element-functions
    static Value promiseAllSettledResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    // https://tc39.es/ecma262/#sec-promise.allsettled-resolve-element-functions
    static Value promiseAllSettledRejectElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    // https://tc39.es/ecma262/#sec-promise.any-reject-element-functions
    static Value promiseAnyRejectElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

    bool hasResolveHandlers() const { return m_fulfillReactions.size() > 0; }
    bool hasRejectHandlers() const { return m_rejectReactions.size() > 0; }

protected:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(PromiseObject, m_promiseResult));
        GC_set_bit(desc, GC_WORD_OFFSET(PromiseObject, m_fulfillReactions));
        GC_set_bit(desc, GC_WORD_OFFSET(PromiseObject, m_rejectReactions));
    }

    PromiseState m_state;
    EncodedValue m_promiseResult;
    Reactions m_fulfillReactions;
    Reactions m_rejectReactions;
};
} // namespace Escargot
#endif // __EscargotPromiseObject__
