/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "runtime/SmallValue.h"
#include "runtime/String.h"
#include "runtime/Value.h"

namespace Escargot {
class ObjectStructure;
class Node;

// <OpcodeName, PushCount, PopCount>
#define FOR_EACH_BYTECODE_OP(F)                       \
    F(LoadLiteral, 1, 0)                              \
    F(LoadByName, 1, 0)                               \
    F(StoreByName, 0, 0)                              \
    F(LoadByHeapIndex, 1, 0)                          \
    F(StoreByHeapIndex, 0, 0)                         \
    F(DeclareFunctionDeclarations, 1, 0)              \
    F(NewOperation, 1, 0)                             \
    F(NewOperationWithSpreadElement, 1, 0)            \
    F(BinaryPlus, 1, 2)                               \
    F(BinaryMinus, 1, 2)                              \
    F(BinaryMultiply, 1, 2)                           \
    F(BinaryDivision, 1, 2)                           \
    F(BinaryMod, 1, 2)                                \
    F(BinaryEqual, 1, 2)                              \
    F(BinaryLessThan, 1, 2)                           \
    F(BinaryLessThanOrEqual, 1, 2)                    \
    F(BinaryGreaterThan, 1, 2)                        \
    F(BinaryGreaterThanOrEqual, 1, 2)                 \
    F(BinaryNotEqual, 1, 2)                           \
    F(BinaryStrictEqual, 1, 2)                        \
    F(BinaryNotStrictEqual, 1, 2)                     \
    F(BinaryBitwiseAnd, 1, 2)                         \
    F(BinaryBitwiseOr, 1, 2)                          \
    F(BinaryBitwiseXor, 1, 2)                         \
    F(BinaryLeftShift, 1, 2)                          \
    F(BinarySignedRightShift, 1, 2)                   \
    F(BinaryUnsignedRightShift, 1, 2)                 \
    F(BinaryInOperation, 1, 2)                        \
    F(BinaryInstanceOfOperation, 1, 2)                \
    F(CreateObject, 1, 0)                             \
    F(CreateArray, 1, 0)                              \
    F(CreateSpreadObject, 1, 0)                       \
    F(CreateFunction, 1, 0)                           \
    F(CreateClass, 0, 0)                              \
    F(SuperReference, 1, 0)                           \
    F(LoadThisBinding, 0, 0)                          \
    F(ObjectDefineOwnPropertyOperation, 0, 0)         \
    F(ObjectDefineOwnPropertyWithNameOperation, 0, 0) \
    F(ArrayDefineOwnPropertyOperation, 0, 0)          \
    F(GetObject, 1, 2)                                \
    F(SetObjectOperation, 0, 2)                       \
    F(GetObjectPreComputedCase, 1, 1)                 \
    F(SetObjectPreComputedCase, 0, 1)                 \
    F(GetGlobalObject, 1, 1)                          \
    F(SetGlobalObject, 0, 1)                          \
    F(Move, 1, 0)                                     \
    F(Increment, 1, 1)                                \
    F(Decrement, 1, 1)                                \
    F(ToNumberIncrement, 2, 2)                        \
    F(ToNumberDecrement, 2, 2)                        \
    F(ToNumber, 1, 1)                                 \
    F(UnaryMinus, 1, 1)                               \
    F(UnaryNot, 1, 1)                                 \
    F(UnaryBitwiseNot, 1, 1)                          \
    F(UnaryTypeof, 1, 1)                              \
    F(UnaryDelete, 1, 1)                              \
    F(TemplateOperation, 1, 1)                        \
    F(Jump, 0, 0)                                     \
    F(JumpComplexCase, 0, 0)                          \
    F(JumpIfTrue, 0, 0)                               \
    F(JumpIfFalse, 0, 0)                              \
    F(JumpIfRelation, 0, 0)                           \
    F(JumpIfEqual, 0, 0)                              \
    F(CallFunction, -1, 0)                            \
    F(CallFunctionWithReceiver, -1, 0)                \
    F(CallFunctionWithSpreadElement, -1, 0)           \
    F(ReturnFunction, 0, 0)                           \
    F(ReturnFunctionWithValue, 0, 0)                  \
    F(ReturnFunctionSlowCase, 0, 0)                   \
    F(TryOperation, 0, 0)                             \
    F(TryCatchWithBodyEnd, 0, 0)                      \
    F(FinallyEnd, 0, 0)                               \
    F(ThrowOperation, 0, 0)                           \
    F(ThrowStaticErrorOperation, 0, 0)                \
    F(EnumerateObject, 1, 0)                          \
    F(EnumerateObjectKey, 1, 0)                       \
    F(CheckIfKeyIsLast, 0, 0)                         \
    F(GetIterator, 1, 0)                              \
    F(IteratorStep, 1, 0)                             \
    F(IteratorValue, 1, 0)                            \
    F(LoadRegexp, 1, 0)                               \
    F(WithOperation, 0, 0)                            \
    F(ObjectDefineGetter, 0, 0)                       \
    F(ObjectDefineSetter, 0, 0)                       \
    F(CallEvalFunction, 0, 0)                         \
    F(CallFunctionInWithScope, 0, 0)                  \
    F(BindingRestElement, 1, 0)                       \
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

/* Byte code is never instantiated on the heap, it is part of the byte code stream. */
class ByteCode {
public:
    ByteCode(Opcode code, const ByteCodeLOC& loc)
#if defined(COMPILER_GCC)
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
#if defined(COMPILER_GCC)
        m_orgOpcode = (Opcode)(size_t)m_opcodeInAddress;
#else
        m_orgOpcode = m_opcode;
#endif
#endif
#if defined(COMPILER_GCC)
        m_opcodeInAddress = g_opcodeTable.m_table[(Opcode)(size_t)m_opcodeInAddress];
#endif
    }

#if defined(COMPILER_GCC)
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

class DeclareFunctionDeclarations : public ByteCode {
public:
    explicit DeclareFunctionDeclarations(InterpretedCodeBlock* cb)
        : ByteCode(Opcode::DeclareFunctionDeclarationsOpcode, ByteCodeLOC(SIZE_MAX))
        , m_codeBlock(cb)
    {
    }
    InterpretedCodeBlock* m_codeBlock;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("function declarations");
    }
#endif
};

