#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/Object.h"
#include "runtime/FunctionObject.h"

namespace Escargot {

class FunctionObject;

class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;

    GlobalObject(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, false)
    {
        m_objectPrototype = Object::createBuiltinObjectPrototype(state);
        setPrototype(state, m_objectPrototype);

        markThisObjectDontNeedStructureTransitionTable(state);
    }

    void installBuiltins(ExecutionState& state)
    {
        installFunction(state);
        installObject(state);
        installError(state);
        installOthers(state);
    }

    void installFunction(ExecutionState& state);
    void installObject(ExecutionState& state);
    void installError(ExecutionState& state);
    void installOthers(ExecutionState& state);

    // object
    FunctionObject* object()
    {
        return m_object;
    }
    Object* objectPrototype()
    {
        return m_objectPrototype;
    }

    // function
    FunctionObject* function()
    {
        return m_function;
    }
    FunctionObject* functionPrototype()
    {
        return m_functionPrototype;
    }

    // errors
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

protected:
    FunctionObject* m_object;
    Object* m_objectPrototype;

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
        return getOwnProperty(state, idx, this);
    }

    bool setPropertyOnIndex(ExecutionState& state, size_t idx, const Value& v)
    {
        return setOwnProperty(state, idx, v);
    }

    size_t findPropertyIndex(ExecutionState& state, const PropertyName& name)
    {
        return m_structure->findProperty(state, name);
    }
};

}

#endif
