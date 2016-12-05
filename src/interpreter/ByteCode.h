#ifndef __EscargotByteCode__
#define __EscargotByteCode__

#include "runtime/Value.h"
#include "runtime/String.h"
#include "parser/CodeBlock.h"
#include "parser/ast/Node.h"
#include "runtime/SmallValue.h"
#include "interpreter/ByteCodeGenerator.h"

namespace Escargot {

class Node;

// <OpcodeName, PushCount, PopCount>
#define FOR_EACH_BYTECODE_OP(F) \
    F(LoadLiteral, 1, 0) \
    F(LoadByName, 1, 0) \
    F(StoreByName, 0, 0) \
    F(LoadByStackIndex, 1, 0) \
    F(StoreByStackIndex, 0, 0) \
    F(LoadByHeapIndex, 1, 0) \
    F(StoreByGlobalName, 0, 0) \
    F(LoadByGlobalName, 1, 0) \
    F(StoreByHeapIndex, 0, 0) \
    F(DeclareVarVariable, 0, 0) \
    F(DeclareFunctionDeclaration, 1, 0) \
    F(DeclareFunctionExpression, 1, 0) \
    F(BinaryPlus, 1, 2) \
    F(BinaryMinus, 1, 2) \
    F(BinaryMultiply, 1, 2) \
    F(BinaryDivision, 1, 2) \
    F(BinaryMod, 1, 2) \
    F(BinaryEqual, 1, 2) \
    F(BinaryLessThan, 1, 2) \
    F(BinaryLessThanOrEqual, 1, 2) \
    F(BinaryGreaterThan, 1, 2) \
    F(BinaryGreaterThanOrEqual, 1, 2) \
    F(BinaryNotEqual, 1, 2) \
    F(BinaryStrictEqual, 1, 2) \
    F(BinaryNotStrictEqual, 1, 2) \
    F(BinaryBitwiseAnd, 1, 2) \
    F(BinaryBitwiseOr, 1, 2) \
    F(BinaryBitwiseXor, 1, 2) \
    F(BinaryLeftShift, 1, 2) \
    F(BinarySignedRightShift, 1, 2) \
    F(BinaryUnsignedRightShift, 1, 2) \
    F(Move, 1, 0) \
    F(Increment, 1, 1) \
    F(Decrement, 1, 1) \
    F(Jump, 0, 0) \
    F(JumpComplexCase, 0, 0) \
    F(JumpIfFalse, 0, 0) \
    F(StoreExecutionResult, 0, 1) \
    F(CallFunction, 1, 0) \
    F(CallFunctionWithReceiver, 1, 0) \
    F(ReturnFunction, 0, 0) \
    F(ThrowOperation, 0, 0) \
    F(CallNativeFunction, 0, 0) \
    F(End, 0, 0) \

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
    case name##Opcode: \
        return #name;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_NAME)
#undef  RETURN_BYTECODE_NAME
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
#endif

#ifndef NDEBUG
inline const char* getByteCodeNameFromAddress(void* opcodeInAddress)
{
    for (size_t i = 0; i < OpcodeKindEnd; i ++) {
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
    case name##Opcode: \
        return pushCount;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_CNT)
#undef  RETURN_BYTECODE_CNT
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline size_t getByteCodePopCount(Opcode code)
{
    switch (code) {
#define RETURN_BYTECODE_CNT(name, pushCount, popCount) \
    case name##Opcode: \
        return popCount;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_CNT)
#undef  RETURN_BYTECODE_CNT
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

    ByteCodeLOC(size_t index)
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
    SmallValue m_value;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("load r%d <- ", (int)m_registerIndex);
        Value v = m_value;
        if (v.isNumber()) {
            printf("%lf", v.asNumber());
        } else if (v.isString()) {
            printf("%s", v.asString()->toUTF8StringData().data());
        } else {
            printf("some value(?)");
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

#ifdef NDEBUG
#define DEFINE_BINARY_OPERATION_DUMP(name)
#else
#define DEFINE_BINARY_OPERATION_DUMP(name) \
    virtual void dump() \
    { \
        printf(name" r%d <- r%d + r%d", (int)m_srcIndex0, (int)m_srcIndex0, (int)m_srcIndex1); \
    }
#endif

#define DEFINE_BINARY_OPERATION(CodeName, HumanName) \
class Binary##CodeName : public ByteCode { \
public: \
    Binary##CodeName(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1) \
        : ByteCode(Opcode::Binary##CodeName##Opcode, loc)  \
        , m_srcIndex0(registerIndex0)  \
        , m_srcIndex1(registerIndex1)  \
    {  \
    }  \
    size_t m_srcIndex0; \
    size_t m_srcIndex1; \
    DEFINE_BINARY_OPERATION_DUMP(HumanName) \
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

class Move : public ByteCode {
public:
    Move(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1)
        : ByteCode(Opcode::MoveOpcode, loc)
        , m_registerIndex0(registerIndex0)
        , m_registerIndex1(registerIndex1)
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

class JumpComplexCase : public ByteCode {
public:
    JumpComplexCase(Jump* j)
        : ByteCode(Opcode::JumpComplexCaseOpcode, j->m_loc)
        , m_jumpPosition(j->m_jumpPosition)
    {
        // TODO
    }

    size_t m_jumpPosition;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("jump complex %d", (int)m_jumpPosition);
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

class StoreExecutionResult : public ByteCode {
public:
    StoreExecutionResult(const ByteCodeLOC& loc, const size_t& registerIndex)
        : ByteCode(Opcode::StoreExecutionResultOpcode, loc)
        , m_registerIndex(registerIndex)
    {
    }
    size_t m_registerIndex;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("store ExecutionResult <- r%d", (int)m_registerIndex);
    }
#endif
};

class CallFunction : public ByteCode {
public:
    // register usage (before call)
    // [callee, arg0, arg1,... arg<argument count-1> ]
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
        printf("call r%d", (int)m_registerIndex);
    }
#endif
};

class CallFunctionWithReceiver : public ByteCode {
public:
    // register usage (before call)
    // [callee, receiver, arg0, arg1,... arg<argument count-1> ]
    // register usage (after call)
    // [return value]
    CallFunctionWithReceiver(const ByteCodeLOC& loc, const size_t& registerIndex, const size_t& argumentCount)
        : ByteCode(Opcode::CallFunctionWithReceiverOpcode, loc)
        , m_registerIndex(registerIndex)
        , m_argumentCount(argumentCount)
    {
    }
    size_t m_registerIndex;
    size_t m_argumentCount;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("call(with receiver) r%d", (int)m_registerIndex);
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


typedef std::vector<char, gc_malloc_ignore_off_page_allocator<char>> ByteCodeBlockData;
class ByteCodeBlock : public gc {
public:
    ByteCodeBlock()
    {
        m_requiredRegisterFileSizeInValueSize = 0;
    }

    template <typename CodeType>
    void pushCode(const CodeType& code, ByteCodeGenerateContext* context, Node* node)
    {
    #ifndef NDEBUG
        {
            CodeType& t = const_cast<CodeType &>(code);
            t.m_node = node;
            t.m_loc.line = computeNodeFromByteCode(&t, context->m_codeBlock).line;
            t.m_loc.column = computeNodeFromByteCode(&t, context->m_codeBlock).column;
        }
    #endif

        const_cast<CodeType &>(code).assignOpcodeInAddress();
        char* first = (char *)&code;
        m_code.insert(m_code.end(), first, first + sizeof(CodeType));

        m_requiredRegisterFileSizeInValueSize = std::max(m_requiredRegisterFileSizeInValueSize, (size_t)context->m_baseRegisterCount);
    }
    template <typename CodeType>
    CodeType* peekCode(size_t position)
    {
        char* pos = m_code.data();
        pos = &pos[position];
        return (CodeType *)pos;
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

    NodeLOC computeNodeFromByteCode(ByteCode* code, CodeBlock* cb);

    ByteCodeBlockData m_code;
    size_t m_requiredRegisterFileSizeInValueSize;
};

}

#endif
