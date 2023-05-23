/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "ProxyObject.h"
#include "Context.h"
#include "runtime/ArrayObject.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

ProxyObject::ProxyObject(ExecutionState& state)
    : DerivedObject(state)
    , m_isCallable(false)
    , m_isConstructible(false)
    , m_target(nullptr)
    , m_handler(nullptr)
{
}

void* ProxyObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ProxyObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_target));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_handler));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ProxyObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Context* ProxyObject::getFunctionRealm(ExecutionState& state)
{
    if (m_handler == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return nullptr;
    }

    return m_target->getFunctionRealm(state);
}


// https://tc39.es/ecma262/#sec-proxycreate
ProxyObject* ProxyObject::createProxy(ExecutionState& state, const Value& target, const Value& handler)
{
    auto strings = &state.context()->staticStrings();

    // If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'target\' argument of Proxy must be an object");
    }

    // If Type(handler) is not Object, throw a TypeError exception.
    if (!handler.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'handler\' argument of Proxy must be an object");
    }

    // Let P be ! MakeBasicObject(« [[ProxyHandler]], [[ProxyTarget]] »).
    // Set P's essential internal methods, except for [[Call]] and [[Construct]], to the definitions specified in 9.5.
    ProxyObject* P = new ProxyObject(state);

    // If IsCallable(target) is true, then
    if (target.isCallable()) {
        // Set P.[[Call]] as specified in 9.5.12.
        P->m_isCallable = true;
        // If IsConstructor(target) is true, then
        if (target.isConstructor()) {
            // Set P.[[Construct]] as specified in 9.5.13.
            P->m_isConstructible = true;
        }
    }

    // Set P.[[ProxyTarget]] to target.
    P->setTarget(target.asObject());

    // Set P.[[ProxyHandler]] to handler.
    P->setHandler(handler.asObject());

    // Return P.
    return P;
}

bool ProxyObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "defineProperty").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->defineProperty.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[DefineOwnProperty]](P, Desc).
    if (trap.isUndefined()) {
        return target.asObject()->defineOwnProperty(state, P, desc);
    }

    // 9. Let descObj be FromPropertyDescriptor(Desc).
    // 10. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, P, descObj»)).
    // 11. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, P.toPropertyKeyValue(), Value(ObjectPropertyDescriptor::fromObjectPropertyDescriptor(state, desc)) };
    booleanTrapResult = Object::call(state, trap, handler, 3, arguments).toBoolean(state);

    // 12. If booleanTrapResult is false, return false.
    if (!booleanTrapResult) {
        return false;
    }

    // 13. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, P);

    // 15. Let extensibleTarget be IsExtensible(target).
    bool extensibleTarget = target.asObject()->isExtensible(state);

    // 17. If Desc has a [[Configurable]] field and if Desc.[[Configurable]] is false, then
    // a. Let settingConfigFalse be true.
    // 18. Else let settingConfigFalse be false.
    bool settingConfigFalse = false;
    if (desc.isConfigurablePresent() && !desc.isConfigurable()) {
        settingConfigFalse = true;
    }

    // 19. If targetDesc is undefined, then
    // a. If extensibleTarget is false, throw a TypeError exception.
    // b. If settingConfigFalse is true, throw a TypeError exception.
    if (!targetDesc.hasValue()) {
        if (!extensibleTarget || settingConfigFalse) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
    } else {
        // 20. Else targetDesc is not undefined,
        // a. If IsCompatiblePropertyDescriptor(extensibleTarget, Desc , targetDesc) is false, throw a TypeError exception.
        // b. If settingConfigFalse is true and targetDesc.[[Configurable]] is true, throw a TypeError exception.
        if (!Object::isCompatiblePropertyDescriptor(state, extensibleTarget, desc, targetDesc) || (settingConfigFalse && targetDesc.isConfigurable())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
        if (targetDesc.isDataProperty() && !targetDesc.isConfigurable() && targetDesc.isWritable()) {
            if (desc.isWritablePresent() && !desc.isWritable()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
                return false;
            }
        }
    }

    return true;
}

