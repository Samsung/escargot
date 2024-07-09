/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

namespace WTF {

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html

// LChar data is interpreted as Latin-1-encoded (zero-extended to 16 bits).

// NOTE: The hash computation here must stay in sync with the create_hash_table script in
// JavaScriptCore and the CodeGeneratorJS.pm script in WebCore.

class SuperFastHash {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr unsigned flagCount = StringHasher::flagCount;
    static constexpr unsigned maskHash = StringHasher::maskHash;
    typedef StringHasher::DefaultConverter DefaultConverter;

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        return StringHasher::finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        return StringHasher::finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data));
    }

    template<typename T, typename Converter = DefaultConverter>
    static unsigned computeHash(const T* data, unsigned length)
    {
        return StringHasher::finalize(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter = DefaultConverter>
    static unsigned computeHash(const T* data)
    {
        return StringHasher::finalize(computeHashImpl<T, Converter>(data));
    }
private:
    friend class StringHasher;

    static unsigned calculateWithRemainingLastCharacter(unsigned hash, unsigned character)
    {
        unsigned result = hash;

        result += character;
        result ^= result << 11;
        result += result >> 17;

        return result;
    }

    ALWAYS_INLINE static unsigned calculateWithTwoCharacters(unsigned hash, unsigned firstCharacter, unsigned secondCharacter)
    {
        unsigned result = hash;

        result += firstCharacter;
        result = (result << 16) ^ ((secondCharacter << 11) ^ result);
        result += result >> 11;

        return result;
    }

    template<typename T, typename Converter>
    static unsigned computeHashImpl(const T* characters, unsigned length)
    {
        unsigned result = stringHashingStartValue;
        bool remainder = length & 1;
        length >>= 1;

        while (length--) {
            result = calculateWithTwoCharacters(result, Converter::convert(characters[0]), Converter::convert(characters[1]));
            characters += 2;
        }

        if (remainder)
            return calculateWithRemainingLastCharacter(result, Converter::convert(characters[0]));
        return result;
    }

    template<typename T, typename Converter>
    static unsigned computeHashImpl(const T* characters)
    {
        unsigned result = stringHashingStartValue;
        while (T a = *characters++) {
            T b = *characters++;
            if (!b)
                return calculateWithRemainingLastCharacter(result, Converter::convert(a));
            result = calculateWithTwoCharacters(result, Converter::convert(a), Converter::convert(b));
        }
        return result;
    }

};

} // namespace WTF

using WTF::SuperFastHash;
