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
#include "runtime/Context.h"
#include "runtime/String.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
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

static std::string createCacheFilePath(const std::string& cacheDirPath, const CodeCacheIndex& cacheIndex)
{
    std::stringstream ss;
    ss << cacheDirPath;
    cacheIndex.createCacheFileName(ss);
    return ss.str();
}

void CodeCache::CodeCacheContext::reset()
{
    m_cacheFilePath.clear();
    m_cacheEntry.reset();

    if (m_cacheFile) {
        fclose(m_cacheFile);
        m_cacheFile = nullptr;
    }

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
    , m_shouldLoadFunctionOnScriptLoading(CODE_CACHE_SHOULD_LOAD_FUNCTIONS_ON_SCRIPT_LOADING)
    , m_status(Status::NONE)
    , m_minSourceLength(CODE_CACHE_MIN_SOURCE_LENGTH)
    , m_maxCacheCount(CODE_CACHE_MAX_CACHE_COUNT)
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

#ifndef NDEBUG
    ESCARGOT_LOG_INFO("[CodeCache] initialization done on cache directory: %s\n", m_cacheDirPath.data());
#endif
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
    m_cacheLRUList.clear();

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
    auto iter = m_cacheList.find(entryChunk.m_index);
    ASSERT(iter == m_cacheList.end());
#endif
    m_cacheList.insert(std::make_pair(entryChunk.m_index, entryChunk.m_entry));
}

bool CodeCache::addCacheEntry(const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry)
{
    ASSERT(m_enabled);

#ifndef NDEBUG
    auto iter = m_cacheList.find(cacheIndex);
    ASSERT(iter == m_cacheList.end());
#endif
    if (m_cacheLRUList.size() == m_maxCacheCount) {
        if (UNLIKELY(!removeLRUCacheEntry())) {
            return false;
        }
    }

    m_cacheList.insert(std::make_pair(cacheIndex, entry));
    return true;
}

bool CodeCache::removeLRUCacheEntry()
{
    ASSERT(m_enabled);
    ASSERT(m_cacheLRUList.size() == m_maxCacheCount);

#ifndef NDEBUG
    uint64_t currentTimeStamp = fastTickCount();
#endif
    CodeCacheIndex::ScriptID lruIndex;
    uint64_t lruTimeStamp = std::numeric_limits<uint64_t>::max();
    for (auto iter = m_cacheLRUList.begin(); iter != m_cacheLRUList.end(); iter++) {
        uint64_t timeStamp = iter->second;
#ifndef NDEBUG
        ASSERT(timeStamp <= currentTimeStamp);
#endif
        if (lruTimeStamp > timeStamp) {
            lruIndex = iter->first;
            lruTimeStamp = timeStamp;
        }
    }

    if (UNLIKELY(!removeCacheFile(lruIndex))) {
        return false;
    }

    size_t eraseReturn = m_cacheLRUList.erase(lruIndex);
    ASSERT(eraseReturn == 1 && m_cacheLRUList.size() == m_maxCacheCount - 1);

    for (auto iter = m_cacheList.begin(); iter != m_cacheList.end();) {
        if (iter->first.scriptID() == lruIndex) {
            iter = m_cacheList.erase(iter);
        } else {
            iter++;
        }
    }

#ifndef NDEBUG
    ESCARGOT_LOG_INFO("[CodeCache] removeLRUCacheEntry %zu_%zu done\n", lruIndex.m_srcHash, lruIndex.m_srcLength);
#endif

    return true;
}

bool CodeCache::removeCacheFile(const CodeCacheIndex::ScriptID& scriptID)
{
    ASSERT(m_enabled);
    ASSERT(m_cacheDirPath.length());
    ASSERT(scriptID.m_srcHash && scriptID.m_srcLength);

    std::string filePath = createCacheFilePath(m_cacheDirPath, CodeCacheIndex(scriptID.m_srcHash, scriptID.m_srcLength, 0));
    if (remove(filePath.data()) != 0) {
        ESCARGOT_LOG_ERROR("[CodeCache] can`t remove a cache file %s\n", filePath.data());
        return false;
    }

#ifndef NDEBUG
    ESCARGOT_LOG_INFO("[CodeCache] remove a cache file %s\n", filePath.data());
#endif
    return true;
}