bool ProxyObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "deleteProperty").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->deleteProperty.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[Delete]](P).
    if (trap.isUndefined()) {
        return target.asObject()->deleteOwnProperty(state, P);
    }

    // 9. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, P»)).
    // 10. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, P.toPropertyKeyValue() };
    booleanTrapResult = Object::call(state, trap, handler, 2, arguments).toBoolean(state);

    // 11. If booleanTrapResult is false, return false.
    if (!booleanTrapResult) {
        return false;
    }

    // 12. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, P);

    // 14. If targetDesc is undefined, return true.
    if (targetDesc.value(state, target).isUndefined()) {
        return true;
    }

    // 15. If targetDesc.[[Configurable]] is false, throw a TypeError exception.
    if (!targetDesc.isConfigurable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }
    if (!target.asObject()->isExtensible(state)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }
    return true;
}

ObjectGetResult ProxyObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return ObjectGetResult();
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "getOwnPropertyDescriptor").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->getOwnPropertyDescriptor.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[GetOwnProperty]](P).
    if (trap.isUndefined()) {
        return target.asObject()->getOwnProperty(state, P);
    }

    // 9. Let trapResultObj be Call(trap, handler, «target, P»).
    // 10. ReturnIfAbrupt(trapResultObj).
    Value trapResultObj;
    Value arguments[] = { target, P.toPropertyKeyValue() };
    trapResultObj = Object::call(state, trap, handler, 2, arguments);

    // 11. If Type(trapResultObj) is neither Object nor Undefined, throw a TypeError exception.
    if (!trapResultObj.isObject() && !trapResultObj.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return ObjectGetResult();
    }

    // 12. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, P);

    // 14. If trapResultObj is undefined, then
    if (trapResultObj.isUndefined()) {
        // a. If targetDesc is undefined, return undefined.
        if (targetDesc.value(state, target).isUndefined()) {
            return ObjectGetResult();
        }
        // b. If targetDesc.[[Configurable]] is false, throw a TypeError exception.
        if (!targetDesc.isConfigurable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return ObjectGetResult();
        }
        // c. Let extensibleTarget be IsExtensible(target).
        // d. ReturnIfAbrupt(extensibleTarget).
        // e. Assert: Type(extensibleTarget) is Boolean.
        bool extensibleTarget = target.asObject()->isExtensible(state);
        // f. If extensibleTarget is false, throw a TypeError exception.
        if (!extensibleTarget) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return ObjectGetResult();
        }
        // g. Return undefined.
        return ObjectGetResult();
    }

    // 15. Let extensibleTarget be IsExtensible(target).
    // 17. Let resultDesc be ToPropertyDescriptor(trapResultObj).
    // 18. ReturnIfAbrupt(resultDesc).
    ObjectPropertyDescriptor resultDesc(state, trapResultObj.asObject());
    // 19. Call CompletePropertyDescriptor(resultDesc).
    ObjectPropertyDescriptor::completePropertyDescriptor(resultDesc);
    // 20. Let valid be IsCompatiblePropertyDescriptor (extensibleTarget, resultDesc, targetDesc).
    // 21. If valid is false, throw a TypeError exception.
    if (!Object::isCompatiblePropertyDescriptor(state, target.asObject()->isExtensible(state), resultDesc, targetDesc)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Proxy::getOwnPropertyDescriptor error");
    }
    // 22. If resultDesc.[[Configurable]] is false, then
    if (!resultDesc.isConfigurable()) {
        // a. If targetDesc is undefined or targetDesc.[[Configurable]] is true, then
        if (!targetDesc.hasValue() || targetDesc.isConfigurable()) {
            // i. Throw a TypeError exception.
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Proxy::getOwnPropertyDescriptor error");
        }
        if (resultDesc.isWritablePresent() && !resultDesc.isWritable() && targetDesc.isWritable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Proxy::getOwnPropertyDescriptor error");
        }
    }

    // 23. Return resultDesc.
    if (resultDesc.isDataDescriptor()) {
        return ObjectGetResult(resultDesc.value(), resultDesc.isWritable(), resultDesc.isEnumerable(), resultDesc.isConfigurable());
    }
    return ObjectGetResult(new JSGetterSetter(resultDesc.getterSetter()), resultDesc.isEnumerable(), resultDesc.isConfigurable());
}

bool ProxyObject::preventExtensions(ExecutionState& state)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
    }

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "preventExtensions").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->preventExtensions.string()));

    // 7. If trap is undefined, then
    // a. Return target.[[PreventExtensions]]().
    if (trap.isUndefined()) {
        return target.asObject()->preventExtensions(state);
    }

    // 8. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target»)).
    // 9. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target };
    booleanTrapResult = Object::call(state, trap, handler, 1, arguments).toBoolean(state);

    // 10. If booleanTrapResult is true, then
    if (booleanTrapResult) {
        // a. Let targetIsExtensible be target.[[IsExtensible]]().
        bool targetIsExtensible = target.asObject()->isExtensible(state);
        // c. If targetIsExtensible is true, throw a TypeError exception.
        if (targetIsExtensible) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
    }

    // 11. Return booleanTrapResult.
    return booleanTrapResult;
}

