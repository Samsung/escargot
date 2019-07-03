/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
    : Object(state)
    , m_isCallable(false)
    , m_isConstructible(false)
    , m_target(nullptr)
    , m_handler(nullptr)
{
}

void* ProxyObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ProxyObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_target));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ProxyObject, m_handler));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ProxyObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxycreate
Value ProxyObject::createProxy(ExecutionState& state, const Value& target, const Value& handler)
{
    auto strings = &state.context()->staticStrings();

    // 1. If Type(target) is not Object, throw a TypeError exception.
    if (!target.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'target\' argument of Proxy must be an object");
        return Value();
    }

    // 2. If target is a Proxy exotic object and target.[[ProxyHandler]] is null, throw a TypeError exception.
    if (target.asObject()->isProxyObject()) {
        ProxyObject* exotic = target.asObject()->asProxyObject();
        if (!exotic->handler()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'target\' Type Error");
            return Value();
        }
    }

    // 3. If Type(handler) is not Object, throw a TypeError exception.
    if (!handler.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'handler\' argument of Proxy must be an object");
        return Value();
    }

    // 4. If handler is a Proxy exotic object and handler.[[ProxyHandler]] is null, throw a TypeError exception.
    if (handler.asObject()->isProxyObject()) {
        ProxyObject* exotic = handler.asObject()->asProxyObject();
        if (!exotic->handler()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: \'handler\' Type Error");
            return Value();
        }
    }

    // 5. Let P be a newly created object.
    ProxyObject* proxy = new ProxyObject(state);

    // TODO
    // 6. Set P's essential internal methods (except for [[Call]] and [[Construct]])

    // 7. If IsCallable(target) is true, then
    // 7.a Set the [[Call]] internal method of P as specified in 9.5.13.
    if (target.isCallable()) {
        proxy->m_isCallable = true;
    }
    // 7.b If target has a [[Construct]] internal method, then Set the [[Construct]] internal method of P as specified in 9.5.14.
    if (target.isConstructor()) {
        proxy->m_isConstructible = true;
    }

    // 8. Set the [[ProxyTarget]] internal slot of P to target.
    proxy->setTarget(target.asObject());

    // 9. Set the [[ProxyHandler]] internal slot of P to handler.
    proxy->setHandler(handler.asObject());

    // 10. Return P.
    return proxy;
}

bool ProxyObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
    Value arguments[] = { target, P.toPlainValue(state), Value(ObjectPropertyDescriptor::fromObjectPropertyDescriptor(state, desc)) };
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
    if (!desc.isConfigurable()) {
        settingConfigFalse = true;
    }

    // 19. If targetDesc is undefined, then
    // a. If extensibleTarget is false, throw a TypeError exception.
    // b. If settingConfigFalse is true, throw a TypeError exception.
    if (targetDesc.value(state, target).isUndefined()) {
        if (!extensibleTarget || settingConfigFalse) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
        // 20. Else targetDesc is not undefined,
        // TODO a. If IsCompatiblePropertyDescriptor(extensibleTarget, Desc , targetDesc) is false, throw a TypeError exception.
        // b. If settingConfigFalse is true and targetDesc.[[Configurable]] is true, throw a TypeError exception.
    } else {
        if (settingConfigFalse && targetDesc.isConfigurable()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
    }

    return true;
}

bool ProxyObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
    Value arguments[] = { target, P.toPlainValue(state) };
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }

    return true;
}

