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
class CallFunctionInWithScope;
class WithOperation;
class TryOperation;
class UnaryDelete;
class DeclareFunctionDeclarationsInGlobal;
class GlobalObject;

class ByteCodeInterpreter {
public:
    static Value interpret(ExecutionState& state, ByteCodeBlock* byteCodeBlock, register size_t programCounter, Value* registerFile);
    static Value loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool throwException = true);
    static EnvironmentRecord* getBindedEnvironmentRecordByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, Value& bindedValue, bool throwException = true);
    static Value loadArgumentsObject(ExecutionState& state, ExecutionContext* e, Value* stackStorage);
    static void storeArgumentsObject(ExecutionState& state, ExecutionContext* ec, Value* stackStorage, Value v);
    static void storeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, const Value& value);
    static Value plusSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value modOperation(ExecutionState& state, const Value& left, const Value& right);
    static Object* newOperation(ExecutionState& state, const Value& callee, size_t argc, Value* argv);
    static Value instanceOfOperation(ExecutionState& state, const Value& left, const Value& right);
    static void deleteOperation(ExecutionState& state, LexicalEnvironment* env, UnaryDelete* code, Value* registerFile);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-11.8.5
    static bool abstractRelationalComparisonSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static bool abstractRelationalComparison(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static bool abstractRelationalComparisonOrEqualSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);
    static bool abstractRelationalComparisonOrEqual(ExecutionState& state, const Value& left, const Value& right, bool leftFirst);

    static Value getObjectPrecomputedCaseOperation(ExecutionState& state, Object* obj, const Value& receiver, const PropertyName& name, GetObjectInlineCache& inlineCache, ByteCodeBlock* block);
    static Value getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& receiver, const PropertyName& name, GetObjectInlineCache& inlineCache, ByteCodeBlock* block);
    static void setObjectPreComputedCaseOperation(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block);
    static void setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block);

    static EnumerateObjectData* executeEnumerateObject(ExecutionState& state, Object* obj);
    static EnumerateObjectData* updateEnumerateObjectData(ExecutionState& state, EnumerateObjectData* data);

    static Object* fastToObject(ExecutionState& state, const Value& obj);

    static Value getGlobalObjectSlowCase(ExecutionState& state, Object* go, GetGlobalObject* code, ByteCodeBlock* block);
    static void setGlobalObjectSlowCase(ExecutionState& state, Object* go, SetGlobalObject* code, const Value& value, ByteCodeBlock* block);

    static size_t tryOperation(ExecutionState& state, TryOperation* code, ExecutionContext* ec, LexicalEnvironment* env, size_t programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile);

    static Value withOperation(ExecutionState& state, WithOperation* code, Object* obj, ExecutionContext* ec, LexicalEnvironment* env, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile, Value* stackStorage);
    static Value callFunctionInWithScope(ExecutionState& state, CallFunctionInWithScope* code, ExecutionContext* ec, LexicalEnvironment* env, Value* argv);

    static void declareFunctionDeclarationsInGlobal(ExecutionState& state, DeclareFunctionDeclarationsInGlobal* code, GlobalObject* globalObject, LexicalEnvironment* lexicalEnvironment);

    static void processException(ExecutionState& state, const Value& value, ExecutionContext* ec, size_t programCounter);
};
}

#endif