ObjectHasPropertyResult ProxyObject::hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    CHECK_STACK_OVERFLOW(state);

    auto strings = &state.context()->staticStrings();

    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return ObjectHasPropertyResult();
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "has").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->has.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[HasProperty]](P).
    if (trap.isUndefined()) {
        auto exist = target.asObject()->hasProperty(state, propertyName);
        if (exist) {
            return ObjectHasPropertyResult([](ExecutionState& state, const ObjectPropertyName& P, void* handlerData) -> Value {
                ProxyObject* p = (ProxyObject*)handlerData;
                return p->get(state, P, p).value(state, p);
            },
                                           this);
        } else {
            return ObjectHasPropertyResult();
        }
    }

    // 9. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, P»)).
    // 10. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, propertyName.toPropertyKeyValue() };
    booleanTrapResult = Object::call(state, trap, handler, 2, arguments).toBoolean(state);

    // 11. If booleanTrapResult is false, then
    if (!booleanTrapResult) {
        // a. Let targetDesc be target.[[GetOwnProperty]](P).
        ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);
        // c. If targetDesc is not undefined, then
        if (targetDesc.hasValue()) {
            // i. If targetDesc.[[Configurable]] is false, throw a TypeError exception.
            if (!targetDesc.isConfigurable()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
                return ObjectGetResult();
            }
            // ii. Let extensibleTarget be IsExtensible(target).
            bool extensibleTarget = target.asObject()->isExtensible(state);
            // iv. If extensibleTarget is false, throw a TypeError exception.
            if (!extensibleTarget) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
                return ObjectGetResult();
            }
        }
    }

    // 12. Return booleanTrapResult.
    if (booleanTrapResult) {
        return ObjectHasPropertyResult([](ExecutionState& state, const ObjectPropertyName& P, void* handlerData) -> Value {
            ProxyObject* p = (ProxyObject*)handlerData;
            return p->get(state, P, p).value(state, p);
        },
                                       this);
    }
    return ObjectHasPropertyResult();
}

