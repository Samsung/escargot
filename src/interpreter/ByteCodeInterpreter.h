#ifndef __EscargotByteCodeInterpreter__
#define __EscargotByteCodeInterpreter__

#include "parser/CodeBlock.h"
#include "runtime/ExecutionState.h"
#include "runtime/String.h"
#include "runtime/Value.h"

namespace Escargot {

class Context;
class ByteCodeBlock;
class LexicalEnvironment;
struct GetObjectInlineCache;
struct SetObjectInlineCache;
struct EnumerateObjectData;
class GetGlobalObject;
class SetGlobalObject;

class ByteCodeInterpreter {
public:
    static void interpret(ExecutionState& state, CodeBlock* codeBlock, size_t programCounter, Value* stackStorage);
    static Value loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool throwException = true);
    static EnvironmentRecord* getBindedEnvironmentRecordByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, Value& bindedValue, bool throwException = true);
    static Value loadArgumentsObject(ExecutionState& state, ExecutionContext* e);
    static void storeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, const Value& value);
    static Value plusSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value modOperation(ExecutionState& state, const Value& left, const Value& right);
    static Value newOperation(ExecutionState& state, const Value& callee, size_t argc, Value* argv);
    static Value instanceOfOperation(ExecutionState& state, const Value& left, const Value& right);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-11.8.5
    static bool abstractRelationalComparisonSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static inline bool abstractRelationalComparison(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static bool abstractRelationalComparisonOrEqualSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static inline bool abstractRelationalComparisonOrEqual(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);

    static inline Value getObjectPrecomputedCaseOperation(ExecutionState& state, Object* obj, const Value& targetObj, const PropertyName& name, GetObjectInlineCache& inlineCache);
    static Value getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& target, const PropertyName& name, GetObjectInlineCache& inlineCache);
    static inline void setObjectPreComputedCaseOperation(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache);
    static void setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache);

    static EnumerateObjectData* executeEnumerateObject(ExecutionState& state, Object* obj);
    static EnumerateObjectData* updateEnumerateObjectData(ExecutionState& state, EnumerateObjectData* data);

    static inline Object* fastToObject(ExecutionState& state, const Value& obj);

    static Value getGlobalObjectSlowCase(ExecutionState& state, Object* go, GetGlobalObject* code);
    static void setGlobalObjectSlowCase(ExecutionState& state, Object* go, SetGlobalObject* code, const Value& value);
};
}

#endif