std::pair<bool, CodeCacheEntry> CodeCache::searchCache(const CodeCacheIndex& cacheIndex)
{
    ASSERT(m_enabled && cacheIndex.isValid());

    CodeCacheEntry entry;
    bool cacheHit = false;

    auto iter = m_cacheList.find(cacheIndex);
    if (iter != m_cacheList.end()) {
        cacheHit = true;
        entry = iter->second;
    }

    return std::make_pair(cacheHit, entry);
}

bool CodeCache::loadGlobalCache(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry, Script* script)
{
    ASSERT(m_enabled && cacheIndex.isValid());

    // load global CodeBlock and its related information
    prepareCacheLoading(context, cacheIndex, entry);

    InterpretedCodeBlock* topCodeBlock = loadCodeBlockTree(context, script);
    ByteCodeBlock* topByteCodeBlock = loadByteCodeBlock(context, topCodeBlock);

    if (!postCacheLoading()) {
        return false;
    }

    ASSERT(!!topCodeBlock && !!topByteCodeBlock);
    script->m_topCodeBlock = topCodeBlock;
    topCodeBlock->m_byteCodeBlock = topByteCodeBlock;

    ESCARGOT_LOG_INFO("[CodeCache] Load CodeCache Done (%s)\n", script->srcName()->toUTF8StringData().data());

    return true;
}

bool CodeCache::loadFunctionCache(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry, InterpretedCodeBlock* codeBlock)
{
    ASSERT(m_enabled && cacheIndex.isValid());

    // load function CodeBlock and its related information
    prepareCacheLoading(context, cacheIndex, entry);

    codeBlock->m_byteCodeBlock = loadByteCodeBlock(context, codeBlock);

    bool result = postCacheLoading();
    if (result) {
        ESCARGOT_LOG_INFO("[CodeCache] Load CodeCache Done (%s: index %zu size %zu)\n", codeBlock->script()->srcName()->toNonGCUTF8StringData().data(),
                          codeBlock->functionStart().index, codeBlock->src().length());
    }
    return result;
}

bool CodeCache::storeGlobalCache(Context* context, const CodeCacheIndex& cacheIndex, InterpretedCodeBlock* topCodeBlock, CodeBlockCacheInfo* codeBlockCacheInfo, Node* programNode, bool inWith)
{
    ASSERT(m_enabled && cacheIndex.isValid());

    // store global CodeBlock and its related information
    prepareCacheWriting(cacheIndex);

    // For storing cache, CodeBlockTree is firstly saved
    storeCodeBlockTree(topCodeBlock, codeBlockCacheInfo);

    // After CodeBlockTree, ByteCode and StringTable are stored sequentially
    try {
        topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(context, topCodeBlock, programNode, inWith, true);
    } catch (const char* message) {
        // handle failure
        m_status = Status::FAILED;
        postCacheWriting(cacheIndex);
        // rethrow
        throw;
    }

    bool result = postCacheWriting(cacheIndex);
#ifndef NDEBUG
    if (result) {
        ESCARGOT_LOG_INFO("[CodeCache] Store CodeCache Done (%s)\n", topCodeBlock->script()->srcName()->toUTF8StringData().data());
    }
#endif
    return result;
}

bool CodeCache::storeFunctionCache(Context* context, const CodeCacheIndex& cacheIndex, InterpretedCodeBlock* codeBlock, Node* functionNode)
{
    ASSERT(m_enabled && cacheIndex.isValid());

    // store function ByteCodeBlock and its related information
    prepareCacheWriting(cacheIndex);

    try {
        codeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(context, codeBlock, functionNode, false, true);
    } catch (const char* message) {
        // handle failure
        m_status = Status::FAILED;
        postCacheWriting(cacheIndex);
        // rethrow
        throw;
    }

    bool result = postCacheWriting(cacheIndex);
#ifndef NDEBUG
    if (result) {
        ESCARGOT_LOG_INFO("[CodeCache] Store CodeCache Done (%s: index %zu size %zu)\n", codeBlock->script()->srcName()->toNonGCUTF8StringData().data(),
                          codeBlock->functionStart().index, codeBlock->src().length());
    }
#endif
    return result;
}

