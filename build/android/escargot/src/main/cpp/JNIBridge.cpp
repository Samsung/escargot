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
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_Bridge_register(JNIEnv* env, jclass clazz, jobject context,
                                              jstring objectName, jstring propertyName,
                                              jobject adapter)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(adapter, "Adapter");

    auto contextPtr = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context),
                                                               context);
    auto jsObjectName = createJSStringFromJava(env, objectName);
    auto jsPropertyName = createJSStringFromJava(env, propertyName);

    if (!jsObjectName->length() || !jsPropertyName->length()) {
        return 0;
    }

    adapter = env->NewGlobalRef(adapter);

    auto evalResult = Evaluator::execute(contextPtr->get(),
                                         [](ExecutionStateRef* state, JNIEnv* env, jobject adapter,
                                            StringRef* jsObjectName,
                                            StringRef* jsPropertyName) -> ValueRef* {
                                             auto globalObject = state->context()->globalObject();
                                             ObjectRef* targetObject;

                                             ValueRef* willBeTargetObject = globalObject->getOwnProperty(
                                                     state, jsObjectName);
                                             if (willBeTargetObject->isObject()) {
                                                 targetObject = willBeTargetObject->asObject();
                                             } else {
                                                 targetObject = ObjectRef::create(state);
                                                 globalObject->defineDataProperty(state,
                                                                                  jsObjectName,
                                                                                  targetObject,
                                                                                  true, true, true);
                                             }

                                             FunctionObjectRef::NativeFunctionInfo info(
                                                     AtomicStringRef::emptyAtomicString(),
                                                     [](ExecutionStateRef* state,
                                                        ValueRef* thisValue, size_t argc,
                                                        ValueRef** argv,
                                                        bool isConstructorCall) -> ValueRef* {
                                                         FunctionObjectRef* callee = state->resolveCallee().get();
                                                         jobject jo = static_cast<jobject>(reinterpret_cast<FunctionObjectRef*>(callee)->extraData());
                                                         auto env = fetchJNIEnvFromCallback();
                                                         if (!env) {
                                                             // give up
                                                             LOGE("could not fetch env from callback");
                                                             return ValueRef::createUndefined();
                                                         }

                                                         if (env->ExceptionCheck()) {
                                                             throwJavaRuntimeException(state);
                                                             return ValueRef::createUndefined();
                                                         }

                                                         env->PushLocalFrame(32);

                                                         jobject callbackArg;
                                                         jclass optionalClazz = env->FindClass(
                                                                 "java/util/Optional");
                                                         if (argc) {
                                                             callbackArg = env->CallStaticObjectMethod(
                                                                     optionalClazz,
                                                                     env->GetStaticMethodID(
                                                                             optionalClazz, "of",
                                                                             "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                                                     createJavaObjectFromValue(env.get(), argv[0]));
                                                         } else {
                                                             callbackArg = env->CallStaticObjectMethod(
                                                                     optionalClazz,
                                                                     env->GetStaticMethodID(
                                                                             optionalClazz, "empty",
                                                                             "()Ljava/util/Optional;"));
                                                         }
                                                         auto javaReturnValue = env->CallObjectMethod(
                                                                 jo,
                                                                 env->GetMethodID(
                                                                         env->GetObjectClass(jo),
                                                                         "callback",
                                                                         "(Lcom/samsung/lwe/escargot/Context;Ljava/util/Optional;)Ljava/util/Optional;"),
                                                                 createJavaObject(env.get(), callee->context()),
                                                                 callbackArg);

                                                         if (env->ExceptionCheck()) {
                                                             env->PopLocalFrame(NULL);
                                                             throwJavaRuntimeException(state);
                                                             return ValueRef::createUndefined();
                                                         }

                                                         auto methodIsPresent = env->GetMethodID(
                                                                 optionalClazz, "isPresent", "()Z");
                                                         ValueRef* nativeReturnValue = ValueRef::createUndefined();
                                                         if (javaReturnValue && env->CallBooleanMethod(javaReturnValue,
                                                                                                       methodIsPresent)) {
                                                             auto methodGet = env->GetMethodID(
                                                                     optionalClazz, "get",
                                                                     "()Ljava/lang/Object;");
                                                             jobject value = env->CallObjectMethod(
                                                                     javaReturnValue, methodGet);
                                                             nativeReturnValue = unwrapValueRefFromValue(
                                                                     env.get(),
                                                                     env->GetObjectClass(value),
                                                                     value);
                                                         }

                                                         env->PopLocalFrame(NULL);
                                                         return nativeReturnValue;
                                                     }, 1, true, false);
                                             FunctionObjectRef* callback = FunctionObjectRef::create(
                                                     state, info);
                                             targetObject->defineDataProperty(state, jsPropertyName,
                                                                              callback, true, true,
                                                                              true);
                                             return callback;
                                         }, env, adapter, jsObjectName, jsPropertyName);

    if (evalResult.isSuccessful()) {
        FunctionObjectRef* callback = evalResult.result->asFunctionObject();
        callback->setExtraData(adapter);
        Memory::gcRegisterFinalizer(callback, [](void* self) {
            jobject jo = static_cast<jobject>(reinterpret_cast<FunctionObjectRef*>(self)->extraData());
            auto env = fetchJNIEnvFromCallback();
            if (env) {
                env->DeleteGlobalRef(jo);
            }
        });
    } else {
        env->DeleteGlobalRef(adapter);
    }

    return evalResult.isSuccessful();
}