ObjectGetResult ProxyObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
    Value arguments[] = { target, P.toPlainValue(state) };
    trapResultObj = Object::call(state, trap, handler, 2, arguments);

    // 11. If Type(trapResultObj) is neither Object nor Undefined, throw a TypeError exception.
    if (!trapResultObj.isObject() && !trapResultObj.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return ObjectGetResult();
        }
        // c. Let extensibleTarget be IsExtensible(target).
        // d. ReturnIfAbrupt(extensibleTarget).
        // e. Assert: Type(extensibleTarget) is Boolean.
        bool extensibleTarget = target.asObject()->isExtensible(state);
        // f. If extensibleTarget is false, throw a TypeError exception.
        if (!extensibleTarget) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return ObjectGetResult();
        }
        // g. Return undefined.
        return ObjectGetResult();
    }

    // 15. Let extensibleTarget be IsExtensible(target).
    bool extensibleTarget = target.asObject()->isExtensible(state);

    // 17. TODO Let resultDesc be ToPropertyDescriptor(trapResultObj).
    // 19. TODO Call CompletePropertyDescriptor(resultDesc).
    // 20. TODO Let valid be IsCompatiblePropertyDescriptor (extensibleTarget, resultDesc, targetDesc).
    // 21. If valid is false, throw a TypeError exception.

    ASSERT(trapResultObj.isObject());
    Value writable = trapResultObj.asObject()->get(state, ObjectPropertyName(state, strings->writable)).value(state, trapResultObj);
    Value enumerable = trapResultObj.asObject()->get(state, ObjectPropertyName(state, strings->enumerable)).value(state, trapResultObj);
    Value configurable = trapResultObj.asObject()->get(state, ObjectPropertyName(state, strings->configurable)).value(state, trapResultObj);
    Value value = trapResultObj.asObject()->get(state, ObjectPropertyName(state, strings->value)).value(state, trapResultObj);
    ObjectGetResult resultDesc(value, writable.toBoolean(state), enumerable.toBoolean(state), configurable.toBoolean(state));

    // 22. If resultDesc.[[Configurable]] is false, then
    // a. If targetDesc is undefined or targetDesc.[[Configurable]] is true, then
    // i. Throw a TypeError exception.
    if (!resultDesc.isConfigurable() && (targetDesc.value(state, target).isUndefined() || targetDesc.isConfigurable())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Proxy::getOwnPropertyDescriptor error");
    }

    // 23. Return resultDesc.
    return resultDesc;
}

bool ProxyObject::preventExtensions(ExecutionState& state)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
            return false;
        }
    }

    // 11. Return booleanTrapResult.
    return booleanTrapResult;
}

bool ProxyObject::hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    auto strings = &state.context()->staticStrings();

    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return false;
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
        return target.asObject()->hasProperty(state, propertyName);
    }

    // 9. Let booleanTrapResult be ToBoolean(Call(trap, handler, «target, P»)).
    // 10. ReturnIfAbrupt(booleanTrapResult).
    bool booleanTrapResult;
    Value arguments[] = { target, propertyName.toPlainValue(state) };
    booleanTrapResult = Object::call(state, trap, handler, 2, arguments).toBoolean(state);

    // 11. If booleanTrapResult is false, then
    if (!booleanTrapResult) {
        // a. Let targetDesc be target.[[GetOwnProperty]](P).
        ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);
        // c. If targetDesc is not undefined, then
        if (!targetDesc.value(state, target).isUndefined()) {
            // i. If targetDesc.[[Configurable]] is false, throw a TypeError exception.
            if (!targetDesc.isConfigurable()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
                return false;
            }
            // ii. Let extensibleTarget be IsExtensible(target).
            bool extensibleTarget = target.asObject()->isExtensible(state);
            // iv. If extensibleTarget is false, throw a TypeError exception.
            if (!extensibleTarget) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
                return false;
            }
        }
    }

    // 12. Return booleanTrapResult.
    return booleanTrapResult;
}

bool ProxyObject::isExtensible(ExecutionState& state)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return false;
    }
    // 13. Return booleanTrapResult.
    return booleanTrapResult;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-setprototypeof-v
bool ProxyObject::setPrototype(ExecutionState& state, const Value& value)
{
    auto strings = &state.context()->staticStrings();
    // 1. Assert: Either Type(V) is Object or Type(V) is Null.
    ASSERT(value.isObject() || value.isNull());

    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
    if (!booleanTrapResult) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy setPrototypeOf could not set the prototype.");
        return false;
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error");
        return Value();
    }

    // 17. Return handlerProto.
    return handlerProto;
}