void CodeCache::prepareCacheLoading(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry)
{
    ASSERT(m_enabled && m_status == Status::READY);
    ASSERT(m_cacheDirPath.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);
    ASSERT(cacheIndex.isValid());

    m_status = Status::IN_PROGRESS;

    m_currentContext.m_cacheFilePath = createCacheFilePath(m_cacheDirPath, cacheIndex);
    m_currentContext.m_cacheEntry = entry;
    FILE* dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "rb");
    if (UNLIKELY(!dataFile)) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        m_status = Status::FAILED;
        return;
    }
    m_currentContext.m_cacheFile = dataFile;
    m_currentContext.m_cacheStringTable = loadCacheStringTable(context);
}

void CodeCache::prepareCacheWriting(const CodeCacheIndex& cacheIndex)
{
    ASSERT(m_enabled && m_status == Status::READY);
    ASSERT(m_cacheDirPath.length());
    ASSERT(!m_currentContext.m_cacheFilePath.length());
    ASSERT(!m_currentContext.m_cacheStringTable);

    m_status = Status::IN_PROGRESS;

    m_currentContext.m_cacheFilePath = createCacheFilePath(m_cacheDirPath, cacheIndex);
    m_currentContext.m_cacheStringTable = new CacheStringTable();
    FILE* dataFile = fopen(m_currentContext.m_cacheFilePath.data(), "ab");
    if (UNLIKELY(!dataFile)) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't open the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        m_status = Status::FAILED;
        return;
    }
    m_currentContext.m_cacheFile = dataFile;
}

bool CodeCache::postCacheLoading()
{
    ASSERT(m_enabled);

    reset();

    if (LIKELY(m_status == Status::FINISH)) {
        m_status = Status::READY;
        return true;
    }

    // failed to load cache
    clearAll();
    m_status = Status::READY;
    return false;
}

bool CodeCache::postCacheWriting(const CodeCacheIndex& cacheIndex)
{
    ASSERT(m_enabled);

    if (LIKELY(m_status == Status::FINISH)) {
        // write time stamp
        m_cacheLRUList[cacheIndex.scriptID()] = fastTickCount();

        if (addCacheEntry(cacheIndex, m_currentContext.m_cacheEntry)) {
            if (writeCacheList()) {
                reset();
                m_status = Status::READY;

                return true;
            }
        }
    }

    // failed to write cache
    reset();
    clearAll();
    m_status = Status::READY;
    return false;
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

void CodeCache::storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock, CodeBlockCacheInfo* codeBlockCacheInfo)
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return;
    }

    ASSERT(!!codeBlockCacheInfo);
    ASSERT(m_currentContext.m_cacheDataOffset == 0);
    ASSERT(!!m_currentContext.m_cacheStringTable);
    ASSERT(!!topCodeBlock);

    m_cacheWriter->setStringTable(m_currentContext.m_cacheStringTable);
    m_cacheWriter->setCodeBlockCacheInfo(codeBlockCacheInfo);

    size_t nodeCount = 0;
    storeCodeBlockTreeNode(topCodeBlock, nodeCount);

    if (UNLIKELY(!writeCacheData(CodeCacheType::CACHE_CODEBLOCK, nodeCount))) {
        m_status = Status::FAILED;
    }
}

void CodeCache::storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount)
{
    ASSERT(!!codeBlock);
    ASSERT(m_cacheWriter->codeBlockCacheInfo()->m_codeBlockIndex.find(codeBlock)->second == nodeCount);

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

    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_CODEBLOCK];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK);

    InterpretedCodeBlock* topCodeBlock = nullptr;

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    if (UNLIKELY(!readCacheData(metaInfo))) {
        m_status = Status::FAILED;
        return nullptr;
    }

    ASSERT(GC_is_disabled());
    // temporal vector to keep the loaded InterpretedCodeBlock (GC is already disabled)
    std::vector<InterpretedCodeBlock*> tempCodeBlockVector;

    // CodeCacheMetaInfo::codeBlockCount has the value of nodeCount for CACHE_CODEBLOCK
    size_t nodeCount = metaInfo.codeBlockCount;
    tempCodeBlockVector.reserve(nodeCount);
    for (size_t i = 0; i < nodeCount; i++) {
        InterpretedCodeBlock* codeBlock = m_cacheReader->loadInterpretedCodeBlock(context, script);
        tempCodeBlockVector.push_back(codeBlock);

        if (i == 0) {
            // GlobalCodeBlock is firstly stored and loaded
            topCodeBlock = codeBlock;
        }
    }
    ASSERT(tempCodeBlockVector.size() == nodeCount);

    // link CodeBlock tree
    for (size_t i = 0; i < tempCodeBlockVector.size(); i++) {
        InterpretedCodeBlock* codeBlock = tempCodeBlockVector[i];
        size_t parentIndex = (size_t)codeBlock->parent();
        if (parentIndex == SIZE_MAX) {
            codeBlock->setParent(nullptr);
        } else {
            ASSERT(parentIndex < tempCodeBlockVector.size());
            codeBlock->setParent(tempCodeBlockVector[parentIndex]);
        }

        if (codeBlock->hasChildren()) {
            for (size_t j = 0; j < codeBlock->children().size(); j++) {
                size_t childIndex = (size_t)codeBlock->children()[j];
                ASSERT(childIndex < tempCodeBlockVector.size());
                codeBlock->children()[j] = tempCodeBlockVector[childIndex];
            }
        }
    }

    // load bytecode of functions
    if (m_shouldLoadFunctionOnScriptLoading) {
        loadAllByteCodeBlockOfFunctions(context, tempCodeBlockVector, script);
    }

    // clear
    tempCodeBlockVector.clear();
    m_cacheReader->clearBuffer();
    ASSERT(topCodeBlock->isGlobalCodeBlock());

