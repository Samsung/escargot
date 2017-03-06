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
        ControlFlowRecord* r = new ControlFlowRecord(ControlFlowRecord::ControlFlowReason::NeedsJump, (cb->peekCode<Jump>(codePos)->m_jumpPosition), iter->second);
        m_byteCodeBlock->m_literalData.pushBack((PointerValue*)r);
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

ByteCodeBlock* ByteCodeGenerator::generateByteCode(Context* c, CodeBlock* codeBlock, Node* ast, ASTScopeContext* scopeCtx, bool isEvalMode, bool isOnGlobal, bool shouldGenerateLOCData)
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

    // nData points interior pointer of ASTScopeContext
    // it is prohibited in Escargot, but this pointer is safe
    // because, lifetime of AST and ASTScopeContext is same in here
    Vector<Value, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<Value>>* nData = &scopeCtx->m_numeralLiteralData;

    if (scopeCtx->m_hasManyNumeralLiteral) {
        nData = nullptr;
    }
    ByteCodeGenerateContext ctx(codeBlock, block, info, nData);
    ctx.m_shouldGenerateLOCData = shouldGenerateLOCData;

    // generate init function decls first
    size_t len = codeBlock->childBlocks().size();
    for (size_t i = 0; i < len; i++) {
        CodeBlock* b = codeBlock->childBlocks()[i];
        if (b->isFunctionDeclaration()) {
            ctx.getRegister();
            block->pushCode(DeclareFunctionDeclaration(b), &ctx, nullptr);
            IdentifierNode idNode(b->m_functionName);
            idNode.generateStoreByteCode(block, &ctx, 0, false);
            ctx.giveUpRegister();
        }
    }

    // generate common codes
    try {
        ast->generateStatementByteCode(block, &ctx);
        if (!codeBlock->isGlobalScopeCodeBlock())
            block->pushCode(ReturnFunction(ByteCodeLOC(SIZE_MAX)), &ctx, nullptr);
    } catch (const char* err) {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (ctx.m_keepNumberalLiteralsInRegisterFile) {
        block->m_numeralLiteralData.resizeWithUninitializedValues(nData->size());
        memcpy(block->m_numeralLiteralData.data(), nData->data(), sizeof(Value) * nData->size());
    }

    scopeCtx->m_numeralLiteralData.clear();

    // ESCARGOT_LOG_INFO("codeSize %lf, %lf\n", block->m_code.size() / 1024.0 / 1024.0, block->m_code.capacity() / 1024.0 / 1024.0);
    block->m_code.shrinkToFit();

    block->m_getObjectCodePositions = std::move(ctx.m_getObjectCodePositions);

    {
        ByteCodeRegisterIndex stackBase = REGULAR_REGISTER_LIMIT;
        ByteCodeRegisterIndex stackBaseWillBe = block->m_requiredRegisterFileSizeInValueSize;
        ByteCodeRegisterIndex stackVariableSize = codeBlock->identifierOnStackCount();
        size_t idx = 0;
        size_t bytecodeCounter = 0;

        char* code = block->m_code.data();
        size_t codeBase = (size_t)code;
        char* end = &block->m_code.data()[block->m_code.size()];
        while (&code[idx] < end) {
            ByteCode* currentCode = (ByteCode*)(&code[idx]);
            Opcode opcode = (Opcode)(size_t)currentCode->m_opcodeInAddress;
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
            case LoadArgumentsObjectOpcode: {
                LoadArgumentsObject* cd = (LoadArgumentsObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case StoreArgumentsObjectOpcode: {
                StoreArgumentsObject* cd = (StoreArgumentsObject*)currentCode;
                assignStackIndexIfNeeded(cd->m_registerIndex, stackBase, stackBaseWillBe, stackVariableSize);
                break;
            }
            case DeclareFunctionExpressionOpcode: {
                DeclareFunctionExpression* cd = (DeclareFunctionExpression*)currentCode;
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
            case SetObjectOpcode: {
                SetObject* cd = (SetObject*)currentCode;
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
                assignStackIndexIfNeeded(cd->m_loadRegisterIndex, stackBase, stackBaseWillBe, stackVariableSize);
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
            case JumpComplexCaseOpcode: {
                JumpComplexCase* cd = (JumpComplexCase*)currentCode;
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

            switch (opcode) {
#define ITER_BYTE_CODE(code, pushCount, popCount) \
    case code##Opcode:                            \
        idx += sizeof(code);                      \
        continue;

                FOR_EACH_BYTECODE_OP(ITER_BYTE_CODE)
#undef ITER_BYTE_CODE
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            };
        }
    }

#ifndef NDEBUG
    if (getenv("DUMP_BYTECODE") && strlen(getenv("DUMP_BYTECODE"))) {
        printf("dumpBytecode %s (%d:%d)>>>>>>>>>>>>>>>>>>>>>>\n", codeBlock->m_functionName.string()->toUTF8StringData().data(), (int)codeBlock->sourceElementStart().line, (int)codeBlock->sourceElementStart().column);
        printf("register info.. [");
        for (size_t i = 0; i < block->m_requiredRegisterFileSizeInValueSize; i++) {
            printf("X,");
        }
        size_t b = block->m_requiredRegisterFileSizeInValueSize;
        for (size_t i = 0; i < block->m_codeBlock->identifierInfos().size(); i++) {
            if (block->m_codeBlock->identifierInfos()[i].m_needToAllocateOnStack) {
                printf("(%d,%s),", (int)b++, block->m_codeBlock->identifierInfos()[i].m_name.string()->toUTF8StringData().data());
            }
        }
        printf("this,");

        ExecutionState tempState(c);
        for (size_t i = 0; i < block->m_numeralLiteralData.size(); i++) {
            printf("(%d,%s),", (int)b++, block->m_numeralLiteralData[i].toString(tempState)->toUTF8StringData().data());
        }

        printf("]\n");
        size_t idx = 0;
        size_t bytecodeCounter = 0;
        char* code = block->m_code.data();
        size_t codeBase = (size_t)code;
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
        currentCode->dumpCode(idx + codeBase);    \
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

    return block;
}
}
