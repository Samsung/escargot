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
#include "ByteCodeGenerator.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/AST.h"

namespace Escargot {

void ByteCodeGenerateContext::consumeLabeledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount)
{
    for (size_t i = 0; i < m_labeledContinueStatmentPositions.size(); i++) {
        if (*m_labeledContinueStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labeledContinueStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labeledContinueStatmentPositions[i].second, outerLimitCount);
            m_labeledContinueStatmentPositions.erase(m_labeledContinueStatmentPositions.begin() + i);
            i = -1;
        }
    }
}

void ByteCodeGenerateContext::consumeBreakPositions(ByteCodeBlock* cb, size_t position, int outerLimitCount)
{
    for (size_t i = 0; i < m_breakStatementPositions.size(); i++) {
        Jump* shouldBeJump = cb->peekCode<Jump>(m_breakStatementPositions[i]);
        ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
        shouldBeJump->m_jumpPosition = position;

        morphJumpPositionIntoComplexCase(cb, m_breakStatementPositions[i], outerLimitCount);
    }
    m_breakStatementPositions.clear();
}

void ByteCodeGenerateContext::consumeLabeledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount)
{
    for (size_t i = 0; i < m_labeledBreakStatmentPositions.size(); i++) {
        if (*m_labeledBreakStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labeledBreakStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labeledBreakStatmentPositions[i].second, outerLimitCount);
            m_labeledBreakStatmentPositions.erase(m_labeledBreakStatmentPositions.begin() + i);
            i = -1;
        }
    }
}

void ByteCodeGenerateContext::consumeContinuePositions(ByteCodeBlock* cb, size_t position, int outerLimitCount)
{
    for (size_t i = 0; i < m_continueStatementPositions.size(); i++) {
        Jump* shouldBeJump = cb->peekCode<Jump>(m_continueStatementPositions[i]);
        ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
        shouldBeJump->m_jumpPosition = position;

        morphJumpPositionIntoComplexCase(cb, m_continueStatementPositions[i], outerLimitCount);
    }
    m_continueStatementPositions.clear();
}

void ByteCodeGenerateContext::morphJumpPositionIntoComplexCase(ByteCodeBlock* cb, size_t codePos, size_t outerLimitCount)
{
    auto iter = m_complexCaseStatementPositions.find(codePos);
    if (iter != m_complexCaseStatementPositions.end()) {
        ControlFlowRecord* r = new ControlFlowRecord(ControlFlowRecord::ControlFlowReason::NeedsJump, (cb->peekCode<Jump>(codePos)->m_jumpPosition), iter->second, outerLimitCount);
        m_byteCodeBlock->m_literalData.pushBack(r);
        JumpComplexCase j(cb->peekCode<Jump>(codePos), r);
        memcpy(cb->m_code.data() + codePos, &j, sizeof(JumpComplexCase));
        m_complexCaseStatementPositions.erase(iter);
    }
}

ALWAYS_INLINE void assignStackIndexIfNeeded(ByteCodeRegisterIndex& registerIndex, ByteCodeRegisterIndex stackBase, ByteCodeRegisterIndex stackBaseWillBe, ByteCodeRegisterIndex stackVariableSize)
{
    if (UNLIKELY(registerIndex == std::numeric_limits<ByteCodeRegisterIndex>::max())) {
        return;
    }
    if (registerIndex >= stackBase) {
        if (registerIndex >= (stackBase + VARIABLE_LIMIT)) {
            registerIndex = stackBaseWillBe + (registerIndex - (stackBase + VARIABLE_LIMIT)) + stackVariableSize;
        } else {
            registerIndex = stackBaseWillBe + (registerIndex - stackBase);
        }
    }
}

void ByteCodeGenerator::generateStoreThisValueByteCode(ByteCodeBlock* block, ByteCodeGenerateContext* context)
{
    InterpretedCodeBlock* codeBlock = block->m_codeBlock;
    InterpretedCodeBlock::IndexedIdentifierInfo info = codeBlock->indexedIdentifierInfo(codeBlock->context()->staticStrings().stringThis);
    ASSERT(!codeBlock->isGlobalScopeCodeBlock());

    if (codeBlock->canUseIndexedVariableStorage()) {
        if (context->m_isWithScope) {
            block->pushCode(StoreByName(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, codeBlock->context()->staticStrings().stringThis), context, nullptr);
        } else {
            ASSERT(info.m_isResultSaved && !info.m_upperIndex && !context->m_catchScopeCount);
            block->pushCode(StoreByHeapIndex(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, 0, info.m_index), context, nullptr);
        }
    } else {
        block->pushCode(StoreByName(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, codeBlock->context()->staticStrings().stringThis), context, nullptr);
    }
}

