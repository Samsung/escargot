#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"

namespace Escargot {

OpcodeTable g_opcodeTable;

OpcodeTable::OpcodeTable()
{
    ExecutionState state(nullptr, nullptr);
    ByteCodeInterpreter::interpret(state, nullptr, nullptr, 0, nullptr, nullptr);
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    return (ch == 0x0A) || (ch == 0x0D) || (ch == 0x2028) || (ch == 0x2029);
}

ExtendedNodeLOC ByteCodeBlock::computeNodeLOCFromByteCode(ByteCode* code, CodeBlock* cb)
{
    if (code->m_loc.index == SIZE_MAX) {
        return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
    }
    size_t line = cb->sourceElementStart().line;
    size_t column = cb->sourceElementStart().column;
    for (size_t i = 0; i < code->m_loc.index; i++) {
        char16_t c = cb->src().charAt(i);
        column++;
        if (isLineTerminator(c)) {
            line++;
            column = 0;
        }
    }
    return ExtendedNodeLOC(line, column, code->m_loc.index);
}

void* GetObjectInlineCache::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCache, m_cache));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* SetObjectInlineCache::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_cachedhiddenClassChain));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_hiddenClassWillBe));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* EnumerateObjectData::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(EnumerateObjectData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_hiddenClassChain));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_object));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_keys));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(EnumerateObjectData));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
