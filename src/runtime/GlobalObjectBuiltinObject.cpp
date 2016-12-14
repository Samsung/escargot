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

    thisValue = thisValue.toObject(state);
    if (thisValue.isFunction()) {
        return AtomicString(state, "[object Function]").string();
    }
    /*
    if (thisVal->isESArrayObject()) {
        return ESString::createAtomicString("[object Array]");
    } else if (thisVal->isESStringObject()) {
        return ESString::createAtomicString("[object String]");
    } else if (thisVal->isESFunctionObject()) {
        return ESString::createAtomicString("[object Function]");
    } else if (thisVal->isESErrorObject()) {
        return ESString::createAtomicString("[object Error]");
    } else if (thisVal->isESBooleanObject()) {
        return ESString::createAtomicString("[object Boolean]");
    } else if (thisVal->isESNumberObject()) {
        return ESString::createAtomicString("[object Number]");
    } else if (thisVal->isESDateObject()) {
        return ESString::createAtomicString("[object Date]");
    } else if (thisVal->isESRegExpObject()) {
        return ESString::createAtomicString("[object RegExp]");
    } else if (thisVal->isESMathObject()) {
        return ESString::createAtomicString("[object Math]");
    } else if (thisVal->isESJSONObject()) {
        return ESString::createAtomicString("[object JSON]");
#ifdef USE_ES6_FEATURE
    } else if (thisVal->isESTypedArrayObject()) {
        ASCIIString ret = "[object ";
        ESValue ta_constructor = thisVal->get(strings->constructor.string());
        // ALWAYS created from new expression
        ASSERT(ta_constructor.isESPointer() && ta_constructor.asESPointer()->isESObject());
        ESValue ta_name = ta_constructor.asESPointer()->asESObject()->get(strings->name.string());
        ret.append(ta_name.toString()->asciiData());
        ret.append("]");
        return ESString::createAtomicString(ret.data());
    } else if (thisVal->isESArrayBufferObject()) {
        return ESString::createAtomicString("[object ArrayBuffer]");
    } else if (thisVal->isESDataViewObject()) {
        return ESString::createAtomicString("[object DataView]");
    } else if (thisVal->isESPromiseObject()) {
        return ESString::createAtomicString("[object Promise]");
#endif
    } else if (thisVal->isESArgumentsObject()) {
        return ESString::createAtomicString("[object Arguments]");
    } else if (thisVal->isGlobalObject()) {
        return ESString::createAtomicString("[object global]");
    }*/
    return AtomicString(state, "[object Object]").string();
}

static Value builtinObjectHasOwnProperty(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    String* keyString = argv[0].toPrimitive(state, Value::PrimitiveTypeHint::PreferString).toString(state);
    Object* obj = thisValue.toObject(state);
    return Value(obj->hasOwnProperty(state, ObjectPropertyName(state, keyString)));
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
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor),
                                         ObjectPropertyDescriptorForDefineOwnProperty(m_object, (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                         ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinObjectToString, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                      (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    // $19.1.3.2 Object.prototype.hasOwnProperty(V)
    m_objectPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().hasOwnProperty),
                                         ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinObjectHasOwnProperty, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Object),
                      ObjectPropertyDescriptorForDefineOwnProperty(m_object, (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
}
}
