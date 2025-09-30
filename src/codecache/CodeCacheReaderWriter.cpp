/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_CODE_CACHE)

#include "Escargot.h"
#include "runtime/AtomicString.h"
#include "CodeCacheReaderWriter.h"
#include "CodeCache.h"
#include "parser/Script.h"
#include "parser/CodeBlock.h"
#include "interpreter/ByteCode.h"
#include "runtime/Context.h"
#include "runtime/ThreadLocal.h"
#include "runtime/ObjectStructurePropertyName.h"

namespace Escargot {

size_t CacheStringTable::add(const AtomicString& string)
{
    size_t index = 0;
    for (; index < m_table.size(); index++) {
        if (m_table[index] == string) {
            return index;
        }
    }

    m_table.push_back(string);
    ASSERT(index == m_table.size() - 1);

    size_t length = string.string()->length();
    m_maxLength = length > m_maxLength ? length : m_maxLength;

    if (!m_has16BitString && !string.string()->has8BitContent()) {
        m_has16BitString = true;
    }

    return index;
}

void CacheStringTable::initAdd(const AtomicString& string)
{
#ifndef NDEBUG
    for (size_t i = 0; i < m_table.size(); i++) {
        if (m_table[i] == string) {
            ASSERT_NOT_REACHED();
        }
    }
#endif

    m_table.push_back(string);
}

AtomicString& CacheStringTable::get(size_t index)
{
    ASSERT(index < m_table.size());
    return m_table[index];
}

void CodeCacheWriter::CacheBuffer::ensureSize(size_t size)
{
    if (!isAvailable(size)) {
        ComputeReservedCapacityFunctionWithLog2<200> f;
        size_t newCapacity = f(m_capacity + size);

        char* newBuffer = static_cast<char*>(malloc(newCapacity));

        if (LIKELY(!!m_buffer)) {
            memcpy(newBuffer, m_buffer, m_index);
            free(m_buffer);
        }

        m_buffer = newBuffer;
        m_capacity = newCapacity;
    }
}

void CodeCacheWriter::CacheBuffer::reset()
{
    free(m_buffer);

    m_buffer = nullptr;
    m_capacity = 0;
    m_index = 0;
}

void CodeCacheWriter::storeInterpretedCodeBlock(InterpretedCodeBlock* codeBlock)
{
    ASSERT(GC_is_disabled());
    ASSERT(!!codeBlock && codeBlock->isInterpretedCodeBlock());

    std::unordered_map<InterpretedCodeBlock*, size_t, std::hash<void*>, std::equal_to<void*>, std::allocator<std::pair<InterpretedCodeBlock* const, size_t>>>& codeBlockIndex = m_codeBlockCacheInfo->m_codeBlockIndex;

    size_t size;

    m_buffer.ensureSize(sizeof(bool) + 4 * sizeof(size_t));

    // represent if InterpretedCodeBlockWithRareData
    bool hasRareData = codeBlock->hasRareData() ? true : false;
    m_buffer.put(hasRareData);

    {
        // InterpretedCodeBlock::m_src
        size_t start = codeBlock->src().start();
        size_t end = codeBlock->src().end();
        m_buffer.put(start);
        m_buffer.put(end);
    }

    // InterpretedCodeBlock::m_parent
    size = SIZE_MAX;
    if (codeBlock->parent()) {
        ASSERT(codeBlockIndex.find(codeBlock->parent()) != codeBlockIndex.end());
        size = codeBlockIndex.find(codeBlock->parent())->second;
    }
    m_buffer.put(size);

    // InterpretedCodeBlock::m_children
    size = codeBlock->hasChildren() ? codeBlock->children().size() : 0;
    m_buffer.put(size);
    m_buffer.ensureSize(size * sizeof(size_t));
    for (size_t i = 0; i < size; i++) {
        ASSERT(codeBlockIndex.find(codeBlock->children()[i]) != codeBlockIndex.end());
        size_t childIndex = codeBlockIndex.find(codeBlock->children()[i])->second;
        m_buffer.put(childIndex);
    }

    // InterpretedCodeBlock::m_parameterNames
    const AtomicStringTightVector& atomicStringVector = codeBlock->m_parameterNames;
    size = atomicStringVector.size();
    m_buffer.ensureSize((size + 1) * sizeof(size_t));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        const size_t stringIndex = m_stringTable->add(atomicStringVector[i]);
        m_buffer.put(stringIndex);
    }

    // InterpretedCodeBlock::m_parameterUsed
    const InterpretedCodeBlock::ParameterUsedVector& parameterUsedVector = codeBlock->m_parameterUsed;
    size = parameterUsedVector.size();
    m_buffer.ensureSize(sizeof(size_t) + size * (sizeof(size_t) + sizeof(bool)));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        const InterpretedCodeBlock::ParameterUsed& info = parameterUsedVector[i];
        m_buffer.put(m_stringTable->add(info.m_name));
        m_buffer.put(info.m_isUsed);
    }

    // InterpretedCodeBlock::m_identifierInfos
    const InterpretedCodeBlock::IdentifierInfoVector& identifierVector = codeBlock->m_identifierInfos;
    size = identifierVector.size();
    m_buffer.ensureSize(sizeof(size_t) + size * (5 * sizeof(bool) + 2 * sizeof(size_t)));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        const InterpretedCodeBlock::IdentifierInfo& info = identifierVector[i];
        m_buffer.put(info.m_needToAllocateOnStack);
        m_buffer.put(info.m_isMutable);
        m_buffer.put(info.m_isParameterName);
        m_buffer.put(info.m_isExplicitlyDeclaredOrParameterName);
        m_buffer.put(info.m_isVarDeclaration);
        m_buffer.put(info.m_indexForIndexedStorage);
        m_buffer.put(m_stringTable->add(info.m_name));
    }

    // InterpretedCodeBlock::m_blockInfos
    InterpretedCodeBlock::BlockInfo** blockInfoVector = codeBlock->m_blockInfos;
    size = codeBlock->m_blockInfosLength;
    m_buffer.ensureSize(sizeof(size_t));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        m_buffer.ensureSize(2 * sizeof(bool) + 3 * sizeof(uint16_t));
        const InterpretedCodeBlock::BlockInfo* info = blockInfoVector[i];
        m_buffer.put(info->canAllocateEnvironmentOnStack());
        m_buffer.put(info->shouldAllocateEnvironment());
        m_buffer.put(info->fromCatchClauseNode());
        m_buffer.put((uint16_t)info->parentBlockIndex());
        m_buffer.put((uint16_t)info->blockIndex());

        // InterpretedCodeBlock::BlockInfo::m_identifiers
        const InterpretedCodeBlock::BlockIdentifierInfoVector& infoVector = info->identifiers();
        const size_t vectorSize = infoVector.size();
        m_buffer.ensureSize(sizeof(size_t) + vectorSize * (3 * sizeof(bool) + 2 * sizeof(size_t)));
        m_buffer.put(vectorSize);
        for (size_t j = 0; j < vectorSize; j++) {
            const InterpretedCodeBlock::BlockIdentifierInfo& info = infoVector[j];
            m_buffer.put(info.m_needToAllocateOnStack);
            m_buffer.put(info.m_isMutable);
            m_buffer.put(info.m_isUsing);
            m_buffer.put(info.m_indexForIndexedStorage);
            m_buffer.put(m_stringTable->add(info.m_name));
        }
    }

    // InterpretedCodeBlock::m_functionName
    m_buffer.ensureSize(sizeof(size_t));
    m_buffer.put(m_stringTable->add(codeBlock->m_functionName));

    // InterpretedCodeBlock::m_functionStart
    m_buffer.ensureSize(sizeof(ExtendedNodeLOC));
    m_buffer.put(codeBlock->m_functionStart);

