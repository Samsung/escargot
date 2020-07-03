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

#ifndef __EscargotByteCode__
#define __EscargotByteCode__

#include "interpreter/ByteCodeGenerator.h"
#include "parser/CodeBlock.h"
#include "parser/ast/Node.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "runtime/EncodedValue.h"
#include "runtime/ExecutionPauser.h"

namespace Escargot {
class ObjectStructure;
class Node;
struct GlobalVariableAccessCacheItem;

// <OpcodeName, PushCount, PopCount>
#define FOR_EACH_BYTECODE_OP(F)                             \
    F(LoadLiteral, 1, 0)                                    \
    F(LoadByName, 1, 0)                                     \
    F(StoreByName, 0, 0)                                    \
    F(InitializeByName, 0, 0)                               \
    F(LoadByHeapIndex, 1, 0)                                \
    F(StoreByHeapIndex, 0, 0)                               \
    F(InitializeByHeapIndex, 0, 0)                          \
    F(NewOperation, 1, 0)                                   \
    F(NewOperationWithSpreadElement, 1, 0)                  \
    F(BinaryPlus, 1, 2)                                     \
    F(BinaryMinus, 1, 2)                                    \
    F(BinaryMultiply, 1, 2)                                 \
    F(BinaryDivision, 1, 2)                                 \
    F(BinaryExponentiation, 1, 2)                           \
    F(BinaryMod, 1, 2)                                      \
    F(BinaryEqual, 1, 2)                                    \
    F(BinaryLessThan, 1, 2)                                 \
    F(BinaryLessThanOrEqual, 1, 2)                          \
    F(BinaryGreaterThan, 1, 2)                              \
    F(BinaryGreaterThanOrEqual, 1, 2)                       \
    F(BinaryNotEqual, 1, 2)                                 \
    F(BinaryStrictEqual, 1, 2)                              \
    F(BinaryNotStrictEqual, 1, 2)                           \
    F(BinaryBitwiseAnd, 1, 2)                               \
    F(BinaryBitwiseOr, 1, 2)                                \
    F(BinaryBitwiseXor, 1, 2)                               \
    F(BinaryLeftShift, 1, 2)                                \
    F(BinarySignedRightShift, 1, 2)                         \
    F(BinaryUnsignedRightShift, 1, 2)                       \
    F(BinaryInOperation, 1, 2)                              \
    F(BinaryInstanceOfOperation, 1, 2)                      \
    F(BreakpointDisabled, 0, 0)                             \
    F(BreakpointEnabled, 0, 0)                              \
    F(CreateObject, 1, 0)                                   \
    F(CreateArray, 1, 0)                                    \
    F(CreateSpreadArrayObject, 1, 0)                        \
    F(CreateFunction, 1, 0)                                 \
    F(CreateClass, 0, 0)                                    \
    F(CreateRestElement, 0, 0)                              \
    F(SuperReference, 1, 0)                                 \
    F(SuperSetObjectOperation, 0, 2)                        \
    F(SuperGetObjectOperation, 1, 2)                        \
    F(LoadThisBinding, 0, 0)                                \
    F(ObjectDefineOwnPropertyOperation, 0, 0)               \
    F(ObjectDefineOwnPropertyWithNameOperation, 0, 0)       \
    F(ArrayDefineOwnPropertyOperation, 0, 0)                \
    F(ArrayDefineOwnPropertyBySpreadElementOperation, 0, 0) \
    F(GetObject, 1, 2)                                      \
    F(SetObjectOperation, 0, 2)                             \
    F(GetObjectPreComputedCase, 1, 1)                       \
    F(SetObjectPreComputedCase, 0, 1)                       \
    F(GetGlobalVariable, 1, 1)                              \
    F(SetGlobalVariable, 0, 1)                              \
    F(InitializeGlobalVariable, 0, 1)                       \
    F(Move, 1, 0)                                           \
    F(Increment, 1, 1)                                      \
    F(Decrement, 1, 1)                                      \
    F(ToNumberIncrement, 2, 2)                              \
    F(ToNumberDecrement, 2, 2)                              \
    F(ToNumber, 1, 1)                                       \
    F(UnaryMinus, 1, 1)                                     \
    F(UnaryNot, 1, 1)                                       \
    F(UnaryBitwiseNot, 1, 1)                                \
    F(UnaryTypeof, 1, 1)                                    \
    F(UnaryDelete, 1, 1)                                    \
    F(TemplateOperation, 1, 1)                              \
    F(Jump, 0, 0)                                           \
    F(JumpComplexCase, 0, 0)                                \
    F(JumpIfTrue, 0, 0)                                     \
    F(JumpIfUndefinedOrNull, 0, 0)                          \
    F(JumpIfFalse, 0, 0)                                    \
    F(JumpIfRelation, 0, 0)                                 \
    F(JumpIfEqual, 0, 0)                                    \
    F(CallFunction, -1, 0)                                  \
    F(CallFunctionWithReceiver, -1, 0)                      \
    F(GetParameter, 0, 0)                                   \
    F(ReturnFunctionSlowCase, 0, 0)                         \
    F(TryOperation, 0, 0)                                   \
    F(TryCatchFinallyWithBlockBodyEnd, 0, 0)                \
    F(ThrowOperation, 0, 0)                                 \
    F(ThrowStaticErrorOperation, 0, 0)                      \
    F(CreateEnumerateObject, 1, 0)                          \
    F(GetEnumerateKey, 1, 0)                                \
    F(CheckLastEnumerateKey, 0, 0)                          \
    F(MarkEnumerateKey, 2, 0)                               \
    F(IteratorOperation, 0, 0)                              \
    F(GetMethod, 0, 0)                                      \
    F(LoadRegexp, 1, 0)                                     \
    F(WithOperation, 0, 0)                                  \
    F(ObjectDefineGetterSetter, 0, 0)                       \
    F(CallFunctionComplexCase, 0, 0)                        \
    F(BindingRestElement, 1, 0)                             \
    F(ExecutionResume, 0, 0)                                \
    F(ExecutionPause, 0, 0)                                 \
    F(NewTargetOperation, 1, 0)                             \
    F(BlockOperation, 0, 0)                                 \
    F(ReplaceBlockLexicalEnvironmentOperation, 0, 0)        \
    F(TaggedTemplateOperation, 0, 0)                        \
    F(EnsureArgumentsObject, 0, 0)                          \
    F(ResolveNameAddress, 1, 0)                             \
    F(StoreByNameWithAddress, 0, 1)                         \
    F(End, 0, 0)


enum Opcode {
#define DECLARE_BYTECODE(name, pushCount, popCount) name##Opcode,
    FOR_EACH_BYTECODE_OP(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
        OpcodeKindEnd,
    // special opcode only used in interpreter
    GetObjectOpcodeSlowCaseOpcode,
    SetObjectOpcodeSlowCaseOpcode,
#if defined(COMPILER_MSVC)
};
#else
};
#endif

struct OpcodeTable {
    void* m_table[OpcodeKindEnd];
    OpcodeTable();
};

extern OpcodeTable g_opcodeTable;

struct ByteCodeLOC {
    size_t index;
#ifndef NDEBUG
    size_t line;
    size_t column;
#endif

    explicit ByteCodeLOC(size_t index)
        : index(index)
#ifndef NDEBUG
        , line(SIZE_MAX)
        , column(SIZE_MAX)
#endif
    {
    }
};

#if defined(NDEBUG) && defined(ESCARGOT_32)
#define BYTECODE_SIZE_CHECK_IN_32BIT(codeName, size) COMPILE_ASSERT(sizeof(codeName) == size, "");
#else
#define BYTECODE_SIZE_CHECK_IN_32BIT(CodeName, Size)
#endif

/* Byte code is never instantiated on the heap, it is part of the byte code stream. */
class ByteCode {
public:
    ByteCode(Opcode code, const ByteCodeLOC& loc)
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
        : m_opcodeInAddress((void*)code)
#else
        : m_opcode(code)
#endif
#ifndef NDEBUG
        , m_loc(loc)
        , m_orgOpcode(code)
#endif
    {
    }

    void assignOpcodeInAddress()
    {
#ifndef NDEBUG
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
        m_orgOpcode = (Opcode)(size_t)m_opcodeInAddress;
#else
        m_orgOpcode = m_opcode;
#endif
#endif
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
        m_opcodeInAddress = g_opcodeTable.m_table[(Opcode)(size_t)m_opcodeInAddress];
#endif
    }

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    void* m_opcodeInAddress;
#else
    Opcode m_opcode;
#endif

#ifndef NDEBUG
    ByteCodeLOC m_loc;
    Opcode m_orgOpcode;