#ifndef NDEBUG
    ESCARGOT_LOG_INFO("[CodeCache] Loading CodeBlock tree done\n");
#endif

    // return topCodeBlock
    return topCodeBlock;
}

ByteCodeBlock* CodeCache::loadByteCodeBlock(Context* context, InterpretedCodeBlock* codeBlock)
{
    if (m_status != Status::IN_PROGRESS) {
        // Caching process failed in the previous stage
        return nullptr;
    }

    CodeCacheMetaInfo& metaInfo = m_currentContext.m_cacheEntry.m_metaInfos[(size_t)CodeCacheType::CACHE_BYTECODE];

    ASSERT(!!context);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE);

    m_cacheReader->setStringTable(m_currentContext.m_cacheStringTable);
    if (UNLIKELY(!readCacheData(metaInfo))) {
        m_status = Status::FAILED;
        return nullptr;
    }

    ByteCodeBlock* block = m_cacheReader->loadByteCodeBlock(context, codeBlock);

    // clear and finish
    m_cacheReader->clearBuffer();
    m_status = Status::FINISH;

    return block;
}

void CodeCache::loadAllByteCodeBlockOfFunctions(Context* context, std::vector<InterpretedCodeBlock*>& codeBlockVector, Script* script)
{
    // load CodeBlock of functions during loading of global code
    ASSERT(m_enabled && m_status == Status::IN_PROGRESS);
    ASSERT(m_currentContext.m_cacheFilePath.length() && m_currentContext.m_cacheFile);

    size_t srcHash = script->sourceCodeHashValue();
    size_t srcLength = script->sourceCode()->length();

    // hold the current context
    CodeCacheContext previousContext = m_currentContext;
    m_cacheReader->clearBuffer();

    for (size_t i = 0; i < codeBlockVector.size(); i++) {
        InterpretedCodeBlock* codeBlock = codeBlockVector[i];
        ASSERT(script == codeBlock->script());
        auto result = searchCache(CodeCacheIndex(srcHash, srcLength, codeBlock->functionStart().index));
        if (result.first) {
            CodeCacheEntry& cacheEntry = result.second;

            // init context
            m_currentContext.m_cacheEntry = cacheEntry;
            m_currentContext.m_cacheStringTable = loadCacheStringTable(context);

            // load ByteCodeBlock
            codeBlock->m_byteCodeBlock = loadByteCodeBlock(context, codeBlock);

            // finalize
            if (m_currentContext.m_cacheStringTable) {
                delete m_currentContext.m_cacheStringTable;
                m_currentContext.m_cacheStringTable = nullptr;
            }

            if (LIKELY(m_status == Status::FINISH)) {
                m_status = Status::IN_PROGRESS;
            } else {
                ESCARGOT_LOG_ERROR("[CodeCache] load function cache data of %s failed\n", m_currentContext.m_cacheFilePath.data());
                break;
            }

            if (LIKELY(codeBlock->m_byteCodeBlock != nullptr)) {
#ifndef NDEBUG
                ESCARGOT_LOG_INFO("[CodeCache] Load CodeCache Done (%s: index %zu size %zu)\n", codeBlock->script()->srcName()->toNonGCUTF8StringData().data(),
                                  codeBlock->functionStart().index, codeBlock->src().length());
#endif
                auto& currentCodeSizeTotal = context->vmInstance()->compiledByteCodeSize();
                ASSERT(currentCodeSizeTotal < std::numeric_limits<size_t>::max());
                currentCodeSizeTotal += codeBlock->m_byteCodeBlock->memoryAllocatedSize();
            }
        }
    }

    // reset status
    m_status = Status::IN_PROGRESS;
    m_currentContext = previousContext;
}

