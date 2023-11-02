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
Java_com_samsung_lwe_escargot_Context_create(JNIEnv* env, jclass clazz, jobject vmInstance)
{
    THROW_NPE_RETURN_NULL(vmInstance, "VMInstance");

    auto vmPtr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(vmInstance),
                                                             vmInstance);
    auto contextRef = ContextRef::create(vmPtr->get());
    return createJavaObject(env, contextRef.get());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_Context_getGlobalObject(JNIEnv* env, jobject thiz)
{
    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(thiz), thiz);
    return createJavaObjectFromValue(env, contextRef->get()->globalObject());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_Context_throwException(JNIEnv* env, jobject thiz, jobject exception)
{
    THROW_NPE_RETURN_NULL(exception, "JavaScriptValue");
    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(thiz), thiz);
    if (contextRef->get()->canThrowException()) {
        jclass clz = env->FindClass("com/samsung/lwe/escargot/internal/JavaScriptRuntimeException");
        jobject obj = env->NewObject(clz, env->GetMethodID(clz, "<init>", "(Lcom/samsung/lwe/escargot/JavaScriptValue;)V"), exception);
        env->Throw(static_cast<jthrowable>(obj));
    }
    return false;
}