Object::OwnPropertyKeyVector ProxyObject::ownPropertyKeys(ExecutionState& state)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-ownpropertykeys
    auto strings = &state.context()->staticStrings();

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return OwnPropertyKeyVector();
    }
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "ownKeys").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->ownKeys));

    // 7. If trap is undefined, then
    //  a. Return target.[[OwnPropertyKeys]]().
    if (trap.isUndefined()) {
        return target.asObject()->ownPropertyKeys(state);
    }

    // 8. Let trapResultArray be Call(trap, handler, «target»).
    // 9. Let trapResult be CreateListFromArrayLike(trapResultArray, «‍String, Symbol»).
    // 10. ReturnIfAbrupt(trapResult).
    Value trapResultArray;
    Value arguments[] = { target };
    trapResultArray = Object::call(state, trap, handler, 1, arguments);
    auto trapResult = Object::createListFromArrayLike(state, trapResultArray, (static_cast<uint8_t>(ElementTypes::String) | static_cast<uint8_t>(ElementTypes::Symbol)));

    // If trapResult contains any duplicate entries, throw a TypeError exception
    for (size_t i = 0; i < trapResult.size(); i++) {
        for (size_t j = i + 1; j < trapResult.size(); j++) {
            if (trapResult[i].equalsTo(state, trapResult[j])) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s Contains duplacted entries.");
            }
        }
    }
    // 11. Let extensibleTarget be IsExtensible(target).
    // 12. ReturnIfAbrupt(extensibleTarget).
    bool extensibleTarget = target.asObject()->isExtensible(state);

    // 13. Let targetKeys be target.[[OwnPropertyKeys]]().
    // 14. ReturnIfAbrupt(targetKeys).
    auto targetKeys = target.asObject()->ownPropertyKeys(state);
    // 15. targetKeys is a List containing only String and Symbol values.
    for (size_t i = 0; i < targetKeys.size(); ++i) {
        ASSERT((targetKeys[i].isString() || targetKeys[i].isSymbol()));
    }

    // 16. Let targetConfigurableKeys be an empty List.
    // 17. Let targetNonconfigurableKeys be an empty List.
    ValueVector targetConfigurableKeys;
    ValueVector targetNonconfigurableKeys;

    // 18 .Repeat, for each element key of targetKeys,
    for (size_t i = 0; i < targetKeys.size(); ++i) {
        auto& key = targetKeys[i];
        // a. Let desc be target.[[GetOwnProperty]](key).
        // b. ReturnIfAbrupt(desc).
        ObjectPropertyName propertyname = ObjectPropertyName(state, key);
        auto desc = target.asObject()->getOwnProperty(state, propertyname);

        // c. If desc is not undefined and desc.[[Configurable]] is false, then
        if (desc.hasValue() && !desc.isConfigurable()) {
            // i. Append key as an element of targetNonconfigurableKeys.
            targetNonconfigurableKeys.pushBack(key);
        } else { // d.Else,
            // i. Append key as an element of targetConfigurableKeys.
            targetConfigurableKeys.pushBack(key);
        }
    }
    // 19. If extensibleTarget is true and targetNonconfigurableKeys is empty, then
    if (extensibleTarget && targetNonconfigurableKeys.size() == 0) {
        // a. Return trapResult.
        return OwnPropertyKeyVector(trapResult.data(), trapResult.size());
    }

    // 20. Let uncheckedResultKeys be a new List which is a copy of trapResult.
    ValueVector uncheckedResultKeys;
    for (size_t i = 0; i < trapResult.size(); ++i) {
        uncheckedResultKeys.push_back(trapResult[i]);
    }

    // 21. Repeat, for each key that is an element of targetNonconfigurableKeys,
    for (size_t i = 0; i < targetNonconfigurableKeys.size(); ++i) {
        auto& key = targetNonconfigurableKeys[i];
        // a. If key is not an element of uncheckedResultKeys, throw a TypeError exception.
        // b. Remove key from uncheckedResultKeys
        bool found = false;
        for (size_t i = 0; i < uncheckedResultKeys.size(); i++) {
            if (uncheckedResultKeys[i].equalsTo(state, key)) {
                uncheckedResultKeys.erase(i);
                found = true;
                i--;
            }
        }
        if (!found) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: the key of targetNonconfigurableKeys is not an element of uncheckedResultKeys.");
        }
    }

    // 22. If extensibleTarget is true, return trapResult.
    if (extensibleTarget) {
        return OwnPropertyKeyVector(trapResult.data(), trapResult.size());
    }

    // 23. Repeat, for each key that is an element of targetConfigurableKeys,
    for (size_t i = 0; i < targetConfigurableKeys.size(); ++i) {
        auto& key = targetConfigurableKeys[i];
        // a. If key is not an element of uncheckedResultKeys, throw a TypeError exception.
        // b. Remove key from uncheckedResultKeys
        bool found = false;
        for (size_t i = 0; i < uncheckedResultKeys.size(); i++) {
            if (uncheckedResultKeys[i].equalsTo(state, key)) {
                uncheckedResultKeys.erase(i);
                found = true;
                i--;
            }
        }
        if (!found) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: the key of targetConfigurableKeys is not an element of uncheckedResultKeys.");
        }
    }
    // 24. If uncheckedResultKeys is not empty, throw a TypeError exception.
    if (uncheckedResultKeys.size()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: uncheckedResultKeys is not empty");
    }

    // 25. Return trapResult.
    return OwnPropertyKeyVector(trapResult.data(), trapResult.size());
}

bool ProxyObject::isExtensible(ExecutionState& state)
{
    CHECK_STACK_OVERFLOW(state);

    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
    }

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "isExtensible").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->isExtensible.string()));

    // 7. If trap is undefined, then
    // a. Return target.[[IsExtensible]]().
    if (trap.isUndefined()) {
        return target.asObject()->isExtensible(state);
    }

    // 8. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target»)).
    // 9. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target };
    booleanTrapResult = Object::call(state, trap, handler, 1, arguments).toBoolean(state);

    // 10. Let targetResult be target.[[IsExtensible]]().
    bool targetResult = target.asObject()->isExtensible(state);

    // 12. If SameValue(booleanTrapResult, targetResult) is false, throw a TypeError exception.
    if (targetResult != booleanTrapResult) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }
    // 13. Return booleanTrapResult.
    return booleanTrapResult;
}

void ProxyObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    // Note : the enumeration method is not [[Enumerate]] () in spec
    // In our implementation, Object::enumeration method should be overridden properly, therefore when calling this functionm, must run enumeration of target
    this->target()->enumeration(state, callback, data, shouldSkipSymbolKey);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-setprototypeof-v
