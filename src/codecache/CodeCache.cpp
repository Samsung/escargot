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

// file libraries
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

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
    , m_cacheDirFD(-1)
    , m_enabled(false)
    , m_modified(false)
    , m_status(Status::NONE)
{
    initialize(baseCacheDir);
}

CodeCache::~CodeCache()
{
    clear();
}

void CodeCache::initialize(const char* baseCacheDir)
{
    ASSERT(!m_enabled);
    ASSERT(baseCacheDir && strlen(baseCacheDir) > 0);

    // set cache file path
    m_cacheDirPath = baseCacheDir;
    m_cacheDirPath += CODE_CACHE_FILE_DIR;

    if (!tryInitCacheDir()) {
        // lock cache directory failed
        clear();
        return;
    }

    if (!tryInitCacheList()) {
        // initialization of cache list failed
        // clear cache directory too
        clearAll();
        return;
    }

    m_cacheWriter = new CodeCacheWriter();
    m_cacheReader = new CodeCacheReader();
    m_enabled = true;
    m_status = Status::READY;
}

bool CodeCache::tryInitCacheDir()
{
    ASSERT(m_cacheList.size() == 0);
    ASSERT(m_cacheDirPath.length());

    // open or create cache directory
    DIR* cacheDir = opendir(m_cacheDirPath.data());
    if (cacheDir) {
        int ret = closedir(cacheDir);
        if (ret != 0) {
            return false;
        }
    } else {
        int ret = mkdir(m_cacheDirPath.data(), 0755);
        if (ret != 0) {
            return false;
        }
    }

    // open cache directory
    if ((m_cacheDirFD = open(m_cacheDirPath.data(), O_RDONLY)) == -1) {
        return false;
    }

    // lock cache directory
    ASSERT(m_cacheDirFD != -1);
    if (flock(m_cacheDirFD, LOCK_EX | LOCK_NB) == -1) {
        close(m_cacheDirFD);
        m_cacheDirFD = -1;
        return false;
    }

    return true;
}

bool CodeCache::tryInitCacheList()
{
    ASSERT(m_cacheList.size() == 0);
    ASSERT(m_cacheDirPath.length());

    // check file permission
    std::string cacheListFilePath = m_cacheDirPath + CODE_CACHE_LIST_FILE_NAME;
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
        bool failLoad = false;

        while (!feof(listFile)) {
            if (fread(&srcHash, sizeof(size_t), 1, listFile) != 1) {
                break;
            }
            if (fread(&entry, sizeof(CodeCacheEntry), 1, listFile) != 1) {
                failLoad = true;
                break;
            }

            setCacheEntry(srcHash, entry);
        }
        fclose(listFile);

        if (failLoad) {
            return false;
        }
    }

    return true;
}

void CodeCache::unLockAndCloseCacheDir()
{
    if (m_cacheDirFD != -1) {
        if (flock(m_cacheDirFD, LOCK_UN) == -1) {
            // exception case - unlock failed
            ESCARGOT_LOG_ERROR("[CodeCache] Failed to unlock cache dir\n");
        }
        close(m_cacheDirFD);
        m_cacheDirFD = -1;
    }
}

void CodeCache::clearCacheDir()
{
    ASSERT(m_cacheDirPath.length());
    const char* path = m_cacheDirPath.data();

    DIR* cacheDir = opendir(path);
    if (!cacheDir) {
        ESCARGOT_LOG_ERROR("[CodeCache] can`t open cache directory : %s\n", path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(cacheDir)) != nullptr) {
        // skip the names "." and ".."
        if (!strncmp(entry->d_name, ".", 1) || !strncmp(entry->d_name, "..", 2)) {
            continue;
        }

        std::string entryPath(path);
        entryPath += entry->d_name;

        struct stat statEntry;
        // stat error
        if (stat(entryPath.data(), &statEntry) < 0) {
            ESCARGOT_LOG_ERROR("[CodeCache] stat error of %s occurred during clearing cache directory\n", entryPath.data());
            continue;
        }

        // another directory exists inside the cache directory
        if (S_ISDIR(statEntry.st_mode) != 0) {
            ESCARGOT_LOG_ERROR("[CodeCache] cache directory has another directory (%s) in itself during clearing cache directory\n", entryPath.data());
            continue;
        }

        // remove a file object
        if (unlink(entryPath.data()) != 0) {
            ESCARGOT_LOG_ERROR("[CodeCache] can`t remove a file (%s) during clearing cache directory\n", entryPath.data());
            continue;
        }
    }
    closedir(cacheDir);
}

