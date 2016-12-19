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
#define FOR_EACH_BYTECODE_OP(F)         \
    F(LoadLiteral, 1, 0)                \
    F(LoadByName, 1, 0)                 \
    F(StoreByName, 0, 0)                \
    F(LoadByStackIndex, 1, 0)           \
    F(StoreByStackIndex, 0, 0)          \
    F(LoadByHeapIndex, 1, 0)            \
    F(StoreByHeapIndex, 0, 0)           \
    F(LoadByGlobalName, 1, 0)           \
    F(StoreByGlobalName, 0, 0)          \
    F(DeclareVarVariable, 0, 0)         \
    F(DeclareFunctionDeclaration, 1, 0) \
    F(DeclareFunctionExpression, 1, 0)  \
    F(GetThis, 1, 0)                    \
    F(NewOperation, 1, 0)               \
    F(BinaryPlus, 1, 2)                 \
    F(BinaryMinus, 1, 2)                \
    F(BinaryMultiply, 1, 2)             \
    F(BinaryDivision, 1, 2)             \
    F(BinaryMod, 1, 2)                  \
    F(BinaryEqual, 1, 2)                \
    F(BinaryLessThan, 1, 2)             \
    F(BinaryLessThanOrEqual, 1, 2)      \
    F(BinaryGreaterThan, 1, 2)          \
    F(BinaryGreaterThanOrEqual, 1, 2)   \
    F(BinaryNotEqual, 1, 2)             \
    F(BinaryStrictEqual, 1, 2)          \
    F(BinaryNotStrictEqual, 1, 2)       \
    F(BinaryBitwiseAnd, 1, 2)           \
    F(BinaryBitwiseOr, 1, 2)            \
    F(BinaryBitwiseXor, 1, 2)           \
    F(BinaryLeftShift, 1, 2)            \
    F(BinarySignedRightShift, 1, 2)     \
    F(BinaryUnsignedRightShift, 1, 2)   \
    F(CreateObject, 1, 0)               \
    F(CreateArray, 1, 0)                \
    F(GetObject, 1, 2)                  \
    F(SetObject, 0, 2)                  \
    F(GetObjectPreComputedCase, 1, 1)   \
    F(SetObjectPreComputedCase, 0, 1)   \
    F(GetGlobalObject, 1, 1)            \
    F(SetGlobalObject, 0, 1)            \
    F(Move, 1, 0)                       \
    F(Increment, 1, 1)                  \
    F(Decrement, 1, 1)                  \
    F(UnaryMinus, 1, 1)                 \
    F(UnaryPlus, 1, 1)                  \
    F(UnaryNot, 1, 1)                   \
    F(UnaryBitwiseNot, 1, 1)            \
    F(UnaryTypeof, 1, 1)                \
    F(Jump, 0, 0)                       \
    F(JumpComplexCase, 0, 0)            \
    F(JumpIfTrue, 0, 0)                 \
    F(JumpIfFalse, 0, 0)                \
    F(CallFunction, -1, 0)              \
    F(ReturnFunction, 0, 0)             \
    F(TryOperation, 0, 0)               \
    F(TryCatchBodyEnd, 0, 0)            \
    F(FinallyEnd, 0, 0)                 \
    F(ThrowOperation, 0, 0)             \
    F(EnumerateObject, 1, 0)            \
    F(EnumerateObjectKey, 1, 0)         \
    F(CheckIfKeyIsLast, 0, 0)           \
    F(LoadRegexp, 1, 0)                 \
    F(CallNativeFunction, 0, 0)         \
    F(CallEvalFunction, 0, 0)           \
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
        , m_loc(loc)
#ifndef NDEBUG
        , m_node(nullptr)
        , m_orgOpcode(EndOpcode)
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
    ByteCodeLOC m_loc;
#ifndef NDEBUG
    Node* m_node;
    Opcode m_orgOpcode;

    void dumpCode(size_t pos)
    {
        printf("%d\t\t", (int)pos);
        dump();
        printf(" %s ", getByteCodeNameFromAddress(m_opcodeInAddress));
        printf("(line: %d:%d)\n", (int)m_loc.line, (int)m_loc.column);
    }

    virtual void dump()
    {
    }

