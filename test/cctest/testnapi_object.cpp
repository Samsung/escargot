#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

// Self-contained coverage for the object/property/array N-API surface
// implemented in src/napi/NapiObject.cpp - unlike testnapi.cpp's dlopen()-based
// tests, these drive napi_* entry points directly against ExecutionStateRef
// objects created in-process, the same way
// Napi.CallFunctionReportsExceptionAsPendingStatus does in testnapi.cpp.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstring>

TEST(Napi, ObjectProperties)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value object;
            napi_create_object(env, &object);

            napi_value key = ToNapi(StringRef::createFromASCII("foo"));
            napi_value value = ToNapi(ValueRef::create(42));

            // set/get via generic (napi_value key) entry points
            EXPECT_EQ(napi_set_property(env, object, key, value), napi_ok);

            napi_value gotten = nullptr;
            EXPECT_EQ(napi_get_property(env, object, key, &gotten), napi_ok);
            EXPECT_EQ(FromNapi(gotten)->asNumber(), 42);

            bool hasIt = false;
            EXPECT_EQ(napi_has_property(env, object, key, &hasIt), napi_ok);
            EXPECT_TRUE(hasIt);

            bool hasOwnIt = false;
            EXPECT_EQ(napi_has_own_property(env, object, key, &hasOwnIt), napi_ok);
            EXPECT_TRUE(hasOwnIt);

            // named-property entry points (utf8name instead of napi_value key)
            bool hasNamed = false;
            EXPECT_EQ(napi_has_named_property(env, object, "foo", &hasNamed), napi_ok);
            EXPECT_TRUE(hasNamed);

            napi_value gottenNamed = nullptr;
            EXPECT_EQ(napi_get_named_property(env, object, "foo", &gottenNamed), napi_ok);
            EXPECT_EQ(FromNapi(gottenNamed)->asNumber(), 42);

            napi_value gottenMissingNamed = nullptr;
            EXPECT_EQ(napi_get_named_property(env, object, "missing", &gottenMissingNamed), napi_ok);
            EXPECT_TRUE(FromNapi(gottenMissingNamed)->isUndefined());

            bool hasMissingNamed = true;
            EXPECT_EQ(napi_has_named_property(env, object, "missing", &hasMissingNamed), napi_ok);
            EXPECT_FALSE(hasMissingNamed);

            // delete
            bool deleted = false;
            EXPECT_EQ(napi_delete_property(env, object, key, &deleted), napi_ok);
            EXPECT_TRUE(deleted);

            bool hasAfterDelete = true;
            EXPECT_EQ(napi_has_property(env, object, key, &hasAfterDelete), napi_ok);
            EXPECT_FALSE(hasAfterDelete);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ObjectIndexedProperties)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value object;
            napi_create_object(env, &object);

            napi_value value = ToNapi(ValueRef::create(7));
            EXPECT_EQ(napi_set_element(env, object, 3, value), napi_ok);

            bool hasIt = false;
            EXPECT_EQ(napi_has_element(env, object, 3, &hasIt), napi_ok);
            EXPECT_TRUE(hasIt);

            napi_value gotten = nullptr;
            EXPECT_EQ(napi_get_element(env, object, 3, &gotten), napi_ok);
            EXPECT_EQ(FromNapi(gotten)->asNumber(), 7);

            bool deleted = false;
            EXPECT_EQ(napi_delete_element(env, object, 3, &deleted), napi_ok);
            EXPECT_TRUE(deleted);

            bool hasAfterDelete = true;
            EXPECT_EQ(napi_has_element(env, object, 3, &hasAfterDelete), napi_ok);
            EXPECT_FALSE(hasAfterDelete);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ObjectPropertyNamesAndPrototype)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value object;
            napi_create_object(env, &object);
            napi_set_named_property(env, object, "a", ToNapi(ValueRef::create(1)));
            napi_set_named_property(env, object, "b", ToNapi(ValueRef::create(2)));

            napi_value names = nullptr;
            EXPECT_EQ(napi_get_property_names(env, object, &names), napi_ok);
            ObjectRef* namesArray = FromNapi(names)->asObject();
            EXPECT_TRUE(FromNapi(names)->isArrayObject());
            EXPECT_EQ(namesArray->length(state), 2u);

            // prototype of a plain object literal is Object.prototype, itself
            // an object (not null)
            napi_value proto = nullptr;
            EXPECT_EQ(napi_get_prototype(env, object, &proto), napi_ok);
            EXPECT_TRUE(FromNapi(proto)->isObject());

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ObjectFreeze)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value object;
            napi_create_object(env, &object);
            napi_set_named_property(env, object, "a", ToNapi(ValueRef::create(1)));

            EXPECT_EQ(napi_object_freeze(env, object), napi_ok);

            // a set on a frozen object is silently rejected (non-strict Set
            // semantics) rather than thrown
            napi_value key = ToNapi(StringRef::createFromASCII("a"));
            napi_value newValue = ToNapi(ValueRef::create(99));
            EXPECT_EQ(napi_set_property(env, object, key, newValue), napi_ok);

            napi_value stillOld = nullptr;
            EXPECT_EQ(napi_get_property(env, object, key, &stillOld), napi_ok);
            EXPECT_EQ(FromNapi(stillOld)->asNumber(), 1);

            // adding a brand-new property to a frozen object is likewise rejected
            napi_value addedKey = ToNapi(StringRef::createFromASCII("b"));
            EXPECT_EQ(napi_set_property(env, object, addedKey, newValue), napi_ok);
            bool hasAdded = true;
            EXPECT_EQ(napi_has_own_property(env, object, addedKey, &hasAdded), napi_ok);
            EXPECT_FALSE(hasAdded);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ObjectSeal)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value object;
            napi_create_object(env, &object);
            napi_set_named_property(env, object, "a", ToNapi(ValueRef::create(1)));

            EXPECT_EQ(napi_object_seal(env, object), napi_ok);

            // existing writable properties can still be updated after seal ...
            napi_value key = ToNapi(StringRef::createFromASCII("a"));
            napi_value newValue = ToNapi(ValueRef::create(99));
            EXPECT_EQ(napi_set_property(env, object, key, newValue), napi_ok);

            napi_value updated = nullptr;
            EXPECT_EQ(napi_get_property(env, object, key, &updated), napi_ok);
            EXPECT_EQ(FromNapi(updated)->asNumber(), 99);

            // ... but a sealed object still rejects new properties
            napi_value addedKey = ToNapi(StringRef::createFromASCII("b"));
            EXPECT_EQ(napi_set_property(env, object, addedKey, newValue), napi_ok);
            bool hasAdded = true;
            EXPECT_EQ(napi_has_own_property(env, object, addedKey, &hasAdded), napi_ok);
            EXPECT_FALSE(hasAdded);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, ObjectArray)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value plainObject;
            napi_create_object(env, &plainObject);
            bool isPlainObjectArray = true;
            EXPECT_EQ(napi_is_array(env, plainObject, &isPlainObjectArray), napi_ok);
            EXPECT_FALSE(isPlainObjectArray);

            napi_value array;
            EXPECT_EQ(napi_create_array(env, &array), napi_ok);
            bool isArray = false;
            EXPECT_EQ(napi_is_array(env, array, &isArray), napi_ok);
            EXPECT_TRUE(isArray);

            uint32_t length = 123;
            EXPECT_EQ(napi_get_array_length(env, array, &length), napi_ok);
            EXPECT_EQ(length, 0u);

            napi_value arrayWithLength;
            EXPECT_EQ(napi_create_array_with_length(env, 5, &arrayWithLength), napi_ok);
            bool isArrayWithLength = false;
            EXPECT_EQ(napi_is_array(env, arrayWithLength, &isArrayWithLength), napi_ok);
            EXPECT_TRUE(isArrayWithLength);

            uint32_t lengthWithLength = 0;
            EXPECT_EQ(napi_get_array_length(env, arrayWithLength, &lengthWithLength), napi_ok);
            EXPECT_EQ(lengthWithLength, 5u);

            EXPECT_EQ(napi_set_element(env, arrayWithLength, 10, ToNapi(ValueRef::create(1))), napi_ok);
            uint32_t lengthAfterSet = 0;
            EXPECT_EQ(napi_get_array_length(env, arrayWithLength, &lengthAfterSet), napi_ok);
            EXPECT_EQ(lengthAfterSet, 11u);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, InstanceOf)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            // Exercise napi_instanceof against a real constructor (the built-in
            // Array). A plain no-op native FunctionObjectRef cannot model ES
            // prototype-chain construction in Escargot: a NativeFunctionInfo
            // constructor must itself return an object, and that object would
            // not carry the constructor's .prototype - so napi_define_class /
            // built-ins are the constructors that produce instanceof-able
            // instances (see napi-notes.md).
            ValueRef* arrayCtor = state->context()->globalObject()->get(state, StringRef::createFromASCII("Array"));

            napi_value arrayInstance = ToNapi(static_cast<ValueRef*>(ArrayObjectRef::create(state)));
            bool isInstance = false;
            EXPECT_EQ(napi_instanceof(env, arrayInstance, ToNapi(arrayCtor), &isInstance), napi_ok);
            EXPECT_TRUE(isInstance);

            napi_value plainObject;
            napi_create_object(env, &plainObject);
            bool isNotInstance = true;
            EXPECT_EQ(napi_instanceof(env, plainObject, ToNapi(arrayCtor), &isNotInstance), napi_ok);
            EXPECT_FALSE(isNotInstance);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