class CreateFunction : public ByteCode {
public:
    CreateFunction(const ByteCodeLOC& loc, const size_t registerIndex, CodeBlock* cb)
        : ByteCode(Opcode::CreateFunctionOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_codeBlock(cb)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    CodeBlock* m_codeBlock;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("create function %s -> r%d", m_codeBlock->functionName().string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class CreateClass : public ByteCode {
public:
    CreateClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t classPrototypeRegisterIndex, const size_t superClassRegisterIndex, AtomicString name, CodeBlock* cb, uint8_t stage)
        : ByteCode(Opcode::CreateClassOpcode, loc)
        , m_classRegisterIndex(classRegisterIndex)
        , m_classPrototypeRegisterIndex(classPrototypeRegisterIndex)
        , m_superClassRegisterIndex(superClassRegisterIndex)
        , m_name(name)
        , m_codeBlock(cb)
        , m_stage(stage)
    {
    }

    ByteCodeRegisterIndex m_classRegisterIndex;
    ByteCodeRegisterIndex m_classPrototypeRegisterIndex;
    ByteCodeRegisterIndex m_superClassRegisterIndex;
    AtomicString m_name;
    CodeBlock* m_codeBlock;
    uint8_t m_stage;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_superClassRegisterIndex == REGISTER_LIMIT) {
            printf("create class(%d) r%d(%s) { r%d }", (int)m_stage, (int)m_classRegisterIndex, m_name.string()->toUTF8StringData().data(), (int)m_classPrototypeRegisterIndex);
        } else {
            printf("create class(%d) r%d : r%d { r%d }", (int)m_stage, (int)m_classRegisterIndex, (int)m_superClassRegisterIndex, (int)m_classPrototypeRegisterIndex);
        }
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

    ByteCodeRegisterIndex m_dstIndex;
    bool m_isCall;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_isCall) {
            printf("Super constructor -> r%d", (int)m_dstIndex);
        } else {
            printf("Super property -> r%d", (int)m_dstIndex);
        }
    }
