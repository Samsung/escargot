#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Context.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "parser/esprima_cpp/esprima.h"


namespace Escargot {

OpcodeTable g_opcodeTable;

OpcodeTable::OpcodeTable()
{
    ExecutionState state(nullptr, nullptr);
    ByteCodeInterpreter::interpret(state, nullptr, 0, nullptr, nullptr);
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    return (ch == 0x0A) || (ch == 0x0D) || (ch == 0x2028) || (ch == 0x2029);
}

void ByteCodeBlock::fillLocDataIfNeeded(Context* c)
{
    if (m_locData.size() || (m_codeBlock->src().length() == 0)) {
        return;
    }

    ByteCodeGenerator g;
    ByteCodeBlock* block;
    if (m_codeBlock->isGlobalScopeCodeBlock()) {
        block = g.generateByteCode(c, m_codeBlock, esprima::parseProgram(m_codeBlock->context(), m_codeBlock->src(), nullptr, m_codeBlock->isStrict()), m_isEvalMode, m_isOnGlobal, true);
    } else {
        block = g.generateByteCode(c, m_codeBlock, c->scriptParser().parseFunction(m_codeBlock), m_isEvalMode, m_isOnGlobal, true);
    }
    m_locData = std::move(block->m_locData);
    // prevent infinate fillLocDataIfNeeded if m_locData.size() == 0 in here
    m_locData.pushBack(std::make_pair(SIZE_MAX, SIZE_MAX));
}

ExtendedNodeLOC ByteCodeBlock::computeNodeLOCFromByteCode(Context* c, size_t codePosition, CodeBlock* cb)
{
    if (codePosition == SIZE_MAX) {
        return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
    }

    fillLocDataIfNeeded(c);

    size_t index = 0;
    for (size_t i = 0; i < m_locData.size(); i++) {
        if (m_locData[i].first == codePosition) {
            index = m_locData[i].second;
            break;
        }
    }

    size_t line = cb->sourceElementStart().line;
    size_t column = cb->sourceElementStart().column;
    for (size_t i = 0; i < index; i++) {
        char16_t c = cb->src().charAt(i);
        column++;
        if (isLineTerminator(c)) {
            // skip \r\n
            if (c == 13 && (i + 1 < index) && cb->src().charAt(i + 1) == 10) {
                i++;
            }
            line++;
            column = 1;
        }
    }
    return ExtendedNodeLOC(line, column, codePosition);
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
