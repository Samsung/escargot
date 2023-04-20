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
#include "runtime/ExecutionPauser.h"

#ifndef NDEBUG
#include "parser/CodeBlock.h"
#endif

namespace Escargot {
class Node;
class ObjectStructure;
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
    F(BinaryStrictEqual, 1, 2)                              \
    F(BinaryBitwiseAnd, 1, 2)                               \
    F(BinaryBitwiseOr, 1, 2)                                \
    F(BinaryBitwiseXor, 1, 2)                               \
    F(BinaryLeftShift, 1, 2)                                \
    F(BinarySignedRightShift, 1, 2)                         \
    F(BinaryUnsignedRightShift, 1, 2)                       \
    F(BinaryInOperation, 1, 2)                              \
    F(BinaryInstanceOfOperation, 1, 2)                      \
    F(CreateObject, 1, 0)                                   \
    F(CreateArray, 1, 0)                                    \
    F(CreateSpreadArrayObject, 1, 0)                        \
    F(CreateFunction, 1, 0)                                 \
    F(InitializeClass, 0, 0)                                \
    F(CreateRestElement, 0, 0)                              \
    F(SuperReference, 1, 0)                                 \
    F(ComplexSetObjectOperation, 0, 2)                      \
    F(ComplexGetObjectOperation, 1, 2)                      \
    F(LoadThisBinding, 0, 0)                                \
    F(ObjectDefineOwnPropertyOperation, 0, 0)               \
    F(ObjectDefineOwnPropertyWithNameOperation, 0, 0)       \
    F(ArrayDefineOwnPropertyOperation, 0, 0)                \
    F(ArrayDefineOwnPropertyBySpreadElementOperation, 0, 0) \
    F(GetObject, 1, 2)                                      \
    F(SetObjectOperation, 0, 2)                             \
    F(GetObjectPreComputedCase, 1, 1)                       \
    F(GetObjectPreComputedCaseSimpleInlineCache, 1, 1)      \
    F(SetObjectPreComputedCase, 0, 1)                       \
    F(GetGlobalVariable, 1, 1)                              \
    F(SetGlobalVariable, 0, 1)                              \
    F(InitializeGlobalVariable, 0, 1)                       \
    F(Move, 1, 0)                                           \
    F(Increment, 1, 1)                                      \
    F(Decrement, 1, 1)                                      \
    F(ToNumericIncrement, 2, 2)                             \
    F(ToNumericDecrement, 2, 2)                             \
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
    F(JumpIfNotFulfilled, 0, 0)                             \
    F(JumpIfEqual, 0, 0)                                    \
    F(Call, -1, 0)                                          \
    F(CallWithReceiver, -1, 0)                              \
    F(CallReturn, -1, 0)                                    \
    F(TailRecursion, -1, 0)                                 \
    F(CallReturnWithReceiver, -1, 0)                        \
    F(TailRecursionWithReceiver, -1, 0)                     \
    F(GetParameter, 0, 0)                                   \
    F(ReturnFunctionSlowCase, 0, 0)                         \
    F(TryOperation, 0, 0)                                   \
    F(CloseLexicalEnvironment, 0, 0)                        \
    F(ThrowOperation, 0, 0)                                 \
    F(ThrowStaticErrorOperation, 0, 0)                      \
    F(CreateEnumerateObject, 1, 0)                          \
    F(GetEnumerateKey, 1, 0)                                \
    F(CheckLastEnumerateKey, 0, 0)                          \
    F(MarkEnumerateKey, 2, 0)                               \
    F(IteratorOperation, 0, 0)                              \
    F(GetMethod, 0, 0)                                      \
    F(LoadRegExp, 1, 0)                                     \
    F(OpenLexicalEnvironment, 0, 0)                         \
    F(ObjectDefineGetterSetter, 0, 0)                       \
    F(CallComplexCase, 0, 0)                                \
    F(BindingRestElement, 1, 0)                             \
    F(ExecutionResume, 0, 0)                                \
    F(ExecutionPause, 0, 0)                                 \
    F(MetaPropertyOperation, 1, 0)                          \
    F(BlockOperation, 0, 0)                                 \
    F(ReplaceBlockLexicalEnvironmentOperation, 0, 0)        \
    F(TaggedTemplateOperation, 0, 0)                        \
    F(EnsureArgumentsObject, 0, 0)                          \
    F(BindingCalleeIntoRegister, 0, 0)                      \
    F(ResolveNameAddress, 1, 0)                             \
    F(StoreByNameWithAddress, 0, 1)                         \
    F(FillOpcodeTable, 0, 0)                                \
    F(End, 0, 0)

#ifdef ESCARGOT_DEBUGGER
#define FOR_EACH_BYTECODE_DEBUGGER_OP(F) \
    F(BreakpointDisabled, 0, 0)          \
    F(BreakpointEnabled, 0, 0)
#else
#define FOR_EACH_BYTECODE_DEBUGGER_OP(F)
#endif /* ESCARGOT_DEBUGGER */

#define FOR_EACH_BYTECODE(F) \
    FOR_EACH_BYTECODE_OP(F)  \
    FOR_EACH_BYTECODE_DEBUGGER_OP(F)

enum Opcode {
#define DECLARE_BYTECODE(name, pushCount, popCount) name##Opcode,
    FOR_EACH_BYTECODE(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
        OpcodeKindEnd,
    // special opcode only used in interpreter
    GetObjectOpcodeSlowCaseOpcode,
    SetObjectOpcodeSlowCaseOpcode,
};

struct OpcodeTable {
    OpcodeTable();

    void* m_addressTable[OpcodeKindEnd];
#if defined(ENABLE_CODE_CACHE)
    std::unordered_map<void*, size_t, std::hash<void*>, std::equal_to<void*>, std::allocator<std::pair<void* const, size_t>>> m_opcodeMap;
#endif
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

#if defined(NDEBUG) && defined(ESCARGOT_32) && !defined(COMPILER_MSVC)
#define BYTECODE_SIZE_CHECK_IN_32BIT(codeName, size) COMPILE_ASSERT(sizeof(codeName) == size, "");
#else
#define BYTECODE_SIZE_CHECK_IN_32BIT(CodeName, Size)
#endif

/* Byte code is never instantiated on the heap, it is part of the byte code stream. */
class ByteCode {
public:
    ByteCode(Opcode code, const ByteCodeLOC& loc)
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
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
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
        m_opcodeInAddress = g_opcodeTable.m_addressTable[(Opcode)(size_t)m_opcodeInAddress];
#endif
    }

#if defined(ENABLE_CODE_CACHE)
    void assignAddressInOpcode()
    {
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
        ASSERT(g_opcodeTable.m_opcodeMap.find(m_opcodeInAddress) != g_opcodeTable.m_opcodeMap.end());
        m_opcodeInAddress = (void*)g_opcodeTable.m_opcodeMap.find(m_opcodeInAddress)->second;
#endif
    }
#endif

    void changeOpcode(Opcode code)
    {
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
        m_opcodeInAddress = g_opcodeTable.m_addressTable[code];
#else
        m_opcode = code;
#endif
    }

#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
    void* m_opcodeInAddress;
#else
    Opcode m_opcode;
#endif

#ifndef NDEBUG
    ByteCodeLOC m_loc;
    Opcode m_orgOpcode;

