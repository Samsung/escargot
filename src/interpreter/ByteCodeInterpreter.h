#ifndef __EscargotByteCodeInterpreter__
#define __EscargotByteCodeInterpreter__

#include "runtime/Value.h"
#include "runtime/String.h"
#include "runtime/ExecutionState.h"
#include "parser/CodeBlock.h"

namespace Escargot {

class Context;
class ByteCodeBlock;
class LexicalEnvironment;

class ByteCodeInterpreter {
public:
    static void interpret(ExecutionState& state, CodeBlock* codeBlock);
    static Value loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name);
};

}

#endif
