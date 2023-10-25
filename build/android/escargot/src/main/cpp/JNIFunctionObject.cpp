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
Java_com_samsung_lwe_escargot_JavaScriptFunctionObject_context(JNIEnv* env, jobject thiz)
{
    FunctionObjectRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->asFunctionObject();
    return createJavaObject(env, thisValueRef->context());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptJavaCallbackFunctionObject_create(JNIEnv* env, jclass clazz,
                                                                          jobject context,
                                                                          jstring functionName,
                                                                          jint argumentCount,
                                                                          jboolean isConstructor,
                                                                          jobject callback)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(callback, "Callback");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context),
                                                               context);

    FunctionObjectRef::NativeFunctionInfo info(
            AtomicStringRef::create(contextRef->get(), createJSStringFromJava(env, functionName)),
            [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall) -> ValueRef* {
                auto env = fetchJNIEnvFromCallback();
                if (!env) {
                    LOGE("failed to fetch env from function callback");
                    return ValueRef::createUndefined();
                }

                if (env->ExceptionCheck()) {
                    throwJavaRuntimeException(state);
                    return ValueRef::createUndefined();
                }

                env->PushLocalFrame(32);
                jobject callback = ensureScriptObjectExtraData(state->resolveCallee().get())->implementSideData;
                auto callbackMethodId = env->GetMethodID(env->GetObjectClass(callback), "callback",
                                                         "(Lcom/samsung/lwe/escargot/Context;Lcom/samsung/lwe/escargot/JavaScriptValue;[Lcom/samsung/lwe/escargot/JavaScriptValue;)Ljava/util/Optional;");

                jobjectArray javaArgv = env->NewObjectArray(argc, env->FindClass("com/samsung/lwe/escargot/JavaScriptValue"), nullptr);

                for (size_t i = 0; i < argc; i ++) {
                    auto ref = createJavaObjectFromValue(env.get(), argv[i]);
                    env->SetObjectArrayElement(javaArgv, i, ref);
                    env->DeleteLocalRef(ref);
                }
                jobject returnValue = env->CallObjectMethod(
                        callback,
                        callbackMethodId,
                        createJavaObject(env.get(), state->resolveCallee()->context()),
                        createJavaObjectFromValue(env.get(), thisValue),
                        javaArgv
                );

                if (env->ExceptionCheck()) {
                    env->PopLocalFrame(NULL);
                    throwJavaRuntimeException(state);
                    return ValueRef::createUndefined();
                }

                ValueRef* nativeReturnValue = ValueRef::createUndefined();
                if (returnValue) {
                    auto classOptional = env->GetObjectClass(returnValue);
                    auto methodIsPresent = env->GetMethodID(classOptional, "isPresent", "()Z");
                    if (env->CallBooleanMethod(returnValue, methodIsPresent)) {
                        auto methodGet = env->GetMethodID(classOptional, "get", "()Ljava/lang/Object;");
                        jobject callbackReturnValue = env->CallObjectMethod(returnValue, methodGet);
                        nativeReturnValue = unwrapValueRefFromValue(env.get(), env->GetObjectClass(callbackReturnValue), callbackReturnValue);
                    }
                }
                env->PopLocalFrame(NULL);
                return nativeReturnValue;
            },
            argumentCount,
            isConstructor);

    auto evaluatorResult = Evaluator::execute(contextRef->get(),
                                              [](ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info) -> ValueRef* {
                                                  return FunctionObjectRef::create(state, info);
                                              }, info);

    assert(evaluatorResult.isSuccessful());

    callback = env->NewGlobalRef(callback);
    FunctionObjectRef* fn = evaluatorResult.result->asFunctionObject();
    ensureScriptObjectExtraData(fn)->implementSideData = callback;
    return createJavaValueObject(env, "com/samsung/lwe/escargot/JavaScriptJavaCallbackFunctionObject", fn);
}