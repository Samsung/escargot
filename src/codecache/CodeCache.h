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

#ifndef CODE_CACHE_MIN_SOURCE_LENGTH
#define CODE_CACHE_MIN_SOURCE_LENGTH 1024 * 8
#endif

#ifndef CODE_CACHE_MAX_CACHE_COUNT
#define CODE_CACHE_MAX_CACHE_COUNT 16
#endif

#ifndef CODE_CACHE_SHOULD_LOAD_FUNCTIONS_ON_SCRIPT_LOADING
#define CODE_CACHE_SHOULD_LOAD_FUNCTIONS_ON_SCRIPT_LOADING false
#endif

namespace Escargot {

class Script;
class Context;
class CodeCacheWriter;
class CodeCacheReader;
class CacheStringTable;
class ByteCodeBlock;
class InterpretedCodeBlock;

struct CodeBlockCacheInfo {
    CodeBlockCacheInfo()
        : m_codeBlockCount(0)
    {
    }

    size_t m_codeBlockCount;
    std::unordered_map<InterpretedCodeBlock*, size_t, std::hash<void*>, std::equal_to<void*>, std::allocator<std::pair<InterpretedCodeBlock* const, size_t>>> m_codeBlockIndex;
};

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

struct CodeCacheItem {
    size_t m_srcHash;
    Optional<size_t> m_functionSourceIndex;
    CodeCacheItem(size_t srcHash, Optional<size_t> functionSourceIndex)
        : m_srcHash(srcHash)
        , m_functionSourceIndex(functionSourceIndex)
    {
    }

    bool operator==(const CodeCacheItem& src) const
    {
        return m_srcHash == src.m_srcHash && m_functionSourceIndex == src.m_functionSourceIndex;
    }
};

} // namespace Escargot

namespace std {
template <>
struct hash<Escargot::CodeCacheItem> {
    size_t operator()(Escargot::CodeCacheItem const& x) const
    {
        return x.m_srcHash + (x.m_functionSourceIndex ? x.m_functionSourceIndex.value() : 0);
    }
};

template <>
struct equal_to<Escargot::CodeCacheItem> {
    bool operator()(Escargot::CodeCacheItem const& a, Escargot::CodeCacheItem const& b) const
    {
        return a == b;
    }
};
} // namespace std

namespace Escargot {

struct CodeCacheEntry {
    CodeCacheEntry()
    {
    }

    void reset()
    {
        for (size_t i = 0; i < (size_t)CodeCacheType::CACHE_TYPE_NUM; i++) {
            m_metaInfos[i].cacheType = CodeCacheType::CACHE_INVALID;
        }
    }
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

        CodeCacheEntryChunk(size_t srcHash, Optional<size_t> functionSourceIndex, const CodeCacheEntry& entry)
            : m_srcHash(srcHash)
            , m_functionSourceIndex(functionSourceIndex)
            , m_entry(entry)
        {
        }

        size_t m_srcHash;
        Optional<size_t> m_functionSourceIndex;
        CodeCacheEntry m_entry;
    };

    CodeCache(const char* baseCacheDir);
    ~CodeCache();

    bool enabled() const { return m_enabled; }
    std::pair<bool, CodeCacheEntry> searchCache(size_t srcHash, Optional<size_t> functionSourceIndex);

    void prepareCacheLoading(Context* context, size_t srcHash, Optional<size_t> functionSourceIndex, const CodeCacheEntry& entry);
    void prepareCacheWriting(size_t srcHash, Optional<size_t> functionSourceIndex);
    bool postCacheLoading();
    void postCacheWriting(size_t srcHash, Optional<size_t> functionSourceIndex);

    void storeStringTable();
    void storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock, CodeBlockCacheInfo* codeBlockCacheInfo);
    void storeByteCodeBlock(ByteCodeBlock* block);

    CacheStringTable* loadCacheStringTable(Context* context);
    InterpretedCodeBlock* loadCodeBlockTree(Context* context, Script* script);
    ByteCodeBlock* loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock);

    void clear();

    size_t minSourceLength();
    void setMinSourceLength(size_t s);
    size_t maxCacheCount();
    void setMaxCacheCount(size_t s);
    bool shouldLoadFunctionOnScriptLoading();
    void setShouldLoadFunctionOnScriptLoading(bool s);

private:
    std::string m_cacheDirPath;

    CodeCacheContext m_currentContext; // current CodeCache infos

    typedef std::unordered_map<CodeCacheItem, CodeCacheEntry, std::hash<CodeCacheItem>, std::equal_to<CodeCacheItem>, std::allocator<std::pair<CodeCacheItem const, CodeCacheEntry>>> CodeCacheListMap;
    CodeCacheListMap m_cacheList;
    typedef std::unordered_map<size_t, uint64_t> CodeCacheLRUList; /* <Hash, TimeStamp> */
    CodeCacheLRUList m_cacheLRUList;

    CodeCacheWriter* m_cacheWriter;
    CodeCacheReader* m_cacheReader;

    int m_cacheDirFD; // CodeCache directory file descriptor
    bool m_enabled; // CodeCache enabled
    bool m_shouldLoadFunctionOnScriptLoading;
    Status m_status; // current caching status

    size_t m_minSourceLength;
    size_t m_maxCacheCount;

    void initialize(const char* baseCacheDir);
    bool tryInitCacheDir();
    bool tryInitCacheList();
    void unLockAndCloseCacheDir();
    void clearCacheDir();

    void clearAll();
    void reset();
    void setCacheEntry(const CodeCacheEntryChunk& entryChunk);
    bool addCacheEntry(size_t hash, Optional<size_t> functionSourceIndex, const CodeCacheEntry& entry);

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
