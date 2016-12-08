#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ArrayObject.h"

namespace Escargot {

static Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    bool interpretArgumentsAsElements = false;
    size_t size = 0;
    if (argc > 1) {
        size = argc;
        interpretArgumentsAsElements = true;
    } else if (argc == 1) {
        Value& val = argv[0];
        if (val.isNumber()) {
            if (val.equalsTo(state, Value(val.toUint32(state)))) {
                size = val.toNumber(state);
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
            }
        } else {
            size = 1;
            interpretArgumentsAsElements = true;
        }
    }
    ArrayObject* array;
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isArrayObject()) {
        array = thisValue.asPointerValue()->asObject()->asArrayObject();
    } else {
        array = new ArrayObject(state);
    }

    array->setLength(state, size);
    if (interpretArgumentsAsElements) {
        Value val = argv[0];
        if (argc > 1 || !val.isInt32()) {
            for (size_t idx = 0; idx < argc; idx++) {
                array->defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), Object::ObjectPropertyDescriptorForDefineOwnProperty(val));
                val = argv[idx + 1];
            }
        }
    }
    return array;
}

void GlobalObject::installArray(ExecutionState& state)
{
    m_array = new FunctionObject(state, new CodeBlock(state.context(), NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                                          return new ArrayObject(state);
                                                      })));
    m_array->markThisObjectDontNeedStructureTransitionTable(state);
    m_array->setPrototype(state, m_functionPrototype);
    // TODO m_array->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_arrayPrototype = m_objectPrototype;
    m_arrayPrototype = new ArrayObject(state);
    m_arrayPrototype->setPrototype(state, m_objectPrototype);

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
