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
#include "ByteCodeGenerator.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"
#include "debugger/Debugger.h"

#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCache.h"
#endif

namespace Escargot {

ByteCodeGenerateContext::ByteCodeGenerateContext(InterpretedCodeBlock* codeBlock, ByteCodeBlock* byteCodeBlock, bool isGlobalScope, bool isEvalCode, bool isWithScope, void* numeralLiteralData)
    : m_baseRegisterCount(0)
    , m_codeBlock(codeBlock)
    , m_byteCodeBlock(byteCodeBlock)
    , m_locData(nullptr)
    , m_stackLimit(codeBlock->context()->vmInstance()->stackLimit())
    , m_isGlobalScope(isGlobalScope)
    , m_isEvalCode(isEvalCode)
    , m_isOutermostContext(true)
    , m_isWithScope(isWithScope)
    , m_isFunctionDeclarationBindingInitialization(false)
    , m_isVarDeclaredBindingInitialization(false)
    , m_isLexicallyDeclaredBindingInitialization(false)
    , m_canSkipCopyToRegister(true)
    , m_keepNumberalLiteralsInRegisterFile(numeralLiteralData)
    , m_inCallingExpressionScope(false)
    , m_inObjectDestruction(false)
    , m_inParameterInitialization(false)
    , m_isHeadOfMemberExpression(false)
    , m_forInOfVarBinding(false)
    , m_isLeftBindingAffectedByRightExpression(false)
    , m_registerStack(new std::vector<ByteCodeRegisterIndex>())
    , m_lexicallyDeclaredNames(new std::vector<std::pair<size_t, AtomicString>>())
    , m_positionToContinue(0)
    , m_complexJumpBreakIgnoreCount(0)
    , m_complexJumpContinueIgnoreCount(0)
    , m_lexicalBlockIndex(0)
    , m_classInfo()
    , m_numeralLiteralData(numeralLiteralData) // should be NumeralLiteralVector
    , m_returnRegister(SIZE_MAX)
#ifdef ESCARGOT_DEBUGGER
    , m_breakpointContext(nullptr)
#endif /* ESCARGOT_DEBUGGER */
{
}

void ByteCodeGenerateContext::consumeLabelledContinuePositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount)
{
    for (size_t i = 0; i < m_labelledContinueStatmentPositions.size(); i++) {
        if (*m_labelledContinueStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labelledContinueStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labelledContinueStatmentPositions[i].second, outerLimitCount);
            m_labelledContinueStatmentPositions.erase(m_labelledContinueStatmentPositions.begin() + i);
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

void ByteCodeGenerateContext::consumeLabelledBreakPositions(ByteCodeBlock* cb, size_t position, String* lbl, int outerLimitCount)
{
    for (size_t i = 0; i < m_labelledBreakStatmentPositions.size(); i++) {
        if (*m_labelledBreakStatmentPositions[i].first == *lbl) {
            Jump* shouldBeJump = cb->peekCode<Jump>(m_labelledBreakStatmentPositions[i].second);
            ASSERT(shouldBeJump->m_orgOpcode == JumpOpcode);
            shouldBeJump->m_jumpPosition = position;
            morphJumpPositionIntoComplexCase(cb, m_labelledBreakStatmentPositions[i].second, outerLimitCount);
            m_labelledBreakStatmentPositions.erase(m_labelledBreakStatmentPositions.begin() + i);
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
        size_t index = m_byteCodeBlock->m_jumpFlowRecordData.size();
        m_byteCodeBlock->m_jumpFlowRecordData.pushBack(JumpFlowRecord((cb->peekCode<Jump>(codePos)->m_jumpPosition), iter->second, outerLimitCount));
        new (cb->m_code.data() + codePos) JumpComplexCase(index);
        m_complexCaseStatementPositions.erase(iter);
    }
}

void ByteCodeGenerateContext::linkOptionalChainingJumpPosition(ByteCodeBlock* cb, size_t jumpToPosition)
{
    ASSERT(m_optionalChainingJumpPositionLists.size());
    std::vector<size_t>& jumpPositions = m_optionalChainingJumpPositionLists.back();
    for (size_t i = 0; i < jumpPositions.size(); i++) {
        cb->peekCode<JumpIfUndefinedOrNull>(jumpPositions[i])->m_jumpPosition = jumpToPosition;
    }
}

#ifdef ESCARGOT_DEBUGGER
void ByteCodeGenerateContext::calculateBreakpointLocation(size_t index, ExtendedNodeLOC sourceElementStart)
{
    ASSERT(index >= sourceElementStart.index);
    const size_t indexOffset = index - sourceElementStart.index;
    size_t lineOffset = m_breakpointContext->m_lastBreakpointLineOffset;
    ASSERT(indexOffset >= m_breakpointContext->m_lastBreakpointIndexOffset);

    // calculate the current breakpoint's location based on the last breakpoint's location
    StringView src = m_codeBlock->src();
    for (size_t i = m_breakpointContext->m_lastBreakpointIndexOffset; i < indexOffset; i++) {
        char16_t c = src.charAt(i);
        if (EscargotLexer::isLineTerminator(c)) {
            // skip \r\n
            if (UNLIKELY(c == 13 && (i + 1 < indexOffset) && src.charAt(i + 1) == 10)) {
                i++;
            }
            lineOffset++;
        }
    }

    // cache the current breakpoint's calculated location (offset)
    m_breakpointContext->m_lastBreakpointIndexOffset = indexOffset;
    m_breakpointContext->m_lastBreakpointLineOffset = lineOffset;
}

void ByteCodeGenerateContext::insertBreakpoint(size_t index, Node* node)
{
    ASSERT(m_breakpointContext != nullptr);
    ASSERT(index != SIZE_MAX);

    // do not insert any breakpoint for unmarked code
    if (UNLIKELY(!m_codeBlock->markDebugging())) {
        return;
    }

    // dynamically generated code should not insert any debugging code
    ASSERT(!m_codeBlock->hasDynamicSourceCode());

    // previous breakpoint's line offset
    size_t lastLineOffset = m_breakpointContext->m_lastBreakpointLineOffset;
    ExtendedNodeLOC sourceElementStart = m_codeBlock->functionStart();
    calculateBreakpointLocation(index, sourceElementStart);

    // insert a breakpoint if its the first breakpoint insertion or
    // there was no breakpoint insertion with the same lineoffset
    if ((m_breakpointContext->m_breakpointLocations->breakpointLocations.size() == 0) || (lastLineOffset != m_breakpointContext->m_lastBreakpointLineOffset)) {
        ASSERT(lastLineOffset <= m_breakpointContext->m_lastBreakpointLineOffset);
        insertBreakpointAt(m_breakpointContext->m_lastBreakpointLineOffset + sourceElementStart.line, node);
    }
}

void ByteCodeGenerateContext::insertBreakpointAt(size_t line, Node* node)
{
    // add breakpoint only when its line offset is bigger than original source's start line offset
    // m_originSourceLineOffset is usually set as 0, but for the manipulated source code case,
    // we should consider original source code and insert bytecode only for the original source code case
    if (line > m_breakpointContext->m_originSourceLineOffset) {
        m_breakpointContext->m_breakpointLocations->breakpointLocations.push_back(Debugger::BreakpointLocation(line, (uint32_t)m_byteCodeBlock->currentCodeSize()));
        m_byteCodeBlock->pushCode(BreakpointDisabled(ByteCodeLOC(node->loc().index)), this, node->m_loc.index);
    }
}

ByteCodeBreakpointContext::ByteCodeBreakpointContext(Debugger* debugger, InterpretedCodeBlock* codeBlock, bool addBreakpointLocationsInfoToDebugger)
    : m_lastBreakpointLineOffset(0)
    , m_lastBreakpointIndexOffset(0)
    , m_originSourceLineOffset(codeBlock->script()->originSourceLineOffset())
    , m_breakpointLocations()
{
    m_breakpointLocations = new Debugger::BreakpointLocationsInfo(reinterpret_cast<Debugger::WeakCodeRef*>(codeBlock));
    if (debugger && codeBlock->markDebugging() && addBreakpointLocationsInfoToDebugger) {
        debugger->appendBreakpointLocations(m_breakpointLocations);
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
                ASSERT(registerIndex < block->m_requiredTotalRegisterNumber);                                             \
            }                                                                                                             \
        }                                                                                                                 \
    }

const uint8_t byteCodeLengths[] = {
#define ITER_BYTE_CODE(code) \
    (uint8_t)sizeof(code),

    FOR_EACH_BYTECODE(ITER_BYTE_CODE)
#undef ITER_BYTE_CODE
};

ByteCodeBlock* ByteCodeGenerator::generateByteCode(Context* context, InterpretedCodeBlock* codeBlock, Node* ast, bool inWithFromRuntime, bool cacheByteCode)
{
    ASSERT(!codeBlock->byteCodeBlock());

    ByteCodeBlock* block = new ByteCodeBlock(codeBlock);

    NumeralLiteralVector* nData = nullptr;

    if (ast->type() == ASTNodeType::Program) {
        nData = &((ProgramNode*)ast)->numeralLiteralVector();
    } else {
        nData = &((FunctionNode*)ast)->numeralLiteralVector();
    }

    if (nData->size() == 0) {
        nData = nullptr;
    }

    ByteCodeGenerateContext ctx(codeBlock, block, codeBlock->isGlobalScope(), codeBlock->isEvalCode(), inWithFromRuntime || codeBlock->inWith(), nData);

#ifdef ESCARGOT_DEBUGGER
    ByteCodeBreakpointContext breakpointContext(context->debugger(), codeBlock);
    ctx.m_breakpointContext = &breakpointContext;
#endif /* ESCARGOT_DEBUGGER */

    // generate bytecode
    ast->generateStatementByteCode(block, &ctx);

    if (ctx.m_keepNumberalLiteralsInRegisterFile) {
        block->m_numeralLiteralData.resizeWithUninitializedValues(nData->size());
        memcpy(block->m_numeralLiteralData.data(), nData->data(), sizeof(Value) * nData->size());
    }

    if (block->m_code.capacity() - block->m_code.size() > 1024 * 4) {
        block->m_code.shrinkToFit();
    }

    block->m_requiredTotalRegisterNumber = block->m_requiredOperandRegisterNumber + codeBlock->totalStackAllocatedVariableSize() + block->m_numeralLiteralData.size();

#if defined(ENABLE_CODE_CACHE)
    // cache bytecode right before relocation
    if (UNLIKELY(cacheByteCode)) {
        context->vmInstance()->codeCache()->storeByteCodeBlock(block);
        context->vmInstance()->codeCache()->storeStringTable();
    }
#endif

    ByteCodeGenerator::relocateByteCode(block);

#ifndef NDEBUG
    ByteCodeGenerator::printByteCode(context, block);
    ctx.checkAllDataUsed();
#endif

    return block;
}

void ByteCodeGenerator::collectByteCodeLOCData(Context* context, InterpretedCodeBlock* codeBlock, ByteCodeLOCData* locData)
{
    // this function is called only to calculate location info of each bytecode
    ASSERT(!!locData && !locData->size());

    GC_disable();

    // Parsing
    Node* ast = nullptr;
    if (codeBlock->isGlobalCodeBlock() || codeBlock->isEvalCode()) {
        auto parseResult = esprima::parseProgram(context, codeBlock->src(), esprima::generateClassInfoFrom(context, codeBlock->parent()),
                                                 codeBlock->script()->isModule(), codeBlock->isStrict(), codeBlock->inWith(), SIZE_MAX, false, false, false, true);
        ast = parseResult.first;
    } else {
        auto parseResult = esprima::parseSingleFunction(context, codeBlock, SIZE_MAX);
        ast = parseResult.first;
    }
    ASSERT(!!ast);

    // Generate ByteCode
    // ByteCodeBlock is temporally allocated on the stack
    ByteCodeBlock block;
    block.m_codeBlock = codeBlock;

    NumeralLiteralVector* nData = nullptr;

    if (ast->type() == ASTNodeType::Program) {
        nData = &((ProgramNode*)ast)->numeralLiteralVector();
    } else {
        nData = &((FunctionNode*)ast)->numeralLiteralVector();
    }

    if (nData->size() == 0) {
        nData = nullptr;
    }

    ByteCodeGenerateContext ctx(codeBlock, &block, codeBlock->isGlobalScope(), codeBlock->isEvalCode(), codeBlock->inWith(), nData);
    ctx.m_locData = locData;

#ifdef ESCARGOT_DEBUGGER
    // create a dummy ByteCodeBreakpointContext because we just calculate location info only
    // unnecessary BreakpointLocationsInfoToDebugger insertion would incur incorrect debugger operations
    ByteCodeBreakpointContext breakpointContext(context->debugger(), codeBlock, false);
    ctx.m_breakpointContext = &breakpointContext;
#endif /* ESCARGOT_DEBUGGER */

    ast->generateStatementByteCode(&block, &ctx);

    // reset ASTAllocator
    context->astAllocator().reset();
    GC_enable();
}

void ByteCodeGenerator::relocateByteCode(ByteCodeBlock* block)
{
    InterpretedCodeBlock* codeBlock = block->codeBlock();

    ByteCodeRegisterIndex stackBase = REGULAR_REGISTER_LIMIT;
    ByteCodeRegisterIndex stackBaseWillBe = block->m_requiredOperandRegisterNumber;
    ByteCodeRegisterIndex stackVariableSize = codeBlock->totalStackAllocatedVariableSize();

    char* code = block->m_code.data();
    size_t codeBase = (size_t)code;
    char* end = code + block->m_code.size();

    while (code < end) {
        ByteCode* currentCode = (ByteCode*)code;
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
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
        case LoadRegExpOpcode: {
            LoadRegExp* cd = (LoadRegExp*)currentCode;
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
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_homeObjectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
        case ToNumericIncrementOpcode:
        case ToNumericDecrementOpcode: {
            ToNumericIncrement* cd = (ToNumericIncrement*)currentCode;
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
        case CallOpcode: {
            Call* cd = (Call*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case CallWithReceiverOpcode: {
            CallWithReceiver* cd = (CallWithReceiver*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_resultIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
#if defined(ENABLE_TCO)
        // TCO
        case CallReturnOpcode: {
            CallReturn* cd = (CallReturn*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case TailRecursionOpcode: {
            TailRecursion* cd = (TailRecursion*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case CallReturnWithReceiverOpcode: {
            CallReturnWithReceiver* cd = (CallReturnWithReceiver*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case TailRecursionWithReceiverOpcode: {
            TailRecursionWithReceiver* cd = (TailRecursionWithReceiver*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_calleeIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_argumentsStartIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
#endif
        case CallComplexCaseOpcode: {
            CallComplexCase* cd = (CallComplexCase*)currentCode;
            if (cd->m_kind != CallComplexCase::InWithScope) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_receiverOrThisIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
        case JumpIfUndefinedOrNullOpcode: {
            JumpIfUndefinedOrNull* cd = (JumpIfUndefinedOrNull*)currentCode;
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
        case JumpIfNotFulfilledOpcode: {
            JumpIfNotFulfilled* cd = (JumpIfNotFulfilled*)currentCode;
            cd->m_jumpPosition = cd->m_jumpPosition + codeBase;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_leftIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_rightIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
        case OpenLexicalEnvironmentOpcode: {
            OpenLexicalEnvironment* cd = (OpenLexicalEnvironment*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_withOrThisRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case BinaryPlusOpcode:
        case BinaryMinusOpcode:
        case BinaryMultiplyOpcode:
        case BinaryDivisionOpcode:
        case BinaryModOpcode:
        case BinaryEqualOpcode:
        case BinaryLessThanOpcode:
        case BinaryLessThanOrEqualOpcode:
        case BinaryGreaterThanOpcode:
        case BinaryGreaterThanOrEqualOpcode:
        case BinaryStrictEqualOpcode:
        case BinaryBitwiseAndOpcode:
        case BinaryBitwiseOrOpcode:
        case BinaryBitwiseXorOpcode:
        case BinaryLeftShiftOpcode:
        case BinarySignedRightShiftOpcode:
        case BinaryUnsignedRightShiftOpcode:
        case BinaryInOperationOpcode:
        case BinaryInstanceOfOperationOpcode:
        case BinaryExponentiationOpcode: {
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
        case InitializeClassOpcode: {
            InitializeClass* cd = (InitializeClass*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_classConstructorRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            if (cd->m_stage == InitializeClass::CreateClass) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_classPrototypeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_superClassRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            } else if (cd->m_stage == InitializeClass::InitField) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyInitRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            } else if (cd->m_stage == InitializeClass::SetFieldData) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertySetRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            } else if (cd->m_stage == InitializeClass::InitStaticField) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_staticPropertyInitRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            } else if (cd->m_stage == InitializeClass::SetStaticFieldData) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_staticPropertySetRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            }
            break;
        }
        case SuperReferenceOpcode: {
            SuperReference* cd = (SuperReference*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_dstIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case ComplexSetObjectOperationOpcode: {
            ComplexSetObjectOperation* cd = (ComplexSetObjectOperation*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            if (cd->m_type == ComplexSetObjectOperation::Super) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyNameIndex, stackBase, stackBaseWillBe, stackVariableSize);
            }
            break;
        }
        case ComplexGetObjectOperationOpcode: {
            ComplexGetObjectOperation* cd = (ComplexGetObjectOperation*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_objectRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_storeRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
            if (cd->m_type == ComplexGetObjectOperation::Super) {
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_propertyNameIndex, stackBase, stackBaseWillBe, stackVariableSize);
            }
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
        case MetaPropertyOperationOpcode: {
            MetaPropertyOperation* cd = (MetaPropertyOperation*)currentCode;
            ASSIGN_STACKINDEX_IF_NEEDED(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
            break;
        }
        case TaggedTemplateOperationOpcode: {
            TaggedTemplateOperation* cd = (TaggedTemplateOperation*)currentCode;
            if (cd->m_operaton == TaggedTemplateOperation::TestCacheOperation) {
                cd->m_testCacheOperationData.m_jumpPosition = cd->m_testCacheOperationData.m_jumpPosition + codeBase;
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_testCacheOperationData.m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
            } else {
                ASSERT(cd->m_operaton == TaggedTemplateOperation::FillCacheOperation);
                ASSIGN_STACKINDEX_IF_NEEDED(cd->m_fillCacheOperationData.m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
            }
            break;
        }
        default:
            break;
        }

#ifdef ESCARGOT_DEBUGGER
        ASSERT((opcode <= EndOpcode) || (opcode == BreakpointDisabledOpcode || opcode == BreakpointEnabledOpcode));
#else
        ASSERT(opcode <= EndOpcode);
#endif
        code += byteCodeLengths[opcode];
    }
}

#ifndef NDEBUG
void ByteCodeGenerator::printByteCode(Context* context, ByteCodeBlock* block)
{
    ASSERT(!!block->codeBlock());
    InterpretedCodeBlock* codeBlock = block->codeBlock();

    char* dumpByteCode = getenv("DUMP_BYTECODE");
    if (dumpByteCode && (strcmp(dumpByteCode, "1") == 0)) {
        printf("dumpBytecode %s (%d:%d)>>>>>>>>>>>>>>>>>>>>>>\n", codeBlock->functionName().string()->toUTF8StringData().data(), (int)codeBlock->functionStart().line, (int)codeBlock->functionStart().column);
        printf("register info.. (stack variable total(%d), this + function + var (%d), max lexical depth (%d)) [", (int)codeBlock->totalStackAllocatedVariableSize(), (int)codeBlock->identifierOnStackCount(), (int)codeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth());
        for (size_t i = 0; i < block->m_requiredOperandRegisterNumber; i++) {
            printf("r%d,", (int)i);
        }

        size_t b = block->m_requiredOperandRegisterNumber + 1;
        printf("`r%d this`,", (int)block->m_requiredOperandRegisterNumber);
        if (!codeBlock->isGlobalCodeBlock()) {
            printf("`r%d function`,", (int)b);
            b++;
        }

        size_t localStart = 0;
        if (codeBlock->isFunctionExpression() && !codeBlock->isFunctionNameSaveOnHeap()) {
            localStart = 1;
        }
        for (size_t i = localStart; i < codeBlock->identifierInfos().size(); i++) {
            if (codeBlock->identifierInfos()[i].m_needToAllocateOnStack) {
                auto name = codeBlock->identifierInfos()[i].m_name.string()->toNonGCUTF8StringData();
                if (i == 0 && codeBlock->isFunctionExpression()) {
                    name += "(function name)";
                }
                printf("`r%d,var %s`,", (int)b++, name.data());
            }
        }


        size_t lexIndex = 0;
        for (size_t i = 0; i < codeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth(); i++) {
            printf("`r%d,%d lexical`,", (int)b++, (int)lexIndex++);
        }

        ExecutionState tempState(context);
        for (size_t i = 0; i < block->m_numeralLiteralData.size(); i++) {
            printf("`r%d,%s(literal)`,", (int)b++, block->m_numeralLiteralData[i].toString(tempState)->toUTF8StringData().data());
        }

        printf("]\n");
        ByteCode::dumpCode(block->m_code.data(), block->m_code.size());
        printf("dumpBytecode...<<<<<<<<<<<<<<<<<<<<<<\n");
    }
}
#endif
} // namespace Escargot
