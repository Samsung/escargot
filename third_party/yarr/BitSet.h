/*
 *  Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <array>
#include <string.h>
#include <type_traits>

namespace WTF {

template<size_t size>
using BitSetWordType = std::conditional_t<(size <= 32 && sizeof(UCPURegister) > sizeof(uint32_t)), uint32_t, UCPURegister>;

template<size_t bitSetSize, typename PassedWordType = BitSetWordType<bitSetSize>>
class BitSet final {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using WordType = PassedWordType;

    static_assert(sizeof(WordType) <= sizeof(UCPURegister), "WordType must not be bigger than the CPU atomic word size");
    BitSet() = default;

    static size_t size()
    {
        return bitSetSize;
    }

    bool get(size_t) const;
    void set(size_t);
    void set(size_t, bool);
    bool testAndSet(size_t); // Returns the previous bit value.
    bool testAndClear(size_t); // Returns the previous bit value.
    size_t nextPossiblyUnset(size_t) const;
    void clear(size_t);
    void clearAll();
    void setAll();
    void invert();
    int64_t findRunOfZeros(size_t runLength) const;
    size_t count(size_t start = 0) const;
    bool isEmpty() const;
    bool isFull() const;

    void merge(const BitSet&);
    void filter(const BitSet&);
    void exclude(const BitSet&);

    bool subsumes(const BitSet&) const;

    size_t findBit(size_t startIndex, bool value) const;

    class iterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        iterator()
            : m_bitSet(nullptr)
            , m_index(0)
        {
        }

        iterator(const BitSet& bitSet, size_t index)
            : m_bitSet(&bitSet)
            , m_index(index)
        {
        }

        size_t operator*() const { return m_index; }

        iterator& operator++()
        {
            m_index = m_bitSet->findBit(m_index + 1, true);
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            return !operator==(other);
        }

    private:
        const BitSet* m_bitSet;
        size_t m_index;
    };

    // Use this to iterate over set bits.
    iterator begin() const { return iterator(*this, findBit(0, true)); }
    iterator end() const { return iterator(*this, bitSetSize); }

    void mergeAndClear(BitSet&);
    void setAndClear(BitSet&);

    void setEachNthBit(size_t n, size_t start = 0, size_t end = bitSetSize);

    bool operator==(const BitSet&) const;

    void operator|=(const BitSet&);
    void operator&=(const BitSet&);
    void operator^=(const BitSet&);

    WordType* storage() { return bits.data(); }
    const WordType* storage() const { return bits.data(); }

    size_t storageLengthInBytes() { return sizeof(bits); }

private:
    void cleanseLastWord();

    static constexpr unsigned wordSize = sizeof(WordType) * 8;
    static constexpr unsigned words = (bitSetSize + wordSize - 1) / wordSize;

    // the literal '1' is of type signed int.  We want to use an unsigned
    // version of the correct size when doing the calculations because if
    // WordType is larger than int, '1 << 31' will first be sign extended
    // and then casted to unsigned, meaning that set(31) when WordType is
    // a 64 bit unsigned int would give 0xffff8000
    static constexpr WordType one = 1;

    std::array<WordType, words> bits { };
};

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::get(size_t n) const
{
    return !!(bits[n / wordSize] & (one << (n % wordSize)));
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE void BitSet<bitSetSize, WordType>::set(size_t n)
{
    bits[n / wordSize] |= (one << (n % wordSize));
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE void BitSet<bitSetSize, WordType>::set(size_t n, bool value)
{
    if (value)
        set(n);
    else
        clear(n);
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::testAndSet(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool previousValue = bits[index] & mask;
    bits[index] |= mask;
    return previousValue;
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::testAndClear(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool previousValue = bits[index] & mask;
    bits[index] &= ~mask;
    return previousValue;
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::clear(size_t n)
{
    bits[n / wordSize] &= ~(one << (n % wordSize));
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::clearAll()
{
    memset(bits.data(), 0, sizeof(bits));
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::cleanseLastWord()
{
    if (!!(bitSetSize % wordSize)) {
        size_t remainingBits = bitSetSize % wordSize;
        WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
        bits[words - 1] &= mask;
    }
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::setAll()
{
    memset(bits.data(), 0xFF, sizeof(bits));
    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::invert()
{
    for (size_t i = 0; i < words; ++i)
        bits[i] = ~bits[i];
    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline size_t BitSet<bitSetSize, WordType>::nextPossiblyUnset(size_t start) const
{
    if (!~bits[start / wordSize])
        return ((start / wordSize) + 1) * wordSize;
    return start + 1;
}

template<size_t bitSetSize, typename WordType>
inline int64_t BitSet<bitSetSize, WordType>::findRunOfZeros(size_t runLength) const
{
    if (!runLength)
        runLength = 1;

    if (runLength > bitSetSize)
        return -1;

    for (size_t i = 0; i <= (bitSetSize - runLength) ; i++) {
        bool found = true;
        for (size_t j = i; j <= (i + runLength - 1) ; j++) {
            if (get(j)) {
                found = false;
                break;
            }
        }
        if (found)
            return i;
    }
    return -1;
}

template<size_t bitSetSize, typename WordType>
inline size_t BitSet<bitSetSize, WordType>::count(size_t start) const
{
    size_t result = 0;
    for ( ; (start % wordSize); ++start) {
        if (get(start))
            ++result;
    }
    for (size_t i = start / wordSize; i < words; ++i)
        result += WTF::bitCount(bits[i]);
    return result;
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::isEmpty() const
{
    for (size_t i = 0; i < words; ++i)
        if (bits[i])
            return false;
    return true;
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::isFull() const
{
    for (size_t i = 0; i < words; ++i)
        if (~bits[i]) {
            if (!!(bitSetSize % wordSize)) {
                if (i == words - 1) {
                    size_t remainingBits = bitSetSize % wordSize;
                    WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
                    if ((bits[i] & mask) == mask)
                        return true;
                }
            }
            return false;
        }
    return true;
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::merge(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::filter(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::exclude(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= ~other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::subsumes(const BitSet& other) const
{
    for (size_t i = 0; i < words; ++i) {
        WordType myBits = bits[i];
        WordType otherBits = other.bits[i];
        if ((myBits | otherBits) != myBits)
            return false;
    }
    return true;
}

template<size_t bitSetSize, typename WordType>
inline size_t BitSet<bitSetSize, WordType>::findBit(size_t startIndex, bool value) const
{
    WordType skipValue = -(static_cast<WordType>(value) ^ 1);
    size_t wordIndex = startIndex / wordSize;
    size_t startIndexInWord = startIndex - wordIndex * wordSize;

    while (wordIndex < words) {
        WordType word = bits[wordIndex];
        if (word != skipValue) {
            size_t index = startIndexInWord;
            if (findBitInWord(word, index, wordSize, value))
                return wordIndex * wordSize + index;
        }

        wordIndex++;
        startIndexInWord = 0;
    }

    return bitSetSize;
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::mergeAndClear(BitSet& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] |= other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::setAndClear(BitSet& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] = other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::setEachNthBit(size_t n, size_t start, size_t end)
{
    ASSERT(start <= end);
    ASSERT(end <= bitSetSize);

    size_t wordIndex = start / wordSize;
    size_t endWordIndex = end / wordSize;
    size_t index = start - wordIndex * wordSize;
    while (wordIndex < endWordIndex) {
        while (index < wordSize) {
            bits[wordIndex] |= (one << index);
            index += n;
        }
        index -= wordSize;
        wordIndex++;
    }

    size_t endIndex = end - endWordIndex * wordSize;
    while (index < endIndex) {
        bits[wordIndex] |= (one << index);
        index += n;
    }

    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline bool BitSet<bitSetSize, WordType>::operator==(const BitSet& other) const
{
    for (size_t i = 0; i < words; ++i) {
        if (bits[i] != other.bits[i])
            return false;
    }
    return true;
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::operator|=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::operator&=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::operator^=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] ^= other.bits[i];
}

} // namespace WTF

// We can't do "using WTF::BitSet;" here because there is a function in the macOS SDK named BitSet() already.
