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

}