#endif
};

class LoadLiteral : public ByteCode {
public:
    LoadLiteral(const ByteCodeLOC& loc, const size_t& registerIndex, const Value& v)
        : ByteCode(Opcode::LoadLiteralOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_value(v)
    {
    }
    size_t m_registerIndex;
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
    size_t m_registerIndex;
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
    size_t m_registerIndex;
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
    size_t m_registerIndex;
    size_t m_index;

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
    size_t m_registerIndex;
    size_t m_index;

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
    size_t m_registerIndex;
    size_t m_upperIndex;
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
    size_t m_registerIndex;
    size_t m_upperIndex;
    size_t m_index;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store heap[%d][%d] <- r%d", (int)m_upperIndex, (int)m_index, (int)m_registerIndex);
    }
#endif
};

class LoadByGlobalName : public ByteCode {
public:
    LoadByGlobalName(const ByteCodeLOC& loc, const size_t& registerIndex, const PropertyName& name)
        : ByteCode(Opcode::LoadByGlobalNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_cacheIndex(SIZE_MAX)
        , m_name(name)
    {
    }
    size_t m_registerIndex;
    size_t m_cacheIndex;
    PropertyName m_name;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("load r%d <- global[%s]", (int)m_registerIndex, m_name.string()->toUTF8StringData().data());
    }
#endif
};

class StoreByGlobalName : public ByteCode {
public:
    StoreByGlobalName(const ByteCodeLOC& loc, const size_t& registerIndex, const PropertyName& name)
        : ByteCode(Opcode::StoreByGlobalNameOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_cacheIndex(SIZE_MAX)
        , m_name(name)
    {
    }
    size_t m_registerIndex;
    size_t m_cacheIndex;
    PropertyName m_name;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store global[%s] <- r%d", m_name.string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class DeclareVarVariable : public ByteCode {
public:
    DeclareVarVariable(const ByteCodeLOC& loc, const AtomicString& name)
        : ByteCode(Opcode::DeclareVarVariableOpcode, loc)
        , m_name(name)
    {
    }
    AtomicString m_name;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("declare %s", m_name.string()->toUTF8StringData().data());
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

    size_t m_registerIndex;
    CodeBlock* m_codeBlock;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("function expression %s -> r%d", m_codeBlock->functionName().string()->toUTF8StringData().data(), (int)m_registerIndex);
    }
#endif
};

class GetThis : public ByteCode {
public:
    GetThis(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::GetThisOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("this -> r%d", (int)m_registerIndex);
    }
#endif
};

class NewOperation : public ByteCode {
public:
    NewOperation(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& argc)
        : ByteCode(Opcode::NewOperationOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_argumentCount(argc)
    {
    }

    size_t m_registerIndex;
    size_t m_argumentCount;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("new -> r%d", (int)m_registerIndex);
    }
#endif
};

#ifdef NDEBUG
#define DEFINE_BINARY_OPERATION_DUMP(name)
#else
#define DEFINE_BINARY_OPERATION_DUMP(name)                                                      \
    virtual void dump()                                                                         \
    {                                                                                           \
        printf(name " r%d <- r%d , r%d", (int)m_srcIndex0, (int)m_srcIndex0, (int)m_srcIndex1); \
    }
#endif

