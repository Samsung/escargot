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
#ifndef NDEBUG
// Represent start code position which is used for bytecode dump
static MAY_THREAD_LOCAL size_t g_dumpCodeStartPosition;
#endif

#ifndef NDEBUG
void ByteCode::dumpCode(const uint8_t* byteCodeStart, const size_t endPos)
{
    // set the start code position
    g_dumpCodeStartPosition = (size_t)byteCodeStart;

    size_t curPos = 0;
    while (curPos < endPos) {
        ByteCode* curCode = (ByteCode*)(byteCodeStart + curPos);

        printf("%zu\t\t", curPos);

        const char* opcodeName = nullptr;
        switch (curCode->m_orgOpcode) {
#define RETURN_BYTECODE_NAME(name)           \
    case name##Opcode:                       \
        static_cast<name*>(curCode)->dump(); \
        opcodeName = #name;                  \
        break;
            FOR_EACH_BYTECODE(RETURN_BYTECODE_NAME)
#undef RETURN_BYTECODE_NAME
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        printf(" | %s ", opcodeName);
        printf("(line: %zu:%zu)\n", curCode->m_loc.line, curCode->m_loc.column);

        if (curCode->m_orgOpcode == ExecutionPauseOpcode) {
            if (static_cast<ExecutionPause*>(curCode)->m_reason == ExecutionPause::Yield) {
                curPos += static_cast<ExecutionPause*>(curCode)->m_yieldData.m_tailDataLength;
            } else if (static_cast<ExecutionPause*>(curCode)->m_reason == ExecutionPause::Await) {
                curPos += static_cast<ExecutionPause*>(curCode)->m_awaitData.m_tailDataLength;
            } else if (static_cast<ExecutionPause*>(curCode)->m_reason == ExecutionPause::GeneratorsInitialize) {
                curPos += static_cast<ExecutionPause*>(curCode)->m_asyncGeneratorInitializeData.m_tailDataLength;
            } else {
                ASSERT_NOT_REACHED();
            }
        }

        curPos += byteCodeLengths[curCode->m_orgOpcode];
    }
}

size_t ByteCode::dumpJumpPosition(size_t pos)
{
    ASSERT(!!g_dumpCodeStartPosition && pos > g_dumpCodeStartPosition);
    return (pos - g_dumpCodeStartPosition);
}

void GetGlobalVariable::dump()
{
    printf("get global variable r%u <- global.%s", m_registerIndex, m_slot->m_propertyName.string()->toUTF8StringData().data());
}

void SetGlobalVariable::dump()
{
    printf("set global variable global.%s <- r%u", m_slot->m_propertyName.string()->toUTF8StringData().data(), m_registerIndex);
}
#endif

ByteCodeBlock::ByteCodeBlock()
    : m_shouldClearStack(false)
    , m_isOwnerMayFreed(false)
    , m_needsExtendedExecutionState(false)
    , m_requiredOperandRegisterNumber(2)
    , m_requiredTotalRegisterNumber(0)
    , m_inlineCacheDataSize(0)
    , m_codeBlock(nullptr)
{
    // This constructor is used to allocate a ByteCodeBlock on the stack
}

static void clearByteCodeBlock(ByteCodeBlock* self)
{
#ifdef ESCARGOT_DEBUGGER
    Debugger* debugger = self->codeBlock()->context()->debugger();
    if (debugger != nullptr && self->codeBlock()->markDebugging()) {
        debugger->byteCodeReleaseNotification(self);
    }
#endif
    self->m_code.clear();
    self->m_numeralLiteralData.clear();
    self->m_jumpFlowRecordData.clear();

    if (!self->m_isOwnerMayFreed) {
        auto& v = self->m_codeBlock->context()->vmInstance()->compiledByteCodeBlocks();
        v.erase(std::find(v.begin(), v.end(), self));
    }
}

ByteCodeBlock::ByteCodeBlock(InterpretedCodeBlock* codeBlock)
    : m_shouldClearStack(false)
    , m_isOwnerMayFreed(false)
    , m_needsExtendedExecutionState(false)
    , m_requiredOperandRegisterNumber(2)
    , m_requiredTotalRegisterNumber(0)
    , m_inlineCacheDataSize(0)
    , m_codeBlock(codeBlock)
{
    auto& v = m_codeBlock->context()->vmInstance()->compiledByteCodeBlocks();
    v.push_back(this);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        ByteCodeBlock* self = (ByteCodeBlock*)obj;
        clearByteCodeBlock(self);
    },
                                   nullptr, nullptr, nullptr);
}

