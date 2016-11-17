#ifndef __EscargotObject__
#define __EscargotObject__

#include "runtime/PointerValue.h"
#include "util/Vector.h"
#include "runtime/SmallValue.h"
#include "runtime/ObjectStructure.h"

namespace Escargot {

struct ObjectRareData {
    bool m_isExtensible;
};

class Object : public PointerValue {
public:
    virtual Type type()
    {
        return ObjectType;
    }

    virtual bool isObject()
    {
        return true;
    }

protected:
    ObjectRareData* m_rareData;
    ObjectStructure* m_structure;
    Vector<SmallValue, gc_allocator_ignore_off_page<SmallValue>> m_values;
};

}

#endif
