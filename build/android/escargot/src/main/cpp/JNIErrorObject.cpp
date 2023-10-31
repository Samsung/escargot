/*
 * Copyright (c) 2023-present Samsung Electronics Co., Ltd
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

#include "EscargotJNI.h"

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptErrorObject_create(JNIEnv* env, jclass clazz,
                                                           jobject context, jobject kind,
                                                           jstring message)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(kind, "ErrorKind");
    THROW_NPE_RETURN_NULL(message, "String");

    auto nameMethod = env->GetMethodID(env->GetObjectClass(kind), "name", "()Ljava/lang/String;");
    auto kindName = (jstring)env->CallObjectMethod(kind, nameMethod);
    auto string = createStringFromJava(env, kindName);

    ErrorObjectRef::Code code = Escargot::ErrorObjectRef::None;
    if (string == "ReferenceError") {
        code = Escargot::ErrorObjectRef::ReferenceError;
    } else if (string == "TypeError") {
        code = Escargot::ErrorObjectRef::TypeError;
    } else if (string == "SyntaxError") {
        code = Escargot::ErrorObjectRef::SyntaxError;
    } else if (string == "RangeError") {
        code = Escargot::ErrorObjectRef::RangeError;
    } else if (string == "URIError") {
        code = Escargot::ErrorObjectRef::URIError;
    } else if (string == "EvalError") {
        code = Escargot::ErrorObjectRef::EvalError;
    } else if (string == "AggregateError") {
        code = Escargot::ErrorObjectRef::AggregateError;
    }

    StringRef* jsMessage = createJSStringFromJava(env, message);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state,
                                                                    ErrorObjectRef::Code code, StringRef* jsMessage) -> ValueRef* {
        return ErrorObjectRef::create(state, code, jsMessage);
    }, code, jsMessage);

    assert(evaluatorResult.isSuccessful());
    return createJavaObjectFromValue(env, evaluatorResult.result->asErrorObject());
}