#ifndef NDEBUG
    // InterpretedCodeBlock::m_bodyEndLOC
    m_buffer.ensureSize(sizeof(ExtendedNodeLOC));
    m_buffer.put(codeBlock->m_bodyEndLOC);
#endif

    m_buffer.ensureSize(5 * sizeof(uint16_t));
    m_buffer.put(codeBlock->m_functionLength);
    m_buffer.put(codeBlock->m_parameterCount);
    m_buffer.put(codeBlock->m_identifierOnStackCount);
    m_buffer.put(codeBlock->m_identifierOnHeapCount);
    m_buffer.put(codeBlock->m_lexicalBlockStackAllocatedIdentifierMaximumDepth);

    m_buffer.ensureSize(2 * sizeof(LexicalBlockIndex));
    m_buffer.put(codeBlock->m_functionBodyBlockIndex);
    m_buffer.put(codeBlock->m_lexicalBlockIndexFunctionLocatedIn);

    m_buffer.ensureSize(31 * sizeof(bool));
    m_buffer.put(codeBlock->m_isFunctionNameUsedBySelf);
    m_buffer.put(codeBlock->m_isFunctionNameSaveOnHeap);
    m_buffer.put(codeBlock->m_isFunctionNameExplicitlyDeclared);
    m_buffer.put(codeBlock->m_canUseIndexedVariableStorage);
    m_buffer.put(codeBlock->m_canAllocateVariablesOnStack);
    m_buffer.put(codeBlock->m_canAllocateEnvironmentOnStack);
    m_buffer.put(codeBlock->m_hasDescendantUsesNonIndexedVariableStorage);
    m_buffer.put(codeBlock->m_hasEval);
    m_buffer.put(codeBlock->m_hasWith);
    m_buffer.put(codeBlock->m_isStrict);
    m_buffer.put(codeBlock->m_inWith);
    m_buffer.put(codeBlock->m_isEvalCode);
    m_buffer.put(codeBlock->m_isEvalCodeInFunction);
    m_buffer.put(codeBlock->m_usesArgumentsObject);
    m_buffer.put(codeBlock->m_isFunctionExpression);
    m_buffer.put(codeBlock->m_isFunctionDeclaration);
    m_buffer.put(codeBlock->m_isArrowFunctionExpression);
    m_buffer.put(codeBlock->m_isOneExpressionOnlyVirtualArrowFunctionExpression);
    m_buffer.put(codeBlock->m_isClassConstructor);
    m_buffer.put(codeBlock->m_isDerivedClassConstructor);
    m_buffer.put(codeBlock->m_isObjectMethod);
    m_buffer.put(codeBlock->m_isClassMethod);
    m_buffer.put(codeBlock->m_isClassStaticMethod);
    m_buffer.put(codeBlock->m_isGenerator);
    m_buffer.put(codeBlock->m_isAsync);
    m_buffer.put(codeBlock->m_needsVirtualIDOperation);
    m_buffer.put(codeBlock->m_hasArrowParameterPlaceHolder);
    m_buffer.put(codeBlock->m_hasParameterOtherThanIdentifier);
    m_buffer.put(codeBlock->m_allowSuperCall);
    m_buffer.put(codeBlock->m_allowSuperProperty);
    m_buffer.put(codeBlock->m_allowArguments);

    // InterpretedCodeBlockWithRareData
    if (UNLIKELY(hasRareData)) {
        ASSERT(codeBlock->taggedTemplateLiteralCache().size() == 0);

        if (auto varMap = codeBlock->identifierInfoMap()) {
            size_t mapSize = varMap->size();
            m_buffer.ensureSize((1 + mapSize * 2) * sizeof(size_t));
            m_buffer.put(mapSize);
            for (auto iter = varMap->begin(); iter != varMap->end(); iter++) {
                size_t stringIndex = m_stringTable->add(iter->first);
                size_t index = iter->second;
                m_buffer.put(stringIndex);
                m_buffer.put(index);
            }
        } else {
            m_buffer.ensureSize(sizeof(size_t));
            m_buffer.put((size_t)0);
        }

        if (auto privateNameVector = codeBlock->classPrivateNames()) {
            size_t nameSize = privateNameVector->size();
            m_buffer.ensureSize((1 + nameSize) * sizeof(size_t));
            m_buffer.put(nameSize);
            for (size_t i = 0; i < nameSize; i++) {
                size_t stringIndex = m_stringTable->add(privateNameVector->data()[i]);
                m_buffer.put(stringIndex);
            }
        } else {
            m_buffer.ensureSize(sizeof(size_t));
            m_buffer.put((size_t)0);
        }
    }
}

