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

#ifndef __CodeCacheReaderWriter__
#define __CodeCacheReaderWriter__

#if defined(ENABLE_CODE_CACHE)

#include "util/Vector.h"

namespace Escargot {

class Script;
class Context;
class StringView;
class AtomicString;
class ByteCodeBlock;
class InterpretedCodeBlock;
struct CodeBlockCacheInfo;

// size_t and limb_t should have the same size (for putBF and getBF)
COMPILE_ASSERT(sizeof(size_t) == sizeof(limb_t), "");

enum class ByteCodeRelocType : uint8_t {
    RELOC_INVALID,
    RELOC_STRING,
    RELOC_ATOMICSTRING,
    RELOC_BIGINT,
    RELOC_CODEBLOCK,
    RELOC_BLOCKINFO,
    RELOC_FREEZEFUNC,
};

struct ByteCodeRelocInfo {
    ByteCodeRelocInfo()
        : relocType(ByteCodeRelocType::RELOC_INVALID)
        , codeOffset(SIZE_MAX)
        , dataOffset(SIZE_MAX)
    {
    }

    ByteCodeRelocInfo(ByteCodeRelocType type, size_t cOffset, size_t dOffset)
        : relocType(type)
        , codeOffset(cOffset)
        , dataOffset(dOffset)
    {
    }

    ByteCodeRelocType relocType;
    size_t codeOffset;
    size_t dataOffset;
};

class CacheStringTable {
public:
    CacheStringTable()
        : m_has16BitString(false)
        , m_maxLength(0)
    {
    }

    ~CacheStringTable()
    {
        m_table.clear();
    }

    Vector<AtomicString, std::allocator<AtomicString>>& table()
    {
        return m_table;
    }

    bool has16BitString() const { return m_has16BitString; }
    size_t maxLength() const { return m_maxLength; }
    size_t add(const AtomicString& string);
    void initAdd(const AtomicString& string);
    AtomicString& get(size_t index);

private:
    bool m_has16BitString;
    size_t m_maxLength;
    Vector<AtomicString, std::allocator<AtomicString>> m_table;
};

class CodeCacheWriter {
public:
    class CacheBuffer {
    public:
        CacheBuffer()
            : m_buffer(nullptr)
            , m_capacity(0)
            , m_index(0)
        {
        }

        ~CacheBuffer()
        {
            reset();
        }

        char* data() const { return m_buffer; }
        size_t size() const { return m_index; }
        inline bool isAvailable(size_t size) const
        {
            return m_index + size <= m_capacity;
        }

        void ensureSize(size_t size);
        void reset();

        template <typename IntegralType>
        void put(IntegralType value)
        {
            ASSERT(isAvailable(sizeof(IntegralType)));
            *reinterpret_cast<IntegralType*>(m_buffer + m_index) = value;
            m_index += sizeof(IntegralType);
        }

        template <typename IntegralType>
        void putData(IntegralType* data, size_t size)
        {
            ensureSize(sizeof(size_t) + size * sizeof(IntegralType));
            put(size);
            if (UNLIKELY(!size)) {
                return;
            }
            memcpy(m_buffer + m_index, data, size * sizeof(IntegralType));
            m_index += (size * sizeof(IntegralType));
        }

        void putString(String* string)
        {
            ASSERT(string->length());
            bool is8Bit = string->has8BitContent();
            ensureSize(sizeof(bool));
            put(is8Bit);
            if (LIKELY(is8Bit)) {
                putData(string->characters8(), string->length());
            } else {
                putData(string->characters16(), string->length());
            }
        }

        void putBF(bf_t* bf)
        {
            ensureSize(sizeof(int) + sizeof(slimb_t));
            put(bf->sign);
            put(bf->expn);
            putData(bf->tab, bf->len);
        }

    private:
        char* m_buffer;
        size_t m_capacity;
        size_t m_index;
    };

    CodeCacheWriter()
        : m_stringTable(nullptr)
        , m_codeBlockCacheInfo(nullptr)
    {
    }

    ~CodeCacheWriter()
    {
        m_stringTable = nullptr;
        m_codeBlockCacheInfo = nullptr;
        clearBuffer();
    }

    void setStringTable(CacheStringTable* table)
    {
        ASSERT(!!table);
        m_stringTable = table;
    }

    CacheStringTable* stringTable()
    {
        return m_stringTable;
    }

