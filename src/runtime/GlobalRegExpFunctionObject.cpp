/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "GlobalRegExpFunctionObject.h"
#include "ExecutionContext.h"
#include "Context.h"

namespace Escargot {

struct GlobalRegExpFunctionObjectBuiltinFunctions {
    static Value builtinGlobalRegExpFunctionObjectInputGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.input;
    }

    static Value builtinGlobalRegExpFunctionObjectInputSetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.input = argv[0].toString(state);
        return Value();
    }

    static Value builtinGlobalRegExpFunctionObjectLastMatchGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.lastMatch));
    }

    static Value builtinGlobalRegExpFunctionObjectLastParenGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.lastParen));
    }

    static Value builtinGlobalRegExpFunctionObjectLeftContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.leftContext));
    }

    static Value builtinGlobalRegExpFunctionObjectRightContextGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
    {
        return Value(new StringView(state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status.rightContext));
    }

#define DEFINE_GETTER(number)                                                                                                                                    \
    static Value builtinGlobalRegExpFunctionObjectDollar##number##Getter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression) \
    {                                                                                                                                                            \
        auto& status = state.executionContext()->resolveCallee()->internalSlot()->asGlobalObject()->regexp()->m_status;                                          \
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
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectInputGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            new FunctionObject(state, NativeFunctionInfo(strings.set, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectInputSetter, 1, nullptr, NativeFunctionInfo::Strict)));
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());
        gs.setter().asFunction()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.input), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$_), desc);
    }

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLastMatchGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.lastMatch), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$Ampersand), desc);
    }

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLastParenGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.lastParen), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$PlusSign), desc);
    }

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectLeftContextGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.leftContext), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$GraveAccent), desc);
    }

    {
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectRightContextGetter, 0, nullptr, NativeFunctionInfo::Strict)),
            Value());
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);
        defineOwnProperty(state, ObjectPropertyName(strings.rightContext), desc);
        ObjectPropertyDescriptor desc2(gs, ObjectPropertyDescriptor::NotPresent);
        defineOwnProperty(state, ObjectPropertyName(strings.$Apostrophe), desc);
    }

#define DEFINE_ATTR(number)                                                                                                                                                                                          \
    {                                                                                                                                                                                                                \
        JSGetterSetter gs(                                                                                                                                                                                           \
            new FunctionObject(state, NativeFunctionInfo(strings.get, GlobalRegExpFunctionObjectBuiltinFunctions::builtinGlobalRegExpFunctionObjectDollar##number##Getter, 0, nullptr, NativeFunctionInfo::Strict)), \
            Value());                                                                                                                                                                                                \
        gs.getter().asFunction()->setInternalSlot(state.context()->globalObject());                                                                                                                                  \
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::EnumerablePresent);                                                                                                                              \
        defineOwnProperty(state, ObjectPropertyName(strings.$##number), desc);                                                                                                                                       \
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
