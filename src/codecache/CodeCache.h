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
class Node;

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

struct CodeCacheIndex {
    size_t m_srcHash; // hash value of source code
    size_t m_srcLength; // length of source code
    size_t m_functionIndex; // index of function start position (SIZE_MAX for global code)

    struct ScriptID {
        size_t m_srcHash;
        size_t m_srcLength;

        ScriptID()
            : m_srcHash(0)
            , m_srcLength(0)
        {
        }

        ScriptID(size_t srcHash, size_t srcLength)
            : m_srcHash(srcHash)
            , m_srcLength(srcLength)
        {
            ASSERT(srcHash && srcLength);
        }

        bool operator==(const ScriptID& src) const
        {
            return m_srcHash == src.m_srcHash && m_srcLength == src.m_srcLength;
        }
    };

    CodeCacheIndex()
        : m_srcHash(0)
        , m_srcLength(0)
        , m_functionIndex(0)
    {
    }

    CodeCacheIndex(size_t srcHash, size_t srcLength, size_t funcIndex)
        : m_srcHash(srcHash)
        , m_srcLength(srcLength)
        , m_functionIndex(funcIndex)
    {
        ASSERT(isValid());
    }

    ScriptID scriptID() const
    {
        ASSERT(isValid());
        return ScriptID(m_srcHash, m_srcLength);
    }

    bool isValid() const
    {
        return m_srcHash && m_srcLength;
    }

    void createCacheFileName(std::stringstream& ss) const
    {
        ss << m_srcHash;
        ss << '_';
        ss << m_srcLength;
    }

    bool operator==(const CodeCacheIndex& src) const
    {
        return m_srcHash == src.m_srcHash && m_srcLength == src.m_srcLength && m_functionIndex == src.m_functionIndex;
    }
};

} // namespace Escargot

namespace std {
template <>
struct hash<Escargot::CodeCacheIndex> {
    size_t operator()(Escargot::CodeCacheIndex const& x) const
    {
        return x.m_srcHash + x.m_functionIndex;
    }
};

template <>
struct equal_to<Escargot::CodeCacheIndex> {
    bool operator()(Escargot::CodeCacheIndex const& a, Escargot::CodeCacheIndex const& b) const
    {
        return a == b;
    }
};

template <>
struct hash<Escargot::CodeCacheIndex::ScriptID> {
    size_t operator()(Escargot::CodeCacheIndex::ScriptID const& x) const
    {
        return x.m_srcHash;
    }
};

template <>
struct equal_to<Escargot::CodeCacheIndex::ScriptID> {
    bool operator()(Escargot::CodeCacheIndex::ScriptID const& a, Escargot::CodeCacheIndex::ScriptID const& b) const
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
    friend class ByteCodeGenerator;

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
            : m_cacheFile(nullptr)
            , m_cacheStringTable(nullptr)
            , m_cacheDataOffset(0)
        {
        }

        void reset();

        std::string m_cacheFilePath; // current cache data file path
        CodeCacheEntry m_cacheEntry; // current cache entry
        FILE* m_cacheFile; // current cache data file
        CacheStringTable* m_cacheStringTable; // current CacheStringTable
        size_t m_cacheDataOffset; // current offset in cache data file
    };

    struct CodeCacheEntryChunk {
        CodeCacheEntryChunk()
        {
        }

        CodeCacheEntryChunk(const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry)
            : m_index(cacheIndex)
            , m_entry(entry)
        {
        }

        CodeCacheIndex m_index;
        CodeCacheEntry m_entry;
    };

    CodeCache(const char* baseCacheDir);
    ~CodeCache();

    bool enabled() const { return m_enabled; }
    std::pair<bool, CodeCacheEntry> searchCache(const CodeCacheIndex& cacheIndex);

    bool loadGlobalCache(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry, Script* script);
    bool loadFunctionCache(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry, InterpretedCodeBlock* codeBlock);
    bool storeGlobalCache(Context* context, const CodeCacheIndex& cacheIndex, InterpretedCodeBlock* topCodeBlock, CodeBlockCacheInfo* codeBlockCacheInfo, Node* programNode, bool inWith);
    bool storeFunctionCache(Context* context, const CodeCacheIndex& cacheIndex, InterpretedCodeBlock* codeBlock, Node* functionNode);

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

    typedef std::unordered_map<CodeCacheIndex, CodeCacheEntry, std::hash<CodeCacheIndex>, std::equal_to<CodeCacheIndex>, std::allocator<std::pair<CodeCacheIndex const, CodeCacheEntry>>> CodeCacheListMap;
    CodeCacheListMap m_cacheList;
    typedef std::unordered_map<CodeCacheIndex::ScriptID, uint64_t, std::hash<CodeCacheIndex::ScriptID>, std::equal_to<CodeCacheIndex::ScriptID>, std::allocator<std::pair<CodeCacheIndex::ScriptID const, uint64_t>>> CodeCacheLRUList; /* <Hash, TimeStamp> */
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
    bool addCacheEntry(const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry);

    bool removeLRUCacheEntry();
    bool removeCacheFile(const CodeCacheIndex::ScriptID& scriptID);

    void prepareCacheLoading(Context* context, const CodeCacheIndex& cacheIndex, const CodeCacheEntry& entry);
    bool postCacheLoading();
    CacheStringTable* loadCacheStringTable(Context* context);
    InterpretedCodeBlock* loadCodeBlockTree(Context* context, Script* script);
    ByteCodeBlock* loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock);
    void loadAllByteCodeBlockOfFunctions(Context* context, std::vector<InterpretedCodeBlock*>& codeBlockVector, Script* script);

    void prepareCacheWriting(const CodeCacheIndex& cacheIndex);
    bool postCacheWriting(const CodeCacheIndex& cacheIndex);
    void storeStringTable();
    void storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock, CodeBlockCacheInfo* codeBlockCacheInfo);
    void storeByteCodeBlock(ByteCodeBlock* block);

    void storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount);
    InterpretedCodeBlock* loadCodeBlockTreeNode(Script* script);

    bool writeCacheList();
    bool writeCacheData(CodeCacheType type, size_t extraCount = 0);
    bool readCacheData(CodeCacheMetaInfo& metaInfo);
};
} // namespace Escargot

#endif // ENABLE_CODE_CACHE

#endif
