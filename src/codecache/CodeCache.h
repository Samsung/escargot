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

namespace Escargot {

class Script;
class CodeCacheWriter;
class CodeCacheReader;
class CacheStringTable;
class ByteCodeBlock;
class InterpretedCodeBlock;

enum class CodeCacheType : uint8_t {
    CACHE_INVALID,
    CACHE_CODEBLOCK,
    CACHE_BYTECODE,
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
    size_t dataOffset;
    size_t dataSize;
};

// GC atomic allocator is used since Script has a reference to this map structure
typedef std::unordered_map<size_t, CodeCacheMetaInfo, std::hash<size_t>, std::equal_to<size_t>, GCUtil::gc_malloc_atomic_allocator<std::pair<size_t const, CodeCacheMetaInfo>>> CodeCacheMetaInfoMap;

class CodeCache {
public:
    CodeCache();
    ~CodeCache();

    CacheStringTable* addStringTable(Script* script);

    bool loadMetaInfos(Script* script);

    void storeStringTable(Script* script);
    CacheStringTable* loadStringTable(Context* context, Script* script);

    void storeCodeBlockTree(Script* script);
    void storeByteCodeBlock(Script* script, ByteCodeBlock* block);

    InterpretedCodeBlock* loadCodeBlockTree(Context* context, Script* script, CacheStringTable* table);
    ByteCodeBlock* loadByteCodeBlock(Context* context, Script* script, CacheStringTable* table);

private:
    CodeCacheWriter* m_writer;
    CodeCacheReader* m_reader;

    typedef std::unordered_map<Script*, CacheStringTable*, std::hash<void*>, std::equal_to<void*>, std::allocator<std::pair<Script* const, CacheStringTable*>>> CacheStringTableMap;
    CacheStringTableMap* m_stringTableMap;

    void storeCodeBlockTreeNode(InterpretedCodeBlock* codeBlock, size_t& nodeCount);
    InterpretedCodeBlock* loadCodeBlockTreeNode(Script* script);

    void writeCodeBlockToFile(Script* script, size_t nodeCount);
    void writeByteCodeBlockToFile(Script* script);
    void loadFromDataFile(Script* script, CodeCacheMetaInfo& metaInfo);

    void getMetaFileName(Script* script, char* buffer);
    void getStringFileName(Script* script, char* buffer);
    void getDataFileName(Script* script, char* buffer);
    void getByteDataFileName(Script* script, char* buffer);
};
}

#endif // ENABLE_CODE_CACHE

#endif
