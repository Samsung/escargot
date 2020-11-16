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

#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "parser/Lexer.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "parser/esprima_cpp/esprima.h"

namespace Escargot {

OpcodeTable g_opcodeTable;

OpcodeTable::OpcodeTable()
{
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    // Dummy bytecode execution to initialize the OpcodeTable.
    ByteCodeInterpreter::interpret(nullptr, nullptr, 0, nullptr);
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

void GetGlobalVariable::dump(const char* byteCodeStart)
{
    printf("get global variable r%d <- global.%s", (int)m_registerIndex, m_slot->m_propertyName.string()->toUTF8StringData().data());
}

void SetGlobalVariable::dump(const char* byteCodeStart)
{
    printf("set global variable global.%s <- r%d", m_slot->m_propertyName.string()->toUTF8StringData().data(), (int)m_registerIndex);
}
#endif

ByteCodeBlock::ByteCodeBlock()
    : m_shouldClearStack(false)
    , m_isOwnerMayFreed(false)
    , m_requiredRegisterFileSizeInValueSize(2)
    , m_inlineCacheDataSize(0)
    , m_codeBlock(nullptr)
{
    // This constructor is used to allocate a ByteCodeBlock on the stack
}

ByteCodeBlock::ByteCodeBlock(InterpretedCodeBlock* codeBlock)
    : m_shouldClearStack(false)
    , m_isOwnerMayFreed(false)
    , m_requiredRegisterFileSizeInValueSize(2)
    , m_inlineCacheDataSize(0)
    , m_codeBlock(codeBlock)
{
    auto& v = m_codeBlock->context()->vmInstance()->compiledByteCodeBlocks();
    v.push_back(this);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        ByteCodeBlock* self = (ByteCodeBlock*)obj;

#ifdef ESCARGOT_DEBUGGER
        Debugger* debugger = self->m_codeBlock->context()->debugger();
        if (debugger && debugger->enabled()) {
            debugger->releaseFunction(self->m_code.data());
        }
#endif /* ESCARGOT_DEBUGGER */

        self->m_code.clear();
        self->m_numeralLiteralData.clear();

        if (!self->m_isOwnerMayFreed) {
            auto& v = self->m_codeBlock->context()->vmInstance()->compiledByteCodeBlocks();
            v.erase(std::find(v.begin(), v.end(), self));
        }
    },
                                   nullptr, nullptr, nullptr);
}

void* ByteCodeBlock::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ByteCodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_stringLiteralData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_otherLiteralData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ByteCodeBlock, m_codeBlock));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ByteCodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void ByteCodeBlock::fillLOCData(Context* context, ByteCodeLOCData* locData)
{
    ASSERT(!!locData && locData->size() == 0);

    if (m_codeBlock->src().length() == 0) {
        // prevent infinate fillLOCData if m_locData.size() == 0 in here
        locData->push_back(std::make_pair(SIZE_MAX, SIZE_MAX));
        return;
    }

    ByteCodeGenerator::collectByteCodeLOCData(context, m_codeBlock, locData);

    // prevent infinate fillLOCData if m_locData.size() == 0 in here
    locData->push_back(std::make_pair(SIZE_MAX, SIZE_MAX));
}