bool ProxyObject::setPrototype(ExecutionState& state, const Value& value)
{
    CHECK_STACK_OVERFLOW(state);

    auto strings = &state.context()->staticStrings();
    // 1. Assert: Either Type(V) is Object or Type(V) is Null.
    ASSERT(value.isObject() || value.isNull());

    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "setPrototypeOf").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->setPrototypeOf.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[SetPrototypeOf]](V).
    if (trap.isUndefined()) {
        return target.asObject()->setPrototype(state, value);
    }

    // 9. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, V»)).
    // 10. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, value };
    booleanTrapResult = Object::call(state, trap, handler, 2, arguments).toBoolean(state);

    // For ES2018 compatibility 9.5.2.9
    if (!booleanTrapResult) {
        return booleanTrapResult;
    }

    // 11. Let extensibleTarget be IsExtensible(target).
    bool extensibleTarget = target.asObject()->isExtensible(state);

    // 13. If extensibleTarget is true, return booleanTrapResult.
    if (extensibleTarget) {
        return booleanTrapResult;
    }

    // 14. Let targetProto be target.[[GetPrototypeOf]]().
    Value targetProto = target.asObject()->getPrototype(state);

    // 16. If booleanTrapResult is true and SameValue(V, targetProto) is false, throw a TypeError exception.
    if (booleanTrapResult && value != targetProto) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }

    // 17. Return booleanTrapResult.
    return booleanTrapResult;
}

Object* ProxyObject::getPrototypeObject(ExecutionState& state)
{
    if (!this->target()) {
        return nullptr;
    }

    Value result = getPrototype(state);
    if (result.isObject()) {
        return result.asObject();
    }

    return nullptr;
}

// https://www.ecma-international.org/ecma-262/6.0/index.html#sec-proxy-object-internal-methods-and-internal-slots-getprototypeof
Value ProxyObject::getPrototype(ExecutionState& state)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return Value();
    }

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "getPrototypeOf").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->getPrototypeOf.string()));

    // 7. If trap is undefined, then
    // a. Return target.[[GetPrototypeOf]]().
    if (trap.isUndefined()) {
        return target.asObject()->getPrototype(state);
    }

    // 8. Let handlerProto be Call(trap, handler, «target»).
    // 9. ReturnIfAbrupt(handlerProto).
    Value handlerProto;
    Value arguments[] = { target };
    handlerProto = Object::call(state, trap, handler, 1, arguments);

    // 10. If Type(handlerProto) is neither Object nor Null, throw a TypeError exception.
    if (!handlerProto.isObject() && !handlerProto.isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return Value();
    }

    // 11. Let extensibleTarget be IsExtensible(target).
    bool extensibleTarget = target.asObject()->isExtensible(state);

    // 13. If extensibleTarget is true, return handlerProto.
    if (extensibleTarget) {
        return handlerProto;
    }

    // 14. Let targetProto be target.[[GetPrototypeOf]]().
    Value targetProto = target.asObject()->getPrototype(state);

    // 16. If SameValue(handlerProto, targetProto) is false, throw a TypeError exception.
    if (handlerProto != targetProto) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return Value();
    }

    // 17. Return handlerProto.
    return handlerProto;
}

// https://www.ecma-international.org/ecma-262/6.0/index.html#sec-proxy-object-internal-methods-and-internal-slots-get-p-receiver
ObjectGetResult ProxyObject::get(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& receiver)
{
    CHECK_STACK_OVERFLOW(state);

    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return ObjectGetResult();
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "get").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->get.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[Get]](P, Receiver).
    if (trap.isUndefined()) {
        return target.asObject()->get(state, propertyName, receiver);
    }

    // 9. Let trapResult be Call(trap, handler, «target, P, Receiver»).
    // 10. ReturnIfAbrupt(trapResult).
    Value trapResult;
    Value arguments[] = { target, propertyName.toPropertyKeyValue(), Value(this) };
    trapResult = Object::call(state, trap, handler, 3, arguments);

    // 11. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);

    // 13. If targetDesc is not undefined, then
    if (targetDesc.hasValue()) {
        // a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Writable]] is false, then
        if (targetDesc.isDataProperty() && !targetDesc.isConfigurable() && !targetDesc.isWritable()) {
            // i. If SameValue(trapResult, targetDesc.[[Value]]) is false, throw a TypeError exception.
            if (trapResult != targetDesc.value(state, target)) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return ObjectGetResult();
            }
        }
        // b. If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Get]] is undefined, then
        if (!targetDesc.isDataProperty() && !targetDesc.isConfigurable() && (!targetDesc.jsGetterSetter()->hasGetter() || targetDesc.jsGetterSetter()->getter().isUndefined())) {
            // i. If trapResult is not undefined, throw a TypeError exception.
            if (!trapResult.isUndefined()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return ObjectGetResult();
            }
        }
    }

    // 14. Return trapResult.
    return ObjectGetResult(trapResult, true, true, true);
}

