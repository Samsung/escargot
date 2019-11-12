/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotPromiseObject__
#define __EscargotPromiseObject__

#include "runtime/Object.h"

namespace Escargot {

struct PromiseReaction {
public:
    struct Capability {
    public:
        Capability()
            : m_promise()
            , m_resolveFunction(nullptr)
            , m_rejectFunction(nullptr)
        {
        }

        Capability(Value promise, Object* resolveFunction, Object* rejectFunction)
            : m_promise(promise)
            , m_resolveFunction(resolveFunction)
            , m_rejectFunction(rejectFunction)
        {
        }

        Capability(const Capability& other)
            : m_promise(other.m_promise)
            , m_resolveFunction(other.m_resolveFunction)
            , m_rejectFunction(other.m_rejectFunction)
        {
        }

        Value m_promise;
        Object* m_resolveFunction;
        Object* m_rejectFunction;
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
    enum PromiseState {
        Pending,
        FulFilled,
        Rejected
    };

    explicit PromiseObject(ExecutionState& state);

    virtual bool isPromiseObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Promise";
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
    static PromiseReaction::Capability newPromiseCapability(ExecutionState& state, Object* constructor);
    PromiseReaction::Capability newPromiseResultCapability(ExecutionState& state);

    static Object* resolvingFunctionAlreadyResolved(ExecutionState& state, Object* callee);

    PromiseState state() { return m_state; }
    Value promiseResult()
    {
        return m_promiseResult;
    }

    PromiseObject* then(ExecutionState& state, Value handler);
    PromiseObject* catchOperation(ExecutionState& state, Value handler);
    // http://www.ecma-international.org/ecma-262/10.0/#sec-performpromisethen
    // You can get return value when you give resultCapability
    Optional<PromiseObject*> then(ExecutionState& state, Value onFulfilled, Value onRejected, Optional<PromiseReaction::Capability> resultCapability);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    PromiseState m_state;
    SmallValue m_promiseResult;
    Reactions m_fulfillReactions;
    Reactions m_rejectReactions;

protected:
};

Value getCapabilitiesExecutorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
// http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve
// The abstract operation PromiseResolve, given a constructor and a value, returns a new promise resolved with that value.
Value promiseResolve(ExecutionState& state, Object* C, const Value& x);
// http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve-functions
Value promiseResolveFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value promiseRejectFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value promiseAllResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
}

#endif // __EscargotPromiseObject__
