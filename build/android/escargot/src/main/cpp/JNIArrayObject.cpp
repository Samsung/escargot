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
Java_com_samsung_lwe_escargot_JavaScriptArrayObject_create(JNIEnv* env, jclass clazz,
                                                           jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state) -> ValueRef* {
        return ArrayObjectRef::create(state);
    });

    assert(evaluatorResult.isSuccessful());
    return createJavaObjectFromValue(env, evaluatorResult.result->asArrayObject());
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_samsung_lwe_escargot_JavaScriptArrayObject_length(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ArrayObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asArrayObject();

    int64_t length = 0;
    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ArrayObjectRef* thisValueRef, int64_t* pLength) -> ValueRef* {
        *pLength = static_cast<int64_t>(thisValueRef->length(state));
        return ValueRef::createUndefined();
    }, thisValueRef, &length);

    return length;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_JavaScriptArrayObject_setLength(JNIEnv* env, jobject thiz,
                                                              jobject context, jlong newLength)
{
    THROW_NPE_RETURN_VOID(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ArrayObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asArrayObject();

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ArrayObjectRef* thisValueRef, jlong pLength) -> ValueRef* {
        if (pLength >= 0) {
            thisValueRef->setLength(state, static_cast<uint64_t>(pLength));
        } else {
            thisValueRef->set(state, AtomicStringRef::create(state->context(), "length")->string(), ValueRef::create(pLength));
        }
        return ValueRef::createUndefined();
    }, thisValueRef, newLength);
}