void CodeCacheWriter::storeByteCodeBlock(ByteCodeBlock* block)
{
    ASSERT(GC_is_disabled());
    ASSERT(!!block);

    size_t size = 0;

    m_buffer.ensureSize(2 * sizeof(bool) + 2 * sizeof(uint16_t));
    m_buffer.put(block->m_shouldClearStack);
    m_buffer.put(block->m_isOwnerMayFreed);
    m_buffer.put(block->m_needsExtendedExecutionState);
    m_buffer.put((uint16_t)block->m_requiredOperandRegisterNumber);
    m_buffer.put((uint16_t)block->m_requiredTotalRegisterNumber);

    // ByteCodeBlock::m_numeralLiteralData
    ByteCodeNumeralLiteralData& numeralLiteralData = block->m_numeralLiteralData;
    m_buffer.putData(numeralLiteralData.data(), numeralLiteralData.size());

    // ByteCodeBlock::m_jumpFlowRecordData
    ByteCodeJumpFlowRecordData& jumpFlowRecordData = block->m_jumpFlowRecordData;
    m_buffer.putData(jumpFlowRecordData.data(), jumpFlowRecordData.size());

    // ByteCodeBlock::m_stringLiteralData
    ByteCodeStringLiteralData& stringLiteralData = block->m_stringLiteralData;
    size = stringLiteralData.size();
    m_buffer.ensureSize(sizeof(size_t));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        m_buffer.putString(stringLiteralData[i]);
    }

    // ByteCodeBlock::m_otherLiteralData
    // Note) only BigInt exists
    ByteCodeOtherLiteralData& bigIntData = block->m_otherLiteralData;
    size = bigIntData.size();
    m_buffer.ensureSize(sizeof(size_t));
    m_buffer.put(size);
    for (size_t i = 0; i < size; i++) {
        ASSERT(static_cast<PointerValue*>(bigIntData[i])->isBigInt());
        bf_t* bf = static_cast<PointerValue*>(bigIntData[i])->asBigInt()->bf();
        m_buffer.putBF(bf);
    }

    // ByteCodeBlock::m_code bytecode stream
    storeByteCodeStream(block);

    // Do not store m_inlineCacheDataSize and m_locData
    // these members are used during the runtime
    ASSERT(block->m_inlineCacheDataSize == 0);
}

void CodeCacheWriter::storeStringTable()
{
    ASSERT(!!m_stringTable);

    Vector<AtomicString, std::allocator<AtomicString>>& table = m_stringTable->table();

    bool has16BitString = m_stringTable->has16BitString();
    size_t maxLength = m_stringTable->maxLength();
    size_t tableSize = table.size();

    m_buffer.ensureSize(sizeof(bool) + 2 * sizeof(size_t));
    m_buffer.put(has16BitString);
    m_buffer.put(maxLength);
    m_buffer.put(tableSize);

    if (LIKELY(!has16BitString)) {
        for (size_t i = 0; i < tableSize; i++) {
            String* string = table[i].string();
            ASSERT(string->has8BitContent());

            m_buffer.putData(string->characters8(), string->length());
        }
    } else {
        for (size_t i = 0; i < tableSize; i++) {
            String* string = table[i].string();
            bool is8Bit = string->has8BitContent();

            m_buffer.ensureSize(sizeof(bool));
            m_buffer.put(is8Bit);
            if (is8Bit) {
                m_buffer.putData(string->characters8(), string->length());
            } else {
                m_buffer.putData(string->characters16(), string->length());
            }
        }
    }
}

#define STORE_ATOMICSTRING_RELOC(member)                 \
    size_t stringIndex = m_stringTable->add(bc->member); \
    relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_ATOMICSTRING, (size_t)currentCode - codeBase, stringIndex));

