/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

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
class GetObjectPreComputedCase;
class SetObjectPreComputedCase;
struct GetObjectInlineCache;
struct GetObjectInlineCacheData;
struct SetObjectInlineCache;
struct GlobalVariableAccessCacheItem;
class InitializeGlobalVariable;
class CallFunctionComplexCase;
class CreateFunction;
class InitializeClass;
class SuperReference;
class ComplexSetObjectOperation;
class ComplexGetObjectOperation;
class OpenLexicalEnvironment;
class BlockOperation;
class ReplaceBlockLexicalEnvironmentOperation;
class TryOperation;
class BinaryInstanceOfOperation;
class BinaryInOperation;
class UnaryDelete;
class TemplateOperation;
class DeclareFunctionDeclarations;
class MetaPropertyOperation;
class ObjectDefineOwnPropertyOperation;
class ObjectDefineOwnPropertyWithNameOperation;
class ArrayDefineOwnPropertyOperation;
class ArrayDefineOwnPropertyBySpreadElementOperation;
class CreateSpreadArrayObject;
class ObjectDefineGetterSetter;
class ResolveNameAddress;
class StoreByNameWithAddress;
class GlobalObject;
class UnaryTypeof;
class GetObject;
class SetObjectOperation;
class EnumerateObject;
class CheckLastEnumerateKey;
class TaggedTemplateOperation;
class MarkEnumerateKey;
class CreateEnumerateObject;

