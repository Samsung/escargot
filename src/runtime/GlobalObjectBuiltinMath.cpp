#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"

namespace Escargot {

static Value builtinMathMax(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0) {
        double n_inf = -1 * std::numeric_limits<double>::infinity();
        return Value(n_inf);
    } else  {
        double maxValue = argv[0].toNumber(state);
        for (unsigned i = 1; i < argc; i++) {
            double value = argv[i].toNumber(state);
            double qnan = std::numeric_limits<double>::quiet_NaN();
            if (std::isnan(value))
                return Value(qnan);
            if (value > maxValue || (!value && !maxValue && !std::signbit(value)))
                maxValue = value;
        }
        return Value(maxValue);
    }
    return Value();
}

void GlobalObject::installMath(ExecutionState& state)
{
    m_math = new Object(state);
    m_math->markThisObjectDontNeedStructureTransitionTable(state);

    // initialize math object: $20.2.1.6 Math.PI
    const StaticStrings* strings = &state.context()->staticStrings();
    m_math->defineOwnPropertyThrowsException(state, strings->PI, ObjectPropertyDescriptorForDefineOwnProperty(Value(3.1415926535897932), ObjectPropertyDescriptor::NotPresent));
    // TODO(add reference)
    m_math->defineOwnPropertyThrowsException(state, strings->E, ObjectPropertyDescriptorForDefineOwnProperty(Value(2.718281828459045), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.3
    m_math->defineOwnPropertyThrowsException(state, strings->LN2, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.6931471805599453), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.2
    m_math->defineOwnPropertyThrowsException(state, strings->LN10, ObjectPropertyDescriptorForDefineOwnProperty(Value(2.302585092994046), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.4
    m_math->defineOwnPropertyThrowsException(state, strings->LOG2E, ObjectPropertyDescriptorForDefineOwnProperty(Value(1.4426950408889634), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.5
    m_math->defineOwnPropertyThrowsException(state, strings->LOG10E, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.4342944819032518), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.7
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT1_2, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.7071067811865476), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.8
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT2, ObjectPropertyDescriptorForDefineOwnProperty(Value(1.4142135623730951), ObjectPropertyDescriptor::NotPresent));

    // initialize math object: $20.2.2.24 Math.max()
    m_math->defineOwnPropertyThrowsException(state, PropertyName(state.context()->staticStrings().max),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().max, builtinMathMax, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    defineOwnProperty(state, PropertyName(state.context()->staticStrings().Math),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(m_math, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}

}
