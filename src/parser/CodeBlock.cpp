#include "Escargot.h"
#include "CodeBlock.h"
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"

namespace Escargot {

void* CodeBlock::operator new(size_t size)
{
#ifdef GC_DEBUG
    return CustomAllocator<CodeBlock>().allocate(1);
#else
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(CodeBlock)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_context));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_script));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_identifierInfos));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_parameterNames));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_parametersInfomation));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_parentCodeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_childBlocks));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_cachedASTNode));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_byteCodeBlock));
#ifndef NDEBUG
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CodeBlock, m_scopeContext));
#endif
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(CodeBlock));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#endif
}

CodeBlock::CodeBlock(Context* ctx, const NativeFunctionInfo& info)
    : m_context(ctx)
    , m_script(nullptr)
    , m_src()
    , m_sourceElementStart(0, 0, 0)
    , m_astNodeStartIndex(0)
    , m_identifierOnStackCount(info.m_argumentCount + (info.m_name.string()->length() ? 1 : 0))
    , m_identifierOnHeapCount(0)
    , m_functionNameIndex(info.m_name.string()->length() ? info.m_argumentCount : SIZE_MAX)
    , m_parentCodeBlock(nullptr)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
    , m_nativeFunctionConstructor(info.m_nativeFunctionConstructor)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    if (m_functionNameIndex != SIZE_MAX) {
        m_functionNameSaveInfo.m_isAllocatedOnStack = true;
        m_functionNameSaveInfo.m_index = m_functionNameIndex;
    }
    m_isNativeFunction = true;
    m_functionName = info.m_name;
    m_isStrict = info.m_isStrict;
    m_hasEval = false;
    m_hasWith = false;
    m_hasCatch = false;
    m_hasYield = false;
    m_usesArgumentsObject = false;
    m_hasArgumentsBinding = false;
    m_canUseIndexedVariableStorage = true;
    m_canAllocateEnvironmentOnStack = true;
    m_needsComplexParameterCopy = false;
    m_isInWithScope = false;

    m_parametersInfomation.resize(info.m_argumentCount);

    m_byteCodeBlock = new ByteCodeBlock(this);
    CallNativeFunction code(info.m_nativeFunction);
    code.assignOpcodeInAddress();
    char* first = (char*)&code;
    size_t start = m_byteCodeBlock->m_code.size();
    m_byteCodeBlock->m_code.resize(m_byteCodeBlock->m_code.size() + sizeof(CallNativeFunction));
    for (size_t i = 0; i < sizeof(CallNativeFunction); i++) {
        m_byteCodeBlock->m_code[start++] = *first;
        first++;
    }
}

CodeBlock::CodeBlock(Context* ctx, FunctionObject* targetFunction, Value& boundThis, size_t boundArgc, Value* boundArgv)
    : m_context(ctx)
    , m_script(nullptr)
    , m_src()
    , m_sourceElementStart(0, 0, 0)
    , m_astNodeStartIndex(0)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_functionNameIndex(SIZE_MAX)
    , m_parentCodeBlock(targetFunction->codeBlock()->parentCodeBlock())
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
    , m_nativeFunctionConstructor(targetFunction->codeBlock()->nativeFunctionConstructor())
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    const CodeBlock* targetCodeBlock = targetFunction->codeBlock();

    m_isNativeFunction = targetCodeBlock->isNativeFunction();
    m_functionName = targetCodeBlock->functionName();
    m_isStrict = targetCodeBlock->isStrict();
    m_hasEval = targetCodeBlock->hasEval();
    m_hasWith = targetCodeBlock->hasWith();
    m_hasCatch = targetCodeBlock->hasCatch();
    m_hasYield = targetCodeBlock->hasYield();
    m_usesArgumentsObject = targetCodeBlock->usesArgumentsObject();
    m_canUseIndexedVariableStorage = targetCodeBlock->canUseIndexedVariableStorage();
    m_canAllocateEnvironmentOnStack = targetCodeBlock->canAllocateEnvironmentOnStack();
    m_needsComplexParameterCopy = targetCodeBlock->needsComplexParameterCopy();
    m_isInWithScope = targetCodeBlock->isInWithScope();

    size_t targetFunctionLength = targetCodeBlock->parametersInfomation().size();
    m_parametersInfomation.resize(targetFunctionLength > boundArgc ? targetFunctionLength - boundArgc : 0);

    m_byteCodeBlock = new ByteCodeBlock(this);
    CallBoundFunction code(ByteCodeLOC(0));
    code.m_boundTargetFunction = targetFunction;
    code.m_boundThis = boundThis;
    code.m_boundArgumentsCount = boundArgc;
    code.m_boundArguments = (Value*)GC_MALLOC(boundArgc * sizeof(Value));
    memcpy(code.m_boundArguments, boundArgv, boundArgc * sizeof(Value));
    m_byteCodeBlock->m_literalData.pushBack(targetFunction);
    m_byteCodeBlock->m_literalData.pushBack(boundThis);
    m_byteCodeBlock->m_literalData.pushBack((PointerValue*)code.m_boundArguments);

    ParserContextInformation defaultInfo;
    ByteCodeGenerateContext context(this, m_byteCodeBlock, defaultInfo);
    m_byteCodeBlock->pushCode(code, &context, nullptr);
    m_byteCodeBlock->m_code.shrinkToFit();
}

