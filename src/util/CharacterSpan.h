/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifndef __EscargotCharacterSpan__
#define __EscargotCharacterSpan__

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(ESCARGOT_LITTLE_ENDIAN)
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#define ESCARGOT_CHARACTER_SPAN_SSE2
#elif defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define ESCARGOT_CHARACTER_SPAN_NEON
#endif
#endif

namespace Escargot {

namespace CharacterSpanDetail {

#if defined(ESCARGOT_CHARACTER_SPAN_SSE2) || defined(ESCARGOT_CHARACTER_SPAN_NEON)
static constexpr size_t vectorBytes = 16;
#if defined(ESCARGOT_CHARACTER_SPAN_SSE2)
using CharacterVector = __m128i;

inline CharacterVector loadVector(const void* input)
{
    return _mm_loadu_si128(static_cast<const __m128i*>(input));
}

inline void storeVector(void* output, CharacterVector input)
{
    _mm_storeu_si128(static_cast<__m128i*>(output), input);
}

inline CharacterVector orVector(CharacterVector a, CharacterVector b) { return _mm_or_si128(a, b); }
inline CharacterVector zeroVector() { return _mm_setzero_si128(); }

template <typename CharType>
inline CharacterVector splatVector(CharType value)
{
    return sizeof(CharType) == 1 ? _mm_set1_epi8(static_cast<char>(value)) : _mm_set1_epi16(static_cast<short>(value));
}

template <typename CharType>
inline CharacterVector equalVector(CharacterVector a, CharacterVector b)
{
    return sizeof(CharType) == 1 ? _mm_cmpeq_epi8(a, b) : _mm_cmpeq_epi16(a, b);
}

template <typename CharType>
inline CharacterVector lessVector(CharacterVector input, CharacterVector limit)
{
    const auto sign = splatVector<CharType>(CharType(1u << (sizeof(CharType) * 8 - 1)));
    input = _mm_xor_si128(input, sign);
    limit = _mm_xor_si128(limit, sign);
    return sizeof(CharType) == 1 ? _mm_cmpgt_epi8(limit, input) : _mm_cmpgt_epi16(limit, input);
}

inline CharacterVector prefixVector(CharacterVector input, size_t bytes)
{
    const auto positions = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    return _mm_and_si128(input, _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(bytes)), positions));
}

inline size_t firstMatchingByte(CharacterVector matches)
{
    const unsigned mask = static_cast<unsigned>(_mm_movemask_epi8(matches));
    if (!mask) {
        return vectorBytes;
    }
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    return __builtin_ctz(mask);
#elif defined(_MSC_VER)
    unsigned long position;
    _BitScanForward(&position, mask);
    return position;
#else
    size_t position = 0;
    while (!(mask & (1u << position))) {
        ++position;
    }
    return position;
#endif
}
#else
using CharacterVector = uint8x16_t;

inline CharacterVector loadVector(const void* input) { return vld1q_u8(static_cast<const uint8_t*>(input)); }
inline void storeVector(void* output, CharacterVector input) { vst1q_u8(static_cast<uint8_t*>(output), input); }
inline CharacterVector orVector(CharacterVector a, CharacterVector b) { return vorrq_u8(a, b); }
inline CharacterVector zeroVector() { return vdupq_n_u8(0); }

template <typename CharType>
inline CharacterVector splatVector(CharType value)
{
    return sizeof(CharType) == 1 ? vdupq_n_u8(static_cast<uint8_t>(value)) : vreinterpretq_u8_u16(vdupq_n_u16(static_cast<uint16_t>(value)));
}

template <typename CharType>
inline CharacterVector equalVector(CharacterVector a, CharacterVector b)
{
    return sizeof(CharType) == 1 ? vceqq_u8(a, b) : vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
}

template <typename CharType>
inline CharacterVector lessVector(CharacterVector input, CharacterVector limit)
{
    return sizeof(CharType) == 1 ? vcltq_u8(input, limit) : vreinterpretq_u8_u16(vcltq_u16(vreinterpretq_u16_u8(input), vreinterpretq_u16_u8(limit)));
}

inline CharacterVector prefixVector(CharacterVector input, size_t bytes)
{
    const uint8x16_t positions = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    return vandq_u8(input, vcltq_u8(positions, vdupq_n_u8(static_cast<uint8_t>(bytes))));
}

inline size_t firstMatchingByte(CharacterVector matches)
{
    if (!vmaxvq_u8(matches)) {
        return vectorBytes;
    }
    const uint8x16_t positions = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    return vminvq_u8(vornq_u8(positions, matches));
}
#endif

inline void collectVector(CharacterVector input, uintptr_t& codeUnits)
{
    uintptr_t words[vectorBytes / sizeof(uintptr_t)];
    storeVector(words, input);
    for (size_t i = 0; i < vectorBytes / sizeof(uintptr_t); ++i) {
        codeUnits |= words[i];
    }
}
#endif

} // namespace CharacterSpanDetail

