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
Java_com_samsung_lwe_escargot_JavaScriptObject_create(JNIEnv* env, jclass clazz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state) -> ValueRef* {
        return ObjectRef::create(state);
    });

    assert(evaluatorResult.isSuccessful());
    return createJavaObjectFromValue(env, evaluatorResult.result->asObject());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptObject_get(JNIEnv* env, jobject thiz, jobject context,
                                                   jobject propertyName)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(propertyName, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asObject();
    ValueRef* propertyNameValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(propertyName), propertyName);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ObjectRef* thisValueRef, ValueRef* propertyNameValueRef) -> ValueRef* {
        return thisValueRef->get(state, propertyNameValueRef);
    }, thisValueRef, propertyNameValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptObject_set(JNIEnv* env, jobject thiz, jobject context,
                                                   jobject propertyName, jobject value)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(propertyName, "JavaScriptValue");
    THROW_NPE_RETURN_NULL(value, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asObject();
    ValueRef* propertyNameValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(propertyName), propertyName);
    ValueRef* valueRef = unwrapValueRefFromValue(env, env->GetObjectClass(value), value);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ObjectRef* thisValueRef, ValueRef* propertyNameValueRef, ValueRef* valueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->set(state, propertyNameValueRef, valueRef));
    }, thisValueRef, propertyNameValueRef, valueRef);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptObject_defineDataProperty(JNIEnv* env, jobject thiz,
                                                                  jobject context,
                                                                  jobject propertyName,
                                                                  jobject value,
                                                                  jboolean isWritable,
                                                                  jboolean isEnumerable,
                                                                  jboolean isConfigurable)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(propertyName, "JavaScriptValue");
    THROW_NPE_RETURN_NULL(value, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asObject();
    ValueRef* propertyNameValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(propertyName), propertyName);
    ValueRef* valueRef = unwrapValueRefFromValue(env, env->GetObjectClass(value), value);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ObjectRef* thisValueRef, ValueRef* propertyNameValueRef, ValueRef* valueRef,
                                                                    jboolean isWritable,
                                                                    jboolean isEnumerable,
                                                                    jboolean isConfigurable) -> ValueRef* {
        return ValueRef::create(thisValueRef->defineDataProperty(state, propertyNameValueRef, valueRef, isWritable, isEnumerable, isConfigurable));
    }, thisValueRef, propertyNameValueRef, valueRef, isWritable, isEnumerable, isConfigurable);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptObject_getOwnProperty(JNIEnv* env, jobject thiz,
                                                              jobject context,
                                                              jobject propertyName)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(propertyName, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asObject();
    ValueRef* propertyNameValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(propertyName), propertyName);

    auto evaluatorResult = Evaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ObjectRef* thisValueRef, ValueRef* propertyNameValueRef) -> ValueRef* {
        return thisValueRef->getOwnProperty(state, propertyNameValueRef);
    }, thisValueRef, propertyNameValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}