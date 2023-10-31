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
#ifndef ESCARGOT_ANDROID_ESCARGOTJNI_H
#define ESCARGOT_ANDROID_ESCARGOTJNI_H

#include <jni.h>
#include <EscargotPublic.h>
#include <cassert>

using namespace Escargot;

#if defined(ANDROID)
#include <android/log.h>
#define  LOG_TAG    "Escargot"
#define  LOGUNK(...)  __android_log_print(ANDROID_LOG_UNKNOWN,LOG_TAG,__VA_ARGS__)
#define  LOGDEF(...)  __android_log_print(ANDROID_LOG_DEFAULT,LOG_TAG,__VA_ARGS__)
#define  LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGF(...)  __android_log_print(ANDROID_FATAL_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGS(...)  __android_log_print(ANDROID_SILENT_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGUNK(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGDEF(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGV(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGD(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGI(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGW(...)  fprintf(stdout,__VA_ARGS__)
#define  LOGE(...)  fprintf(stderr,__VA_ARGS__)
#define  LOGF(...)  fprintf(stderr,__VA_ARGS__)
#define  LOGS(...)  fprintf(stderr,__VA_ARGS__)
#endif

#define THROW_NPE_RETURN_NULL(param, paramType)                                                      \
    if (env->ExceptionCheck()) {                                                                     \
        return 0;                                                                                    \
    }                                                                                                \
    if (!param) {                                                                                    \
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), paramType" cannot be null"); \
        return 0;                                                                                    \
    }

#define THROW_NPE_RETURN_VOID(param, paramType)                                                      \
    if (env->ExceptionCheck()) {                                                                     \
        return;                                                                                      \
    }                                                                                                \
    if (!param) {                                                                                    \
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), paramType" cannot be null"); \
        return;                                                                                      \
    }

#define THROW_CAST_EXCEPTION_IF_NEEDS(param, value, typeName)                                        \
    if (env->ExceptionCheck()) {                                                                     \
        return NULL;                                                                                 \
    }                                                                                                \
    if (!value->is##typeName()) {                                                                    \
        env->ThrowNew(env->FindClass("java/lang/ClassCastException"), "Can not cast to " #typeName); \
        return NULL;                                                                                 \
    }

extern JavaVM* g_jvm;
extern size_t g_nonPointerValueLast;

jobject createJavaValueObject(JNIEnv* env, jclass clazz, ValueRef* value);
jobject createJavaValueObject(JNIEnv* env, const char* className, ValueRef* value);
jobject createJavaObject(JNIEnv* env, VMInstanceRef* value);
jobject createJavaObject(JNIEnv* env, ContextRef* value);

ValueRef* unwrapValueRefFromValue(JNIEnv* env, jclass clazz, jobject object);
jobject createJavaObjectFromValue(JNIEnv* env, ValueRef* value);
ValueRef* unwrapValueRefFromValue(JNIEnv* env, jclass clazz, jobject object);
OptionalRef<JNIEnv> fetchJNIEnvFromCallback();

std::string fetchStringFromJavaOptionalString(JNIEnv *env, jobject optional);
StringRef* createJSStringFromJava(JNIEnv* env, jstring str);
std::string createStringFromJava(JNIEnv* env, jstring str);
jstring createJavaStringFromJS(JNIEnv* env, StringRef* string);
void throwJavaRuntimeException(ExecutionStateRef* state);
jobject storeExceptionOnContextAndReturnsIt(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult);
jobject createOptionalValueFromEvaluatorJavaScriptValueResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult);
jobject createOptionalValueFromEvaluatorBooleanResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult);
jobject createOptionalValueFromEvaluatorIntegerResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult);
jobject createOptionalValueFromEvaluatorDoubleResult(JNIEnv* env, jobject contextObject, ContextRef* context, Evaluator::EvaluatorResult& evaluatorResult);

struct ScriptObjectExtraData {
    jobject userData;
    jobject implementSideData;

    ScriptObjectExtraData()
        : userData(nullptr)
        , implementSideData(nullptr)
    {
    }

    void* operator new(size_t t)
    {
        return Memory::gcMallocAtomic(sizeof(ScriptObjectExtraData));
    }
};

ScriptObjectExtraData* ensureScriptObjectExtraData(ObjectRef* ref);

template<typename T>
jobject nativeOptionalValueIntoJavaOptionalValue(JNIEnv* env, OptionalRef<T> ref)
{
    if (env->ExceptionCheck()) {
        return nullptr;
    }
    jclass optionalClazz = env->FindClass("java/util/Optional");
    if (ref) {
        jmethodID ctorMethod = env->GetStaticMethodID(optionalClazz, "of",
                                                      "(Ljava/lang/Object;)Ljava/util/Optional;");
        return env->CallStaticObjectMethod(optionalClazz, ctorMethod, createJavaObjectFromValue(env, ref.value()));
    }

    return env->CallStaticObjectMethod(optionalClazz, env->GetStaticMethodID(optionalClazz, "empty",
                                                                             "()Ljava/util/Optional;"));
}

template<typename NativeType>
PersistentRefHolder<NativeType>* getPersistentPointerFromJava(JNIEnv *env, jclass clazz, jobject object)
{
    auto ptr = env->GetLongField(object, env->GetFieldID(clazz, "m_nativePointer", "J"));
    PersistentRefHolder<NativeType>* pVMRef = reinterpret_cast<PersistentRefHolder<NativeType>*>(ptr);
    return pVMRef;
}

#endif //ESCARGOT_ANDROID_ESCARGOTJNI_H