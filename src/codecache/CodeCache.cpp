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
#include "codecache/CodeCache.h"
#include "codecache/CodeCacheReaderWriter.h"
#include "parser/Script.h"
#include "parser/CodeBlock.h"

namespace Escargot {

#if defined(ESCARGOT_ENABLE_TEST)
const char* CodeCache::filePathPrefix = "";
#else
const char* CodeCache::filePathPrefix = "/tmp/";
#endif

CodeCache::CodeCache()
    : m_writer(new CodeCacheWriter())
    , m_reader(new CodeCacheReader())
    , m_stringTableMap(new CacheStringTableMap())
{
}

CodeCache::~CodeCache()
{
    delete m_writer;
    delete m_reader;

    for (auto iter = m_stringTableMap->begin(); iter != m_stringTableMap->end(); iter++) {
        delete iter->second;
    }
    delete m_stringTableMap;
}

CacheStringTable* CodeCache::addStringTable(Script* script)
{
    auto iter = m_stringTableMap->find(script);
    if (iter != m_stringTableMap->end()) {
        return iter->second;
    }

    CacheStringTable* table = new CacheStringTable();
    m_stringTableMap->insert(std::make_pair(script, table));
    return table;
}

std::pair<bool, std::pair<CodeCacheMetaInfo, CodeCacheMetaInfo>> CodeCache::tryLoadCodeCacheMetaInfo(String* srcName, String* source)
{
    ASSERT(srcName->has8BitContent());
    ASSERT(source->length() > CODE_CACHE_MIN_SOURCE_LENGTH);

    CodeCacheMetaInfo codeBlockTreeInfo;
    CodeCacheMetaInfo byteCodeInfo;
    bool success = false;

    char metaFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getMetaFileName(srcName, metaFileName);

    // load Meta file
    FILE* metaFile = fopen(metaFileName, "rb");

    if (metaFile) {
        CodeCacheMetaInfo meta;
        size_t srcHash;
        size_t sourceOffset;
        size_t codeBlockDataSize;

#ifndef NDEBUG
        size_t metaCount = 0;
#endif

        fread(&srcHash, sizeof(size_t), 1, metaFile);
        if (srcHash != source->hashValue()) {
            fclose(metaFile);
            removeCacheFiles(srcName);
            return std::make_pair(false, std::make_pair(codeBlockTreeInfo, byteCodeInfo));
        }

        while (!feof(metaFile)) {
            fread(&sourceOffset, sizeof(size_t), 1, metaFile);
            int check = fread(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);

            if (!check) {
                break;
            }

            ASSERT(meta.cacheType != CodeCacheType::CACHE_INVALID);
            if (meta.cacheType == CodeCacheType::CACHE_CODEBLOCK) {
                codeBlockTreeInfo = meta;
            } else {
                ASSERT(meta.cacheType == CodeCacheType::CACHE_BYTECODE);
                byteCodeInfo = meta;
            }
#ifndef NDEBUG
            metaCount++;
#endif
        }

        fclose(metaFile);
        ASSERT(metaCount == 2);

        success = true;
#if defined(ESCARGOT_ENABLE_TEST)
        ESCARGOT_LOG_INFO("CODECACHE: Load Cache Success\n");
#endif
    }

    return std::make_pair(success, std::make_pair(codeBlockTreeInfo, byteCodeInfo));
}

void CodeCache::storeStringTable(Script* script)
{
    auto iter = m_stringTableMap->find(script);
    ASSERT(iter != m_stringTableMap->end());

    Vector<AtomicString, std::allocator<AtomicString>>& table = iter->second->table();
    bool has16BitString = iter->second->has16BitString();
    size_t maxLength = iter->second->maxLength();
    size_t tableSize = table.size();
    size_t length;

    char stringFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getStringFileName(script->srcName(), stringFileName);
    FILE* stringFile = fopen(stringFileName, "ab");
    ASSERT(!!stringFile);

    fwrite(&has16BitString, sizeof(bool), 1, stringFile);
    fwrite(&maxLength, sizeof(size_t), 1, stringFile);
    fwrite(&tableSize, sizeof(size_t), 1, stringFile);

    if (LIKELY(!has16BitString)) {
        for (size_t i = 0; i < tableSize; i++) {
            String* string = table[i].string();
            ASSERT(string->has8BitContent());
            length = string->length();
            fwrite(&length, sizeof(size_t), 1, stringFile);
            fwrite(string->characters8(), sizeof(LChar), length, stringFile);
        }
    } else {
        for (size_t i = 0; i < tableSize; i++) {
            String* string = table[i].string();
            bool is8Bit = string->has8BitContent();
            length = string->length();
            fwrite(&is8Bit, sizeof(bool), 1, stringFile);
            fwrite(&length, sizeof(size_t), 1, stringFile);
            if (is8Bit) {
                fwrite(string->characters8(), sizeof(LChar), length, stringFile);
            } else {
                fwrite(string->characters16(), sizeof(UChar), length, stringFile);
            }
        }
    }

    fclose(stringFile);
#if defined(ESCARGOT_ENABLE_TEST)
    ESCARGOT_LOG_INFO("CODECACHE: Store StringTable\n");
#endif
}

CacheStringTable* CodeCache::loadStringTable(Context* context, Script* script)
{
    char stringFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getStringFileName(script->srcName(), stringFileName);
    FILE* stringFile = fopen(stringFileName, "rb");
    ASSERT(!!stringFile);

    CacheStringTable* table = new CacheStringTable();

    bool has16BitString;
    size_t maxLength, tableSize, length;

    fread(&has16BitString, sizeof(bool), 1, stringFile);
    fread(&maxLength, sizeof(size_t), 1, stringFile);
    fread(&tableSize, sizeof(size_t), 1, stringFile);

    if (LIKELY(!has16BitString)) {
        LChar* buffer = new LChar[maxLength + 1];
        for (size_t i = 0; i < tableSize; i++) {
            fread(&length, sizeof(size_t), 1, stringFile);
            fread(buffer, sizeof(LChar), length, stringFile);
            buffer[length] = '\0';
            if (UNLIKELY(length == 0)) {
                table->initAdd(AtomicString());
            } else {
                table->initAdd(AtomicString(context, buffer, length));
            }
        }

        delete[] buffer;

    } else {
        bool is8Bit;
        LChar* lBuffer = new LChar[maxLength + 1];
        UChar* uBuffer = new UChar[maxLength + 1];
        for (size_t i = 0; i < tableSize; i++) {
            fread(&is8Bit, sizeof(bool), 1, stringFile);
            fread(&length, sizeof(size_t), 1, stringFile);

            if (is8Bit) {
                fread(lBuffer, sizeof(LChar), length, stringFile);
                lBuffer[length] = '\0';
                if (UNLIKELY(length == 0)) {
                    table->initAdd(AtomicString());
                } else {
                    table->initAdd(AtomicString(context, lBuffer, length));
                }
            } else {
                ASSERT(length > 0);
                fread(uBuffer, sizeof(UChar), length, stringFile);
                uBuffer[length] = '\0';
                table->initAdd(AtomicString(context, uBuffer, length));
            }
        }

        delete[] lBuffer;
        delete[] uBuffer;
    }

    ASSERT(table->table().size() == tableSize);
    fclose(stringFile);

    return table;
}

void CodeCache::storeCodeBlockTree(Script* script)
{
    CacheStringTable* table = addStringTable(script);
    m_writer->setStringTable(table);

    size_t nodeCount = 0;
    InterpretedCodeBlock* topCodeBlock = script->topCodeBlock();
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    writeCodeBlockToFile(script, nodeCount);

    m_writer->clear();
#if defined(ESCARGOT_ENABLE_TEST)
    ESCARGOT_LOG_INFO("CODECACHE: Store CodeBlockTree\n");
#endif
}

void CodeCache::storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount)
{
    ASSERT(!!codeBlock);

    m_writer->storeInterpretedCodeBlock(codeBlock);
    nodeCount++;

    if (codeBlock->hasChildren()) {
        InterpretedCodeBlockVector& childrenVector = codeBlock->children();
        for (size_t i = 0; i < childrenVector.size(); i++) {
            storeCodeBlockTreeNode(childrenVector[i], nodeCount);
        }
    }
}