void CodeCacheWriter::storeByteCodeStream(ByteCodeBlock* block)
{
    ByteCodeBlockData& byteCodeStream = block->m_code;
    ASSERT(byteCodeStream.size() > 0);

    // store ByteCode stream
    m_buffer.putData(byteCodeStream.data(), byteCodeStream.size());

    Vector<ByteCodeRelocInfo, std::allocator<ByteCodeRelocInfo>> relocInfoVector;
    ByteCodeStringLiteralData& stringLiteralData = block->m_stringLiteralData;
    ByteCodeOtherLiteralData& bigIntData = block->m_otherLiteralData;

    // mark bytecode relocation infos
    {
        uint8_t* code = byteCodeStream.data();
        size_t codeBase = (size_t)code;
        uint8_t* end = code + byteCodeStream.size();

        Context* context = block->codeBlock()->context();

        while (code < end) {
            ByteCode* currentCode = reinterpret_cast<ByteCode*>(code);
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
            Opcode opcode = (Opcode)(size_t)currentCode->m_opcodeInAddress;
#else
            Opcode opcode = currentCode->m_opcode;
#endif
            switch (opcode) {
            case LoadLiteralOpcode: {
                LoadLiteral* bc = static_cast<LoadLiteral*>(currentCode);
                Value value = bc->m_value;
                if (value.isPointerValue()) {
                    if (LIKELY(value.asPointerValue()->isString())) {
                        String* string = value.asPointerValue()->asString();
                        if (UNLIKELY(!string->length())) {
                            relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, SIZE_MAX));
                        } else {
                            size_t stringIndex = VectorUtil::findInVector(stringLiteralData, string);
                            if (stringIndex != VectorUtil::invalidIndex) {
                                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, stringIndex));
                            } else {
                                ASSERT(string->isAtomicStringSource());
                                stringIndex = m_stringTable->add(AtomicString(context, string));
                                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_ATOMICSTRING, (size_t)currentCode - codeBase, stringIndex));
                            }
                        }
                    } else if (value.asPointerValue()->isBigInt()) {
                        BigInt* bigInt = value.asPointerValue()->asBigInt();
                        size_t bigIntIndex = VectorUtil::findInVector(bigIntData, bigInt);
                        ASSERT(bigIntIndex != VectorUtil::invalidIndex);
                        relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_BIGINT, (size_t)currentCode - codeBase, bigIntIndex));
                    } else {
                        ASSERT(value.asPointerValue()->isFunctionObject() && value.asPointerValue() == context->globalObject()->objectFreeze());
                        relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_FREEZEFUNC, (size_t)currentCode - codeBase, SIZE_MAX));
                    }
                }
                break;
            }
            case LoadByNameOpcode: {
                LoadByName* bc = static_cast<LoadByName*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case StoreByNameOpcode: {
                StoreByName* bc = static_cast<StoreByName*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case InitializeByNameOpcode: {
                InitializeByName* bc = static_cast<InitializeByName*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case CreateFunctionOpcode: {
                CreateFunction* bc = static_cast<CreateFunction*>(currentCode);
                InterpretedCodeBlock* codeBlock = bc->m_codeBlock;
                ASSERT(!!codeBlock);
                size_t codeBlockIndex = VectorUtil::findInVector(block->codeBlock()->children(), codeBlock);
                ASSERT(codeBlockIndex != VectorUtil::invalidIndex);
                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_CODEBLOCK, (size_t)currentCode - codeBase, codeBlockIndex));
                break;
            }
            case InitializeClassOpcode: {
                InitializeClass* bc = static_cast<InitializeClass*>(currentCode);
                if (bc->m_stage == InitializeClass::CreateClass) {
                    InterpretedCodeBlock* codeBlock = bc->m_codeBlock;
                    if (codeBlock) {
                        size_t codeBlockIndex = VectorUtil::findInVector(block->codeBlock()->children(), codeBlock);
                        ASSERT(codeBlockIndex != VectorUtil::invalidIndex);
                        relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_CODEBLOCK, (size_t)currentCode - codeBase, codeBlockIndex));
                    }

                    String* string = bc->m_classSrc;
                    ASSERT(!!string && string->length() > 0);
                    size_t stringIndex = VectorUtil::findInVector(stringLiteralData, string);

                    ASSERT(stringIndex != VectorUtil::invalidIndex);
                    relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, stringIndex));

                    String* name = bc->m_name;
                    if (name) {
                        ASSERT(name->isAtomicStringSource());
                        size_t nameIndex = m_stringTable->add(AtomicString(context, name));
                        relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_ATOMICSTRING, (size_t)currentCode - codeBase, nameIndex));
                    }
                }
                break;
            }
            case ComplexSetObjectOperationOpcode: {
                ComplexSetObjectOperation* bc = static_cast<ComplexSetObjectOperation*>(currentCode);
                if (bc->m_type == ComplexSetObjectOperation::Private) {
                    STORE_ATOMICSTRING_RELOC(m_propertyName);
                }
                break;
            }
            case ComplexGetObjectOperationOpcode: {
                ComplexGetObjectOperation* bc = static_cast<ComplexGetObjectOperation*>(currentCode);
                if (bc->m_type == ComplexGetObjectOperation::Private) {
                    STORE_ATOMICSTRING_RELOC(m_propertyName);
                }
                break;
            }
            case ObjectDefineOwnPropertyWithNameOperationOpcode: {
                ObjectDefineOwnPropertyWithNameOperation* bc = static_cast<ObjectDefineOwnPropertyWithNameOperation*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case InitializeGlobalVariableOpcode: {
                InitializeGlobalVariable* bc = static_cast<InitializeGlobalVariable*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_variableName);
                break;
            }
            case UnaryTypeofOpcode: {
                UnaryTypeof* bc = static_cast<UnaryTypeof*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_id);
                break;
            }
            case UnaryDeleteOpcode: {
                UnaryDelete* bc = static_cast<UnaryDelete*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_id);
                break;
            }
            case CallComplexCaseOpcode: {
                CallComplexCase* bc = static_cast<CallComplexCase*>(currentCode);
                if (bc->m_kind == CallComplexCase::InWithScope) {
                    STORE_ATOMICSTRING_RELOC(m_calleeName);
                }
                break;
            }
            case GetMethodOpcode: {
                GetMethod* bc = static_cast<GetMethod*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case LoadRegExpOpcode: {
                LoadRegExp* bc = static_cast<LoadRegExp*>(currentCode);

                String* bodyString = bc->m_body;
                String* optionString = bc->m_option;
                ASSERT(!!bodyString && bodyString->length() > 0);
                ASSERT(!!optionString);

                size_t bodyIndex = VectorUtil::findInVector(stringLiteralData, bodyString);
                ASSERT(bodyIndex != VectorUtil::invalidIndex);
                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, bodyIndex));

                if (optionString->length()) {
                    size_t optionIndex = VectorUtil::findInVector(stringLiteralData, optionString);
                    ASSERT(optionIndex != VectorUtil::invalidIndex);
                    relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, optionIndex));
                } else {
                    relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_STRING, (size_t)currentCode - codeBase, SIZE_MAX));
                }
                break;
            }
            case BlockOperationOpcode: {
                BlockOperation* bc = static_cast<BlockOperation*>(currentCode);
                InterpretedCodeBlock::BlockInfo* info = reinterpret_cast<InterpretedCodeBlock::BlockInfo*>(bc->m_blockInfo);
                size_t infoIndex = ArrayUtil::findInArray(block->codeBlock()->m_blockInfos, block->codeBlock()->m_blockInfosLength, info);
                ASSERT(infoIndex != ArrayUtil::invalidIndex);
                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_BLOCKINFO, (size_t)currentCode - codeBase, infoIndex));
                break;
            }
            case ReplaceBlockLexicalEnvironmentOperationOpcode: {
                ReplaceBlockLexicalEnvironmentOperation* bc = static_cast<ReplaceBlockLexicalEnvironmentOperation*>(currentCode);
                InterpretedCodeBlock::BlockInfo* info = reinterpret_cast<InterpretedCodeBlock::BlockInfo*>(bc->m_blockInfo);
                size_t infoIndex = ArrayUtil::findInArray(block->codeBlock()->m_blockInfos, block->codeBlock()->m_blockInfosLength, info);
                ASSERT(infoIndex != ArrayUtil::invalidIndex);
                relocInfoVector.push_back(ByteCodeRelocInfo(ByteCodeRelocType::RELOC_BLOCKINFO, (size_t)currentCode - codeBase, infoIndex));
                break;
            }
            case ResolveNameAddressOpcode: {
                ResolveNameAddress* bc = static_cast<ResolveNameAddress*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case StoreByNameWithAddressOpcode: {
                StoreByNameWithAddress* bc = static_cast<StoreByNameWithAddress*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case GetObjectPreComputedCaseOpcode: {
                GetObjectPreComputedCase* bc = static_cast<GetObjectPreComputedCase*>(currentCode);
                ASSERT(!bc->hasInlineCache());
                ASSERT(bc->m_propertyName.hasAtomicString());
                STORE_ATOMICSTRING_RELOC(m_propertyName.asAtomicString());
                break;
            }
            case SetObjectPreComputedCaseOpcode: {
                SetObjectPreComputedCase* bc = static_cast<SetObjectPreComputedCase*>(currentCode);
                ASSERT(!bc->m_inlineCache);
                ASSERT(bc->m_propertyName.hasAtomicString());
                STORE_ATOMICSTRING_RELOC(m_propertyName.asAtomicString());
                break;
            }
            case GetGlobalVariableOpcode: {
                GetGlobalVariable* bc = static_cast<GetGlobalVariable*>(currentCode);
                ASSERT(!!bc->m_slot);
                STORE_ATOMICSTRING_RELOC(m_slot->m_propertyName);
                break;
            }
            case SetGlobalVariableOpcode: {
                SetGlobalVariable* bc = static_cast<SetGlobalVariable*>(currentCode);
                ASSERT(!!bc->m_slot);
                STORE_ATOMICSTRING_RELOC(m_slot->m_propertyName);
                break;
            }
            case ThrowStaticErrorOperationOpcode: {
                ThrowStaticErrorOperation* bc = static_cast<ThrowStaticErrorOperation*>(currentCode);
                STORE_ATOMICSTRING_RELOC(m_templateDataString);
                break;
            }
            case GetObjectPreComputedCaseSimpleInlineCacheOpcode:
            case ExecutionPauseOpcode: {
                // add tail data length
                ExecutionPause* bc = static_cast<ExecutionPause*>(currentCode);
                if (bc->m_reason == ExecutionPause::Reason::Yield) {
                    code += bc->m_yieldData.m_tailDataLength;
                } else if (bc->m_reason == ExecutionPause::Reason::Await) {
                    code += bc->m_awaitData.m_tailDataLength;
                } else if (bc->m_reason == ExecutionPause::Reason::GeneratorsInitialize) {
                    code += bc->m_asyncGeneratorInitializeData.m_tailDataLength;
                }
                break;
            }
            case FinalizeDisposableOpcode: {
                // add tail data length
                FinalizeDisposable* bc = static_cast<FinalizeDisposable*>(currentCode);
                code += bc->m_tailDataLength;
                break;
            }
            case ExecutionResumeOpcode:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            default:
                break;
            }

            ASSERT(opcode <= EndOpcode);
            code += byteCodeLengths[opcode];
        }
    }

    // store relocInfoVector
    {
        m_buffer.putData(relocInfoVector.data(), relocInfoVector.size());
    }
}

