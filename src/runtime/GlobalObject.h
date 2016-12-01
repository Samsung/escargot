#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/Object.h"

namespace Escargot {

class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;

    GlobalObject(ExecutionState& state)
        : Object(state)
    {

    }

protected:
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