void CodeCache::storeByteCodeBlock(Script* script, ByteCodeBlock* block)
{
    CacheStringTable* table = addStringTable(script);
    m_writer->setStringTable(table);

    m_writer->storeByteCodeBlock(block);
    writeByteCodeBlockToFile(script);

    m_writer->clear();
#if defined(ESCARGOT_ENABLE_TEST)
    ESCARGOT_LOG_INFO("CODECACHE: Store ByteCodeBlock\n");
#endif
}

InterpretedCodeBlock* CodeCache::loadCodeBlockTree(Context* context, Script* script, CacheStringTable* table, CodeCacheMetaInfo metaInfo)
{
    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_reader->setStringTable(table);
    loadFromDataFile(script->srcName(), metaInfo);

    // temporal vector to keep the loaded InterpretedCodeBlock in GC heap
    std::unordered_map<size_t, InterpretedCodeBlock*, std::hash<size_t>, std::equal_to<size_t>, GCUtil::gc_malloc_allocator<std::pair<size_t const, InterpretedCodeBlock*>>> tempCodeBlockMap;

    // CodeCacheMetaInfo::dataOffset has the value of nodeCount for CACHE_CODEBLOCK
    size_t nodeCount = metaInfo.dataOffset;
    for (size_t i = 0; i < nodeCount; i++) {
        InterpretedCodeBlock* codeBlock = m_reader->loadInterpretedCodeBlock(context, script);
        // end position is used as cache id of each CodeBlock
        ASSERT(tempCodeBlockMap.find(codeBlock->src().end()) == tempCodeBlockMap.end());
        tempCodeBlockMap.insert(std::make_pair(codeBlock->src().end(), codeBlock));

        if (i == 0) {
            // GlobalCodeBlock is firstly stored and loaded
            topCodeBlock = codeBlock;
        }
    }

    // link CodeBlock tree
    for (auto iter = tempCodeBlockMap.begin(); iter != tempCodeBlockMap.end(); iter++) {
        InterpretedCodeBlock* codeBlock = iter->second;
        size_t parentIndex = (size_t)codeBlock->parent();
        if (parentIndex == SIZE_MAX) {
            codeBlock->setParent(nullptr);
        } else {
            auto parentIter = tempCodeBlockMap.find(parentIndex);
            ASSERT(parentIter != tempCodeBlockMap.end());
            codeBlock->setParent(parentIter->second);
        }

        if (codeBlock->hasChildren()) {
            for (size_t childIndex = 0; childIndex < codeBlock->children().size(); childIndex++) {
                size_t srcIndex = (size_t)codeBlock->children()[childIndex];
                auto childIter = tempCodeBlockMap.find(srcIndex);
                ASSERT(childIter != tempCodeBlockMap.end());
                codeBlock->children()[childIndex] = childIter->second;
            }
        }
    }

    // clear
    tempCodeBlockMap.clear();
    m_reader->clear();

    ASSERT(topCodeBlock->isGlobalCodeBlock());
    // return topCodeBlock
    return topCodeBlock;
}

