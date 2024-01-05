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
Java_com_samsung_lwe_escargot_JavaScriptSymbol_create(JNIEnv* env, jclass clazz, jobject value)
{
    OptionalRef<StringRef> descString;
    if (value) {
        auto classOptionalJavaScriptString = env->GetObjectClass(value);
        auto methodIsPresent = env->GetMethodID(classOptionalJavaScriptString, "isPresent", "()Z");
        if (env->CallBooleanMethod(value, methodIsPresent)) {
            auto methodGet = env->GetMethodID(classOptionalJavaScriptString, "get", "()Ljava/lang/Object;");
            jboolean isSucceed;
            jobject javaObjectValue = env->CallObjectMethod(value, methodGet);
            descString = unwrapValueRefFromValue(env, env->GetObjectClass(javaObjectValue), javaObjectValue)->asString();
        }
    }

    return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptSymbol", SymbolRef::create(descString));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptSymbol_fromGlobalSymbolRegistry(JNIEnv* env, jclass clazz,
                                                                        jobject vm,
                                                                        jobject stringKey)
{
    THROW_NPE_RETURN_NULL(vm, "VMInstance");
    THROW_NPE_RETURN_NULL(stringKey, "JavaScriptString");

    auto ptr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(vm), vm);
    auto key = unwrapValueRefFromValue(env, env->GetObjectClass(stringKey), stringKey);

    auto symbol = SymbolRef::fromGlobalSymbolRegistry(ptr->get(), key->asString());
    return createJavaObjectFromValue(env, symbol);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptSymbol_descriptionString(JNIEnv* env, jobject thiz)
{
    auto desc = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asSymbol()->descriptionString();
    return createJavaObjectFromValue(env, desc);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptSymbol_descriptionValue(JNIEnv* env, jobject thiz)
{
    auto desc = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asSymbol()->descriptionValue();
    return createJavaObjectFromValue(env, desc);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptSymbol_symbolDescriptiveString(JNIEnv* env, jobject thiz)
{
    auto desc = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asSymbol()->symbolDescriptiveString();
    return createJavaObjectFromValue(env, desc);
}
