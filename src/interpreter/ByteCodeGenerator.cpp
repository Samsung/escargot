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
        new (cb->m_code.data() + codePos) JumpComplexCase(r);
        m_complexCaseStatementPositions.erase(iter);
    }
}

#ifdef ESCARGOT_DEBUGGER
void ByteCodeGenerateContext::insertBreakpoint(size_t line, Node* node)
{
    ASSERT(m_breakpointContext != nullptr);

    if (line != 0 && line != m_breakpointContext->m_lastBreakpointLine) {
        m_breakpointContext->m_breakpointLocations.push_back(Debugger::BreakpointLocation(line, (uint32_t)m_byteCodeBlock->currentCodeSize()));
        m_byteCodeBlock->pushCode(BreakpointDisabled(ByteCodeLOC(node->loc().index)), this, node);
        m_breakpointContext->m_lastBreakpointLine = line;
    }
}
#endif /* ESCARGOT_DEBUGGER */

#define ASSIGN_STACKINDEX_IF_NEEDED(registerIndex, stackBase, stackBaseWillBe, stackVariableSize)                         \
    {                                                                                                                     \
        if (LIKELY(registerIndex != REGISTER_LIMIT)) {                                                                    \
            if (registerIndex >= stackBase) {                                                                             \
                if (registerIndex >= (stackBase + VARIABLE_LIMIT)) {                                                      \
                    registerIndex = stackBaseWillBe + (registerIndex - (stackBase + VARIABLE_LIMIT)) + stackVariableSize; \
                } else {                                                                                                  \
                    registerIndex = stackBaseWillBe + (registerIndex - stackBase);                                        \
                }                                                                                                         \
            }                                                                                                             \
        }                                                                                                                 \
    }

static const uint8_t byteCodeLengths[] = {
#define ITER_BYTE_CODE(code, pushCount, popCount) \
    (uint8_t)sizeof(code),

    FOR_EACH_BYTECODE_OP(ITER_BYTE_CODE)
#undef ITER_BYTE_CODE
};

ByteCodeBlock* ByteCodeGenerator::generateByteCode(Context* c, InterpretedCodeBlock* codeBlock, Node* ast, ASTFunctionScopeContext* scopeCtx, bool isEvalMode, bool isOnGlobal, bool inWithFromRuntime, bool shouldGenerateLOCData)
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
    ParserContextInformation info(isEvalMode, isGlobalScope, codeBlock->isStrict(), inWithFromRuntime || codeBlock->inWith());

    NumeralLiteralVector* nData = nullptr;

    if (ast->type() == ASTNodeType::Program) {
        nData = &((ProgramNode*)ast)->numeralLiteralVector();
    } else {
        nData = &((FunctionNode*)ast)->numeralLiteralVector();
    }

    if (nData->size() == 0) {
        nData = nullptr;
    }

    ByteCodeGenerateContext ctx(codeBlock, block, info, nData);

#ifdef ESCARGOT_DEBUGGER
    ByteCodeBreakpointContext breakpointContext;
    ctx.m_breakpointContext = &breakpointContext;
#endif /* ESCARGOT_DEBUGGER */

    ctx.m_shouldGenerateLOCData = shouldGenerateLOCData;
    if (shouldGenerateLOCData) {
        block->m_locData = new ByteCodeLOCData();
    }

    // generate common codes
    try {
        AtomicString name = codeBlock->functionName();
        if (name.string()->length()) {
            if (UNLIKELY(codeBlock->isFunctionNameExplicitlyDeclared())) {
                if (codeBlock->canUseIndexedVariableStorage()) {
                    if (!codeBlock->isFunctionNameSaveOnHeap()) {
                        auto r = ctx.getRegister();
                        block->pushCode(LoadLiteral(ByteCodeLOC(0), r, Value()), &ctx, nullptr);
                        block->pushCode(Move(ByteCodeLOC(0), r, REGULAR_REGISTER_LIMIT + 1), &ctx, nullptr);
                        ctx.giveUpRegister();
                    }
                }
            } else if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap() && !name.string()->equals("arguments"))) {
                ctx.m_isVarDeclaredBindingInitialization = true;
                IdentifierNode* id = new (alloca(sizeof(IdentifierNode))) IdentifierNode(codeBlock->functionName());
                id->generateStoreByteCode(block, &ctx, REGULAR_REGISTER_LIMIT + 1, true);
            }
        }

        ast->generateStatementByteCode(block, &ctx);

