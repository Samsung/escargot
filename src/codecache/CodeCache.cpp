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
#include "runtime/String.h"
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
#define CODE_CACHE_MAX_CACHE_NUM 32

namespace Escargot {

void CodeCache::CodeCacheContext::reset()
{
    m_cacheFilePath.clear();
    m_cacheEntry.reset();

    if (m_cacheStringTable) {
        delete m_cacheStringTable;
        m_cacheStringTable = nullptr;
    }
    m_cacheDataOffset = 0;
}

CodeCache::CodeCache(const char* baseCacheDir)
    : m_cacheWriter(nullptr)
    , m_cacheReader(nullptr)
    , m_cacheDirFD(-1)
    , m_enabled(false)
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
            ESCARGOT_LOG_ERROR("[CodeCache] can't properly close cache directory %s\n", m_cacheDirPath.data());
            return false;
        }
    } else {
        int ret = mkdir(m_cacheDirPath.data(), 0755);
        if (ret != 0) {
            ESCARGOT_LOG_ERROR("[CodeCache] can't properly generate cache directory %s\n", m_cacheDirPath.data());
            return false;
        }
    }

    // open cache directory
    if ((m_cacheDirFD = open(m_cacheDirPath.data(), O_RDONLY)) == -1) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't properly open cache directory %s\n", m_cacheDirPath.data());
        return false;
    }

    // lock cache directory
    ASSERT(m_cacheDirFD != -1);
    if (flock(m_cacheDirFD, LOCK_EX | LOCK_NB) == -1) {
        ESCARGOT_LOG_ERROR("[CodeCache] cache directory (%s) lock failed\n", m_cacheDirPath.data());
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

    std::string listFilePath = m_cacheDirPath + CODE_CACHE_LIST_FILE_NAME;

    struct stat statFile;
    if (stat(listFilePath.data(), &statFile) != 0) {
        // there is no list file
        // cache has not been saved or all files might be removed
        return true;
    }

    // check file permission
    if (!S_ISREG(statFile.st_mode) || !(statFile.st_mode & S_IRUSR) || !(statFile.st_mode & S_IWUSR)) {
        ESCARGOT_LOG_ERROR("[CodeCache] limited cache file (%s) permission\n", listFilePath.data());
        return false;
    }

    // open list file
    FILE* listFile = fopen(listFilePath.data(), "rb");
    if (!listFile) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache list file %s\n", listFilePath.data());
        return false;
    }

    // check Escargot version
    size_t cacheVerHash;
    if (UNLIKELY(fread(&cacheVerHash, sizeof(size_t), 1, listFile) != 1)) {
        ESCARGOT_LOG_ERROR("[CodeCache] fread of %s failed\n", listFilePath.data());
        fclose(listFile);
        return false;
    }
    std::string currentVer = ESCARGOT_VERSION;
    ASSERT(currentVer.length() > 0);
    size_t currentVerHash = std::hash<std::string>{}(currentVer);
    if (UNLIKELY(currentVerHash != cacheVerHash)) {
        ESCARGOT_LOG_ERROR("[CodeCache] Different Escargot version (current: %zu / cache: %zu), clear cache\n", currentVerHash, cacheVerHash);
        fclose(listFile);
        return false;
    }

    // load list entries from file
    size_t listSize;
    if (UNLIKELY(fread(&listSize, sizeof(size_t), 1, listFile) != 1)) {
        ESCARGOT_LOG_ERROR("[CodeCache] fread of %s failed\n", listFilePath.data());
        fclose(listFile);
        return false;
    }

    ASSERT(listSize > 0 && listSize <= CODE_CACHE_MAX_CACHE_NUM);
    for (size_t i = 0; i < listSize; i++) {
        CodeCacheEntryChunk entryChunk;
        if (UNLIKELY(fread(&entryChunk, sizeof(CodeCacheEntryChunk), 1, listFile) != 1)) {
            ESCARGOT_LOG_ERROR("[CodeCache] fread of %s failed\n", listFilePath.data());
            fclose(listFile);
            return false;
        }

        setCacheEntry(entryChunk);
    }

    fclose(listFile);
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
        if (stat(entryPath.data(), &statEntry) != 0) {
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

    if (m_cacheWriter) {
        delete m_cacheWriter;
        m_cacheWriter = nullptr;
    }
    if (m_cacheReader) {
        delete m_cacheReader;
        m_cacheReader = nullptr;
    }

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

void CodeCache::setCacheEntry(const CodeCacheEntryChunk& entryChunk)
{
#ifndef NDEBUG
    auto iter = m_cacheList.find(entryChunk.m_srcHash);
    ASSERT(iter == m_cacheList.end());
#endif
    m_cacheList.insert(std::make_pair(entryChunk.m_srcHash, entryChunk.m_entry));
}

bool CodeCache::addCacheEntry(size_t hash, const CodeCacheEntry& entry)
{
    ASSERT(m_enabled);

#ifndef NDEBUG
    auto iter = m_cacheList.find(hash);
    ASSERT(iter == m_cacheList.end());
#endif
    if (m_cacheList.size() == CODE_CACHE_MAX_CACHE_NUM) {
        if (UNLIKELY(!removeLRUCacheEntry())) {
            return false;
        }
    }

    m_cacheList.insert(std::make_pair(hash, entry));
    return true;
}

bool CodeCache::removeLRUCacheEntry()
{
    ASSERT(m_enabled);
    ASSERT(m_cacheList.size() == CODE_CACHE_MAX_CACHE_NUM);

#ifndef NDEBUG
    uint64_t currentTimeStamp = fastTickCount();
#endif
    size_t lruHash = 0;
    uint64_t lruTimeStamp = std::numeric_limits<uint64_t>::max();
    for (auto iter = m_cacheList.begin(); iter != m_cacheList.end(); iter++) {
        uint64_t timeStamp = iter->second.m_lastWrittenTimeStamp;
#ifndef NDEBUG
        ASSERT(timeStamp <= currentTimeStamp);
#endif
        if (lruTimeStamp > timeStamp) {
            lruHash = iter->first;
            lruTimeStamp = timeStamp;
        }
    }

    if (UNLIKELY(!removeCacheFile(lruHash))) {
        return false;
    }

    size_t eraseReturn = m_cacheList.erase(lruHash);
    ASSERT(eraseReturn == 1 && m_cacheList.size() == CODE_CACHE_MAX_CACHE_NUM - 1);

    return true;
}

bool CodeCache::removeCacheFile(size_t hash)
{
    ASSERT(m_enabled);
    ASSERT(m_cacheDirPath.length());

    std::string filePath = m_cacheDirPath + std::to_string(hash);

    if (remove(filePath.data()) != 0) {
        ESCARGOT_LOG_ERROR("[CodeCache] can`t remove a cache file %s\n", filePath.data());
        return false;
    }

    return true;
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
    ASSERT(m_enabled && m_status == Status::READY);
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
    ASSERT(m_enabled && m_status == Status::READY);
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
        CodeCacheEntry& entry = m_currentContext.m_cacheEntry;
        ASSERT(entry.m_lastWrittenTimeStamp == 0);

        // write time stamp
        entry.m_lastWrittenTimeStamp = fastTickCount();

        if (addCacheEntry(srcHash, m_currentContext.m_cacheEntry)) {
            if (writeCacheList()) {
                reset();
                m_status = Status::READY;

                return;
            }
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
    ASSERT(m_enabled);
    ASSERT(m_cacheList.size() > 0 && m_cacheList.size() <= CODE_CACHE_MAX_CACHE_NUM);
    ASSERT(m_cacheDirPath.length());

    std::string cacheListFilePath = m_cacheDirPath + CODE_CACHE_LIST_FILE_NAME;
    FILE* listFile = fopen(cacheListFilePath.data(), "wb");
    if (UNLIKELY(!listFile)) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache list file %s\n", cacheListFilePath.data());
        return false;
    }

    // first write Escargot version
    std::string version = ESCARGOT_VERSION;
    ASSERT(version.length() > 0);
    size_t versionHash = std::hash<std::string>{}(version);
    if (UNLIKELY(fwrite(&versionHash, sizeof(size_t), 1, listFile) != 1)) {
        ESCARGOT_LOG_ERROR("[CodeCache] fwrite of %s failed\n", cacheListFilePath.data());
        fclose(listFile);
        return false;
    }

    size_t listSize = m_cacheList.size();
    // write the number of cache entries
    if (UNLIKELY(fwrite(&listSize, sizeof(size_t), 1, listFile) != 1)) {
        ESCARGOT_LOG_ERROR("[CodeCache] fwrite of %s failed\n", cacheListFilePath.data());
        fclose(listFile);
        return false;
    }

    size_t entryCount = 0;
    auto iter = m_cacheList.begin();
    while (entryCount < listSize) {
        ASSERT(iter != m_cacheList.end());

        CodeCacheEntryChunk entryChunk(iter->first, iter->second);
        if (UNLIKELY(fwrite(&entryChunk, sizeof(CodeCacheEntryChunk), 1, listFile) != 1)) {
            ESCARGOT_LOG_ERROR("[CodeCache] fwrite of %s failed\n", cacheListFilePath.data());
            fclose(listFile);
            return false;
        }

        entryCount++;
        iter++;
    }
    ASSERT(iter == m_cacheList.end());

    fflush(listFile);
    // FIXME frequent fsync calls can slow down the overall performance
    /* for performance issue, fsync is skipped for now
    fsync(fileno(listFile));
    */
    fclose(listFile);
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
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        return false;
    }

    // write cache data
    if (UNLIKELY(fwrite(m_cacheWriter->bufferData(), sizeof(char), m_cacheWriter->bufferSize(), dataFile) != m_cacheWriter->bufferSize())) {
        ESCARGOT_LOG_ERROR("[CodeCache] fwrite of %s failed\n", m_currentContext.m_cacheFilePath.data());
        fclose(dataFile);
        m_cacheWriter->clearBuffer();
        return false;
    }

    fflush(dataFile);
    // FIXME frequent fsync calls can slow down the overall performance
    /* for performance issue, fsync is skipped for now
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
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        return false;
    }

    if (UNLIKELY(fseek(dataFile, dataOffset, SEEK_SET) != 0)) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't seek the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        fclose(dataFile);
        return false;
    }

    if (UNLIKELY(!m_cacheReader->loadData(dataFile, metaInfo.dataSize))) {
        ESCARGOT_LOG_ERROR("[CodeCache] load cache data of %s failed\n", m_currentContext.m_cacheFilePath.data());
        fclose(dataFile);
        return false;
    }

    fclose(dataFile);
    return true;
}
} // namespace Escargot
#endif // ENABLE_CODE_CACHE