void ByteCodeGenerator::generateLoadThisValueByteCode(ByteCodeBlock* block, ByteCodeGenerateContext* context)
{
    InterpretedCodeBlock* codeBlock = block->m_codeBlock;
    InterpretedCodeBlock::IndexedIdentifierInfo info = codeBlock->upperIndexedIdentifierInfo(codeBlock->context()->staticStrings().stringThis);
    ASSERT(codeBlock->isArrowFunctionExpression());

    if (codeBlock->canUseIndexedVariableStorage()) {
        if (context->m_isWithScope || !info.m_isResultSaved) {
            block->pushCode(LoadByName(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, codeBlock->context()->staticStrings().stringThis), context, nullptr);
        } else {
            ASSERT(info.m_isResultSaved && info.m_upperIndex == 1 && !context->m_catchScopeCount);
            block->pushCode(LoadByHeapIndex(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, info.m_upperIndex, info.m_index), context, nullptr);
        }
    } else {
        block->pushCode(LoadByName(ByteCodeLOC(SIZE_MAX), REGULAR_REGISTER_LIMIT, codeBlock->context()->staticStrings().stringThis), context, nullptr);
    }
}

static const uint8_t byteCodeLengths[] = {
#define ITER_BYTE_CODE(code, pushCount, popCount) \
    (uint8_t)sizeof(code),

    FOR_EACH_BYTECODE_OP(ITER_BYTE_CODE)
#undef ITER_BYTE_CODE
};

