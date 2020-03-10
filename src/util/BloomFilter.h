/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#ifndef __EscargotBloomFilter__
#define __EscargotBloomFilter__

#include <bitset>
#include "runtime/AtomicString.h"

namespace Escargot {

template <unsigned tableSizeInBit>
class BloomFilter {
public:
    static const long long keyMask = tableSizeInBit - 1;

    BloomFilter()
    {
    }

    void add(unsigned hash)
    {
        m_table[hash & keyMask] = true;
        m_table[(hash >> 16) & keyMask] = true;
    }
    bool mayContain(unsigned hash) const
    {
        return m_table[hash & keyMask] && m_table[(hash >> 16) & keyMask];
    }

    void clear()
    {
        m_table.reset();
    }

    void add(const AtomicString& string)
    {
        add(hashValue(string));
    }
    bool mayContain(const AtomicString& string) const
    {
        return mayContain(hashValue(string));
    }

private:
    static unsigned hashValue(const AtomicString& s)
    {
        std::hash<Escargot::AtomicString> hasher;
        return hasher(s);
    }

    std::bitset<tableSizeInBit> m_table;
};
}

#endif