#define DEFINE_BINARY_OPERATION(CodeName, HumanName)                                                         \
    class Binary##CodeName : public ByteCode {                                                               \
    public:                                                                                                  \
        Binary##CodeName(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1) \
            : ByteCode(Opcode::Binary##CodeName##Opcode, loc)                                                \
            , m_srcIndex0(registerIndex0)                                                                    \
            , m_srcIndex1(registerIndex1)                                                                    \
        {                                                                                                    \
        }                                                                                                    \
        size_t m_srcIndex0;                                                                                  \
        size_t m_srcIndex1;                                                                                  \
        DEFINE_BINARY_OPERATION_DUMP(HumanName)                                                              \
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
DEFINE_BINARY_OPERATION(StrictEqual, "strict erqual");
DEFINE_BINARY_OPERATION(NotStrictEqual, "not strict erqual");
DEFINE_BINARY_OPERATION(BitwiseAnd, "bitwise and");
DEFINE_BINARY_OPERATION(BitwiseOr, "bitwise or");
DEFINE_BINARY_OPERATION(BitwiseXor, "bitwise Xor");
DEFINE_BINARY_OPERATION(LeftShift, "left shift");
DEFINE_BINARY_OPERATION(SignedRightShift, "signed right shift");
DEFINE_BINARY_OPERATION(UnsignedRightShift, "unsigned right shift");


class CreateObject : public ByteCode {
public:
    CreateObject(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::CreateObjectOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

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
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("createarray -> r%d", (int)m_registerIndex);
    }
#endif
};

class GetObject : public ByteCode {
public:
    // [object, property] -> [value]
    GetObject(const ByteCodeLOC& loc, const size_t& objectRegisterIndex)
        : ByteCode(Opcode::GetObjectOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
    {
    }

    size_t m_objectRegisterIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("get object r%d <- r%d[r%d]", (int)m_objectRegisterIndex, (int)m_objectRegisterIndex, (int)m_objectRegisterIndex + 1);
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

    size_t m_objectRegisterIndex;
    size_t m_propertyRegisterIndex;
    size_t m_loadRegisterIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("set object r%d[r%d] <- r%d", (int)m_objectRegisterIndex, (int)m_propertyRegisterIndex, (int)m_loadRegisterIndex);
    }
#endif
};


typedef Vector<std::pair<ObjectStructure*, size_t>, gc_malloc_ignore_off_page_allocator<std::pair<ObjectStructure*, size_t>>> ObjectStructureChain;

struct GetObjectInlineCacheData {
    GetObjectInlineCacheData()
    {
        m_cachedIndex = SIZE_MAX;
    }
    ObjectStructureChain m_cachedhiddenClassChain;
    size_t m_cachedIndex;
};

struct GetObjectInlineCache {
    Vector<GetObjectInlineCacheData, gc_malloc_ignore_off_page_allocator<GetObjectInlineCacheData>> m_cache;
    size_t m_executeCount;
    GetObjectInlineCache()
    {
        m_executeCount = 0;
    }
};

class GetObjectPreComputedCase : public ByteCode {
public:
    // [object] -> [value]
    GetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, PropertyName propertyName)
        : ByteCode(Opcode::GetObjectPreComputedCaseOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyName(propertyName)
    {
    }

    size_t m_objectRegisterIndex;
    PropertyName m_propertyName;
    GetObjectInlineCache m_inlineCache;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("get object r%d <- r%d.%s", (int)m_objectRegisterIndex, (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data());
    }
#endif
};

struct SetObjectInlineCache {
    ObjectStructureChain m_cachedhiddenClassChain;
    size_t m_cachedIndex;
    ObjectStructure* m_hiddenClassWillBe;
    SetObjectInlineCache()
    {
        m_cachedIndex = SIZE_MAX;
        m_hiddenClassWillBe = nullptr;
    }

    void invalidateCache()
    {
        m_cachedIndex = SIZE_MAX;
        m_hiddenClassWillBe = nullptr;
        m_cachedhiddenClassChain.clear();
    }
};

class SetObjectPreComputedCase : public ByteCode {
public:
    SetObjectPreComputedCase(const ByteCodeLOC& loc, const size_t& objectRegisterIndex, PropertyName propertyName, const size_t& loadRegisterIndex)
        : ByteCode(Opcode::SetObjectPreComputedCaseOpcode, loc)
        , m_objectRegisterIndex(objectRegisterIndex)
        , m_propertyName(propertyName)
        , m_loadRegisterIndex(loadRegisterIndex)
    {
    }

    size_t m_objectRegisterIndex;
    PropertyName m_propertyName;
    size_t m_loadRegisterIndex;
    SetObjectInlineCache m_inlineCache;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("set object r%d.%s <- r%d", (int)m_objectRegisterIndex, m_propertyName.string()->toUTF8StringData().data(), (int)m_loadRegisterIndex);
    }
#endif
};

class GetGlobalObject : public ByteCode {
public:
    GetGlobalObject(const ByteCodeLOC& loc, const size_t& registerIndex, PropertyName propertyName)
        : ByteCode(Opcode::GetGlobalObjectOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_propertyName(propertyName)
    {
    }

    size_t m_registerIndex;
    PropertyName m_propertyName;
    GetObjectInlineCache m_inlineCache;
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
    }

