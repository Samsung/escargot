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

#include "NapiTypes.h"

#include <cstring>

namespace Escargot {
namespace Napi {

// shared by napi_create_error/napi_create_type_error/napi_create_range_error/
// node_api_create_syntax_error: builds an ErrorObjectRef of the given `code`
// (ErrorObjectRef::Code, not to be confused with the napi_value `code`
// parameter below) from `msg`, then - mirroring napi_throw_error - sets the
// object's ".code" property to `codeValue`'s string contents when non-null.
// Constructing an ErrorObjectRef never throws, so no Evaluator::execute
// wrapping is needed here.
static napi_status CreateErrorWithCode(napi_env env, napi_value codeValue, napi_value msg, ErrorObjectRef::Code code, napi_value* result)
{
    ExecutionStateRef* state = env->executionState;
    StringRef* message = FromNapi(msg)->asString();
    ErrorObjectRef* error = ErrorObjectRef::create(state, code, message);
    if (codeValue != nullptr) {
        StringRef* codeString = FromNapi(codeValue)->asString();
        error->set(state, StringRef::createFromASCII("code"), codeString);
    }
    *result = ToNapi(error);
    return napi_ok;
}

// shared by napi_throw_type_error/napi_throw_range_error/
// node_api_throw_syntax_error: mirrors napi_throw_error (NapiFunctions.cpp)
// exactly, storing the newly created error into env->pendingException instead
// of throwing across the API boundary.
static napi_status ThrowErrorWithCode(napi_env env, const char* code, const char* msg, ErrorObjectRef::Code errorCode)
{
    ExecutionStateRef* state = env->executionState;
    StringRef* message = StringRef::createFromUTF8(msg, strlen(msg));
    ErrorObjectRef* error = ErrorObjectRef::create(state, errorCode, message);
    if (code) {
        error->set(state, StringRef::createFromASCII("code"), StringRef::createFromUTF8(code, strlen(code)));
    }
    env->pendingException = error;
    return napi_ok;
}

extern "C" {

ESCARGOT_NAPI_EXPORT napi_status napi_create_error(napi_env env, napi_value code, napi_value msg, napi_value* result)
{
    return CreateErrorWithCode(env, code, msg, ErrorObjectRef::Code::None, result);
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_type_error(napi_env env, napi_value code, napi_value msg, napi_value* result)
{
    return CreateErrorWithCode(env, code, msg, ErrorObjectRef::Code::TypeError, result);
}

ESCARGOT_NAPI_EXPORT napi_status napi_create_range_error(napi_env env, napi_value code, napi_value msg, napi_value* result)
{
    return CreateErrorWithCode(env, code, msg, ErrorObjectRef::Code::RangeError, result);
}

ESCARGOT_NAPI_EXPORT napi_status node_api_create_syntax_error(napi_env env, napi_value code, napi_value msg, napi_value* result)
{
    return CreateErrorWithCode(env, code, msg, ErrorObjectRef::Code::SyntaxError, result);
}

ESCARGOT_NAPI_EXPORT napi_status napi_throw_type_error(napi_env env, const char* code, const char* msg)
{
    return ThrowErrorWithCode(env, code, msg, ErrorObjectRef::Code::TypeError);
}

ESCARGOT_NAPI_EXPORT napi_status napi_throw_range_error(napi_env env, const char* code, const char* msg)
{
    return ThrowErrorWithCode(env, code, msg, ErrorObjectRef::Code::RangeError);
}

ESCARGOT_NAPI_EXPORT napi_status node_api_throw_syntax_error(napi_env env, const char* code, const char* msg)
{
    return ThrowErrorWithCode(env, code, msg, ErrorObjectRef::Code::SyntaxError);
}

ESCARGOT_NAPI_EXPORT napi_status napi_is_error(napi_env env, napi_value value, bool* result)
{
    *result = FromNapi(value)->isErrorObject();
    return napi_ok;
}

} // extern "C"

} // namespace Napi
} // namespace Escargot

#endif // ENABLE_NAPI
