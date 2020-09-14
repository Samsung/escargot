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
#include <dirent.h>
#include <sys/stat.h>

#define CODE_CACHE_FILE_DIR "/Escargot-cache/"
#define CODE_CACHE_LIST_FILE_NAME "cache_list"

namespace Escargot {

void CodeCacheContext::reset()
{
    m_cacheFilePath.clear();
    m_cacheEntry.reset();

    delete m_cacheStringTable;
    m_cacheStringTable = nullptr;
    m_cacheDataOffset = 0;
}

CodeCache::CodeCache(const char* baseCacheDir)
    : m_cacheWriter(nullptr)
    , m_cacheReader(nullptr)
    , m_enabled(false)
    , m_modified(false)
{
    initialize(baseCacheDir);
}

CodeCache::~CodeCache()
{
    clear();
}

void CodeCache::initialize(const char* baseCacheDir)
{
    // set cache file path
    ASSERT(baseCacheDir && strlen(baseCacheDir) > 0);
    m_cacheFileDir = baseCacheDir;
    m_cacheFileDir += CODE_CACHE_FILE_DIR;

    bool initList = tryInitCacheList();
    if (UNLIKELY(!initList)) {
        // initialization of cache list failed
        // disable CodeCache
        m_enabled = false;
        clear();
    }

    m_cacheWriter = new CodeCacheWriter();
    m_cacheReader = new CodeCacheReader();
    m_enabled = true;
}

bool CodeCache::tryInitCacheList()
{
    ASSERT(m_cacheList.size() == 0);
    ASSERT(m_cacheFileDir.length());

    // check cache directory
    DIR* cacheDir = opendir(m_cacheFileDir.data());
    if (cacheDir) {
        int ret = closedir(cacheDir);
        if (ret != 0) {
            return false;
        }
    } else {
        int ret = mkdir(m_cacheFileDir.data(), 0755);
        if (ret != 0) {
            return false;
        }
    }

    // check file permission
    std::string cacheListFilePath = m_cacheFileDir + CODE_CACHE_LIST_FILE_NAME;
    FILE* listFile = fopen(cacheListFilePath.data(), "ab");
    if (!listFile) {
        return false;
    }
    fclose(listFile);

    // load list entries from file
    listFile = fopen(cacheListFilePath.data(), "rb");
    if (listFile) {
        size_t srcHash;
        CodeCacheEntry entry;

        while (!feof(listFile)) {
            int check = fread(&srcHash, sizeof(size_t), 1, listFile);
            if (!check) {
                break;
            }
            fread(&entry, sizeof(CodeCacheEntry), 1, listFile);

            setCacheEntry(srcHash, entry);
        }
    }

    return true;
}

void CodeCache::clear()
{
    // current CodeCache infos are already reset
    ASSERT(!m_currentContext.m_cacheStringTable);
    ASSERT(m_currentContext.m_cacheDataOffset == 0)

    m_cacheFileDir.clear();
    m_cacheList.clear();

    delete m_cacheWriter;
    delete m_cacheReader;
}

void CodeCache::reset()
{
    // reset current CodeCache infos
    m_currentContext.reset();
}

void CodeCache::setCacheEntry(size_t hash, const CodeCacheEntry& entry)
{
#ifndef NDEBUG
    auto iter = m_cacheList.find(hash);
    ASSERT(iter == m_cacheList.end());
#endif
    m_cacheList.insert(std::make_pair(hash, entry));
}

void CodeCache::addCacheEntry(size_t hash, const CodeCacheEntry& entry)
{
    auto iter = m_cacheList.find(hash);
    if (iter != m_cacheList.end()) {
        iter->second = entry;
    } else {
        m_cacheList.insert(std::make_pair(hash, entry));
    }

    m_modified = true;
}

std::pair<bool, CodeCacheEntry> CodeCache::searchCache(size_t srcHash)
{
    ASSERT(m_enabled);

    CodeCacheEntry entry;
    bool cacheHit = false;

    auto iter = m_cacheList.find(srcHash);
    if (iter != m_cacheList.end()) {
        cacheHit = true;
        entry = iter->second;
    }

    return std::make_pair(cacheHit, entry);
}

void CodeCache::prepareCacheLoading(Context* context, size_t srcHash, const CodeCacheEntry& entry)
{
    ASSERT(!m_modified);
    ASSERT(m_cacheFileDir.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);

    m_currentContext.m_cacheFilePath = m_cacheFileDir + std::to_string(srcHash);
    m_currentContext.m_cacheEntry = entry;
    m_currentContext.m_cacheStringTable = loadCacheStringTable(context);
}

void CodeCache::prepareCacheRecording(size_t srcHash)
{
    ASSERT(!m_modified);
    ASSERT(m_cacheFileDir.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);

    m_currentContext.m_cacheFilePath = m_cacheFileDir + std::to_string(srcHash);
    m_currentContext.m_cacheStringTable = new CacheStringTable();
}

void CodeCache::postCacheLoading()
{
    reset();
}

void CodeCache::postCacheRecording(size_t srcHash)
{
    addCacheEntry(srcHash, m_currentContext.m_cacheEntry);
    recordCacheList();
    reset();
}

void CodeCache::storeStringTable()
{
    ASSERT(!!m_currentContext.m_cacheStringTable);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);
    m_cacheWriter->storeStringTable();