// https://www.ecma-international.org/ecma-262/6.0/index.html#sec-proxy-object-internal-methods-and-internal-slots-set-p-v-receiver
bool ProxyObject::set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver)
{
    CHECK_STACK_OVERFLOW(state);

    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
        return false;
    }

    // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 4. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 6. Let trap be GetMethod(handler, "set").
    // 7. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->set.string()));

    // 8. If trap is undefined, then
    // a. Return target.[[Set]](P, V, Receiver).
    if (trap.isUndefined()) {
        return target.asObject()->set(state, propertyName, v, receiver);
    }

    // 9. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, P, V, Receiver»)).
    // 10. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, propertyName.toPropertyKeyValue(), v, receiver };
    booleanTrapResult = Object::call(state, trap, handler, 4, arguments).toBoolean(state);

    // 11. If booleanTrapResult is false, return false.
    if (!booleanTrapResult) {
        return false;
    }

    // 12. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);

    // 14. If targetDesc is not undefined, then
    if (targetDesc.hasValue()) {
        // a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Writable]] is false, then
        if (targetDesc.isDataProperty() && !targetDesc.isConfigurable() && !targetDesc.isWritable()) {
            // i. If SameValue(V, targetDesc.[[Value]]) is false, throw a TypeError exception.
            if (v != targetDesc.value(state, target)) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return false;
            }
        }
        // b. TODO If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]] is false, then
        if (!targetDesc.isDataProperty() && !targetDesc.isConfigurable()) {
            // i. If targetDesc.[[Set]] is undefined, throw a TypeError exception.
            if ((!targetDesc.jsGetterSetter()->hasSetter() || targetDesc.jsGetterSetter()->setter().isUndefined())) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return false;
            }
        }
    }

    // 15. Return true.
    return true;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-call-thisargument-argumentslist
Value ProxyObject::call(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv)
{
    if (UNLIKELY(!m_isCallable)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }

    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return Value();
    }

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "apply").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->apply.string()));

    // 7. If trap is undefined, then
    // a. Return Call(target, thisArgument, argumentsList).
    if (trap.isUndefined()) {
        return Object::call(state, target, receiver, argc, argv);
    }

    // 8. Let argArray be CreateArrayFromList(argumentsList).
    ArrayObject* argArray = Object::createArrayFromList(state, argc, argv);

    // 9. Return Call(trap, handler, «target, thisArgument, argArray»).
    Value arguments[] = { target, receiver, Value(argArray) };
    return Object::call(state, trap, handler, 3, arguments);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-construct-argumentslist-newtarget
Value ProxyObject::construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return Value();
    }

    // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    Value handler(this->handler());

    // 3. Assert: Type(handler) is Object.
    ASSERT(handler.isObject());

    // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
    Value target(this->target());

    // 5. Let trap be GetMethod(handler, "construct").
    // 6. ReturnIfAbrupt(trap).
    Value trap;
    trap = Object::getMethod(state, handler, ObjectPropertyName(state, strings->construct.string()));

    // 7. If trap is undefined, then
    // a. Assert: target has a [[Construct]] internal method.
    // b. Return Construct(target, argumentsList, newTarget).
    if (trap.isUndefined()) {
        ASSERT(target.isConstructor());
        return Object::construct(state, target, argc, argv, newTarget).toObject(state);
    }

    // 8. Let argArray be CreateArrayFromList(argumentsList).
    ArrayObject* argArray = Object::createArrayFromList(state, argc, argv);

    // 9. Let newObj be Call(trap, handler, «target, argArray, newTarget »).
    Value arguments[] = { target, Value(argArray), Value(newTarget) };
    Value newObj = Object::call(state, trap, handler, 3, arguments);

    // 11. If Type(newObj) is not Object, throw a TypeError exception.
    if (!newObj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: The result of [[Construct]] must be an Object.");
        return Value();
    }

    // 12. Return newObj.
    return newObj;
}
} // namespace Escargot