void CodeCacheReader::CacheBuffer::resize(size_t size)
{
    ASSERT(!m_buffer && m_capacity == 0 && m_index == 0);

    m_buffer = static_cast<char*>(malloc(size));
    m_capacity = size;
}

void CodeCacheReader::CacheBuffer::reset()
{
    if (m_buffer) {
        free(m_buffer);
        m_buffer = nullptr;
    }
    m_capacity = 0;
    m_index = 0;
}

bool CodeCacheReader::loadData(FILE* file, size_t size)
{
    m_buffer.resize(size);
    if (UNLIKELY(fread((void*)bufferData(), sizeof(char), size, file) != size)) {
        clearBuffer();
        return false;
    }

    return true;
}

InterpretedCodeBlock* CodeCacheReader::loadInterpretedCodeBlock(Context* context, Script* script)
{
    ASSERT(!!context);
    ASSERT(GC_is_disabled());

    size_t size;
    bool hasRareData = m_buffer.get<bool>();

    InterpretedCodeBlock* codeBlock = InterpretedCodeBlock::createInterpretedCodeBlock(context, script, hasRareData);

    {
        // InterpretedCodeBlock::m_src
        size_t start = m_buffer.get<size_t>();
        size_t end = m_buffer.get<size_t>();
        codeBlock->m_src = StringView(script->sourceCode(), start, end);
    }

    // InterpretedCodeBlock::m_parent
    size = m_buffer.get<size_t>();
    codeBlock->m_parent = reinterpret_cast<InterpretedCodeBlock*>(size);

    // InterpretedCodeBlock::m_children
    ASSERT(!codeBlock->hasChildren());
    size = m_buffer.get<size_t>();
    if (size) {
        size_t vectorSize = size;
        InterpretedCodeBlockVector* codeBlockVector = new InterpretedCodeBlockVector();
        codeBlockVector->resizeWithUninitializedValues(vectorSize);
        for (size_t i = 0; i < vectorSize; i++) {
            size_t childIndex = m_buffer.get<size_t>();
            (*codeBlockVector)[i] = reinterpret_cast<InterpretedCodeBlock*>(childIndex);
        }
        codeBlock->setChildren(codeBlockVector);
    }

    // InterpretedCodeBlock::m_parameterNames
    AtomicStringTightVector& atomicStringVector = codeBlock->m_parameterNames;
    size = m_buffer.get<size_t>();
    atomicStringVector.resizeWithUninitializedValues(size);
    for (size_t i = 0; i < size; i++) {
        size_t stringIndex = m_buffer.get<size_t>();
        atomicStringVector[i] = m_stringTable->get(stringIndex);
    }

    // InterpretedCodeBlock::m_parameterUsed
    InterpretedCodeBlock::ParameterUsedVector& parameterUsedVector = codeBlock->m_parameterUsed;
    size = m_buffer.get<size_t>();
    parameterUsedVector.resizeWithUninitializedValues(size);
    for (size_t i = 0; i < size; i++) {
        InterpretedCodeBlock::ParameterUsed info;
        info.m_name = m_stringTable->get(m_buffer.get<size_t>());
        info.m_isUsed = m_buffer.get<bool>();
        parameterUsedVector[i] = info;
    }

    // InterpretedCodeBlock::m_identifierInfos
    InterpretedCodeBlock::IdentifierInfoVector& identifierVector = codeBlock->m_identifierInfos;
    size = m_buffer.get<size_t>();
    identifierVector.resizeWithUninitializedValues(size);
    for (size_t i = 0; i < size; i++) {
        InterpretedCodeBlock::IdentifierInfo info;
        info.m_needToAllocateOnStack = m_buffer.get<bool>();
        info.m_isMutable = m_buffer.get<bool>();
        info.m_isParameterName = m_buffer.get<bool>();
        info.m_isExplicitlyDeclaredOrParameterName = m_buffer.get<bool>();
        info.m_isVarDeclaration = m_buffer.get<bool>();
        info.m_indexForIndexedStorage = m_buffer.get<size_t>();
        info.m_name = m_stringTable->get(m_buffer.get<size_t>());
        identifierVector[i] = info;
    }

    size = m_buffer.get<size_t>();
    InterpretedCodeBlock::BlockInfo** blockInfoVector = codeBlock->m_blockInfos = (InterpretedCodeBlock::BlockInfo**)GC_MALLOC(sizeof(InterpretedCodeBlock::BlockInfo*) * size);
    codeBlock->m_blockInfosLength = size;
    for (size_t i = 0; i < size; i++) {
        bool canAllocateEnvironmentOnStack = m_buffer.get<bool>();
        bool shouldAllocateEnvironment = m_buffer.get<bool>();
        bool fromCatchClauseNode = m_buffer.get<bool>();
        LexicalBlockIndex parentBlockIndex = (LexicalBlockIndex)m_buffer.get<uint16_t>();
        LexicalBlockIndex blockIndex = (LexicalBlockIndex)m_buffer.get<uint16_t>();
        size_t vectorSize = m_buffer.get<size_t>();

        InterpretedCodeBlock::BlockInfo* info = InterpretedCodeBlock::BlockInfo::create(
            canAllocateEnvironmentOnStack,
            shouldAllocateEnvironment,
            fromCatchClauseNode,
            parentBlockIndex,
            blockIndex,
            vectorSize);
        if (vectorSize) {
            ASSERT(!info->isGenericBlockInfo());
            info->identifiers().resizeWithUninitializedValues(vectorSize);
            for (size_t j = 0; j < vectorSize; j++) {
                InterpretedCodeBlock::BlockIdentifierInfo idInfo;
                idInfo.m_needToAllocateOnStack = m_buffer.get<bool>();
                idInfo.m_isMutable = m_buffer.get<bool>();
                idInfo.m_isUsing = m_buffer.get<bool>();
                idInfo.m_indexForIndexedStorage = m_buffer.get<size_t>();
                idInfo.m_name = m_stringTable->get(m_buffer.get<size_t>());
                info->identifiers()[j] = idInfo;
            }
        }

        blockInfoVector[i] = info;
    }

    // InterpretedCodeBlock::m_functionName
    codeBlock->m_functionName = m_stringTable->get(m_buffer.get<size_t>());

    // InterpretedCodeBlock::m_functionStart
    codeBlock->m_functionStart = m_buffer.get<ExtendedNodeLOC>();

#ifndef NDEBUG
    // InterpretedCodeBlock::m_bodyEndLOC
    codeBlock->m_bodyEndLOC = m_buffer.get<ExtendedNodeLOC>();
#endif

    codeBlock->m_functionLength = m_buffer.get<uint16_t>();
    codeBlock->m_parameterCount = m_buffer.get<uint16_t>();
    codeBlock->m_identifierOnStackCount = m_buffer.get<uint16_t>();
    codeBlock->m_identifierOnHeapCount = m_buffer.get<uint16_t>();
    codeBlock->m_lexicalBlockStackAllocatedIdentifierMaximumDepth = m_buffer.get<uint16_t>();

    codeBlock->m_functionBodyBlockIndex = m_buffer.get<LexicalBlockIndex>();
    codeBlock->m_lexicalBlockIndexFunctionLocatedIn = m_buffer.get<LexicalBlockIndex>();

    codeBlock->m_isFunctionNameUsedBySelf = m_buffer.get<bool>();
    codeBlock->m_isFunctionNameSaveOnHeap = m_buffer.get<bool>();
    codeBlock->m_isFunctionNameExplicitlyDeclared = m_buffer.get<bool>();
    codeBlock->m_canUseIndexedVariableStorage = m_buffer.get<bool>();
    codeBlock->m_canAllocateVariablesOnStack = m_buffer.get<bool>();
    codeBlock->m_canAllocateEnvironmentOnStack = m_buffer.get<bool>();
    codeBlock->m_hasDescendantUsesNonIndexedVariableStorage = m_buffer.get<bool>();
    codeBlock->m_hasEval = m_buffer.get<bool>();
    codeBlock->m_hasWith = m_buffer.get<bool>();
    codeBlock->m_isStrict = m_buffer.get<bool>();
    codeBlock->m_inWith = m_buffer.get<bool>();
    codeBlock->m_isEvalCode = m_buffer.get<bool>();
    codeBlock->m_isEvalCodeInFunction = m_buffer.get<bool>();
    codeBlock->m_usesArgumentsObject = m_buffer.get<bool>();
    codeBlock->m_isFunctionExpression = m_buffer.get<bool>();
    codeBlock->m_isFunctionDeclaration = m_buffer.get<bool>();
    codeBlock->m_isArrowFunctionExpression = m_buffer.get<bool>();
    codeBlock->m_isOneExpressionOnlyVirtualArrowFunctionExpression = m_buffer.get<bool>();
    codeBlock->m_isClassConstructor = m_buffer.get<bool>();
    codeBlock->m_isDerivedClassConstructor = m_buffer.get<bool>();
    codeBlock->m_isObjectMethod = m_buffer.get<bool>();
    codeBlock->m_isClassMethod = m_buffer.get<bool>();
    codeBlock->m_isClassStaticMethod = m_buffer.get<bool>();
    codeBlock->m_isGenerator = m_buffer.get<bool>();
    codeBlock->m_isAsync = m_buffer.get<bool>();
    codeBlock->m_needsVirtualIDOperation = m_buffer.get<bool>();
    codeBlock->m_hasArrowParameterPlaceHolder = m_buffer.get<bool>();
    codeBlock->m_hasParameterOtherThanIdentifier = m_buffer.get<bool>();
    codeBlock->m_allowSuperCall = m_buffer.get<bool>();
    codeBlock->m_allowSuperProperty = m_buffer.get<bool>();
    codeBlock->m_allowArguments = m_buffer.get<bool>();

    // InterpretedCodeBlockWithRareData
    if (UNLIKELY(hasRareData)) {
        ASSERT(codeBlock->hasRareData());
        ASSERT(codeBlock->taggedTemplateLiteralCache().size() == 0);
        ASSERT(!codeBlock->identifierInfoMap());

        if (size_t mapSize = m_buffer.get<size_t>()) {
            FunctionContextVarMap* varMap = new (GC) FunctionContextVarMap();
            for (size_t i = 0; i < mapSize; i++) {
                size_t stringIndex = m_buffer.get<size_t>();
                size_t index = m_buffer.get<size_t>();
                varMap->insert(std::make_pair(m_stringTable->get(stringIndex), index));
            }
            codeBlock->rareData()->m_identifierInfoMap = varMap;
        }

        if (size_t privateNameSize = m_buffer.get<size_t>()) {
            AtomicStringTightVector* nameVector = new AtomicStringTightVector();
            nameVector->resizeWithUninitializedValues(privateNameSize);
            for (size_t i = 0; i < privateNameSize; i++) {
                size_t stringIndex = m_buffer.get<size_t>();
                auto name = m_stringTable->get(stringIndex);
                (*nameVector)[i] = name;
            }
            codeBlock->rareData()->m_classPrivateNames = nameVector;
        }
    }

    return codeBlock;
}