ExtendedNodeLOC ByteCodeBlock::computeNodeLOCFromByteCode(Context* c, size_t codePosition, InterpretedCodeBlock* cb, ByteCodeLOCData* locData)
{
    ASSERT(!!locData);

    if (codePosition == SIZE_MAX) {
        return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
    }

    if (!locData->size()) {
        fillLOCData(c, locData);
    }

    size_t index = 0;
    for (size_t i = 0; i < locData->size(); i++) {
        if ((*locData)[i].first == codePosition) {
            index = (*locData)[i].second;
            if (index == SIZE_MAX) {
                return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            }
            break;
        }
    }

    size_t indexRelatedWithScript = index;
    index -= cb->functionStart().index;

    auto result = computeNodeLOC(cb->src(), cb->functionStart(), index);
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

void ByteCodeBlock::initFunctionDeclarationWithinBlock(ByteCodeGenerateContext* context, InterpretedCodeBlock::BlockInfo* bi, Node* node)
{
    InterpretedCodeBlock* codeBlock = context->m_codeBlock;
    if (!codeBlock->hasChildren()) {
        return;
    }

    InterpretedCodeBlockVector& childrenVector = codeBlock->children();
    for (size_t i = 0; i < childrenVector.size(); i++) {
        InterpretedCodeBlock* child = childrenVector[i];
        if (child->isFunctionDeclaration() && child->lexicalBlockIndexFunctionLocatedIn() == context->m_lexicalBlockIndex) {
            IdentifierNode* id = new (alloca(sizeof(IdentifierNode))) IdentifierNode(child->functionName());
            id->m_loc = node->m_loc;

            // add useless register for don't ruin script result
            // because script or eval result is final value of first register
            // eg) function foo() {} -> result should be `undefined` not `function foo`
            context->getRegister();

            auto dstIndex = id->getRegister(this, context);
            pushCode(CreateFunction(ByteCodeLOC(node->m_loc.index), dstIndex, SIZE_MAX, child), context, node);

            // We would not use InitializeByName where `global + eval`
            // ex) eval("delete f; function f() {}")
            if (codeBlock->isStrict() || !codeBlock->isEvalCode() || codeBlock->isEvalCodeInFunction()) {
                context->m_isFunctionDeclarationBindingInitialization = true;
            }

            for (size_t i = 0; i < bi->m_identifiers.size(); i++) {
                if (bi->m_identifiers[i].m_name == child->functionName()) {
                    context->m_isLexicallyDeclaredBindingInitialization = true;
                    break;
                }
            }

            id->generateStoreByteCode(this, context, dstIndex, true);
            context->giveUpRegister();

            context->giveUpRegister(); // give up useless register
        }
    }
}

ByteCodeBlock::ByteCodeLexicalBlockContext ByteCodeBlock::pushLexicalBlock(ByteCodeGenerateContext* context, InterpretedCodeBlock::BlockInfo* bi, Node* node, bool initFunctionDeclarationInside)
{
    ByteCodeBlock::ByteCodeLexicalBlockContext ctx;
    ctx.lexicallyDeclaredNamesCount = context->m_lexicallyDeclaredNames->size();

    if (bi->m_shouldAllocateEnvironment) {
        ctx.lexicalBlockSetupStartPosition = currentCodeSize();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Block, ctx.lexicalBlockSetupStartPosition));
        this->pushCode(BlockOperation(ByteCodeLOC(node->m_loc.index), bi), context, nullptr);
    }

    if (initFunctionDeclarationInside) {
        initFunctionDeclarationWithinBlock(context, bi, node);
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

    this->pushCode(TryCatchFinallyWithBlockBodyEnd(ByteCodeLOC(SIZE_MAX)), context, nullptr);
    this->peekCode<BlockOperation>(ctx.lexicalBlockSetupStartPosition)->m_blockEndPosition = this->currentCodeSize();
    context->m_recursiveStatementStack.pop_back();
}

void ByteCodeBlock::pushPauseStatementExtraData(ByteCodeGenerateContext* context)
{
    auto iter = context->m_recursiveStatementStack.begin();
    while (iter != context->m_recursiveStatementStack.end()) {
        size_t pos = m_code.size();
        m_code.resizeWithUninitializedValues(pos + sizeof(ByteCodeGenerateContext::RecursiveStatementKind));
        new (m_code.data() + pos) size_t(iter->first);
        pos = m_code.size();
        m_code.resizeWithUninitializedValues(pos + sizeof(size_t));
        new (m_code.data() + pos) size_t(iter->second);
        iter++;
    }
}

void* GetObjectInlineCache::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCache, m_cache));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* SetObjectInlineCache::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_cachedHiddenClassChainData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_hiddenClassWillBe));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
