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

#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Context.h"
#include "parser/Lexer.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "parser/esprima_cpp/esprima.h"

namespace Escargot {

OpcodeTable g_opcodeTable;

OpcodeTable::OpcodeTable()
{
#if defined(COMPILER_GCC)
    ExecutionState state((Context*)nullptr);
    // Dummy bytecode execution to initialize the OpcodeTable.
    ByteCodeInterpreter::interpret(state, nullptr, 0, nullptr);
#endif
}

#ifndef NDEBUG
void ByteCode::dumpCode(size_t pos, const char* byteCodeStart)
{
    printf("%d\t\t", (int)pos);

    const char* opcodeName = NULL;
    switch (m_orgOpcode) {
#define RETURN_BYTECODE_NAME(name, pushCount, popCount) \
    case name##Opcode:                                  \
        ((name*)this)->dump(byteCodeStart);             \
        opcodeName = #name;                             \
        break;
        FOR_EACH_BYTECODE_OP(RETURN_BYTECODE_NAME)
#undef RETURN_BYTECODE_NAME
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    printf(" | %s ", opcodeName);
    printf("(line: %d:%d)\n", (int)m_loc.line, (int)m_loc.column);
}

int ByteCode::dumpJumpPosition(size_t pos, const char* byteCodeStart)
{
    return (int)(pos - (size_t)byteCodeStart);
}
#endif

void* ByteCodeBlock::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ByteCodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_literalData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_codeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_objectStructuresInUse));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ByteCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void ByteCodeBlock::fillLocDataIfNeeded(Context* c)
{
    if (!m_codeBlock->isInterpretedCodeBlock() || m_locData || (m_codeBlock->isInterpretedCodeBlock() && m_codeBlock->asInterpretedCodeBlock()->src().length() == 0)) {
        return;
    }

    ByteCodeBlock* block;
    // TODO
    // give correct stack limit to parser
    if (m_codeBlock->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
        RefPtr<ProgramNode> nd = esprima::parseProgram(c, m_codeBlock->asInterpretedCodeBlock()->src(), m_codeBlock->asInterpretedCodeBlock()->isStrict(), m_codeBlock->inWith(), SIZE_MAX);
        block = ByteCodeGenerator::generateByteCode(c, m_codeBlock->asInterpretedCodeBlock(), nd.get(), nd->scopeContext(), m_isEvalMode, m_isOnGlobal, false, true);
    } else {
        ASTFunctionScopeContext* scopeContext = nullptr;
        auto body = esprima::parseSingleFunction(c, m_codeBlock->asInterpretedCodeBlock(), scopeContext, SIZE_MAX);
        block = ByteCodeGenerator::generateByteCode(c, m_codeBlock->asInterpretedCodeBlock(), body.get(), scopeContext, m_isEvalMode, m_isOnGlobal, false, true);
    }
    m_locData = block->m_locData;
    block->m_locData = nullptr;
    // prevent infinate fillLocDataIfNeeded if m_locData.size() == 0 in here
    m_locData->push_back(std::make_pair(SIZE_MAX, SIZE_MAX));
}

ExtendedNodeLOC ByteCodeBlock::computeNodeLOCFromByteCode(Context* c, size_t codePosition, CodeBlock* cb)
{
    if (codePosition == SIZE_MAX) {
        return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
    }

    fillLocDataIfNeeded(c);

    size_t index = 0;
    for (size_t i = 0; i < m_locData->size(); i++) {
        if ((*m_locData)[i].first == codePosition) {
            index = (*m_locData)[i].second;
            if (index == SIZE_MAX) {
                return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            }
            break;
        }
    }

    size_t indexRelatedWithScript = index;
    index -= cb->asInterpretedCodeBlock()->sourceElementStart().index;

    auto result = computeNodeLOC(cb->asInterpretedCodeBlock()->src(), cb->asInterpretedCodeBlock()->sourceElementStart(), index);
    result.index = indexRelatedWithScript;

    return result;
}

