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
    delete m_stringTableMap;
}

CacheStringTable* CodeCache::addStringTable(Script* script)
{
    ASSERT(m_stringTableMap->find(script) == m_stringTableMap->end());

    CacheStringTable* table = new CacheStringTable();
    m_stringTableMap->insert(std::make_pair(script, table));
    return table;
}

void CodeCache::loadMetaInfos(Script* script)
{
    char metaFileName[128] = { 0 };
    getMetaFileName(script, metaFileName);
    FILE* metaFile = fopen(metaFileName, "rb");

    if (metaFile) {
        CodeCacheMetaInfo meta;
        size_t sourceOffset;
        size_t codeBlockDataSize;
        CodeCacheMetaInfoMap* metaInfoMap = new (GC) CodeCacheMetaInfoMap;

        while (!feof(metaFile)) {
            fread(&sourceOffset, sizeof(size_t), 1, metaFile);
            int check = fread(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);

            if (!check) {
                break;
            }

            ASSERT(meta.cacheType != CodeCacheType::CACHE_INVALID);
            ASSERT(metaInfoMap->find(sourceOffset) == metaInfoMap->end());
            metaInfoMap->insert(std::make_pair(sourceOffset, meta));
        }

        script->setCodeCacheMetaInfo(metaInfoMap);
        fclose(metaFile);
    }
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

    char stringFileName[128] = { 0 };
    getStringFileName(script, stringFileName);
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
            fwrite(&is8Bit, sizeof(bool), 1, stringFile);
            length = string->length();
            fwrite(&length, sizeof(size_t), 1, stringFile);
            if (is8Bit) {
                fwrite(string->characters8(), sizeof(LChar), length, stringFile);
            } else {
                fwrite(string->characters16(), sizeof(UChar), length, stringFile);
            }
        }
    }

    fclose(stringFile);
}

CacheStringTable* CodeCache::loadStringTable(Context* context, Script* script)
{
    char stringFileName[128] = { 0 };
    getStringFileName(script, stringFileName);
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
    // m_writer->clear();

    CacheStringTable* table = addStringTable(script);
    m_writer->setStringTable(table);

    size_t nodeCount = 0;
    InterpretedCodeBlock* topCodeBlock = script->topCodeBlock();
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    writeCodeBlockToFile(script, nodeCount);

    //printf("STORE CodeBlock Num: %lu\n", nodeCount);

    m_writer->clear();
}

void CodeCache::storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount)
{
    ASSERT(!!codeBlock);

    //printf("STORE CodeBlock: %lu\n", codeBlock->src().end());

    m_writer->storeInterpretedCodeBlock(codeBlock);
    nodeCount++;

    if (codeBlock->hasChildren()) {
        InterpretedCodeBlockVector& childrenVector = codeBlock->children();
        for (size_t i = 0; i < childrenVector.size(); i++) {
            storeCodeBlockTreeNode(childrenVector[i], nodeCount);
        }
    }
}

