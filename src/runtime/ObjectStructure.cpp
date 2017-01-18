#include "Escargot.h"
#include "Object.h"

namespace Escargot {

void* ObjectStructure::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructure)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructure, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructure, m_transitionTable));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructure));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureWithFastAccess::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        const size_t len = GC_BITMAP_SIZE(ObjectStructureWithFastAccess);
        GC_word obj_bitmap[len] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_transitionTable));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithFastAccess, m_propertyNameMap));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithFastAccess));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
