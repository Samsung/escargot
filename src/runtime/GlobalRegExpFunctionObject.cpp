/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "GlobalRegExpFunctionObject.h"
#include "Context.h"
#include "NativeFunctionObject.h"

namespace Escargot {

struct GlobalRegExpFunctionObjectBuiltinFunctions {
    static Value builtinGlobalRegExpFunctionObjectInputGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.input;
    }

    static Value builtinGlobalRegExpFunctionObjectInputSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.input = argv[0].toString(state);
        return Value();
    }

    static Value builtinGlobalRegExpFunctionObjectLastMatchGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.lastMatch));
    }

    static Value builtinGlobalRegExpFunctionObjectLastParenGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.lastParen));
    }

    static Value builtinGlobalRegExpFunctionObjectLeftContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.leftContext));
    }

    static Value builtinGlobalRegExpFunctionObjectRightContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.rightContext));
    }

#define DEFINE_GETTER(number)                                                                                                                                    \
    static Value builtinGlobalRegExpFunctionObjectDollar##number##Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression) \
    {                                                                                                                                                            \
        auto& status = state.resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status;                                                              \
        if (status.pairCount < number) {                                                                                                                         \
            return Value(String::emptyString);                                                                                                                   \
        }                                                                                                                                                        \
        return Value(new StringView(thisValue.asObject()->asGlobalRegExpFunctionObject()->m_status.pairs[number - 1]));                                          \
    }

    DEFINE_GETTER(1)
    DEFINE_GETTER(2)
    DEFINE_GETTER(3)
    DEFINE_GETTER(4)
    DEFINE_GETTER(5)
    DEFINE_GETTER(6)
    DEFINE_GETTER(7)
    DEFINE_GETTER(8)
    DEFINE_GETTER(9)
};

void GlobalRegExpFunctionObject::initInternalProperties(ExecutionState& state)
{
    const StaticStrings& strings = state.context()->staticStrings();
    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectInputGetter, 0, NativeFunctionInfo::Strict)),
            new NativeFunctionObject(state, NativeFunctionInfo(strings.set, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectInputSetter, 1, NativeFunctionInfo::Strict)));
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());
        gs.setter().asObject()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.input), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$_), desc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLastMatchGetter, 0, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.lastMatch), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$Ampersand), desc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLastParenGetter, 0, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.lastParen), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$PlusSign), desc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLeftContextGetter, 0, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.leftContext), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$GraveAccent), desc);
    }

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectRightContextGetter, 0, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.rightContext), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$Apostrophe), desc);
    }

#define DEFINE_ATTR(number)                                                                                                                                                                                       \
    {                                                                                                                                                                                                             \
        JSGetterSetter gs(                                                                                                                                                                                        \
            new NativeFunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectDollar##number##Getter, 0, NativeFunctionInfo::Strict)), \
            Value());                                                                                                                                                                                             \
        gs.getter().asObject()->setInternalSlot(state.context()->globalObject());                                                                                                                                 \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);                                                                                                                           \
        defineOwnProperty(state, ObjectPropertyName(strings.$##number), desc);                                                                                                                                    \
    }

    DEFINE_ATTR(1)
    DEFINE_ATTR(2)
    DEFINE_ATTR(3)
    DEFINE_ATTR(4)
    DEFINE_ATTR(5)
    DEFINE_ATTR(6)
    DEFINE_ATTR(7)
    DEFINE_ATTR(8)
    DEFINE_ATTR(9)
}
}