InterpretedCodeBlock* CodeCache::loadCodeBlockTree(Context* context, Script* script, CacheStringTable* table)
{
    ASSERT(!!context);
    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_reader->setStringTable(table);

    ASSERT(!!script->codeCacheMetaInfoMap());
    CodeCacheMetaInfoMap* map = script->codeCacheMetaInfoMap();

    // CodeBlock info has 0 as its source offset
    auto iter = map->find(0);
    ASSERT(iter != map->end() && iter->second.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    loadFromDataFile(script, iter->second);

    // temporal vector to keep the loaded InterpretedCodeBlock in GC heap
    std::unordered_map<size_t, InterpretedCodeBlock*, std::hash<size_t>, std::equal_to<size_t>, GCUtil::gc_malloc_allocator<std::pair<size_t const, InterpretedCodeBlock*>>> tempCodeBlockMap;

    // CodeCacheMetaInfo::dataOffset has the value of nodeCount for CACHE_CODEBLOCK
    size_t nodeCount = iter->second.dataOffset;
    //printf("LOAD CodeBlock Num: %lu\n", nodeCount);
    for (size_t i = 0; i < nodeCount; i++) {
        InterpretedCodeBlock* codeBlock = m_reader->loadInterpretedCodeBlock(context, script);
        // end position is used as cache id of each CodeBlock
        ASSERT(tempCodeBlockMap.find(codeBlock->src().end()) == tempCodeBlockMap.end());
        tempCodeBlockMap.insert(std::make_pair(codeBlock->src().end(), codeBlock));
        //printf("LOAD CodeBlock: %lu\n", codeBlock->src().end());

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

    ASSERT(topCodeBlock->isGlobalScopeCodeBlock());
    // return topCodeBlock
    return topCodeBlock;
}

void CodeCache::writeCodeBlockToFile(Script* script, size_t nodeCount)
{
    size_t sourceOffset = 0;

    // CodeCacheMetaInfo::dataOffset has the value of nodeCount for CACHE_CODEBLOCK
    CodeCacheMetaInfo meta(CodeCacheType::CACHE_CODEBLOCK, nodeCount, m_writer->bufferSize());
    char metaFileName[128] = { 0 };
    getMetaFileName(script, metaFileName);
    FILE* metaFile = fopen(metaFileName, "ab");
    ASSERT(metaFile);
    fwrite(&sourceOffset, sizeof(size_t), 1, metaFile);
    fwrite(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);
    fclose(metaFile);

    char dataFileName[128] = { 0 };
    getDataFileName(script, dataFileName);
    FILE* dataFile = fopen(dataFileName, "ab");
    ASSERT(dataFile);
    fwrite(m_writer->bufferData(), sizeof(char), m_writer->bufferSize(), dataFile);
    fclose(dataFile);
}

void CodeCache::loadFromDataFile(Script* script, CodeCacheMetaInfo& metaInfo)
{
    ASSERT(metaInfo.cacheType != CodeCacheType::CACHE_INVALID);
    if (metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK) {
        char dataFileName[128] = { 0 };
        getDataFileName(script, dataFileName);
        FILE* dataFile = fopen(dataFileName, "rb");
        ASSERT(!!dataFile);

        size_t dataSize = metaInfo.dataSize;
        m_reader->loadDataFile(dataFile, dataSize);
        fclose(dataFile);
    }
}

void CodeCache::getMetaFileName(Script* script, char* buffer)
{
    size_t srcNameLength;
    char srcName[128] = { 0 };

    ASSERT(script->src()->has8BitContent());
    ASSERT(script->src()->length() < 120);
    srcNameLength = script->src()->length();
    strncpy(srcName, script->src()->characters<char>(), srcNameLength);
    srcName[srcNameLength] = '\0';

    // replace '/' with '_'
    char* pch = strchr(srcName, '/');
    while (pch) {
        *pch = '_';
        pch = strchr(srcName, '/');
    }

    strncpy(buffer, srcName, srcNameLength + 1);
    strncat(buffer, "_meta", 5);
}

void CodeCache::getStringFileName(Script* script, char* buffer)
{
    size_t srcNameLength;
    char srcName[128] = { 0 };

    ASSERT(script->src()->has8BitContent());
    ASSERT(script->src()->length() < 120);
    srcNameLength = script->src()->length();
    strncpy(srcName, script->src()->characters<char>(), srcNameLength);
    srcName[srcNameLength] = '\0';

    // replace '/' with '_'
    char* pch = strchr(srcName, '/');
    while (pch) {
        *pch = '_';
        pch = strchr(srcName, '/');
    }

    strncpy(buffer, srcName, srcNameLength + 1);
    strncat(buffer, "_string", 7);
}

void CodeCache::getDataFileName(Script* script, char* buffer)
{
    size_t srcNameLength;
    char srcName[128] = { 0 };

    ASSERT(script->src()->has8BitContent());
    ASSERT(script->src()->length() < 120);
    srcNameLength = script->src()->length();
    strncpy(srcName, script->src()->characters<char>(), srcNameLength);
    srcName[srcNameLength] = '\0';

    // replace '/' with '_'
    char* pch = strchr(srcName, '/');
    while (pch) {
        *pch = '_';
        pch = strchr(srcName, '/');
    }

    strncpy(buffer, srcName, srcNameLength + 1);
    strncat(buffer, "_data", 5);
}
}
#endif // ENABLE_CODE_CACHE
