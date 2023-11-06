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
Java_com_samsung_lwe_escargot_JavaScriptValue_createUndefined(JNIEnv* env, jclass clazz)
{
    return createJavaValueObject(env, clazz, ValueRef::createUndefined());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_createNull(JNIEnv* env, jclass clazz)
{
    return createJavaValueObject(env, clazz, ValueRef::createNull());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_create__I(JNIEnv* env, jclass clazz, jint value)
{
    return createJavaValueObject(env, clazz, ValueRef::create(value));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_create__D(JNIEnv* env, jclass clazz, jdouble value)
{
    return createJavaValueObject(env, clazz, ValueRef::create(value));
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_create__Z(JNIEnv* env, jclass clazz, jboolean value)
{
    return createJavaValueObject(env, clazz, ValueRef::create(static_cast<bool>(value)));
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isUndefined(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isUndefined();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isNull(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isNull();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isUndefinedOrNull(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isUndefinedOrNull();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isNumber(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isNumber();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isInt32(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isInt32();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isBoolean(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isBoolean();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isTrue(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isTrue();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isFalse(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isFalse();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isString(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isString();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isSymbol(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isSymbol();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isBigInt(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isBigInt();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isCallable(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isCallable();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isObject(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isObject();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isArrayObject(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isArrayObject();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isFunctionObject(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isFunctionObject();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isPromiseObject(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isPromiseObject();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_isErrorObject(JNIEnv* env, jobject thiz)
{
    return unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz)->isErrorObject();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asBoolean(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, Boolean);
    return ref->asBoolean();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asInt32(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, Int32);
    return ref->asInt32();
}

extern "C"
JNIEXPORT jdouble JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asNumber(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, Number);
    return ref->asNumber();
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptString(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, String);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptSymbol(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, Symbol);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptBigInt(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, BigInt);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptObject(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, Object);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptArrayObject(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, ArrayObject);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptFunctionObject(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, FunctionObject);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptPromiseObject(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, PromiseObject);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_asScriptErrorObject(JNIEnv* env, jobject thiz)
{
    ValueRef* ref = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    THROW_CAST_EXCEPTION_IF_NEEDS(env, ref, ErrorObject);
    return thiz;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toString(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return thisValueRef->toString(state);
    }, thisValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toNumber(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->toNumber(state));
    }, thisValueRef);

    return createOptionalValueFromEvaluatorDoubleResult(env, context, contextRef->get(),
                                                        evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toInteger(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->toInteger(state));
    }, thisValueRef);

    return createOptionalValueFromEvaluatorDoubleResult(env, context, contextRef->get(),
                                                        evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toInt32(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->toInt32(state));
    }, thisValueRef);

    return createOptionalValueFromEvaluatorIntegerResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toBoolean(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->toBoolean(state));
    }, thisValueRef);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_toObject(JNIEnv* env, jobject thiz, jobject context)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef) -> ValueRef* {
        return thisValueRef->toObject(state);
    }, thisValueRef);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_abstractEqualsTo(JNIEnv* env, jobject thiz,
                                                               jobject context, jobject other)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(other, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    ValueRef* otherValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(other), other);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef, ValueRef* otherValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->abstractEqualsTo(state, otherValueRef));
    }, thisValueRef, otherValueRef);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_equalsTo(JNIEnv* env, jobject thiz, jobject context,
                                                       jobject other)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(other, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    ValueRef* otherValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(other), other);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef, ValueRef* otherValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->equalsTo(state, otherValueRef));
    }, thisValueRef, otherValueRef);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_instanceOf(JNIEnv* env, jobject thiz, jobject context,
                                                         jobject other)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(other, "JavaScriptValue");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    ValueRef* otherValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(other), other);

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(), [](ExecutionStateRef* state, ValueRef* thisValueRef, ValueRef* otherValueRef) -> ValueRef* {
        return ValueRef::create(thisValueRef->instanceOf(state, otherValueRef));
    }, thisValueRef, otherValueRef);

    return createOptionalValueFromEvaluatorBooleanResult(env, context, contextRef->get(),
                                                         evaluatorResult);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_call(JNIEnv* env, jobject thiz, jobject context,
                                                   jobject receiver, jobjectArray argv)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(receiver, "JavaScriptValue");
    THROW_NPE_RETURN_NULL(argv, "JavaScriptValue[]");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);
    ValueRef* receiverValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(receiver), receiver);

    auto argvLength = env->GetArrayLength(argv);
    ValueRef** argVector = reinterpret_cast<ValueRef**>(Memory::gcMalloc(argvLength * sizeof(ValueRef*)));

    for (jsize i = 0; i < argvLength; i++) {
        jobject e = env->GetObjectArrayElement(argv, i);
        argVector[i] = unwrapValueRefFromValue(env, env->GetObjectClass(e), e);
        env->DeleteLocalRef(e);
    }

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(),
                                              [](ExecutionStateRef* state, ValueRef* thisValueRef, ValueRef* receiverValueRef, ValueRef** argVector, int argvLength) -> ValueRef* {
                                                  return thisValueRef->call(state, receiverValueRef, argvLength, argVector);
                                              }, thisValueRef, receiverValueRef, argVector, argvLength);

    Memory::gcFree(argVector);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}


extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_JavaScriptValue_construct(JNIEnv* env, jobject thiz, jobject context,
                                                        jobjectArray argv)
{
    THROW_NPE_RETURN_NULL(context, "Context");
    THROW_NPE_RETURN_NULL(argv, "JavaScriptValue[]");

    auto contextRef = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    ValueRef* thisValueRef = unwrapValueRefFromValue(env, env->GetObjectClass(thiz), thiz);

    auto argvLength = env->GetArrayLength(argv);
    ValueRef** argVector = reinterpret_cast<ValueRef**>(Memory::gcMalloc(argvLength * sizeof(ValueRef*)));

    for (jsize i = 0; i < argvLength; i++) {
        jobject e = env->GetObjectArrayElement(argv, i);
        argVector[i] = unwrapValueRefFromValue(env, env->GetObjectClass(e), e);
        env->DeleteLocalRef(e);
    }

    auto evaluatorResult = ScriptEvaluator::execute(contextRef->get(),
                                              [](ExecutionStateRef* state, ValueRef* thisValueRef, ValueRef** argVector, int argvLength) -> ValueRef* {
                                                  return thisValueRef->construct(state, argvLength, argVector);
                                              }, thisValueRef, argVector, argvLength);

    Memory::gcFree(argVector);

    return createOptionalValueFromEvaluatorJavaScriptValueResult(env, context, contextRef->get(),
                                                                 evaluatorResult);
}