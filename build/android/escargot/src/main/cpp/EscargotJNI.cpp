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

JavaVM* g_jvm;
size_t g_nonPointerValueLast = reinterpret_cast<size_t>(ValueRef::createUndefined());

jobject createJavaValueObject(JNIEnv* env, jclass clazz, ValueRef* value)
{
    jobject valueObject;
    if (!value->isStoredInHeap()) {
        valueObject = env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "(JZ)V"), reinterpret_cast<jlong>(value), jboolean(false));
    } else {
        PersistentRefHolder<ValueRef>* pRef = new PersistentRefHolder<ValueRef>(value);
        jlong ptr = reinterpret_cast<size_t>(pRef);
        valueObject = env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "(J)V"), ptr);
    }
    return valueObject;
}

jobject createJavaValueObject(JNIEnv* env, const char* className, ValueRef* value)
{
    return createJavaValueObject(env, env->FindClass(className), value);
}

ValueRef* unwrapValueRefFromValue(JNIEnv* env, jclass clazz, jobject object)
{
    auto ptr = env->GetLongField(object, env->GetFieldID(clazz, "m_nativePointer", "J"));
    if (static_cast<size_t>(ptr) <= g_nonPointerValueLast || (static_cast<size_t>(ptr) & 1)) {
        return reinterpret_cast<ValueRef*>(ptr);
    } else {
        PersistentRefHolder<ValueRef>* ref = reinterpret_cast<PersistentRefHolder<ValueRef>*>(ptr);
        return ref->get();
    }
}

jobject createJavaObjectFromValue(JNIEnv* env, ValueRef* value)
{
    if (!value->isStoredInHeap() || value->isNumber()) {
        return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptValue", value);
    } else if (value->isString()) {
        return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptString", value);
    } else if (value->isSymbol()) {
        return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptSymbol", value);
    } else if (value->isBigInt()) {
        return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptBigInt", value);
    } else if (value->isObject()) {
        if (value->isArrayObject()) {
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptArrayObject", value);
        } else if (value->isGlobalObject()) {
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptGlobalObject", value);
        } else if (value->isFunctionObject()) {
            if (value->asFunctionObject()->extraData()) {
                ScriptObjectExtraData* data = reinterpret_cast<ScriptObjectExtraData*>(value->asFunctionObject()->extraData());
                if (data->implementSideData) {
                    return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptJavaCallbackFunctionObject", value);
                }
            }
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptFunctionObject", value);
        } else if (value->isPromiseObject()) {
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptPromiseObject", value);
        } else if (value->isErrorObject()) {
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptErrorObject", value);
        } else {
            return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptObject", value);
        }
    } else {
        abort();
    }
}

jobject createJavaObject(JNIEnv* env, VMInstanceRef* value)
{
    PersistentRefHolder<VMInstanceRef>* pRef = new PersistentRefHolder<VMInstanceRef>(value);
    jlong ptr = reinterpret_cast<size_t>(pRef);
    jclass clazz = env->FindClass("com/samsung/lwe/escargot/VMInstance");
    return env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "(J)V"), ptr);
}

jobject createJavaObject(JNIEnv* env, ContextRef* value)
{
    PersistentRefHolder<ContextRef>* pRef = new PersistentRefHolder<ContextRef>(value);
    jlong ptr = reinterpret_cast<size_t>(pRef);
    jclass clazz = env->FindClass("com/samsung/lwe/escargot/Context");
    return env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "(J)V"), ptr);
}

