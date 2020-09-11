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

#define CODE_CACHE_FILE_NAME_LENGTH 32
#define CODE_CACHE_MIN_SOURCE_LENGTH 1024 * 4

namespace Escargot {

class Script;
class Context;
class CodeCacheWriter;
class CodeCacheReader;
class CacheStringTable;
class ByteCodeBlock;
class InterpretedCodeBlock;

enum class CodeCacheType : uint8_t {
    CACHE_INVALID,
    CACHE_CODEBLOCK,
    CACHE_BYTECODE,
    CACHE_STRING,
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

struct CodeCacheMetaChunk {
    CodeCacheMetaInfo codeBlockInfo;
    CodeCacheMetaInfo byteCodeInfo;
    CodeCacheMetaInfo stringInfo;
};

class CodeCache {
public:
    struct CodeCacheContext {
        static const char* filePathPrefix;

        CodeCacheContext()
            : dataOffset(0)
            , stringTable(nullptr)
            , metaFileName(nullptr)
            , dataFileName(nullptr)
        {
        }

        void initStringTable();
        void initStringTable(CacheStringTable* table);
        void initFileName(String* srcName);
        void reset();

        size_t srcHashValue; // hash value of target source code
        size_t dataOffset; // current offset in cache data file
        CacheStringTable* stringTable; // current CacheStringTable

        char* metaFileName;
        char* dataFileName;
    };

    CodeCache();
    ~CodeCache();

    std::pair<bool, CodeCacheMetaChunk> tryLoadCodeCacheMetaInfo(String* srcName, String* source);

    CodeCacheContext& context() { return m_context; }
    void storeStringTable();
    void storeCodeBlockTree(InterpretedCodeBlock* topCodeBlock);
    void storeByteCodeBlock(ByteCodeBlock* block);

    CacheStringTable* loadStringTable(Context* context, Script* script, CodeCacheMetaInfo metaInfo);
    InterpretedCodeBlock* loadCodeBlockTree(Context* context, Script* script, CodeCacheMetaInfo metaInfo);
    ByteCodeBlock* loadByteCodeBlock(Context* context, Script* script, CodeCacheMetaInfo metaInfo);

private:
    CodeCacheContext m_context;

    CodeCacheWriter* m_writer;
    CodeCacheReader* m_reader;

    void storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount);
    InterpretedCodeBlock* loadCodeBlockTreeNode(Script* script);

    void writeCacheToFile(CodeCacheType type, size_t extraCount = 0);
    void loadCacheFromDataFile(CodeCacheMetaInfo& metaInfo);
    void removeCacheFiles();
};
}

#endif // ENABLE_CODE_CACHE

#endif