    static void dumpCode(const char* byteCodeStart, const size_t endPos);
    static size_t dumpJumpPosition(size_t pos);
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
    void dump()
    {
        printf("load r%u <- ", m_registerIndex);
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

class MetaPropertyOperation : public ByteCode {
public:
    enum Type ENSURE_ENUM_UNSIGNED {
        NewTarget,
        ImportMeta
    };

    MetaPropertyOperation(const ByteCodeLOC& loc, Type type, const size_t registerIndex)
        : ByteCode(Opcode::MetaPropertyOperationOpcode, loc)
        , m_type(type)
        , m_registerIndex(registerIndex)
    {
    }

    Type m_type : 1;
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("r%u <- new.target", m_registerIndex);
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(MetaPropertyOperation, sizeof(size_t) * 2);

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
    void dump()
    {
        printf("load r%u <- %s", m_registerIndex, m_name.string()->toUTF8StringData().data());
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
    void dump()
    {
        printf("store %s <- r%u", m_name.string()->toUTF8StringData().data(), m_registerIndex);
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
    void dump()
    {
        printf("initialize %s <- r%u", m_name.string()->toUTF8StringData().data(), m_registerIndex);
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
    void dump()
    {
        printf("load r%u <- heap[%u][%u]", m_registerIndex, m_upperIndex, m_index);
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
    void dump()
    {
        printf("store heap[%u][%u] <- r%u", m_upperIndex, m_index, m_registerIndex);
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
    void dump()
    {
        printf("initialize heap[0][%u] <- r%u", m_index, m_registerIndex);
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
    void dump()
    {
        printf("create ");
        if (m_codeBlock->isArrowFunctionExpression()) {
            printf("arrow");
        }
        printf("function %s -> r%u", m_codeBlock->functionName().string()->toUTF8StringData().data(), m_registerIndex);
    }
#endif
};

class InitializeClass : public ByteCode {
public:
    enum Stage ENSURE_ENUM_UNSIGNED {
        CreateClass,
        SetFieldSize,
        InitField,
        InitPrivateField,
        SetFieldData,
        SetPrivateFieldData,
        InitStaticField,
        InitStaticPrivateField,
        SetStaticFieldData,
        SetStaticPrivateFieldData,
        RunStaticInitializer,
        CleanupStaticData,
    };

    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t classPrototypeRegisterIndex,
                    const size_t superClassRegisterIndex, InterpretedCodeBlock* cb, String* src, const Optional<AtomicString>& name)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::CreateClass)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_classPrototypeRegisterIndex(classPrototypeRegisterIndex)
        , m_superClassRegisterIndex(superClassRegisterIndex)
        , m_codeBlock(cb)
        , m_classSrc(src)
        , m_name(name ? name.value().string() : nullptr)
    {
    }

    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldSize, const size_t staticFieldSize)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::SetFieldSize)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_fieldSize(fieldSize)
        , m_staticFieldSize(staticFieldSize)
    {
    }

    enum InitFieldDataTag {
        InitFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, InitFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::InitField)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_initFieldIndex(fieldIndex)
        , m_propertyInitRegisterIndex(propertyIndex)
    {
    }

    enum InitPrivateFieldTag {
        InitPrivateFieldTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, const size_t type, InitPrivateFieldTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::InitPrivateField)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_initPrivateFieldIndex(fieldIndex)
        , m_privatePropertyInitRegisterIndex(propertyIndex)
        , m_initPrivateFieldType(type)
    {
    }

    enum SetFieldDataTag {
        SetFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, SetFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::SetFieldData)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_setFieldIndex(fieldIndex)
        , m_propertySetRegisterIndex(propertyIndex)
    {
    }

    enum SetPrivateFieldDataTag {
        SetPrivateFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, const size_t type, SetPrivateFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::SetPrivateFieldData)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_setPrivateFieldIndex(fieldIndex)
        , m_privatePropertySetRegisterIndex(propertyIndex)
        , m_setPrivateFieldType(type)
    {
    }

    enum InitStaticFieldDataTag {
        InitStaticFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, InitStaticFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::InitStaticField)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_staticFieldInitIndex(fieldIndex)
        , m_staticPropertyInitRegisterIndex(propertyIndex)
    {
    }

    enum InitStaticPrivateFieldDataTag {
        InitStaticPrivateFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t propertyIndex, InitStaticPrivateFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::InitStaticPrivateField)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_staticPrivateFieldInitIndex(fieldIndex)
        , m_staticPrivatePropertyInitRegisterIndex(propertyIndex)
    {
    }

    enum SetStaticFieldDataTag {
        SetStaticFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t valueIndex, SetStaticFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::SetStaticFieldData)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_staticFieldSetIndex(fieldIndex)
        , m_staticPropertySetRegisterIndex(valueIndex)
    {
    }

    enum SetStaticPrivateFieldDataTag {
        SetStaticPrivateFieldDataTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t fieldIndex, const size_t valueIndex, const size_t type, SetStaticPrivateFieldDataTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::SetStaticPrivateFieldData)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_staticPrivateFieldSetIndex(fieldIndex)
        , m_staticPrivatePropertySetRegisterIndex(valueIndex)
        , m_setStaticPrivateFieldType(type)
    {
    }

    enum RunStaticInitializerTag {
        RunStaticInitializerTagValue
    };
    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex, const size_t valueIndex, RunStaticInitializerTag)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::RunStaticInitializer)
        , m_classConstructorRegisterIndex(classRegisterIndex)
        , m_staticPropertySetRegisterIndex(valueIndex)
    {
    }

    InitializeClass(const ByteCodeLOC& loc, const size_t classRegisterIndex)
        : ByteCode(Opcode::InitializeClassOpcode, loc)
        , m_stage(Stage::CleanupStaticData)
        , m_classConstructorRegisterIndex(classRegisterIndex)
    {
    }

    Stage m_stage : 4;
    ByteCodeRegisterIndex m_classConstructorRegisterIndex;
    union {
        struct {
            ByteCodeRegisterIndex m_classPrototypeRegisterIndex;
            ByteCodeRegisterIndex m_superClassRegisterIndex;
            InterpretedCodeBlock* m_codeBlock;
            String* m_classSrc;
            String* m_name;
        }; // CreateClass
        struct {
            uint16_t m_fieldSize;
            uint16_t m_staticFieldSize;
        }; // SetFieldSize
        struct {
            uint16_t m_initFieldIndex;
            ByteCodeRegisterIndex m_propertyInitRegisterIndex;
        }; // InitField
        struct {
            uint16_t m_initPrivateFieldIndex;
            ByteCodeRegisterIndex m_privatePropertyInitRegisterIndex;
            uint16_t m_initPrivateFieldType;
        }; // InitPrivateField
        struct {
            uint16_t m_setFieldIndex;
            ByteCodeRegisterIndex m_propertySetRegisterIndex;
        }; // SetFieldData
        struct {
            uint16_t m_setPrivateFieldIndex;
            ByteCodeRegisterIndex m_privatePropertySetRegisterIndex;
            uint16_t m_setPrivateFieldType;
        }; // SetPrivateFieldData
        struct {
            uint16_t m_staticFieldInitIndex;
            ByteCodeRegisterIndex m_staticPropertyInitRegisterIndex;
        }; // InitStaticField
        struct {
            uint16_t m_staticPrivateFieldInitIndex;
            ByteCodeRegisterIndex m_staticPrivatePropertyInitRegisterIndex;
        }; // InitPrivateStaticField
        struct {
            uint16_t m_staticFieldSetIndex;
            ByteCodeRegisterIndex m_staticPropertySetRegisterIndex;
        }; // SetStaticFieldData
        struct {
            uint16_t m_staticPrivateFieldSetIndex;
            ByteCodeRegisterIndex m_staticPrivatePropertySetRegisterIndex;
            uint16_t m_setStaticPrivateFieldType;
        }; // SetStaticPrivateFieldData
    };
#ifndef NDEBUG
    void dump()
    {
        if (m_stage == Stage::CreateClass) {
            if (m_superClassRegisterIndex == REGISTER_LIMIT) {
                printf("create class r%u { r%u }", m_classConstructorRegisterIndex, m_classPrototypeRegisterIndex);
            } else {
                printf("create class r%u : r%u { r%u }", m_classConstructorRegisterIndex, m_superClassRegisterIndex, m_classPrototypeRegisterIndex);
            }
        } else if (m_stage == Stage::SetFieldSize) {
            printf("set field size r%u -> %u,%u", m_classConstructorRegisterIndex, m_fieldSize, m_staticFieldSize);
        } else if (m_stage == Stage::InitField) {
            printf("init field r%u[%u, r%u]", m_classConstructorRegisterIndex, m_initFieldIndex, m_propertyInitRegisterIndex);
        } else if (m_stage == Stage::InitPrivateField) {
            printf("init private field r%u[%u, r%u] type %u", m_classConstructorRegisterIndex, m_initPrivateFieldIndex, m_privatePropertyInitRegisterIndex, m_initPrivateFieldType);
        } else if (m_stage == Stage::SetFieldData) {
            printf("set field data r%u[%u] = r%u", m_classConstructorRegisterIndex, m_setFieldIndex, m_propertySetRegisterIndex);
        } else if (m_stage == Stage::SetPrivateFieldData) {
            printf("set private field data r%u[%u] = r%u type %u", m_classConstructorRegisterIndex, m_setPrivateFieldIndex, m_privatePropertySetRegisterIndex, m_setPrivateFieldType);
        } else if (m_stage == Stage::InitStaticField) {
            printf("init static field r%u.r%u", m_classConstructorRegisterIndex, m_staticPropertyInitRegisterIndex);
        } else if (m_stage == Stage::InitStaticPrivateField) {
            printf("init private static field r%u.r%u", m_classConstructorRegisterIndex, m_staticPrivatePropertyInitRegisterIndex);
        } else if (m_stage == Stage::SetStaticFieldData) {
            printf("set static field r%u.? = r%u", m_classConstructorRegisterIndex, m_staticPropertySetRegisterIndex);
        } else if (m_stage == Stage::SetStaticPrivateFieldData) {
            printf("set static private field r%u.? = r%u(%u)", m_classConstructorRegisterIndex, m_staticPrivatePropertySetRegisterIndex, m_setStaticPrivateFieldType);
        } else if (m_stage == Stage::RunStaticInitializer) {
            printf("run static initializer r%u(r%u)", m_classConstructorRegisterIndex, m_staticPropertySetRegisterIndex);
        } else {
            ASSERT(m_stage == Stage::CleanupStaticData);
            printf("cleanup static field data r%u", m_classConstructorRegisterIndex);
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
    void dump()
    {
        printf("Create RestElement -> r%u", m_registerIndex);
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
    void dump()
    {
        if (m_isCall) {
            printf("super constructor -> r%u", m_dstIndex);
        } else {
            printf("super property -> r%u", m_dstIndex);
        }
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(SuperReference, sizeof(size_t) * 2);

class ComplexSetObjectOperation : public ByteCode {
public:
    enum Type ENSURE_ENUM_UNSIGNED {
        Super,
        Private,
        PrivateWithoutOuterClass
    };

    ComplexSetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t propertyNameIndex, const size_t loadRegisterIndex)
        : ByteCode(Opcode::ComplexSetObjectOperationOpcode, loc)
        , m_type(Super)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyNameIndex(propertyNameIndex)
    {
    }

    ComplexSetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, AtomicString s, const size_t loadRegisterIndex, bool shouldReferOuterClass = true)
        : ByteCode(Opcode::ComplexSetObjectOperationOpcode, loc)
        , m_type(shouldReferOuterClass ? Private : PrivateWithoutOuterClass)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
        , m_propertyName(s)
    {
    }

    Type m_type : 2;
    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    union {
        ByteCodeRegisterIndex m_propertyNameIndex;
        AtomicString m_propertyName;
    };

#ifndef NDEBUG
    void dump()
    {
        if (m_type == Super) {
            printf("set object complex(r%u).r%u <- r%u", m_objectRegisterIndex, m_propertyNameIndex, m_loadRegisterIndex);
        } else {
            ASSERT(m_type == Private || m_type == PrivateWithoutOuterClass);
            printf("set object complex(r%u).#%s <- r%u %s", m_objectRegisterIndex,
                   m_propertyName.string()->toNonGCUTF8StringData().data(), m_loadRegisterIndex,
                   m_type == Private ? "refer outer class" : "");
        }
    }
#endif
};

class ComplexGetObjectOperation : public ByteCode {
public:
    enum Type ENSURE_ENUM_UNSIGNED {
        Super,
        Private,
        PrivateWithoutOuterClass
    };

    ComplexGetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, const size_t propertyNameIndex)
        : ByteCode(Opcode::ComplexGetObjectOperationOpcode, loc)
        , m_type(Super)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
        , m_propertyNameIndex(propertyNameIndex)
    {
    }

    ComplexGetObjectOperation(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, AtomicString propertyName, bool shouldReferOuterClass = true)
        : ByteCode(Opcode::ComplexGetObjectOperationOpcode, loc)
        , m_type(shouldReferOuterClass ? Private : PrivateWithoutOuterClass)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    Type m_type : 2;
    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;
    union {
        ByteCodeRegisterIndex m_propertyNameIndex;
        AtomicString m_propertyName;
    };

#ifndef NDEBUG
    void dump()
    {
        if (m_type == Super) {
            printf("get object r%u <- complex(r%u).r%u", m_storeRegisterIndex, m_objectRegisterIndex, m_propertyNameIndex);
        } else {
            ASSERT(m_type == Private || m_type == PrivateWithoutOuterClass);
            printf("get object r%u <- complex(r%u).#%s %s", m_storeRegisterIndex, m_objectRegisterIndex,
                   m_propertyName.string()->toNonGCUTF8StringData().data(), m_type == Private ? "refer outer class" : "");
        }
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
    void dump()
    {
        printf("load this binding -> r%u", m_dstIndex);
    }
#endif
};

#ifdef NDEBUG
#define DEFINE_BINARY_OPERATION_DUMP(name)
#else
#define DEFINE_BINARY_OPERATION_DUMP(name)                                      \
    void dump()                                                                 \
    {                                                                           \
        printf(name " r%u <- r%u , r%u", m_dstIndex, m_srcIndex0, m_srcIndex1); \
    }
#endif

#define DEFINE_BINARY_OPERATION(CodeName, HumanName)                                                                                      \
    class Binary##CodeName : public ByteCode {                                                                                            \
    public:                                                                                                                               \
        Binary##CodeName(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1, const size_t dstRegisterIndex, \
                         const size_t extraData = 0)                                                                                      \
            : ByteCode(Opcode::Binary##CodeName##Opcode, loc)                                                                             \
            , m_srcIndex0(registerIndex0)                                                                                                 \
            , m_srcIndex1(registerIndex1)                                                                                                 \
            , m_dstIndex(dstRegisterIndex)                                                                                                \
            , m_extraData(extraData)                                                                                                      \
        {                                                                                                                                 \
        }                                                                                                                                 \
        ByteCodeRegisterIndex m_srcIndex0;                                                                                                \
        ByteCodeRegisterIndex m_srcIndex1;                                                                                                \
        ByteCodeRegisterIndex m_dstIndex;                                                                                                 \
        ByteCodeRegisterIndex m_extraData;                                                                                                \
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
DEFINE_BINARY_OPERATION(Plus, "plus");
DEFINE_BINARY_OPERATION(SignedRightShift, "signed right shift");
DEFINE_BINARY_OPERATION(StrictEqual, "strict equal");
DEFINE_BINARY_OPERATION(UnsignedRightShift, "unsigned right shift");

#ifdef ESCARGOT_DEBUGGER
class BreakpointDisabled : public ByteCode {
public:
    BreakpointDisabled(const ByteCodeLOC& loc)
        : ByteCode(Opcode::BreakpointDisabledOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump()
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
    void dump()
    {
        printf("breakpointEnabled");
    }
#endif
};

COMPILE_ASSERT(sizeof(BreakpointDisabled) == sizeof(BreakpointEnabled), "");
#endif /* ESCARGOT_DEBUGGER */

class CreateObject : public ByteCode {
public:
    CreateObject(const ByteCodeLOC& loc, const size_t registerIndex)
        : ByteCode(Opcode::CreateObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("createobject -> r%u", m_registerIndex);
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
    void dump()
    {
        printf("createarray -> r%u", m_registerIndex);
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
    void dump()
    {
        printf("create spread array object(r%u) -> r%u", m_argumentIndex, m_registerIndex);
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
    void dump()
    {
        printf("get object r%u <- r%u[r%u]", m_storeRegisterIndex, m_objectRegisterIndex, m_propertyRegisterIndex);
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
    void dump()
    {
        printf("set object r%u[r%u] <- r%u", m_objectRegisterIndex, m_propertyRegisterIndex, m_loadRegisterIndex);
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

    void dump()
    {
        printf("object define own property r%u[r%u] <- r%u", m_objectRegisterIndex, m_propertyRegisterIndex, m_loadRegisterIndex);
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
        , m_missCount(0)
        , m_inlineCachedStructureBefore(nullptr)
        , m_inlineCachedStructureAfter(nullptr)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex : REGISTER_INDEX_IN_BIT;
    ByteCodeRegisterIndex m_loadRegisterIndex : REGISTER_INDEX_IN_BIT;
    AtomicString m_propertyName;
    ObjectPropertyDescriptor::PresentAttribute m_presentAttribute : 8;
    size_t m_missCount : 4;
    ObjectStructure* m_inlineCachedStructureBefore;
    ObjectStructure* m_inlineCachedStructureAfter;

#ifndef NDEBUG
    void dump()
    {
        printf("object define own property with name r%u.%s <- r%u", m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), m_loadRegisterIndex);
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(ObjectDefineOwnPropertyWithNameOperation, sizeof(size_t) * 6);

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
    void dump()
    {
        printf("array define own property r%u[%u - %u] <- ", m_objectRegisterIndex, m_baseIndex, (m_baseIndex + m_count));
        for (int i = 0; i < m_count; i++) {
            if (m_loadRegisterIndexs[i] == REGISTER_LIMIT) {
                printf(", ");
            } else {
                printf("r%u, ", m_loadRegisterIndexs[i]);
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
    void dump()
    {
        printf("array define own property by spread element r%u[ - ] <- ", m_objectRegisterIndex);
        for (int i = 0; i < m_count; i++) {
            if (m_loadRegisterIndexs[i] == REGISTER_LIMIT) {
                printf(", ");
            } else {
                printf("r%u, ", m_loadRegisterIndexs[i]);
            }
        }
    }
#endif
};

struct GetObjectInlineCacheData {
    GetObjectInlineCacheData()
    {
        m_cachedhiddenClassChain = nullptr;
        m_cachedhiddenClassChainLength = 0;
        m_isPlainDataProperty = false;
        m_cachedIndex = 0;
    }

    ObjectStructure** m_cachedhiddenClassChain;
    bool m_isPlainDataProperty : 1;
    // 15bits of storage is enough
    // inlineCacheProtoTraverseMaxCount is so small
    uint16_t m_cachedhiddenClassChainLength : 15;
    static constexpr size_t inlineCacheCachedIndexMax = std::numeric_limits<uint16_t>::max();
    uint16_t m_cachedIndex : 16;
};

typedef Vector<GetObjectInlineCacheData, CustomAllocator<GetObjectInlineCacheData>, ComputeReservedCapacityFunctionWithLog2<>> GetObjectInlineCacheDataVector;

struct GetObjectInlineCacheComplexCaseData {
    GetObjectInlineCacheComplexCaseData(ObjectStructurePropertyName propertyName)
        : m_propertyName(propertyName)
    {
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    GetObjectInlineCacheDataVector m_cache;

    ObjectStructurePropertyName m_propertyName;
};

struct GetObjectInlineCacheSimpleCaseData : public gc {
    GetObjectInlineCacheSimpleCaseData(ObjectStructurePropertyName propertyName)
        : m_propertyName(propertyName)
    {
        memset(m_cachedStructures, 0, sizeof(ObjectStructure*) * inlineBufferSize);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static constexpr size_t inlineBufferSize = 3;

    ObjectStructure* m_cachedStructures[inlineBufferSize];
    uint8_t m_cachedIndexes[inlineBufferSize];

    ObjectStructurePropertyName m_propertyName;
};

class GetObjectPreComputedCase : public ByteCode {
public:
    // [object] -> [value]
    GetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t objectRegisterIndex, const size_t storeRegisterIndex, ObjectStructurePropertyName propertyName)
        : ByteCode(Opcode::GetObjectPreComputedCaseOpcode, loc)
        , m_inlineCacheMode(None)
        , m_isLength(propertyName.plainString()->equals("length"))
        , m_inlineCacheProtoTraverseMaxIndex(0)
        , m_cacheMissCount(0)
        , m_propertyName(propertyName)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_storeRegisterIndex(storeRegisterIndex)
    {
    }

    bool hasInlineCache() const
    {
        return m_inlineCacheMode != None;
    }

    static constexpr size_t inlineCacheProtoTraverseMaxCount = 12;

    enum GetInlineCacheMode {
        None,
        Simple,
        Complex
    };

    GetInlineCacheMode m_inlineCacheMode : 2;
    bool m_isLength : 1;
    unsigned char m_inlineCacheProtoTraverseMaxIndex : 8;
    size_t m_cacheMissCount : 16;
    union {
        GetObjectInlineCacheSimpleCaseData* m_simpleInlineCache;
        GetObjectInlineCacheComplexCaseData* m_complexInlineCache;
        ObjectStructurePropertyName m_propertyName;
    };

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_storeRegisterIndex;
#ifndef NDEBUG
    void dump()
    {
        printf("get object r%u <- r%u.%s", m_storeRegisterIndex, m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data());
    }
#endif
};

class GetObjectPreComputedCaseSimpleInlineCache : public GetObjectPreComputedCase {
public:
};

COMPILE_ASSERT(sizeof(GetObjectPreComputedCaseSimpleInlineCache) == sizeof(GetObjectPreComputedCase), "");

struct SetObjectInlineCacheData {
    SetObjectInlineCacheData()
    {
        m_cachedHiddenClass = nullptr;
        m_cachedIndex = m_cachedhiddenClassChainLength = 0;
    }

    union {
        ObjectStructure** m_cachedHiddenClassChainData;
        ObjectStructure* m_cachedHiddenClass;
    };
    // 16bits of storage is enough
    // inlineCacheProtoTraverseMaxCount is so small
    uint16_t m_cachedhiddenClassChainLength : 16;
    static constexpr size_t inlineCacheCachedIndexMax = std::numeric_limits<uint16_t>::max();
    uint16_t m_cachedIndex : 16;
};

typedef Vector<SetObjectInlineCacheData, CustomAllocator<SetObjectInlineCacheData>, ComputeReservedCapacityFunctionWithLog2<>> SetObjectInlineCacheDataVector;

struct SetObjectInlineCache {
    SetObjectInlineCache()
    {
    }

    SetObjectInlineCacheDataVector m_cache;

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
        , m_inlineCacheProtoTraverseMaxIndex(0)
        , m_missCount(0)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;
    ObjectStructurePropertyName m_propertyName;
    SetObjectInlineCache* m_inlineCache;

    static constexpr size_t inlineCacheProtoTraverseMaxCount = 12;

    bool m_isLength : 1;
    unsigned char m_inlineCacheProtoTraverseMaxIndex : 8;
    uint16_t m_missCount : 16;
#ifndef NDEBUG
    void dump()
    {
        printf("set object r%u.%s <- r%u", m_objectRegisterIndex, m_propertyName.plainString()->toUTF8StringData().data(), m_loadRegisterIndex);
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
    void dump();
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
    void dump();
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
    void dump()
    {
        printf("initialize global variable %s <- r%u", m_variableName.string()->toUTF8StringData().data(), m_registerIndex);
    }
#endif
};

class Move : public ByteCode {
public:
    Move(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1) // 1 <= 0
        : ByteCode(Opcode::MoveOpcode, loc)
        , m_registerIndex0(registerIndex0)
        , m_registerIndex1(registerIndex1)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;

#ifndef NDEBUG
    void dump()
    {
        printf("mov r%u <- r%u", m_registerIndex1, m_registerIndex0);
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
    void dump()
    {
        printf("to number r%u <- r%u", m_dstIndex, m_srcIndex);
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
    void dump()
    {
        printf("increment r%u <- r%u", m_dstIndex, m_srcIndex);
    }
#endif
};

class ToNumericIncrement : public ByteCode {
public:
    ToNumericIncrement(const ByteCodeLOC& loc, const size_t srcIndex, const size_t storeIndex, const size_t dstIndex)
        : ByteCode(Opcode::ToNumericIncrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_storeIndex(storeIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_storeIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("to numeric increment(r%u) -> r%u, r%u", m_srcIndex, m_storeIndex, m_dstIndex);
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
    void dump()
    {
        printf("decrement r%u <- r%u", m_dstIndex, m_srcIndex);
    }
#endif
};

class ToNumericDecrement : public ByteCode {
public:
    ToNumericDecrement(const ByteCodeLOC& loc, const size_t srcIndex, const size_t storeIndex, const size_t dstIndex)
        : ByteCode(Opcode::ToNumericDecrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_storeIndex(storeIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_storeIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("to numeric decrement(r%u) -> r%u, r%u", m_srcIndex, m_storeIndex, m_dstIndex);
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
    void dump()
    {
        printf("unary minus r%u <- r%u", m_dstIndex, m_srcIndex);
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
    void dump()
    {
        printf("unary not r%u <- r%u", m_dstIndex, m_srcIndex);
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
    void dump()
    {
        printf("unary bitwise not r%u <- r%u", m_dstIndex, m_srcIndex);
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
    void dump()
    {
        printf("unary typeof r%u <- r%u", m_dstIndex, m_srcIndex);
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
    void dump()
    {
        printf("unary delete r%u <- r%u r%u %s", m_dstIndex, m_srcIndex0, m_srcIndex1, m_id.string()->toUTF8StringData().data());
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
    void dump()
    {
        printf("template operation(+) r%u <- r%u + r%u", m_dstIndex, m_src0Index, m_src1Index);
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

    explicit Jump(Opcode code, const ByteCodeLOC& loc, size_t jumpPosition)
        : ByteCode(code, loc)
        , m_jumpPosition(jumpPosition)
    {
    }

    size_t m_jumpPosition;

#ifndef NDEBUG
    void dump()
    {
        printf("jump %zu", dumpJumpPosition(m_jumpPosition));
    }
#endif
};

class ControlFlowRecord : public gc {
    friend class InterpreterSlowPath;
#if defined(ENABLE_CODE_CACHE)
    friend class CodeCacheWriter;
    friend class CodeCacheReader;
#endif

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

class JumpFlowRecord {
public:
    JumpFlowRecord(const size_t value, size_t count = 0, size_t outerLimitCount = SIZE_MAX)
        : m_wordValue(value)
        , m_count(count)
        , m_outerLimitCount(outerLimitCount)
    {
    }

    MAKE_STACK_ALLOCATED();

    ControlFlowRecord* createControlFlowRecord()
    {
        return new ControlFlowRecord(ControlFlowRecord::ControlFlowRecord::NeedsJump, m_wordValue, m_count, m_outerLimitCount);
    }

    size_t m_wordValue;
    size_t m_count;
    size_t m_outerLimitCount;
};

class JumpComplexCase : public ByteCode {
public:
    JumpComplexCase(size_t recordIndex)
        : ByteCode(Opcode::JumpComplexCaseOpcode, ByteCodeLOC(SIZE_MAX))
        , m_recordIndex(recordIndex)
    {
    }

    size_t m_recordIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("jump complex");
    }
#endif
};

COMPILE_ASSERT(sizeof(Jump) == sizeof(JumpComplexCase), "");

class JumpIfTrue : public Jump {
public:
    JumpIfTrue(const ByteCodeLOC& loc, const size_t registerIndex)
        : Jump(Opcode::JumpIfTrueOpcode, loc, SIZE_MAX)
        , m_registerIndex(registerIndex)
    {
    }

    JumpIfTrue(const ByteCodeLOC& loc, const size_t registerIndex, size_t pos)
        : Jump(Opcode::JumpIfTrueOpcode, loc, pos)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("jump if r%u is true -> %zu", m_registerIndex, dumpJumpPosition(m_jumpPosition));
    }
#endif
};

class JumpIfUndefinedOrNull : public Jump {
public:
    JumpIfUndefinedOrNull(const ByteCodeLOC& loc, bool shouldNegate, const size_t registerIndex)
        : Jump(Opcode::JumpIfUndefinedOrNullOpcode, loc, SIZE_MAX)
        , m_shouldNegate(shouldNegate)
        , m_registerIndex(registerIndex)
    {
    }

    JumpIfUndefinedOrNull(const ByteCodeLOC& loc, bool shouldNegate, const size_t registerIndex, size_t pos)
        : Jump(Opcode::JumpIfUndefinedOrNullOpcode, loc, pos)
        , m_shouldNegate(shouldNegate)
        , m_registerIndex(registerIndex)
    {
    }

    bool m_shouldNegate; // condition should meet NOT undefined NOR null
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump()
    {
        if (m_shouldNegate) {
            printf("jump if r%u is not undefined nor null -> %zu", m_registerIndex, dumpJumpPosition(m_jumpPosition));
        } else {
            printf("jump if r%u is undefined or null -> %zu", m_registerIndex, dumpJumpPosition(m_jumpPosition));
        }
    }
#endif
};

class JumpIfFalse : public Jump {
public:
    JumpIfFalse(const ByteCodeLOC& loc, const size_t registerIndex)
        : Jump(Opcode::JumpIfFalseOpcode, loc, SIZE_MAX)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    void dump()
    {
        printf("jump if r%u is false -> %zu", m_registerIndex, dumpJumpPosition(m_jumpPosition));
    }
#endif
};

class JumpIfNotFulfilled : public Jump {
public:
    // compare if left value is less than (or equal) right value
    JumpIfNotFulfilled(const ByteCodeLOC& loc, const size_t leftIndex, const size_t rightIndex, bool containEqual, bool switched)
        : Jump(Opcode::JumpIfNotFulfilledOpcode, loc, SIZE_MAX)
        , m_leftIndex(leftIndex)
        , m_rightIndex(rightIndex)
        , m_containEqual(containEqual)
        , m_switched(switched)
    {
    }

    ByteCodeRegisterIndex m_leftIndex;
    ByteCodeRegisterIndex m_rightIndex;
    bool m_containEqual; // include equal condition
    bool m_switched; // left and right operands are switched

#ifndef NDEBUG
    void dump()
    {
        char* op;
        if (m_switched) {
            if (m_containEqual) {
                op = (char*)"greater than or equal";
            } else {
                op = (char*)"greater than";
            }
        } else {
            if (m_containEqual) {
                op = (char*)"less than or equal";
            } else {
                op = (char*)"less than";
            }
        }
        printf("jump if r%u is not %s r%u -> %zu", m_leftIndex, op, m_rightIndex, dumpJumpPosition(m_jumpPosition));
    }
#endif
};

class JumpIfEqual : public Jump {
public:
    JumpIfEqual(const ByteCodeLOC& loc, const size_t registerIndex0, const size_t registerIndex1, bool isStrict, bool shouldNegate)
        : Jump(Opcode::JumpIfEqualOpcode, loc, SIZE_MAX)
        , m_registerIndex0(registerIndex0)
        , m_registerIndex1(registerIndex1)
        , m_isStrict(isStrict)
        , m_shouldNegate(shouldNegate)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;
    bool m_isStrict;
    bool m_shouldNegate; // condition should meet NOT EQUAL

#ifndef NDEBUG
    void dump()
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
        printf("jump if r%u is %s to r%u -> %zu", m_registerIndex0, op, m_registerIndex1, dumpJumpPosition(m_jumpPosition));
    }
#endif
};

class Call : public ByteCode {
public:
    Call(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallOpcode, loc)
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
    void dump()
    {
        printf("call r%u <- r%u(r%u-r%u)", m_resultIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

class CallWithReceiver : public ByteCode {
public:
    CallWithReceiver(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallWithReceiverOpcode, loc)
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
    void dump()
    {
        printf("call r%u <- r%u,r%u(r%u-r%u)", m_resultIndex, m_receiverIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

// TCO
class CallReturn : public ByteCode {
public:
    CallReturn(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallReturnOpcode, loc)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
    {
    }
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump()
    {
        printf("tail call r%u(r%u-r%u)", m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

class TailRecursion : public ByteCode {
public:
    TailRecursion(const ByteCodeLOC& loc, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount)
        : ByteCode(Opcode::TailRecursionOpcode, loc)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
    {
    }
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump()
    {
        printf("tail recursion call r%u(r%u-r%u)", m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

class CallReturnWithReceiver : public ByteCode {
public:
    CallReturnWithReceiver(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallReturnWithReceiverOpcode, loc)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
    {
    }

    ByteCodeRegisterIndex m_receiverIndex;
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump()
    {
        printf("tail call with receiver <- r%u,r%u(r%u-r%u)", m_receiverIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

class TailRecursionWithReceiver : public ByteCode {
public:
    TailRecursionWithReceiver(const ByteCodeLOC& loc, const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t argumentCount)
        : ByteCode(Opcode::TailRecursionWithReceiverOpcode, loc)
        , m_receiverIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_argumentCount(argumentCount)
    {
    }

    ByteCodeRegisterIndex m_receiverIndex;
    ByteCodeRegisterIndex m_calleeIndex;
    ByteCodeRegisterIndex m_argumentsStartIndex;
    uint16_t m_argumentCount;

#ifndef NDEBUG
    void dump()
    {
        printf("tail recursion call with receiver r%u,r%u(r%u-r%u)", m_receiverIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
    }
#endif
};

COMPILE_ASSERT(sizeof(CallReturn) == sizeof(TailRecursion), "");
COMPILE_ASSERT(sizeof(CallReturnWithReceiver) == sizeof(TailRecursionWithReceiver), "");

class CallComplexCase : public ByteCode {
public:
    enum Kind ENSURE_ENUM_UNSIGNED {
        WithSpreadElement,
        MayBuiltinApply,
        MayBuiltinEval,
        InWithScope,
        Super,
        Import
    };

    CallComplexCase(const ByteCodeLOC& loc, Kind kind,
                    bool inWithScope, bool hasSpreadElement, bool isOptional,
                    const size_t receiverIndex, const size_t calleeIndex, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallComplexCaseOpcode, loc)
        , m_kind(kind)
        , m_inWithScope(inWithScope)
        , m_hasSpreadElement(hasSpreadElement)
        , m_isOptional(isOptional)
        , m_argumentCount(argumentCount)
        , m_receiverOrThisIndex(receiverIndex)
        , m_calleeIndex(calleeIndex)
        , m_argumentsStartIndex(argumentsStartIndex)
        , m_resultIndex(resultIndex)
    {
        ASSERT(m_kind != InWithScope);
    }

    CallComplexCase(const ByteCodeLOC& loc, bool hasSpreadElement, bool isOptional,
                    AtomicString calleeName, const size_t argumentsStartIndex, const size_t resultIndex, const size_t argumentCount)
        : ByteCode(Opcode::CallComplexCaseOpcode, loc)
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
            ByteCodeRegisterIndex m_receiverOrThisIndex;
            ByteCodeRegisterIndex m_calleeIndex;
        };
        AtomicString m_calleeName; // used with InWithScope
    };
    ByteCodeRegisterIndex m_argumentsStartIndex;
    ByteCodeRegisterIndex m_resultIndex;

#ifndef NDEBUG
    void dump()
    {
        if (m_kind == Kind::InWithScope) {
            printf("call(complex inWith) r%u <- %s(r%u-r%u)", m_resultIndex, m_calleeName.string()->toNonGCUTF8StringData().data(), m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
        } else {
            if (m_receiverOrThisIndex != REGISTER_LIMIT) {
                printf("call(complex %u) r%u <- r%u,r%u(r%u-r%u)", m_kind, m_resultIndex, m_receiverOrThisIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
            } else {
                printf("call(complex %u) r%u <- r%u(r%u-r%u)", m_kind, m_resultIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
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
    void dump()
    {
        printf("execution resume");
    }
#endif
};

class ExecutionPause : public ByteCode {
public:
    enum Reason ENSURE_ENUM_UNSIGNED {
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
    void dump()
    {
        printf("execution pause ");

        if (m_reason == Reason::Yield) {
            printf("yield r%u r%u r%u", m_yieldData.m_yieldIndex, m_yieldData.m_dstIndex, m_yieldData.m_dstStateIndex);
        } else if (m_reason == Reason::Await) {
            printf("await r%u r%u r%u", m_awaitData.m_awaitIndex, m_awaitData.m_dstIndex, m_awaitData.m_dstStateIndex);
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
    void dump()
    {
        printf("new r%u <- r%u(r%u-r%u)", m_resultIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
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
    void dump()
    {
        printf("new(spread) r%u <- r%u(r%u-r%u)", m_resultIndex, m_calleeIndex, m_argumentsStartIndex, m_argumentsStartIndex + m_argumentCount);
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
    void dump()
    {
        printf("get parameter r%u <- argv[%u]", m_registerIndex, m_paramIndex);
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
    void dump()
    {
        printf("return r%u", m_registerIndex);
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
    void dump()
    {
        printf("try (hasCatch:%u, save catched value to:r%u, hasFinalizer:%u)", m_hasCatch, m_catchedValueRegisterIndex, m_hasFinalizer);
    }
#endif
};

class CloseLexicalEnvironment : public ByteCode {
public:
    explicit CloseLexicalEnvironment(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CloseLexicalEnvironmentOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump()
    {
        printf("close lexical environment");
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
    void dump()
    {
        printf("throw r%u", m_registerIndex);
    }
#endif
};

class ThrowStaticErrorOperation : public ByteCode {
public:
    ThrowStaticErrorOperation(const ByteCodeLOC& loc, uint8_t errorKind, const char* errorMessage, AtomicString templateDataString = AtomicString())
        : ByteCode(Opcode::ThrowStaticErrorOperationOpcode, loc)
        , m_errorKind(errorKind)
        , m_errorMessage(errorMessage)
        , m_templateDataString(templateDataString)
    {
    }
    uint8_t m_errorKind;
    const char* m_errorMessage;
    AtomicString m_templateDataString;

#ifndef NDEBUG
    void dump()
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
    void dump()
    {
        printf("create enumerate object r%u <- r%u", m_dataRegisterIndex, m_objectRegisterIndex);
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
    void dump()
    {
        printf("get enumerate key r%u r%u", m_registerIndex, m_dataRegisterIndex);
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
    void dump()
    {
        printf("check if key is last r%u", m_registerIndex);
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
    void dump()
    {
        printf("mark enumerate key r%u to enumerate object r%u", m_keyRegisterIndex, m_dataRegisterIndex);
    }
#endif
};

class IteratorOperation : public ByteCode {
public:
    enum Operation ENSURE_ENUM_UNSIGNED {
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
    void dump()
    {
        if (m_operation == Operation::GetIterator) {
            printf("get iterator(r%u) -> r%u", m_getIteratorData.m_srcObjectRegisterIndex, m_getIteratorData.m_dstIteratorRecordIndex);
        } else if (m_operation == Operation::IteratorClose) {
            printf("iterator close(r%u, r%u)", m_iteratorCloseData.m_iterRegisterIndex, m_iteratorCloseData.m_execeptionRegisterIndexIfExists);
        } else if (m_operation == Operation::IteratorBind) {
            printf("iterator bind(r%u) -> r%u", m_iteratorBindData.m_iterRegisterIndex, m_iteratorBindData.m_registerIndex);
        } else if (m_operation == Operation::IteratorTestDone) {
            printf("iterator test done iteratorRecord(%u)->m_done -> r%u", m_iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex,
                   m_iteratorTestDoneData.m_dstRegisterIndex);
        } else if (m_operation == Operation::IteratorNext) {
            printf("iterator next r%u.[[next]](r%u) -> r%u", m_iteratorNextData.m_iteratorRecordRegisterIndex,
                   m_iteratorNextData.m_valueRegisterIndex, m_iteratorNextData.m_returnRegisterIndex);
        } else if (m_operation == Operation::IteratorTestResultIsObject) {
            printf("iterator test result is object r%u", m_iteratorTestResultIsObjectData.m_valueRegisterIndex);
        } else if (m_operation == Operation::IteratorValue) {
            printf("iterator value r%u -> r%u", m_iteratorValueData.m_srcRegisterIndex, m_iteratorValueData.m_dstRegisterIndex);
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
    void dump()
    {
        printf("get method r%u.%s -> r%u", m_objectRegisterIndex, m_propertyName.string()->toNonGCUTF8StringData().data(), m_resultRegisterIndex);
    }
#endif
};

class LoadRegExp : public ByteCode {
public:
    LoadRegExp(const ByteCodeLOC& loc, const size_t registerIndex, String* body, String* opt)
        : ByteCode(Opcode::LoadRegExpOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_body(body)
        , m_option(opt)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    String* m_body;
    String* m_option;
#ifndef NDEBUG
    void dump()
    {
        printf("load regexp %s -> r%u", m_body->toUTF8StringData().data(), m_registerIndex);
    }
#endif
};

class OpenLexicalEnvironment : public ByteCode {
public:
    enum Kind ENSURE_ENUM_UNSIGNED {
        WithStatement,
        ResumeExecution
    };
    OpenLexicalEnvironment(const ByteCodeLOC& loc, Kind kind, size_t registerIndex)
        : ByteCode(Opcode::OpenLexicalEnvironmentOpcode, loc)
        , m_kind(kind)
        , m_withOrThisRegisterIndex(registerIndex)
    {
        m_endPostion = SIZE_MAX;
    }

    Kind m_kind : 1;
    ByteCodeRegisterIndex m_withOrThisRegisterIndex : 16;
    size_t m_endPostion;
#ifndef NDEBUG
    void dump()
    {
        printf("open lexical env r%u(%u)", m_withOrThisRegisterIndex, m_kind);
    }
#endif
};

class ObjectDefineGetterSetter : public ByteCode {
public:
    ObjectDefineGetterSetter(const ByteCodeLOC& loc, size_t objectRegisterIndex, size_t objectPropertyNameRegisterIndex, size_t objectPropertyValueRegisterIndex, ObjectPropertyDescriptor::PresentAttribute presentAttribute, bool isGetter, bool isPrecomputed)
        : ByteCode(Opcode::ObjectDefineGetterSetterOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_objectPropertyNameRegisterIndex(objectPropertyNameRegisterIndex)
        , m_objectPropertyValueRegisterIndex(objectPropertyValueRegisterIndex)
        , m_presentAttribute(presentAttribute)
        , m_isGetter(isGetter)
        , m_isPrecomputed(isPrecomputed)
        , m_missCount(0)
        , m_inlineCachedStructureBefore(nullptr)
        , m_inlineCachedStructureAfter(nullptr)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex : REGISTER_INDEX_IN_BIT;
    ByteCodeRegisterIndex m_objectPropertyNameRegisterIndex : REGISTER_INDEX_IN_BIT;
    ByteCodeRegisterIndex m_objectPropertyValueRegisterIndex : REGISTER_INDEX_IN_BIT;
    ObjectPropertyDescriptor::PresentAttribute m_presentAttribute : 8;
    bool m_isGetter : 1; // other case, this is setter
    bool m_isPrecomputed : 1;
    size_t m_missCount : 4;
    ObjectStructure* m_inlineCachedStructureBefore;
    ObjectStructure* m_inlineCachedStructureAfter;

#ifndef NDEBUG
    void dump()
    {
        if (m_isGetter) {
            printf("object define getter r%u[r%u] = r%u", m_objectRegisterIndex, m_objectPropertyNameRegisterIndex, m_objectPropertyValueRegisterIndex);
        } else {
            printf("object define setter r%u[r%u] = r%u", m_objectRegisterIndex, m_objectPropertyNameRegisterIndex, m_objectPropertyValueRegisterIndex);
        }
    }
#endif
};

BYTECODE_SIZE_CHECK_IN_32BIT(ObjectDefineGetterSetter, sizeof(size_t) * 5);

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
    void dump()
    {
        printf("binding rest element(r%u) -> r%u", m_iterOrEnumIndex, m_dstIndex);
    }
#endif
};

class BlockOperation : public ByteCode {
public:
    BlockOperation(const ByteCodeLOC& loc, void* bi)
        : ByteCode(Opcode::BlockOperationOpcode, loc)
        , m_blockEndPosition(SIZE_MAX)
        , m_blockInfo(bi)
    {
    }

    size_t m_blockEndPosition;
    void* m_blockInfo; // should be InterpretedCodeBlock::BlockInfo
#ifndef NDEBUG
    void dump()
    {
        printf("block operation (%p) -> end %zu", m_blockInfo, m_blockEndPosition);
    }
#endif
};

class ReplaceBlockLexicalEnvironmentOperation : public ByteCode {
public:
    ReplaceBlockLexicalEnvironmentOperation(const ByteCodeLOC& loc, void* bi)
        : ByteCode(Opcode::ReplaceBlockLexicalEnvironmentOperationOpcode, loc)
        , m_blockInfo(bi)
    {
    }

    void* m_blockInfo; // should be InterpretedCodeBlock::BlockInfo
#ifndef NDEBUG
    void dump()
    {
        printf("replace block lexical env operation (%p)", m_blockInfo);
    }
#endif
};

class TaggedTemplateOperation : public ByteCode {
public:
    enum Operation ENSURE_ENUM_UNSIGNED {
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
    void dump()
    {
        if (m_operaton == TestCacheOperation) {
            printf("tagged template operation operation (test cache %zu)", m_testCacheOperationData.m_cacheIndex);
        } else {
            printf("tagged template operation operation (fill cache %zu)", m_testCacheOperationData.m_cacheIndex);
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
    void dump()
    {
        printf("ensure arguments object");
    }
#endif
};

class BindingCalleeIntoRegister : public ByteCode {
public:
    BindingCalleeIntoRegister(const ByteCodeLOC& loc)
        : ByteCode(Opcode::BindingCalleeIntoRegisterOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump()
    {
        printf("binding callee into register");
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
    void dump()
    {
        printf("r%u <- resolve name address (%s)", m_registerIndex, m_name.string()->toNonGCUTF8StringData().data());
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
    void dump()
    {
        printf("store with address r%u, %s <- r%u", m_addressRegisterIndex, m_name.string()->toUTF8StringData().data(), m_valueRegisterIndex);
    }
#endif
};

class FillOpcodeTable : public ByteCode {
public:
    explicit FillOpcodeTable(const ByteCodeLOC& loc)
        : ByteCode(Opcode::FillOpcodeTableOpcode, loc)
    {
    }

#ifndef NDEBUG
    void dump()
    {
        printf("fill opcode table");
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
    void dump()
    {
        printf("end(return with r[%u])", m_registerIndex);
    }
#endif
};

typedef Vector<char, std::allocator<char>, ComputeReservedCapacityFunctionWithLog2<200>> ByteCodeBlockData;
typedef Vector<Value, std::allocator<Value>> ByteCodeNumeralLiteralData;
typedef Vector<JumpFlowRecord, std::allocator<JumpFlowRecord>> ByteCodeJumpFlowRecordData;

typedef Vector<String*, GCUtil::gc_malloc_allocator<String*>> ByteCodeStringLiteralData;
typedef Vector<void*, GCUtil::gc_malloc_allocator<void*>> ByteCodeOtherLiteralData;

typedef std::vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>> ByteCodeLOCData;
typedef std::unordered_map<ByteCodeBlock*, ByteCodeLOCData*, std::hash<void*>, std::equal_to<void*>, std::allocator<std::pair<ByteCodeBlock* const, ByteCodeLOCData*>>> ByteCodeLOCDataMap;

class ByteCodeBlock : public gc {
public:
    explicit ByteCodeBlock();
    explicit ByteCodeBlock(InterpretedCodeBlock* codeBlock);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    template <typename CodeType>
    void pushCode(const CodeType& code, ByteCodeGenerateContext* context, size_t idx)
    {
#ifndef NDEBUG
        {
            CodeType& t = const_cast<CodeType&>(code);
            char* dumpByteCode = getenv("DUMP_BYTECODE");
            char* dumpCodeblockTree = getenv("DUMP_CODEBLOCK_TREE");
            if ((dumpByteCode && (strcmp(dumpByteCode, "1") == 0)) || (dumpCodeblockTree && (strcmp(dumpCodeblockTree, "1") == 0))) {
                if (idx != SIZE_MAX) {
                    auto loc = computeNodeLOC(m_codeBlock->src(), m_codeBlock->functionStart(), idx);
                    t.m_loc.line = loc.line;
                    t.m_loc.column = loc.column;
                }
            }
        }
#endif

        char* first = (char*)&code;
        size_t start = m_code.size();

        if (UNLIKELY(!!context->m_locData)) {
            context->m_locData->push_back(std::make_pair(start, idx));
        }

        m_code.resizeWithUninitializedValues(m_code.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_code[start++] = *first;
            first++;
        }

        m_requiredOperandRegisterNumber = std::max(m_requiredOperandRegisterNumber, (ByteCodeRegisterIndex)context->m_baseRegisterCount);

        // TODO throw exception
        RELEASE_ASSERT(m_requiredOperandRegisterNumber < REGISTER_LIMIT);

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

    ByteCodeLexicalBlockContext pushLexicalBlock(ByteCodeGenerateContext* context, void* bi, Node* node, bool initFunctionDeclarationInside = true);
    void finalizeLexicalBlock(ByteCodeGenerateContext* context, const ByteCodeBlock::ByteCodeLexicalBlockContext& ctx);
    void initFunctionDeclarationWithinBlock(ByteCodeGenerateContext* context, void* bi, Node* node);

    void pushPauseStatementExtraData(ByteCodeGenerateContext* context);

    InterpretedCodeBlock* codeBlock() { return m_codeBlock; }
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
        siz += m_numeralLiteralData.size() * sizeof(Value);
        siz += m_jumpFlowRecordData.size() * sizeof(JumpFlowRecord);
        siz += m_stringLiteralData.size() * sizeof(intptr_t);
        siz += m_otherLiteralData.size() * sizeof(intptr_t);
        siz += m_inlineCacheDataSize;
        return siz;
    }

    ExtendedNodeLOC computeNodeLOCFromByteCode(Context* c, size_t codePosition, InterpretedCodeBlock* cb, ByteCodeLOCData* locData);
    ExtendedNodeLOC computeNodeLOC(StringView src, ExtendedNodeLOC sourceElementStart, size_t index);
    void fillLOCData(Context* c, ByteCodeLOCData* locData);

    bool m_shouldClearStack : 1;
    bool m_isOwnerMayFreed : 1;
    // number of bytecode registers used for bytecode operation like adding...moving...
    ByteCodeRegisterIndex m_requiredOperandRegisterNumber : REGISTER_INDEX_IN_BIT;
    // precomputed value of total register number which is "m_requiredTotalRegisterNumber + stack allocated variables size"
    ByteCodeRegisterIndex m_requiredTotalRegisterNumber : REGISTER_INDEX_IN_BIT;
    size_t m_inlineCacheDataSize;

    ByteCodeBlockData m_code;
    ByteCodeNumeralLiteralData m_numeralLiteralData;
    ByteCodeJumpFlowRecordData m_jumpFlowRecordData;

    // m_stringLiteralData only holds String addesses not to be deallocated by GC
    ByteCodeStringLiteralData m_stringLiteralData;
    // m_otherLiteralData only holds various typed addesses not to be deallocated by GC
    ByteCodeOtherLiteralData m_otherLiteralData;

    InterpretedCodeBlock* m_codeBlock;
};
} // namespace Escargot

#endif