CodeBlock::CodeBlock(Context* ctx, Script* script, StringView src, bool isStrict, ExtendedNodeLOC sourceElementStart, const AtomicStringVector& innerIdentifiers, CodeBlockInitFlag initFlags)
    : m_context(ctx)
    , m_script(script)
    , m_src(src)
    , m_sourceElementStart(sourceElementStart)
    , m_astNodeStartIndex(sourceElementStart.index)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_functionNameIndex(SIZE_MAX)
    , m_parentCodeBlock(nullptr)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
    , m_nativeFunctionConstructor(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_isFunctionDeclaration = false;
    m_isFunctionExpression = false;
    m_isNativeFunction = false;
    m_isStrict = isStrict;
    m_hasEval = false;
    if (initFlags & CodeBlockInitFlag::CodeBlockHasEval) {
        m_hasEval = true;
    } else {
        m_hasEval = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasWith) {
        m_hasWith = true;
    } else {
        m_hasWith = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasCatch) {
        m_hasCatch = true;
    } else {
        m_hasCatch = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasYield) {
        m_hasYield = true;
    } else {
        m_hasYield = false;
    }
    m_usesArgumentsObject = false;
    m_hasArgumentsBinding = false;
    m_canUseIndexedVariableStorage = false;
    m_canAllocateEnvironmentOnStack = false;
    m_needsComplexParameterCopy = false;
    m_isInWithScope = false;

    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i];
        info.m_needToAllocateOnStack = false;
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }
}

CodeBlock::CodeBlock(Context* ctx, Script* script, StringView src, ExtendedNodeLOC sourceElementStart, bool isStrict, size_t astNodeStartIndex, AtomicString functionName, const AtomicStringVector& parameterNames, const AtomicStringVector& innerIdentifiers,
                     CodeBlock* parentBlock, CodeBlockInitFlag initFlags)
    : m_context(ctx)
    , m_script(script)
    , m_src(src)
    , m_sourceElementStart(sourceElementStart)
    , m_astNodeStartIndex(astNodeStartIndex)
    , m_identifierOnStackCount(0)
    , m_identifierOnHeapCount(0)
    , m_functionNameIndex(SIZE_MAX)
    , m_functionName(functionName)
    , m_parameterNames(parameterNames)
    , m_parentCodeBlock(parentBlock)
    , m_cachedASTNode(nullptr)
    , m_byteCodeBlock(nullptr)
    , m_nativeFunctionConstructor(nullptr)
#ifndef NDEBUG
    , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , m_scopeContext(nullptr)
#endif
{
    m_isNativeFunction = false;
    m_isStrict = isStrict;
    if (initFlags & CodeBlockInitFlag::CodeBlockHasEval) {
        m_hasEval = true;
    } else {
        m_hasEval = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasWith) {
        m_hasWith = true;
    } else {
        m_hasWith = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasCatch) {
        m_hasCatch = true;
    } else {
        m_hasCatch = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockHasYield) {
        m_hasYield = true;
    } else {
        m_hasYield = false;
    }

    m_usesArgumentsObject = false;
    m_hasArgumentsBinding = false;

    if (initFlags & CodeBlockInitFlag::CodeBlockIsFunctionDeclaration) {
        m_isFunctionDeclaration = true;
    } else {
        m_isFunctionDeclaration = false;
    }

    if (initFlags & CodeBlockInitFlag::CodeBlockIsFunctionExpression) {
        m_isFunctionExpression = true;
    } else {
        m_isFunctionExpression = false;
    }

    m_canUseIndexedVariableStorage = !hasEvalWithCatchYield();

    if (m_canUseIndexedVariableStorage) {
        m_canAllocateEnvironmentOnStack = true;
    } else {
        m_canAllocateEnvironmentOnStack = false;
    }

    for (size_t i = 0; i < innerIdentifiers.size(); i++) {
        IdentifierInfo info;
        info.m_name = innerIdentifiers[i];
        info.m_needToAllocateOnStack = m_canUseIndexedVariableStorage;
        info.m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos.push_back(info);
    }

    m_needsComplexParameterCopy = false;
    m_isInWithScope = false;
}

bool CodeBlock::tryCaptureIdentifiersFromChildCodeBlock(AtomicString name)
{
    for (size_t i = 0; i < m_identifierInfos.size(); i++) {
        if (m_identifierInfos[i].m_name == name) {
            m_canAllocateEnvironmentOnStack = false;
            m_identifierInfos[i].m_needToAllocateOnStack = false;
            return true;
        }
    }
    return false;
}

