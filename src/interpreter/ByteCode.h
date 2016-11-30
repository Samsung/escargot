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
    F(DeclareVarVariable, 0, 0) \
    F(DeclareFunctionDeclaration, 1, 0) \
    F(DeclareFunctionExpression, 1, 0) \
    F(BinaryPlus, 1, 2) \
    F(StoreExecutionResult, 1, 0) \
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
    size_t line;
    size_t column;
    size_t index;

    ByteCodeLOC(size_t line, size_t column, size_t index)
    {
        this->line = line;
        this->column = column;
        this->index = index;
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
#endif
    {
    }

    void assignOpcodeInAddress()
    {
        m_opcodeInAddress = g_opcodeTable.m_table[(Opcode)(size_t)m_opcodeInAddress];
    }

    void* m_opcodeInAddress;
    ByteCodeLOC m_loc;
#ifndef NDEBUG
    Node* m_node;
    void dumpCode()
    {
        printf("%s ", getByteCodeNameFromAddress(m_opcodeInAddress));
        dump();
        printf(" (line: %d:%d)\n", (int)m_loc.line, (int)m_loc.column);
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
        : ByteCode(Opcode::DeclareFunctionDeclarationOpcode, ByteCodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX))
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

class BinaryPlus : public ByteCode {
public:
    BinaryPlus(const ByteCodeLOC& loc, const size_t& registerIndex0, const size_t& registerIndex1)
        : ByteCode(Opcode::BinaryPlusOpcode, loc)
        , m_srcIndex0(registerIndex0)
        , m_srcIndex1(registerIndex1)
    {
    }
    size_t m_srcIndex0; // src0 == dst0
    size_t m_srcIndex1;

#ifndef NDEBUG
    virtual void dump()
    {
        printf("add r%d <- r%d + r%d", (int)m_srcIndex0, (int)m_srcIndex0, (int)m_srcIndex1);
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

class End : public ByteCode {
public:
    End(const ByteCodeLOC& loc)
        : ByteCode(Opcode::EndOpcode, loc)
    {
    }
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
        }
    #endif

        const_cast<CodeType &>(code).assignOpcodeInAddress();
        char* first = (char *)&code;
        m_code.insert(m_code.end(), first, first + sizeof(CodeType));

        m_requiredRegisterFileSizeInValueSize = std::max(m_requiredRegisterFileSizeInValueSize, (size_t)context->m_baseRegisterCount);
    }
    ByteCodeBlockData m_code;
    size_t m_requiredRegisterFileSizeInValueSize;
};

}

#endif