    void setCodeBlockCacheInfo(CodeBlockCacheInfo* info)
    {
        ASSERT(!m_codeBlockCacheInfo && !!info);
        m_codeBlockCacheInfo = info;
    }

    CodeBlockCacheInfo* codeBlockCacheInfo()
    {
        return m_codeBlockCacheInfo;
    }

    char* bufferData() { return m_buffer.data(); }
    size_t bufferSize() const { return m_buffer.size(); }
    void clearBuffer()
    {
        m_codeBlockCacheInfo = nullptr;
        m_buffer.reset();
    }
    void storeInterpretedCodeBlock(InterpretedCodeBlock* codeBlock);
    void storeByteCodeBlock(ByteCodeBlock* block);
    void storeStringTable();

private:
    CacheBuffer m_buffer;
    CacheStringTable* m_stringTable;
    CodeBlockCacheInfo* m_codeBlockCacheInfo;

    void storeByteCodeStream(ByteCodeBlock* block);
    void storeGlobalVariableAccessCache(Context* context);
};

class CodeCacheReader {
public:
    class CacheBuffer {
    public:
        CacheBuffer()
            : m_buffer(nullptr)
            , m_capacity(0)
            , m_index(0)
        {
        }

        ~CacheBuffer()
        {
            reset();
        }

        char* data() const { return m_buffer; }
        size_t size() const { return m_index; }
        size_t index() const { return m_index; }
        void resize(size_t size);
        void reset();

        template <typename IntegralType>
        IntegralType get()
        {
            ASSERT(m_index < m_capacity);
            IntegralType value = *(reinterpret_cast<IntegralType*>(m_buffer + m_index));
            m_index += sizeof(IntegralType);
            return value;
        }

        template <typename IntegralType>
        void getData(IntegralType* data, size_t size)
        {
            if (UNLIKELY(!size)) {
                return;
            }
            size_t dataSize = size * sizeof(IntegralType);
            memcpy(data, m_buffer + m_index, dataSize);
            m_index += dataSize;
        }

        // FIXME
        String* getString()
        {
            String* str = nullptr;
            bool is8Bit = get<bool>();
            size_t length = get<size_t>();
            ASSERT(length);
            if (LIKELY(is8Bit)) {
                LChar* buffer = ALLOCA(sizeof(LChar) * (length + 1), LChar, nullptr);
                buffer[length] = '\0';
                getData(buffer, length);
                str = new Latin1String(buffer, length);
            } else {
                UChar* buffer = ALLOCA(sizeof(UChar) * (length + 1), UChar, nullptr);
                buffer[length] = '\0';
                getData(buffer, length);
                str = new UTF16String(buffer, length);
            }
            return str;
        }

        bf_t getBF(bf_context_t* bfContext)
        {
            bf_t bf;
            bf_init(bfContext, &bf);
            bf.sign = get<int>();
            bf.expn = get<slimb_t>();

            limb_t len = get<limb_t>();
            bf_resize(&bf, len);
            getData(bf.tab, len);
            return bf;
        }

    private:
        char* m_buffer;
        size_t m_capacity;
        size_t m_index;
    };

    CodeCacheReader()
        : m_stringTable(nullptr)
    {
    }

    ~CodeCacheReader()
    {
        m_stringTable = nullptr;
        clearBuffer();
    }

    void setStringTable(CacheStringTable* table)
    {
        ASSERT(!!table);
        m_stringTable = table;
    }

    CacheStringTable* stringTable()
    {
        return m_stringTable;
    }

    char* bufferData() { return m_buffer.data(); }
    size_t bufferIndex() const { return m_buffer.index(); }
    void clearBuffer() { m_buffer.reset(); }
    bool loadData(FILE*, size_t);

    InterpretedCodeBlock* loadInterpretedCodeBlock(Context* context, Script* script);
    ByteCodeBlock* loadByteCodeBlock(Context* context, InterpretedCodeBlock* topCodeBlock);
    CacheStringTable* loadStringTable(Context* context);

private:
    CacheBuffer m_buffer;
    CacheStringTable* m_stringTable;

    void loadByteCodeStream(Context* context, ByteCodeBlock* block);
    void loadGlobalVariableAccessCache(Context* context);
};
} // namespace Escargot

#endif // ENABLE_CODE_CACHE

#endif