void CodeBlock::notifySelfOrChildHasEvalWithCatchYield()
{
    m_canAllocateEnvironmentOnStack = false;
    m_canUseIndexedVariableStorage = false;

    for (size_t i = 0; i < m_identifierInfos.size(); i++) {
        m_identifierInfos[i].m_indexForIndexedStorage = SIZE_MAX;
        m_identifierInfos[i].m_needToAllocateOnStack = false;
    }
}

bool CodeBlock::hasNonConfiguableNameOnGlobal(const AtomicString& name)
{
    ASSERT(!inNotIndexedCodeBlockScope());
    CodeBlock* top = this;
    while (top->parentCodeBlock()) {
        top = top->parentCodeBlock();
    }

    if (top->hasName(name)) {
        return true;
    }

    ExecutionState state(m_context);
    auto desc = m_context->globalObject()->getOwnProperty(state, ObjectPropertyName(state, name));
    if (desc.hasValue()) {
        if (!desc.isConfigurable()) {
            return true;
        }
    }
    return false;
}

void CodeBlock::computeVariables()
{
    if (m_usesArgumentsObject) {
        m_canUseIndexedVariableStorage = false;
        m_canAllocateEnvironmentOnStack = false;
    }

    if (inEvalWithCatchYieldScope() || inNotIndexedCodeBlockScope()) {
        m_canAllocateEnvironmentOnStack = false;
    }

    if (m_functionName.string()->length()) {
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == m_functionName) {
                m_functionNameIndex = i;
                break;
            }
        }
        ASSERT(m_functionNameIndex != SIZE_MAX);
    }

    if (canUseIndexedVariableStorage()) {
        size_t s = 0;
        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            if (m_identifierInfos[i].m_name == m_functionName) {
                m_functionNameSaveInfo.m_isAllocatedOnStack = m_identifierInfos[i].m_needToAllocateOnStack;
                if (m_identifierInfos[i].m_needToAllocateOnStack) {
                    m_functionNameSaveInfo.m_index = s;
                } else {
                    m_functionNameSaveInfo.m_index = h;
                }
            }
            if (m_identifierInfos[i].m_needToAllocateOnStack) {
                m_identifierInfos[i].m_indexForIndexedStorage = s;
                s++;
            } else {
                m_identifierInfos[i].m_indexForIndexedStorage = h;
                h++;
            }
        }

        m_identifierOnStackCount = s;
        m_identifierOnHeapCount = h;
    } else {
        m_needsComplexParameterCopy = true;

        size_t h = 0;
        for (size_t i = 0; i < m_identifierInfos.size(); i++) {
            m_identifierInfos[i].m_needToAllocateOnStack = false;

            if (m_identifierInfos[i].m_name == m_functionName) {
                m_functionNameSaveInfo.m_isAllocatedOnStack = false;
                m_functionNameSaveInfo.m_index = h;
            }

            m_identifierInfos[i].m_indexForIndexedStorage = h;
            h++;
        }

        size_t siz = m_parameterNames.size();
        m_parametersInfomation.resizeWithUninitializedValues(siz);
        for (size_t i = 0; i < siz; i++) {
            m_parametersInfomation[i].m_isHeapAllocated = true;
            m_parametersInfomation[i].m_name = m_parameterNames[i];
            m_parametersInfomation[i].m_index = SIZE_MAX;
        }

        m_identifierOnStackCount = 0;
        m_identifierOnHeapCount = h;
    }

    size_t siz = m_parameterNames.size();
    m_parametersInfomation.resizeWithUninitializedValues(siz);
    size_t heapCount = 0;
    size_t stackCount = 0;

    std::vector<std::pair<AtomicString, size_t>> computedNameIndex;

    for (size_t i = 0; i < siz; i++) {
        AtomicString name = m_parameterNames[i];
        bool isHeap = !m_identifierInfos[findName(name)].m_needToAllocateOnStack;
        m_parametersInfomation[i].m_isHeapAllocated = isHeap;
        m_parametersInfomation[i].m_name = name;

        bool computed = false;
        size_t computedIndex = SIZE_MAX;
        for (size_t j = 0; j < computedNameIndex.size(); j++) {
            if (computedNameIndex[j].first == name) {
                // found dup parameter name!
                m_needsComplexParameterCopy = true;
                computed = true;
                computedIndex = computedNameIndex[j].second;
                break;
            }
        }

        if (!computed) {
            if (isHeap) {
                m_parametersInfomation[i].m_index = i - stackCount;
                m_needsComplexParameterCopy = true;
                computedNameIndex.push_back(std::make_pair(name, i - stackCount));
                heapCount++;
            } else {
                m_parametersInfomation[i].m_index = i - heapCount;
                computedNameIndex.push_back(std::make_pair(name, i - heapCount));
                stackCount++;
            }
        } else {
            m_parametersInfomation[i].m_index = computedNameIndex[computedIndex].second;
        }
    }
}
}
