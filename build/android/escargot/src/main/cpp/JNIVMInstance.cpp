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
Java_com_samsung_lwe_escargot_VMInstance_create(JNIEnv *env, jclass clazz, jobject locale,
                                                jobject timezone)
{
    THROW_NPE_RETURN_NULL(locale, "Optional<String>");
    THROW_NPE_RETURN_NULL(timezone, "Optional<String>");

    std::string localeString = fetchStringFromJavaOptionalString(env, locale);
    std::string timezoneString = fetchStringFromJavaOptionalString(env, timezone);

    auto vmRef = VMInstanceRef::create(localeString.length() ? localeString.data() : nullptr,
                                       timezoneString.length() ? timezoneString.data() : nullptr);
    return createJavaObject(env, vmRef.get());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_VMInstance_hasPendingJob(JNIEnv* env, jobject thiz)
{
    auto vmPtr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(thiz),
                                                             thiz);
    return vmPtr->get()->hasPendingJob();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_VMInstance_executePendingJob(JNIEnv* env, jobject thiz)
{
    auto vmPtr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(thiz),
                                                             thiz);
    if (vmPtr->get()->hasPendingJob()) {
        vmPtr->get()->executePendingJob();
    }
}