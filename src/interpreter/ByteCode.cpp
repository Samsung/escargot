#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"

namespace Escargot {

OpcodeTable g_opcodeTable;

OpcodeTable::OpcodeTable()
{
    ExecutionState state(nullptr, nullptr);
    ByteCodeInterpreter::interpret(state, nullptr);
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    return (ch == 0x0A) || (ch == 0x0D) || (ch == 0x2028) || (ch == 0x2029);
}

NodeLOC ByteCodeBlock::computeNodeFromByteCode(ByteCode* code, CodeBlock* cb)
{
    if (code->m_loc.index == SIZE_MAX) {
        return NodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
    }
    size_t line = cb->sourceElementStart().line;
    size_t column = cb->sourceElementStart().column;
    for(size_t i = 0; i < code->m_loc.index; i ++) {
        char16_t c = cb->src().charAt(i);
        if (isLineTerminator(c)) {
            line++;
            column = 1;
        }
    }
    return NodeLOC(line, column, code->m_loc.index);
}

}
