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
    F(LoadByStackIndex, 1, 0)                         \
    F(StoreByStackIndex, 0, 0)                        \
    F(LoadByHeapIndex, 1, 0)                          \
    F(StoreByHeapIndex, 0, 0)                         \
    F(DeclareFunctionDeclaration, 1, 0)               \
    F(DeclareFunctionExpression, 1, 0)                \
    F(NewOperation, 1, 0)                             \
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
    F(ObjectDefineOwnPropertyOperation, 0, 0)         \
    F(ObjectDefineOwnPropertyWithNameOperation, 0, 0) \
    F(GetObject, 1, 2)                                \
    F(SetObject, 0, 2)                                \
    F(GetObjectPreComputedCase, 1, 1)                 \
    F(SetObjectPreComputedCase, 0, 1)                 \
    F(GetGlobalObject, 1, 1)                          \
    F(SetGlobalObject, 0, 1)                          \
    F(Move, 1, 0)                                     \
    F(Increment, 1, 1)                                \
    F(Decrement, 1, 1)                                \
    F(ToNumber, 1, 1)                                 \
    F(UnaryMinus, 1, 1)                               \
    F(UnaryNot, 1, 1)                                 \
    F(UnaryBitwiseNot, 1, 1)                          \
    F(UnaryTypeof, 1, 1)                              \
    F(UnaryDelete, 1, 1)                              \
    F(Jump, 0, 0)                                     \
    F(JumpComplexCase, 0, 0)                          \
    F(JumpIfTrue, 0, 0)                               \
    F(JumpIfFalse, 0, 0)                              \
    F(CallFunction, -1, 0)                            \
    F(CallFunctionWithReceiver, -1, 0)                \
    F(ReturnFunction, 0, 0)                           \
    F(ReturnFunctionWithValue, 0, 0)                  \
    F(TryOperation, 0, 0)                             \
    F(TryCatchWithBodyEnd, 0, 0)                      \
    F(FinallyEnd, 0, 0)                               \
    F(ThrowOperation, 0, 0)                           \
    F(EnumerateObject, 1, 0)                          \
    F(EnumerateObjectKey, 1, 0)                       \
    F(CheckIfKeyIsLast, 0, 0)                         \
    F(LoadRegexp, 1, 0)                               \
    F(WithOperation, 0, 0)                            \
    F(ObjectDefineGetter, 0, 0)                       \
    F(ObjectDefineSetter, 0, 0)                       \
    F(LoadArgumentsObject, 0, 0)                      \
    F(LoadArgumentsInWithScope, 0, 0)                 \
    F(StoreArgumentsObject, 0, 0)                     \
    F(CallNativeFunction, 0, 0)                       \
    F(CallEvalFunction, 0, 0)                         \
    F(CallBoundFunction, 0, 0)                        \
    F(CallFunctionInWithScope, 0, 0)                  \
    F(ResetExecuteResult, 0, 0)                       \
    F(End, 0, 0)

