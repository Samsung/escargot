#include "Escargot.h"
#include "ByteCodeGenerator.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/AST.h"

namespace Escargot {

void ByteCodeGenerateContext::consumeLabeledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl)
{
    for (size_t i = 0; i < m_labeledContinueStatmentPositions.size(); i++) {
        if (*m_labeledContinueStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labeledContinueStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labeledContinueStatmentPositions[i].second);
            m_labeledContinueStatmentPositions.erase(m_labeledContinueStatmentPositions.begin() + i);
            i = -1;
        }
    }
}

void ByteCodeGenerateContext::consumeBreakPositions(ByteCodeBlock* cb, size_t position)
{
    for (size_t i = 0; i < m_breakStatementPositions.size(); i++) {
        Jump* shouldBeJump = cb->peekCode<Jump>(m_breakStatementPositions[i]);
        ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
        shouldBeJump->m_jumpPosition = position;

        morphJumpPositionIntoComplexCase(cb, m_breakStatementPositions[i]);
    }
    m_breakStatementPositions.clear();
}

void ByteCodeGenerateContext::consumeLabeledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl)
{
    for (size_t i = 0; i < m_labeledBreakStatmentPositions.size(); i++) {
        if (*m_labeledBreakStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labeledBreakStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labeledBreakStatmentPositions[i].second);
            m_labeledBreakStatmentPositions.erase(m_labeledBreakStatmentPositions.begin() + i);
            i = -1;
        }
    }
}

void ByteCodeGenerateContext::consumeContinuePositions(ByteCodeBlock* cb, size_t position)
{
    for (size_t i = 0; i < m_continueStatementPositions.size(); i++) {
        Jump* shouldBeJump = cb->peekCode<Jump>(m_continueStatementPositions[i]);
        ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
        shouldBeJump->m_jumpPosition = position;

        morphJumpPositionIntoComplexCase(cb, m_continueStatementPositions[i]);
    }
    m_continueStatementPositions.clear();
}

void ByteCodeGenerateContext::morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos)
{
    auto iter = m_complexCaseStatementPositions.find(codePos);
    if (iter != m_complexCaseStatementPositions.end()) {
        JumpComplexCase j(cb->peekCode<Jump>(codePos), iter->second);
        j.m_jumpPosition = iter->second;
        j.assignOpcodeInAddress();
        memcpy(cb->m_code.data() + codePos, &j, sizeof(JumpComplexCase));
        m_complexCaseStatementPositions.erase(iter);
    }
}

void ByteCodeGenerator::generateByteCode(Context* c, CodeBlock* codeBlock, Node* ast)
{
    ByteCodeBlock* block = new ByteCodeBlock();

    // TODO
    // fill ParserContextInformation info
    ParserContextInformation info(false, true);

    ByteCodeGenerateContext ctx(codeBlock, block, info);

    // generate init function decls first
    size_t len = codeBlock->childBlocks().size();
    for (size_t i = 0; i < len; i++) {
        CodeBlock* b = codeBlock->childBlocks()[i];
        if (b->isFunctionDeclaration()) {
            ctx.getRegister();
            block->pushCode(DeclareFunctionDeclaration(b), &ctx, nullptr);
            IdentifierNode idNode(b->m_functionName);
            idNode.generateStoreByteCode(block, &ctx);
            ctx.giveUpRegister();
        }
    }

    // generate common codes
    ast->generateStatementByteCode(block, &ctx);
    if (codeBlock->isGlobalScopeCodeBlock())
        block->pushCode(End(ByteCodeLOC(SIZE_MAX)), &ctx, nullptr);
    else
        block->pushCode(ReturnFunction(ByteCodeLOC(SIZE_MAX), SIZE_MAX), &ctx, nullptr);

#ifndef NDEBUG
    if (getenv("DUMP_BYTECODE") && strlen(getenv("DUMP_BYTECODE"))) {
        printf("dumpBytecode %s (%d:%d)>>>>>>>>>>>>>>>>>>>>>>\n", codeBlock->m_functionName.string()->toUTF8StringData().data(), (int)ast->loc().line, (int)ast->loc().column);
        size_t idx = 0;
        size_t bytecodeCounter = 0;
        char* code = block->m_code.data();
        char* end = &block->m_code.data()[block->m_code.size()];
        while (&code[idx] < end) {
            ByteCode* currentCode = (ByteCode*)(&code[idx]);

            Opcode opcode = EndOpcode;
            for (size_t i = 0; i < OpcodeKindEnd; i++) {
                if (g_opcodeTable.m_reverseTable[i].first == currentCode->m_opcodeInAddress) {
                    opcode = g_opcodeTable.m_reverseTable[i].second;
                    break;
                }
            }

            switch (opcode) {
#define DUMP_BYTE_CODE(code, pushCount, popCount) \
    case code##Opcode:                            \
        currentCode->dumpCode(idx);               \
        idx += sizeof(code);                      \
        continue;

                FOR_EACH_BYTECODE_OP(DUMP_BYTE_CODE)

#undef DUMP_BYTE_CODE
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            };
        }
        printf("dumpBytecode...<<<<<<<<<<<<<<<<<<<<<<\n");
    }
#endif

    codeBlock->m_byteCodeBlock = block;
}
}
