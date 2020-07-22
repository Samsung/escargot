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

#define INITIAL_CACHE_BUFFER_SIZE 1024 * 4

namespace Escargot {

class Script;
class StringView;
class AtomicString;
class InterpretedCodeBlock;

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

    bool has16BitString() { return m_has16BitString; }
    size_t maxLength() { return m_maxLength; }
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
            : m_buffer(static_cast<char*>(malloc(INITIAL_CACHE_BUFFER_SIZE)))
            , m_capacity(INITIAL_CACHE_BUFFER_SIZE)
            , m_index(0)
        {
        }

        ~CacheBuffer()
        {
            reset();
        }

        char* data() const { return m_buffer; }
        size_t size() const { return m_index; }
        inline bool isAvailable(size_t size)
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

    private:
        char* m_buffer;
        size_t m_capacity;
        size_t m_index;
    };

    CodeCacheWriter()
        : m_stringTable(nullptr)
    {
    }

    ~CodeCacheWriter()
    {
        clear();
    }

    void setStringTable(CacheStringTable* table)
    {
        m_stringTable = table;
    }

    char* bufferData() { return m_buffer.data(); }
    size_t bufferSize() { return m_buffer.size(); }
    void clear() { m_buffer.reset(); }
    void storeInterpretedCodeBlock(InterpretedCodeBlock* codeBlock);
    void storeByteCodeBlock();

private:
    CacheBuffer m_buffer;
    CacheStringTable* m_stringTable;
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
        clear();
    }

    void setStringTable(CacheStringTable* table)
    {
        m_stringTable = table;
    }

    char* bufferData() { return m_buffer.data(); }
    size_t bufferIndex() { return m_buffer.index(); }
    void clear() { m_buffer.reset(); }
    void loadDataFile(FILE*, size_t);

    InterpretedCodeBlock* loadInterpretedCodeBlock(Context* context, Script* script);

private:
    CacheBuffer m_buffer;
    CacheStringTable* m_stringTable;
};
}

#endif // ENABLE_CODE_CACHE

#endif