bool CodeCache::writeCacheList()
{
    ASSERT(m_enabled);
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
    ASSERT(!!m_currentContext.m_cacheFilePath.length());
    ASSERT(!!m_currentContext.m_cacheFile);

    FILE* dataFile = m_currentContext.m_cacheFile;

    // meta info
    CodeCacheMetaInfo meta(type, m_currentContext.m_cacheDataOffset, m_cacheWriter->bufferSize());
    if (type == CodeCacheType::CACHE_CODEBLOCK) {
        ASSERT(m_currentContext.m_cacheDataOffset == 0);
        // extraCount represents the total count of CodeBlocks used only for CodeBlockTree caching
        meta.codeBlockCount = extraCount;
    }

    // record correct position for function
    if (type != CodeCacheType::CACHE_CODEBLOCK && m_currentContext.m_cacheDataOffset == 0) {
        meta.dataOffset = m_currentContext.m_cacheDataOffset = ftell(dataFile);
    }

    m_currentContext.m_cacheEntry.m_metaInfos[(size_t)type] = meta;

    // write cache data
    if (UNLIKELY(fwrite(m_cacheWriter->bufferData(), sizeof(char), m_cacheWriter->bufferSize(), dataFile) != m_cacheWriter->bufferSize())) {
        ESCARGOT_LOG_ERROR("[CodeCache] fwrite of %s failed\n", m_currentContext.m_cacheFilePath.data());
        m_cacheWriter->clearBuffer();
        return false;
    }

    fflush(dataFile);
    // FIXME frequent fsync calls can slow down the overall performance
    /* for performance issue, fsync is skipped for now
    fsync(fileno(dataFile));
    */

    m_currentContext.m_cacheDataOffset += m_cacheWriter->bufferSize();
    m_cacheWriter->clearBuffer();
    return true;
}

bool CodeCache::readCacheData(CodeCacheMetaInfo& metaInfo)
{
    ASSERT(m_enabled);
    ASSERT(metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK || metaInfo.cacheType == CodeCacheType::CACHE_BYTECODE || metaInfo.cacheType == CodeCacheType::CACHE_STRING);
    ASSERT(!!m_currentContext.m_cacheFilePath.length());
    ASSERT(!!m_currentContext.m_cacheFile);

    size_t dataOffset = metaInfo.cacheType == CodeCacheType::CACHE_CODEBLOCK ? 0 : metaInfo.dataOffset;
    FILE* dataFile = m_currentContext.m_cacheFile;

    if (UNLIKELY(fseek(dataFile, dataOffset, SEEK_SET) != 0)) {
        ESCARGOT_LOG_ERROR("[CodeCache] can't seek the cache data file %s\n", m_currentContext.m_cacheFilePath.data());
        return false;
    }

    if (UNLIKELY(!m_cacheReader->loadData(dataFile, metaInfo.dataSize))) {
        ESCARGOT_LOG_ERROR("[CodeCache] load cache data of %s failed\n", m_currentContext.m_cacheFilePath.data());
        return false;
    }

    return true;
}

size_t CodeCache::minSourceLength()
{
    return m_minSourceLength;
}

void CodeCache::setMinSourceLength(size_t s)
{
    m_minSourceLength = s;
}

size_t CodeCache::maxCacheCount()
{
    return m_maxCacheCount;
}

void CodeCache::setMaxCacheCount(size_t s)
{
    m_maxCacheCount = s;
}

bool CodeCache::shouldLoadFunctionOnScriptLoading()
{
    return m_shouldLoadFunctionOnScriptLoading;
}

void CodeCache::setShouldLoadFunctionOnScriptLoading(bool s)
{
    m_shouldLoadFunctionOnScriptLoading = s;
}
} // namespace Escargot
#endif // ENABLE_CODE_CACHE
