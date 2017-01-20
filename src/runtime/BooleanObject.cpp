#include "Escargot.h"
#include "BooleanObject.h"
#include "Context.h"

namespace Escargot {

BooleanObject::BooleanObject(ExecutionState& state, bool value)
    : Object(state)
    , m_primitiveValue(value)
{
    setPrototype(state, state.context()->globalObject()->booleanPrototype());
}

void* BooleanObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(BooleanObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_rareData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BooleanObject, m_values));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(BooleanObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
