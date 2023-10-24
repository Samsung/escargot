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
Java_com_samsung_lwe_escargot_JavaScriptBigInt_create__I(JNIEnv* env, jclass clazz, jint num)
{
    return createJavaValueObject(env, clazz, BigIntRef::create(static_cast<int64_t>(num)));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_create__J(JNIEnv* env, jclass clazz, jlong num)
{
    return createJavaValueObject(env, clazz, BigIntRef::create(static_cast<int64_t>(num)));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_create__Ljava_lang_String_2I(JNIEnv* env,
                                                                            jclass clazz,
                                                                            jstring numString,
                                                                            jint radix)
{
    return createJavaValueObject(env, clazz,
                                 BigIntRef::create(createJSStringFromJava(env, numString), radix));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_create__Lcom_samsung_lwe_escargot_JavaScriptString_2I(
        JNIEnv* env, jclass clazz, jobject numString, jint radix)
{
    if (numString) {
        return createJavaValueObject(env, clazz,
                                     BigIntRef::create(unwrapValueRefFromValue(env, env->GetObjectClass(
                                             numString), numString)->asString(), radix));
    } else {
        return createJavaValueObject(env, clazz, BigIntRef::create(static_cast<int64_t>(0)));
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_toString(JNIEnv* env, jobject thiz, jint radix)
{
    return createJavaObjectFromValue(env, unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asBigInt()->toString(radix));
}

extern "C"
JNIEXPORT jdouble JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_toNumber(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asBigInt()->toNumber();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_samsung_lwe_escargot_JavaScriptBigInt_toInt64(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asBigInt()->toInt64();
}