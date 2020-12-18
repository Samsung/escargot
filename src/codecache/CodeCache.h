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

#ifndef __CodeCache__
#define __CodeCache__

#if defined(ENABLE_CODE_CACHE)

#ifndef OS_POSIX
#error "Code Cache does not support OS other than POSIX"
#endif

#if defined(ESCARGOT_ENABLE_TEST)
#define CODE_CACHE_MIN_SOURCE_LENGTH 1024
#else
#define CODE_CACHE_MIN_SOURCE_LENGTH 1024 * 4
#endif

namespace Escargot {

class Script;
class Context;
class CodeCacheWriter;
class CodeCacheReader;
class CacheStringTable;
class ByteCodeBlock;
class InterpretedCodeBlock;

enum class CodeCacheType : uint8_t {
    CACHE_CODEBLOCK = 0,
    CACHE_BYTECODE = 1,
    CACHE_STRING = 2,
    CACHE_INVALID = 3,
    CACHE_TYPE_NUM = CACHE_INVALID
};

struct CodeCacheMetaInfo {
    CodeCacheMetaInfo()
        : cacheType(CodeCacheType::CACHE_INVALID)
        , dataOffset(0)
        , dataSize(0)
    {
    }

    CodeCacheMetaInfo(CodeCacheType type, size_t offset, size_t size)
        : cacheType(type)
        , dataOffset(offset)
        , dataSize(size)
    {
    }

    CodeCacheType cacheType;
    union {
        size_t dataOffset; // data offset in cache data file
        size_t codeBlockCount; // total count of CodeBlocks used only for CodeBlockTree caching
    };
    size_t dataSize;
};

struct CodeCacheEntry {
    CodeCacheEntry()
        : m_lastWrittenTimeStamp(0)
    {
    }

    void reset()
    {
        m_lastWrittenTimeStamp = 0;
        for (size_t i = 0; i < (size_t)CodeCacheType::CACHE_TYPE_NUM; i++) {
            m_metaInfos[i].cacheType = CodeCacheType::CACHE_INVALID;
        }
    }

    uint64_t m_lastWrittenTimeStamp;
    CodeCacheMetaInfo m_metaInfos[(size_t)CodeCacheType::CACHE_TYPE_NUM];
};

class CodeCache {
public:
    enum class Status : uint8_t {
        NONE,
        READY,
        IN_PROGRESS,
        FINISH,
        FAILED,
    };

    struct CodeCacheContext {
        CodeCacheContext()
            : m_cacheStringTable(nullptr)
            , m_cacheDataOffset(0)
        {
        }

        void reset();

        std::string m_cacheFilePath; // current cache data file path
        CodeCacheEntry m_cacheEntry; // current cache entry
        CacheStringTable* m_cacheStringTable; // current CacheStringTable
        size_t m_cacheDataOffset; // current offset in cache data file
    };

    struct CodeCacheEntryChunk {
        CodeCacheEntryChunk()
            : m_srcHash(0)
        {
        }

        CodeCacheEntryChunk(size_t srcHash, const CodeCacheEntry& entry)
            : m_srcHash(srcHash)
            , m_entry(entry)
        {
        }

        size_t m_srcHash;
        CodeCacheEntry m_entry;
    };

    CodeCache(const char* baseCacheDir);
    ~CodeCache();

    bool enabled() { return m_enabled; }
    std::pair<bool, CodeCacheEntry> searchCache(size_t srcHash);

    void prepareCacheLoading(Context* context, size_t srcHash, const CodeCacheEntry& entry);
    void prepareCacheWriting(size_t srcHash);
    bool postCacheLoading();
    void postCacheWriting(size_t srcHash);

    void storeStringTable();
    void storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock);
    void storeByteCodeBlock(ByteCodeBlock* block);

    CacheStringTable* loadCacheStringTable(Context* context);
    InterpretedCodeBlock* loadCodeBlockTree(Context* context, Script* script);
    ByteCodeBlock* loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock);

    void clear();

private:
    std::string m_cacheDirPath;

    CodeCacheContext m_currentContext; // current CodeCache infos

    typedef std::unordered_map<size_t, CodeCacheEntry, std::hash<size_t>, std::equal_to<size_t>, std::allocator<std::pair<size_t const, CodeCacheEntry>>> CodeCacheListMap;
    CodeCacheListMap m_cacheList;

    CodeCacheWriter* m_cacheWriter;
    CodeCacheReader* m_cacheReader;

    int m_cacheDirFD; // CodeCache directory file descriptor
    bool m_enabled; // CodeCache enabled

    Status m_status; // current caching status

    void initialize(const char* baseCacheDir);
    bool tryInitCacheDir();
    bool tryInitCacheList();
    void unLockAndCloseCacheDir();
    void clearCacheDir();

    void clearAll();
    void reset();
    void setCacheEntry(const CodeCacheEntryChunk& entryChunk);
    bool addCacheEntry(size_t hash, const CodeCacheEntry& entry);

    bool removeLRUCacheEntry();
    bool removeCacheFile(size_t hash);

    void storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount);
    InterpretedCodeBlock* loadCodeBlockTreeNode(Script* script);

    bool writeCacheList();
    bool writeCacheData(CodeCacheType type, size_t extraCount = 0);
    bool readCacheData(CodeCacheMetaInfo& metaInfo);
};
} // namespace Escargot

#endif // ENABLE_CODE_CACHE

#endif