#endif
};

class LoadThisBinding : public ByteCode {
public:
    LoadThisBinding(const ByteCodeLOC& loc, const size_t dstIndex)
        : ByteCode(Opcode::LoadThisBindingOpcode, loc)
        , m_dstIndex(dstIndex == REGULAR_REGISTER_LIMIT ? SIZE_MAX : dstIndex)
    {
    }

    ByteCodeRegisterIndex m_dstIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_dstIndex == REGISTER_LIMIT) {
            printf("Load this binding");
        } else {
            printf("Load this binding -> r%d", (int)m_dstIndex);
        }
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

DEFINE_BINARY_OPERATION(Plus, "plus");
DEFINE_BINARY_OPERATION(Minus, "minus");
DEFINE_BINARY_OPERATION(Multiply, "multiply");
DEFINE_BINARY_OPERATION(Division, "division");
DEFINE_BINARY_OPERATION(Mod, "mod");
DEFINE_BINARY_OPERATION(Equal, "equal");
DEFINE_BINARY_OPERATION(NotEqual, "notequal");
DEFINE_BINARY_OPERATION(LessThan, "lessthan");
DEFINE_BINARY_OPERATION(LessThanOrEqual, "lessthan or equal");
DEFINE_BINARY_OPERATION(GreaterThan, "greaterthan");
DEFINE_BINARY_OPERATION(GreaterThanOrEqual, "greaterthan or equal");
DEFINE_BINARY_OPERATION(StrictEqual, "strict equal");
DEFINE_BINARY_OPERATION(NotStrictEqual, "not strict equal");
DEFINE_BINARY_OPERATION(BitwiseAnd, "bitwise and");
DEFINE_BINARY_OPERATION(BitwiseOr, "bitwise or");
DEFINE_BINARY_OPERATION(BitwiseXor, "bitwise Xor");
DEFINE_BINARY_OPERATION(LeftShift, "left shift");
DEFINE_BINARY_OPERATION(SignedRightShift, "signed right shift");
DEFINE_BINARY_OPERATION(UnsignedRightShift, "unsigned right shift");
DEFINE_BINARY_OPERATION(InOperation, "in operation");
DEFINE_BINARY_OPERATION(InstanceOfOperation, "instance of");


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
    CreateArray(const ByteCodeLOC& loc, const size_t registerIndex, bool hasSpreadElement)
        : ByteCode(Opcode::CreateArrayOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_hasSpreadElement(hasSpreadElement)
    {
        m_length = 0;
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_length;
    bool m_hasSpreadElement : 1;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("createarray -> r%d", (int)m_registerIndex);
    }
#endif
};

