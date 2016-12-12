#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "DateObject.h"
#include "ErrorObject.h"

namespace Escargot {

static Value builtinDateConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DateObject* thisObject;
    if (isNewExpression) {
        thisObject = thisValue.asObject()->asDateObject();

        if (argc == 0) {
            thisObject->setPrimitiveValueAsCurrentTime();
        } else if (argc == 1) {
            RELEASE_ASSERT_NOT_REACHED();
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
        // return thisObject->toFullString();
        return thisObject;
    } else {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static Value builtinDateGetTime(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, getTime);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().getTime.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    uint64_t val = thisObject->asDateObject()->primitiveValue();
    return Value(val);
}

static Value builtinDateValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Date, valueOf);
    if (!thisObject->isDateObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Date.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotDateObject);
    }
    uint64_t val = thisObject->asDateObject()->primitiveValue();
    return Value(val);
}

void GlobalObject::installDate(ExecutionState& state)
{
    m_date = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Date, builtinDateConstructor, 7, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                    return new DateObject(state, std::numeric_limits<double>::quiet_NaN());
                                }));
    m_date->markThisObjectDontNeedStructureTransitionTable(state);
    m_date->setPrototype(state, m_functionPrototype);
    // TODO m_date->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_datePrototype = m_objectPrototype;
    m_datePrototype = new Object(state);
    m_datePrototype->setPrototype(state, m_objectPrototype);

    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().getTime),
                                       Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getTime, builtinDateGetTime, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                            (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    m_datePrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().valueOf),
                                       Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().valueOf, builtinDateValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                            (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));


    m_date->setFunctionPrototype(state, m_datePrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Date),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_date, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