    size_t m_registerIndex;
    PropertyName m_propertyName;
    SetObjectInlineCache m_inlineCache;
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

    size_t m_registerIndex0;
    size_t m_registerIndex1;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("mov r%d <- r%d", (int)m_registerIndex1, (int)m_registerIndex0);
    }
#endif
};

class Increment : public ByteCode {
public:
    Increment(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::IncrementOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("increment r%d", (int)m_registerIndex);
    }
#endif
};

class Decrement : public ByteCode {
public:
    Decrement(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::DecrementOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("decrement r%d", (int)m_registerIndex);
    }
#endif
};

class UnaryMinus : public ByteCode {
public:
    UnaryMinus(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::UnaryMinusOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary minus r%d", (int)m_registerIndex);
    }
#endif
};

class UnaryPlus : public ByteCode {
public:
    UnaryPlus(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::UnaryPlusOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary plus r%d", (int)m_registerIndex);
    }
#endif
};

class UnaryNot : public ByteCode {
public:
    UnaryNot(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::UnaryNotOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary not r%d", (int)m_registerIndex);
    }
#endif
};

class UnaryBitwiseNot : public ByteCode {
public:
    UnaryBitwiseNot(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::UnaryBitwiseNotOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }

    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary bitwise not r%d", (int)m_registerIndex);
    }
#endif
};

class UnaryTypeof : public ByteCode {
public:
    UnaryTypeof(const ByteCodeLOC& loc, const size_t& registerIndex, AtomicString name)
        : ByteCode(Opcode::UnaryTypeofOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_id(name)
    {
    }

    size_t m_registerIndex;
    AtomicString m_id;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("unary typeof r%d", (int)m_registerIndex);
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
public:
    enum ControlFlowReason {
        NeedsReturn,
        NeedsJump,
        NeedsThrow,
    };

protected:
    ControlFlowRecord(const ControlFlowReason& reason, const Value& value)
    {
        m_reason = reason;
        m_value = value;
    }

    ControlFlowRecord(const ControlFlowReason& reason, const Value& value, const Value& tryDupCount)
    {
        m_reason = reason;
        m_value = value;
        m_tryDupCount = tryDupCount;
    }

public:
    ControlFlowRecord* clone()
    {
        return new ControlFlowRecord(*this);
    }

    static ControlFlowRecord* create(const ControlFlowReason& reason, const Value& value)
    {
        return new ControlFlowRecord(reason, value);
    }

    static ControlFlowRecord* create(const ControlFlowReason& reason, const Value& value, const Value& tryDupCount)
    {
        return new ControlFlowRecord(reason, value, tryDupCount);
    }

    const ControlFlowReason& reason()
    {
        return m_reason;
    }

    const Value& value()
    {
        return m_value;
    }

    const Value& tryDupCount()
    {
        return m_tryDupCount;
    }

    void setValue(const Value& value)
    {
        m_value = value;
    }

    void setTryDupCount(const Value& tryDupCount)
    {
        m_tryDupCount = tryDupCount;
    }

protected:
    ControlFlowReason m_reason;
    Value m_value;
    Value m_tryDupCount;
};

class JumpComplexCase : public ByteCode {
public:
    JumpComplexCase(Jump* jmp, size_t tryDupCount)
        : ByteCode(Opcode::JumpComplexCaseOpcode, jmp->m_loc)
        , m_jumpPosition(jmp->m_jumpPosition)
    {
        m_controlFlowRecord = ControlFlowRecord::create(ControlFlowRecord::ControlFlowReason::NeedsJump,
                                                        Value((size_t)jmp->m_jumpPosition), Value((int32_t)tryDupCount));
#ifndef NDEBUG
        m_node = jmp->m_node;
#endif
    }

    ControlFlowRecord* m_controlFlowRecord;
    size_t m_jumpPosition;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("jump complex %d", (int)m_jumpPosition);
    }
#endif
};