// Find either delimiter or a code unit below limit in a non-null, bounded
// span. Word-parallel tests work on every architecture and byte order.
// On little-endian machines the first flagged lane is the exact first match;
// subtraction may only flag additional, later lanes through a borrow. Other
// byte orders check the flagged word scalarly.
// Comparisons use unsigned code units; limit must be at most half their range.
template <bool collectCodeUnits, typename CharType>
inline size_t findFirstOfOrLessThanImpl(const CharType* data, size_t length,
                                        CharType first, CharType second, CharType limit,
                                        uintptr_t& codeUnits)
{
    static_assert(sizeof(CharType) == 1 || sizeof(CharType) == 2, "Expected 8-bit or UTF-16 code units");
    using Word = uintptr_t;
    using UnsignedChar = typename std::make_unsigned<CharType>::type;
    constexpr unsigned bits = sizeof(CharType) * 8;
    constexpr Word ones = std::numeric_limits<Word>::max() / ((Word(1) << bits) - 1);
    constexpr Word highBits = ones << (bits - 1);
    constexpr size_t lanes = sizeof(Word) / sizeof(CharType);
    const Word firstWord = ones * static_cast<UnsignedChar>(first);
    const Word secondWord = ones * static_cast<UnsignedChar>(second);
    const Word limitWord = ones * static_cast<UnsignedChar>(limit);
    ASSERT(static_cast<UnsignedChar>(limit) <= (Word(1) << (bits - 1)));

    size_t position = 0;
#if defined(ESCARGOT_CHARACTER_SPAN_SSE2) || defined(ESCARGOT_CHARACTER_SPAN_NEON)
    constexpr size_t vectorLanes = CharacterSpanDetail::vectorBytes / sizeof(CharType);
    if (length >= vectorLanes) {
        using namespace CharacterSpanDetail;
        const auto firstVector = splatVector(first);
        const auto secondVector = splatVector(second);
        const auto limitVector = splatVector(limit);
        auto accumulated = zeroVector();
        while (length - position >= vectorLanes) {
            const auto input = loadVector(data + position);
            auto matches = orVector(orVector(equalVector<CharType>(input, firstVector), equalVector<CharType>(input, secondVector)), lessVector<CharType>(input, limitVector));
            const size_t firstByte = firstMatchingByte(matches);
            if (firstByte != vectorBytes) {
                const size_t firstLane = firstByte / sizeof(CharType);
                if (collectCodeUnits) {
                    collectVector(orVector(accumulated, prefixVector(input, firstLane * sizeof(CharType))), codeUnits);
                }
                return position + firstLane;
            }
            if (collectCodeUnits) {
                accumulated = orVector(accumulated, input);
            }
            position += vectorLanes;
        }
        if (collectCodeUnits) {
            collectVector(accumulated, codeUnits);
        }
    }
#endif
    while (length - position >= lanes) {
        Word word;
        memcpy(&word, data + position, sizeof(word));
        const Word a = word ^ firstWord;
        const Word b = word ^ secondWord;
        const Word firstMatch = (a - ones) & ~a & highBits;
        const Word secondMatch = (b - ones) & ~b & highBits;
        const Word belowLimit = (word - limitWord) & ~word & highBits;
        const Word matches = firstMatch | secondMatch | belowLimit;
        if (matches) {
#if defined(ESCARGOT_LITTLE_ENDIAN) && (defined(COMPILER_GCC) || defined(COMPILER_CLANG))
            const size_t firstLane = __builtin_ctzll(static_cast<unsigned long long>(matches)) / bits;
            if (collectCodeUnits) {
                codeUnits |= word & ((Word(1) << (firstLane * bits)) - 1);
            }
#else
            CharType characters[lanes];
            memcpy(characters, &word, sizeof(word));
            size_t firstLane = 0;
            while (firstLane < lanes) {
                const UnsignedChar c = static_cast<UnsignedChar>(characters[firstLane]);
                if (c == static_cast<UnsignedChar>(first) || c == static_cast<UnsignedChar>(second) || c < static_cast<UnsignedChar>(limit)) {
                    break;
                }
                if (collectCodeUnits) {
                    codeUnits |= c;
                }
                ++firstLane;
            }
#endif
            return position + firstLane;
        }
        if (collectCodeUnits) {
            codeUnits |= word;
        }
        position += lanes;
    }
    while (position < length) {
        const UnsignedChar c = static_cast<UnsignedChar>(data[position]);
        if (c == static_cast<UnsignedChar>(first) || c == static_cast<UnsignedChar>(second) || c < static_cast<UnsignedChar>(limit)) {
            break;
        }
        if (collectCodeUnits) {
            codeUnits |= c;
        }
        position++;
    }
    return position;
}

template <typename CharType>
inline size_t findFirstOfOrLessThan(const CharType* data, size_t length,
                                    CharType first, CharType second, CharType limit)
{
    uintptr_t ignored = 0;
    return findFirstOfOrLessThanImpl<false>(data, length, first, second, limit, ignored);
}

// Also accumulate the code-unit bits of the accepted prefix, excluding the
// delimiter and every later lane. A caller can classify width while scanning.
template <typename CharType>
inline size_t findFirstOfOrLessThan(const CharType* data, size_t length,
                                    CharType first, CharType second, CharType limit,
                                    uintptr_t& codeUnits)
{
    return findFirstOfOrLessThanImpl<true>(data, length, first, second, limit, codeUnits);
}

} // namespace Escargot

#undef ESCARGOT_CHARACTER_SPAN_SSE2
#undef ESCARGOT_CHARACTER_SPAN_NEON

#endif
