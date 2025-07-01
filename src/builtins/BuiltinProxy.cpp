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
#include "runtime/ProxyObject.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/ArrayObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"

namespace Escargot {

// $26.2.1 The Proxy Constructor
static Value builtinProxyConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();

    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->Proxy.string(), false, String::emptyString(), "%s: calling a builtin Proxy constructor without new is forbidden");
        return Value();
    }

    Value target = argv[0];
    Value handler = argv[1];

    return ProxyObject::createProxy(state, target, handler);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy-revocation-functions
static Value builtinProxyRevoke(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();

    ExtendedNativeFunctionObject* revoke = state.resolveCallee()->asExtendedNativeFunctionObject();
    ASSERT(revoke);

    // 1. Let p be the value of F’s [[RevocableProxy]] internal slot.
    Value p = revoke->internalSlot(ProxyObject::BuiltinFunctionSlot::RevocableProxy);

    // 2. If p is null, return undefined.
    if (p.isNull()) {
        return Value();
    }

    ProxyObject* proxy = p.asObject()->asProxyObject();

    // 3. Set the value of F’s [[RevocableProxy]] internal slot to null.
    revoke->setInternalSlot(ProxyObject::BuiltinFunctionSlot::RevocableProxy, Value(Value::Null));

    // 5. Set the [[ProxyTarget]] internal slot of p to null.
    proxy->setTarget(nullptr);

    // 6. Set the [[ProxyHandler]] internal slot of p to null.
    proxy->setHandler(nullptr);

    // 6. Return undefined.
    return Value();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-proxy.revocable
static Value builtinProxyRevocable(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();

    Value target = argv[0];
    Value handler = argv[1];

    // 1. Let p be ProxyCreate(target, handler).
    Value proxy = ProxyObject::createProxy(state, target, handler);

    // 3. Let revoker be a new built-in function object as defined in 26.2.2.1.1.
    // 4. Set the [[RevocableProxy]] internal slot of revoker to p.
    ExtendedNativeFunctionObject* revoker = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), builtinProxyRevoke, 0, NativeFunctionInfo::Strict));
    revoker->setInternalSlot(ProxyObject::BuiltinFunctionSlot::RevocableProxy, proxy);

    // 5. Let result be ObjectCreate(%ObjectPrototype%).
    Object* result = new Object(state);

    // 6. Perform CreateDataProperty(result, "proxy", p).
    result->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->proxy),
                                             ObjectPropertyDescriptor(proxy, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 7. Perform CreateDataProperty(result, "revoke", revoker).
    result->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->revoke),
                                             ObjectPropertyDescriptor(revoker, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // 8. Return result.
    return result;
}

void GlobalObject::initializeProxy(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->proxy(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Proxy), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installProxy(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_proxy = new NativeFunctionObject(state, NativeFunctionInfo(strings->Proxy, builtinProxyConstructor, 2), NativeFunctionObject::__ForBuiltinProxyConstructor__);
    m_proxy->setGlobalIntrinsicObject(state);

    m_proxy->directDefineOwnProperty(state, ObjectPropertyName(strings->revocable), ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->revocable, builtinProxyRevocable, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(strings->Proxy),
                        ObjectPropertyDescriptor(m_proxy, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
