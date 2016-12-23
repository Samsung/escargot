#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/FunctionObject.h"
#include "runtime/Object.h"

namespace Escargot {

class FunctionObject;

#define RESOLVE_THIS_BINDING_TO_OBJECT(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                    \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                 \
    Object* NAME = thisValue.toObject(state);

#define RESOLVE_THIS_BINDING_TO_STRING(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                    \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                 \
    String* NAME = thisValue.toString(state);

class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;
    friend class GlobalEnvironmentRecord;

    GlobalObject(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, false)
    {
        m_objectPrototype = Object::createBuiltinObjectPrototype(state);
        setPrototype(state, m_objectPrototype);

        m_structure = m_structure->convertToWithFastAccess(state);
    }

    virtual bool isGlobalObject()
    {
        return true;
    }

    void installBuiltins(ExecutionState& state)
    {
        installFunction(state);
        installObject(state);
        installError(state);
        installString(state);
        installNumber(state);
        installBoolean(state);
        installArray(state);
        installMath(state);
        installDate(state);
        installRegExp(state);
        installOthers(state);
    }

    void installFunction(ExecutionState& state);
    void installObject(ExecutionState& state);
    void installError(ExecutionState& state);
    void installString(ExecutionState& state);
    void installNumber(ExecutionState& state);
    void installBoolean(ExecutionState& state);
    void installArray(ExecutionState& state);
    void installMath(ExecutionState& state);
    void installDate(ExecutionState& state);
    void installRegExp(ExecutionState& state);
    void installOthers(ExecutionState& state);

    Value eval(ExecutionState& state, const Value& arg, CodeBlock* parentCodeBlock);

    FunctionObject* object()
    {
        return m_object;
    }
    Object* objectPrototype()
    {
        return m_objectPrototype;
    }
    FunctionObject* objectPrototypeToString()
    {
        return m_objectPrototypeToString;
    }

    FunctionObject* function()
    {
        return m_function;
    }
    FunctionObject* functionPrototype()
    {
        return m_functionPrototype;
    }

    FunctionObject* error()
    {
        return m_error;
    }
    Object* errorPrototype()
    {
        return m_errorPrototype;
    }
    FunctionObject* referenceError()
    {
        return m_referenceError;
    }
    Object* referenceErrorPrototype()
    {
        return m_referenceErrorPrototype;
    }
    FunctionObject* typeError()
    {
        return m_typeError;
    }
    Object* typeErrorPrototype()
    {
        return m_typeErrorPrototype;
    }
    FunctionObject* rangeError()
    {
        return m_rangeError;
    }
    Object* rangeErrorPrototype()
    {
        return m_rangeErrorPrototype;
    }
    FunctionObject* syntaxError()
    {
        return m_syntaxError;
    }
    Object* syntaxErrorPrototype()
    {
        return m_syntaxErrorPrototype;
    }
    FunctionObject* uriError()
    {
        return m_uriError;
    }
    Object* uriErrorPrototype()
    {
        return m_uriErrorPrototype;
    }
    FunctionObject* evalError()
    {
        return m_evalError;
    }
    Object* evalErrorPrototype()
    {
        return m_evalErrorPrototype;
    }

    FunctionObject* string()
    {
        return m_string;
    }
    Object* stringPrototype()
    {
        return m_stringPrototype;
    }

    FunctionObject* number()
    {
        return m_number;
    }
    Object* numberPrototype()
    {
        return m_numberPrototype;
    }

    FunctionObject* array()
    {
        return m_array;
    }
    Object* arrayPrototype()
    {
        return m_arrayPrototype;
    }

    FunctionObject* boolean()
    {
        return m_boolean;
    }
    Object* booleanPrototype()
    {
        return m_booleanPrototype;
    }

    FunctionObject* date()
    {
        return m_date;
    }
    Object* datePrototype()
    {
        return m_datePrototype;
    }

    Object* math()
    {
        return m_math;
    }

    FunctionObject* regexp()
    {
        return m_regexp;
    }

    Object* regexpPrototype()
    {
        return m_regexpPrototype;
    }

    FunctionObject* eval()
    {
        return m_eval;
    }

    StringObject* stringProxyObject()
    {
        return m_stringProxyObject;
    }

    NumberObject* numberProxyObject()
    {
        return m_numberProxyObject;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "global";
    }

protected:
    FunctionObject* m_object;
    Object* m_objectPrototype;
    FunctionObject* m_objectPrototypeToString;

    FunctionObject* m_function;
    FunctionObject* m_functionPrototype;

    FunctionObject* m_error;
    Object* m_errorPrototype;
    FunctionObject* m_referenceError;
    Object* m_referenceErrorPrototype;
    FunctionObject* m_typeError;
    Object* m_typeErrorPrototype;
    FunctionObject* m_rangeError;
    Object* m_rangeErrorPrototype;
    FunctionObject* m_syntaxError;
    Object* m_syntaxErrorPrototype;
    FunctionObject* m_uriError;
    Object* m_uriErrorPrototype;
    FunctionObject* m_evalError;
    Object* m_evalErrorPrototype;

    FunctionObject* m_string;
    Object* m_stringPrototype;

    FunctionObject* m_number;
    Object* m_numberPrototype;

    FunctionObject* m_array;
    Object* m_arrayPrototype;

    FunctionObject* m_boolean;
    Object* m_booleanPrototype;

    FunctionObject* m_date;
    Object* m_datePrototype;

    FunctionObject* m_regexp;
    Object* m_regexpPrototype;

    Object* m_math;

    FunctionObject* m_eval;

    StringObject* m_stringProxyObject;
    NumberObject* m_numberProxyObject;

    bool hasPropertyOnIndex(ExecutionState& state, const PropertyName& name, size_t idx)
    {
        if (idx < m_structure->propertyCount()) {
            const ObjectStructureItem& item = m_structure->readProperty(state, idx);
            if (item.m_propertyName == name) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    Value getPropertyOnIndex(ExecutionState& state, size_t idx)
    {
        return getOwnPropertyUtilForObject(state, idx, this);
    }

    bool setPropertyOnIndex(ExecutionState& state, size_t idx, const Value& v)
    {
        return setOwnPropertyUtilForObject(state, idx, v);
    }

    size_t findPropertyIndex(ExecutionState& state, const PropertyName& name)
    {
        return m_structure->findProperty(state, name);
    }
};
}

#endif
