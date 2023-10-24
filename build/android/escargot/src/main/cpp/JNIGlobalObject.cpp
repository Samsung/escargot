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
Java_com_samsung_lwe_escargot_JavaScriptGlobalObject_jsonStringify(JNIEnv* env, jobject thiz,
                                                                   jobject context, jobject input)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(input, "JavaScriptValue");

    auto globalObjectRef = getPersistentPointerFromJava<GlobalObjectRef>(env, env->GetObjectClass(context), thiz);
    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* inputValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(input), input);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, GlobalObjectRef* globalObject, ValueRef* inputValueRef) -> ValueRef* {
        return globalObject->jsonStringify()->call(state, globalObject->json(), 1, &inputValueRef);
    }, globalObjectRef->get(), inputValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptGlobalObject_jsonParse(JNIEnv* env, jobject thiz,
                                                               jobject context, jobject input)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(input, "JavaScriptValue");

    auto globalObjectRef = getPersistentPointerFromJava<GlobalObjectRef>(env, env->GetObjectClass(context), thiz);
    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* inputValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(input), input);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, GlobalObjectRef* globalObject, ValueRef* inputValueRef) -> ValueRef* {
        return globalObject->jsonParse()->call(state, globalObject->json(), 1, &inputValueRef);
    }, globalObjectRef->get(), inputValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}