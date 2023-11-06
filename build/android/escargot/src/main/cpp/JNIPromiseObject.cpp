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
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_create(JNIEnv* env, jclass clazz,
                                                             jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state) -> ValueRef* {
        return PromiseObjectRef::create(state);
    });

    assert(evaluatorResult.isSuccessful());
    return createJavaObjectFromValue(env, evaluatorResult.result->asPromiseObject());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_state(JNIEnv* env, jobject thiz)
{
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();

    jclass enumClass = env->FindClass(
            "com/samsung/lwe/escargot/JavaScriptPromiseObject$PromiseState");
    jfieldID enumFieldID;
    PromiseObjectRef::PromiseState state = thisValueRef->state();
    if (state == PromiseObjectRef::Pending) {
        enumFieldID = env->GetStaticFieldID(enumClass, "Pending",
                                            "Lcom/samsung/lwe/escargot/JavaScriptPromiseObject$PromiseState;");
    } else if (state == PromiseObjectRef::FulFilled) {
        enumFieldID = env->GetStaticFieldID(enumClass, "FulFilled",
                                            "Lcom/samsung/lwe/escargot/JavaScriptPromiseObject$PromiseState;");
    } else {
        enumFieldID = env->GetStaticFieldID(enumClass, "Rejected",
                                            "Lcom/samsung/lwe/escargot/JavaScriptPromiseObject$PromiseState;");
    }
    return env->GetStaticObjectField(enumClass, enumFieldID);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_promiseResult(JNIEnv* env, jobject thiz)
{
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    return createJavaObjectFromValue(env, thisValueRef->promiseResult());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_then__Lcom_samsung_lwe_escargot_Context_2Lcom_samsung_lwe_escargot_JavaScriptValue_2(
        JNIEnv* env, jobject thiz, jobject context, jobject handler)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(handler, "JavaScriptValue");
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    ValueRef* handlerRef = unwrapValueRefFromValue(env, env->GetObjectClass(handler), handler);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, PromiseObjectRef* promiseObject, ValueRef* handlerRef) -> ValueRef* {
        return promiseObject->then(state, handlerRef);
    }, thisValueRef, handlerRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_then__Lcom_samsung_lwe_escargot_Context_2Lcom_samsung_lwe_escargot_JavaScriptValue_2Lcom_samsung_lwe_escargot_JavaScriptValue_2(
        JNIEnv* env, jobject thiz, jobject context, jobject onFulfilled, jobject onRejected)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(onFulfilled, "JavaScriptValue");
    THROW_NPE_RETURN_NULL(onRejected, "JavaScriptValue");

    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    ValueRef* onFulfilledRef = unwrapValueRefFromValue(env, env->GetObjectClass(onFulfilled), onFulfilled);
    ValueRef* onRejectedRef = unwrapValueRefFromValue(env, env->GetObjectClass(onRejected), onRejected);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, PromiseObjectRef* promiseObject, ValueRef* onFulfilledRef, ValueRef* onRejectedRef) -> ValueRef* {
        return promiseObject->then(state, onFulfilledRef, onRejectedRef);
    }, thisValueRef, onFulfilledRef, onRejectedRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_catchOperation(JNIEnv* env, jobject thiz,
                                                                     jobject context,
                                                                     jobject handler)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(handler, "JavaScriptValue");
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    ValueRef* handlerRef = unwrapValueRefFromValue(env, env->GetObjectClass(handler), handler);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, PromiseObjectRef* promiseObject, ValueRef* handlerRef) -> ValueRef* {
        return promiseObject->catchOperation(state, handlerRef);
    }, thisValueRef, handlerRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_fulfill(JNIEnv* env, jobject thiz,
                                                              jobject context, jobject value)
{
    THROW_NPE_RETURN_VOID(context, "Context");
    THROW_NPE_RETURN_VOID(value, "JavaScriptValue");
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    ValueRef* valueRef = unwrapValueRefFromValue(env, env->GetObjectClass(value), value);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, PromiseObjectRef* promiseObject, ValueRef* valueRef) -> ValueRef* {
        promiseObject->fulfill(state, valueRef);
        return ValueRef::createUndefined();
    }, thisValueRef, valueRef);
    assert(evaluatorResult.isSuccessful());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_reject(JNIEnv* env, jobject thiz,
                                                             jobject context, jobject reason)
{
    THROW_NPE_RETURN_VOID(context, "Context");
    THROW_NPE_RETURN_VOID(reason, "JavaScriptValue");
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    ValueRef* reasonRef = unwrapValueRefFromValue(env, env->GetObjectClass(reason), reason);

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, PromiseObjectRef* promiseObject, ValueRef* reasonRef) -> ValueRef* {
        promiseObject->reject(state, reasonRef);
        return ValueRef::createUndefined();
    }, thisValueRef, reasonRef);
    assert(evaluatorResult.isSuccessful());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptPromiseObject_hasHandler(JNIEnv* env, jobject thiz)
{
    PromiseObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asPromiseObject();
    return thisValueRef->hasResolveHandlers() || thisValueRef->hasRejectHandlers();
}