// https://www.ecma-international.org/ecma-262/6.0/index.html#sec-proxy-object-internal-methods-and-internal-slots-get-p-receiver
ObjectGetResult ProxyObject::get(ExecutionState& state, const ObjectPropertyName& propertyName)
{
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
        return target.asObject()->get(state, propertyName);
    }

    // 9. Let trapResult be Call(trap, handler, «target, P, Receiver»).
    // 10. ReturnIfAbrupt(trapResult).
    Value trapResult;
    Value arguments[] = { target, propertyName.toPlainValue(state), Value(this) };
    trapResult = Object::call(state, trap, handler, 3, arguments);

    // 11. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);

    // 13. If targetDesc is not undefined, then
    if (!targetDesc.value(state, target).isUndefined()) {
        // a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Writable]] is false, then
        if (targetDesc.isDataProperty() && !targetDesc.isConfigurable() && !targetDesc.isWritable()) {
            // i. If SameValue(trapResult, targetDesc.[[Value]]) is false, throw a TypeError exception.
            if (trapResult != targetDesc.value(state, target)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return ObjectGetResult();
            }
        }
        // b. If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Get]] is undefined, then
        if (!targetDesc.isDataProperty() && !targetDesc.isConfigurable() && (!targetDesc.jsGetterSetter()->hasGetter() || targetDesc.jsGetterSetter()->getter().isUndefined())) {
            // i. If trapResult is not undefined, throw a TypeError exception.
            if (!trapResult.isUndefined()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
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
    auto strings = &state.context()->staticStrings();
    // 3. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
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
    Value arguments[] = { target, propertyName.toPlainValue(state), v, receiver };
    booleanTrapResult = Object::call(state, trap, handler, 4, arguments).toBoolean(state);

    // 11. If booleanTrapResult is false, return false.
    if (!booleanTrapResult) {
        return false;
    }

    // 12. Let targetDesc be target.[[GetOwnProperty]](P).
    ObjectGetResult targetDesc = target.asObject()->getOwnProperty(state, propertyName);

    // 14. If targetDesc is not undefined, then
    if (!targetDesc.value(state, target).isUndefined()) {
        // a. TODO If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is false and targetDesc.[[Writable]] is false, then
        if (!targetDesc.isConfigurable() && !targetDesc.isWritable()) {
            // i. If SameValue(V, targetDesc.[[Value]]) is false, throw a TypeError exception.
            if (v != targetDesc.value(state, target)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return false;
            }
        }
        // b. TODO If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]] is false, then
        if (!targetDesc.isConfigurable()) {
            // i. If targetDesc.[[Set]] is undefined, throw a TypeError exception.
            if ((!targetDesc.jsGetterSetter()->hasGetter() || targetDesc.jsGetterSetter()->getter().isUndefined())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy Type Error.");
                return false;
            }
        }
    }

    // 15. Return true.
    return true;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-call-thisargument-argumentslist
Value ProxyObject::call(ExecutionState& state, const Value& receiver, const size_t argc, NULLABLE Value* argv)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
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
    ArrayObject* argArray = new ArrayObject(state);
    for (size_t n = 0; n < argc; n++) {
        argArray->defineOwnProperty(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(argv[n], ObjectPropertyDescriptor::AllPresent));
    }

    // 9. Return Call(trap, handler, «target, thisArgument, argArray»).
    Value arguments[] = { target, receiver, Value(argArray) };
    return Object::call(state, trap, handler, 3, arguments);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-object-internal-methods-and-internal-slots-construct-argumentslist-newtarget
Object* ProxyObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget)
{
    auto strings = &state.context()->staticStrings();
    // 2. If handler is null, throw a TypeError exception.
    if (this->handler() == nullptr) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: Proxy handler should not be null.");
        return nullptr;
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
        return Object::construct(state, target, argc, argv, newTarget);
    }

    // 8. Let argArray be CreateArrayFromList(argumentsList).
    ArrayObject* argArray = new ArrayObject(state);
    for (size_t n = 0; n < argc; n++) {
        argArray->defineOwnProperty(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(argv[n], ObjectPropertyDescriptor::AllPresent));
    }

    // 9. Let newObj be Call(trap, handler, «target, argArray, newTarget »).
    Value arguments[] = { target, Value(argArray), newTarget };
    Value newObj = Object::call(state, trap, handler, 3, arguments);

    // 11. If Type(newObj) is not Object, throw a TypeError exception.
    if (!newObj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Proxy.string(), false, String::emptyString, "%s: The result of [[Construct]] must be an Object.");
        return nullptr;
    }

    // 12. Return newObj.
    return newObj.asObject();
}
}
