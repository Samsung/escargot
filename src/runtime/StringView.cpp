#include "Escargot.h"
#include "StringView.h"

namespace Escargot {

void* StringView::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(StringView)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringView, m_string));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(StringView));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