ByteCodeBlock* CodeCache::loadByteCodeBlock(Context* context, Script* script, CacheStringTable* table, CodeCacheMetaInfo metaInfo)
{
    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE);

    m_reader->setStringTable(table);
    loadFromDataFile(script->srcName(), metaInfo);

    ByteCodeBlock* block = m_reader->loadByteCodeBlock(context, script);

    // clear
    m_reader->clear();

    return block;
}

void CodeCache::writeCodeBlockToFile(Script* script, size_t nodeCount)
{
    size_t sourceOffset = 0;

    // CodeCache stores the CodeBlock tree first
    // hash value of source code
    size_t hash = script->sourceCode()->hashValue();

    // CodeCacheMetaInfo::dataOffset has the value of nodeCount for CACHE_CODEBLOCK
    CodeCacheMetaInfo meta(CodeCacheType::CACHE_CODEBLOCK, nodeCount, m_writer->bufferSize());
    char metaFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getMetaFileName(script->srcName(), metaFileName);
    FILE* metaFile = fopen(metaFileName, "ab");
    ASSERT(metaFile);
    fwrite(&hash, sizeof(size_t), 1, metaFile);

    fwrite(&sourceOffset, sizeof(size_t), 1, metaFile);
    fwrite(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);
    fclose(metaFile);

    char dataFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getCodeBlockFileName(script->srcName(), dataFileName);
    FILE* dataFile = fopen(dataFileName, "ab");
    ASSERT(dataFile);
    fwrite(m_writer->bufferData(), sizeof(char), m_writer->bufferSize(), dataFile);
    fclose(dataFile);
}