void* ByteCodeBlock::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
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

    size_t index = SIZE_MAX;
    for (size_t i = 0; i < locData->size(); i++) {
        if ((*locData)[i].first == codePosition) {
            index = (*locData)[i].second;
            if (index == SIZE_MAX) {
                return ExtendedNodeLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            }
            break;
        }
    }

    size_t indexRelatedWithScript = 0;
    if (index == SIZE_MAX) {
        indexRelatedWithScript = cb->functionStart().index;
    } else {
        ASSERT(index >= cb->functionStart().index);
        indexRelatedWithScript = index;
    }
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

    // subtract `originSourceLineOffset` for line offset
    ASSERT(line > codeBlock()->script()->originSourceLineOffset());
    line -= codeBlock()->script()->originSourceLineOffset();

    return ExtendedNodeLOC(line, column, index);
}

void ByteCodeBlock::initFunctionDeclarationWithinBlock(ByteCodeGenerateContext* context, void* bi, Node* node)
{
    InterpretedCodeBlock* codeBlock = context->m_codeBlock;
    if (!codeBlock->hasChildren()) {
        return;
    }

    ASSERT(!!bi);
    InterpretedCodeBlock::BlockInfo* blockInfo = reinterpret_cast<InterpretedCodeBlock::BlockInfo*>(bi);
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
            pushCode(CreateFunction(ByteCodeLOC(node->m_loc.index), dstIndex, SIZE_MAX, child), context, node->m_loc.index);

            // We would not use InitializeByName where `global + eval`
            // ex) eval("delete f; function f() {}")
            if (codeBlock->isStrict() || !codeBlock->isEvalCode() || codeBlock->isEvalCodeInFunction()) {
                context->m_isFunctionDeclarationBindingInitialization = true;
            }

            for (size_t i = 0; i < blockInfo->identifiers().size(); i++) {
                if (blockInfo->identifiers()[i].m_name == child->functionName()) {
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

ByteCodeBlock::ByteCodeLexicalBlockContext ByteCodeBlock::pushLexicalBlock(ByteCodeGenerateContext* context, void* bi, Node* node, bool initFunctionDeclarationInside)
{
    ASSERT(!!bi);
    InterpretedCodeBlock::BlockInfo* blockInfo = reinterpret_cast<InterpretedCodeBlock::BlockInfo*>(bi);

    ByteCodeBlock::ByteCodeLexicalBlockContext ctx;
    ctx.lexicallyDeclaredNamesCount = context->m_lexicallyDeclaredNames->size();

    if (blockInfo->shouldAllocateEnvironment()) {
        ctx.lexicalBlockSetupStartPosition = currentCodeSize();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::Block, ctx.lexicalBlockSetupStartPosition));
        this->pushCode(BlockOperation(ByteCodeLOC(node->m_loc.index), blockInfo), context, SIZE_MAX);
        context->m_needsExtendedExecutionState = true;
    }

    if (initFunctionDeclarationInside) {
        initFunctionDeclarationWithinBlock(context, blockInfo, node);
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

    this->pushCode(CloseLexicalEnvironment(ByteCodeLOC(SIZE_MAX)), context, SIZE_MAX);
    this->peekCode<BlockOperation>(ctx.lexicalBlockSetupStartPosition)->m_blockEndPosition = this->currentCodeSize();
    context->m_recursiveStatementStack.pop_back();
}

void ByteCodeBlock::pushPauseStatementExtraData(ByteCodeGenerateContext* context)
{
    if (context->m_recursiveStatementStack.size()) {
        size_t startSize = m_code.size();
        size_t tailDataLength = context->m_recursiveStatementStack.size() * (sizeof(ByteCodeGenerateContext::RecursiveStatementKind) + sizeof(size_t));
        m_code.resizeFitWithUninitializedValues(startSize + tailDataLength);

        auto* codeAddr = m_code.data();
        auto iter = context->m_recursiveStatementStack.begin();
        size_t pos = startSize;
        while (iter != context->m_recursiveStatementStack.end()) {
            new (codeAddr + pos) size_t(iter->first);
            pos += sizeof(ByteCodeGenerateContext::RecursiveStatementKind);
            new (codeAddr + pos) size_t(iter->second);
            pos += sizeof(size_t);
            iter++;
        }

        ASSERT(tailDataLength == (pos - startSize));
    }
}

void* GetObjectInlineCacheSimpleCaseData::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GetObjectInlineCacheSimpleCaseData)] = { 0 };
        for (size_t i = 0; i < inlineBufferSize; i++) {
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCacheSimpleCaseData, m_cachedStructures) + i);
        }
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCacheSimpleCaseData, m_propertyName));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GetObjectInlineCacheSimpleCaseData));
        typeInited = true;
    }

    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* GetObjectInlineCacheComplexCaseData::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GetObjectInlineCacheComplexCaseData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCacheComplexCaseData, m_cache));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GetObjectInlineCacheComplexCaseData));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* SetObjectInlineCache::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObjectInlineCache)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObjectInlineCache, m_cache));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObjectInlineCache));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
} // namespace Escargot
