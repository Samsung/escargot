/* Copyright (c) 2026-present Samsung Electronics Co., Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#if defined(ENABLE_C_API)
#include "Escargot.h"
#include "api/internal/CBinding.h"
#include <map>
#include <new>

using namespace Escargot;

// These objects live in uncollectable, scanned memory: the result includes a
// GC-managed stack-trace buffer whose pointer would otherwise be invisible in
// a C/Rust allocator. Release runs destructors before freeing the storage.
struct escargot_result__ {
    Evaluator::EvaluatorResult result;
    explicit escargot_result__(Evaluator::EvaluatorResult&& result)
        : result(std::move(result))
    {
    }
};
struct escargot_reference__ {
    PersistentRefHolder<void> value;
    size_t owners = 1;
    explicit escargot_reference__(void* value)
        : value(value)
    {
    }
};
struct escargot_handle_scope__ {
    Optional<escargot_handle_scope> parent;
    std::vector<escargot_reference> references;
    bool escaped = false;
};

struct escargot_template__ {
    PersistentRefHolder<TemplateRef> value;
    explicit escargot_template__(TemplateRef* value)
        : value(value)
    {
    }
};
struct escargot_backing_store__ {
    PersistentRefHolder<BackingStoreRef> value;
    explicit escargot_backing_store__(BackingStoreRef* value)
        : value(value)
    {
    }
};

namespace {
thread_local std::vector<ExecutionStateRef*> activeStates;
thread_local Optional<escargot_handle_scope> topScope;
thread_local size_t liveReferences = 0;
thread_local size_t liveResults = 0;
thread_local size_t liveTemplates = 0;
thread_local size_t liveStores = 0;
thread_local bool ownsRuntime = false;
thread_local bool ownsThread = false;
thread_local std::map<VMInstanceRef*, PersistentRefHolder<VMInstanceRef>> ownedVMs;
thread_local std::map<ContextRef*, PersistentRefHolder<ContextRef>> ownedContexts;

ContextRef* context(escargot_context value) { return reinterpret_cast<ContextRef*>(value); }
VMInstanceRef* vm(escargot_vm value) { return reinterpret_cast<VMInstanceRef*>(value); }
ValueRef* value(escargot_value v) { return reinterpret_cast<ValueRef*>(v); }
escargot_value handle(ValueRef* v) { return reinterpret_cast<escargot_value>(v); }

template <typename F>
escargot_status guard(F function)
{
    try {
        return function();
    } catch (const std::bad_alloc&) {
        return ESCARGOT_OUT_OF_MEMORY;
#if defined(ENABLE_RUNTIME_ICU_BINDER)
    } catch (const RuntimeICUBinder::LoadError&) {
        return ESCARGOT_DEPENDENCY_UNAVAILABLE;
#endif
    } catch (...) {
        return ESCARGOT_INTERNAL_ERROR;
    }
}

escargot_status storeResult(Evaluator::EvaluatorResult&& result, escargot_result* out)
{
    void* storage = Memory::gcMallocUncollectable(sizeof(escargot_result__));
    auto* owned = new (storage) escargot_result__(std::move(result));
    ++liveResults;
    *out = owned;
    return owned->result.isSuccessful() ? ESCARGOT_OK : ESCARGOT_PENDING_EXCEPTION;
}

bool resourcesAlive()
{
    return liveReferences || liveResults || liveTemplates || liveStores || topScope || !activeStates.empty() || !ownedVMs.empty() || !ownedContexts.empty();
}

class CPlatform : public PlatformRef {
public:
    void markJSJobEnqueued(ContextRef*) override {}
    void markJSJobFromAnotherThreadExists(ContextRef*) override {}
    LoadModuleResult onLoadModule(ContextRef*, ScriptRef*, StringRef*, ModuleType) override
    {
        return LoadModuleResult(ErrorObjectRef::Code::TypeError, StringRef::createFromASCII("No module loader installed"));
    }
    void didLoadModule(ContextRef*, OptionalRef<ScriptRef>, ScriptRef*) override {}
};

struct Callback {
    escargot_function_callback function;
    Optional<void*> data;
};
static ValueRef* invokeCallback(ExecutionStateRef* state, ValueRef* receiver, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget)
{
    CAPI::ExecutionScope scope(state);
    auto callee = state->resolveCallee();
    auto* stored = static_cast<Callback*>(callee.value()->extraData());
    // These handles borrow the rooted interpreter arguments. The
    // copied array itself does not need to own any JS value.
    std::vector<escargot_value> args(argc);
    for (size_t i = 0; i < argc; ++i)
        args[i] = handle(argv[i]);
    escargot_value result = nullptr;
    auto status = stored->function(CAPI::toC(state->context()), handle(receiver), argc, args.data(), newTarget ? handle(newTarget.value()) : nullptr, stored->data.unwrap(), &result);
    if (status == ESCARGOT_PENDING_EXCEPTION && result)
        state->throwException(value(result));
    if (status != ESCARGOT_OK)
        state->throwException(ErrorObjectRef::create(state, ErrorObjectRef::TypeError, StringRef::createFromASCII("Native callback failed")));
    return result ? value(result) : ValueRef::createUndefined();
}

template <typename F>
escargot_status execute(escargot_context ctx, escargot_result* out, F function)
{
    if (out)
        *out = nullptr;
    if (!ctx || !out || !Globals::isInitialized())
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status {
        Optional<ExecutionStateRef*> parent;
        if (!activeStates.empty() && activeStates.back()->context()->vmInstance() == context(ctx)->vmInstance()) {
            parent = activeStates.back();
        }
        auto runner = [](ExecutionStateRef* state, void* data, void*) -> ValueRef* {
            CAPI::ExecutionScope scope(state);
            return (*static_cast<F*>(data))(state);
        };
        auto result = parent
            ? Evaluator::executeFunction(context(ctx), parent.value(), runner, &function, nullptr)
            : Evaluator::executeFunction(context(ctx), runner, &function, nullptr);
        return storeResult(std::move(result), out);
    });
}
} // namespace

namespace Escargot {
namespace CAPI {
void trackLocal(ValueRef* value)
{
    if (topScope && escargot_handle_scope_add(topScope.value(), toC(value)) != ESCARGOT_OK) {
        throw std::bad_alloc();
    }
}
ExecutionScope::ExecutionScope(ExecutionStateRef* state)
    : m_depth(activeStates.size())
{
    activeStates.push_back(state);
}
ExecutionScope::~ExecutionScope()
{
    ASSERT(activeStates.size() == m_depth + 1);
    activeStates.pop_back();
}
Escargot::Evaluator::EvaluatorResult takeResult(escargot_status status, escargot_result result)
{
    std::unique_ptr<escargot_result__, decltype(&escargot_result_release)> owner(result, escargot_result_release);
    if (!result) {
        if (status == ESCARGOT_OUT_OF_MEMORY)
            throw std::bad_alloc();
        throw std::runtime_error("C API execution failed");
    }
    return result->result;
}
} // namespace CAPI
} // namespace Escargot

extern "C" {
uint32_t escargot_api_version(void) { return ESCARGOT_C_API_VERSION; }
escargot_status escargot_initialize(void)
{
    return guard([]() -> escargot_status {
        if (!Globals::isInitialized()) {
            Globals::initialize(new CPlatform());
            ownsRuntime = true;
        }
        return ESCARGOT_OK;
    });
}
escargot_status escargot_finalize(void)
{
    if (!ownsRuntime || resourcesAlive())
        return ESCARGOT_INVALID_STATE;
    return guard([]() -> escargot_status { Globals::finalize(); ownsRuntime = false; return ESCARGOT_OK; });
}
escargot_status escargot_thread_initialize(void)
{
    return guard([]() -> escargot_status {
        if (!Globals::isInitialized()) {
            Globals::initializeThread();
            ownsThread = true;
        }
        return ESCARGOT_OK;
    });
}
escargot_status escargot_thread_finalize(void)
{
    if (!ownsThread || resourcesAlive())
        return ESCARGOT_INVALID_STATE;
    return guard([]() -> escargot_status { Globals::finalizeThread(); ownsThread = false; return ESCARGOT_OK; });
}
escargot_status escargot_vm_create(const char* locale, const char* timezone, escargot_vm* out)
{
    if (out)
        *out = nullptr;
    if (!out || !Globals::isInitialized())
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status {
        auto root = VMInstanceRef::create(locale, timezone);
        VMInstanceRef* pointer = root.get();
        ownedVMs.emplace(pointer, std::move(root));
        *out = reinterpret_cast<escargot_vm>(pointer);
        return ESCARGOT_OK;
    });
}
escargot_status escargot_vm_destroy(escargot_vm v)
{
    if (!v || !ownedVMs.count(vm(v)))
        return ESCARGOT_INVALID_ARGUMENT;
    for (const auto& item : ownedContexts)
        if (item.first->vmInstance() == vm(v))
            return ESCARGOT_INVALID_STATE;
    for (auto* state : activeStates)
        if (state->context()->vmInstance() == vm(v))
            return ESCARGOT_INVALID_STATE;
    ownedVMs.erase(vm(v));
    return ESCARGOT_OK;
}
escargot_status escargot_context_create(escargot_vm v, escargot_context* out)
{
    if (out)
        *out = nullptr;
    if (!v || !out || !Globals::isInitialized())
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status {
        auto root = ContextRef::create(vm(v));
        ContextRef* pointer = root.get();
        ownedContexts.emplace(pointer, std::move(root));
        *out = reinterpret_cast<escargot_context>(pointer);
        return ESCARGOT_OK;
    });
}
escargot_status escargot_context_destroy(escargot_context ctx)
{
    if (!ctx || !ownedContexts.count(context(ctx)))
        return ESCARGOT_INVALID_ARGUMENT;
    for (auto* state : activeStates)
        if (state->context() == context(ctx))
            return ESCARGOT_INVALID_STATE;
    return guard([&]() -> escargot_status { context(ctx)->clearRelatedQueuedJobs(); ownedContexts.erase(context(ctx)); return ESCARGOT_OK; });
}
escargot_vm escargot_context_get_vm(escargot_context ctx) { return ctx ? reinterpret_cast<escargot_vm>(context(ctx)->vmInstance()) : nullptr; }
escargot_value escargot_context_get_global(escargot_context ctx) { return ctx ? handle(context(ctx)->globalObject()) : nullptr; }
escargot_context escargot_execution_get_context(escargot_execution_state state)
{
    return state ? reinterpret_cast<escargot_context>(reinterpret_cast<ExecutionStateRef*>(state)->context()) : nullptr;
}
escargot_status escargot_context_execute(escargot_context ctx, escargot_execute_callback callback, void* data, escargot_result* out)
{
    if (!callback) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* {
        auto result = callback(reinterpret_cast<escargot_execution_state>(state), data);
        return result ? value(result) : ValueRef::createUndefined();
    });
}
escargot_status escargot_context_eval(escargot_context ctx, const char* source, size_t length, const char* name, size_t nameLength, escargot_result* out)
{
    if ((!source && length) || (!name && nameLength)) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* {
        auto parsed = state->context()->scriptParser()->initializeScript(StringRef::createFromUTF8(source ? source : "", length), StringRef::createFromUTF8(name ? name : "", nameLength));
        return parsed.fetchScriptThrowsExceptionIfParseError(state)->execute(state);
    });
}
escargot_status escargot_vm_run_jobs(escargot_vm v, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!v || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status {
        Evaluator::EvaluatorResult result;
        result.result = ValueRef::createUndefined();
        while (vm(v)->hasPendingJob()) {
            auto next = vm(v)->executePendingJob();
            result = next;
            if (!result.isSuccessful())
                break;
        }
        return storeResult(std::move(result), out);
    });
}
int escargot_vm_has_pending_job(escargot_vm v) { return v && vm(v)->hasPendingJob(); }
escargot_status escargot_result_status(escargot_result r) { return !r ? ESCARGOT_INVALID_ARGUMENT : r->result.isSuccessful() ? ESCARGOT_OK
                                                                                                                             : ESCARGOT_PENDING_EXCEPTION; }
escargot_value escargot_result_value(escargot_result r) { return r && r->result.isSuccessful() ? handle(r->result.result) : nullptr; }
escargot_value escargot_result_exception(escargot_result r) { return r && r->result.error ? handle(r->result.error.value()) : nullptr; }
size_t escargot_result_stack_size(escargot_result r) { return r ? r->result.stackTrace.size() : 0; }
escargot_status escargot_result_stack_frame(escargot_result r, size_t index, escargot_value* source, escargot_value* function, size_t* line, size_t* column)
{
    if (source)
        *source = nullptr;
    if (function)
        *function = nullptr;
    if (line)
        *line = 0;
    if (column)
        *column = 0;
    if (!r || index >= r->result.stackTrace.size() || !source || !function || !line || !column)
        return ESCARGOT_INVALID_ARGUMENT;
    auto& frame = r->result.stackTrace[index];
    *source = handle(frame.srcName);
    *function = handle(frame.functionName);
    *line = frame.loc.line;
    *column = frame.loc.column;
    return ESCARGOT_OK;
}
void escargot_result_release(escargot_result r)
{
    if (r) {
        r->~escargot_result__();
        Memory::gcFree(r);
        --liveResults;
    }
}
escargot_status escargot_reference_create(escargot_value v, escargot_reference* out)
{
    if (out)
        *out = nullptr;
    if (!v || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status { *out = new escargot_reference__(v); ++liveReferences; return ESCARGOT_OK; });
}
escargot_status escargot_reference_retain(escargot_reference r)
{
    if (!r || r->owners == SIZE_MAX)
        return ESCARGOT_INVALID_ARGUMENT;
    ++r->owners;
    return ESCARGOT_OK;
}
void escargot_reference_release(escargot_reference r)
{
    if (r && !--r->owners) {
        delete r;
        --liveReferences;
    }
}
escargot_value escargot_reference_get(escargot_reference r) { return r ? reinterpret_cast<escargot_value>(r->value.get()) : nullptr; }
escargot_status escargot_reference_set_weak(escargot_reference r, int weak)
{
    if (!r)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!weak && !r->value.get())
        return ESCARGOT_INVALID_STATE;
    return guard([&]() -> escargot_status { if (weak) r->value.setWeak(); else r->value.clearWeak(); return ESCARGOT_OK; });
}
int escargot_reference_is_weak(escargot_reference r) { return r && r->value.isWeak(); }
escargot_status escargot_handle_scope_open(escargot_handle_scope* out)
{
    if (out)
        *out = nullptr;
    if (!out)
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status { auto* scope = new escargot_handle_scope__(); scope->parent = topScope; topScope = scope; *out = scope; return ESCARGOT_OK; });
}
escargot_status escargot_handle_scope_add(escargot_handle_scope scope, escargot_value v)
{
    if (!scope || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    escargot_reference reference = nullptr;
    auto status = escargot_reference_create(v, &reference);
    if (status != ESCARGOT_OK)
        return status;
    status = guard([&]() -> escargot_status { scope->references.push_back(reference); return ESCARGOT_OK; });
    if (status != ESCARGOT_OK)
        escargot_reference_release(reference);
    return status;
}
escargot_status escargot_handle_scope_escape(escargot_handle_scope scope, escargot_value v)
{
    if (!scope || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!topScope || topScope.value() != scope || !scope->parent || scope->escaped)
        return ESCARGOT_SCOPE_MISMATCH;
    auto status = escargot_handle_scope_add(scope->parent.value(), v);
    if (status == ESCARGOT_OK)
        scope->escaped = true;
    return status;
}
escargot_status escargot_handle_scope_close(escargot_handle_scope scope)
{
    if (!scope || !topScope || topScope.value() != scope)
        return ESCARGOT_SCOPE_MISMATCH;
    for (auto reference : scope->references)
        escargot_reference_release(reference);
    topScope = scope->parent;
    delete scope;
    return ESCARGOT_OK;
}
escargot_value escargot_value_undefined(void) { return handle(ValueRef::createUndefined()); }
escargot_value escargot_value_null(void) { return handle(ValueRef::createNull()); }
escargot_value escargot_value_boolean(int v) { return handle(ValueRef::create(v != 0)); }
escargot_status escargot_value_number(double v, escargot_value* out)
{
    if (!out)
        return ESCARGOT_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&]() -> escargot_status { *out = handle(ValueRef::create(v)); return ESCARGOT_OK; });
}
escargot_status escargot_value_get_type(escargot_value v, escargot_value_type* out)
{
    if (out)
        *out = 0;
    if (!v || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    auto* p = value(v);
    *out = p->isUndefined() ? ESCARGOT_UNDEFINED : p->isNull() ? ESCARGOT_NULL
        : p->isBoolean()                                       ? ESCARGOT_BOOLEAN
        : p->isNumber()                                        ? ESCARGOT_NUMBER
        : p->isString()                                        ? ESCARGOT_STRING
        : p->isSymbol()                                        ? ESCARGOT_SYMBOL
        : p->isBigInt()                                        ? ESCARGOT_BIGINT
                                                               : ESCARGOT_OBJECT;
    return ESCARGOT_OK;
}
escargot_status escargot_value_get_number(escargot_value v, double* out)
{
    if (out)
        *out = 0;
    if (!v || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(v)->isNumber())
        return ESCARGOT_TYPE_ERROR;
    *out = value(v)->asNumber();
    return ESCARGOT_OK;
}
escargot_status escargot_value_get_boolean(escargot_value v, int* out)
{
    if (out)
        *out = 0;
    if (!v || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(v)->isBoolean())
        return ESCARGOT_TYPE_ERROR;
    *out = value(v)->asBoolean();
    return ESCARGOT_OK;
}
escargot_status escargot_string_create_utf8(const char* data, size_t length, escargot_value* out)
{
    if (out)
        *out = nullptr;
    if (!out || (!data && length))
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status { *out = handle(StringRef::createFromUTF8(data ? data : "", length)); return ESCARGOT_OK; });
}
escargot_status escargot_string_create_utf16(const uint16_t* data, size_t length, escargot_value* out)
{
    if (out)
        *out = nullptr;
    if (!out || (!data && length))
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status { std::u16string text;
        text.reserve(length);
        for (size_t i = 0; i < length; ++i) text.push_back(static_cast<char16_t>(data[i]));
        *out = handle(StringRef::createFromUTF16(text.data(), text.size())); return ESCARGOT_OK; });
}
escargot_status escargot_string_copy_utf8(escargot_value v, char* buffer, size_t capacity, size_t* out)
{
    if (out)
        *out = 0;
    if (!v || !out || (!buffer && capacity))
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(v)->isString())
        return ESCARGOT_TYPE_ERROR;
    return guard([&]() -> escargot_status {
        auto text = value(v)->asString()->toStdUTF8String();
        *out = text.size();
        if (!buffer)
            return ESCARGOT_OK;
        if (capacity < text.size())
            return ESCARGOT_BUFFER_TOO_SMALL;
        memcpy(buffer, text.data(), text.size());
        return ESCARGOT_OK;
    });
}
escargot_status escargot_string_copy_utf16(escargot_value v, uint16_t* buffer, size_t capacity, size_t* out)
{
    if (out)
        *out = 0;
    if (!v || !out || (!buffer && capacity))
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(v)->isString())
        return ESCARGOT_TYPE_ERROR;
    return guard([&]() -> escargot_status {
        auto text = value(v)->asString()->stringBufferAccessData();
        *out = text.length;
        if (!buffer)
            return ESCARGOT_OK;
        if (capacity < text.length)
            return ESCARGOT_BUFFER_TOO_SMALL;
        for (size_t i = 0; i < text.length; ++i)
            buffer[i] = text.charAt(i);
        return ESCARGOT_OK;
    });
}
escargot_status escargot_value_to_string(escargot_context ctx, escargot_value v, escargot_result* out)
{
    if (!v) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return value(v)->toString(state); });
}
escargot_status escargot_value_to_number(escargot_context ctx, escargot_value v, escargot_result* out)
{
    if (!v) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ValueRef::create(value(v)->toNumber(state)); });
}
escargot_status escargot_object_create(escargot_context ctx, escargot_result* out)
{
    return execute(ctx, out, [](ExecutionStateRef* state) -> ValueRef* { return ObjectRef::create(state); });
}
escargot_status escargot_array_create(escargot_context ctx, uint32_t length, escargot_result* out)
{
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ArrayObjectRef::create(state, length); });
}
escargot_status escargot_object_get_property(escargot_context ctx, escargot_value object, escargot_value key, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!object || !key)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(object)->isObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return value(object)->asObject()->get(state, value(key)); });
}
escargot_status escargot_object_set_property(escargot_context ctx, escargot_value object, escargot_value key, escargot_value v, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!object || !key || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(object)->isObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ValueRef::create(value(object)->asObject()->set(state, value(key), value(v))); });
}
escargot_status escargot_object_has_property(escargot_context ctx, escargot_value object, escargot_value key, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!object || !key)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(object)->isObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ValueRef::create(value(object)->asObject()->hasProperty(state, value(key))); });
}
escargot_status escargot_object_delete_property(escargot_context ctx, escargot_value object, escargot_value key, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!object || !key)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(object)->isObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ValueRef::create(value(object)->asObject()->deleteOwnProperty(state, value(key))); });
}
escargot_status escargot_function_call(escargot_context ctx, escargot_value function, escargot_value receiver, size_t argc, const escargot_value* argv, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!function || !receiver || (!argv && argc))
        return ESCARGOT_INVALID_ARGUMENT;
    for (size_t i = 0; i < argc; ++i)
        if (!argv[i])
            return ESCARGOT_INVALID_ARGUMENT;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* {
        GCManagedVector<ValueRef*> args(argc);
        for (size_t i = 0; i < argc; ++i)
            args[i] = value(argv[i]);
        return value(function)->call(state, value(receiver), argc, argc ? &args[0] : nullptr);
    });
}
escargot_status escargot_function_construct(escargot_context ctx, escargot_value function, size_t argc, const escargot_value* argv, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!function || (!argv && argc))
        return ESCARGOT_INVALID_ARGUMENT;
    for (size_t i = 0; i < argc; ++i)
        if (!argv[i])
            return ESCARGOT_INVALID_ARGUMENT;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* {
        GCManagedVector<ValueRef*> args(argc);
        for (size_t i = 0; i < argc; ++i)
            args[i] = value(argv[i]);
        return value(function)->construct(state, argc, argc ? &args[0] : nullptr);
    });
}
escargot_status escargot_function_create(escargot_context ctx, const char* name, size_t length, uint32_t argumentCount, int constructible, escargot_function_callback callback, void* data, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!callback || (!name && length))
        return ESCARGOT_INVALID_ARGUMENT;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* {
        auto* stored = new (Memory::gcMalloc(sizeof(Callback))) Callback{ callback, data };
        FunctionObjectRef::NativeFunctionInfo info(
            AtomicStringRef::create(state->context(), StringRef::createFromUTF8(name ? name : "", length)),
            invokeCallback, argumentCount, true, constructible != 0);
        auto* function = FunctionObjectRef::create(state, info);
        function->setExtraData(stored);
        return function;
    });
}

escargot_status escargot_object_template_create(escargot_template* out)
{
    if (!out)
        return ESCARGOT_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&]() -> escargot_status { *out = new escargot_template__(ObjectTemplateRef::create()); ++liveTemplates; return ESCARGOT_OK; });
}
escargot_status escargot_function_template_create(escargot_context ctx, const char* name, size_t length, uint32_t argumentCount, int constructible, escargot_function_callback callback, void* data, escargot_template* out)
{
    if (out)
        *out = nullptr;
    if (!ctx || !callback || !out || (!name && length))
        return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status {
        auto* stored = new (Memory::gcMalloc(sizeof(Callback))) Callback{ callback, data };
        auto* target = FunctionTemplateRef::create(AtomicStringRef::create(context(ctx), StringRef::createFromUTF8(name ? name : "", length)), argumentCount, true, constructible != 0, invokeCallback);
        target->setInstanceExtraData(stored);
        *out = new escargot_template__(target);
        ++liveTemplates;
        return ESCARGOT_OK;
    });
}
escargot_status escargot_template_set(escargot_template target, escargot_value key, escargot_value v, int writable, int enumerable, int configurable)
{
    if (!target || !key || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(key)->isString() && !value(key)->isSymbol())
        return ESCARGOT_TYPE_ERROR;
    if (target->value->didInstantiate())
        return ESCARGOT_INVALID_STATE;
    return guard([&]() -> escargot_status { target->value->set(value(key), value(v), writable, enumerable, configurable); return ESCARGOT_OK; });
}
escargot_status escargot_template_set_template(escargot_template target, escargot_value key, escargot_template v, int writable, int enumerable, int configurable)
{
    if (!target || !key || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(key)->isString() && !value(key)->isSymbol())
        return ESCARGOT_TYPE_ERROR;
    if (target->value->didInstantiate())
        return ESCARGOT_INVALID_STATE;
    return guard([&]() -> escargot_status { target->value->set(value(key), v->value.get(), writable, enumerable, configurable); return ESCARGOT_OK; });
}
escargot_status escargot_template_instantiate(escargot_context ctx, escargot_template target, escargot_result* out)
{
    if (!target) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return target->value->instantiate(state->context()); });
}
escargot_status escargot_function_template_get_instance(escargot_template target, escargot_template* out)
{
    if (out)
        *out = nullptr;
    if (!target || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!target->value->isFunctionTemplate())
        return ESCARGOT_TYPE_ERROR;
    return guard([&]() -> escargot_status { *out = new escargot_template__(static_cast<FunctionTemplateRef*>(target->value.get())->instanceTemplate()); ++liveTemplates; return ESCARGOT_OK; });
}
escargot_status escargot_function_template_get_prototype(escargot_template target, escargot_template* out)
{
    if (out)
        *out = nullptr;
    if (!target || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!target->value->isFunctionTemplate())
        return ESCARGOT_TYPE_ERROR;
    return guard([&]() -> escargot_status { *out = new escargot_template__(static_cast<FunctionTemplateRef*>(target->value.get())->prototypeTemplate()); ++liveTemplates; return ESCARGOT_OK; });
}
escargot_status escargot_function_template_inherit(escargot_template target, escargot_template parent)
{
    if (!target || target == parent)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!target->value->isFunctionTemplate() || (parent && !parent->value->isFunctionTemplate()))
        return ESCARGOT_TYPE_ERROR;
    auto* child = static_cast<FunctionTemplateRef*>(target->value.get());
    if (child->didInstantiate())
        return ESCARGOT_INVALID_STATE;
    auto ancestor = parent ? OptionalRef<FunctionTemplateRef>(static_cast<FunctionTemplateRef*>(parent->value.get())) : OptionalRef<FunctionTemplateRef>();
    for (auto current = ancestor; current; current = current->parent())
        if (current.value() == child)
            return ESCARGOT_INVALID_ARGUMENT;
    return guard([&]() -> escargot_status { child->inherit(ancestor); return ESCARGOT_OK; });
}
void escargot_template_release(escargot_template target)
{
    if (target) {
        delete target;
        --liveTemplates;
    }
}
escargot_status escargot_backing_store_create(size_t length, escargot_backing_store* out)
{
    if (!out)
        return ESCARGOT_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&]() -> escargot_status { *out = new escargot_backing_store__(BackingStoreRef::createDefaultNonSharedBackingStore(length)); ++liveStores; return ESCARGOT_OK; });
}
escargot_status escargot_backing_store_create_external(void* data, size_t length, escargot_backing_store_deleter deleter, void* callbackData, escargot_backing_store* out)
{
    if (out)
        *out = nullptr;
    if (!out || !deleter || (!data && length))
        return ESCARGOT_INVALID_ARGUMENT;
    // Allocate the owner before transferring the caller's buffer to the GC.
    return guard([&]() -> escargot_status {
        std::unique_ptr<escargot_backing_store__> owner(new escargot_backing_store__(nullptr));
        owner->value.reset(BackingStoreRef::createNonSharedBackingStore(data, length, deleter, callbackData));
        *out = owner.release();
        ++liveStores;
        return ESCARGOT_OK;
    });
}
void* escargot_backing_store_data(escargot_backing_store store) { return store ? store->value->data() : nullptr; }
size_t escargot_backing_store_size(escargot_backing_store store) { return store ? store->value->byteLength() : 0; }
void escargot_backing_store_release(escargot_backing_store store)
{
    if (store) {
        delete store;
        --liveStores;
    }
}
escargot_status escargot_array_buffer_create(escargot_context ctx, escargot_backing_store store, escargot_result* out)
{
    if (!store) {
        if (out)
            *out = nullptr;
        return ESCARGOT_INVALID_ARGUMENT;
    }
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { auto* buffer = ArrayBufferObjectRef::create(state); buffer->attachBuffer(store->value.get()); return buffer; });
}
escargot_status escargot_array_buffer_get_backing_store(escargot_value buffer, escargot_backing_store* out)
{
    if (out)
        *out = nullptr;
    if (!buffer || !out)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(buffer)->isArrayBuffer())
        return ESCARGOT_TYPE_ERROR;
    auto store = value(buffer)->asArrayBuffer()->backingStore();
    if (!store)
        return ESCARGOT_INVALID_STATE;
    return guard([&]() -> escargot_status { *out = new escargot_backing_store__(store.value()); ++liveStores; return ESCARGOT_OK; });
}
escargot_status escargot_array_buffer_detach(escargot_value buffer)
{
    if (!buffer)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(buffer)->isArrayBufferObject())
        return ESCARGOT_TYPE_ERROR;
    return guard([&]() -> escargot_status { value(buffer)->asArrayBufferObject()->detachArrayBuffer(); return ESCARGOT_OK; });
}
escargot_status escargot_promise_create(escargot_context ctx, escargot_result* out)
{
    return execute(ctx, out, [](ExecutionStateRef* state) -> ValueRef* { return PromiseObjectRef::create(state); });
}
escargot_status escargot_promise_resolve(escargot_context ctx, escargot_value promise, escargot_value v, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!promise || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(promise)->isPromiseObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { value(promise)->asPromiseObject()->fulfill(state, value(v)); return ValueRef::createUndefined(); });
}
escargot_status escargot_promise_reject(escargot_context ctx, escargot_value promise, escargot_value v, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!promise || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(promise)->isPromiseObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { value(promise)->asPromiseObject()->reject(state, value(v)); return ValueRef::createUndefined(); });
}
escargot_status escargot_object_define_data_property(escargot_context ctx, escargot_value object, escargot_value key, escargot_value v, int writable, int enumerable, int configurable, escargot_result* out)
{
    if (out)
        *out = nullptr;
    if (!object || !key || !v)
        return ESCARGOT_INVALID_ARGUMENT;
    if (!value(object)->isObject())
        return ESCARGOT_TYPE_ERROR;
    return execute(ctx, out, [&](ExecutionStateRef* state) -> ValueRef* { return ValueRef::create(value(object)->asObject()->defineDataProperty(state, value(key), value(v), writable, enumerable, configurable)); });
}
} // extern "C"
#endif // ENABLE_C_API
