#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotNapiTypes__
#define __EscargotNapiTypes__

#include "api/EscargotPublic.h"
#include "NapiEnv.h"

#include <node_api.h>

#include <string>

// exports each napi_* definition individually with default visibility,
// so the binary can be compiled with -fvisibility=hidden (the project default,
// see target.cmake) while still letting dlopen()'d addons resolve these
// specific symbols against this process (see ESCARGOT_EXPORT for the
// equivalent convention used by the rest of the public API).
#if !defined(ESCARGOT_NAPI_EXPORT)
#if defined(_MSC_VER)
#define ESCARGOT_NAPI_EXPORT __declspec(dllexport)
#else
#define ESCARGOT_NAPI_EXPORT __attribute__((visibility("default")))
#endif
#endif

namespace Escargot {
namespace Napi {

// napi_value/napi_ref/etc are opaque pointer types for ABI stability
// (`struct napi_value__*` and friends); nothing ever dereferences the
// pointee, so we punn them directly onto Escargot's own GC pointers instead
// of allocating a wrapper per value. This is safe because Escargot's GC
// (Boehm) never moves objects. This is a temporary simplification: proper
// handle-scope rooting still needs to be designed,
// so values are only valid while their originating ValueRef* is otherwise
// reachable (e.g. still on the interpreter stack).
inline napi_value ToNapi(ValueRef* value)
{
    return reinterpret_cast<napi_value>(value);
}

inline ValueRef* FromNapi(napi_value value)
{
    return reinterpret_cast<ValueRef*>(value);
}

// the real napi_env__ definition (only forward-declared by node_api.h)
struct EnvData {
    NapiEnv* napiEnv = nullptr;
    ExecutionStateRef* executionState = nullptr; // only valid for the duration of the current call
    OptionalRef<ValueRef> pendingException;
    std::string lastErrorMessage;
    napi_extended_error_info lastErrorInfo;

    ContextRef* context()
    {
        return napiEnv->context();
    }
};

// data stashed on a native FunctionObjectRef via setExtraData(), so the
// callback trampoline can find the user's napi_callback + data pointer
struct CallbackData {
    napi_env env;
    napi_callback callback;
    void* data;
};

} // namespace Napi
} // namespace Escargot

// the opaque type node_api.h forward-declares; napi_env is `EnvData*` in disguise
struct napi_env__ : public Escargot::Napi::EnvData {
};

// the opaque type node_api.h forward-declares for napi_get_cb_info et al.
struct napi_callback_info__ {
    size_t argc;
    Escargot::ValueRef** argv;
    Escargot::ValueRef* thisValue;
    void* data;
};

#endif
#endif // ENABLE_NAPI
