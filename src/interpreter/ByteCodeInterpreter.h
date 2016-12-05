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
    static Value plusSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value modOperation(ExecutionState& state, const Value& left, const Value& right);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-11.8.5
    static bool abstractRelationalComparisonSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static inline bool abstractRelationalComparison(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
    {
        // consume very fast case
        if (LIKELY(left.isInt32() && right.isInt32())) {
            return left.asInt32() < right.asInt32();
        }

        if (LIKELY(left.isNumber() && right.isNumber())) {
            return left.asNumber() < right.asNumber();
        }

        return abstractRelationalComparisonSlowCase(state, left, right, leftFirst);
    }

    static bool abstractRelationalComparisonOrEqualSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static inline bool abstractRelationalComparisonOrEqual(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
    {
        // consume very fast case
        if (LIKELY(left.isInt32() && right.isInt32())) {
            return left.asInt32() <= right.asInt32();
        }

        if (LIKELY(left.isNumber() && right.isNumber())) {
            return left.asNumber() <= right.asNumber();
        }

        return abstractRelationalComparisonOrEqualSlowCase(state, left, right, leftFirst);
    }
};

}

#endif
