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
const char* CodeCache::CodeCacheContext::filePathPrefix = "";
#else
const char* CodeCache::CodeCacheContext::filePathPrefix = "/tmp/";
#endif

void CodeCache::CodeCacheContext::initStringTable()
{
    ASSERT(!this->stringTable);

    this->stringTable = new CacheStringTable();
}

void CodeCache::CodeCacheContext::initStringTable(CacheStringTable* table)
{
    ASSERT(!!table);
    ASSERT(!this->stringTable);

    this->stringTable = table;
}

void CodeCache::CodeCacheContext::initFileName(String* srcName)
{
    ASSERT(srcName->has8BitContent() && srcName->length());
    ASSERT(this->dataOffset == 0);
    ASSERT(!this->metaFileName && !this->dataFileName);

    char hashName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    char metaName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };
    char dataName[CODE_CACHE_FILE_NAME_LENGTH] = { 0 };

    strncpy(metaName, filePathPrefix, 5);
    strncpy(dataName, filePathPrefix, 5);

    snprintf(hashName, CODE_CACHE_FILE_NAME_LENGTH, "%zu", srcName->hashValue());
    strncat(metaName, hashName, CODE_CACHE_FILE_NAME_LENGTH);
    strncat(dataName, hashName, CODE_CACHE_FILE_NAME_LENGTH);

    strncat(metaName, "_meta", 6);
    strncat(dataName, "_data", 6);

    this->metaFileName = new char[strlen(metaName) + 1];
    this->dataFileName = new char[strlen(dataName) + 1];
    strncpy(this->metaFileName, metaName, strlen(metaName) + 1);
    strncpy(this->dataFileName, dataName, strlen(dataName) + 1);
}

void CodeCache::CodeCacheContext::reset()
{
    ASSERT(!!metaFileName && !!dataFileName);

    if (stringTable) {
        delete stringTable;
    }

    delete[] metaFileName;
    delete[] dataFileName;

    dataOffset = 0;
    stringTable = nullptr;
    metaFileName = dataFileName = nullptr;
}

CodeCache::CodeCache()
    : m_writer(new CodeCacheWriter())
    , m_reader(new CodeCacheReader())
{
}

CodeCache::~CodeCache()
{
    delete m_writer;
    delete m_reader;
}

std::pair<bool, CodeCacheMetaChunk> CodeCache::tryLoadCodeCacheMetaInfo(String* srcName, String* source)
{
    ASSERT(srcName->length() && srcName->has8BitContent());
    ASSERT(source->length() > CODE_CACHE_MIN_SOURCE_LENGTH);

    CodeCacheMetaChunk metaChunk;
    bool success = false;

    m_context.initFileName(srcName);

    // load Meta file
    FILE* metaFile = fopen(m_context.metaFileName, "rb");

    if (metaFile) {
        CodeCacheMetaInfo meta;
        size_t srcHash;
#ifndef NDEBUG
        size_t metaCount = 0;
#endif

        fread(&srcHash, sizeof(size_t), 1, metaFile);

        if (srcHash != source->hashValue()) {
            while (!feof(metaFile)) {
                int check = fread(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);

                if (!check) {
                    break;
                }

                switch (meta.cacheType) {
                case CodeCacheType::CACHE_CODEBLOCK: {
                    metaChunk.codeBlockInfo = meta;
                    break;
                }
                case CodeCacheType::CACHE_BYTECODE: {
                    metaChunk.byteCodeInfo = meta;
                    break;
                }
                case CodeCacheType::CACHE_STRING: {
                    metaChunk.stringInfo = meta;
                    break;
                }
                default: {
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                }
#ifndef NDEBUG
                metaCount++;
#endif
            }

            success = true;
            ASSERT(metaCount == 3);
#if defined(ESCARGOT_ENABLE_TEST)
            ESCARGOT_LOG_INFO("CODECACHE: Load Cache Success\n");
#endif
        }

        fclose(metaFile);
    }

    if (!success) {
        removeCacheFiles();
    }

    return std::make_pair(success, metaChunk);
}

void CodeCache::storeStringTable()
{
    ASSERT(!!m_context.stringTable);

    m_writer->setStringTable(m_context.stringTable);
    m_writer->storeStringTable();

    writeCacheToFile(CodeCacheType::CACHE_STRING);

#if defined(ESCARGOT_ENABLE_TEST)
    ESCARGOT_LOG_INFO("CODECACHE: Store StringTable\n");
#endif
}

CacheStringTable* CodeCache::loadStringTable(Context* context, Script* script, CodeCacheMetaInfo metaInfo)
{
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_context.dataFileName);

    loadCacheFromDataFile(metaInfo);

    CacheStringTable* table = m_reader->loadStringTable(context);

    // clear
    m_reader->clearBuffer();
    return table;
}