ByteCodeBlock* ByteCodeGenerator::generateByteCode(Context* c, InterpretedCodeBlock* codeBlock, Node* ast, ASTScopeContext* scopeCtx, bool isEvalMode, bool isOnGlobal, bool shouldGenerateLOCData)
{
    ByteCodeBlock* block = new ByteCodeBlock(codeBlock);
    block->m_isEvalMode = isEvalMode;
    block->m_isOnGlobal = isOnGlobal;

    bool isGlobalScope;
    if (!isEvalMode) {
        isGlobalScope = codeBlock->isGlobalScopeCodeBlock();
    } else {
        isGlobalScope = isOnGlobal;
    }
    ParserContextInformation info(isEvalMode, isGlobalScope, codeBlock->isStrict(), codeBlock->isInWithScope());

    Vector<Value, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<Value>>* nData = &scopeCtx->m_numeralLiteralData;

    if (nData->size() == 0) {
        nData = nullptr;
    }

    ByteCodeGenerateContext ctx(codeBlock, block, info, nData);
    ctx.m_shouldGenerateLOCData = shouldGenerateLOCData;
    if (shouldGenerateLOCData) {
        block->m_locData = new ByteCodeLOCData();
    }

    // load/store this value first
    if (codeBlock->needToLoadThisValue()) {
        generateLoadThisValueByteCode(block, &ctx);
    }

    if (codeBlock->needToStoreThisValue()) {
        generateStoreThisValueByteCode(block, &ctx);
    }

    // generate init function decls
    size_t len = codeBlock->childBlocks().size();
    for (size_t i = 0; i < len; i++) {
        CodeBlock* b = codeBlock->childBlocks()[i];
        if (b->isFunctionDeclaration()) {
            block->pushCode(DeclareFunctionDeclarations(codeBlock), &ctx, nullptr);
            break;
        }
    }

    // generate common codes
    try {
        ast->generateStatementByteCode(block, &ctx);
        if (!isGlobalScope && !isEvalMode) {
            ASSERT(ast->type() == ASTNodeType::BlockStatement);
            BlockStatementNode* blk = (BlockStatementNode*)ast;
            StatementNode* nd = blk->firstChild();
            StatementNode* last = nullptr;
            while (nd) {
                last = nd;
                nd = nd->nextSilbing();
            }
            if (!(last && last->type() == ASTNodeType::ReturnStatement)) {
                block->pushCode(ReturnFunction(ByteCodeLOC(SIZE_MAX)), &ctx, nullptr);
            }
        }
    } catch (const ByteCodeGenerateError& err) {
        block->m_code.clear();
        char* data = (char*)GC_MALLOC_ATOMIC(err.m_message.size());
        memcpy(data, err.m_message.data(), err.m_message.size());
        data[err.m_message.size()] = 0;
        block->m_literalData.pushBack(data);
        ThrowStaticErrorOperation code(ByteCodeLOC(err.m_index), ErrorObject::SyntaxError, data);
        block->m_code.resize(sizeof(ThrowStaticErrorOperation));
        memcpy(block->m_code.data(), &code, sizeof(ThrowStaticErrorOperation));
        if (block->m_locData) {
            delete block->m_locData;
        }
        block->m_locData = new ByteCodeLOCData();
        block->m_locData->push_back(std::make_pair(0, err.m_index));
    } catch (const char* err) {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (ctx.m_keepNumberalLiteralsInRegisterFile) {
        block->m_numeralLiteralData.resizeWithUninitializedValues(nData->size());
        memcpy(block->m_numeralLiteralData.data(), nData->data(), sizeof(Value) * nData->size());
    }

    block->m_code.shrinkToFit();

    block->m_getObjectCodePositions = std::move(ctx.m_getObjectCodePositions);

    {
        ByteCodeRegisterIndex stackBase = REGULAR_REGISTER_LIMIT;
        ByteCodeRegisterIndex stackBaseWillBe = block->m_requiredRegisterFileSizeInValueSize;
        ByteCodeRegisterIndex stackVariableSize = codeBlock->identifierOnStackCount();

        char* code = block->m_code.data();
        size_t codeBase = (size_t)code;
        char* end = code + block->m_code.size();

        while (code < end) {
            ByteCode* currentCode = (ByteCode*)code;
#if defined(COMPILER_GCC)
            Opcode opcode = (Opcode)(size_t)currentCode->m_opcodeInAddress;
#else
            Opcode opcode = currentCode->m_opcode;
#endif
            currentCode->assignOpcodeInAddress();

            switch (opcode) {
            case LoadLiteralOpcode: {
                LoadLiteral* cd = (LoadLiteral*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadRegexpOpcode: {
                LoadRegexp* cd = (LoadRegexp*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadByNameOpcode: {
                LoadByName* cd = (LoadByName*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreByNameOpcode: {
                StoreByName* cd = (StoreByName*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadByHeapIndexOpcode: {
                LoadByHeapIndex* cd = (LoadByHeapIndex*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreByHeapIndexOpcode: {
                StoreByHeapIndex* cd = (StoreByHeapIndex*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateFunctionOpcode: {
                CreateFunction* cd = (CreateFunction*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateObjectOpcode: {
                CreateObject* cd = (CreateObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateArrayOpcode: {
                CreateArray* cd = (CreateArray*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetObjectOpcode: {
                GetObject* cd = (GetObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_storeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetObjectOperationOpcode: {
                SetObjectOperation* cd = (SetObjectOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineOwnPropertyOperationOpcode: {
                ObjectDefineOwnPropertyOperation* cd = (ObjectDefineOwnPropertyOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineOwnPropertyWithNameOperationOpcode: {
                ObjectDefineOwnPropertyWithNameOperation* cd = (ObjectDefineOwnPropertyWithNameOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ArrayDefineOwnPropertyOperationOpcode: {
                ArrayDefineOwnPropertyOperation* cd = (ArrayDefineOwnPropertyOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                for (size_t i = 0; i < cd->m_count; i++)
                    assignStackIndexIfNeeded(cd->m_loadRegisterIndexs[i], stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetObjectPreComputedCaseOpcode: {
                GetObjectPreComputedCase* cd = (GetObjectPreComputedCase*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_storeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetObjectPreComputedCaseOpcode: {
                SetObjectPreComputedCase* cd = (SetObjectPreComputedCase*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ReturnFunctionWithValueOpcode: {
                ReturnFunctionWithValue* cd = (ReturnFunctionWithValue*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ReturnFunctionSlowCaseOpcode: {
                ReturnFunctionSlowCase* cd = (ReturnFunctionSlowCase*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case MoveOpcode: {
                Move* cd = (Move*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_registerIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineGetterOpcode: {
                ObjectDefineGetter* cd = (ObjectDefineGetter*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_objectPropertyNameRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_objectPropertyValueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineSetterOpcode: {
                ObjectDefineSetter* cd = (ObjectDefineSetter*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_objectPropertyNameRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_objectPropertyValueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetGlobalObjectOpcode: {
                GetGlobalObject* cd = (GetGlobalObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetGlobalObjectOpcode: {
                SetGlobalObject* cd = (SetGlobalObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ToNumberOpcode:
            case IncrementOpcode:
            case DecrementOpcode:
            case UnaryMinusOpcode:
            case UnaryNotOpcode:
            case UnaryBitwiseNotOpcode: {
                ToNumber* cd = (ToNumber*)currentCode;
                assignStackIndexIfNeeded(cd->m_srcIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case UnaryTypeofOpcode: {
                UnaryTypeof* cd = (UnaryTypeof*)currentCode;
                assignStackIndexIfNeeded(cd->m_srcIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case UnaryDeleteOpcode: {
                UnaryDelete* cd = (UnaryDelete*)currentCode;
                assignStackIndexIfNeeded(cd->m_srcIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_srcIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case TemplateOperationOpcode: {
                TemplateOperation* cd = (TemplateOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_src0Index, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_src1Index, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionOpcode: {
                CallFunction* cd = (CallFunction*)currentCode;
                assignStackIndexIfNeeded(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionWithReceiverOpcode: {
                CallFunctionWithReceiver* cd = (CallFunctionWithReceiver*)currentCode;
                assignStackIndexIfNeeded(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallEvalFunctionOpcode: {
                CallEvalFunction* cd = (CallEvalFunction*)currentCode;
                assignStackIndexIfNeeded(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionInWithScopeOpcode: {
                CallFunctionInWithScope* cd = (CallFunctionInWithScope*)currentCode;
                assignStackIndexIfNeeded(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case NewOperationOpcode: {
                NewOperation* cd = (NewOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case JumpOpcode: {
                Jump* cd = (Jump*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                break;
            }
            case JumpIfTrueOpcode: {
                JumpIfTrue* cd = (JumpIfTrue*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case JumpIfFalseOpcode: {
                JumpIfFalse* cd = (JumpIfFalse*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ThrowOperationOpcode: {
                ThrowOperation* cd = (ThrowOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case EnumerateObjectOpcode: {
                EnumerateObject* cd = (EnumerateObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case WithOperationOpcode: {
                WithOperation* cd = (WithOperation*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case BinaryPlusOpcode:
            case BinaryMinusOpcode:
            case BinaryMultiplyOpcode:
            case BinaryDivisionOpcode:
            case BinaryModOpcode:
            case BinaryEqualOpcode:
            case BinaryNotEqualOpcode:
            case BinaryLessThanOpcode:
            case BinaryLessThanOrEqualOpcode:
            case BinaryGreaterThanOpcode:
            case BinaryGreaterThanOrEqualOpcode:
            case BinaryStrictEqualOpcode:
            case BinaryNotStrictEqualOpcode:
            case BinaryBitwiseAndOpcode:
            case BinaryBitwiseOrOpcode:
            case BinaryBitwiseXorOpcode:
            case BinaryLeftShiftOpcode:
            case BinarySignedRightShiftOpcode:
            case BinaryUnsignedRightShiftOpcode:
            case BinaryInOperationOpcode:
            case BinaryInstanceOfOperationOpcode: {
                BinaryPlus* plus = (BinaryPlus*)currentCode;
                assignStackIndexIfNeeded(plus->m_srcIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(plus->m_srcIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                assignStackIndexIfNeeded(plus->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            default:
                break;
            }

            ASSERT(opcode <= EndOpcode);
            code += byteCodeLengths[opcode];
        }
    }

#ifndef NDEBUG
    if (!shouldGenerateLOCData && getenv("DUMP_BYTECODE") && strlen(getenv("DUMP_BYTECODE"))) {
        printf("dumpBytecode %s (%d:%d)>>>>>>>>>>>>>>>>>>>>>>\n", codeBlock->m_functionName.string()->toUTF8StringData().data(), (int)codeBlock->sourceElementStart().line, (int)codeBlock->sourceElementStart().column);
        printf("register info.. (stack variable size(%d)) [", (int)codeBlock->identifierOnStackCount());
        for (size_t i = 0; i < block->m_requiredRegisterFileSizeInValueSize; i++) {
            printf("X,");
        }

        size_t b = block->m_requiredRegisterFileSizeInValueSize + 1;
        printf("`this`,");
        if (!codeBlock->isGlobalScopeCodeBlock()) {
            printf("`function`,");
            b++;
        }

        for (size_t i = 0; i < block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos().size(); i++) {
            if (block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos()[i].m_needToAllocateOnStack) {
                printf("(%d,%s),", (int)b++, block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos()[i].m_name.string()->toUTF8StringData().data());
            }
        }

        ExecutionState tempState(c);
        for (size_t i = 0; i < block->m_numeralLiteralData.size(); i++) {
            printf("(%d,%s),", (int)b++, block->m_numeralLiteralData[i].toString(tempState)->toUTF8StringData().data());
        }

        printf("]\n");

        char* code = block->m_code.data();
        size_t idx = 0;
        size_t end = block->m_code.size();

        while (idx < end) {
            ByteCode* currentCode = (ByteCode*)(code + idx);

            currentCode->dumpCode(idx, (const char*)code);
            idx += byteCodeLengths[currentCode->m_orgOpcode];
        }

        printf("dumpBytecode...<<<<<<<<<<<<<<<<<<<<<<\n");
    }
#endif

    return block;
}
}