class JumpIfTrue : public ByteCode {
public:
    JumpIfTrue(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::JumpIfTrueOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_jumpPosition(SIZE_MAX)
    {
    }

    size_t m_registerIndex;
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

    size_t m_registerIndex;
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
    // register usage (before call)
    // [receiver, callee, arg0, arg1,... arg<argument count-1> ]
    // register usage (after call)
    // [return value]
    CallFunction(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& argumentCount)
        : ByteCode(Opcode::CallFunctionOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_argumentCount(argumentCount)
    {
    }
    size_t m_registerIndex;
    size_t m_argumentCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call r%d <- r%d-r%d", (int)m_registerIndex, (int)m_registerIndex, (int)m_registerIndex + (int)m_argumentCount + 1);
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
    size_t m_registerIndex;
    size_t m_argumentCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call eval r%d <- r%d-r%d", (int)m_registerIndex, (int)m_registerIndex, (int)m_registerIndex + (int)m_argumentCount);
    }
#endif
};

class ReturnFunction : public ByteCode {
public:
    ReturnFunction(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::ReturnFunctionOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    size_t m_registerIndex;

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

class TryCatchBodyEnd : public ByteCode {
public:
    TryCatchBodyEnd(const ByteCodeLOC& loc)
        : ByteCode(Opcode::TryCatchBodyEndOpcode, loc)
    {
        m_finalizerPosition = SIZE_MAX;
    }

    size_t m_finalizerPosition;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("try-catch end");
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
    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("throw r%d", (int)m_registerIndex);
    }
#endif
};

struct EnumerateObjectData : public gc {
    EnumerateObjectData()
    {
        m_idx = 0;
    }

    ObjectStructureChain m_hiddenClassChain;
    Object* m_object;
    size_t m_idx;
    Vector<Value, gc_malloc_ignore_off_page_allocator<Value>> m_keys;
};


class EnumerateObject : public ByteCode {
public:
    EnumerateObject(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EnumerateObjectOpcode, loc)
    {
        m_objectRegisterIndex = m_dataRegisterIndex = SIZE_MAX;
    }
    size_t m_objectRegisterIndex;
    size_t m_dataRegisterIndex;

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
        m_dataRegisterIndex = m_registerIndex = SIZE_MAX;
    }
    size_t m_registerIndex;
    size_t m_dataRegisterIndex;

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
        m_forInEndPosition = m_registerIndex = SIZE_MAX;
    }
    size_t m_registerIndex;
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

    size_t m_registerIndex;
    String* m_body;
    String* m_option;
#ifndef NDEBUG
    virtual void dump()
    {
        printf("load regexp %s -> r%d", m_body->toUTF8StringData().data(), (int)m_registerIndex);
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


typedef Vector<char, gc_malloc_ignore_off_page_allocator<char>, 200> ByteCodeBlockData;
class ByteCodeBlock : public gc {
public:
    ByteCodeBlock()
    {
        m_requiredRegisterFileSizeInValueSize = 1;
    }

    template <typename CodeType>
    void pushCode(const CodeType& code, ByteCodeGenerateContext* context, Node* node)
    {
#ifndef NDEBUG
        {
            CodeType& t = const_cast<CodeType&>(code);
            t.m_node = node;
            if (node) {
                t.m_loc.line = node->m_loc.line;
                t.m_loc.column = node->m_loc.column;
            } else {
                t.m_loc.line = -1;
                t.m_loc.column = -1;
            }
            // t.m_loc.line = computeNodeLOCFromByteCode(&t, context->m_codeBlock).line;
            // t.m_loc.column = computeNodeLOCFromByteCode(&t, context->m_codeBlock).column;
        }
#endif

        const_cast<CodeType&>(code).assignOpcodeInAddress();
        char* first = (char*)&code;
        size_t start = m_code.size();
        m_code.resize(m_code.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_code[start++] = *first;
            first++;
        }

        m_requiredRegisterFileSizeInValueSize = std::max(m_requiredRegisterFileSizeInValueSize, (size_t)context->m_baseRegisterCount);
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

    NodeLOC computeNodeLOCFromByteCode(ByteCode* code, CodeBlock* cb);

    ByteCodeBlockData m_code;
    size_t m_requiredRegisterFileSizeInValueSize;
};
}

#endif