#ifdef ESCARGOT_DEBUGGER
        if (c->debugger() && c->debugger()->enabled()) {
            c->debugger()->sendBreakpointLocations(breakpointContext.m_breakpointLocations);
        }
#endif /* ESCARGOT_DEBUGGER */
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

    if (ctx.m_maxPauseStatementExtraDataLength) {
        // yield delegate + .next call can use yield * 2 at once
        block->m_code.reserve(block->m_code.size() + ctx.m_maxPauseStatementExtraDataLength * 2);
    } else {
        if (block->m_code.capacity() - block->m_code.size() > 1024 * 4) {
            block->m_code.shrinkToFit();
        }
    }

    {
        ByteCodeRegisterIndex stackBase = REGULAR_REGISTER_LIMIT;
        ByteCodeRegisterIndex stackBaseWillBe = block->m_requiredRegisterFileSizeInValueSize;
        ByteCodeRegisterIndex stackVariableSize = codeBlock->totalStackAllocatedVariableSize();

        char* code = block->m_code.data();
        size_t codeBase = (size_t)code;
        char* end = code + block->m_code.size();

        while (code < end) {
            ByteCode* currentCode = (ByteCode*)code;
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
            Opcode opcode = (Opcode)(size_t)currentCode->m_opcodeInAddress;
#else
            Opcode opcode = currentCode->m_opcode;
#endif
            currentCode->assignOpcodeInAddress();

            switch (opcode) {
            case LoadLiteralOpcode: {
                LoadLiteral* cd = (LoadLiteral*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadRegexpOpcode: {
                LoadRegexp* cd = (LoadRegexp*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadByNameOpcode: {
                LoadByName* cd = (LoadByName*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreByNameOpcode: {
                StoreByName* cd = (StoreByName*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case InitializeByNameOpcode: {
                InitializeByName* cd = (InitializeByName*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreByNameWithAddressOpcode: {
                StoreByNameWithAddress* cd = (StoreByNameWithAddress*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_valueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadByHeapIndexOpcode: {
                LoadByHeapIndex* cd = (LoadByHeapIndex*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreByHeapIndexOpcode: {
                StoreByHeapIndex* cd = (StoreByHeapIndex*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case InitializeByHeapIndexOpcode: {
                InitializeByHeapIndex* cd = (InitializeByHeapIndex*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateFunctionOpcode: {
                CreateFunction* cd = (CreateFunction*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateRestElementOpcode: {
                CreateRestElement* cd = (CreateRestElement*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateObjectOpcode: {
                CreateObject* cd = (CreateObject*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateArrayOpcode: {
                CreateArray* cd = (CreateArray*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetObjectOpcode: {
                GetObject* cd = (GetObject*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_storeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetObjectOperationOpcode: {
                SetObjectOperation* cd = (SetObjectOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineOwnPropertyOperationOpcode: {
                ObjectDefineOwnPropertyOperation* cd = (ObjectDefineOwnPropertyOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineOwnPropertyWithNameOperationOpcode: {
                ObjectDefineOwnPropertyWithNameOperation* cd = (ObjectDefineOwnPropertyWithNameOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ArrayDefineOwnPropertyOperationOpcode: {
                ArrayDefineOwnPropertyOperation* cd = (ArrayDefineOwnPropertyOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                for (size_t i = 0; i < cd->m_count; i++)
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndexs[i], stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ArrayDefineOwnPropertyBySpreadElementOperationOpcode: {
                ArrayDefineOwnPropertyBySpreadElementOperation* cd = (ArrayDefineOwnPropertyBySpreadElementOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                for (size_t i = 0; i < cd->m_count; i++)
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndexs[i], stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetObjectPreComputedCaseOpcode: {
                GetObjectPreComputedCase* cd = (GetObjectPreComputedCase*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_storeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetObjectPreComputedCaseOpcode: {
                SetObjectPreComputedCase* cd = (SetObjectPreComputedCase*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetParameterOpcode: {
                GetParameter* cd = (GetParameter*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case EndOpcode: {
                End* cd = (End*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ReturnFunctionSlowCaseOpcode: {
                ReturnFunctionSlowCase* cd = (ReturnFunctionSlowCase*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case MoveOpcode: {
                Move* cd = (Move*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ObjectDefineGetterSetterOpcode: {
                ObjectDefineGetterSetter* cd = (ObjectDefineGetterSetter*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectPropertyNameRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectPropertyValueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case GetGlobalVariableOpcode: {
                GetGlobalVariable* cd = (GetGlobalVariable*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SetGlobalVariableOpcode: {
                SetGlobalVariable* cd = (SetGlobalVariable*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case InitializeGlobalVariableOpcode: {
                InitializeGlobalVariable* cd = (InitializeGlobalVariable*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ToNumberOpcode:
            case IncrementOpcode:
            case DecrementOpcode:
            case UnaryMinusOpcode:
            case UnaryNotOpcode:
            case UnaryBitwiseNotOpcode: {
                ToNumber* cd = (ToNumber*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_srcIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ToNumberIncrementOpcode:
            case ToNumberDecrementOpcode: {
                ToNumberIncrement* cd = (ToNumberIncrement*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_srcIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_storeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case UnaryTypeofOpcode: {
                UnaryTypeof* cd = (UnaryTypeof*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_srcIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case UnaryDeleteOpcode: {
                UnaryDelete* cd = (UnaryDelete*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_srcIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_srcIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case TemplateOperationOpcode: {
                TemplateOperation* cd = (TemplateOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_src0Index, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_src1Index, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionOpcode: {
                CallFunction* cd = (CallFunction*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionWithReceiverOpcode: {
                CallFunctionWithReceiver* cd = (CallFunctionWithReceiver*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CallFunctionComplexCaseOpcode: {
                CallFunctionComplexCase* cd = (CallFunctionComplexCase*)currentCode;
                if (cd->m_kind != CallFunctionComplexCase::InWithScope) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                }
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case NewOperationOpcode: {
                NewOperation* cd = (NewOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case JumpIfFalseOpcode: {
                JumpIfFalse* cd = (JumpIfFalse*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case JumpIfRelationOpcode: {
                JumpIfRelation* cd = (JumpIfRelation*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case JumpIfEqualOpcode: {
                JumpIfEqual* cd = (JumpIfEqual*)currentCode;
                cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ThrowOperationOpcode: {
                ThrowOperation* cd = (ThrowOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateEnumerateObjectOpcode: {
                CreateEnumerateObject* cd = (CreateEnumerateObject*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case IteratorOperationOpcode: {
                IteratorOperation* cd = (IteratorOperation*)currentCode;
                if (cd->m_operation == IteratorOperation::Operation::GetIterator) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_getIteratorData.m_srcObjectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_getIteratorData.m_dstIteratorRecordIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_getIteratorData.m_dstIteratorObjectIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorClose) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorCloseData.m_iterRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorCloseData.m_execeptionRegisterIndexIfExists, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorBind) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorBindData.m_iterRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorTestDone) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorTestDoneData.m_dstRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorNext) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorNextData.m_iteratorRecordRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorNextData.m_valueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorNextData.m_valueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorTestResultIsObject) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorTestResultIsObjectData.m_valueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorValue) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorValueData.m_srcRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iteratorValueData.m_dstRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                } else if (cd->m_operation == IteratorOperation::Operation::IteratorCheckOngoingExceptionOnAsyncIteratorClose) {
                } else {
                    ASSERT_NOT_REACHED();
                }
                break;
            }
            case GetMethodOpcode: {
                GetMethod* cd = (GetMethod*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case BindingRestElementOpcode: {
                BindingRestElement* cd = (BindingRestElement*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_iterOrEnumIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case WithOperationOpcode: {
                WithOperation* cd = (WithOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
                ASSIGN_STACKINDEX_IF_NEEDED(plus->m_srcIndex0, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(plus->m_srcIndex1, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(plus->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateSpreadArrayObjectOpcode: {
                CreateSpreadArrayObject* cd = (CreateSpreadArrayObject*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case NewOperationWithSpreadElementOpcode: {
                NewOperationWithSpreadElement* cd = (NewOperationWithSpreadElement*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case CreateClassOpcode: {
                CreateClass* cd = (CreateClass*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_classConstructorRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_classPrototypeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_superClassRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SuperReferenceOpcode: {
                SuperReference* cd = (SuperReference*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SuperSetObjectOperationOpcode: {
                SuperSetObjectOperation* cd = (SuperSetObjectOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyNameIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case SuperGetObjectOperationOpcode: {
                SuperSetObjectOperation* cd = (SuperSetObjectOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyNameIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case LoadThisBindingOpcode: {
                LoadThisBinding* cd = (LoadThisBinding*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case ExecutionPauseOpcode: {
                ExecutionPause* cd = (ExecutionPause*)currentCode;
                if (cd->m_reason == ExecutionPause::Reason::Yield) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_yieldData.m_yieldIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_yieldData.m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_yieldData.m_dstStateIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    code += cd->m_yieldData.m_tailDataLength;
                } else if (cd->m_reason == ExecutionPause::Reason::Await) {
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_awaitData.m_awaitIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_awaitData.m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    ASSIGN_STACKINDEX_IF_NEEDED(cd->m_awaitData.m_dstStateIndex, stackBase, stackBaseWillBe, stackVariableSize);
                    code += cd->m_awaitData.m_tailDataLength;
                } else if (cd->m_reason == ExecutionPause::Reason::GeneratorsInitialize) {
                    code += cd->m_asyncGeneratorInitializeData.m_tailDataLength;
                }
                break;
            }
            case TryOperationOpcode: {
                TryOperation* cd = (TryOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_catchedValueRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case NewTargetOperationOpcode: {
                NewTargetOperation* cd = (NewTargetOperation*)currentCode;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
        printf("dumpBytecode %s (%d:%d)>>>>>>>>>>>>>>>>>>>>>>\n", codeBlock->m_functionName.string()->toUTF8StringData().data(), (int)codeBlock->functionStart().line, (int)codeBlock->functionStart().column);
        printf("register info.. (stack variable total(%d), this + function + var (%d), max lexical depth (%d)) [", (int)codeBlock->totalStackAllocatedVariableSize(), (int)codeBlock->identifierOnStackCount(), (int)codeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth());
        for (size_t i = 0; i < block->m_requiredRegisterFileSizeInValueSize; i++) {
            printf("r%d,", (int)i);
        }

        size_t b = block->m_requiredRegisterFileSizeInValueSize + 1;
        printf("`r%d this`,", (int)block->m_requiredRegisterFileSizeInValueSize);
        if (!codeBlock->isGlobalScopeCodeBlock()) {
            printf("`r%d function`,", (int)b);
            b++;
        }

        for (size_t i = 0; i < block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos().size(); i++) {
            if (block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos()[i].m_needToAllocateOnStack) {
                auto name = block->m_codeBlock->asInterpretedCodeBlock()->identifierInfos()[i].m_name.string()->toNonGCUTF8StringData();
                if (i == 0 && block->m_codeBlock->isFunctionExpression()) {
                    name += "(function name)";
                }
                printf("`r%d,var %s`,", (int)b++, name.data());
            }
        }


        size_t lexIndex = 0;
        for (size_t i = 0; i < block->m_codeBlock->asInterpretedCodeBlock()->lexicalBlockStackAllocatedIdentifierMaximumDepth(); i++) {
            printf("`r%d,%d lexical`,", (int)b++, (int)lexIndex++);
        }

        ExecutionState tempState(c);
        for (size_t i = 0; i < block->m_numeralLiteralData.size(); i++) {
            printf("`r%d,%s(literal)`,", (int)b++, block->m_numeralLiteralData[i].toString(tempState)->toUTF8StringData().data());
        }

        printf("]\n");

        char* code = block->m_code.data();
        size_t idx = 0;
        size_t end = block->m_code.size();

        while (idx < end) {
            ByteCode* currentCode = (ByteCode*)(code + idx);

            currentCode->dumpCode(idx, (const char*)code);

            if (currentCode->m_orgOpcode == ExecutionPauseOpcode) {
                if (((ExecutionPause*)currentCode)->m_reason == ExecutionPause::Yield) {
                    idx += ((ExecutionPause*)currentCode)->m_yieldData.m_tailDataLength;
                } else if (((ExecutionPause*)currentCode)->m_reason == ExecutionPause::Await) {
                    idx += ((ExecutionPause*)currentCode)->m_awaitData.m_tailDataLength;
                } else if (((ExecutionPause*)currentCode)->m_reason == ExecutionPause::GeneratorsInitialize) {
                    idx += ((ExecutionPause*)currentCode)->m_asyncGeneratorInitializeData.m_tailDataLength;
                } else {
                    ASSERT_NOT_REACHED();
                }
            }

            idx += byteCodeLengths[currentCode->m_orgOpcode];
        }

        printf("dumpBytecode...<<<<<<<<<<<<<<<<<<<<<<\n");
    }
#endif

    return block;
}
}