void CodeCache::clear()
{
    m_currentContext.reset();

    unLockAndCloseCacheDir();

    m_cacheDirPath.clear();
    m_cacheList.clear();

    delete m_cacheWriter;
    delete m_cacheReader;

    // disable code cache
    m_enabled = false;
    m_status = Status::NONE;
}

void CodeCache::clearAll()
{
    // clear CodeCache and all cache files
    ASSERT(m_status == Status::FAILED || m_status == Status::NONE);
    clearCacheDir();
    clear();
}

void CodeCache::reset()
{
    ASSERT(m_enabled);

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
    ASSERT(m_enabled);

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
    ASSERT(m_enabled && !m_modified && m_status == Status::READY);
    ASSERT(m_cacheDirPath.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);

    m_status = Status::IN_PROGRESS;

    m_currentContext.m_cacheFilePath = m_cacheDirPath + std::to_string(srcHash);
    m_currentContext.m_cacheEntry = entry;
    m_currentContext.m_cacheStringTable = loadCacheStringTable(context);
}

void CodeCache::prepareCacheWriting(size_t srcHash)
{
    ASSERT(m_enabled && !m_modified && m_status == Status::READY);
    ASSERT(m_cacheDirPath.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);

    m_status = Status::IN_PROGRESS;

    m_currentContext.m_cacheFilePath = m_cacheDirPath + std::to_string(srcHash);
    m_currentContext.m_cacheStringTable = new CacheStringTable();
}

bool CodeCache::postCacheLoading()
{
    ASSERT(m_enabled);

    if (LIKELY(m_status == Status::FINISH)) {
        reset();
        m_status = Status::READY;

        return true;
    }

    // failed to load cache
    clearAll();

    return false;
}

void CodeCache::postCacheWriting(size_t srcHash)
{
    ASSERT(m_enabled);

    if (LIKELY(m_status == Status::FINISH)) {
        addCacheEntry(srcHash, m_currentContext.m_cacheEntry);
        if (writeCacheList()) {
            reset();
            m_status = Status::READY;

            return;
        }
    }

    // failed to write cache
    clearAll();
}

void CodeCache::storeStringTable()
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return;
    }

    ASSERT(!!m_currentContext.m_cacheStringTable);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);
    m_cacheWriter->storeStringTable();

    if (UNLIKELY(!writeCacheData(CodeCacheType::CACHE_STRING))) {
        m_status = Status::FAILED;
        return;
    }

    // the last stage of writing cache done
    m_status = Status::FINISH;
}

void CodeCache::storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock)
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return;
    }

    ASSERT(m_currentContext.m_cacheDataOffset == 0);
    ASSERT(!!m_currentContext.m_cacheStringTable);
    ASSERT(!!topCodeBlock);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);

    size_t nodeCount = 0;
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    if (UNLIKELY(!writeCacheData(CodeCacheType::CACHE_CODEBLOCK, nodeCount))) {
        m_status = Status::FAILED;
    }
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
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return;
    }

    ASSERT(!!m_currentContext.m_cacheStringTable);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);

    m_cacheWriter->storeByteCodeBlock(block);
    if (UNLIKELY(!writeCacheData(CodeCacheType::CACHE_BYTECODE))) {
        m_status = Status::FAILED;
    }
}

CacheStringTable* CodeCache::loadCacheStringTable(Context* context)
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return nullptr;
    }

    ASSERT(m_status == Status::IN_PROGRESS);

    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_STRING];

    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(m_currentContext.m_cacheFilePath.length());

    if (UNLIKELY(!readCacheData(metaInfo))) {
        m_status = Status::FAILED;
        return nullptr;
    }

    CacheStringTable* table = m_cacheReader->loadStringTable(context);

    // clear
    m_cacheReader->clearBuffer();
    return table;
}