ByteCodeBlock* CodeCacheReader::loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock)
{
    ASSERT(GC_is_disabled());
    ASSERT(!!topCodeBlock);

    size_t size;
    ByteCodeBlock* block = new ByteCodeBlock(topCodeBlock);

    block->m_shouldClearStack = m_buffer.get<bool>();
    block->m_isOwnerMayFreed = m_buffer.get<bool>();
    block->m_needsExtendedExecutionState = m_buffer.get<bool>();
    block->m_requiredOperandRegisterNumber = m_buffer.get<uint16_t>();
    block->m_requiredTotalRegisterNumber = m_buffer.get<uint16_t>();

    // ByteCodeBlock::m_numeralLiteralData
    ByteCodeNumeralLiteralData& numeralLiteralData = block->m_numeralLiteralData;
    size = m_buffer.get<size_t>();
    numeralLiteralData.resizeFitWithUninitializedValues(size);
    m_buffer.getData(numeralLiteralData.data(), size);

    // ByteCodeBlock::m_jumpFlowRecordData
    ByteCodeJumpFlowRecordData& jumpFlowRecordData = block->m_jumpFlowRecordData;
    size = m_buffer.get<size_t>();
    jumpFlowRecordData.resizeFitWithUninitializedValues(size);
    m_buffer.getData(jumpFlowRecordData.data(), size);

    // ByteCodeBlock::m_stringLiteralData
    ByteCodeStringLiteralData& stringLiteralData = block->m_stringLiteralData;
    size = m_buffer.get<size_t>();
    stringLiteralData.resizeFitWithUninitializedValues(size);
    for (size_t i = 0; i < size; i++) {
        stringLiteralData[i] = m_buffer.getString();
    }

    // ByteCodeBlock::m_otherLiteralData
    // Note) only BigInt exists
    ByteCodeOtherLiteralData& bigIntData = block->m_otherLiteralData;
    size = m_buffer.get<size_t>();
    bigIntData.resizeFitWithUninitializedValues(size);
    for (size_t i = 0; i < size; i++) {
        bigIntData[i] = new BigInt(m_buffer.getBF(ThreadLocal::bfContext()));
    }

    // ByteCodeBlock::m_code bytecode stream
    loadByteCodeStream(context, block);

    // finally, relocate opcode address and register index for each bytecode
    ByteCodeGenerator::relocateByteCode(block);

    return block;
}