void CodeCache::storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock)
{
    ASSERT(m_context.dataOffset == 0);
    ASSERT(!!m_context.stringTable);
    ASSERT(!!topCodeBlock);

    m_writer->setStringTable(m_context.stringTable);

    size_t nodeCount = 0;
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    writeCacheToFile(CodeCacheType::CACHE_CODEBLOCK, nodeCount);

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

void CodeCache::storeByteCodeBlock(ByteCodeBlock* block)
{
    ASSERT(!!m_context.stringTable);

    m_writer->setStringTable(m_context.stringTable);

    m_writer->storeByteCodeBlock(block);
    writeCacheToFile(CodeCacheType::CACHE_BYTECODE);

#if defined(ESCARGOT_ENABLE_TEST)
    ESCARGOT_LOG_INFO("CODECACHE: Store ByteCodeBlock\n");
#endif
}

InterpretedCodeBlock* CodeCache::loadCodeBlockTree(Context* context, Script* script, CodeCacheMetaInfo metaInfo)
{
    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_reader->setStringTable(m_context.stringTable);
    loadCacheFromDataFile(metaInfo);

    // temporal vector to keep the loaded InterpretedCodeBlock in GC heap
    std::unordered_map<size_t, InterpretedCodeBlock*, std::hash<size_t>, std::equal_to<size_t>, GCUtil::gc_malloc_allocator<std::pair<size_t const, InterpretedCodeBlock*>>> tempCodeBlockMap;

    // CodeCacheMetaInfo::codeBlockCount has the value of nodeCount for CACHE_CODEBLOCK
    size_t nodeCount = metaInfo.codeBlockCount;
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
    m_reader->clearBuffer();

    ASSERT(topCodeBlock->isGlobalCodeBlock());
    // return topCodeBlock
    return topCodeBlock;
}

ByteCodeBlock* CodeCache::loadByteCodeBlock(Context* context, Script* script, CodeCacheMetaInfo metaInfo)
{
    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE);

    m_reader->setStringTable(m_context.stringTable);
    loadCacheFromDataFile(metaInfo);

    ByteCodeBlock* block = m_reader->loadByteCodeBlock(context, script);

    // clear
    m_reader->clearBuffer();

    return block;
}

void CodeCache::writeCacheToFile(CodeCacheType type, size_t extraCount)
{
    ASSERT(type == CodeCacheType::CACHE_CODEBLOCK || type == CodeCacheType::CACHE_BYTECODE || type == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_context.metaFileName && !!m_context.dataFileName);

    // write meta info
    CodeCacheMetaInfo meta(type, m_context.dataOffset, m_writer->bufferSize());

    FILE* metaFile = fopen(m_context.metaFileName, "ab");
    ASSERT(metaFile);
    if (type == CodeCacheType::CACHE_CODEBLOCK) {
        ASSERT(m_context.dataOffset == 0);
        // extraCount represents the total count of CodeBlocks used only for CodeBlockTree caching
        meta.codeBlockCount = extraCount;

        // write hash value at the top of meta file
        size_t hashValue = m_context.srcHashValue;
        fwrite(&hashValue, sizeof(size_t), 1, metaFile);
    }
    fwrite(&meta, sizeof(CodeCacheMetaInfo), 1, metaFile);
    fclose(metaFile);

    // write cache data
    FILE* dataFile = fopen(m_context.dataFileName, "ab");
    ASSERT(dataFile);
    fwrite(m_writer->bufferData(), sizeof(char), m_writer->bufferSize(), dataFile);
    fclose(dataFile);

    m_context.dataOffset += m_writer->bufferSize();
    m_writer->clearBuffer();
}

void CodeCache::loadCacheFromDataFile(CodeCacheMetaInfo& metaInfo)
{
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK || metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE || metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_context.dataFileName);

    size_t dataOffset = metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK ? 0 : metaInfo.dataOffset;
    FILE* dataFile = fopen(m_context.dataFileName, "rb");
    ASSERT(!!dataFile);
    fseek(dataFile, dataOffset, SEEK_SET);

    m_reader->loadDataFile(dataFile, metaInfo.dataSize);

    fclose(dataFile);
}

void CodeCache::removeCacheFiles()
{
    ASSERT(!!m_context.metaFileName && !!m_context.dataFileName);
    remove(m_context.metaFileName);
    remove(m_context.dataFileName);
}
}
#endif // ENABLE_CODE_CACHE