    void dumpCode(size_t pos, const char* byteCodeStart);
    int dumpJumpPosition(size_t pos, const char* byteCodeStart);
#endif
};

class JumpByteCode : public ByteCode {
public:
    explicit JumpByteCode(Opcode code, const ByteCodeLOC& loc, size_t jumpPosition)
        : ByteCode(code, loc)
        , m_jumpPosition(jumpPosition)
    {
    }

    size_t m_jumpPosition;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
#endif
};

class LoadLiteral : public ByteCode {
public:
    LoadLiteral(const ByteCodeLOC& loc, const size_t registerIndex, const Value& v)
        : ByteCode(Opcode::LoadLiteralOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_value(v)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    Value m_value;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("load r%d <- ", (int)m_registerIndex);
        Value v = m_value;
        if (v.isNumber()) {
            printf("%lf", v.asNumber());
        } else if (v.isString()) {
            printf("\"%.32s\"", v.asString()->toUTF8StringData().data());
        } else if (v.isUndefined()) {
            printf("undefined");
        } else if (v.isNull()) {
            printf("null");
        } else if (v.isBoolean()) {
            printf("%s", v.asBoolean() ? "true" : "false");
        } else {
            printf("other value.. sorry");
        }
    }
#endif
};

class NewTargetOperation : public ByteCode {
public:
    NewTargetOperation(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::NewTargetOperationOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("r%d <- new.target", (int)m_registerIndex);
    }
#endif
};

class LoadByName : public ByteCode {
public:
    LoadByName(const ByteCodeLOC& loc, const size_t registerIndex, const AtomicString& name)
        : ByteCode(Opcode::LoadByNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_name(name)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_name;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("load r%d <- %s", (int)m_registerIndex, m_name.string()->toUTF8StringData().data());
    }
#endif
};

class StoreByName : public ByteCode {
public:
    StoreByName(const ByteCodeLOC& loc, const size_t registerIndex, const AtomicString& name)
        : ByteCode(Opcode::StoreByNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_name(name)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_name;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("store %s <- r%d", m_name.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

// only {let, const, function} declaration use this code
class InitializeByName : public ByteCode {
public:
    InitializeByName(const ByteCodeLOC& loc, const size_t registerIndex, const AtomicString& name, bool isLexicallyDeclaredName)
        : ByteCode(Opcode::InitializeByNameOpcode, loc)
        , m_isLexicallyDeclaredName(isLexicallyDeclaredName)
        , m_registerIndex(registerIndex)
        , m_name(name)
    {
    }
    bool m_isLexicallyDeclaredName : 1;
    ByteCodeRegisterIndex m_registerIndex : REGISTER_INDEX_IN_BIT;
    AtomicString m_name;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("initialize %s <- r%d", m_name.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(InitializeByName, sizeof(size_t) * 3);

class LoadByHeapIndex : public ByteCode {
public:
    LoadByHeapIndex(const ByteCodeLOC& loc, const size_t registerIndex, const size_t upperIndex, const size_t index)
        : ByteCode(Opcode::LoadByHeapIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_upperIndex(upperIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_upperIndex;
    ByteCodeRegisterIndex m_index;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("load r%d <- heap[%d][%d]", (int)m_registerIndex, (int)m_upperIndex, (int)m_index);
    }
#endif
};

class StoreByHeapIndex : public ByteCode {
public:
    StoreByHeapIndex(const ByteCodeLOC& loc, const size_t registerIndex, const size_t upperIndex, const size_t index)
        : ByteCode(Opcode::StoreByHeapIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_upperIndex(upperIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_upperIndex;
    ByteCodeRegisterIndex m_index;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("store heap[%d][%d] <- r%d", (int)m_upperIndex, (int)m_index, (int)m_registerIndex);
    }
#endif
};

class InitializeByHeapIndex : public ByteCode {
public:
    InitializeByHeapIndex(const ByteCodeLOC& loc, const size_t registerIndex, const size_t index)
        : ByteCode(Opcode::InitializeByHeapIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_index;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("initialize heap[0][%d] <- r%d", (int)m_index, (int)m_registerIndex);
    }
#endif
};

class CreateFunction : public ByteCode {
public:
    CreateFunction(const ByteCodeLOC& loc, const size_t registerIndex, const size_t homeObjectRegisterIndex, InterpretedCodeBlock* cb)
        : ByteCode(Opcode::CreateFunctionOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_homeObjectRegisterIndex(homeObjectRegisterIndex)
        , m_codeBlock(cb)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_homeObjectRegisterIndex;
    InterpretedCodeBlock* m_codeBlock;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("create ");
        if (m_codeBlock->isArrowFunctionExpression()) {
            printf("arrow");
        }
        printf("function %s -> r%d", m_codeBlock->functionName().string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class CreateClass : public ByteCode {
public:
    CreateClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t classPrototypeRegisterIndex, const size_t superClassRegisterIndex, InterpretedCodeBlock* cb, String* src)
        : ByteCode(Opcode::CreateClassOpcode, loc)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_classPrototypeRegisterIndex(classPrototypeRegisterIndex)
        , m_superClassRegisterIndex(superClassRegisterIndex)
        , m_codeBlock(cb)
        , m_classSrc(src)
    {
    }

    ByteCodeRegisterIndex m_classConstructorRegisterIndex;
    ByteCodeRegisterIndex m_classPrototypeRegisterIndex;
    ByteCodeRegisterIndex m_superClassRegisterIndex;
    InterpretedCodeBlock* m_codeBlock;
    String* m_classSrc;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_superClassRegisterIndex == REGISTER_LIMIT) {
            printf("create class r%d { r%d }", (int)m_classConstructorRegisterIndex, (int)m_classPrototypeRegisterIndex);
        } else {
            printf("create class r%d : r%d { r%d }", (int)m_classConstructorRegisterIndex, (int)m_superClassRegisterIndex, (int)m_classPrototypeRegisterIndex);
        }
    }
#endif
};

class CreateRestElement : public ByteCode {
public:
    CreateRestElement(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::CreateRestElementOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("Create RestElement -> r%d", (int)m_registerIndex);
    }
#endif
};

class SuperReference : public ByteCode {
public:
    SuperReference(const ByteCodeLOC& loc, const size_t dstIndex, bool isCall)
        : ByteCode(Opcode::SuperReferenceOpcode, loc)
        , m_dstIndex(dstIndex)
        , m_isCall(isCall)
    {
    }

    ByteCodeRegisterIndex m_dstIndex : REGISTER_INDEX_IN_BIT;
    bool m_isCall : 1;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_isCall) {
            printf("super constructor -> r%d", (int)m_dstIndex);
        } else {
            printf("super property -> r%d", (int)m_dstIndex);
        }
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(SuperReference, sizeof(size_t) * 2);

class SuperSetObjectOperation : public ByteCode {
public:
    SuperSetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyNameIndex, const size_t loadRegisterIndex)
        : ByteCode(Opcode::SuperSetObjectOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyNameIndex(propertyNameIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    ByteCodeRegisterIndex m_propertyNameIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("set object super(r%d).r%d <- r%d", (int)m_objectRegisterIndex, (int)m_propertyNameIndex, (int)m_loadRegisterIndex);
    }
#endif
};

class SuperGetObjectOperation : public ByteCode {
public:
    SuperGetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, const size_t propertyNameIndex)
        : ByteCode(Opcode::SuperGetObjectOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
        , m_propertyNameIndex(propertyNameIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;
    ByteCodeRegisterIndex m_propertyNameIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get object r%d <- super(r%d).r%d", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, (int)m_propertyNameIndex);
    }
#endif
};

class LoadThisBinding : public ByteCode {
public:
    LoadThisBinding(const ByteCodeLOC& loc, const size_t dstIndex)
        : ByteCode(Opcode::LoadThisBindingOpcode, loc)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_dstIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("load this binding -> r%d", (int)m_dstIndex);
    }
#endif
};

#ifdef NDEBUG
#define DEFINE_BINARY_OPERATION_DUMP(name)
#else
#define DEFINE_BINARY_OPERATION_DUMP(name)                                                     \
    void dump(const char* byteCodeStart)                                                       \
    {                                                                                          \
        printf(name " r%d <- r%d , r%d", (int)m_dstIndex, (int)m_srcIndex0, (int)m_srcIndex1); \
    }
#endif

#define DEFINE_BINARY_OPERATION(CodeName, HumanName)                                                                                      \
    class Binary##CodeName : public ByteCode {                                                                                            \
    public:                                                                                                                               \
        Binary##CodeName(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1, const size_t dstRegisterIndex) \
            : ByteCode(Opcode::Binary##CodeName##Opcode, loc)                                                                             \
            , m_srcIndex0(registerIndex0)                                                                                                 \
            , m_srcIndex1(registerIndex1)                                                                                                 \
            , m_dstIndex(dstRegisterIndex)                                                                                                \
        {                                                                                                                                 \
        }                                                                                                                                 \
        ByteCodeRegisterIndex m_srcIndex0;                                                                                                \
        ByteCodeRegisterIndex m_srcIndex1;                                                                                                \
        ByteCodeRegisterIndex m_dstIndex;                                                                                                 \
        DEFINE_BINARY_OPERATION_DUMP(HumanName)                                                                                           \
    };

DEFINE_BINARY_OPERATION(BitwiseAnd, "bitwise and");
DEFINE_BINARY_OPERATION(BitwiseOr, "bitwise or");
DEFINE_BINARY_OPERATION(BitwiseXor, "bitwise Xor");
DEFINE_BINARY_OPERATION(Division, "division");
DEFINE_BINARY_OPERATION(Equal, "equal");
DEFINE_BINARY_OPERATION(Exponentiation, "exponentiation operation");
DEFINE_BINARY_OPERATION(GreaterThan, "greaterthan");
DEFINE_BINARY_OPERATION(GreaterThanOrEqual, "greaterthan or equal");
DEFINE_BINARY_OPERATION(InOperation, "in operation");
DEFINE_BINARY_OPERATION(InstanceOfOperation, "instance of");
DEFINE_BINARY_OPERATION(LeftShift, "left shift");
DEFINE_BINARY_OPERATION(LessThan, "lessthan");
DEFINE_BINARY_OPERATION(LessThanOrEqual, "lessthan or equal");
DEFINE_BINARY_OPERATION(Minus, "minus");
DEFINE_BINARY_OPERATION(Mod, "mod");
DEFINE_BINARY_OPERATION(Multiply, "multiply");
DEFINE_BINARY_OPERATION(NotEqual, "notequal");
DEFINE_BINARY_OPERATION(NotStrictEqual, "not strict equal");
DEFINE_BINARY_OPERATION(Plus, "plus");
DEFINE_BINARY_OPERATION(SignedRightShift, "signed right shift");
DEFINE_BINARY_OPERATION(StrictEqual, "strict equal");
DEFINE_BINARY_OPERATION(UnsignedRightShift, "unsigned right shift");

class BreakpointDisabled : public ByteCode {
public:
    BreakpointDisabled(const ByteCodeLOC& loc)
        : ByteCode(Opcode::BreakpointDisabledOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("breakpointDisabled");
    }
#endif
};

class BreakpointEnabled : public ByteCode {
public:
    BreakpointEnabled(const ByteCodeLOC& loc)
        : ByteCode(Opcode::BreakpointDisabledOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("breakpointEnabled");
    }
#endif
};

class CreateObject : public ByteCode {
public:
    CreateObject(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::CreateObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("createobject -> r%d", (int)m_registerIndex);
    }
#endif
};

class CreateArray : public ByteCode {
public:
    CreateArray(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::CreateArrayOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_length(0)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_length;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("createarray -> r%d", (int)m_registerIndex);
    }
#endif
};

class CreateSpreadArrayObject : public ByteCode {
public:
    CreateSpreadArrayObject(const ByteCodeLOC& loc, const size_t registerIndex, const size_t argumentIndex)
        : ByteCode(Opcode::CreateSpreadArrayObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_argumentIndex(argumentIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_argumentIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("create spread array object(r%d) -> r%d", (int)m_argumentIndex, (int)m_registerIndex);
    }
#endif
};

class GetObject : public ByteCode {
public:
    GetObject(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyRegisterIndex, const size_t storeRegisterIndex)
        : ByteCode(Opcode::GetObjectOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyRegisterIndex(propertyRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_propertyRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get object r%d <- r%d[r%d]", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex);
    }
#endif
};

class SetObjectOperation : public ByteCode {
public:
    SetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyRegisterIndex, const size_t loadRegisterIndex)
        : ByteCode(Opcode::SetObjectOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyRegisterIndex(propertyRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_propertyRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("set object r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};

class ObjectDefineOwnPropertyOperation : public ByteCode {
public:
    ObjectDefineOwnPropertyOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyRegisterIndex, const size_t loadRegisterIndex, ObjectPropertyDescriptor::PresentAttribute presentAttribute, bool redefineFunctionOrClassName)
        : ByteCode(Opcode::ObjectDefineOwnPropertyOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyRegisterIndex(propertyRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_presentAttribute(presentAttribute)
        , m_redefineFunctionOrClassName(redefineFunctionOrClassName)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_propertyRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    ObjectPropertyDescriptor::PresentAttribute m_presentAttribute : 8;
    bool m_redefineFunctionOrClassName : 1;
#ifndef NDEBUG

    void dump(const char* byteCodeStart)
    {
        printf("object define own property r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(ObjectDefineOwnPropertyOperation, sizeof(size_t) * 3);

class ObjectDefineOwnPropertyWithNameOperation : public ByteCode {
public:
    ObjectDefineOwnPropertyWithNameOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, AtomicString propertyName, const size_t loadRegisterIndex, ObjectPropertyDescriptor::PresentAttribute presentAttribute)
        : ByteCode(Opcode::ObjectDefineOwnPropertyWithNameOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyName(propertyName)
        , m_presentAttribute(presentAttribute)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    AtomicString m_propertyName;
    ObjectPropertyDescriptor::PresentAttribute m_presentAttribute;
#ifndef NDEBUG

    void dump(const char* byteCodeStart)
    {
        printf("object define own property with name r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(ObjectDefineOwnPropertyWithNameOperation, sizeof(size_t) * 4);

#define ARRAY_DEFINE_OPERATION_MERGE_COUNT 8

class ArrayDefineOwnPropertyOperation : public ByteCode {
public:
    ArrayDefineOwnPropertyOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const uint32_t baseIndex, uint8_t count)
        : ByteCode(Opcode::ArrayDefineOwnPropertyOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_count(count)
        , m_baseIndex(baseIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex : REGISTER_INDEX_IN_BIT;
    uint8_t m_count : 8;
    uint32_t m_baseIndex;
    ByteCodeRegisterIndex m_loadRegisterIndexs[ARRAY_DEFINE_OPERATION_MERGE_COUNT];

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("array define own property r%d[%d - %d] <- ", (int)m_objectRegisterIndex, (int)m_baseIndex, (int)(m_baseIndex + m_count));
        for (int i = 0; i < m_count; i++) {
            if (m_loadRegisterIndexs[i] == REGISTER_LIMIT) {
                printf(", ");
            } else {
                printf("r%d, ", m_loadRegisterIndexs[i]);
            }
        }
    }
#endif
};

class ArrayDefineOwnPropertyBySpreadElementOperation : public ByteCode {
public:
    ArrayDefineOwnPropertyBySpreadElementOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, uint8_t count)
        : ByteCode(Opcode::ArrayDefineOwnPropertyBySpreadElementOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_count(count)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex : REGISTER_INDEX_IN_BIT;
    uint8_t m_count : 8;
    ByteCodeRegisterIndex m_loadRegisterIndexs[ARRAY_DEFINE_OPERATION_MERGE_COUNT];

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("array define own property by spread element r%d[ - ] <- ", (int)m_objectRegisterIndex);
        for (int i = 0; i < m_count; i++) {
            if (m_loadRegisterIndexs[i] == REGISTER_LIMIT) {
                printf(", ");
            } else {
                printf("r%d, ", m_loadRegisterIndexs[i]);
            }
        }
    }
#endif
};

class ObjectStructureChainItem : public gc {
public:
    ObjectStructureChainItem()
        : m_objectStructure(nullptr)
    {
    }

    ObjectStructureChainItem(ObjectStructure* structure)
        : m_objectStructure(structure)
    {
    }

    ObjectStructure* m_objectStructure;

    bool operator==(const ObjectStructureChainItem& item) const
    {
        return m_objectStructure == item.m_objectStructure;
    }

    bool operator!=(const ObjectStructureChainItem& item) const
    {
        return !operator==(item);
    }
};

struct GetObjectInlineCacheData {
    GetObjectInlineCacheData()
    {
        m_cachedhiddenClassChain = nullptr;
        m_cachedhiddenClassChainLength = 0;
        m_cachedIndex = 0;
    }

    union {
        ObjectStructure** m_cachedhiddenClassChain;
        ObjectStructure* m_cachedhiddenClass;
    };
    size_t m_cachedhiddenClassChainLength;
    size_t m_cachedIndex;
};

typedef Vector<GetObjectInlineCacheData, CustomAllocator<GetObjectInlineCacheData>, ComputeReservedCapacityFunctionWithLog2<>> GetObjectInlineCacheDataVector;

struct GetObjectInlineCache {
    GetObjectInlineCache()
    {
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    GetObjectInlineCacheDataVector m_cache;
};

class GetObjectPreComputedCase : public ByteCode {
public:
    // [object] -> [value]
    GetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, ObjectStructurePropertyName propertyName)
        : ByteCode(Opcode::GetObjectPreComputedCaseOpcode, loc)
        , m_isLength(propertyName.plainString()->equals("length"))
        , m_cacheMissCount(0)
        , m_inlineCache(nullptr)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    bool m_isLength : 1;
    uint16_t m_cacheMissCount : 16;
    GetObjectInlineCache* m_inlineCache;

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;
    ObjectStructurePropertyName m_propertyName;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get object r%d <- r%d.%s", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data());
    }
#endif
};

struct SetObjectInlineCache {
    union {
        ObjectStructure** m_cachedHiddenClassChainData;
        ObjectStructure* m_cachedHiddenClass;
    };
    size_t m_cachedhiddenClassChainLength;
    union {
        size_t m_cachedIndex;
        ObjectStructure* m_hiddenClassWillBe;
    };

    SetObjectInlineCache()
    {
        m_cachedHiddenClass = m_hiddenClassWillBe = nullptr;
        m_cachedhiddenClassChainLength = 0;
    }

    void invalidateCache()
    {
        m_cachedHiddenClass = m_hiddenClassWillBe = nullptr;
        m_cachedhiddenClassChainLength = 0;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class SetObjectPreComputedCase : public ByteCode {
public:
    SetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t objectRegisterIndex, ObjectStructurePropertyName propertyName, const size_t loadRegisterIndex)
        : ByteCode(Opcode::SetObjectPreComputedCaseOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyName(propertyName)
        , m_inlineCache(nullptr)
        , m_isLength(propertyName.plainString()->equals("length"))
        , m_missCount(0)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    ObjectStructurePropertyName m_propertyName;
    SetObjectInlineCache* m_inlineCache;
    bool m_isLength : 1;
    uint16_t m_missCount : 16;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("set object r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

class GetGlobalVariable : public ByteCode {
public:
    GetGlobalVariable(const ByteCodeLOC& loc, const size_t registerIndex, GlobalVariableAccessCacheItem* slot)
        : ByteCode(Opcode::GetGlobalVariableOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_slot(slot)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    GlobalVariableAccessCacheItem* m_slot;

#ifndef NDEBUG
    void dump(const char* byteCodeStart);
#endif
};

class SetGlobalVariable : public ByteCode {
public:
    SetGlobalVariable(const ByteCodeLOC& loc, const size_t registerIndex, GlobalVariableAccessCacheItem* slot)
        : ByteCode(Opcode::SetGlobalVariableOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_slot(slot)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    GlobalVariableAccessCacheItem* m_slot;

#ifndef NDEBUG
    void dump(const char* byteCodeStart);
#endif
};

class InitializeGlobalVariable : public ByteCode {
public:
    InitializeGlobalVariable(const ByteCodeLOC& loc, const size_t registerIndex, AtomicString variableName)
        : ByteCode(Opcode::InitializeGlobalVariableOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_variableName(variableName)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_variableName;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("initialize global variable %s <- r%d", m_variableName.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class Move : public ByteCode {
public:
    Move(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1) // 1 <= 0
        : ByteCode(Opcode::MoveOpcode, loc),
          m_registerIndex0(registerIndex0),
          m_registerIndex1(registerIndex1)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("mov r%d <- r%d", (int)m_registerIndex1, (int)m_registerIndex0);
    }
#endif
};

class ToNumber : public ByteCode {
public:
    ToNumber(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::ToNumberOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("to number r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class Increment : public ByteCode {
public:
    Increment(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::IncrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("increment r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class ToNumberIncrement : public ByteCode {
public:
    ToNumberIncrement(const ByteCodeLOC& loc, const size_t srcIndex, const size_t storeIndex, const size_t dstIndex)
        : ByteCode(Opcode::ToNumberIncrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_storeIndex(storeIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_storeIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("to number increment(r%d) -> r%d, r%d", (int)m_srcIndex, (int)m_storeIndex, (int)m_dstIndex);
    }
#endif
};

class Decrement : public ByteCode {
public:
    Decrement(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::DecrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("decrement r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class ToNumberDecrement : public ByteCode {
public:
    ToNumberDecrement(const ByteCodeLOC& loc, const size_t srcIndex, const size_t storeIndex, const size_t dstIndex)
        : ByteCode(Opcode::ToNumberDecrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_storeIndex(storeIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_storeIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("to number decrement(r%d) -> r%d, r%d", (int)m_srcIndex, (int)m_storeIndex, (int)m_dstIndex);
    }
#endif
};

class UnaryMinus : public ByteCode {
public:
    UnaryMinus(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::UnaryMinusOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("unary minus r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryNot : public ByteCode {
public:
    UnaryNot(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::UnaryNotOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("unary not r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryBitwiseNot : public ByteCode {
public:
    UnaryBitwiseNot(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex)
        : ByteCode(Opcode::UnaryBitwiseNotOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("unary bitwise not r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryTypeof : public ByteCode {
public:
    UnaryTypeof(const ByteCodeLOC& loc, const size_t srcIndex, const size_t dstIndex, AtomicString name)
        : ByteCode(Opcode::UnaryTypeofOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
        , m_id(name)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;
    AtomicString m_id;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("unary typeof r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryDelete : public ByteCode {
public:
    UnaryDelete(const ByteCodeLOC& loc, const size_t srcIndex0, const size_t srcIndex1, const size_t dstIndex, AtomicString name, bool hasSuperExpression)
        : ByteCode(Opcode::UnaryDeleteOpcode, loc)
        , m_hasSuperExpression(hasSuperExpression)
        , m_srcIndex0(srcIndex0)
        , m_srcIndex1(srcIndex1)
        , m_dstIndex(dstIndex)
        , m_id(name)
    {
    }
    bool m_hasSuperExpression;
    ByteCodeRegisterIndex m_srcIndex0;
    ByteCodeRegisterIndex m_srcIndex1;
    ByteCodeRegisterIndex m_dstIndex;
    AtomicString m_id;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("unary delete r%d <- r%d r%d %s", (int)m_dstIndex, (int)m_srcIndex0, (int)m_srcIndex1, m_id.string()->toUTF8StringData().data());
    }
#endif
};

class TemplateOperation : public ByteCode {
public:
    TemplateOperation(const ByteCodeLOC& loc, const size_t src0Index, const size_t src1Index, const size_t dstIndex)
        : ByteCode(Opcode::TemplateOperationOpcode, loc)
        , m_src0Index(src0Index)
        , m_src1Index(src1Index)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_src0Index;
    ByteCodeRegisterIndex m_src1Index;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("template operation(+) r%d <- r%d + r%d", (int)m_dstIndex, (int)m_src0Index, (int)m_src1Index);
    }
#endif
};

class Jump : public ByteCode {
public:
    Jump(const ByteCodeLOC& loc, size_t pos = SIZE_MAX)
        : ByteCode(Opcode::JumpOpcode, loc)
        , m_jumpPosition(pos)
    {
    }

    size_t m_jumpPosition;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("jump %d", dumpJumpPosition(m_jumpPosition, byteCodeStart));
    }
#endif
};

class ControlFlowRecord : public gc {
    friend class ByteCodeInterpreter;

public:
    enum ControlFlowReason {
        NeedsReturn,
        NeedsJump,
        NeedsThrow,
    };

    ControlFlowRecord(const ControlFlowReason& reason, const Value& value, size_t count = 0, size_t outerLimitCount = SIZE_MAX)
        : m_reason(reason)
        , m_value(value)
        , m_count(count)
        , m_outerLimitCount(outerLimitCount)
    {
    }

    ControlFlowRecord(const ControlFlowReason& reason, const size_t value, size_t count = 0, size_t outerLimitCount = SIZE_MAX)
        : m_reason(reason)
        , m_wordValue(value)
        , m_count(count)
        , m_outerLimitCount(outerLimitCount)
    {
    }

    const ControlFlowReason& reason()
    {
        return m_reason;
    }

    const Value& value()
    {
        return m_value;
    }

    void setValue(const Value& value)
    {
        m_value = value;
    }

    const size_t& wordValue()
    {
        return m_wordValue;
    }

    void setWordValue(const size_t value)
    {
        m_wordValue = value;
    }

    size_t count()
    {
        return m_count;
    }

    size_t outerLimitCount()
    {
        return m_outerLimitCount;
    }

    ControlFlowRecord* clone()
    {
        return new ControlFlowRecord(m_reason, m_value, m_count, m_outerLimitCount);
    }

private:
    ControlFlowReason m_reason;
    union {
        Value m_value;
        size_t m_wordValue;
    };
    // m_count is for saving tryStatementScopeCount of the context which contains
    // the occurrence(departure point) of this controlflow (e.g. break;)
    size_t m_count;
    // m_outerLimitCount is for saving tryStatementScopeCount of the context which contains
    // the destination of this controlflow
    size_t m_outerLimitCount;
};

class JumpComplexCase : public ByteCode {
public:
    JumpComplexCase(ControlFlowRecord* controlFlowRecord)
        : ByteCode(Opcode::JumpComplexCaseOpcode, ByteCodeLOC(SIZE_MAX))
        , m_controlFlowRecord(controlFlowRecord)
    {
    }

    ControlFlowRecord* m_controlFlowRecord;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("jump complex");

        printf(" out count %d ", (int)m_controlFlowRecord->count());

        if (m_controlFlowRecord->reason() == ControlFlowRecord::NeedsReturn) {
            printf("(return)");
        } else if (m_controlFlowRecord->reason() == ControlFlowRecord::NeedsJump) {
            printf("(jump -> %d)", (int)m_controlFlowRecord->wordValue());
        } else if (m_controlFlowRecord->reason() == ControlFlowRecord::NeedsThrow) {
            printf("(throw)");
        }
    }
#endif
};

COMPILE_ASSERT(sizeof(Jump) == sizeof(JumpComplexCase), "");

class JumpIfTrue : public JumpByteCode {
public:
    JumpIfTrue(const ByteCodeLOC& loc, const size_t registerIndex)
        : JumpByteCode(Opcode::JumpIfTrueOpcode, loc, SIZE_MAX)
        , m_registerIndex(registerIndex)
    {
    }

    JumpIfTrue(const ByteCodeLOC& loc, const size_t registerIndex, size_t pos)
        : JumpByteCode(Opcode::JumpIfTrueOpcode, loc, pos)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("jump if true r%d -> %d", (int)m_registerIndex, dumpJumpPosition(m_jumpPosition, byteCodeStart));
    }
#endif
};

class JumpIfUndefinedOrNull : public JumpByteCode {
public:
    JumpIfUndefinedOrNull(const ByteCodeLOC& loc, bool shouldNegate, const size_t registerIndex)
        : JumpByteCode(Opcode::JumpIfUndefinedOrNullOpcode, loc, SIZE_MAX)
        , m_shouldNegate(shouldNegate)
        , m_registerIndex(registerIndex)
    {
    }

    JumpIfUndefinedOrNull(const ByteCodeLOC& loc, bool shouldNegate, const size_t registerIndex, size_t pos)
        : JumpByteCode(Opcode::JumpIfUndefinedOrNullOpcode, loc, pos)
        , m_shouldNegate(shouldNegate)
        , m_registerIndex(registerIndex)
    {
    }

    bool m_shouldNegate;
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_shouldNegate) {
            printf("jump if not undefined nor null r%d -> %d", (int)m_registerIndex, dumpJumpPosition(m_jumpPosition, byteCodeStart));
        } else {
            printf("jump if undefined or null r%d -> %d", (int)m_registerIndex, dumpJumpPosition(m_jumpPosition, byteCodeStart));
        }
    }
#endif
};

class JumpIfFalse : public JumpByteCode {
public:
    JumpIfFalse(const ByteCodeLOC& loc, const size_t registerIndex)
        : JumpByteCode(Opcode::JumpIfFalseOpcode, loc, SIZE_MAX)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("jump if false r%d -> %d", (int)m_registerIndex, dumpJumpPosition(m_jumpPosition, byteCodeStart));
    }
#endif
};

class JumpIfRelation : public JumpByteCode {
public:
    JumpIfRelation(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1, bool isEqual, bool isLeftFirst)
        : JumpByteCode(Opcode::JumpIfRelationOpcode, loc, SIZE_MAX)
        , m_registerIndex0(registerIndex0)
        , m_registerIndex1(registerIndex1)
        , m_isEqual(isEqual)
        , m_isLeftFirst(isLeftFirst)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;
    bool m_isEqual;
    bool m_isLeftFirst;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        char* op;
        if (m_isEqual) {
            if (m_isLeftFirst) {
                op = (char*)"less than or equal";
            } else {
                op = (char*)"greater than or equal";
            }
        } else {
            if (m_isLeftFirst) {
                op = (char*)"less than";
            } else {
                op = (char*)"greater than";
            }
        }
        printf("jump if not %s (r%d, r%d) -> %d", op, (int)m_registerIndex0, (int)m_registerIndex1, dumpJumpPosition(m_jumpPosition, byteCodeStart));
    }
#endif
};

class JumpIfEqual : public JumpByteCode {
public:
    JumpIfEqual(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1, bool shouldNegate, bool isStrict)
        : JumpByteCode(Opcode::JumpIfEqualOpcode, loc, SIZE_MAX)
        , m_registerIndex0(registerIndex0)
        , m_registerIndex1(registerIndex1)
        , m_shouldNegate(shouldNegate)
        , m_isStrict(isStrict)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;
    bool m_shouldNegate;
    bool m_isStrict;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        char* op;
        if (m_isStrict) {
            if (m_shouldNegate) {
                op = (char*)"not strict equal";
            } else {
                op = (char*)"strict equal";
            }
        } else {
            if (m_shouldNegate) {
                op = (char*)"not equal";
            } else {
                op = (char*)"equal";
            }
        }
        printf("jump if %s (r%d, r%d) -> %d", op, (int)m_registerIndex0, (int)m_registerIndex1, dumpJumpPosition(m_jumpPosition, byteCodeStart));
    }
#endif
};

class CallFunction : public ByteCode {
public:
    CallFunction(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallFunctionOpcode, loc)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_resultIndex(resultIndex)
        , m_argumentCount(argumentCount)
    {
    }
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    ByteCodeRegisterIndex m_resultIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("call r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionWithReceiver : public ByteCode {
public:
    CallFunctionWithReceiver(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallFunctionWithReceiverOpcode, loc)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_resultIndex(resultIndex)
        , m_argumentCount(argumentCount)
    {
    }

    ByteCodeRegisterIndex m_receiverIndex;
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    ByteCodeRegisterIndex m_resultIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("call r%d <- r%d,r%d(r%d-r%d)", (int)m_resultIndex, (int)m_receiverIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionComplexCase : public ByteCode {
public:
    enum Kind {
        WithSpreadElement,
        MayBuiltinApply,
        MayBuiltinEval,
        InWithScope,
        Super
    };

    CallFunctionComplexCase(const ByteCodeLOC& loc, Kind kind,
                            bool inWithScope, bool hasSpreadElement, bool isOptional,
                            const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallFunctionComplexCaseOpcode, loc)
        , m_kind(kind)
        , m_inWithScope(inWithScope)
        , m_hasSpreadElement(hasSpreadElement)
        , m_isOptional(isOptional)
        , m_argumentCount(argumentCount)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_resultIndex(resultIndex)
    {
    }

    CallFunctionComplexCase(const ByteCodeLOC& loc, bool hasSpreadElement, bool isOptional,
                            AtomicString calleeName, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallFunctionComplexCaseOpcode, loc)
        , m_kind(InWithScope)
        , m_inWithScope(true)
        , m_hasSpreadElement(hasSpreadElement)
        , m_isOptional(isOptional)
        , m_argumentCount(argumentCount)
        , m_calleeName(calleeName)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_resultIndex(resultIndex)
    {
    }

    Kind m_kind : 3;
    bool m_inWithScope : 1;
    bool m_hasSpreadElement : 1;
    bool m_isOptional : 1;
    uint16_t m_argumentCount : 16;

    union {
        struct {
            ByteCodeRegisterIndex m_receiverIndex;
            ByteCodeRegisterIndex m_calleeIndex;
        };
        AtomicString m_calleeName; // used with InWithScope
    };
    ByteCodeRegisterIndex m_argumentsStartIndex;
    ByteCodeRegisterIndex m_resultIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_kind == Kind::InWithScope) {
            printf("call(complex inWith) r%d <- %s(r%d-r%d)", (int)m_resultIndex, m_calleeName.string()->toNonGCUTF8StringData().data(), (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
        } else {
            if (m_receiverIndex != REGISTER_LIMIT) {
                printf("call(complex %d) r%d <- r%d,r%d(r%d-r%d)", (int)m_kind, (int)m_resultIndex, (int)m_receiverIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
            } else {
                printf("call(complex %d) r%d <- r%d(r%d-r%d)", (int)m_kind, (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
            }
        }
    }
#endif
};

class ExecutionResume : public ByteCode {
public:
    ExecutionResume(const ByteCodeLOC& loc, ExecutionPauser* pauser)
        : ByteCode(Opcode::ExecutionResumeOpcode, loc)
        , m_needsReturn(false)
        , m_needsThrow(false)
        , m_pauser(pauser)
    {
    }

    bool m_needsReturn;
    bool m_needsThrow;
    ExecutionPauser* m_pauser;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("execution resume");
    }
#endif
};

class ExecutionPause : public ByteCode {
public:
    enum Reason {
        Yield,
        Await,
        GeneratorsInitialize
    };

    struct ExecutionPauseYieldData {
        bool m_needsToWrapYieldValueWithIterResultObject;
        ByteCodeRegisterIndex m_yieldIndex;
        ByteCodeRegisterIndex m_dstIndex;
        ByteCodeRegisterIndex m_dstStateIndex;
        size_t m_tailDataLength;
    };

    struct ExecutionPauseAwaitData {
        ByteCodeRegisterIndex m_awaitIndex;
        ByteCodeRegisterIndex m_dstIndex;
        ByteCodeRegisterIndex m_dstStateIndex;
        size_t m_tailDataLength;
    };

    struct ExecutionPauseGeneratorsInitializeData {
        size_t m_tailDataLength;
    };

    ExecutionPause(const ByteCodeLOC& loc, ExecutionPauseYieldData yieldData)
        : ByteCode(Opcode::ExecutionPauseOpcode, loc)
        , m_reason(Reason::Yield)
        , m_yieldData(yieldData)
    {
    }

    ExecutionPause(const ByteCodeLOC& loc, ExecutionPauseAwaitData awaitData)
        : ByteCode(Opcode::ExecutionPauseOpcode, loc)
        , m_reason(Reason::Await)
        , m_awaitData(awaitData)
    {
    }

    ExecutionPause(const ByteCodeLOC& loc, ExecutionPauseGeneratorsInitializeData data)
        : ByteCode(Opcode::ExecutionPauseOpcode, loc)
        , m_reason(Reason::GeneratorsInitialize)
        , m_asyncGeneratorInitializeData(data)
    {
    }

    Reason m_reason;
    union {
        ExecutionPauseYieldData m_yieldData;
        ExecutionPauseAwaitData m_awaitData;
        ExecutionPauseGeneratorsInitializeData m_asyncGeneratorInitializeData;
    };

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("execution pause ");

        if (m_reason == Reason::Yield) {
            printf("yield r%d r%d r%d", (int)m_yieldData.m_yieldIndex, (int)m_yieldData.m_dstIndex, (int)m_yieldData.m_dstStateIndex);
        } else if (m_reason == Reason::Await) {
            printf("await r%d r%d r%d", (int)m_awaitData.m_awaitIndex, (int)m_awaitData.m_dstIndex, (int)m_awaitData.m_dstStateIndex);
        } else if (m_reason == Reason::GeneratorsInitialize) {
            printf("generators initialize ");
        }
    }
#endif
};

class NewOperation : public ByteCode {
public:
    NewOperation(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount, const size_t resultIndex)
        : ByteCode(Opcode::NewOperationOpcode, loc)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
    {
    }

    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("new r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class NewOperationWithSpreadElement : public ByteCode {
public:
    NewOperationWithSpreadElement(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount, const size_t resultIndex)
        : ByteCode(Opcode::NewOperationWithSpreadElementOpcode, loc)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
    {
    }

    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("new(spread) r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class GetParameter : public ByteCode {
public:
    explicit GetParameter(const ByteCodeLOC& loc, const size_t dstIndex, const size_t paramIndex)
        : ByteCode(Opcode::GetParameterOpcode, loc)
        , m_registerIndex(dstIndex)
        , m_paramIndex(paramIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    uint16_t m_paramIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get parameter r%d <- argv[%d]", (int)m_registerIndex, (int)m_paramIndex);
    }
#endif
};

class ReturnFunctionSlowCase : public ByteCode {
public:
    ReturnFunctionSlowCase(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::ReturnFunctionSlowCaseOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("return r%d", (int)m_registerIndex);
    }
#endif
};

class TryOperation : public ByteCode {
public:
    explicit TryOperation(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryOperationOpcode, loc)
    {
        m_isTryResumeProcess = m_isCatchResumeProcess = m_isFinallyResumeProcess = m_hasCatch = m_hasFinalizer = false;
        m_finallyEndPosition = m_tryCatchEndPosition = m_catchPosition = SIZE_MAX;
        m_catchedValueRegisterIndex = REGISTER_LIMIT;
    }

    bool m_hasCatch : 1;
    bool m_hasFinalizer : 1;
    bool m_isTryResumeProcess : 1;
    bool m_isCatchResumeProcess : 1;
    bool m_isFinallyResumeProcess : 1;
    ByteCodeRegisterIndex m_catchedValueRegisterIndex;
    size_t m_catchPosition;
    size_t m_tryCatchEndPosition;
    size_t m_finallyEndPosition;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("try (hasCatch:%d, save catched value to:r%d, hasFinalizer:%d)", (int)m_hasCatch, (int)m_catchedValueRegisterIndex, (int)m_hasFinalizer);
    }
#endif
};

class TryCatchFinallyWithBlockBodyEnd : public ByteCode {
public:
    explicit TryCatchFinallyWithBlockBodyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryCatchFinallyWithBlockBodyEndOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("try-catch-finally-with-block");
    }
#endif
};

class ThrowOperation : public ByteCode {
public:
    ThrowOperation(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::ThrowOperationOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("throw r%d", (int)m_registerIndex);
    }
#endif
};

class ThrowStaticErrorOperation : public ByteCode {
public:
    ThrowStaticErrorOperation(const ByteCodeLOC& loc, char errorKind, const char* errorMessage, AtomicString templateDataString = AtomicString())
        : ByteCode(Opcode::ThrowStaticErrorOperationOpcode, loc)
        , m_errorKind(errorKind)
        , m_errorMessage(errorMessage)
        , m_templateDataString(templateDataString)
    {
    }
    char m_errorKind;
    const char* m_errorMessage;
    AtomicString m_templateDataString;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("throw static error %s", m_errorMessage);
    }
#endif
};

class CreateEnumerateObject : public ByteCode {
public:
    explicit CreateEnumerateObject(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CreateEnumerateObjectOpcode, loc)
        , m_objectRegisterIndex(REGISTER_LIMIT)
        , m_dataRegisterIndex(REGISTER_LIMIT)
        , m_isDestruction(false)
    {
    }

    explicit CreateEnumerateObject(const ByteCodeLOC& loc, size_t objIndex, size_t dataIndex, bool isDestruction)
        : ByteCode(Opcode::CreateEnumerateObjectOpcode, loc)
        , m_objectRegisterIndex(objIndex)
        , m_dataRegisterIndex(dataIndex)
        , m_isDestruction(isDestruction)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;
    bool m_isDestruction : 1;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("create enumerate object r%d <- r%d", (int)m_dataRegisterIndex, (int)m_objectRegisterIndex);
    }
#endif
};

class GetEnumerateKey : public ByteCode {
public:
    explicit GetEnumerateKey(const ByteCodeLOC& loc)
        : ByteCode(Opcode::GetEnumerateKeyOpcode, loc)
    {
        m_dataRegisterIndex = m_registerIndex = REGISTER_LIMIT;
    }
    explicit GetEnumerateKey(const ByteCodeLOC& loc, size_t dstIndex, size_t dataIndex)
        : ByteCode(Opcode::GetEnumerateKeyOpcode, loc)
        , m_registerIndex(dstIndex)
        , m_dataRegisterIndex(dataIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get enumerate key r%d r%d", (int)m_registerIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class CheckLastEnumerateKey : public ByteCode {
public:
    explicit CheckLastEnumerateKey(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CheckLastEnumerateKeyOpcode, loc)
        , m_registerIndex(REGISTER_LIMIT)
        , m_exitPosition(SIZE_MAX)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    size_t m_exitPosition;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("check if key is last r%d", (int)m_registerIndex);
    }
#endif
};

class MarkEnumerateKey : public ByteCode {
public:
    explicit MarkEnumerateKey(const ByteCodeLOC& loc, size_t dataIndex, size_t keyIndex)
        : ByteCode(Opcode::MarkEnumerateKeyOpcode, loc)
        , m_dataRegisterIndex(dataIndex)
        , m_keyRegisterIndex(keyIndex)
    {
    }
    ByteCodeRegisterIndex m_dataRegisterIndex;
    ByteCodeRegisterIndex m_keyRegisterIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("mark enumerate key r%d to enumerate object r%d", (int)m_keyRegisterIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class IteratorOperation : public ByteCode {
public:
    enum Operation {
        GetIterator,
        IteratorClose,
        IteratorBind,
        IteratorTestDone,
        IteratorNext,
        IteratorTestResultIsObject,
        IteratorValue,
        // https://www.ecma-international.org/ecma-262/10.0/#sec-asynciteratorclose (8)
        IteratorCheckOngoingExceptionOnAsyncIteratorClose
    };

    struct GetIteratorData {
        bool m_isSyncIterator;
        ByteCodeRegisterIndex m_srcObjectRegisterIndex;
        ByteCodeRegisterIndex m_dstIteratorRecordIndex;
        ByteCodeRegisterIndex m_dstIteratorObjectIndex;
    };

    struct IteratorStepData {
        ByteCodeRegisterIndex m_registerIndex;
        ByteCodeRegisterIndex m_iterRegisterIndex;
        size_t m_forOfEndPosition;
    };

    struct IteratorCloseData {
        ByteCodeRegisterIndex m_iterRegisterIndex;
        ByteCodeRegisterIndex m_execeptionRegisterIndexIfExists;
    };

    struct IteratorBindData {
        ByteCodeRegisterIndex m_registerIndex;
        ByteCodeRegisterIndex m_iterRegisterIndex;
    };

    struct IteratorTestDoneData {
        bool m_isIteratorRecord;
        ByteCodeRegisterIndex m_iteratorRecordOrObjectRegisterIndex;
        ByteCodeRegisterIndex m_dstRegisterIndex;
    };

    struct IteratorNextData {
        ByteCodeRegisterIndex m_iteratorRecordRegisterIndex;
        ByteCodeRegisterIndex m_valueRegisterIndex;
        ByteCodeRegisterIndex m_returnRegisterIndex;
    };

    struct IteratorTestResultIsObjectData {
        ByteCodeRegisterIndex m_valueRegisterIndex;
    };

    struct IteratorValueData {
        ByteCodeRegisterIndex m_srcRegisterIndex;
        ByteCodeRegisterIndex m_dstRegisterIndex;
    };

    struct IteratorCheckOngoingExceptionOnAsyncIteratorCloseData {
    };


    explicit IteratorOperation(const ByteCodeLOC& loc, const GetIteratorData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::GetIterator)
        , m_getIteratorData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorCloseData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorClose)
        , m_iteratorCloseData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorBindData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorBind)
        , m_iteratorBindData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorTestDoneData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorTestDone)
        , m_iteratorTestDoneData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorNextData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorNext)
        , m_iteratorNextData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorTestResultIsObjectData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorTestResultIsObject)
        , m_iteratorTestResultIsObjectData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorValueData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorValue)
        , m_iteratorValueData(data)
    {
    }

    explicit IteratorOperation(const ByteCodeLOC& loc, const IteratorCheckOngoingExceptionOnAsyncIteratorCloseData& data)
        : ByteCode(Opcode::IteratorOperationOpcode, loc)
        , m_operation(Operation::IteratorCheckOngoingExceptionOnAsyncIteratorClose)
        , m_iteratorCheckOngoingExceptionOnAsyncIteratorCloseData(data)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_operation == Operation::GetIterator) {
            printf("get iterator(r%d) -> r%d", (int)m_getIteratorData.m_srcObjectRegisterIndex, (int)m_getIteratorData.m_dstIteratorRecordIndex);
        } else if (m_operation == Operation::IteratorClose) {
            printf("iterator close(r%d, r%d)", (int)m_iteratorCloseData.m_iterRegisterIndex, (int)m_iteratorCloseData.m_execeptionRegisterIndexIfExists);
        } else if (m_operation == Operation::IteratorBind) {
            printf("iterator bind(r%d) -> r%d", (int)m_iteratorBindData.m_iterRegisterIndex, (int)m_iteratorBindData.m_registerIndex);
        } else if (m_operation == Operation::IteratorTestDone) {
            printf("iterator test done iteratorRecord(%d)->m_done -> r%d", (int)m_iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex,
                   (int)m_iteratorTestDoneData.m_dstRegisterIndex);
        } else if (m_operation == Operation::IteratorNext) {
            printf("iterator next r%d.[[next]](r%d) -> r%d", (int)m_iteratorNextData.m_iteratorRecordRegisterIndex,
                   (int)m_iteratorNextData.m_valueRegisterIndex, (int)m_iteratorNextData.m_returnRegisterIndex);
        } else if (m_operation == Operation::IteratorTestResultIsObject) {
            printf("iterator test result is object r%d", (int)m_iteratorTestResultIsObjectData.m_valueRegisterIndex);
        } else if (m_operation == Operation::IteratorValue) {
            printf("iterator value r%d -> r%d", (int)m_iteratorValueData.m_srcRegisterIndex, (int)m_iteratorValueData.m_dstRegisterIndex);
        } else if (m_operation == Operation::IteratorCheckOngoingExceptionOnAsyncIteratorClose) {
            printf("iterator check ongoing exception");
        } else {
            ASSERT_NOT_REACHED();
        }
    }

#endif

    Operation m_operation;
    union {
        GetIteratorData m_getIteratorData;
        IteratorCloseData m_iteratorCloseData;
        IteratorBindData m_iteratorBindData;
        IteratorTestDoneData m_iteratorTestDoneData;
        IteratorNextData m_iteratorNextData;
        IteratorTestResultIsObjectData m_iteratorTestResultIsObjectData;
        IteratorValueData m_iteratorValueData;
        IteratorCheckOngoingExceptionOnAsyncIteratorCloseData m_iteratorCheckOngoingExceptionOnAsyncIteratorCloseData;
    };
};

class GetMethod : public ByteCode {
public:
    explicit GetMethod(const ByteCodeLOC& loc, ByteCodeRegisterIndex objectRegisterIndex, ByteCodeRegisterIndex resultRegisterIndex, AtomicString propertyName)
        : ByteCode(Opcode::GetMethodOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_resultRegisterIndex(resultRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_resultRegisterIndex;
    AtomicString m_propertyName;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get method r%d.%s -> r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toNonGCUTF8StringData().data(), (int)m_resultRegisterIndex);
    }
#endif
};

class LoadRegexp : public ByteCode {
public:
    LoadRegexp(const ByteCodeLOC& loc, const size_t registerIndex, String* body, String* opt)
        : ByteCode(Opcode::LoadRegexpOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_body(body)
        , m_option(opt)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    String* m_body;
    String* m_option;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("load regexp %s -> r%d", m_body->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class WithOperation : public ByteCode {
public:
    WithOperation(const ByteCodeLOC& loc, size_t registerIndex)
        : ByteCode(Opcode::WithOperationOpcode, loc)
        , m_registerIndex(registerIndex)
    {
        m_withEndPostion = SIZE_MAX;
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_withEndPostion;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("with r%d", (int)m_registerIndex);
    }
#endif
};

class ObjectDefineGetterSetter : public ByteCode {
public:
    ObjectDefineGetterSetter(const ByteCodeLOC& loc, size_t objectRegisterIndex, size_t objectPropertyNameRegisterIndex, size_t objectPropertyValueRegisterIndex, ObjectPropertyDescriptor::PresentAttribute presentAttribute, bool isGetter)
        : ByteCode(Opcode::ObjectDefineGetterSetterOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_objectPropertyNameRegisterIndex(objectPropertyNameRegisterIndex)
        , m_objectPropertyValueRegisterIndex(objectPropertyValueRegisterIndex)
        , m_presentAttribute(presentAttribute)
        , m_isGetter(isGetter)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex : REGISTER_INDEX_IN_BIT;
    ByteCodeRegisterIndex m_objectPropertyNameRegisterIndex : REGISTER_INDEX_IN_BIT;
    ByteCodeRegisterIndex m_objectPropertyValueRegisterIndex : REGISTER_INDEX_IN_BIT;
    ObjectPropertyDescriptor::PresentAttribute m_presentAttribute : 8;
    bool m_isGetter : 1; // other case, this is setter

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_isGetter) {
            printf("object define setter r%d[r%d] = r%d", (int)m_objectRegisterIndex, (int)m_objectPropertyNameRegisterIndex, (int)m_objectPropertyValueRegisterIndex);
        } else {
            printf("object define setter r%d[r%d] = r%d", (int)m_objectRegisterIndex, (int)m_objectPropertyNameRegisterIndex, (int)m_objectPropertyValueRegisterIndex);
        }
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(ObjectDefineGetterSetter, sizeof(size_t) * 3);

class BindingRestElement : public ByteCode {
public:
    BindingRestElement(const ByteCodeLOC& loc, size_t iterOrEnumIndex, size_t dstIndex)
        : ByteCode(Opcode::BindingRestElementOpcode, loc)
        , m_iterOrEnumIndex(iterOrEnumIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_iterOrEnumIndex;
    ByteCodeRegisterIndex m_dstIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("binding rest element(r%d) -> r%d", (int)m_iterOrEnumIndex, (int)m_dstIndex);
    }
#endif
};

class BlockOperation : public ByteCode {
public:
    BlockOperation(const ByteCodeLOC& loc, InterpretedCodeBlock::BlockInfo* bi)
        : ByteCode(Opcode::BlockOperationOpcode, loc)
        , m_blockEndPosition(SIZE_MAX)
        , m_blockInfo(bi)
    {
    }

    size_t m_blockEndPosition;
    InterpretedCodeBlock::BlockInfo* m_blockInfo;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("block operation (%p) -> end %d", m_blockInfo, (int)m_blockEndPosition);
    }
#endif
};

class ReplaceBlockLexicalEnvironmentOperation : public ByteCode {
public:
    ReplaceBlockLexicalEnvironmentOperation(const ByteCodeLOC& loc, InterpretedCodeBlock::BlockInfo* bi)
        : ByteCode(Opcode::ReplaceBlockLexicalEnvironmentOperationOpcode, loc)
        , m_blockInfo(bi)
    {
    }

    InterpretedCodeBlock::BlockInfo* m_blockInfo;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("replace block lexical env operation (%p)", m_blockInfo);
    }
#endif
};

class TaggedTemplateOperation : public ByteCode {
public:
    enum Operation {
        TestCacheOperation,
        FillCacheOperation
    };
    struct TestCacheOperationData {
        size_t m_cacheIndex;
        size_t m_jumpPosition;
        ByteCodeRegisterIndex m_registerIndex;
    };

    struct FillCacheOperationData {
        size_t m_cacheIndex;
        ByteCodeRegisterIndex m_registerIndex;
    };

    TaggedTemplateOperation(const ByteCodeLOC& loc, const TestCacheOperationData& data)
        : ByteCode(Opcode::TaggedTemplateOperationOpcode, loc)
        , m_operaton(TestCacheOperation)
        , m_testCacheOperationData(data)
    {
    }

    TaggedTemplateOperation(const ByteCodeLOC& loc, const FillCacheOperationData& data)
        : ByteCode(Opcode::TaggedTemplateOperationOpcode, loc)
        , m_operaton(FillCacheOperation)
        , m_fillCacheOperationData(data)
    {
    }

    Operation m_operaton;
    union {
        TestCacheOperationData m_testCacheOperationData;
        FillCacheOperationData m_fillCacheOperationData;
    };

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_operaton == TestCacheOperation) {
            printf("tagged template operation operation (test cache %d)", (int)m_testCacheOperationData.m_cacheIndex);
        } else {
            printf("tagged template operation operation (fill cache %d)", (int)m_testCacheOperationData.m_cacheIndex);
        }
    }
#endif
};

class EnsureArgumentsObject : public ByteCode {
public:
    EnsureArgumentsObject(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnsureArgumentsObjectOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("ensure arguments object");
    }
#endif
};

class ResolveNameAddress : public ByteCode {
public:
    ResolveNameAddress(const ByteCodeLOC& loc, AtomicString name, ByteCodeRegisterIndex registerIndex)
        : ByteCode(Opcode::ResolveNameAddressOpcode, loc)
        , m_name(name)
        , m_registerIndex(registerIndex)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("r%d <- resolve name address (%s)", (int)m_registerIndex, m_name.string()->toNonGCUTF8StringData().data());
    }
#endif

    AtomicString m_name;
    ByteCodeRegisterIndex m_registerIndex;
};

class StoreByNameWithAddress : public ByteCode {
public:
    StoreByNameWithAddress(const ByteCodeLOC& loc, const size_t addressRegisterIndex, const size_t valueRegisterIndex, const AtomicString& name)
        : ByteCode(Opcode::StoreByNameWithAddressOpcode, loc)
        , m_addressRegisterIndex(addressRegisterIndex)
        , m_valueRegisterIndex(valueRegisterIndex)
        , m_name(name)
    {
    }
    ByteCodeRegisterIndex m_addressRegisterIndex;
    ByteCodeRegisterIndex m_valueRegisterIndex;
    AtomicString m_name;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("store with address r%d, %s <- r%d", (int)m_addressRegisterIndex, m_name.string()->toUTF8StringData().data(), (int)m_valueRegisterIndex);
    }
#endif
};

class End : public ByteCode {
public:
    explicit End(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::EndOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("end(return with r[%d])", (int)m_registerIndex);
    }
#endif
};


typedef Vector<char, std::allocator<char>, ComputeReservedCapacityFunctionWithLog2<200>> ByteCodeBlockData;
typedef std::vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>> ByteCodeLOCData;
typedef Vector<void*, GCUtil::gc_malloc_allocator<void*>> ByteCodeLiteralData;
typedef Vector<Value, std::allocator<Value>> ByteCodeNumeralLiteralData;

class ByteCodeBlock : public gc {
    friend struct OpcodeTable;
    friend class VMInstance;
    friend int getValidValueInByteCodeBlock(void* ptr, GC_mark_custom_result* arr);

    ByteCodeBlock()
    {
    }

public:
    explicit ByteCodeBlock(InterpretedCodeBlock* codeBlock);

    template <typename CodeType>
    void pushCode(const CodeType& code, ByteCodeGenerateContext* context, Node* node)
    {
        size_t idx = node ? node->m_loc.index : SIZE_MAX;
#ifndef NDEBUG
        {
            CodeType& t = const_cast<CodeType&>(code);
            if ((getenv("DUMP_BYTECODE") && strlen(getenv("DUMP_BYTECODE"))) || (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE")))) {
                if (idx != SIZE_MAX) {
                    auto loc = computeNodeLOC(m_codeBlock->src(), m_codeBlock->functionStart(), idx);
                    t.m_loc.line = loc.line;
                    t.m_loc.column = loc.column;
                }
            }
        }
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
        Opcode opcode = (Opcode)(size_t)code.m_opcodeInAddress;
#else
        Opcode opcode = code.m_opcode;
#endif

        char* first = (char*)&code;
        size_t start = m_code.size();
        if (context->m_shouldGenerateLOCData)
            m_locData->push_back(std::make_pair(start, idx));

        m_code.resizeWithUninitializedValues(m_code.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_code[start++] = *first;
            first++;
        }

        m_requiredRegisterFileSizeInValueSize = std::max(m_requiredRegisterFileSizeInValueSize, (ByteCodeRegisterIndex)context->m_baseRegisterCount);

        // TODO throw exception
        RELEASE_ASSERT(m_requiredRegisterFileSizeInValueSize < REGISTER_LIMIT);

        if (std::is_same<CodeType, ExecutionPause>::value) {
            pushPauseStatementExtraData(context);
        }
    }

    struct ByteCodeLexicalBlockContext {
        size_t lexicalBlockSetupStartPosition;
        size_t lexicalBlockStartPosition;
        size_t lexicallyDeclaredNamesCount;
        size_t lexicallyDeclaredNamesCountBefore;

        ByteCodeLexicalBlockContext()
            : lexicalBlockSetupStartPosition(SIZE_MAX)
            , lexicalBlockStartPosition(SIZE_MAX)
            , lexicallyDeclaredNamesCount(SIZE_MAX)
            , lexicallyDeclaredNamesCountBefore(SIZE_MAX)
        {
        }
    };
    ByteCodeLexicalBlockContext pushLexicalBlock(ByteCodeGenerateContext* context, InterpretedCodeBlock::BlockInfo* bi, Node* node, bool initFunctionDeclarationInside = true);
    void finalizeLexicalBlock(ByteCodeGenerateContext* context, const ByteCodeBlock::ByteCodeLexicalBlockContext& ctx);
    void initFunctionDeclarationWithinBlock(ByteCodeGenerateContext* context, InterpretedCodeBlock::BlockInfo* bi, Node* node);

    void updateMaxPauseStatementExtraDataLength(ByteCodeGenerateContext* context);
    void pushPauseStatementExtraData(ByteCodeGenerateContext* context);

    template <typename CodeType>
    CodeType* peekCode(size_t position)
    {
        char* pos = m_code.data();
        pos = &pos[position];
        return (CodeType*)pos;
    }

    template <typename CodeType>
    size_t lastCodePosition()
    {
        return m_code.size() - sizeof(CodeType);
    }

    size_t currentCodeSize()
    {
        return m_code.size();
    }

    size_t memoryAllocatedSize()
    {
        size_t siz = m_code.capacity();
        siz += sizeof(ByteCodeBlock);
        siz += m_locData ? (m_locData->size() * sizeof(std::pair<size_t, size_t>)) : 0;
        siz += m_numeralLiteralData.size() * sizeof(Value);
        siz += m_literalData.size() * sizeof(size_t);
        siz += m_inlineCacheDataSize;
        return siz;
    }

    ExtendedNodeLOC computeNodeLOCFromByteCode(Context* c, size_t codePosition, InterpretedCodeBlock* cb);
    ExtendedNodeLOC computeNodeLOC(StringView src, ExtendedNodeLOC sourceElementStart, size_t index);
    void fillLocDataIfNeeded(Context* c);

    bool m_isEvalMode : 1;
    bool m_isOnGlobal : 1;
    bool m_shouldClearStack : 1;
    bool m_isOwnerMayFreed : 1;
    ByteCodeRegisterIndex m_requiredRegisterFileSizeInValueSize : REGISTER_INDEX_IN_BIT;

    ByteCodeBlockData m_code;
    ByteCodeNumeralLiteralData m_numeralLiteralData;
    ByteCodeLiteralData m_literalData;
    size_t m_inlineCacheDataSize;

    ByteCodeLOCData* m_locData;
    InterpretedCodeBlock* m_codeBlock;

    void* operator new(size_t size);
};
} // namespace Escargot

#endif