class ByteCodeInterpreter {
public:
    static Value interpret(ExecutionState* state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile);

private:
    static Value loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool throwException = true);
    static EnvironmentRecord* getBindedEnvironmentRecordByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, Value& bindedValue, bool throwException = true);
    static void storeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, const Value& value);
    static void initializeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool isLexicallyDeclaredName, const Value& value);
    static void resolveNameAddress(ExecutionState& state, ResolveNameAddress* code, Value* registerFile);
    static void storeByNameWithAddress(ExecutionState& state, StoreByNameWithAddress* code, Value* registerFile);

    static Value plusSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value minusSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value multiplySlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value divisionSlowCase(ExecutionState& state, const Value& a, const Value& b);
    static Value unaryMinusSlowCase(ExecutionState& state, const Value& a);
    static Value modOperation(ExecutionState& state, const Value& left, const Value& right);
    static Value exponentialOperation(ExecutionState& state, const Value& left, const Value& right);
    static void instanceOfOperation(ExecutionState& state, BinaryInstanceOfOperation* code, Value* registerFile);
    static void deleteOperation(ExecutionState& state, LexicalEnvironment* env, UnaryDelete* code, Value* registerFile, ByteCodeBlock* byteCodeBlock);
    static void templateOperation(ExecutionState& state, LexicalEnvironment* env, TemplateOperation* code, Value* registerFile);
    enum BitwiseOperationKind : unsigned {
        And,
        Or,
        Xor,
    };
    static Value bitwiseOperationSlowCase(ExecutionState& state, const Value& a, const Value& b, BitwiseOperationKind kind);
    static Value bitwiseNotOperationSlowCase(ExecutionState& state, const Value& a);
    enum ShiftOperationKind : unsigned {
        Left,
        SignedRight,
        UnsignedRight,
    };
    static Value shiftOperationSlowCase(ExecutionState& state, const Value& a, const Value& b, ShiftOperationKind kind);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-11.8.5
    static bool abstractLeftIsLessThanRightSlowCase(ExecutionState& state, const Value& left, const Value& right, bool switched);
    static bool abstractLeftIsLessThanRight(ExecutionState& state, const Value& left, const Value& right, bool switched);
    static bool abstractLeftIsLessThanEqualRightSlowCase(ExecutionState& state, const Value& left, const Value& right, bool switched);
    static bool abstractLeftIsLessThanEqualRight(ExecutionState& state, const Value& left, const Value& right, bool switched);

    static void getObjectPrecomputedCaseOperation(ExecutionState& state, GetObjectPreComputedCase* code, Value* registerFile, ByteCodeBlock* block);
    static void getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& receiver, GetObjectPreComputedCase* code, Value* registerFile, ByteCodeBlock* block);
    static void setObjectPreComputedCaseOperation(ExecutionState& state, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block);
    static bool setObjectPreComputedCaseOperationSlowCase(ExecutionState& state, Object* obj, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block);
    static void setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block);

    static Object* fastToObject(ExecutionState& state, const Value& obj);

    static Value getGlobalVariableSlowCase(ExecutionState& state, Object* go, GlobalVariableAccessCacheItem* slot, ByteCodeBlock* block);
    static void setGlobalVariableSlowCase(ExecutionState& state, Object* go, GlobalVariableAccessCacheItem* slot, const Value& value, ByteCodeBlock* block);
    static void initializeGlobalVariable(ExecutionState& state, InitializeGlobalVariable* code, const Value& value);

    static Value tryOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile);

    static void createFunctionOperation(ExecutionState& state, CreateFunction* createFunction, ByteCodeBlock* byteCodeBlock, Value* registerFile);
    static ArrayObject* createRestElementOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock);
    static void initializeClassOperation(ExecutionState& state, InitializeClass* code, Value* registerFile);
    static void superOperation(ExecutionState& state, SuperReference* code, Value* registerFile);
    static void complexSetObjectOperation(ExecutionState& state, ComplexSetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock);
    static void complexGetObjectOperation(ExecutionState& state, ComplexGetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock);
    static Value openLexicalEnvironment(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile);
    static Value blockOperation(ExecutionState*& state, BlockOperation* code, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile);
    static void replaceBlockLexicalEnvironmentOperation(ExecutionState& state, size_t programCounter, ByteCodeBlock* byteCodeBlock);
    static void binaryInOperation(ExecutionState& state, BinaryInOperation* code, Value* registerFile);
    static Value constructOperation(ExecutionState& state, const Value& constructor, const size_t argc, Value* argv);
    static void callFunctionComplexCase(ExecutionState& state, CallFunctionComplexCase* code, Value* registerFile, ByteCodeBlock* byteCodeBlock);
    static void spreadFunctionArguments(ExecutionState& state, const Value* argv, const size_t argc, ValueVector& argVector);

    static void createEnumerateObject(ExecutionState& state, CreateEnumerateObject* code, Value* registerFile);
    static void checkLastEnumerateKey(ExecutionState& state, CheckLastEnumerateKey* code, char* codeBuffer, size_t& programCounter, Value* registerFile);
    static void markEnumerateKey(ExecutionState& state, MarkEnumerateKey* code, Value* registerFile);

    static void yieldOperation(ExecutionState& state, Value* registerFile, size_t programCounter, char* codeBuffer);
    static Value yieldDelegateOperation(ExecutionState& state, Value* registerFile, size_t& programCounter, char* codeBuffer);
    static void executionPauseOperation(ExecutionState& state, Value* registerFile, size_t& programCounter, char* codeBuffer);
    static Value executionResumeOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock);

    static void metaPropertyOperation(ExecutionState& state, MetaPropertyOperation* code, ByteCodeBlock* byteCodeBlock, Value* registerFile);

    static void objectDefineOwnPropertyOperation(ExecutionState& state, ObjectDefineOwnPropertyOperation* code, Value* registerFile);
    static void objectDefineOwnPropertyWithNameOperation(ExecutionState& state, ObjectDefineOwnPropertyWithNameOperation* code, ByteCodeBlock* byteCodeBlock, Value* registerFile);
    static void arrayDefineOwnPropertyOperation(ExecutionState& state, ArrayDefineOwnPropertyOperation* code, Value* registerFile);
    static void arrayDefineOwnPropertyBySpreadElementOperation(ExecutionState& state, ArrayDefineOwnPropertyBySpreadElementOperation* code, Value* registerFile);
    static void createSpreadArrayObject(ExecutionState& state, CreateSpreadArrayObject* code, Value* registerFile);
    static void defineObjectGetterSetter(ExecutionState& state, ObjectDefineGetterSetter* code, ByteCodeBlock* byteCodeBlock, Value* registerFile);
    static Value incrementOperation(ExecutionState& state, const Value& value);
    static Value incrementOperationSlowCase(ExecutionState& state, const Value& value);
    static Value decrementOperation(ExecutionState& state, const Value& value);
    static Value decrementOperationSlowCase(ExecutionState& state, const Value& value);

    static void getObjectOpcodeSlowCase(ExecutionState& state, GetObject* code, Value* registerFile);
    static void setObjectOpcodeSlowCase(ExecutionState& state, SetObjectOperation* code, Value* registerFile);

    static void unaryTypeof(ExecutionState& state, UnaryTypeof* code, Value* registerFile);

    static void iteratorOperation(ExecutionState& state, size_t& programCounter, Value* registerFile, char* codeBuffer);
    static void getMethodOperation(ExecutionState& state, size_t programCounter, Value* registerFile);
    static Object* restBindOperation(ExecutionState& state, IteratorRecord* iteratorRecord);

    static void taggedTemplateOperation(ExecutionState& state, size_t& programCounter, Value* registerFile, char* codeBuffer, ByteCodeBlock* byteCodeBlock);

    static void ensureArgumentsObjectOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock, Value* registerFile);

    static int evaluateImportAssertionOperation(ExecutionState& state, const Value& options);
};
} // namespace Escargot

#endif