ExtendedNodeLOC ByteCodeBlock::computeNodeLOC(StringView src, ExtendedNodeLOC sourceElementStart, size_t index)
{
    size_t line = sourceElementStart.line;
    size_t column = sourceElementStart.column;
    size_t srcLength = src.length();
    for (size_t i = 0; i < index && i < srcLength; i++) {
        char16_t c = src.charAt(i);
        column++;
        if (EscargotLexer::isLineTerminator(c)) {
            // skip \r\n
            if (c == 13 && (i + 1 < index) && src.charAt(i + 1) == 10) {
                i++;
            }
            line++;
            column = 1;
        }
    }
    return ExtendedNodeLOC(line, column, index);
}

ByteCodeBlock::ByteCodeLexicalBlockContext ByteCodeBlock::pushLexicalBlock(ByteCodeGenerateContext* context, InterpretedCodeBlock::BlockInfo* bi, Node* node)
{
    ByteCodeBlock::ByteCodeLexicalBlockContext ctx;
    InterpretedCodeBlock* codeBlock = context->m_codeBlock->asInterpretedCodeBlock();

    ctx.lexicallyDeclaredNamesCount = context->m_lexicallyDeclaredNames->size();

    if (bi->m_shouldAllocateEnvironment) {
        context->m_tryStatementScopeCount++;
        ctx.lexicalBlockSetupStartPosition = currentCodeSize();
        this->pushCode(BlockOperation(ByteCodeLOC(node->m_loc.index), bi), context, nullptr);
    }

    size_t len = codeBlock->childBlocks().size();
    for (size_t i = 0; i < len; i++) {
        CodeBlock* b = codeBlock->childBlocks()[i];
        if (b->isFunctionDeclaration() && b->asInterpretedCodeBlock()->lexicalBlockIndexFunctionLocatedIn() == context->m_lexicalBlockIndex) {
            IdentifierNode* id = new (alloca(sizeof(IdentifierNode))) IdentifierNode(b->functionName());
            id->m_loc = node->m_loc;

            // add useless register for don't ruin script result
            // because script or eval result is final value of first register
            // eg) function foo() {} -> result should be `undefined` not `function foo`
            context->getRegister();

            auto dstIndex = id->getRegister(this, context);
            pushCode(CreateFunction(ByteCodeLOC(node->m_loc.index), dstIndex, b), context, node);
            context->m_isFunctionDeclarationBindingInitialization = true;
            id->generateStoreByteCode(this, context, dstIndex, false);
            context->giveUpRegister();

            context->giveUpRegister(); // give up useless register
        }
    }

    ctx.lexicalBlockStartPosition = currentCodeSize();

    return ctx;
}

void ByteCodeBlock::finalizeLexicalBlock(ByteCodeGenerateContext* context, const ByteCodeBlock::ByteCodeLexicalBlockContext& ctx)
{
    context->m_lexicallyDeclaredNames->resize(ctx.lexicallyDeclaredNamesCount);

    if (ctx.lexicalBlockSetupStartPosition == SIZE_MAX) {
        return;
    }

    if (ctx.lexicalBlockStartPosition != SIZE_MAX) {
        context->registerJumpPositionsToComplexCase(ctx.lexicalBlockStartPosition);
    }

    this->pushCode(TryCatchWithBodyEnd(ByteCodeLOC(SIZE_MAX)), context, nullptr);
    this->peekCode<BlockOperation>(ctx.lexicalBlockSetupStartPosition)->m_blockEndPosition = this->currentCodeSize();
    context->m_tryStatementScopeCount--;
}

void* SetObjectInlineCache::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_cachedhiddenClassChain));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_hiddenClassWillBe));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* EnumerateObjectData::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(EnumerateObjectData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_hiddenClassChain));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_object));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(EnumerateObjectData, m_keys));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(EnumerateObjectData));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