    recordCacheData(CodeCacheType::CACHE_STRING);
}

void CodeCache::storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock)
{
    ASSERT(m_currentContext.m_cacheDataOffset == 0);
    ASSERT(!!m_currentContext.m_cacheStringTable);
    ASSERT(!!topCodeBlock);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);

    size_t nodeCount = 0;
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    recordCacheData(CodeCacheType::CACHE_CODEBLOCK, nodeCount);
}

void CodeCache::storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount)
{
    ASSERT(!!codeBlock);

    m_cacheWriter->storeInterpretedCodeBlock(codeBlock);
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
    ASSERT(!!m_currentContext.m_cacheStringTable);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);

    m_cacheWriter->storeByteCodeBlock(block);
    recordCacheData(CodeCacheType::CACHE_BYTECODE);
}

CacheStringTable* CodeCache::loadCacheStringTable(Context* context)
{
    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_STRING];

    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(m_currentContext.m_cacheFilePath.length());

    loadCacheData(metaInfo);

    CacheStringTable* table = m_cacheReader->loadStringTable(context);

    // clear
    m_cacheReader->clearBuffer();
    return table;
}

InterpretedCodeBlock* CodeCache::loadCodeBlockTree(Context* context, Script* script)
{
    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_CODEBLOCK];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    loadCacheData(metaInfo);

    // temporal vector to keep the loaded InterpretedCodeBlock in GC heap
    std::unordered_map<size_t, InterpretedCodeBlock*, std::hash<size_t>, std::equal_to<size_t>, GCUtil::gc_malloc_allocator<std::pair<size_t const, InterpretedCodeBlock*>>> tempCodeBlockMap;

    // CodeCacheMetaInfo::codeBlockCount has the value of nodeCount for CACHE_CODEBLOCK
    size_t nodeCount = metaInfo.codeBlockCount;
    for (size_t i = 0; i < nodeCount; i++) {
        InterpretedCodeBlock* codeBlock = m_cacheReader->loadInterpretedCodeBlock(context, script);
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
    m_cacheReader->clearBuffer();

    ASSERT(topCodeBlock->isGlobalCodeBlock());
    // return topCodeBlock
    return topCodeBlock;
}

ByteCodeBlock* CodeCache::loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock)
{
    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_BYTECODE];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE);

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    loadCacheData(metaInfo);

    ByteCodeBlock* block = m_cacheReader->loadByteCodeBlock(context, topCodeBlock);

    // clear
    m_cacheReader->clearBuffer();

    return block;
}

void CodeCache::recordCacheList()
{
    ASSERT(m_modified);
    ASSERT(m_cacheList.size() > 0);
    ASSERT(m_cacheFileDir.length());

    std::string cacheListFilePath = m_cacheFileDir + CODE_CACHE_LIST_FILE_NAME;
    FILE* listFile = fopen(cacheListFilePath.data(), "wb");
    ASSERT(listFile);

    for (auto iter = m_cacheList.begin(); iter != m_cacheList.end(); iter++) {
        size_t srcHash = iter->first;
        CodeCacheEntry entry = iter->second;
        fwrite(&srcHash, sizeof(size_t), 1, listFile);
        fwrite(&entry, sizeof(CodeCacheEntry), 1, listFile);
    }
    fclose(listFile);

    m_modified = false;
}

void CodeCache::recordCacheData(CodeCacheType type, size_t extraCount)
{
    ASSERT(type == CodeCacheType::CACHE_CODEBLOCK || type == CodeCacheType::CACHE_BYTECODE || type == CodeCacheType::CACHE_STRING);
    ASSERT(m_currentContext.m_cacheFilePath.length());

    FILE* dataFile = nullptr;

    // meta info
    CodeCacheMetaInfo meta(type, m_currentContext.m_cacheDataOffset, m_cacheWriter->bufferSize());
    if (type == CodeCacheType::CACHE_CODEBLOCK) {
        ASSERT(m_currentContext.m_cacheDataOffset == 0);
        // extraCount represents the total count of CodeBlocks used only for CodeBlockTree caching
        meta.codeBlockCount = extraCount;
        dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "wb");
    } else {
        dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "ab");
    }
    m_currentContext.m_cacheEntry.m_metaInfos[(size_t)type] = meta;

    // write cache data
    ASSERT(dataFile);
    fwrite(m_cacheWriter->bufferData(), sizeof(char), m_cacheWriter->bufferSize(), dataFile);
    fclose(dataFile);

    m_currentContext.m_cacheDataOffset += m_cacheWriter->bufferSize();
    m_cacheWriter->clearBuffer();
}

void CodeCache::loadCacheData(CodeCacheMetaInfo& metaInfo)
{
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK || metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE || metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_currentContext.m_cacheFilePath.length());

    size_t dataOffset = metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK ? 0 : metaInfo.dataOffset;
    FILE* dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "rb");
    ASSERT(!!dataFile);
    fseek(dataFile, dataOffset, SEEK_SET);

    m_cacheReader->loadData(dataFile, metaInfo.dataSize);

    fclose(dataFile);
}

void CodeCache::removeCacheFile(size_t srcHash)
{
}
}
#endif // ENABLE_CODE_CACHE
