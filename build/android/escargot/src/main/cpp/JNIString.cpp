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
Java_com_samsung_lwe_escargot_JavaScriptString_create(JNIEnv* env, jclass clazz, jstring value)
{
    return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptString", createJSStringFromJava(env, value));
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_samsung_lwe_escargot_JavaScriptString_toJavaString(JNIEnv* env, jobject thiz)
{
    StringRef* string = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asString();
    return createJavaStringFromJS(env, string);
}