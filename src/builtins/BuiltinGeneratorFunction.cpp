/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/GeneratorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ScriptGeneratorFunctionObject.h"

namespace Escargot {

static Value builtinGeneratorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    size_t argumentVectorCount = argc > 1 ? argc - 1 : 0;
    Value sourceValue = argc >= 1 ? argv[argc - 1] : Value(String::emptyString);
    auto functionSource = FunctionObject::createDynamicFunctionScript(state, state.context()->staticStrings().anonymous, argumentVectorCount, argv, sourceValue, false, true, false, false);
    RETURN_VALUE_IF_PENDING_EXCEPTION

    // Let proto be ? GetPrototypeFromConstructor(newTarget, fallbackProto).
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->generator();
    });
    RETURN_VALUE_IF_PENDING_EXCEPTION

    return new ScriptGeneratorFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment);
}

static Value builtinGeneratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return GeneratorObject::generatorResume(state, thisValue, argc > 0 ? argv[0] : Value());
}

static Value builtinGeneratorReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return GeneratorObject::generatorResumeAbrupt(state, thisValue, argc > 0 ? argv[0] : Value(), GeneratorObject::GeneratorAbruptType::Return);
}

static Value builtinGeneratorThrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return GeneratorObject::generatorResumeAbrupt(state, thisValue, argc > 0 ? argv[0] : Value(), GeneratorObject::GeneratorAbruptType::Throw);
}

void GlobalObject::initializeGenerator(ExecutionState& state)
{
    // do nothing
}

void GlobalObject::installGenerator(ExecutionState& state)
{
    ASSERT(!!m_iteratorPrototype);

    // %GeneratorFunction% : The constructor of generator objects
    m_generatorFunction = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().GeneratorFunction, builtinGeneratorFunction, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_generatorFunction->setGlobalIntrinsicObject(state);

    // %Generator% : The initial value of the prototype property of %GeneratorFunction%
    m_generator = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().GeneratorFunction, nullptr, 0, NativeFunctionInfo::Strict));
    m_generator->setGlobalIntrinsicObject(state, true);
    m_generatorFunction->setFunctionPrototype(state, m_generator);

    // 25.2.3.1 The initial value of GeneratorFunction.prototype.constructor is the intrinsic object %GeneratorFunction%.
    m_generator->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_generatorFunction, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    {
        ASSERT(!!m_callerAndArgumentsGetterSetter);
        JSGetterSetter gs(m_callerAndArgumentsGetterSetter, m_callerAndArgumentsGetterSetter);
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);

        m_generator->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), desc);
        m_generator->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().arguments), desc);
    }

    // %GeneratorPrototype% : The initial value of the prototype property of %Generator%
    m_generatorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_generatorPrototype->setGlobalIntrinsicObject(state, true);

    m_generator->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().prototype), ObjectPropertyDescriptor(m_generatorPrototype, ObjectPropertyDescriptor::ConfigurablePresent));

    // 25.2.3.3 GeneratorFunction.prototype [ @@toStringTag ]
    // The initial value of the @@toStringTag property is the String value "GeneratorFunction"..
    // This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
    m_generator->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().GeneratorFunction.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // The initial value of Generator.prototype.constructor is the intrinsic object %Generator%.
    m_generatorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_generator, ObjectPropertyDescriptor::ConfigurablePresent));

    m_generatorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinGeneratorNext, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_generatorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringReturn),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringReturn, builtinGeneratorReturn, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_generatorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringThrow),
                                                  ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringThrow, builtinGeneratorThrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // http://www.ecma-international.org/ecma-262/6.0/#sec-generatorfunction.prototype-@@tostringtag
    m_generatorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                  ObjectPropertyDescriptor(Value(state.context()->staticStrings().Generator.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