OptionalRef<JNIEnv> fetchJNIEnvFromCallback()
{
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
#if defined(_JAVASOFT_JNI_H_) // oraclejdk or openjdk
        if (g_jvm->AttachCurrentThread(reinterpret_cast<void **>(&env), NULL) != 0) {
#else
        if (g_jvm->AttachCurrentThread(reinterpret_cast<JNIEnv **>(&env), NULL) != 0) {
#endif
            // give up
            return nullptr;
        }
    }
    return env;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Escargot_init(JNIEnv* env, jclass clazz)
{
    thread_local static bool inited = false;
    if (!inited) {
        if (!g_jvm) {
            env->GetJavaVM(&g_jvm);
        }
        inited = true;
    }
}

std::string fetchStringFromJavaOptionalString(JNIEnv *env, jobject optional)
{
    auto classOptionalString = env->GetObjectClass(optional);
    auto methodIsPresent = env->GetMethodID(classOptionalString, "isPresent", "()Z");
    if (env->CallBooleanMethod(optional, methodIsPresent)) {
        auto methodGet = env->GetMethodID(classOptionalString, "get", "()Ljava/lang/Object;");
        jboolean isSucceed;
        jstring value = static_cast<jstring>(env->CallObjectMethod(optional, methodGet));
        const char* str = env->GetStringUTFChars(
                value, &isSucceed);
        auto length = env->GetStringUTFLength(value);
        auto ret = std::string(str, length);
        env->ReleaseStringUTFChars(value, str);
        return ret;
    }
    return std::string();
}

StringRef* createJSStringFromJava(JNIEnv* env, jstring str)
{
    if (!str) {
        return StringRef::emptyString();
    }
    jboolean isSucceed;
    const char* cString = env->GetStringUTFChars(str, &isSucceed);
    StringRef* code = StringRef::createFromUTF8(cString, env->GetStringUTFLength(str));
    env->ReleaseStringUTFChars(str, cString);
    return code;
}

std::string createStringFromJava(JNIEnv* env, jstring str)
{
    if (!str) {
        return std::string();
    }
    jboolean isSucceed;
    const char* cString = env->GetStringUTFChars(str, &isSucceed);
    std::string ret = std::string(cString, env->GetStringUTFLength(str));
    env->ReleaseStringUTFChars(str, cString);
    return ret;
}

jstring createJavaStringFromJS(JNIEnv* env, StringRef* string)
{
    std::basic_string<uint16_t> buf;
    auto bad = string->stringBufferAccessData();
    buf.reserve(bad.length);

    for (size_t i = 0; i < bad.length ; i ++) {
        buf.push_back(bad.charAt(i));
    }

    return env->NewString(buf.data(), buf.length());
}

void throwJavaRuntimeException(ExecutionStateRef* state)
{
    state->throwException(ErrorObjectRef::create(state, ErrorObjectRef::None, StringRef::createFromASCII("Java runtime exception")));
}

bool hasJavaScriptRuntimeExceptionOnEnv(JNIEnv* env)
{
    if (env->ExceptionCheck()) {
        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();
        bool ret = env->IsInstanceOf(exception, env->FindClass("com/samsung/lwe/escargot/internal/JavaScriptRuntimeException"));
        env->Throw(exception);
        return ret;
    }
    return false;
}

OptionalRef<ValueRef> extractExceptionFromEnv(JNIEnv* env)
{
    if (env->ExceptionCheck()) {
        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();
        bool ret = env->IsInstanceOf(exception, env->FindClass("com/samsung/lwe/escargot/internal/JavaScriptRuntimeException"));
        if (ret) {
            jclass clz = env->FindClass("com/samsung/lwe/escargot/internal/JavaScriptRuntimeException");
            jobject jv = env->CallObjectMethod(exception, env->GetMethodID(clz, "exception", "()Lcom/samsung/lwe/escargot/JavaScriptValue;"));
            ValueRef* v = unwrapValueRefFromValue(env, env->GetObjectClass(jv), jv);
            return v;
        } else {
            env->Throw(exception);
        }
    }

    return nullptr;
}

bool hasJavaExcpetion(JNIEnv* env)
{
    if (env->ExceptionCheck() && !hasJavaScriptRuntimeExceptionOnEnv(env)) {
        return true;
    }
    return false;
}

jobject storeExceptionOnContextAndReturnsIt(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult)
{
    if (hasJavaExcpetion(env)) {
        return nullptr;
    }
    auto exceptionOnEnv = extractExceptionFromEnv(env);
    jclass optionalClazz = env->FindClass("java/util/Optional");
    // store exception to context
    auto fieldId = env->GetFieldID(env->GetObjectClass(contextObject), "m_lastThrownException", "Ljava/util/Optional;");
    ValueRef* exception = exceptionOnEnv ? exceptionOnEnv.get() : evaluatorResult.error.value();
    auto fieldValue = env->CallStaticObjectMethod(optionalClazz,
                                                  env->GetStaticMethodID(optionalClazz, "of",
                                                                         "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                                  createJavaObjectFromValue(env, exception));
    env->SetObjectField(contextObject, fieldId, fieldValue);

    return env->CallStaticObjectMethod(optionalClazz, env->GetStaticMethodID(optionalClazz, "empty",
                                                                             "()Ljava/util/Optional;"));
}

jobject createOptionalValueFromEvaluatorJavaScriptValueResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult)
{
    if (hasJavaExcpetion(env)) {
        return nullptr;
    }
    if (evaluatorResult.isSuccessful()) {
        jclass optionalClazz = env->FindClass("java/util/Optional");
        return env->CallStaticObjectMethod(optionalClazz,
                                           env->GetStaticMethodID(optionalClazz, "of",
                                                                  "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                           createJavaObjectFromValue(env, evaluatorResult.result));
    }

    return storeExceptionOnContextAndReturnsIt(env, contextObject, context, evaluatorResult);
}

jobject createOptionalValueFromEvaluatorBooleanResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult)
{
    if (hasJavaExcpetion(env)) {
        return nullptr;
    }
    if (evaluatorResult.isSuccessful()) {
        jclass optionalClazz = env->FindClass("java/util/Optional");
        auto booleanClazz = env->FindClass("java/lang/Boolean");
        auto valueOfMethodId = env->GetStaticMethodID(booleanClazz, "valueOf", "(Z)Ljava/lang/Boolean;");
        auto javaBoolean = env->CallStaticObjectMethod(booleanClazz, valueOfMethodId, (jboolean)evaluatorResult.result->asBoolean());
        return env->CallStaticObjectMethod(optionalClazz,
                                           env->GetStaticMethodID(optionalClazz, "of",
                                                                  "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                           javaBoolean);
    }

    return storeExceptionOnContextAndReturnsIt(env, contextObject, context, evaluatorResult);
}

jobject createOptionalValueFromEvaluatorIntegerResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult)
{
    if (hasJavaExcpetion(env)) {
        return nullptr;
    }
    if (evaluatorResult.isSuccessful()) {
        jclass optionalClazz = env->FindClass("java/util/Optional");
        auto containerClass = env->FindClass("java/lang/Integer");
        auto valueOfMethodId = env->GetStaticMethodID(containerClass, "valueOf", "(I)Ljava/lang/Integer;");
        auto javaValue = env->CallStaticObjectMethod(containerClass, valueOfMethodId, (jint)evaluatorResult.result->asInt32());
        return env->CallStaticObjectMethod(optionalClazz,
                                           env->GetStaticMethodID(optionalClazz, "of",
                                                                  "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                           javaValue);
    }

    return storeExceptionOnContextAndReturnsIt(env, contextObject, context, evaluatorResult);
}

jobject createOptionalValueFromEvaluatorDoubleResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult)
{
    if (hasJavaExcpetion(env)) {
        return nullptr;
    }
    if (evaluatorResult.isSuccessful()) {
        jclass optionalClazz = env->FindClass("java/util/Optional");
        auto containerClass = env->FindClass("java/lang/Double");
        auto valueOfMethodId = env->GetStaticMethodID(containerClass, "valueOf", "(D)Ljava/lang/Double;");
        auto javaValue = env->CallStaticObjectMethod(containerClass, valueOfMethodId, (jdouble)evaluatorResult.result->asNumber());
        return env->CallStaticObjectMethod(optionalClazz,
                                           env->GetStaticMethodID(optionalClazz, "of",
                                                                  "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                           javaValue);
    }

    return storeExceptionOnContextAndReturnsIt(env, contextObject, context, evaluatorResult);
}

ScriptObjectExtraData* ensureScriptObjectExtraData(ObjectRef* ref)
{
    ScriptObjectExtraData* data = reinterpret_cast<ScriptObjectExtraData*>(ref->extraData());
    if (!data) {
        data = new ScriptObjectExtraData;
        ref->setExtraData(data);

        Memory::gcRegisterFinalizer(ref, [](void* self) {
            ScriptObjectExtraData* extraData = reinterpret_cast<ScriptObjectExtraData*>(reinterpret_cast<ObjectRef*>(self)->extraData());
            auto env = fetchJNIEnvFromCallback();
            if (env) {
                if (extraData->implementSideData) {
                    env->DeleteGlobalRef(extraData->implementSideData);
                    extraData->implementSideData = nullptr;
                }
                if (extraData->userData) {
                    env->DeleteGlobalRef(extraData->userData);
                    extraData->userData = nullptr;
                }
            }
        });

    }
    return data;
}