void CodeCache::writeByteCodeBlockToFile(Script* script)
{
    size_t sourceOffset = 0;

    CodeCacheMetaInfo meta(CodeCacheType::CACHE_BYTECODE, 0, m_writer->bufferSize());
    char metaFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getMetaFileName(script->srcName(), metaFileName);
    FILE* metaFile = fopen(metaFileName, "ab");
    ASSERT(metaFile);
    fwrite(&sourceOffset, sizeof(size_t), 1, metaFile);
    fwrite(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);
    fclose(metaFile);

    char dataFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    getByteCodeFileName(script->srcName(), dataFileName);
    FILE* dataFile = fopen(dataFileName, "ab");
    ASSERT(dataFile);
    fwrite(m_writer->bufferData(), sizeof(char), m_writer->bufferSize(), dataFile);
    fclose(dataFile);
}

void CodeCache::loadFromDataFile(String* srcName, CodeCacheMetaInfo& metaInfo)
{
    ASSERT(metaInfo.cacheType != CodeCacheType::CACHE_INVALID);
    if (metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK) {
        char codeBlockFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
        getCodeBlockFileName(srcName, codeBlockFileName);
        FILE* dataFile = fopen(codeBlockFileName, "rb");
        ASSERT(!!dataFile);

        size_t dataSize = metaInfo.dataSize;
        m_reader->loadDataFile(dataFile, dataSize);
        fclose(dataFile);
    } else if (metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE) {
        char byteCodeFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
        getByteCodeFileName(srcName, byteCodeFileName);
        FILE* dataFile = fopen(byteCodeFileName, "rb");
        ASSERT(!!dataFile);

        size_t dataSize = metaInfo.dataSize;
        m_reader->loadDataFile(dataFile, dataSize);
        fclose(dataFile);
    }
}

void CodeCache::removeCacheFiles(String* srcName)
{
    char metaFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    char strFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    char codeBlockFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    char byteCodeFileName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };

    getMetaFileName(srcName, metaFileName);
    getStringFileName(srcName, strFileName);
    getCodeBlockFileName(srcName, codeBlockFileName);
    getByteCodeFileName(srcName, byteCodeFileName);

    remove(metaFileName);
    remove(strFileName);
    remove(codeBlockFileName);
    remove(byteCodeFileName);
}

void CodeCache::getMetaFileName(String* srcName, char* buffer)
{
    ASSERT(srcName->has8BitContent());

    char hashName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    snprintf(hashName, CODE_CACHE_FILE_NAME_LENGTH, "%zu", srcName->hashValue());

    strncpy(buffer, filePathPrefix, 5);
    strncat(buffer, hashName, CODE_CACHE_FILE_NAME_LENGTH);
    strncat(buffer, "_meta", 6);
}

void CodeCache::getStringFileName(String* srcName, char* buffer)
{
    ASSERT(srcName->has8BitContent());

    char hashName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    snprintf(hashName, CODE_CACHE_FILE_NAME_LENGTH, "%zu", srcName->hashValue());

    strncpy(buffer, filePathPrefix, 5);
    strncat(buffer, hashName, CODE_CACHE_FILE_NAME_LENGTH);
    strncat(buffer, "_str", 5);
}

void CodeCache::getCodeBlockFileName(String* srcName, char* buffer)
{
    ASSERT(srcName->has8BitContent());

    char hashName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    snprintf(hashName, CODE_CACHE_FILE_NAME_LENGTH, "%zu", srcName->hashValue());

    strncpy(buffer, filePathPrefix, 5);
    strncat(buffer, hashName, CODE_CACHE_FILE_NAME_LENGTH);
    strncat(buffer, "_data", 6);
}

void CodeCache::getByteCodeFileName(String* srcName, char* buffer)
{
    ASSERT(srcName->has8BitContent());

    char hashName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    snprintf(hashName, CODE_CACHE_FILE_NAME_LENGTH, "%zu", srcName->hashValue());

    strncpy(buffer, filePathPrefix, 5);
    strncat(buffer, hashName, CODE_CACHE_FILE_NAME_LENGTH);
    strncat(buffer, "_byte", 6);
}
}
#endif // ENABLE_CODE_CACHE