enum Opcode {
#define DECLARE_BYTECODE(name, pushCount, popCount) name##Opcode,
    FOR_EACH_BYTECODE_OP(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
        OpcodeKindEnd
} __attribute__((packed));

struct OpcodeTable {
    void* m_table[OpcodeKindEnd];
    std::pair<void*, Opcode> m_reverseTable[OpcodeKindEnd];
    OpcodeTable();
};

extern OpcodeTable g_opcodeTable;

#ifndef NDEBUG
inline const char* getByteCodeName(Opcode opcode)
{
    switch (opcode) {
#define RETURN_BYTECODE_NAME(name, pushCount, popCount) \
    case name##Opcode:                                  \
        return #name;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_NAME)
#undef RETURN_BYTECODE_NAME
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
#endif

#ifndef NDEBUG
inline const char* getByteCodeNameFromAddress(void* opcodeInAddress)
{
    for (size_t i = 0; i < OpcodeKindEnd; i++) {
        if (g_opcodeTable.m_reverseTable[i].first == opcodeInAddress)
            return getByteCodeName(g_opcodeTable.m_reverseTable[i].second);
    }
    ASSERT_NOT_REACHED();
}
#endif

inline size_t getByteCodePushCount(Opcode code)
{
    switch (code) {
#define RETURN_BYTECODE_CNT(name, pushCount, popCount) \
    case name##Opcode:                                 \
        return pushCount;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_CNT)
#undef RETURN_BYTECODE_CNT
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline size_t getByteCodePopCount(Opcode code)
{
    switch (code) {
#define RETURN_BYTECODE_CNT(name, pushCount, popCount) \
    case name##Opcode:                                 \
        return popCount;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_CNT)
#undef RETURN_BYTECODE_CNT
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}


struct ByteCodeLOC {
#ifndef NDEBUG
    size_t line;
    size_t column;
#endif
    size_t index;

    explicit ByteCodeLOC(size_t index)
    {
        this->index = index;
#ifndef NDEBUG
        this->line = SIZE_MAX;
        this->column = SIZE_MAX;
#endif
    }
};

class ByteCode : public gc {
public:
    MAKE_STACK_ALLOCATED();
    ByteCode(Opcode code, const ByteCodeLOC& loc)
        : m_opcodeInAddress((void*)code)
#ifndef NDEBUG
        , m_loc(loc)
        , m_orgOpcode(code)
#endif
    {
    }

    void assignOpcodeInAddress()
    {
#ifndef NDEBUG
        m_orgOpcode = (Opcode)(size_t)m_opcodeInAddress;
#endif
        m_opcodeInAddress = g_opcodeTable.m_table[(Opcode)(size_t)m_opcodeInAddress];
    }

    void* m_opcodeInAddress;
#ifndef NDEBUG
    ByteCodeLOC m_loc;
    Opcode m_orgOpcode;

    void dumpCode(size_t pos)
    {
        printf("%d\t\t", (int)pos);
        dump();
        printf(" | %s ", getByteCodeNameFromAddress(m_opcodeInAddress));
        printf("(line: %d:%d)\n", (int)m_loc.line, (int)m_loc.column);
    }

    virtual void dump()
    {
    }

#endif
};

#ifdef NDEBUG
COMPILE_ASSERT(sizeof(ByteCode) == (sizeof(size_t)), "");
#endif

class LoadLiteral : public ByteCode {
public:
    LoadLiteral(const ByteCodeLOC& loc, const size_t& registerIndex, const Value& v)
        : ByteCode(Opcode::LoadLiteralOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_value(v)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    Value m_value;

#ifndef NDEBUG
    virtual void dump()
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
    LoadByName(const ByteCodeLOC& loc, const size_t& registerIndex, const AtomicString& name)
        : ByteCode(Opcode::LoadByNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_name(name)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_name;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("load r%d <- %s", (int)m_registerIndex, m_name.string()->toUTF8StringData().data());
    }
#endif
};

class StoreByName : public ByteCode {
public:
    StoreByName(const ByteCodeLOC& loc, const size_t& registerIndex, const AtomicString& name)
        : ByteCode(Opcode::StoreByNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_name(name)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_name;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store %s <- r%d", m_name.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class LoadByStackIndex : public ByteCode {
public:
    LoadByStackIndex(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& index)
        : ByteCode(Opcode::LoadByStackIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    uint16_t m_index;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("load r%d <- stack[%d]", (int)m_registerIndex, (int)m_index);
    }
#endif
};

class StoreByStackIndex : public ByteCode {
public:
    StoreByStackIndex(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& index)
        : ByteCode(Opcode::StoreByStackIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    uint16_t m_index;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store stack[%d] <- r%d", (int)m_index, (int)m_registerIndex);
    }
#endif
};

class LoadByHeapIndex : public ByteCode {
public:
    LoadByHeapIndex(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& upperIndex, const size_t& index)
        : ByteCode(Opcode::LoadByHeapIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_upperIndex(upperIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    uint16_t m_upperIndex;
    size_t m_index;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("load r%d <- heap[%d][%d]", (int)m_registerIndex, (int)m_upperIndex, (int)m_index);
    }
#endif
};

class StoreByHeapIndex : public ByteCode {
public:
    StoreByHeapIndex(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& upperIndex, const size_t& index)
        : ByteCode(Opcode::StoreByHeapIndexOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_upperIndex(upperIndex)
        , m_index(index)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    uint16_t m_upperIndex;
    size_t m_index;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store heap[%d][%d] <- r%d", (int)m_upperIndex, (int)m_index, (int)m_registerIndex);
    }
#endif
};

class DeclareFunctionDeclaration : public ByteCode {
public:
    DeclareFunctionDeclaration(CodeBlock* cb)
        : ByteCode(Opcode::DeclareFunctionDeclarationOpcode, ByteCodeLOC(SIZE_MAX))
        , m_codeBlock(cb)
    {
    }
    CodeBlock* m_codeBlock;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("function declaration %s", m_codeBlock->functionName().string()->toUTF8StringData().data());
    }
#endif
};

class DeclareFunctionExpression : public ByteCode {
public:
    DeclareFunctionExpression(const ByteCodeLOC& loc, const size_t& registerIndex, CodeBlock* cb)
        : ByteCode(Opcode::DeclareFunctionExpressionOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_codeBlock(cb)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    CodeBlock* m_codeBlock;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("function expression %s -> r%d", m_codeBlock->functionName().string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

#ifdef NDEBUG
#define DEFINE_BINARY_OPERATION_DUMP(name)
#else
#define DEFINE_BINARY_OPERATION_DUMP(name)                                                     \
    virtual void dump()                                                                        \
    {                                                                                          \
        printf(name " r%d <- r%d , r%d", (int)m_dstIndex, (int)m_srcIndex0, (int)m_srcIndex1); \
    }
#endif

#define DEFINE_BINARY_OPERATION(CodeName, HumanName)                                                                                         \
    class Binary##CodeName : public ByteCode {                                                                                               \
    public:                                                                                                                                  \
        Binary##CodeName(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1, const size_t& dstRegisterIndex) \
            : ByteCode(Opcode::Binary##CodeName##Opcode, loc)                                                                                \
            , m_srcIndex0(registerIndex0)                                                                                                    \
            , m_srcIndex1(registerIndex1)                                                                                                    \
            , m_dstIndex(dstRegisterIndex)                                                                                                   \
        {                                                                                                                                    \
        }                                                                                                                                    \
        ByteCodeRegisterIndex m_srcIndex0;                                                                                                   \
        ByteCodeRegisterIndex m_srcIndex1;                                                                                                   \
        ByteCodeRegisterIndex m_dstIndex;                                                                                                    \
        DEFINE_BINARY_OPERATION_DUMP(HumanName)                                                                                              \
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
    CreateObject(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::CreateObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("createobject -> r%d", (int)m_registerIndex);
    }
#endif
};

class CreateArray : public ByteCode {
public:
    CreateArray(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::CreateArrayOpcode, loc)
        , m_registerIndex(registerIndex)
    {
        m_length = 0;
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_length;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("createarray -> r%d", (int)m_registerIndex);
    }
#endif
};

class GetObject : public ByteCode {
public:
    GetObject(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, const size_t& propertyRegisterIndex, const size_t& storeRegisterIndex)
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
    virtual void dump()
    {
        printf("get object r%d <- r%d[r%d]", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex);
    }
#endif
};

class SetObject : public ByteCode {
public:
    SetObject(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, const size_t& propertyRegisterIndex, const size_t& loadRegisterIndex)
        : ByteCode(Opcode::SetObjectOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyRegisterIndex(propertyRegisterIndex)
        , m_loadRegisterIndex(loadRegisterIndex)
    {
    }

    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_propertyRegisterIndex;
    ByteCodeRegisterIndex m_loadRegisterIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("set object r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};

class ObjectDefineOwnPropertyOperation : public ByteCode {
public:
    ObjectDefineOwnPropertyOperation(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, const size_t& propertyRegisterIndex, const size_t& loadRegisterIndex)
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
    virtual void dump()
    {
        printf("object define own property r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};

class ObjectDefineOwnPropertyWithNameOperation : public ByteCode {
public:
    ObjectDefineOwnPropertyWithNameOperation(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, AtomicString propertyName, const size_t& loadRegisterIndex)
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
    virtual void dump()
    {
        printf("object define own property with name r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};


struct ObjectStructureChainItem : public gc {
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
    GetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, const size_t& storeRegisterIndex, PropertyName propertyName)
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
    virtual void dump()
    {
        printf("get object r%d <- r%d.%s", (int)m_storeRegisterIndex, (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data());
    }
#endif
};

struct SetObjectInlineCache {
    ObjectStructureChain m_cachedhiddenClassChain;
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
    SetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, PropertyName propertyName, const size_t& loadRegisterIndex, SetObjectInlineCache* inlineCache)
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
    virtual void dump()
    {
        printf("set object r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

class GetGlobalObject : public ByteCode {
public:
    GetGlobalObject(const ByteCodeLOC& loc, const size_t& registerIndex, PropertyName propertyName, size_t index = SIZE_MAX)
        : ByteCode(Opcode::GetGlobalObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_propertyName(propertyName)
    {
        ASSERT(propertyName.hasAtomicString());
        m_cachedIndex = index;
        m_cachedStructure = nullptr;
    }

    ByteCodeRegisterIndex m_registerIndex;
    PropertyName m_propertyName;
    size_t m_cachedIndex;
    ObjectStructure* m_cachedStructure;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("get global object r%d <- global.%s", (int)m_registerIndex, m_propertyName.string()->toUTF8StringData().data());
    }
#endif
};

class SetGlobalObject : public ByteCode {
public:
    SetGlobalObject(const ByteCodeLOC& loc, const size_t& registerIndex, PropertyName propertyName)
        : ByteCode(Opcode::SetGlobalObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_propertyName(propertyName)
    {
        ASSERT(propertyName.hasAtomicString());
        m_cachedIndex = SIZE_MAX;
        m_cachedStructure = nullptr;
    }

    ByteCodeRegisterIndex m_registerIndex;
    PropertyName m_propertyName;
    size_t m_cachedIndex;
    ObjectStructure* m_cachedStructure;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("set global object global.%s <- r%d", m_propertyName.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class Move : public ByteCode {
public:
    Move(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1) // 1 <= 0
        : ByteCode(Opcode::MoveOpcode, loc),
          m_registerIndex0(registerIndex0),
          m_registerIndex1(registerIndex1)
    {
    }

    ByteCodeRegisterIndex m_registerIndex0;
    ByteCodeRegisterIndex m_registerIndex1;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("mov r%d <- r%d", (int)m_registerIndex1, (int)m_registerIndex0);
    }
#endif
};

class ToNumber : public ByteCode {
public:
    ToNumber(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::ToNumberOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("to number r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class Increment : public ByteCode {
public:
    Increment(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::IncrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("increment r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class Decrement : public ByteCode {
public:
    Decrement(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::DecrementOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("decrement r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryMinus : public ByteCode {
public:
    UnaryMinus(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::UnaryMinusOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary minus r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryNot : public ByteCode {
public:
    UnaryNot(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::UnaryNotOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary not r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryBitwiseNot : public ByteCode {
public:
    UnaryBitwiseNot(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex)
        : ByteCode(Opcode::UnaryBitwiseNotOpcode, loc)
        , m_srcIndex(srcIndex)
        , m_dstIndex(dstIndex)
    {
    }

    ByteCodeRegisterIndex m_srcIndex;
    ByteCodeRegisterIndex m_dstIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary bitwise not r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryTypeof : public ByteCode {
public:
    UnaryTypeof(const ByteCodeLOC& loc, const size_t& srcIndex, const size_t& dstIndex, AtomicString name)
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
    virtual void dump()
    {
        printf("unary typeof r%d <- r%d", (int)m_dstIndex, (int)m_srcIndex);
    }
#endif
};

class UnaryDelete : public ByteCode {
public:
    UnaryDelete(const ByteCodeLOC& loc, const size_t& srcIndex0, const size_t& srcIndex1, const size_t& dstIndex, AtomicString name)
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
    virtual void dump()
    {
        printf("unary delete r%d <- r%d r%d %s", (int)m_dstIndex, (int)m_srcIndex0, (int)m_srcIndex1, m_id.string()->toUTF8StringData().data());
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
    virtual void dump()
    {
        printf("jump %d", (int)m_jumpPosition);
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

    ControlFlowRecord(const ControlFlowReason& reason, const Value& value, size_t count = 0)
    {
        m_reason = reason;
        m_value = value;
        m_count = count;
    }

    ControlFlowRecord(const ControlFlowReason& reason, const size_t& value, size_t count = 0)
    {
        m_reason = reason;
        m_wordValue = value;
        m_count = count;
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

    void setWordValue(const size_t& value)
    {
        m_wordValue = value;
    }

    size_t count()
    {
        return m_count;
    }

    ControlFlowRecord* clone()
    {
        return new ControlFlowRecord(m_reason, m_value, m_count);
    }

protected:
    ControlFlowReason m_reason;
    union {
        Value m_value;
        size_t m_wordValue;
    };
    size_t m_count;
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
    virtual void dump()
    {
        printf("jump complex");
    }
#endif
};

COMPILE_ASSERT(sizeof(Jump) == sizeof(JumpComplexCase), "");

class JumpIfTrue : public ByteCode {
public:
    JumpIfTrue(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::JumpIfTrueOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_jumpPosition(SIZE_MAX)
    {
    }

    JumpIfTrue(const ByteCodeLOC& loc, const size_t& registerIndex, size_t pos)
        : ByteCode(Opcode::JumpIfTrueOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_jumpPosition(pos)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_jumpPosition;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("jump if true r%d -> %d", (int)m_registerIndex, (int)m_jumpPosition);
    }
#endif
};

class JumpIfFalse : public ByteCode {
public:
    JumpIfFalse(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::JumpIfFalseOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_jumpPosition(SIZE_MAX)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
    size_t m_jumpPosition;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("jump if false r%d -> %d", (int)m_registerIndex, (int)m_jumpPosition);
    }
#endif
};

class CallFunction : public ByteCode {
public:
    CallFunction(const ByteCodeLOC& loc, const size_t& calleeIndex, const size_t& argumentsStartIndex, const size_t& argumentCount, const size_t& resultIndex)
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
    virtual void dump()
    {
        printf("call r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallFunctionWithReceiver : public ByteCode {
public:
    CallFunctionWithReceiver(const ByteCodeLOC& loc, const size_t& receiverIndex, const size_t& calleeIndex, const size_t& argumentsStartIndex, const size_t& argumentCount, const size_t& resultIndex)
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
    virtual void dump()
    {
        printf("call r%d <- r%d,r%d(r%d-r%d)", (int)m_resultIndex, (int)m_receiverIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class CallEvalFunction : public ByteCode {
public:
    // register usage (before call)
    // [arg0, arg1,... arg<argument count-1> ]
    // register usage (after call)
    // [return value]
    CallEvalFunction(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& argumentCount)
        : ByteCode(Opcode::CallEvalFunctionOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_argumentCount(argumentCount)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    size_t m_argumentCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call eval r%d <- r%d-r%d", (int)m_registerIndex, (int)m_registerIndex, (int)m_registerIndex + (int)m_argumentCount);
    }
#endif
};

class CallBoundFunction : public ByteCode {
public:
    CallBoundFunction(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CallBoundFunctionOpcode, loc)
    {
    }
    FunctionObject* m_boundTargetFunction;
    Value m_boundThis;
    Value* m_boundArguments;
    size_t m_boundArgumentsCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call bound (bound args: %zu)", m_boundArgumentsCount);
    }
#endif
};

class CallFunctionInWithScope : public ByteCode {
public:
    // register usage (before call)
    // [arg0, arg1,... arg<argument count-1> ]
    // register usage (after call)
    // [return value]
    CallFunctionInWithScope(const ByteCodeLOC& loc, const size_t& registerIndex, const AtomicString& calleeName, const size_t& argumentCount)
        : ByteCode(Opcode::CallFunctionInWithScopeOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_calleeName(calleeName)
        , m_argumentCount(argumentCount)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;
    AtomicString m_calleeName;
    size_t m_argumentCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call in with r%d <- r%d-r%d", (int)m_registerIndex, (int)m_registerIndex, (int)m_registerIndex + (int)m_argumentCount - 1);
    }
#endif
};

class NewOperation : public ByteCode {
public:
    NewOperation(const ByteCodeLOC& loc, const size_t& calleeIndex, const size_t& argumentsStartIndex, const size_t& argumentCount, const size_t& resultIndex)
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
    virtual void dump()
    {
        printf("new r%d <- r%d(r%d-r%d)", (int)m_resultIndex, (int)m_calleeIndex, (int)m_argumentsStartIndex, (int)m_argumentsStartIndex + (int)m_argumentCount);
    }
#endif
};

class ReturnFunction : public ByteCode {
public:
    ReturnFunction(const ByteCodeLOC& loc)
        : ByteCode(Opcode::ReturnFunctionOpcode, loc)
    {
    }
#ifndef NDEBUG
    virtual void dump()
    {
        printf("return");
    }
#endif
};

class ReturnFunctionWithValue : public ByteCode {
public:
    ReturnFunctionWithValue(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::ReturnFunctionWithValueOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("return r%d", (int)m_registerIndex);
    }
#endif
};

class TryOperation : public ByteCode {
public:
    TryOperation(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryOperationOpcode, loc)
    {
        m_hasCatch = false;
        m_tryCatchEndPosition = m_catchPosition = SIZE_MAX;
    }

    bool m_hasCatch;
    size_t m_catchPosition;
    size_t m_tryCatchEndPosition;
    AtomicString m_catchVariableName;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("try (hasCatch:%d)", (int)m_hasCatch);
    }
#endif
};

class TryCatchWithBodyEnd : public ByteCode {
public:
    TryCatchWithBodyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryCatchWithBodyEndOpcode, loc)
    {
    }

#ifndef NDEBUG
    virtual void dump()
    {
        printf("try-catch-with end");
    }
#endif
};

class FinallyEnd : public ByteCode {
public:
    FinallyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::FinallyEndOpcode, loc)
    {
        m_tryDupCount = 0;
    }

    size_t m_tryDupCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("finally end");
    }
#endif
};

class ThrowOperation : public ByteCode {
public:
    ThrowOperation(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::ThrowOperationOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    ByteCodeRegisterIndex m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("throw r%d", (int)m_registerIndex);
    }
#endif
};

struct EnumerateObjectData : public PointerValue {
    EnumerateObjectData()
    {
        m_idx = 0;
    }

    virtual Type type()
    {
        return EnumerateObjectDataType;
    }

    ObjectStructureChain m_hiddenClassChain;
    Object* m_object;
    size_t m_idx;
    SmallValueVector m_keys;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};


class EnumerateObject : public ByteCode {
public:
    EnumerateObject(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnumerateObjectOpcode, loc)
    {
        m_objectRegisterIndex = m_dataRegisterIndex = std::numeric_limits<ByteCodeRegisterIndex>::max();
    }
    ByteCodeRegisterIndex m_objectRegisterIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("enumerate object r%d, r%d", (int)m_objectRegisterIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class EnumerateObjectKey : public ByteCode {
public:
    EnumerateObjectKey(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnumerateObjectKeyOpcode, loc)
    {
        m_dataRegisterIndex = m_registerIndex = std::numeric_limits<ByteCodeRegisterIndex>::max();
    }
    ByteCodeRegisterIndex m_registerIndex;
    ByteCodeRegisterIndex m_dataRegisterIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("enumerate object key r%d r%d", (int)m_registerIndex, (int)m_dataRegisterIndex);
    }
#endif
};

class CheckIfKeyIsLast : public ByteCode {
public:
    CheckIfKeyIsLast(const ByteCodeLOC& loc)
        : ByteCode(Opcode::CheckIfKeyIsLastOpcode, loc)
    {
        m_forInEndPosition = SIZE_MAX;
        m_registerIndex = std::numeric_limits<ByteCodeRegisterIndex>::max();
    }
    ByteCodeRegisterIndex m_registerIndex;
    size_t m_forInEndPosition;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("check if key is last r%d", (int)m_registerIndex);
    }
#endif
};

class LoadRegexp : public ByteCode {
public:
    LoadRegexp(const ByteCodeLOC& loc, const size_t& registerIndex, String* body, String* opt)
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
    virtual void dump()
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
    virtual void dump()
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
    virtual void dump()
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
    virtual void dump()
    {
        printf("object define setter r%d[r%d] = r%d", (int)m_objectRegisterIndex, (int)m_objectPropertyNameRegisterIndex, (int)m_objectPropertyValueRegisterIndex);
    }
#endif
};

class LoadArgumentsObject : public ByteCode {
public:
    LoadArgumentsObject(const ByteCodeLOC& loc, size_t registerIndex)
        : ByteCode(Opcode::LoadArgumentsObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("load arguments r%d", (int)m_registerIndex);
    }
#endif
};

class LoadArgumentsInWithScope : public ByteCode {
public:
    LoadArgumentsInWithScope(const ByteCodeLOC& loc, size_t registerIndex)
        : ByteCode(Opcode::LoadArgumentsInWithScopeOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("load arguments in with r%d", (int)m_registerIndex);
    }
#endif
};

class StoreArgumentsObject : public ByteCode {
public:
    StoreArgumentsObject(const ByteCodeLOC& loc, size_t registerIndex)
        : ByteCode(Opcode::StoreArgumentsObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    ByteCodeRegisterIndex m_registerIndex;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("store arguments r%d", (int)m_registerIndex);
    }
#endif
};

class CallNativeFunction : public ByteCode {
public:
    CallNativeFunction(NativeFunctionPointer fn)
        : ByteCode(Opcode::CallNativeFunctionOpcode, ByteCodeLOC(SIZE_MAX))
        , m_fn(fn)
    {
    }

    NativeFunctionPointer m_fn;
};

class ResetExecuteResult : public ByteCode {
public:
    ResetExecuteResult(const ByteCodeLOC& loc)
        : ByteCode(Opcode::ResetExecuteResultOpcode, loc)
    {
    }

#ifndef NDEBUG
    virtual void dump()
    {
        printf("reset execute result");
    }
#endif
};

class End : public ByteCode {
public:
    End(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EndOpcode, loc)
    {
    }

#ifndef NDEBUG
    virtual void dump()
    {
        printf("end");
    }
#endif
};


typedef Vector<char, std::allocator<char>, 200> ByteCodeBlockData;
typedef Vector<std::pair<size_t, size_t>, std::allocator<std::pair<size_t, size_t>>> ByteCodeLOCData;
typedef Vector<SmallValue, gc_malloc_ignore_off_page_allocator<SmallValue>> ByteCodeLiteralData;

class ByteCodeBlock : public gc {
public:
    ByteCodeBlock(CodeBlock* codeBlock)
    {
        m_requiredRegisterFileSizeInValueSize = 1;
        m_codeBlock = codeBlock;
        m_isEvalMode = false;
        m_isOnGlobal = false;
        m_shouldClearStack = false;

        GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                                void*) {
            ByteCodeBlock* self = (ByteCodeBlock*)obj;
            for (size_t i = 0; i < self->m_getObjectCodePositions.size(); i++) {
                GetObjectInlineCacheDataVector().swap(((GetObjectPreComputedCase*)((size_t)self->m_code.data() + self->m_getObjectCodePositions[i]))->m_inlineCache.m_cache);
            }
            std::vector<size_t>().swap(self->m_getObjectCodePositions);

            self->m_code.clear();
            self->m_locData.clear();
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
                if (idx != SIZE_MAX) {
                    auto loc = computeNodeLOC(m_codeBlock->src(), m_codeBlock->sourceElementStart(), idx);
                    t.m_loc.line = loc.line;
                    t.m_loc.column = loc.column;
                }
            }
        }
#endif

        // const_cast<CodeType&>(code).assignOpcodeInAddress();
        char* first = (char*)&code;
        size_t start = m_code.size();
        if (context->m_shouldGenerateLOCData)
            m_locData.pushBack(std::make_pair(start, idx));

        m_code.resize(m_code.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_code[start++] = *first;
            first++;
        }

        m_requiredRegisterFileSizeInValueSize = std::max(m_requiredRegisterFileSizeInValueSize, (size_t)context->m_baseRegisterCount);

        // TODO throw exception
        RELEASE_ASSERT(m_requiredRegisterFileSizeInValueSize < std::numeric_limits<ByteCodeRegisterIndex>::max());
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

    ExtendedNodeLOC computeNodeLOCFromByteCode(Context* c, size_t codePosition, CodeBlock* cb);
    ExtendedNodeLOC computeNodeLOC(StringView src, ExtendedNodeLOC sourceElementStart, size_t index);
    void fillLocDataIfNeeded(Context* c);

    bool m_isEvalMode;
    bool m_isOnGlobal;
    bool m_shouldClearStack;

    ByteCodeBlockData m_code;
    ByteCodeLiteralData m_literalData;
    ByteCodeLOCData m_locData;
    size_t m_requiredRegisterFileSizeInValueSize;
    CodeBlock* m_codeBlock;

    std::vector<size_t> m_getObjectCodePositions;
};
}

#endif