CacheStringTable* CodeCacheReader::loadStringTable(Context* context)
{
    CacheStringTable* table = new CacheStringTable();

    bool has16BitString = m_buffer.get<bool>();
    size_t maxLength = m_buffer.get<size_t>();
    size_t tableSize = m_buffer.get<size_t>();

    if (LIKELY(!has16BitString)) {
        LChar* buffer = new LChar[maxLength + 1];
        for (size_t i = 0; i < tableSize; i++) {
            size_t length = m_buffer.get<size_t>();
            m_buffer.getData(buffer, length);
            buffer[length] = '\0';

            if (UNLIKELY(length == 0)) {
                table->initAdd(AtomicString());
            } else {
                table->initAdd(AtomicString(context, buffer, length));
            }
        }

        delete[] buffer;

    } else {
        LChar* lBuffer = new LChar[maxLength + 1];
        UChar* uBuffer = new UChar[maxLength + 1];
        for (size_t i = 0; i < tableSize; i++) {
            bool is8Bit = m_buffer.get<bool>();
            size_t length = m_buffer.get<size_t>();

            if (is8Bit) {
                m_buffer.getData(lBuffer, length);
                lBuffer[length] = '\0';

                if (UNLIKELY(length == 0)) {
                    table->initAdd(AtomicString());
                } else {
                    table->initAdd(AtomicString(context, lBuffer, length));
                }
            } else {
                ASSERT(length > 0);
                m_buffer.getData(uBuffer, length);
                uBuffer[length] = '\0';

                table->initAdd(AtomicString(context, uBuffer, length));
            }
        }

        delete[] lBuffer;
        delete[] uBuffer;
    }

    ASSERT(table->table().size() == tableSize);
    return table;
}

#define LOAD_ATOMICSTRING_RELOC(member)                              \
    ASSERT(info.relocType == ByteCodeRelocType::RELOC_ATOMICSTRING); \
    size_t stringIndex = info.dataOffset;                            \
    bc->member = m_stringTable->get(stringIndex);