class CreateSpreadObject : public ByteCode {
public:
    CreateSpreadObject(const ByteCodeLOC& loc, const size_t registerIndex, const size_t spreadIndex)
        : ByteCode(Opcode::CreateSpreadObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_spreadIndex(spreadIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_spreadIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("create spread object(r%d) -> r%d", (int)m_spreadIndex, (int)m_registerIndex);
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
    ObjectDefineOwnPropertyOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyRegisterIndex, const size_t loadRegisterIndex)
        : ByteCode(Opcode::ObjectDefineOwnPropertyOperationOpcode, loc)
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
        printf("object define own property r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};

class ObjectDefineOwnPropertyWithNameOperation : public ByteCode {
public:
    ObjectDefineOwnPropertyWithNameOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, AtomicString propertyName, const size_t loadRegisterIndex)
        : ByteCode(Opcode::ObjectDefineOwnPropertyWithNameOperationOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    AtomicString m_propertyName;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("object define own property with name r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

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

    ByteCodeRegisterIndex m_objectRegisterIndex;
    uint8_t m_count;
    uint32_t m_baseIndex;
    ByteCodeRegisterIndex m_loadRegisterIndexs[ARRAY_DEFINE_OPERATION_MERGE_COUNT];

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("array define own property r%d[%d - %d] <- r<--->", (int)m_objectRegisterIndex, (int)m_baseIndex, (int)m_count);
    }
#endif
};

class ObjectStructureChainItem : public gc {
public:
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

typedef std::vector<ObjectStructureChainItem, std::allocator<ObjectStructureChainItem>> ObjectStructureChain;
typedef std::vector<ObjectStructureChainItem, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<ObjectStructureChainItem>> ObjectStructureChainGC;
typedef Vector<ObjectStructureChainItem, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectStructureChainItem>, 200> ObjectStructureChainWithGC;

struct GetObjectInlineCacheData {
    GetObjectInlineCacheData()
    {
        m_cachedIndex = SIZE_MAX;
    }

    GetObjectInlineCacheData(const GetObjectInlineCacheData& src)
    {
        m_cachedhiddenClassChain = src.m_cachedhiddenClassChain;
        m_cachedIndex = src.m_cachedIndex;
    }

    GetObjectInlineCacheData(GetObjectInlineCacheData&& src)
    {
        m_cachedhiddenClassChain = std::move(src.m_cachedhiddenClassChain);
        m_cachedIndex = src.m_cachedIndex;
    }

    GetObjectInlineCacheData& operator=(const GetObjectInlineCacheData& src)
    {
        if (&src == this)
            return *this;

        m_cachedhiddenClassChain = src.m_cachedhiddenClassChain;
        m_cachedIndex = src.m_cachedIndex;
        return *this;
    }

    ObjectStructureChain m_cachedhiddenClassChain;
    size_t m_cachedIndex;
};

typedef std::vector<GetObjectInlineCacheData, std::allocator<GetObjectInlineCacheData>> GetObjectInlineCacheDataVector;

struct GetObjectInlineCache {
    GetObjectInlineCache()
    {
        m_cacheMissCount = m_executeCount = 0;
    }

    GetObjectInlineCacheDataVector m_cache;
    uint16_t m_executeCount;
    uint16_t m_cacheMissCount;
};

class GetObjectPreComputedCase : public ByteCode {
public:
    // [object] -> [value]
    GetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, PropertyName propertyName)
        : ByteCode(Opcode::GetObjectPreComputedCaseOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    GetObjectInlineCache m_inlineCache;
    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;
    PropertyName m_propertyName;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get object r%d <- r%d.%s", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data());
    }
#endif
};

struct SetObjectInlineCache {
    ObjectStructureChainGC m_cachedhiddenClassChain;
    size_t m_cachedIndex;
    ObjectStructure* m_hiddenClassWillBe;
    size_t m_cacheMissCount;
    SetObjectInlineCache()
    {
        m_cachedIndex = SIZE_MAX;
        m_hiddenClassWillBe = nullptr;
        m_cacheMissCount = 0;
    }

    void invalidateCache()
    {
        m_cachedIndex = SIZE_MAX;
        m_hiddenClassWillBe = nullptr;
        m_cachedhiddenClassChain.clear();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class SetObjectPreComputedCase : public ByteCode {
public:
    SetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t objectRegisterIndex, PropertyName propertyName, const size_t loadRegisterIndex, SetObjectInlineCache* inlineCache)
        : ByteCode(Opcode::SetObjectPreComputedCaseOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyName(propertyName)
        , m_inlineCache(inlineCache)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    PropertyName m_propertyName;
    SetObjectInlineCache* m_inlineCache;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("set object r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

class GetGlobalObject : public ByteCode {
public:
    GetGlobalObject(const ByteCodeLOC& loc, const size_t registerIndex, PropertyName propertyName)
        : ByteCode(Opcode::GetGlobalObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_propertyName(propertyName)
    {
        ASSERT(propertyName.hasAtomicString());
        m_cachedAddress = nullptr;
        m_cachedStructure = nullptr;
    }

    ByteCodeRegisterIndex m_registerIndex;
    PropertyName m_propertyName;
    void* m_cachedAddress;
    ObjectStructure* m_cachedStructure;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get global object r%d <- global.%s", (int)m_registerIndex, m_propertyName.plainString()->toUTF8StringData().data());
    }
#endif
};

class SetGlobalObject : public ByteCode {
public:
    SetGlobalObject(const ByteCodeLOC& loc, const size_t registerIndex, PropertyName propertyName)
        : ByteCode(Opcode::SetGlobalObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_propertyName(propertyName)
    {
        ASSERT(propertyName.hasAtomicString());
        m_cachedAddress = nullptr;
        m_cachedStructure = nullptr;
    }

    ByteCodeRegisterIndex m_registerIndex;
    PropertyName m_propertyName;
    void* m_cachedAddress;
    ObjectStructure* m_cachedStructure;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("set global object global.%s <- r%d", m_propertyName.plainString()->toUTF8StringData().data(), (int)m_registerIndex);
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
    UnaryDelete(const ByteCodeLOC& loc, const size_t srcIndex0, const size_t srcIndex1, const size_t dstIndex, AtomicString name)
        : ByteCode(Opcode::UnaryDeleteOpcode, loc)
        , m_srcIndex0(srcIndex0)
        , m_srcIndex1(srcIndex1)
        , m_dstIndex(dstIndex)
        , m_id(name)
    {
    }

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
        , m_quasi(NULL)
        , m_src0Index(src0Index)
        , m_src1Index(src1Index)
        , m_dstIndex(dstIndex)
    {
    }

    String* m_quasi;
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
    JumpComplexCase(Jump* jmp, ControlFlowRecord* controlFlowRecord)
        : ByteCode(Opcode::JumpComplexCaseOpcode, ByteCodeLOC(SIZE_MAX))
        , m_controlFlowRecord(controlFlowRecord)
    {
    }

    ControlFlowRecord* m_controlFlowRecord;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("jump complex");
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
    CallFunction(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount, const size_t resultIndex)
        : ByteCode(Opcode::CallFunctionOpcode, loc)
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
        printf("call r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionWithReceiver : public ByteCode {
public:
    CallFunctionWithReceiver(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount, const size_t resultIndex)
        : ByteCode(Opcode::CallFunctionWithReceiverOpcode, loc)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
    {
    }

    ByteCodeRegisterIndex m_receiverIndex;
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("call r%d <- r%d,r%d(r%d-r%d)", (int)m_resultIndex, (int)m_receiverIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionWithSpreadElement : public ByteCode {
public:
    CallFunctionWithSpreadElement(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount, const size_t resultIndex)
        : ByteCode(Opcode::CallFunctionWithSpreadElementOpcode, loc)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
    {
    }

    ByteCodeRegisterIndex m_receiverIndex;
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        if (m_receiverIndex != REGISTER_LIMIT) {
            printf("call(spread) r%d <- r%d,r%d(r%d-r%d)", (int)m_resultIndex, (int)m_receiverIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
        } else {
            printf("call(spread) r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
        }
    }
#endif
};

class CallEvalFunction : public ByteCode {
public:
    CallEvalFunction(const ByteCodeLOC& loc, const size_t evalIndex, const size_t argumentsStartIndex, size_t argumentCount, const size_t resultIndex, bool inWithScope, bool hasSpreadElement)
        : ByteCode(Opcode::CallEvalFunctionOpcode, loc)
        , m_evalIndex(evalIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
        , m_inWithScope(inWithScope)
        , m_hasSpreadElement(hasSpreadElement)
    {
    }
    ByteCodeRegisterIndex m_evalIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;
    bool m_inWithScope : 1;
    bool m_hasSpreadElement : 1;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("call eval r%d <- r%d, r%d-r%d", (int)m_resultIndex, (int)m_evalIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionInWithScope : public ByteCode {
public:
    CallFunctionInWithScope(const ByteCodeLOC& loc, const AtomicString& calleeName, const size_t argumentsStartIndex, size_t argumentCount, const size_t resultIndex, bool hasSpreadElement)
        : ByteCode(Opcode::CallFunctionInWithScopeOpcode, loc)
        , m_calleeName(calleeName)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
        , m_resultIndex(resultIndex)
        , m_hasSpreadElement(hasSpreadElement)
    {
    }

    AtomicString m_calleeName;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;
    ByteCodeRegisterIndex m_resultIndex;
    bool m_hasSpreadElement : 1;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("call in with r%d <- r%d-r%d", (int)m_resultIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount - 1);
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

class ReturnFunction : public ByteCode {
public:
    explicit ReturnFunction(const ByteCodeLOC& loc)
        : ByteCode(Opcode::ReturnFunctionOpcode, loc)
    {
    }
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("return");
    }
#endif
};

class ReturnFunctionWithValue : public ByteCode {
public:
    ReturnFunctionWithValue(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::ReturnFunctionWithValueOpcode, loc)
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
        m_hasCatch = false;
        m_tryCatchEndPosition = m_catchPosition = SIZE_MAX;
    }

    bool m_hasCatch : 1;
    size_t m_catchPosition;
    size_t m_tryCatchEndPosition;
    AtomicString m_catchVariableName;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("try (hasCatch:%d)", (int)m_hasCatch);
    }
#endif
};

class TryCatchWithBodyEnd : public ByteCode {
public:
    explicit TryCatchWithBodyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryCatchWithBodyEndOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("try-catch-with end");
    }
#endif
};

class FinallyEnd : public ByteCode {
public:
    explicit FinallyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::FinallyEndOpcode, loc)
    {
        m_tryDupCount = 0;
    }

    size_t m_tryDupCount;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("finally end");
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
    ThrowStaticErrorOperation(const ByteCodeLOC& loc, char errorKind, const char* errorMessage)
        : ByteCode(Opcode::ThrowStaticErrorOperationOpcode, loc)
        , m_errorKind(errorKind)
        , m_errorMessage(errorMessage)
    {
    }
    char m_errorKind;
    const char* m_errorMessage;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("throw static error %s", m_errorMessage);
    }
#endif
};

class EnumerateObjectData : public PointerValue {
public:
    EnumerateObjectData()
    {
        m_object = nullptr;
        m_originalLength = 0;
        m_idx = 0;
    }

    ObjectStructureChainWithGC m_hiddenClassChain;
    Object* m_object;
    uint64_t m_originalLength;
    size_t m_idx;
    SmallValueVector m_keys;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};


class EnumerateObject : public ByteCode {
public:
    explicit EnumerateObject(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnumerateObjectOpcode, loc)
    {
        m_objectRegisterIndex = m_dataRegisterIndex = REGISTER_LIMIT;
    }
    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("enumerate object r%d, r%d", (int)m_objectRegisterIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class EnumerateObjectKey : public ByteCode {
public:
    explicit EnumerateObjectKey(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnumerateObjectKeyOpcode, loc)
    {
        m_dataRegisterIndex = m_registerIndex = REGISTER_LIMIT;
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("enumerate object key r%d r%d", (int)m_registerIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class CheckIfKeyIsLast : public ByteCode {
public:
    explicit CheckIfKeyIsLast(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CheckIfKeyIsLastOpcode, loc)
    {
        m_forInEndPosition = SIZE_MAX;
        m_registerIndex = REGISTER_LIMIT;
    }
    ByteCodeRegisterIndex m_registerIndex;
    size_t m_forInEndPosition;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("check if key is last r%d", (int)m_registerIndex);
    }
#endif
};

class GetIterator : public ByteCode {
public:
    explicit GetIterator(const ByteCodeLOC& loc)
        : ByteCode(Opcode::GetIteratorOpcode, loc)
    {
        m_registerIndex = m_objectRegisterIndex = REGISTER_LIMIT;
    }

    GetIterator(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t registerIndex)
        : ByteCode(Opcode::GetIteratorOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_objectRegisterIndex(objectRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_objectRegisterIndex;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("get iterator(r%d) -> r%d", (int)m_objectRegisterIndex, (int)m_registerIndex);
    }
#endif
};

class IteratorStep : public ByteCode {
public:
    explicit IteratorStep(const ByteCodeLOC& loc)
        : ByteCode(Opcode::IteratorStepOpcode, loc)
    {
        m_forOfEndPosition = SIZE_MAX;
        m_registerIndex = m_iterRegisterIndex = REGISTER_LIMIT;
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_iterRegisterIndex;
    size_t m_forOfEndPosition;

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("iterator step(r%d) -> r%d", (int)m_iterRegisterIndex, (int)m_registerIndex);
    }
#endif
};

class IteratorValue : public ByteCode {
public:
    IteratorValue(const ByteCodeLOC& loc, size_t iterIndex, size_t dstIndex)
        : ByteCode(Opcode::IteratorValueOpcode, loc)
        , m_iterIndex(iterIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_iterIndex;
    ByteCodeRegisterIndex m_dstIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("iterator value (r%d) -> r%d", (int)m_iterIndex, (int)m_dstIndex);
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

class ObjectDefineGetter : public ByteCode {
public:
    ObjectDefineGetter(const ByteCodeLOC& loc, size_t objectRegisterIndex, size_t objectPropertyNameRegisterIndex, size_t objectPropertyValueRegisterIndex)
        : ByteCode(Opcode::ObjectDefineGetterOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_objectPropertyNameRegisterIndex(objectPropertyNameRegisterIndex)
        , m_objectPropertyValueRegisterIndex(objectPropertyValueRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_objectPropertyNameRegisterIndex;
    ByteCodeRegisterIndex m_objectPropertyValueRegisterIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("object define getter r%d[r%d] = r%d", (int)m_objectRegisterIndex, (int)m_objectPropertyNameRegisterIndex, (int)m_objectPropertyValueRegisterIndex);
    }
#endif
};

class ObjectDefineSetter : public ByteCode {
public:
    ObjectDefineSetter(const ByteCodeLOC& loc, size_t objectRegisterIndex, size_t objectPropertyNameRegisterIndex, size_t objectPropertyValueRegisterIndex)
        : ByteCode(Opcode::ObjectDefineSetterOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_objectPropertyNameRegisterIndex(objectPropertyNameRegisterIndex)
        , m_objectPropertyValueRegisterIndex(objectPropertyValueRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_objectPropertyNameRegisterIndex;
    ByteCodeRegisterIndex m_objectPropertyValueRegisterIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("object define setter r%d[r%d] = r%d", (int)m_objectRegisterIndex, (int)m_objectPropertyNameRegisterIndex, (int)m_objectPropertyValueRegisterIndex);
    }
#endif
};

class BindingRestElement : public ByteCode {
public:
    BindingRestElement(const ByteCodeLOC& loc, size_t iterIndex, size_t dstIndex)
        : ByteCode(Opcode::BindingRestElementOpcode, loc)
        , m_dstIndex(dstIndex)
        , m_iterIndex(iterIndex)
    {
    }

    ByteCodeRegisterIndex m_dstIndex;
    ByteCodeRegisterIndex m_iterIndex;
#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("binding rest element(r%d) -> r%d", (int)m_iterIndex, (int)m_dstIndex);
    }
#endif
};

class End : public ByteCode {
public:
    explicit End(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EndOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump(const char* byteCodeStart)
    {
        printf("end");
    }
#endif
};


typedef Vector<char, std::allocator<char>, 200> ByteCodeBlockData;
typedef std::vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>> ByteCodeLOCData;
typedef Vector<void*, GCUtil::gc_malloc_ignore_off_page_allocator<void*>> ByteCodeLiteralData;
typedef Vector<Value, std::allocator<Value>> ByteCodeNumeralLiteralData;
typedef std::unordered_set<ObjectStructure*, std::hash<ObjectStructure*>, std::equal_to<ObjectStructure*>,
                           GCUtil::gc_malloc_ignore_off_page_allocator<ObjectStructure*>>
    ObjectStructuresInUse;
class ByteCodeBlock : public gc {
    friend struct OpcodeTable;
    ByteCodeBlock()
    {
    }

public:
    explicit ByteCodeBlock(InterpretedCodeBlock* codeBlock)
        : m_isEvalMode(false)
        , m_isOnGlobal(false)
        , m_shouldClearStack(false)
        , m_requiredRegisterFileSizeInValueSize(2)
        , m_objectStructuresInUse((codeBlock->hasCallNativeFunctionCode()) ? nullptr : new (GC) ObjectStructuresInUse())
        , m_locData(nullptr)
        , m_codeBlock(codeBlock)
    {
        GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
            ByteCodeBlock* self = (ByteCodeBlock*)obj;
            for (size_t i = 0; i < self->m_getObjectCodePositions.size(); i++) {
                GetObjectInlineCacheDataVector().swap(((GetObjectPreComputedCase*)((size_t)self->m_code.data() + self->m_getObjectCodePositions[i]))->m_inlineCache.m_cache);
            }
            std::vector<size_t>().swap(self->m_getObjectCodePositions);

            self->m_numeralLiteralData.clear();
            self->m_code.clear();
            if (self->m_locData)
                delete self->m_locData;
        },
                                       nullptr, nullptr, nullptr);
    }

    template <typename CodeType>
    void pushCode(const CodeType& code, ByteCodeGenerateContext* context, Node* node)
    {
        size_t idx = node ? node->m_loc.index : SIZE_MAX;
#ifndef NDEBUG
        {
            CodeType& t = const_cast<CodeType&>(code);
            if ((getenv("DUMP_BYTECODE") && strlen(getenv("DUMP_BYTECODE"))) || (getenv("DUMP_CODEBLOCK_TREE") && strlen(getenv("DUMP_CODEBLOCK_TREE")))) {
                if (idx != SIZE_MAX && !m_codeBlock->hasCallNativeFunctionCode()) {
                    auto loc = computeNodeLOC(m_codeBlock->asInterpretedCodeBlock()->src(), m_codeBlock->asInterpretedCodeBlock()->sourceElementStart(), idx);
                    t.m_loc.line = loc.line;
                    t.m_loc.column = loc.column;
                }
            }
        }
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
    }
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
        size_t siz = m_code.size();
        siz += m_locData ? (m_locData->size() * sizeof(std::pair<size_t, size_t>)) : 0;
        siz += m_literalData.size() * sizeof(size_t);
        siz += m_objectStructuresInUse->size() * sizeof(size_t);
        siz += m_getObjectCodePositions.size() * sizeof(size_t);
        return siz;
    }

    ExtendedNodeLOC computeNodeLOCFromByteCode(Context* c, size_t codePosition, CodeBlock* cb);
    ExtendedNodeLOC computeNodeLOC(StringView src, ExtendedNodeLOC sourceElementStart, size_t index);
    void fillLocDataIfNeeded(Context* c);

    bool m_isEvalMode : 1;
    bool m_isOnGlobal : 1;
    bool m_shouldClearStack : 1;
    ByteCodeRegisterIndex m_requiredRegisterFileSizeInValueSize : REGISTER_INDEX_IN_BIT;

    ByteCodeBlockData m_code;
    ByteCodeNumeralLiteralData m_numeralLiteralData;
    ByteCodeLiteralData m_literalData;
    ObjectStructuresInUse* m_objectStructuresInUse;

    ByteCodeLOCData* m_locData;
    InterpretedCodeBlock* m_codeBlock;

    std::vector<size_t> m_getObjectCodePositions;

    void* operator new(size_t size);
};
} // namespace Escargot

#endif