TEST(Napi, TypeChecksOnDowncasts)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    Evaluator::EvaluatorResult result = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env) -> ValueRef* {
            env->executionState = state;

            napi_value nonObject = nullptr;
            napi_create_double(env, 42.0, &nonObject);

            napi_value key = nullptr;
            napi_create_string_utf8(env, "prop", NAPI_AUTO_LENGTH, &key);

            napi_value value = nullptr;
            napi_create_double(env, 100.0, &value);

            bool boolResult = false;
            napi_value valResult = nullptr;

            EXPECT_EQ(napi_set_property(env, nonObject, key, value), napi_object_expected);
            EXPECT_EQ(napi_get_property(env, nonObject, key, &valResult), napi_object_expected);
            EXPECT_EQ(napi_has_property(env, nonObject, key, &boolResult), napi_object_expected);
            EXPECT_EQ(napi_delete_property(env, nonObject, key, &boolResult), napi_object_expected);
            EXPECT_EQ(napi_has_own_property(env, nonObject, key, &boolResult), napi_object_expected);

            EXPECT_EQ(napi_set_named_property(env, nonObject, "prop", value), napi_object_expected);
            EXPECT_EQ(napi_get_named_property(env, nonObject, "prop", &valResult), napi_object_expected);
            EXPECT_EQ(napi_has_named_property(env, nonObject, "prop", &boolResult), napi_object_expected);

            EXPECT_EQ(napi_set_element(env, nonObject, 0, value), napi_object_expected);
            EXPECT_EQ(napi_get_element(env, nonObject, 0, &valResult), napi_object_expected);
            EXPECT_EQ(napi_has_element(env, nonObject, 0, &boolResult), napi_object_expected);
            EXPECT_EQ(napi_delete_element(env, nonObject, 0, &boolResult), napi_object_expected);

            EXPECT_EQ(napi_define_properties(env, nonObject, 0, nullptr), napi_object_expected);

            napi_ref wrapRef = nullptr;
            EXPECT_EQ(napi_wrap(env, nonObject, nullptr, nullptr, nullptr, &wrapRef), napi_object_expected);

            void* unwrapResult = nullptr;
            EXPECT_EQ(napi_unwrap(env, nonObject, &unwrapResult), napi_object_expected);
            EXPECT_EQ(napi_remove_wrap(env, nonObject, &unwrapResult), napi_object_expected);

            return ValueRef::createUndefined();
        },
        napiEnv->env());

    ASSERT_TRUE(result.isSuccessful()) << result.resultOrErrorToString(napiEnv->context())->toStdUTF8String();
}

#endif // ENABLE_NAPI