InterpretedCodeBlock* CodeCache::loadCodeBlockTree(Context* context, Script* script)
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return nullptr;
    }

    ASSERT(m_status == Status::IN_PROGRESS);

    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_CODEBLOCK];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    if (UNLIKELY(!readCacheData(metaInfo))) {
        m_status = Status::FAILED;
        return nullptr;
    }

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
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return nullptr;
    }

    ASSERT(m_status == Status::IN_PROGRESS);

    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_BYTECODE];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE);

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    if (UNLIKELY(!readCacheData(metaInfo))) {
        m_status = Status::FAILED;
        return nullptr;
    }

    ByteCodeBlock* block = m_cacheReader->loadByteCodeBlock(context, topCodeBlock);

    // clear and finish
    m_cacheReader->clearBuffer();
    m_status = Status::FINISH;

    return block;
}

bool CodeCache::writeCacheList()
{
    ASSERT(m_enabled && m_modified);
    ASSERT(m_cacheList.size() > 0);
    ASSERT(m_cacheDirPath.length());

    std::string cacheListFilePath = m_cacheDirPath + CODE_CACHE_LIST_FILE_NAME;
    FILE* listFile = fopen(cacheListFilePath.data(), "wb");
    if (UNLIKELY(!listFile)) {
        return false;
    }

    bool failed = false;
    for (auto iter = m_cacheList.begin(); iter != m_cacheList.end(); iter++) {
        size_t srcHash = iter->first;
        CodeCacheEntry entry = iter->second;
        if (UNLIKELY(fwrite(&srcHash, sizeof(size_t), 1, listFile) != 1)) {
            failed = true;
            break;
        }
        if (UNLIKELY(fwrite(&entry, sizeof(CodeCacheEntry), 1, listFile) != 1)) {
            failed = true;
            break;
        }
    }

    if (UNLIKELY(failed)) {
        fclose(listFile);
        return false;
    }

    // FIXME frequent flush and fsync call can slow down the overall performance
    // cache list is the main file of code cache, so we directly flush and sync it
    fflush(listFile);
    fsync(fileno(listFile));
    fclose(listFile);
    m_modified = false;

    return true;
}

bool CodeCache::writeCacheData(CodeCacheType type, size_t extraCount)
{
    ASSERT(m_enabled);
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

    if (UNLIKELY(!dataFile)) {
        return false;
    }

    // write cache data
    if (UNLIKELY(fwrite(m_cacheWriter->bufferData(), sizeof(char), m_cacheWriter->bufferSize(), dataFile) != m_cacheWriter->bufferSize())) {
        fclose(dataFile);
        m_cacheWriter->clearBuffer();
        return false;
    }

    // FIXME frequent flush and fsync call can slow down the overall performance
    /* for performance issue, flush and fsync are skipped for cache data file now
    fflush(dataFile);
    fsync(fileno(dataFile));
    */
    fclose(dataFile);

    m_currentContext.m_cacheDataOffset += m_cacheWriter->bufferSize();
    m_cacheWriter->clearBuffer();
    return true;
}

bool CodeCache::readCacheData(CodeCacheMetaInfo& metaInfo)
{
    ASSERT(m_enabled);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK || metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE || metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_currentContext.m_cacheFilePath.length());

    size_t dataOffset = metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK ? 0 : metaInfo.dataOffset;
    FILE* dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "rb");

    if (UNLIKELY(!dataFile)) {
        return false;
    }

    if (UNLIKELY(fseek(dataFile, dataOffset, SEEK_SET) != 0)) {
        fclose(dataFile);
        return false;
    }

    if (UNLIKELY(!m_cacheReader->loadData(dataFile, metaInfo.dataSize))) {
        return false;
    }

    fclose(dataFile);
    return true;
}
}
#endif // ENABLE_CODE_CACHE