void CodeCacheReader::loadByteCodeStream(Context* context, ByteCodeBlock* block)
{
    ByteCodeBlockData& byteCodeStream = block->m_code;

    // load ByteCode stream
    {
        size_t codeSize = m_buffer.get<size_t>();
        byteCodeStream.resizeWithUninitializedValues(codeSize);
        m_buffer.getData(byteCodeStream.data(), codeSize);
    }

    Vector<ByteCodeRelocInfo, std::allocator<ByteCodeRelocInfo>> relocInfoVector;
    ByteCodeStringLiteralData& stringLiteralData = block->m_stringLiteralData;
    ByteCodeOtherLiteralData& bigIntData = block->m_otherLiteralData;

    // load relocInfoVector
    {
        size_t relocSize = m_buffer.get<size_t>();
        relocInfoVector.resizeWithUninitializedValues(relocSize);
        m_buffer.getData(relocInfoVector.data(), relocSize);
    }

    // relocate ByteCodeStream
    {
        uint8_t* code = byteCodeStream.data();
        uint8_t* end = code + byteCodeStream.size();

        InterpretedCodeBlock* codeBlock = block->codeBlock();

        // mark for LoadRegExp bytecode
        bool bodyStringForLoadRegExp = true;

        for (size_t i = 0; i < relocInfoVector.size(); i++) {
            ByteCodeRelocInfo& info = relocInfoVector[i];
            ByteCode* currentCode = reinterpret_cast<ByteCode*>(code + info.codeOffset);

#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
            Opcode opcode = (Opcode)(size_t)currentCode->m_opcodeInAddress;
#else
            Opcode opcode = currentCode->m_opcode;
#endif
            switch (opcode) {
            case LoadLiteralOpcode: {
                LoadLiteral* bc = static_cast<LoadLiteral*>(currentCode);
                size_t dataIndex = info.dataOffset;

                switch (info.relocType) {
                case ByteCodeRelocType::RELOC_STRING: {
                    if (UNLIKELY(dataIndex == SIZE_MAX)) {
                        bc->m_value = String::emptyString();
                    } else {
                        ASSERT(dataIndex < stringLiteralData.size());
                        String* string = stringLiteralData[dataIndex];
                        bc->m_value = Value(string);
                    }
                    break;
                }
                case ByteCodeRelocType::RELOC_ATOMICSTRING: {
                    bc->m_value = Value(m_stringTable->get(dataIndex).string());
                    break;
                }
                case ByteCodeRelocType::RELOC_BIGINT: {
                    PointerValue* value = static_cast<PointerValue*>(bigIntData[dataIndex]);
                    bc->m_value = Value(value->asBigInt());
                    break;
                }
                case ByteCodeRelocType::RELOC_FREEZEFUNC: {
                    bc->m_value = Value(context->globalObject()->objectFreeze());
                    break;
                }
                default: {
                    ASSERT_NOT_REACHED();
                    break;
                }
                }
                break;
            }
            case LoadByNameOpcode: {
                LoadByName* bc = static_cast<LoadByName*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case StoreByNameOpcode: {
                StoreByName* bc = static_cast<StoreByName*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case InitializeByNameOpcode: {
                InitializeByName* bc = static_cast<InitializeByName*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case CreateFunctionOpcode: {
                CreateFunction* bc = static_cast<CreateFunction*>(currentCode);
                InterpretedCodeBlockVector& children = codeBlock->children();
                size_t childIndex = info.dataOffset;
                ASSERT(childIndex < children.size());
                bc->m_codeBlock = children[childIndex];
                break;
            }
            case InitializeClassOpcode: {
                InitializeClass* bc = static_cast<InitializeClass*>(currentCode);

                if (bc->m_stage == InitializeClass::CreateClass) {
                    size_t dataIndex = info.dataOffset;
                    if (info.relocType == ByteCodeRelocType::RELOC_CODEBLOCK) {
                        InterpretedCodeBlockVector& children = codeBlock->children();
                        ASSERT(dataIndex < children.size());
                        bc->m_codeBlock = children[dataIndex];
                    } else if (info.relocType == ByteCodeRelocType::RELOC_STRING) {
                        ASSERT(dataIndex < stringLiteralData.size());
                        bc->m_classSrc = stringLiteralData[dataIndex];
                    } else {
                        ASSERT(info.relocType == ByteCodeRelocType::RELOC_ATOMICSTRING);
                        bc->m_name = m_stringTable->get(dataIndex).string();
                    }
                }
                break;
            }
            case ComplexSetObjectOperationOpcode: {
                ComplexSetObjectOperation* bc = static_cast<ComplexSetObjectOperation*>(currentCode);
                if (bc->m_type == ComplexSetObjectOperation::Private) {
                    LOAD_ATOMICSTRING_RELOC(m_propertyName);
                }
                break;
            }
            case ComplexGetObjectOperationOpcode: {
                ComplexGetObjectOperation* bc = static_cast<ComplexGetObjectOperation*>(currentCode);
                if (bc->m_type == ComplexGetObjectOperation::Private) {
                    LOAD_ATOMICSTRING_RELOC(m_propertyName);
                }
                break;
            }
            case ObjectDefineOwnPropertyWithNameOperationOpcode: {
                ObjectDefineOwnPropertyWithNameOperation* bc = static_cast<ObjectDefineOwnPropertyWithNameOperation*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case InitializeGlobalVariableOpcode: {
                InitializeGlobalVariable* bc = static_cast<InitializeGlobalVariable*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_variableName);
                break;
            }
            case UnaryTypeofOpcode: {
                UnaryTypeof* bc = static_cast<UnaryTypeof*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_id);
                break;
            }
            case UnaryDeleteOpcode: {
                UnaryDelete* bc = static_cast<UnaryDelete*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_id);
                break;
            }
            case CallComplexCaseOpcode: {
                CallComplexCase* bc = static_cast<CallComplexCase*>(currentCode);
                ASSERT(bc->m_kind == CallComplexCase::InWithScope);
                LOAD_ATOMICSTRING_RELOC(m_calleeName);
                break;
            }
            case GetMethodOpcode: {
                GetMethod* bc = static_cast<GetMethod*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case LoadRegExpOpcode: {
                LoadRegExp* bc = static_cast<LoadRegExp*>(currentCode);
                if (bodyStringForLoadRegExp) {
                    ASSERT(info.dataOffset < stringLiteralData.size());
                    bc->m_body = stringLiteralData[info.dataOffset];
                    bodyStringForLoadRegExp = false;
                } else {
                    if (info.dataOffset == SIZE_MAX) {
                        bc->m_option = String::emptyString();
                    } else {
                        ASSERT(info.dataOffset < stringLiteralData.size());
                        bc->m_option = stringLiteralData[info.dataOffset];
                    }
                    bodyStringForLoadRegExp = true;
                }
                break;
            }
            case BlockOperationOpcode: {
                BlockOperation* bc = static_cast<BlockOperation*>(currentCode);
                size_t blockIndex = info.dataOffset;
                ASSERT(blockIndex < codeBlock->m_blockInfosLength);
                bc->m_blockInfo = codeBlock->m_blockInfos[blockIndex];
                break;
            }
            case ReplaceBlockLexicalEnvironmentOperationOpcode: {
                ReplaceBlockLexicalEnvironmentOperation* bc = static_cast<ReplaceBlockLexicalEnvironmentOperation*>(currentCode);
                size_t blockIndex = info.dataOffset;
                ASSERT(blockIndex < codeBlock->m_blockInfosLength);
                bc->m_blockInfo = codeBlock->m_blockInfos[blockIndex];
                break;
            }
            case ResolveNameAddressOpcode: {
                ResolveNameAddress* bc = static_cast<ResolveNameAddress*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case StoreByNameWithAddressOpcode: {
                StoreByNameWithAddress* bc = static_cast<StoreByNameWithAddress*>(currentCode);
                LOAD_ATOMICSTRING_RELOC(m_name);
                break;
            }
            case GetObjectPreComputedCaseOpcode: {
                GetObjectPreComputedCase* bc = static_cast<GetObjectPreComputedCase*>(currentCode);
                ASSERT(!bc->hasInlineCache());
                LOAD_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case SetObjectPreComputedCaseOpcode: {
                SetObjectPreComputedCase* bc = static_cast<SetObjectPreComputedCase*>(currentCode);
                ASSERT(!bc->m_inlineCache);
                LOAD_ATOMICSTRING_RELOC(m_propertyName);
                break;
            }
            case GetGlobalVariableOpcode: {
                GetGlobalVariable* bc = static_cast<GetGlobalVariable*>(currentCode);
                size_t stringIndex = info.dataOffset;
                bc->m_slot = context->ensureGlobalVariableAccessCacheSlot(m_stringTable->get(stringIndex));
                break;
            }
            case SetGlobalVariableOpcode: {
                SetGlobalVariable* bc = static_cast<SetGlobalVariable*>(currentCode);
                size_t stringIndex = info.dataOffset;
                bc->m_slot = context->ensureGlobalVariableAccessCacheSlot(m_stringTable->get(stringIndex));
                break;
            }
            case ThrowStaticErrorOperationOpcode: {
                ThrowStaticErrorOperation* bc = static_cast<ThrowStaticErrorOperation*>(currentCode);
                bc->m_errorMessage = ErrorObject::Messages::CodeCache_Loaded_StaticError;
                LOAD_ATOMICSTRING_RELOC(m_templateDataString);
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    }
}
} // namespace Escargot

#endif // ENABLE_CODE_CACHE
