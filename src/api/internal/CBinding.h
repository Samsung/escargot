/* Copyright (c) 2026-present Samsung Electronics Co., Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ESCARGOT_C_BINDING_H
#define ESCARGOT_C_BINDING_H

#include "EscargotC.h"
#include "EscargotPublic.h"
#include <memory>
#include <new>
#include <stdexcept>

namespace Escargot {
namespace CAPI {

inline escargot_context toC(ContextRef* value) { return reinterpret_cast<escargot_context>(value); }
inline escargot_vm toC(VMInstanceRef* value) { return reinterpret_cast<escargot_vm>(value); }
inline escargot_value toC(ValueRef* value) { return reinterpret_cast<escargot_value>(value); }
inline ValueRef* fromC(escargot_value value) { return reinterpret_cast<ValueRef*>(value); }

// Internal adapter for existing C++ bindings. Public C clients only use the
// opaque handles in EscargotC.h. Retention is implemented once by the C API.
template <typename T>
class Persistent {
    struct Release {
        void operator()(escargot_reference__* reference) const { escargot_reference_release(reference); }
    };
    std::unique_ptr<escargot_reference__, Release> m_reference;

public:
    Persistent() = default;
    Persistent(T* value) { reset(value); }
    Persistent(Persistent&&) = default;
    Persistent& operator=(Persistent&&) = default;
    Persistent(const Persistent&) = delete;
    Persistent& operator=(const Persistent&) = delete;
    void reset(T* value)
    {
        if (!value) {
            m_reference.reset();
            return;
        }
        escargot_reference reference = nullptr;
        if (escargot_reference_create(reinterpret_cast<escargot_value>(value), &reference) != ESCARGOT_OK) {
            throw std::bad_alloc();
        }
        m_reference.reset(reference);
    }
    T* get() const { return reinterpret_cast<T*>(escargot_reference_get(m_reference.get())); }
    T* operator->() const { return get(); }
    operator T*() const { return get(); }
    bool isWeak() const { return escargot_reference_is_weak(m_reference.get()); }
    void setWeak() { escargot_reference_set_weak(m_reference.get(), 1); }
    void clearWeak() { escargot_reference_set_weak(m_reference.get(), 0); }
    T* release()
    {
        T* value = get();
        m_reference.reset();
        return value;
    }
};

template <typename T>
class PendingReference {
    Persistent<T> m_value;

public:
    PendingReference& operator=(T* value)
    {
        m_value.reset(value);
        return *this;
    }
    PendingReference& operator=(std::nullptr_t)
    {
        m_value.reset(nullptr);
        return *this;
    }
    bool hasValue() const { return m_value.get() != nullptr; }
    explicit operator bool() const { return hasValue(); }
    T* value() const { return m_value.get(); }
    T* get() const { return m_value.get(); }
};

// Track only synchronous native frames. Destruction restores the preceding
// frame, including when a callback exits by throwing a JS exception.
class ESCARGOT_C_EXPORT ExecutionScope {
public:
    explicit ExecutionScope(ExecutionStateRef* state);
    ~ExecutionScope();
    ExecutionScope(const ExecutionScope&) = delete;
    ExecutionScope& operator=(const ExecutionScope&) = delete;

private:
    size_t m_depth;
};

ESCARGOT_C_EXPORT void trackLocal(ValueRef* value);

ESCARGOT_C_EXPORT Escargot::Evaluator::EvaluatorResult takeResult(escargot_status status, escargot_result result);

class Evaluator {
public:
    using EvaluatorResult = Escargot::Evaluator::EvaluatorResult;
    template <typename... Args, typename F>
    static EvaluatorResult execute(ContextRef* context, F&& function, Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef*, Args...);
        std::tuple<ExecutionStateRef*, Args...> values(nullptr, args...);
        struct Call {
            Closure function;
            std::tuple<ExecutionStateRef*, Args...>* values;
        } call = { Closure(function), &values };
        escargot_result result = nullptr;
        auto status = escargot_context_execute(toC(context), [](escargot_execution_state state, void* data) -> escargot_value {
            auto* call = static_cast<Call*>(data);
            std::get<0>(*call->values) = reinterpret_cast<ExecutionStateRef*>(state);
            return toC(EvaluatorUtil::applyTupleIntoArgumentsOfVariadicTemplateFunction(call->function, *call->values)); }, &call, &result);
        return takeResult(status, result);
    }
    template <typename... Args, typename F>
    static EvaluatorResult execute(ExecutionStateRef* state, F&& function, Args... args)
    {
        ExecutionScope scope(state);
        return execute(state->context(), std::forward<F>(function), args...);
    }
};

} // namespace CAPI
} // namespace Escargot
#endif
