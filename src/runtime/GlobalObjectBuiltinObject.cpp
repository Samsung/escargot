#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"

namespace Escargot {

static Value builtinObjectConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value value = argv[0];
    if (value.isUndefined() || value.isNull()) {
        return new Object(state);
    } else {
        return value.toObject(state);
    }
}

static Value builtinObjectToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isUndefined()) {
        return new ASCIIString("[object Undefined]");
    } else if (thisValue.isNull()) {
        return new ASCIIString("[object Null]");
    }

    Object* thisObject = thisValue.toObject(state);
    if (thisObject->isFunctionObject()) {
        return AtomicString(state, "[object Function]").string();
    } else if (thisObject->isArrayObject()) {
        return AtomicString(state, "[object Array]").string();
    } else if (thisObject->isStringObject()) {
        return AtomicString(state, "[object String]").string();
    } else if (thisObject->isFunctionObject()) {
        return AtomicString(state, "[object Function]").string();
    } else if (thisObject->isErrorObject()) {
        return AtomicString(state, "[object Error]").string();
    } else if (thisObject->isBooleanObject()) {
        return AtomicString(state, "[object Boolean]").string();
    } else if (thisObject->isNumberObject()) {
        return AtomicString(state, "[object Number]").string();
    } else if (thisObject->isDateObject()) {
        return AtomicString(state, "[object Date]").string();
    } else if (thisObject->isRegExpObject()) {
        return AtomicString(state, "[object RegExp]").string();
    } /*else if (thisObject->isESMathObject()) {
        return AtomicString(state, "[object Math]").string();
    } else if (thisObject->isESJSONObject()) {
        return AtomicString(state, "[object JSON]").string();
#ifdef USE_ES6_FEATURE
    } else if (thisObject->isESTypedArrayObject()) {
        ASCIIString ret = "[object ";
        ESValue ta_constructor = thisObject->get(strings->constructor.string()).string();
        // ALWAYS created from new expression
        ASSERT(ta_constructor.isESPointer() && ta_constructor.asESPointer()->isESObject()).string();
        ESValue ta_name = ta_constructor.asESPointer()->asESObject()->get(strings->name.string()).string();
        ret.append(ta_name.toString()->asciiData()).string();
        ret.append("]").string();
        return AtomicString(state, ret.data()).string();
    } else if (thisObject->isESArrayBufferObject()) {
        return AtomicString(state, "[object ArrayBuffer]").string();
    } else if (thisObject->isESDataViewObject()) {
        return AtomicString(state, "[object DataView]").string();
    } else if (thisObject->isESPromiseObject()) {
        return AtomicString(state, "[object Promise]").string();
#endif
    } else if (thisObject->isESArgumentsObject()) {
        return AtomicString(state, "[object Arguments]").string();
    }*/ else if (thisObject->isGlobalObject()) {
        return AtomicString(state, "[object global]").string();
    }
    return AtomicString(state, "[object Object]").string();
}

static Value builtinObjectHasOwnProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    String* keyString = argv[0].toPrimitive(state, Value::PrimitiveTypeHint::PreferString).toString(state);
    Object* obj = thisValue.toObject(state);
    return Value(obj->hasOwnProperty(state, ObjectPropertyName(state, keyString)));
}

inline Value objectDefineProperties(ExecutionState& state, Value object, Value properties)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    if (!object.isObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Object.string(), false, strings->defineProperty.string(), errorMessage_GlobalObject_FirstArgumentNotObject);
    Object* props = properties.toObject(state);
    std::vector<std::pair<ObjectPropertyName, ObjectPropertyDescriptor> > descriptors;
    props->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) -> bool {
        auto propDesc = props->getOwnProperty(state, name);
        if (propDesc.hasValue() && !propDesc.value().isUndefined() && desc.isEnumerable()) {
            Value propVal = propDesc.value();
            if (!propVal.isObject())
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Object.string(), false, strings->defineProperty.string(), errorMessage_GlobalObject_DescriptorNotObject);
            descriptors.push_back(std::make_pair(name, ObjectPropertyDescriptor(state, propVal.toObject(state))));
        }
        return true;
    });
    for (auto it : descriptors) {
        object.asObject()->defineOwnPropertyThrowsException(state, it.first, it.second);
    }
    return object;
}

static Value builtinObjectCreate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject() && !argv[0].isNull())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Object.string(), false, state.context()->staticStrings().create.string(), errorMessage_GlobalObject_FirstArgumentNotObjectAndNotNull);
    Object* obj = new Object(state);
    if (argv[0].isNull())
        obj->setPrototype(state, Value(Value::Null));
    else
        obj->setPrototype(state, argv[0]);

    if (!argv[1].isUndefined())
        return objectDefineProperties(state, obj, argv[1]);
    return obj;
}


void GlobalObject::installObject(ExecutionState& state)
{
    FunctionObject* emptyFunction = m_functionPrototype;
    m_object = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Object, builtinObjectConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                      return new Object(state);
                                  }));
    m_object->markThisObjectDontNeedStructureTransitionTable(state);
    m_object->setPrototype(state, emptyFunction);
    // TODO m_object->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_object->setFunctionPrototype(state, m_objectPrototype);
    // $19.1.2.2 Object.create (O [,Properties])
    m_object->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().create),
        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().create, builtinObjectCreate, 2, nullptr, NativeFunctionInfo::Strict)),
                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                         ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinObjectToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                      (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().hasOwnProperty),
                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinObjectHasOwnProperty, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Object),
                      ObjectPropertyDescriptor(m_object, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
