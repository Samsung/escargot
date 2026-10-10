/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Portions adapted from V8 RegExp optimizations.
 * Copyright 2011 the V8 project authors. All rights reserved.
 * Copyright 2019 the V8 project authors. All rights reserved.
 * Copyright 2014, the V8 project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WTFBridge.h"
#include "YarrInterpreter.h"

#include "Yarr.h"
#include "YarrCanonicalize.h"
#include "BumpPointerAllocator.h"

using namespace WTF;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Yarr {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BytecodePattern);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ByteDisjunction);

template<typename CharType>
class Interpreter {
public:
    static constexpr bool verbose = false;
    static constexpr char32_t errorCodePoint = 0xFFFFFFFFu;

    struct ParenthesesDisjunctionContext;

    struct BackTrackInfoParentheses {
        uintptr_t begin;
        uintptr_t matchAmount;
        ParenthesesDisjunctionContext* lastContext;
    };

    static inline void appendParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack, ParenthesesDisjunctionContext* context)
    {
        context->next = backTrack->lastContext;
        backTrack->lastContext = context;
        ++backTrack->matchAmount;
    }

    static inline void popParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack)
    {
        RELEASE_ASSERT(backTrack->matchAmount);
        RELEASE_ASSERT(backTrack->lastContext);
        backTrack->lastContext = backTrack->lastContext->next;
        --backTrack->matchAmount;
    }

    struct DisjunctionContext
    {
        DisjunctionContext() = default;

        void* operator new(size_t, void* where)
        {
            return where;
        }

        static size_t allocationSize(unsigned numberOfFrames)
        {
            size_t rawSize = sizeof(DisjunctionContext) - sizeof(uintptr_t) + Checked<size_t>(numberOfFrames) * sizeof(uintptr_t);
            size_t roundedSize = roundUpToMultipleOf<sizeof(void*)>(rawSize);
            RELEASE_ASSERT(roundedSize >= rawSize);
            return roundedSize;
        }

        ByteTerm* term { nullptr };
        unsigned matchBegin;
        unsigned matchEnd;
#if ASSERT_ENABLED
        constexpr static uint64_t magicNumber = 0x01aabbccddeeff01;
        uint64_t m_magicNumber { magicNumber };
#endif
        uintptr_t frame[1];
    };

    DisjunctionContext* allocDisjunctionContext(ByteDisjunction* disjunction)
    {
        size_t size = DisjunctionContext::allocationSize(disjunction->m_frameSize);
        auto* newAllocatorPool = allocatorPool->ensureCapacity(size);
        if (UNLIKELY(!newAllocatorPool))
            return nullptr;
        allocatorPool = newAllocatorPool;
        return new (allocatorPool->alloc(size)) DisjunctionContext();
    }

    void freeDisjunctionContext(DisjunctionContext* context)
    {
#if ASSERT_ENABLED
        ASSERT(context->m_magicNumber == DisjunctionContext::magicNumber);
        context->m_magicNumber = 0;
#endif
        allocatorPool = allocatorPool->dealloc(context);
    }

    struct ParenthesesDisjunctionContext
    {
        ParenthesesDisjunctionContext(BytecodePattern* pattern, unsigned* output, ByteTerm& term, unsigned numDuplicateNamedGroups, BitVector& duplicateNamedGroups)
            : m_pattern(pattern)
            , m_numNestedSubpatterns(term.atom.parenthesesDisjunction->m_numSubpatterns)
            , m_duplicateNamedGroups(duplicateNamedGroups)

        {
            m_numBackupIds = m_numNestedSubpatterns * 2 + numDuplicateNamedGroups;
            unsigned firstSubpatternId = term.subpatternId();

            for (unsigned i = 0; i < (m_numNestedSubpatterns << 1); ++i) {
                subpatternAndGroupIdBackup[i] = output[(firstSubpatternId << 1) + i];
                output[(firstSubpatternId << 1) + i] = offsetNoMatch;
            }

            unsigned nameGroupIdx = 0;
            for (unsigned duplicateNamedGroupId : m_duplicateNamedGroups) {
                subpatternAndGroupIdBackup[backupOffsetForDuplicateNamedGroup(nameGroupIdx)] = output[m_pattern->offsetForDuplicateNamedGroupId(duplicateNamedGroupId)];
                output[pattern->offsetForDuplicateNamedGroupId(duplicateNamedGroupId)] = 0;
                ++nameGroupIdx;
            }

            new (getDisjunctionContext()) DisjunctionContext();
        }

        void* operator new(size_t, void* where)
        {
            return where;
        }

        void restoreOutput(unsigned* output, unsigned firstSubpatternId)
        {
            for (unsigned i = 0; i < (m_numNestedSubpatterns << 1); ++i)
                output[(firstSubpatternId << 1) + i] = subpatternAndGroupIdBackup[i];

            unsigned nameGroupIdx = 0;
            for (unsigned duplicateNamedGroupId : m_duplicateNamedGroups) {
                output[m_pattern->offsetForDuplicateNamedGroupId(duplicateNamedGroupId)] = subpatternAndGroupIdBackup[backupOffsetForDuplicateNamedGroup(nameGroupIdx)];
                ++nameGroupIdx;
            }
        }

        DisjunctionContext* getDisjunctionContext()
        {
            return bitwise_cast<DisjunctionContext*>(bitwise_cast<uintptr_t>(this) + allocationSize(m_numBackupIds));
        }

        unsigned backupOffsetForDuplicateNamedGroup(unsigned duplicateNamedGroup)
        {
            unsigned offset = (m_numNestedSubpatterns << 1) + duplicateNamedGroup;
            ASSERT(offset < m_numBackupIds);
            return offset;
        }

        static size_t allocationSize(unsigned numberOfSubpatterns, unsigned numDuplicateNamedGroups)
        {
            Checked<size_t> numBackupIds = (Checked<size_t>(numberOfSubpatterns) * 2U) + Checked<size_t>(numDuplicateNamedGroups);
            return allocationSize(numBackupIds.value());
        }

        static size_t allocationSize(unsigned numBackupIds)
        {
            size_t rawSize = sizeof(ParenthesesDisjunctionContext) + Checked<size_t>(numBackupIds) * sizeof(unsigned);
            size_t roundedSize = roundUpToMultipleOf<sizeof(void*)>(rawSize);
            RELEASE_ASSERT(roundedSize >= rawSize);
            return roundedSize;
        }

        ParenthesesDisjunctionContext* next { nullptr };
        BytecodePattern* m_pattern;
        unsigned m_numNestedSubpatterns;
        size_t m_numBackupIds;
        BitVector m_duplicateNamedGroups;
#if ASSERT_ENABLED
        constexpr static uint64_t magicNumber = 0x02aabbccddeeff02;
        uint64_t m_magicNumber { magicNumber };
#endif
        unsigned subpatternAndGroupIdBackup[1];
    };

    ParenthesesDisjunctionContext* allocParenthesesDisjunctionContext(ByteDisjunction* disjunction, unsigned* output, ByteTerm& term)
    {
        BitVector duplicateNamedCaptureGroups;
        unsigned firstSubpatternId = term.subpatternId();
        unsigned numNestedSubpatterns = term.atom.parenthesesDisjunction->m_numSubpatterns;
        unsigned numDuplicateNamedGroups = 0;

        if (pattern->hasDuplicateNamedCaptureGroups()) {
            for (unsigned i = 0; i < numNestedSubpatterns; ++i) {
                unsigned subpatternId = firstSubpatternId + i;
                unsigned duplicateNamedGroup = pattern->m_duplicateNamedGroupForSubpatternId[subpatternId];
                if (duplicateNamedGroup)
                    duplicateNamedCaptureGroups.set(duplicateNamedGroup);
            }

            numDuplicateNamedGroups = duplicateNamedCaptureGroups.bitCount();
        }

        size_t size = Checked<size_t>(ParenthesesDisjunctionContext::allocationSize(numNestedSubpatterns, numDuplicateNamedGroups)) + DisjunctionContext::allocationSize(disjunction->m_frameSize);
        auto* newAllocatorPool = allocatorPool->ensureCapacity(size);
        if (UNLIKELY(!newAllocatorPool))
            return nullptr;
        allocatorPool = newAllocatorPool;
        return new (allocatorPool->alloc(size)) ParenthesesDisjunctionContext(pattern, output, term, numDuplicateNamedGroups, duplicateNamedCaptureGroups);
    }

    void freeParenthesesDisjunctionContext(ParenthesesDisjunctionContext* context)
    {
#if ASSERT_ENABLED
        ASSERT(context->m_magicNumber == ParenthesesDisjunctionContext::magicNumber);
        context->m_magicNumber = 0;
#endif
        context->~ParenthesesDisjunctionContext();
        allocatorPool = allocatorPool->dealloc(context);
    }

    class InputStream {
    public:
        InputStream(const CharType* input, size_t length, unsigned start, bool decodeSurrogatePairs)
            : input(input)
            , current(input + start)
            , inputEnd(input + length)
            , decodeSurrogatePairs(decodeSurrogatePairs)
        {
        }

        void next()
        {
            ++current;
        }

        void advance(unsigned count)
        {
            ASSERT(count <= static_cast<size_t>(inputEnd - current));
            current += count;
        }

        // Adapted from V8 SkipUntilCharOrChar and
        // ChoiceNode::MaybeEmitFixedLengthConsumeScan. This bounded Yarr
        // implementation tests whole words, then locates the exact first
        // exit character without alignment assumptions or reads past the span.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-interpreter.cc
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        static unsigned countUntilEitherCharacter(const CharType* begin, unsigned length, unsigned first, unsigned second)
        {
            ASSERT(first <= 0xff && second <= 0xff);
            const CharType* cursor = begin;
            const CharType* end = begin + length;
            constexpr uintptr_t unitMask = sizeof(CharType) == 1 ? 0xff : 0xffff;
            constexpr uintptr_t ones = static_cast<uintptr_t>(-1) / unitMask;
            constexpr uintptr_t highBits = ones * (unitMask / 2 + 1);
            constexpr unsigned wordLength = sizeof(uintptr_t) / sizeof(CharType);
            uintptr_t firstWord = ones * first;
            uintptr_t secondWord = ones * second;
            while (static_cast<size_t>(end - cursor) >= wordLength) {
                uintptr_t word;
                memcpy(&word, cursor, sizeof(word));
                uintptr_t firstDifference = word ^ firstWord;
                uintptr_t secondDifference = word ^ secondWord;
                if ((((firstDifference - ones) & ~firstDifference)
                    | ((secondDifference - ones) & ~secondDifference)) & highBits)
                    break;
                cursor += wordLength;
            }
            while (cursor < end && *cursor != first && *cursor != second)
                ++cursor;
            return static_cast<unsigned>(cursor - begin);
        }

        // Adapted from V8 ChoiceNode::MaybeEmitFixedLengthConsumeScan and
        // SkipUntilBitInTable / SkipUntilChar: consume a class run without
        // per-character interpreter dispatch, retaining normal backtracking.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-interpreter.cc
        unsigned consumeLatin1CharacterClass(const CharacterClass& characterClass,
            unsigned maximum, unsigned negativeOffset, bool invert)
        {
            unsigned available = std::min(maximum, static_cast<unsigned>(inputEnd - current));
            if (!available)
                return 0;
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativeOffset);
            const CharType* begin = current - negativeOffset;
            // A negated class with no wide members has the same complete
            // exit set on a legacy UTF-16 subject. Unicode subjects still
            // use the normal code-point path at the call site.
            if (sizeof(CharType) == 1 || (invert && !characterClass.m_hasNonLatin1Matches)) {
                const uint16_t* stops = invert ? characterClass.m_latin1SmallMatches : characterClass.m_latin1SmallNonMatches;
                if (stops[0] <= 0xff) {
                    unsigned consumed;
                    if (stops[1] <= 0xff || sizeof(CharType) == 2)
                        consumed = countUntilEitherCharacter(begin, available, stops[0], stops[1] <= 0xff ? stops[1] : stops[0]);
                    else {
                        ::Escargot::Optional<const CharType*> found = static_cast<const CharType*>(memchr(begin, stops[0], available));
                        consumed = found ? static_cast<unsigned>(found.value() - begin) : available;
                    }
                    current += consumed;
                    return consumed;
                }
            }
            const CharType* cursor = begin;
            const CharType* end = begin + available;
            while (cursor < end) {
                char32_t ch = *cursor;
                // Leave wide characters to the full Unicode range lookup.
                if (ch > 0xff)
                    break;
                bool matches = characterClass.m_latin1Bitmap[ch >> 5] & (1u << (ch & 31));
                if (matches == invert)
                    break;
                ++cursor;
            }
            unsigned consumed = static_cast<unsigned>(cursor - begin);
            current += consumed;
            return consumed;
        }

        char32_t peek(unsigned offset)
        {
            ASSERT(offset < static_cast<size_t>(inputEnd - current));
            return current[offset];
        }

        // Adapted from V8 SkipUntilChar and EmitSkipUntilSmallCharSet.
        // Use bounded memchr for one-byte subjects and a bounded word scan
        // for UTF-16, keeping the original start when the check has an offset.
        // The caller must rule out wider members of the necessary set.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-interpreter.cc
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        bool skipUntilLatin1Character(unsigned character, unsigned offset = 0, unsigned minimumLength = 1)
        {
            ASSERT(character <= 0xff && offset < minimumLength);
            if (!isAvailableInput(minimumLength))
                return false;
            if (sizeof(CharType) == 1) {
                ::Escargot::Optional<const CharType*> found = static_cast<const CharType*>(memchr(current + offset, character, inputEnd - current - minimumLength + 1));
                current = found ? found.value() - offset : inputEnd;
                return !!found;
            }
            unsigned available = static_cast<unsigned>(inputEnd - current) - minimumLength + 1;
            unsigned skipped = countUntilEitherCharacter(current + offset, available, character, character);
            if (skipped == available) {
                current = inputEnd;
                return false;
            }
            current += skipped;
            return true;
        }

        void rewind(unsigned amount)
        {
            ASSERT(static_cast<size_t>(current - input) >= amount);
            current -= amount;
        }

        char32_t read()
        {
            ASSERT(current < inputEnd);
            if (current < inputEnd)
                return *current;
            return errorCodePoint;
        }

        char32_t readChecked(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativePositionOffest);
            const CharType* p = current - negativePositionOffest;
            ASSERT(p < inputEnd);
            auto result = *p;
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && p + 1 < inputEnd && U16_IS_TRAIL(p[1])) {
                if (atEnd())
                    return errorCodePoint;
                next();
                return U16_GET_SUPPLEMENTARY(result, p[1]);
            } else if (decodeSurrogatePairs && p > input && U16_IS_TRAIL(result) && U16_IS_LEAD(p[-1]))
                return errorCodePoint;
            return result;
        }

        char32_t readCheckedDontAdvance(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativePositionOffest);
            const CharType* p = current - negativePositionOffest;
            ASSERT(p < inputEnd);
            auto result = *p;
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && p + 1 < inputEnd && U16_IS_TRAIL(p[1])) {
                if (atEnd())
                    return errorCodePoint;
                return U16_GET_SUPPLEMENTARY(result, p[1]);
            }
            return result;
        }

        // readForCharacterDump() is only for use by the DUMP_CURR_CHAR macro.
        // We don't want any side effects like the next() in readChecked() above.
        char32_t readForCharacterDump(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativePositionOffest);
            const CharType* p = current - negativePositionOffest;
            ASSERT(p < inputEnd);
            auto result = *p;
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && p + 1 < inputEnd && U16_IS_TRAIL(p[1])) {
                if (atEnd())
                    return errorCodePoint;
                return U16_GET_SUPPLEMENTARY(result, p[1]);
            }
            return result;
        }

        char32_t tryReadBackward(unsigned negativePositionOffest)
        {
            if (static_cast<size_t>(current - input) < negativePositionOffest)
                return errorCodePoint;
            const CharType* p = current - negativePositionOffest;
            ASSERT(p < inputEnd);
            auto result = *p;
            if (U16_IS_TRAIL(result) && decodeSurrogatePairs && p > input && U16_IS_LEAD(p[-1])) {
                rewind(1);
                return U16_GET_SUPPLEMENTARY(p[-1], result);
            }
            return result;
        }

        char32_t readSurrogatePairChecked(unsigned negativePositionOffset)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativePositionOffset);
            const CharType* p = current - negativePositionOffset;
            ASSERT(p < inputEnd);
            if (p + 1 >= inputEnd)
                return errorCodePoint;
            auto first = *p;
            auto second = p[1];
            if (U16_IS_LEAD(first) && U16_IS_TRAIL(second))
                return U16_GET_SUPPLEMENTARY(first, second);
            return errorCodePoint;
        }

        char32_t reread(unsigned from)
        {
            const CharType* p = input + from;
            ASSERT(p < inputEnd);
            auto result = *p;
            if (decodeSurrogatePairs && p + 1 < inputEnd) {
                if (U16_IS_LEAD(result) && U16_IS_TRAIL(p[1]))
                    return U16_GET_SUPPLEMENTARY(result, p[1]);
                if (U16_IS_TRAIL(result) && U16_IS_LEAD(p[1]))
                    return errorCodePoint;
            }
            return result;
        }

        char32_t prev()
        {
            ASSERT(!(current > inputEnd));
            if (current != input && inputEnd != input)
                return current[-1];
            return errorCodePoint;
        }

        unsigned getPos()
        {
            return static_cast<unsigned>(current - input);
        }

        void setPos(unsigned p)
        {
            current = input + p;
        }

        bool atStart()
        {
            return current == input;
        }

        bool atEnd()
        {
            return current == inputEnd;
        }

        unsigned end()
        {
            return static_cast<unsigned>(inputEnd - input);
        }

        bool checkInput(unsigned count)
        {
            if (LIKELY(count <= static_cast<size_t>(inputEnd - current))) {
                current += count;
                return true;
            }
            return false;
        }

        void uncheckInput(unsigned count)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= count);
            current -= count;
        }

        bool tryUncheckInput(unsigned count)
        {
            if (count > static_cast<size_t>(current - input))
                return false;
            current -= count;
            return true;
        }

        bool atStart(unsigned negativePositionOffset)
        {
            return static_cast<unsigned>(current - input) == negativePositionOffset;
        }

        bool atEnd(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(static_cast<size_t>(current - input) >= negativePositionOffest);
            return (current - negativePositionOffest) == inputEnd;
        }

        bool isAvailableInput(unsigned offset)
        {
            return offset <= static_cast<size_t>(inputEnd - current);
        }

        bool isValidNegativeInputOffset(unsigned offset)
        {
            return (static_cast<size_t>(current - input) >= offset) && ((current - offset) < inputEnd);
        }

        // Inspired by V8 BoyerMooreLookahead search prefilters. This
        // Yarr-specific absence check also handles variable-width prefixes;
        // it does not change the candidate position or alternative order.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        bool containsRequiredAtom(const Vector<LChar>& atom)
        {
            unsigned length = atom.size();
            if (length > static_cast<size_t>(inputEnd - current))
                return false;
            const CharType* cursor = current;
            const CharType* last = inputEnd - length;
            while (cursor <= last) {
                if (sizeof(CharType) == 1) {
                    ::Escargot::Optional<const CharType*> found = static_cast<const CharType*>(memchr(cursor, atom[0], last - cursor + 1));
                    if (!found)
                        return false;
                    cursor = found.value();
                    if (!memcmp(cursor, &atom[0], length))
                        return true;
                } else if (*cursor == atom[0]) {
                    unsigned i = 1;
                    while (i < length && cursor[i] == atom[i])
                        ++i;
                    if (i == length)
                        return true;
                }
                ++cursor;
            }
            return false;
        }

        // Adapted from V8 QuickCheckDetails::Rationalize and
        // RegExpNode::EmitQuickCheck; exact prefix tests follow the mask.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        bool matchesPackedPrefix(uint32_t mask, uint32_t value)
        {
            ASSERT(sizeof(CharType) == 1 && isAvailableInput(sizeof(uint32_t)));
            uint32_t actual;
            memcpy(&actual, current, sizeof(actual));
            return (actual & mask) == value;
        }

        // Adapted from V8 CheckNotBackRef / CheckNotBackRefBackward and
        // BackRefMatchesNoCase. Keep Yarr bounds and Unicode fallbacks.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-interpreter.cc
        bool matchesBackReference(unsigned begin, unsigned length, unsigned negativeOffset, bool ignoreCase)
        {
            ASSERT(begin <= static_cast<size_t>(inputEnd - input));
            ASSERT(length <= static_cast<size_t>(inputEnd - input) - begin);
            if (negativeOffset > static_cast<size_t>(current - input))
                return false;
            const CharType* chars = current - negativeOffset;
            ASSERT(length <= static_cast<size_t>(inputEnd - chars));
            if (sizeof(CharType) == 1 && ignoreCase) {
                for (unsigned i = 0; i < length; ++i) {
                    unsigned expected = input[begin + i];
                    unsigned actual = chars[i];
                    if (expected == actual)
                        continue;
                    unsigned folded = expected | 0x20;
                    if (folded != (actual | 0x20))
                        return false;
                    // Latin1 case pairs differ at bit 5 only for letters.
                    // Exclude the multiplication/division sign pair and
                    // folds whose other member is outside this subject.
                    if (!(folded - 'a' <= 'z' - 'a'
                        || (folded - 0xe0 <= 0xfe - 0xe0 && folded != 0xf7)))
                        return false;
                }
                return true;
            }
            return !memcmp(input + begin, chars, static_cast<size_t>(length) * sizeof(CharType));
        }

        bool matchesLiteral(const ByteTerm& term)
        {
            ASSERT(static_cast<size_t>(current - input) >= term.inputPosition);
            const CharType* chars = current - term.inputPosition;
            unsigned length = term.literal.length;
            ASSERT(length <= static_cast<size_t>(inputEnd - chars));
            unsigned i = 0;
            if (sizeof(CharType) == 1) {
                // memcpy permits unaligned subjects on ARM32 and never reads
                // beyond the checked literal, unlike a rounded-up word load.
                for (; i + sizeof(uint32_t) <= length; i += sizeof(uint32_t)) {
                    uint32_t actual, expected, mask;
                    memcpy(&actual, chars + i, sizeof(actual));
                    memcpy(&expected, term.literal.characters + i, sizeof(expected));
                    memcpy(&mask, term.literal.masks + i, sizeof(mask));
                    if ((actual | mask) != expected)
                        return false;
                }
            }
            for (; i < length; ++i) {
                if ((chars[i] | term.literal.masks[i]) != term.literal.characters[i])
                    return false;
            }
            return true;
        }

        bool matchesLiteral16(const ByteTerm& term)
        {
            ASSERT(static_cast<size_t>(current - input) >= term.inputPosition);
            const CharType* chars = current - term.inputPosition;
            unsigned length = term.literal16.length;
            ASSERT(length <= static_cast<size_t>(inputEnd - chars));
            unsigned i = 0;
            if (sizeof(CharType) == 2) {
                for (; i + 2 <= length; i += 2) {
                    uint32_t actual, expected, mask;
                    memcpy(&actual, chars + i, sizeof(actual));
                    memcpy(&expected, term.literal16.characters + i, sizeof(expected));
                    memcpy(&mask, term.literal16.masks + i, sizeof(mask));
                    if ((actual | mask) != expected)
                        return false;
                }
            }
            for (; i < length; ++i) {
                if ((chars[i] | term.literal16.masks[i]) != term.literal16.characters[i])
                    return false;
            }
            return true;
        }

        void dump(PrintStream& out) const
        {
        }

    private:
        const CharType* input;
        const CharType* current;
        const CharType* inputEnd;
        bool decodeSurrogatePairs;
    };

    bool testCharacterClass(const CharacterClass* characterClass, char32_t ch)
    {
        auto linearSearchMatches = [ch](const Vector<char32_t>& matches) {
            for (unsigned i = 0; i < matches.size(); ++i) {
                if (ch == matches[i])
                    return true;
            }

            return false;
        };

        auto binarySearchMatches = [ch](const Vector<char32_t>& matches) {
            size_t low = 0;
            size_t high = matches.size() - 1;

            while (low <= high) {
                size_t mid = low + (high - low) / 2;
                int diff = ch - matches[mid];
                if (!diff)
                    return true;

                if (diff < 0) {
                    if (mid == low)
                        return false;
                    high = mid - 1;
                } else
                    low = mid + 1;
            }
            return false;
        };

        auto linearSearchRanges = [ch](const Vector<CharacterRange>& ranges) {
            for (unsigned i = 0; i < ranges.size(); ++i) {
                if ((ch >= ranges[i].begin) && (ch <= ranges[i].end))
                    return true;
            }

            return false;
        };

        auto binarySearchRanges = [ch](const Vector<CharacterRange>& ranges) {
            size_t low = 0;
            size_t high = ranges.size() - 1;

            while (low <= high) {
                size_t mid = low + (high - low) / 2;
                int rangeBeginDiff = ch - ranges[mid].begin;
                if (rangeBeginDiff >= 0 && ch <= ranges[mid].end)
                    return true;

                if (rangeBeginDiff < 0) {
                    if (mid == low)
                        return false;
                    high = mid - 1;
                } else
                    low = mid + 1;
            }
            return false;
        };

        if (characterClass->m_anyCharacter)
            return true;

        if (ch <= 0xff && characterClass->m_hasLatin1Bitmap)
            return characterClass->m_latin1Bitmap[ch >> 5] & (1u << (ch & 31));

        const size_t thresholdForBinarySearch = 6;

        if (!isASCII(ch)) {
            if (characterClass->m_matchesUnicode.size()) {
                if (characterClass->m_matchesUnicode.size() > thresholdForBinarySearch) {
                    if (binarySearchMatches(characterClass->m_matchesUnicode))
                        return true;
                } else if (linearSearchMatches(characterClass->m_matchesUnicode))
                    return true;
            }

            if (characterClass->m_rangesUnicode.size()) {
                if (characterClass->m_rangesUnicode.size() > thresholdForBinarySearch) {
                    if (binarySearchRanges(characterClass->m_rangesUnicode))
                        return true;
                } else if (linearSearchRanges(characterClass->m_rangesUnicode))
                    return true;
            }
        } else {
            if (characterClass->m_matches.size()) {
                if (characterClass->m_matches.size() > thresholdForBinarySearch) {
                    if (binarySearchMatches(characterClass->m_matches))
                        return true;
                } else if (linearSearchMatches(characterClass->m_matches))
                    return true;
            }

            if (characterClass->m_ranges.size()) {
                if (characterClass->m_ranges.size() > thresholdForBinarySearch) {
                    if (binarySearchRanges(characterClass->m_ranges))
                        return true;
                } else if (linearSearchRanges(characterClass->m_ranges))
                    return true;
            }
        }

        return false;
    }

    ALWAYS_INLINE bool mayStartMatchAt(char32_t ch)
    {
        const StartCharFilter& filter = pattern->m_startCharFilter;
        if (ch > 0xFF)
            return filter.mayStartAboveLatin1;
        return filter.latin1Bitmap[ch >> 5] & (1u << (ch & 31));
    }

    // Advances over the start offsets at which no alternative of the body
    // disjunction can consume its first character. Returns false if that
    // exhausts the input, which means there is no match at all: a valid filter
    // implies the body cannot match the empty string.
    ALWAYS_INLINE bool advanceToPossibleStart()
    {
        if (pattern->m_fixedPrefixSearch) {
            const auto& search = *pattern->m_fixedPrefixSearch.value();
            if (search.length) {
                while (input.isAvailableInput(search.length)) {
                    if (search.singleLatin1Character <= 0xff
                        && (sizeof(CharType) == 1 || !search.positions[search.singleCharacterOffset].mayStartAboveLatin1)
                        && !input.skipUntilLatin1Character(search.singleLatin1Character, search.singleCharacterOffset, search.length))
                        return false;
                    // A packed necessary condition rejects four Latin1
                    // positions with one bounded, unaligned-safe load. Keep
                    // exact membership checks for the admitted candidates.
                    if (sizeof(CharType) != 1 || !search.packedMask
                        || input.matchesPackedPrefix(search.packedMask, search.packedValue)) {
                        unsigned i = search.length;
                        while (i) {
                            --i;
                            char32_t ch = input.peek(i);
                            const auto& filter = search.positions[i];
                            if (ch > 0xff ? !filter.mayStartAboveLatin1 : !(filter.latin1Bitmap[ch >> 5] & (1u << (ch & 31))))
                                break;
                            if (!i)
                                return true;
                        }
                    }
                    char32_t last = input.peek(search.length - 1);
                    // A later start can only match if this sampled character
                    // occurs at its corresponding prefix offset. Use the
                    // nearest such offset across every alternative, so the
                    // Horspool shift never skips a possible match.
                    input.advance(last <= 0xff ? search.shifts[last] : 1);
                }
                return false;
            }
        }
        if (!pattern->m_startCharFilter.valid)
            return true;

        if (pattern->m_startCharFilter.fixedPosition)
            return !input.atEnd() && mayStartMatchAt(input.read());

        if (pattern->m_startCharFilter.singleLatin1Character <= 0xff
            && (sizeof(CharType) == 1 || !pattern->m_startCharFilter.mayStartAboveLatin1))
            return input.skipUntilLatin1Character(pattern->m_startCharFilter.singleLatin1Character);

        while (!input.atEnd()) {
            if (mayStartMatchAt(input.read()))
                return true;
            input.next();
        }
        return false;
    }

    bool checkCharacter(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterType());
        if (term.matchDirection() == Forward)
            return term.atom.patternCharacter == static_cast<char32_t>(input.readChecked(negativeInputOffset));

        return term.atom.patternCharacter == static_cast<char32_t>(input.tryReadBackward(negativeInputOffset));
    }

    bool checkSurrogatePair(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterType());
        return term.atom.patternCharacter == static_cast<char32_t>(input.readSurrogatePairChecked(negativeInputOffset));
    }

    bool checkCasedCharacter(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCasedCharacterType());
        char32_t ch = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);
        return (term.atom.casedCharacter.lo == ch) || (term.atom.casedCharacter.hi == ch);
    }

    bool checkCharacterClass(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterClass());

        auto inputChar = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);
        if (inputChar == errorCodePoint)
            return false;

        if (term.type == ByteTerm::Type::CharacterClassWithNegativeAssertion) {
            ASSERT(!isEitherUnicodeCompilation() && term.matchDirection() == Forward);
            return testCharacterClass(term.atom.characterClass, inputChar) != term.invert()
                && !testCharacterClass(term.atom.secondaryCharacterClass, inputChar);
        }

        bool match;
        // Escargot update for `built-ins/RegExp/regexp-modifiers/add-ignoreCase-affects-slash-upper-p.js`
        if (term.m_flags.contains(Flags::IgnoreCase) && term.m_flags.contains(Flags::Unicode)) {
            char32_t ch = inputChar;
            // ASCII range: use the C library's ctype functions instead of ICU's,
            // since ICU's Unicode case-folding is unneeded overhead for plain ASCII
            // and ctype's argument is only well-defined for values <= 128 anyway.
            if (ch <= 128) {
                if (islower(ch)) {
                    if (term.invert()) {
                        match = testCharacterClass(term.atom.characterClass, ch) && testCharacterClass(term.atom.characterClass, toupper(ch));
                    } else {
                        match = testCharacterClass(term.atom.characterClass, ch) || testCharacterClass(term.atom.characterClass, toupper(ch));
                    }
                } else if (isupper(ch)) {
                    if (term.invert()) {
                        match = testCharacterClass(term.atom.characterClass, ch) && testCharacterClass(term.atom.characterClass, tolower(ch));
                    } else {
                        match = testCharacterClass(term.atom.characterClass, ch) || testCharacterClass(term.atom.characterClass, tolower(ch));
                    }
                } else {
                    match = testCharacterClass(term.atom.characterClass, ch);
                }
            }
#if defined(ENABLE_ICU)
            else if (u_islower(ch)) {
                if (term.invert()) {
                    match = testCharacterClass(term.atom.characterClass, ch) && testCharacterClass(term.atom.characterClass, u_toupper(ch));
                } else {
                    match = testCharacterClass(term.atom.characterClass, ch) || testCharacterClass(term.atom.characterClass, u_toupper(ch));
                }
            } else if (u_isupper(ch)) {
                if (term.invert()) {
                    match = testCharacterClass(term.atom.characterClass, ch) && testCharacterClass(term.atom.characterClass, u_tolower(ch));
                } else {
                    match = testCharacterClass(term.atom.characterClass, ch) || testCharacterClass(term.atom.characterClass, u_tolower(ch));
                }
            }
#endif
            else {
                match = testCharacterClass(term.atom.characterClass, ch);
            }
        } else {
            match = testCharacterClass(term.atom.characterClass, static_cast<char32_t>(inputChar));
        }
        return term.invert() ? !match : match;
    }
    
    bool checkCharacterClassDontAdvanceInputForNonBMP(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterClass());
        const CharacterClass* characterClass = term.atom.characterClass;

        if (term.matchDirection() == Backward && negativeInputOffset > input.getPos())
            return false;

        auto readCharacter = characterClass->hasOnlyNonBMPCharacters() ? input.readSurrogatePairChecked(negativeInputOffset) :  input.readChecked(negativeInputOffset);

        if (readCharacter == errorCodePoint)
            return false;

        if (term.m_flags.contains(Flags::IgnoreCase)) {
            // Escargot update for `built-ins/RegExp/regexp-modifiers/add-ignoreCase-affects-slash-lower-p.js`
#if defined(ENABLE_ICU)
            char32_t ch = readCharacter;
            if (u_islower(ch)) {
                return testCharacterClass(characterClass, ch) ||
                    testCharacterClass(characterClass, u_toupper(ch));
            } else if (u_isupper(ch)) {
                return testCharacterClass(characterClass, ch) ||
                    testCharacterClass(characterClass, u_tolower(ch));
            } else {
                return testCharacterClass(characterClass, ch);
            }
#else
            return testCharacterClass(characterClass, static_cast<char32_t>(readCharacter));
#endif
        } else {
            return testCharacterClass(characterClass, static_cast<char32_t>(readCharacter));
        }
    }

    bool tryConsumeBackReference(int matchBegin, int matchEnd, ByteTerm& term)
    {
        unsigned matchSize = (unsigned)(matchEnd - matchBegin);

        if (term.matchDirection() == Forward) {
            if (!input.checkInput(matchSize))
                return false;
        }

        if (sizeof(CharType) == 1 || (!term.ignoreCase() && isLegacyCompilation())) {
            if (!input.matchesBackReference(matchBegin, matchSize, term.inputPosition + matchSize, term.ignoreCase())) {
                if (term.matchDirection() == Forward)
                    input.uncheckInput(matchSize);
                return false;
            }
            if (term.matchDirection() == Backward)
                input.uncheckInput(matchSize);
            return true;
        }

        for (unsigned i = 0; i < matchSize; ++i) {
            unsigned negativeInputOffset = term.inputPosition + matchSize - i;
            if (term.matchDirection() == Backward && negativeInputOffset > input.getPos())
                return false;

            char32_t oldCh = input.reread(matchBegin + i);
            char32_t ch;
            if (!U_IS_BMP(oldCh)) {
                ch = input.readSurrogatePairChecked(negativeInputOffset);
                ++i;
            } else
                ch = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);

            if (oldCh == errorCodePoint || ch == errorCodePoint)
                return false;

            if (oldCh == ch)
                continue;

            if (term.ignoreCase()) {
                // See ES 6.0, 21.2.2.8.2 for the definition of Canonicalize(). For non-Unicode
                // patterns, Unicode values are never allowed to match against ASCII ones.
                // For Unicode, we need to check all canonical equivalents of a character.
                if (isLegacyCompilation() && (isASCII(oldCh) || isASCII(ch))) {
                    if (toASCIIUpper(oldCh) == toASCIIUpper(ch))
                        continue;
                } else if (areCanonicallyEquivalent(oldCh, ch, isEitherUnicodeCompilation() ? CanonicalMode::Unicode : CanonicalMode::UCS2))
                    continue;
            }

            if (term.matchDirection() == Forward)
                input.uncheckInput(matchSize);

            return false;
        }

        if (term.matchDirection() == Backward)
            input.uncheckInput(matchSize);

        return true;
    }

    bool matchAssertionBOL(ByteTerm& term)
    {
        return (input.atStart(term.inputPosition)) || (term.multiline() && testCharacterClass(pattern->newlineCharacterClass, input.readCheckedDontAdvance(term.inputPosition + 1)));
    }

    bool matchAssertionEOL(ByteTerm& term)
    {
        if (term.inputPosition)
            return (input.atEnd(term.inputPosition)) || (term.multiline() && testCharacterClass(pattern->newlineCharacterClass, input.readCheckedDontAdvance(term.inputPosition)));

        return (input.atEnd()) || (term.multiline() && testCharacterClass(pattern->newlineCharacterClass, input.read()));
    }

    bool matchAssertionWordBoundary(ByteTerm& term)
    {
        unsigned inputOffset = term.inputPosition;

        auto boundaryCharacterClass = term.ignoreCase() ? pattern->ignoreCaseWordcharCharacterClass : pattern->wordcharCharacterClass;

        bool prevIsWordchar = !input.atStart(inputOffset) && testCharacterClass(boundaryCharacterClass, input.readChecked(inputOffset + 1));
        bool readIsWordchar;
        if (inputOffset)
            readIsWordchar = !input.atEnd(inputOffset) && testCharacterClass(boundaryCharacterClass, input.readChecked(inputOffset));
        else
            readIsWordchar = !input.atEnd() && testCharacterClass(boundaryCharacterClass, input.read());

        bool wordBoundary = prevIsWordchar != readIsWordchar;
        return term.invert() ? !wordBoundary : wordBoundary;
    }

    bool backtrackPatternCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            break;

        case QuantifierType::Greedy:
            if (term.m_possessive) {
                ASSERT(!isEitherUnicodeCompilation() && term.matchDirection() == Forward);
                input.uncheckInput(backTrack->matchAmount * U16_LENGTH(term.atom.patternCharacter));
                backTrack->matchAmount = 0;
                return false;
            }
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(U16_LENGTH(term.atom.patternCharacter));
                else {
                    if (!input.checkInput(U16_LENGTH(term.atom.patternCharacter)))
                        break;
                }
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCharacter(term, term.inputPosition + 1))
                        return true;
                }
                input.setPos(backTrack->begin);
                break;
            }
            // matchDirection Backward
            unsigned position = input.getPos();

            if (position < term.inputPosition)
                break;

            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacter(term, term.inputPosition))
                    return true;
            }
            input.setPos(backTrack->begin);
            break;

        }

        return false;
    }

    bool backtrackPatternCasedCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            break;

        case QuantifierType::Greedy:
            if (term.m_possessive) {
                ASSERT(!isEitherUnicodeCompilation() && term.matchDirection() == Forward);
                input.uncheckInput(backTrack->matchAmount);
                backTrack->matchAmount = 0;
                return false;
            }
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(1);
                else {
                    if (!input.checkInput(1))
                        break;
                }
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCasedCharacter(term, term.inputPosition + 1))
                        return true;
                }
                input.uncheckInput(backTrack->matchAmount);
                break;
            }
            // matchDirection Backward
            unsigned position = input.getPos();

            if (position < term.inputPosition)
                break;

            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCasedCharacter(term, term.inputPosition))
                    return true;
            }
            input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    bool matchCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::CharacterClass
            || term.type == ByteTerm::Type::CharacterClassWithNegativeAssertion);
        BackTrackInfoCharacterClass* backTrack = reinterpret_cast<BackTrackInfoCharacterClass*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            if (term.matchDirection() == Forward) {
                if (isEitherUnicodeCompilation()) {
                    backTrack->begin = input.getPos();
                    unsigned matchAmount = 0;
                    for (matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                        if (term.invert()) {
                            if (!checkCharacterClass(term, term.inputPosition - matchAmount)) {
                                input.setPos(backTrack->begin);
                                return false;
                            }
                        } else {
                            unsigned matchOffset = matchAmount * (term.atom.characterClass->hasOnlyNonBMPCharacters() ? 2 : 1);
                            if (!checkCharacterClassDontAdvanceInputForNonBMP(term, term.inputPosition - matchOffset)) {
                                input.setPos(backTrack->begin);
                                return false;
                            }
                        }
                    }

                    return true;
                }

                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacterClass(term, term.inputPosition - matchAmount))
                        return false;
                }
                return true;
            }

            // matchDirection is Backward
            if (isEitherUnicodeCompilation()) {
                backTrack->begin = input.getPos();
                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    unsigned matchOffset = term.atom.quantityMaxCount - 1 - matchAmount;
                    if (term.invert()) {
                        if (!checkCharacterClass(term, term.inputPosition - matchOffset)) {
                            input.setPos(backTrack->begin);
                            return false;
                        }
                    } else {
                        matchOffset = matchOffset * (term.atom.characterClass->hasOnlyNonBMPCharacters() ? 2 : 1);
                        if (!checkCharacterClassDontAdvanceInputForNonBMP(term, term.inputPosition - matchOffset)) {
                            input.setPos(backTrack->begin);
                            return false;
                        }
                    }
                }

                return true;
            }

            if (input.getPos() < term.inputPosition)
                return false;

            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                if (!checkCharacterClass(term, term.inputPosition - term.atom.quantityMaxCount + matchAmount + 1))
                    return false;
            }
            return true;
        }

        case QuantifierType::Greedy: {
            unsigned position = input.getPos();
            unsigned matchAmount = 0;
            if (term.matchDirection() == Forward) {
                if (!isEitherUnicodeCompilation() && term.type == ByteTerm::Type::CharacterClass
                    && term.atom.characterClass->m_hasLatin1Bitmap) {
                    matchAmount = input.consumeLatin1CharacterClass(*term.atom.characterClass,
                        term.atom.quantityMaxCount, term.inputPosition, term.invert());
                    position = input.getPos();
                }
                while ((matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCharacterClass(term, term.inputPosition + 1)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
                backTrack->matchAmount = matchAmount;
                return true;
            }

            // matchDirection = Backward
            if (input.getPos() < term.inputPosition)
                return false;

            while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                if (!checkCharacterClass(term, term.inputPosition)) {
                    input.setPos(position);
                    break;
                }
                ++matchAmount;
                position = input.getPos();
            }
            backTrack->matchAmount = matchAmount;
            return true;
        }

        case QuantifierType::NonGreedy:
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            return true;
        }

        ASSERT_UNREACHABLE();
        return false;
    }

    bool backtrackCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::CharacterClass
            || term.type == ByteTerm::Type::CharacterClassWithNegativeAssertion);
        BackTrackInfoCharacterClass* backTrack = reinterpret_cast<BackTrackInfoCharacterClass*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            if (isEitherUnicodeCompilation())
                input.setPos(backTrack->begin);
            break;

        case QuantifierType::Greedy:
            if (term.m_possessive) {
                ASSERT(!isEitherUnicodeCompilation() && term.matchDirection() == Forward);
                input.uncheckInput(backTrack->matchAmount);
                backTrack->matchAmount = 0;
                return false;
            }
            if (backTrack->matchAmount) {
                if (isEitherUnicodeCompilation()) {
                    // Unmatch one codepoint
                    if (term.matchDirection() == Forward) {
                        --backTrack->matchAmount;
                        input.uncheckInput(1);
                        input.tryReadBackward(term.inputPosition);
                        return true;
                    }
                    // matchDirection Backwards
                    --backTrack->matchAmount;
                    input.readChecked(term.inputPosition);
                    input.checkInput(1);
                    return true;
                }
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(1);
                else
                    input.checkInput(1);
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCharacterClass(term, term.inputPosition + 1))
                        return true;
                }
                input.setPos(backTrack->begin);
                break;
            }
            // matchDirection Backward
            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacterClass(term, term.inputPosition))
                    return true;
            }
            input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    bool matchBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::BackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        // Initialize backtracking info first before we check for possible null matches.
        switch (term.atom.quantityType) {
        case QuantifierType::NonGreedy:
            backTrack->matchAmount = 0;
            FALLTHROUGH;

        case QuantifierType::FixedCount:
            backTrack->begin = input.getPos();
            break;

        case QuantifierType::Greedy:
            backTrack->matchAmount = 0;
            break;
        }

        unsigned subpatternId;

        if (auto duplicateNamedGroupId = term.duplicateNamedGroupId()) {
            subpatternId = output[pattern->offsetForDuplicateNamedGroupId(duplicateNamedGroupId)];
            if (subpatternId < 1) {
                // If we don't have a subpattern that matched, then the string to match is empty.
                return true;
            }
        } else
            subpatternId = term.subpatternId();

        unsigned matchBegin = output[(subpatternId << 1)];
        unsigned matchEnd = output[(subpatternId << 1) + 1];

        // If the end position of the referenced match hasn't set yet then the backreference in the same parentheses where it references to that.
        // In this case the result of match is empty string like when it references to a parentheses with zero-width match.
        // Eg.: /(a\1)/
        if (matchEnd == offsetNoMatch)
            return true;

        if (matchBegin == offsetNoMatch)
            return true;

        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return true;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                if (!tryConsumeBackReference(matchBegin, matchEnd, term)) {
                    input.setPos(backTrack->begin);
                    return false;
                }
            }
            return true;
        }

        case QuantifierType::Greedy: {
            unsigned matchAmount = 0;
            while ((matchAmount < term.atom.quantityMaxCount) && tryConsumeBackReference(matchBegin, matchEnd, term))
                ++matchAmount;
            backTrack->matchAmount = matchAmount;
            return true;
        }

        case QuantifierType::NonGreedy:
            return true;
        }

        ASSERT_UNREACHABLE();
        return false;
    }

    bool backtrackBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::BackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        unsigned subpatternId;

        if (auto duplicateNamedGroupId = term.duplicateNamedGroupId()) {
            subpatternId = output[pattern->offsetForDuplicateNamedGroupId(duplicateNamedGroupId)];
            if (subpatternId < 1) {
                // If we don't have a subpattern that matched, then the string to match is empty.
                return false;
            }
        } else
            subpatternId = term.subpatternId();

        unsigned matchBegin = output[(subpatternId << 1)];
        unsigned matchEnd = output[(subpatternId << 1) + 1];

        if (matchBegin == offsetNoMatch)
            return false;

        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return false;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            // for quantityMaxCount == 1, could rewind.
            input.setPos(backTrack->begin);
            break;

        case QuantifierType::Greedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                if (term.matchDirection() == Backward)
                    return input.checkInput(matchEnd - matchBegin);
                input.rewind(matchEnd - matchBegin);
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && tryConsumeBackReference(matchBegin, matchEnd, term)) {
                ++backTrack->matchAmount;
                return true;
            }
            input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    void recordParenthesesMatch(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes are recorded end then start.
            output[(subpatternId << 1) + term.matchDirection()] = context->getDisjunctionContext()->matchBegin - term.inputPosition;
            output[(subpatternId << 1) + 1 - term.matchDirection()] = context->getDisjunctionContext()->matchEnd - term.inputPosition;

            if (term.duplicateNamedGroupId()) {
                // Record which of the duplicate named subpatterns matched.
                output[pattern->offsetForDuplicateNamedGroupId(term.duplicateNamedGroupId())] = subpatternId;
            }
        }
    }
    void resetMatches(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        unsigned firstSubpatternId = term.subpatternId();
        context->restoreOutput(output, firstSubpatternId);
    }

    JSRegExpResult parenthesesDoBacktrack(ByteTerm& term, BackTrackInfoParentheses* backTrack)
    {
        while (backTrack->matchAmount) {
            ParenthesesDisjunctionContext* context = backTrack->lastContext;

            JSRegExpResult result = matchDisjunction(term.atom.parenthesesDisjunction, context->getDisjunctionContext(), true);
            if (result == JSRegExpResult::Match)
                return JSRegExpResult::Match;

            resetMatches(term, context);
            popParenthesesDisjunctionContext(backTrack);
            freeParenthesesDisjunctionContext(context);

            if (result != JSRegExpResult::NoMatch)
                return result;
        }

        return JSRegExpResult::NoMatch;
    }

    bool matchParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy: {
            // set this speculatively; if we get to the parens end this will be true.
            backTrack->begin = input.getPos();
            break;
        }
        case QuantifierType::NonGreedy: {
            backTrack->begin = notFound;
            context->term += term.atom.parenthesesWidth;
            return true;
        }
        case QuantifierType::FixedCount:
            break;
        }

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes are recorded end then start.
            output[(subpatternId << 1) + term.matchDirection()] = input.getPos() - term.inputPosition;
        }

        return true;
    }

    bool matchParenthesesOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes are recorded end then start.
            output[(subpatternId << 1) + 1 - term.matchDirection()] = input.getPos() - term.inputPosition;

            if (term.duplicateNamedGroupId()) {
                // Record which of the duplicate named subpatterns matched.
                output[pattern->offsetForDuplicateNamedGroupId(term.duplicateNamedGroupId())] = subpatternId;
            }
        }

        if (term.atom.quantityType == QuantifierType::FixedCount)
            return true;

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);
        return backTrack->begin != input.getPos();
    }

    bool backtrackParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            output[(subpatternId << 1)] = offsetNoMatch;
            output[(subpatternId << 1) + 1] = offsetNoMatch;

            if (term.duplicateNamedGroupId()) {
                // Clear matching subpatternId.
                output[pattern->offsetForDuplicateNamedGroupId(term.duplicateNamedGroupId())] = 0;
            }
        }

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy:
            // if we backtrack to this point, there is another chance - try matching nothing.
            ASSERT(backTrack->begin != notFound);
            backTrack->begin = notFound;
            context->term += term.atom.parenthesesWidth;
            return true;
        case QuantifierType::NonGreedy:
            ASSERT(backTrack->begin != notFound);
            FALLTHROUGH;
        case QuantifierType::FixedCount:
            break;
        }

        return false;
    }

    bool backtrackParenthesesOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy:
            if (backTrack->begin == notFound) {
                context->term -= term.atom.parenthesesWidth;
                return false;
            }
            FALLTHROUGH;
        case QuantifierType::NonGreedy:
            if (backTrack->begin == notFound) {
                backTrack->begin = input.getPos();
                if (term.capture()) {
                    // Technically this access to inputPosition should be accessing the begin term's
                    // inputPosition, but for repeats other than fixed these values should be
                    // the same anyway! (We don't pre-check for greedy or non-greedy matches.)
                    ASSERT((&term - term.atom.parenthesesWidth)->type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
                    ASSERT((&term - term.atom.parenthesesWidth)->inputPosition == term.inputPosition);
                    unsigned subpatternId = term.subpatternId();
                    // For Backward matches, the captured indexes are recorded end then start.
                    output[(subpatternId << 1) + term.matchDirection()] = input.getPos() - term.inputPosition;
                }
                context->term -= term.atom.parenthesesWidth;
                return true;
            }
            FALLTHROUGH;
        case QuantifierType::FixedCount:
            break;
        }

        return false;
    }

    bool matchParenthesesTerminalBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);
        ASSERT(term.atom.quantityType == QuantifierType::Greedy);
        ASSERT(term.atom.quantityMinCount <= 1);
        ASSERT(term.atom.quantityMaxCount == quantifyInfinite);
        ASSERT(!term.capture());

        BackTrackInfoParenthesesTerminal* backTrack = reinterpret_cast<BackTrackInfoParenthesesTerminal*>(context->frame + term.frameLocation);
        backTrack->begin = input.getPos();
        backTrack->entryPosition = input.getPos();
        return true;
    }

    bool matchParenthesesTerminalEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalEnd);
        ASSERT(term.atom.quantityMinCount <= 1);

        BackTrackInfoParenthesesTerminal* backTrack = reinterpret_cast<BackTrackInfoParenthesesTerminal*>(context->frame + term.frameLocation);
        if (backTrack->begin == input.getPos()) {
            // One empty iteration satisfies +, but an empty iteration after
            // any completed iteration must fail to prevent an endless loop.
            if (!term.atom.quantityMinCount || backTrack->entryPosition != input.getPos())
                return false;
            backTrack->entryPosition = notFound;
        }
        backTrack->begin = input.getPos();

        // Successful match! Okay, what's next? - loop around and try to match more!
        // Initialize the group once; subsequent iterations enter its body.
        context->term -= term.atom.parenthesesWidth;
        return true;
    }

    bool backtrackParenthesesTerminalBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);
        ASSERT(term.atom.quantityType == QuantifierType::Greedy);
        ASSERT(term.atom.quantityMinCount <= 1);
        ASSERT(term.atom.quantityMaxCount == quantifyInfinite);
        ASSERT(!term.capture());

        // If we backtrack to this point, we have failed to match this iteration of the parens.
        // A failed iteration completes the match only after its minimum has
        // been satisfied. There is no following term that could benefit from
        // restoring an earlier iteration's alternatives.
        if (term.atom.quantityMinCount) {
            BackTrackInfoParenthesesTerminal* backTrack = reinterpret_cast<BackTrackInfoParenthesesTerminal*>(context->frame + term.frameLocation);
            if (backTrack->entryPosition == input.getPos())
                return false;
        }
        context->term += term.atom.parenthesesWidth;
        return true;
    }

    bool backtrackParenthesesTerminalEnd(ByteTerm&, DisjunctionContext*)
    {
        // 'Terminal' parentheses are at the end of the regex, and as such a match past end
        // should always be returned as a successful match - we should never backtrack to here.
        ASSERT_UNREACHABLE();
        return false;
    }

    bool matchParentheticalAssertionBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        backTrack->begin = input.getPos();

        return true;
    }

    bool matchParentheticalAssertionEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        // We've reached the end of the parens; if they are inverted, this is failure.
        if (term.invert()) {
            if (term.containsAnyCaptures()) {
                for (unsigned subpattern = term.subpatternId(); subpattern <= term.lastSubpatternId(); subpattern++)
                    output[subpattern << 1] = offsetNoMatch;
            }
            context->term -= term.atom.parenthesesWidth;
            return false;
        }

        return true;
    }

    bool backtrackParentheticalAssertionBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        if (term.matchDirection() == Backward) {
            BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);
            input.setPos(backTrack->begin);
        }

        // We've failed to match parens; if they are inverted, this is win!
        if (term.invert()) {
            context->term += term.atom.parenthesesWidth;
            return true;
        }

        return false;
    }

    bool backtrackParentheticalAssertionEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        if (term.containsAnyCaptures()) {
            for (unsigned subpattern = term.subpatternId(); subpattern <= term.lastSubpatternId(); subpattern++)
                output[subpattern << 1] = offsetNoMatch;
        }

        context->term -= term.atom.parenthesesWidth;
        return false;
    }

    JSRegExpResult matchParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpattern);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);
        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        backTrack->begin = input.getPos();
        backTrack->matchAmount = 0;
        backTrack->lastContext = nullptr;

        ASSERT(term.atom.quantityType != QuantifierType::FixedCount || term.atom.quantityMinCount == term.atom.quantityMaxCount);

        unsigned minimumMatchCount = term.atom.quantityMinCount;
        JSRegExpResult fixedMatchResult;

        // Handle fixed matches and the minimum part of a variable length match.
        if (minimumMatchCount) {
            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < minimumMatchCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (UNLIKELY(!context))
                    return JSRegExpResult::ErrorNoMemory;
                fixedMatchResult = matchDisjunction(disjunctionBody, context->getDisjunctionContext());
                if (fixedMatchResult == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                    
                    if (fixedMatchResult != JSRegExpResult::NoMatch)
                        return fixedMatchResult;
                    JSRegExpResult backtrackResult = parenthesesDoBacktrack(term, backTrack);
                    if (backtrackResult != JSRegExpResult::Match)
                        return backtrackResult;
                }
            }

            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
        }

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);
            return JSRegExpResult::Match;
        }

        case QuantifierType::Greedy: {
            while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (UNLIKELY(!context))
                    return JSRegExpResult::ErrorNoMemory;
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext());
                if (result == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);

                    if (result != JSRegExpResult::NoMatch)
                        return result;

                    break;
                }
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return JSRegExpResult::Match;
        }

        case QuantifierType::NonGreedy:
            return JSRegExpResult::Match;
        }

        ASSERT_UNREACHABLE();
        return JSRegExpResult::ErrorNoMatch;
    }

    // Rules for backtracking differ depending on whether this is greedy or non-greedy.
    //
    // Greedy matches never should try just adding more - you should already have done
    // the 'more' cases.  Always backtrack, at least a leetle bit.  However cases where
    // you backtrack an item off the list needs checking, since we'll never have matched
    // the one less case.  Tracking forwards, still add as much as possible.
    //
    // Non-greedy, we've already done the one less case, so don't match on popping.
    // We haven't done the one more case, so always try to add that.
    //
    JSRegExpResult backtrackParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpattern);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);
        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);

            ParenthesesDisjunctionContext* context = nullptr;
            JSRegExpResult result = parenthesesDoBacktrack(term, backTrack);

            if (result != JSRegExpResult::Match)
                return result;

            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (UNLIKELY(!context))
                    return JSRegExpResult::ErrorNoMemory;
                result = matchDisjunction(disjunctionBody, context->getDisjunctionContext());

                if (result == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);

                    if (result != JSRegExpResult::NoMatch)
                        return result;
                    JSRegExpResult backtrackResult = parenthesesDoBacktrack(term, backTrack);
                    if (backtrackResult != JSRegExpResult::Match)
                        return backtrackResult;
                }
            }

            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);
            context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
            return JSRegExpResult::Match;
        }

        case QuantifierType::Greedy: {
            if (!backTrack->matchAmount)
                return JSRegExpResult::NoMatch;

            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(), true);
            if (result == JSRegExpResult::Match) {
                while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                    ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                    if (UNLIKELY(!context))
                        return JSRegExpResult::ErrorNoMemory;
                    JSRegExpResult parenthesesResult = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext());
                    if (parenthesesResult == JSRegExpResult::Match)
                        appendParenthesesDisjunctionContext(backTrack, context);
                    else {
                        resetMatches(term, context);
                        freeParenthesesDisjunctionContext(context);

                        if (parenthesesResult != JSRegExpResult::NoMatch)
                            return parenthesesResult;

                        break;
                    }
                }
            } else {
                resetMatches(term, context);
                popParenthesesDisjunctionContext(backTrack);
                freeParenthesesDisjunctionContext(context);

                if (backTrack->matchAmount < term.atom.quantityMinCount) {
                    while (backTrack->matchAmount) {
                        context = backTrack->lastContext;
                        resetMatches(term, context);
                        popParenthesesDisjunctionContext(backTrack);
                        freeParenthesesDisjunctionContext(context);
                    }

                    input.setPos(backTrack->begin);
                    return result;
                }

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return JSRegExpResult::Match;
        }

        case QuantifierType::NonGreedy: {
            // If we've not reached the limit, try to add one more match.
            if (backTrack->matchAmount < term.atom.quantityMaxCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (UNLIKELY(!context))
                    return JSRegExpResult::ErrorNoMemory;
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext());
                if (result == JSRegExpResult::Match) {
                    appendParenthesesDisjunctionContext(backTrack, context);
                    recordParenthesesMatch(term, context);
                    return JSRegExpResult::Match;
                }

                resetMatches(term, context);
                freeParenthesesDisjunctionContext(context);

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            // Nope - okay backtrack looking for an alternative.
            while (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(), true);
                if (result == JSRegExpResult::Match) {
                    // successful backtrack! we're back in the game!
                    if (backTrack->matchAmount) {
                        context = backTrack->lastContext;
                        recordParenthesesMatch(term, context);
                    }
                    return JSRegExpResult::Match;
                }

                // pop a match off the stack
                resetMatches(term, context);
                popParenthesesDisjunctionContext(backTrack);
                freeParenthesesDisjunctionContext(context);

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            return JSRegExpResult::NoMatch;
        }
        }

        ASSERT_UNREACHABLE();
        return JSRegExpResult::ErrorNoMatch;
    }

    bool matchDotStarEnclosure(ByteTerm& term, DisjunctionContext* context)
    {
        UNUSED_PARAM(term);

        if (term.dotAll()) {
            context->matchBegin = startOffset;
            context->matchEnd = input.end();
            return true;
        }

        unsigned matchBegin = context->matchBegin;

        if (matchBegin > startOffset) {
            for (matchBegin--; true; matchBegin--) {
                if (testCharacterClass(pattern->newlineCharacterClass, input.reread(matchBegin))) {
                    ++matchBegin;
                    break;
                }

                if (matchBegin == startOffset)
                    break;
            }
        }

        unsigned matchEnd = input.getPos();

        for (; (matchEnd != input.end())
             && (!testCharacterClass(pattern->newlineCharacterClass, input.reread(matchEnd))); matchEnd++) { }

        if (((matchBegin && term.anchors.m_bol)
             || ((matchEnd != input.end()) && term.anchors.m_eol))
            && !term.multiline())
            return false;

        context->matchBegin = matchBegin;
        context->matchEnd = matchEnd;
        return true;
    }

#define dataLog(...)
#define dataLogIf(...)
#define dataLogLnIf(...)
#define MATCH_NEXT() { ++context->term; goto matchAgain; }
#define BACKTRACK() { --context->term; goto backtrack; }
#define currentTerm() (*context->term)

#define DUMP_TERM()
#define DUMP_EXTRA(...)
#define DUMP_EXTRA_IF(predicate, ...)
#define DUMP_CURR_CHAR()

    JSRegExpResult matchDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return JSRegExpResult::ErrorNoMemory;

        if (!--remainingMatchCount) {
            dataLogLnIf(verbose, "      Reached match limit - Returning ErrorHitLimit");
            return JSRegExpResult::ErrorHitLimit;
        }

        if (btrack)
            BACKTRACK();

        // Only the body may skip start offsets; a parentheses/assertion
        // disjunction has to match exactly where its caller left the input.
        // A once-through alternative has to try the requested position. Do
        // not scan ahead before its BOL check; the body search loop will skip
        // anchored alternatives and filter later starts if necessary.
        if (disjunction == pattern->m_body.get() && !disjunction->terms[0].alternative.onceThrough && !advanceToPossibleStart())
            return JSRegExpResult::NoMatch;

        context->matchBegin = input.getPos();
        context->term = disjunction->terms.data();

    matchAgain:
        ASSERT(context->term < disjunction->terms.data() + disjunction->terms.size());

        DUMP_TERM();

        switch (currentTerm().type) {
        case ByteTerm::Type::SubpatternBegin:
            DUMP_EXTRA_IF(currentTerm().capture(), "id:", currentTerm().subpatternId());
            MATCH_NEXT();
        case ByteTerm::Type::SubpatternEnd:
            DUMP_EXTRA_IF(currentTerm().capture(), "id:", currentTerm().subpatternId(), " - Return Match\n");
            context->matchEnd = input.getPos();
            return JSRegExpResult::Match;

        case ByteTerm::Type::BodyAlternativeBegin:
            MATCH_NEXT();
        case ByteTerm::Type::BodyAlternativeDisjunction:
        case ByteTerm::Type::BodyAlternativeEnd:
            context->matchEnd = input.getPos();
            DUMP_EXTRA("- Return Match\n");
            return JSRegExpResult::Match;

        case ByteTerm::Type::AlternativeBegin:
            MATCH_NEXT();
        case ByteTerm::Type::AlternativeDisjunction:
        case ByteTerm::Type::AlternativeEnd: {
            int offset = currentTerm().alternative.end;
            BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + currentTerm().frameLocation);
            backTrack->offset = offset;
            context->term += offset;
            MATCH_NEXT();
        }

        case ByteTerm::Type::AssertionBOL:
            if (matchAssertionBOL(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::AssertionEOL:
            if (matchAssertionEOL(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::AssertionWordBoundary:
            if (matchAssertionWordBoundary(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::PatternLiteral:
            if (input.matchesLiteral(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::PatternLiteral16:
            if (input.matchesLiteral16(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::CheckInputLiteral:
            if (!input.checkInput(currentTerm().frameLocation))
                BACKTRACK();
            if (input.matchesLiteral(currentTerm()))
                MATCH_NEXT();
            input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();
        case ByteTerm::Type::CheckInputLiteral16:
            if (!input.checkInput(currentTerm().frameLocation))
                BACKTRACK();
            if (input.matchesLiteral16(currentTerm()))
                MATCH_NEXT();
            input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();
        case ByteTerm::Type::CheckInputCharacter:
            if (!input.checkInput(currentTerm().frameLocation))
                BACKTRACK();
            if ((input.readCheckedDontAdvance(currentTerm().inputPosition) | currentTerm().literal.masks[0]) == currentTerm().literal.characters[0])
                MATCH_NEXT();
            input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();
        case ByteTerm::Type::CheckInputCharacter16:
            if (!input.checkInput(currentTerm().frameLocation))
                BACKTRACK();
            if ((input.readCheckedDontAdvance(currentTerm().inputPosition) | currentTerm().literal16.masks[0]) == currentTerm().literal16.characters[0])
                MATCH_NEXT();
            input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();
        case ByteTerm::Type::PatternCharacterOnce:
        case ByteTerm::Type::PatternCharacterFixed: {
            DUMP_CURR_CHAR();
            if (currentTerm().matchDirection() == Forward) {
                if (isEitherUnicodeCompilation()) {
                    if (!U_IS_BMP(currentTerm().atom.patternCharacter)) {
                        for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                            if (!checkSurrogatePair(currentTerm(), currentTerm().inputPosition - 2 * matchAmount))
                                BACKTRACK();
                        }
                        MATCH_NEXT();
                    }
                }

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacter(currentTerm(), currentTerm().inputPosition - matchAmount)) {
                        input.setPos(position);
                        BACKTRACK();
                    }
                }
            } else {
                auto& term = currentTerm();

                if (isEitherUnicodeCompilation()) {
                    if (!U_IS_BMP(term.atom.patternCharacter)) {
                        for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                            auto inputPosition = term.inputPosition + 2 * matchAmount;
                            if (input.getPos() < inputPosition)
                                BACKTRACK();
                            if (!checkSurrogatePair(term, inputPosition))
                                BACKTRACK();
                        }
                        MATCH_NEXT();
                    }
                }

                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacter(term, term.inputPosition + matchAmount + 1 - term.atom.quantityMaxCount)) {
                        input.setPos(position);
                        BACKTRACK();
                    }
                }
            }
            MATCH_NEXT();
        }
        case ByteTerm::Type::PatternCharacterGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);
            unsigned matchAmount = 0;
            unsigned position = input.getPos(); // May need to back out reading a surrogate pair.
            if (currentTerm().matchDirection() == Forward) {
                while ((matchAmount < currentTerm().atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCharacter(currentTerm(), currentTerm().inputPosition + 1)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
            } else {
                auto& term = currentTerm();
                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                    if (!checkCharacter(currentTerm(), term.inputPosition)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
            }
            backTrack->matchAmount = matchAmount;

            MATCH_NEXT();
        }

        case ByteTerm::Type::PatternCasedCharacterNonGreedy:
            // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
            ASSERT(!isEitherUnicodeCompilation() || U_IS_BMP(currentTerm().atom.patternCharacter));
            FALLTHROUGH;
        case ByteTerm::Type::PatternCharacterNonGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            MATCH_NEXT();
        }

        case ByteTerm::Type::PatternCasedCharacterOnce:
        case ByteTerm::Type::PatternCasedCharacterFixed: {
            DUMP_CURR_CHAR();
            if (isEitherUnicodeCompilation()) {
                // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
                ASSERT(U_IS_BMP(currentTerm().atom.patternCharacter));

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                if (currentTerm().matchDirection() == Forward) {
                    for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                        if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition - matchAmount)) {
                            input.setPos(position);
                            BACKTRACK();
                        }
                    }
                } else {
                    auto& term = currentTerm();

                    if (input.getPos() < term.inputPosition)
                        BACKTRACK();

                    for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                        if (!checkCasedCharacter(term, term.inputPosition - term.atom.quantityMaxCount + matchAmount + 1)) {
                            input.setPos(position);
                            BACKTRACK();
                        }
                    }
                }
                MATCH_NEXT();
            }

            for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition - matchAmount))
                    BACKTRACK();
            }
            MATCH_NEXT();
        }
        case ByteTerm::Type::PatternCasedCharacterGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);

            // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
            ASSERT(!isEitherUnicodeCompilation() || U_IS_BMP(currentTerm().atom.patternCharacter));

            if (currentTerm().matchDirection() == Forward) {
                unsigned matchAmount = 0;
                while ((matchAmount < currentTerm().atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition + 1)) {
                        input.uncheckInput(1);
                        break;
                    }
                    ++matchAmount;
                }
                backTrack->matchAmount = matchAmount;

                MATCH_NEXT();
            } else {
                auto& term = currentTerm();

                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                unsigned position = input.getPos();
                unsigned matchAmount = 0;
                while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                    if (!checkCasedCharacter(term, term.inputPosition)) {
                        input.setPos(position);
                        break;
                    }

                    ++matchAmount;
                    position = input.getPos();
                }
                backTrack->matchAmount = matchAmount;

                MATCH_NEXT();
            }
        }

        case ByteTerm::Type::CharacterClass:
        case ByteTerm::Type::CharacterClassWithNegativeAssertion:
            DUMP_CURR_CHAR();
            if (matchCharacterClass(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::CheckInputCapturedCharacterClass:
            if (!input.checkInput(currentTerm().frameLocation))
                BACKTRACK();
            FALLTHROUGH;
        case ByteTerm::Type::CapturedCharacterClass: {
            unsigned captureOffset = currentTerm().atom.parenthesesWidth << 1;
            if (checkCharacterClass(currentTerm(), currentTerm().inputPosition)) {
                unsigned begin = input.getPos() - currentTerm().inputPosition;
                output[captureOffset] = begin;
                output[captureOffset + 1] = begin + 1;
                MATCH_NEXT();
            }
            output[captureOffset] = offsetNoMatch;
            output[captureOffset + 1] = offsetNoMatch;
            if (currentTerm().type == ByteTerm::Type::CheckInputCapturedCharacterClass)
                input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();
        }
        case ByteTerm::Type::BackReference:
            if (matchBackReference(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpattern: {
            DUMP_EXTRA("\n");
            JSRegExpResult result = matchParentheses(currentTerm(), context);

            if (result == JSRegExpResult::Match) {
                DUMP_EXTRA("     ParenthesesSubpattern");
                MATCH_NEXT();
            }  else if (result != JSRegExpResult::NoMatch)
                return result;

            DUMP_EXTRA("     ParenthesesSubpattern");
            BACKTRACK();
        }
        case ByteTerm::Type::ParenthesesSubpatternOnceBegin:
            if (matchParenthesesOnceBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternOnceEnd:
            if (matchParenthesesOnceEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalBegin:
            if (matchParenthesesTerminalBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalEnd:
            if (matchParenthesesTerminalEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionBegin:
            if (matchParentheticalAssertionBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionEnd:
            if (matchParentheticalAssertionEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::CheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            if (input.checkInput(currentTerm().checkInputCount))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::UncheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.uncheckInput(currentTerm().checkInputCount);
            MATCH_NEXT();

        case ByteTerm::Type::HaveCheckedInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            if (input.isValidNegativeInputOffset(currentTerm().checkInputCount))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::DotStarEnclosure:
            if (matchDotStarEnclosure(currentTerm(), context)) {
                DUMP_EXTRA("- Return Match\n");
                return JSRegExpResult::Match;
            }
            BACKTRACK();
        }

        // We should never fall-through to here.
        ASSERT_UNREACHABLE();

    backtrack:
        ASSERT(context->term < disjunction->terms.data() + disjunction->terms.size());

        DUMP_TERM();

        switch (currentTerm().type) {
        case ByteTerm::Type::SubpatternBegin:
            DUMP_EXTRA("id:", currentTerm().subpatternId(), " - Return NoMatch\n");
            return JSRegExpResult::NoMatch;
        case ByteTerm::Type::SubpatternEnd:
            ASSERT_UNREACHABLE();

        case ByteTerm::Type::BodyAlternativeBegin:
        case ByteTerm::Type::BodyAlternativeDisjunction: {
            int offset = currentTerm().alternative.next;
            context->term += offset;
            if (offset > 0)
                MATCH_NEXT();

            // We have wrapped back to the first alternative, so the body failed at this
            // start offset. Alternatives flagged onceThrough are anchored by a
            // non-multiline ^ (optimizeBOL() has appended unanchored copies of the rest),
            // so they cannot match at any later offset - skip over all of them. If the
            // body is entirely onceThrough there is nothing left to retry.
            while (currentTerm().alternative.onceThrough) {
                int onceThroughNext = currentTerm().alternative.next;
                if (onceThroughNext <= 0) {
                    DUMP_EXTRA("- Return NoMatch\n");
                    return JSRegExpResult::NoMatch;
                }
                context->term += onceThroughNext;
            }

            if (input.atEnd() || pattern->sticky()) {
                DUMP_EXTRA("- Return NoMatch\n");
                return JSRegExpResult::NoMatch;
            }

            input.next();

            // Skip the start offsets where no alternative can even consume its
            // first character, rather than retrying the whole body at each one.
            if (!advanceToPossibleStart()) {
                DUMP_EXTRA("- Return NoMatch\n");
                return JSRegExpResult::NoMatch;
            }

            context->matchBegin = input.getPos();

            MATCH_NEXT();
        }
        case ByteTerm::Type::BodyAlternativeEnd:
            ASSERT_UNREACHABLE();

        case ByteTerm::Type::AlternativeBegin:
        case ByteTerm::Type::AlternativeDisjunction: {
            int offset = currentTerm().alternative.next;
            context->term += offset;
            if (offset > 0)
                MATCH_NEXT();
            BACKTRACK();
        }
        case ByteTerm::Type::AlternativeEnd: {
            // We should never backtrack back into an alternative of the main body of the regex.
            BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + currentTerm().frameLocation);
            unsigned offset = backTrack->offset;
            context->term -= offset;
            BACKTRACK();
        }

        case ByteTerm::Type::AssertionBOL:
        case ByteTerm::Type::AssertionEOL:
        case ByteTerm::Type::AssertionWordBoundary:
        case ByteTerm::Type::PatternLiteral:
        case ByteTerm::Type::PatternLiteral16:
            BACKTRACK();

        case ByteTerm::Type::CheckInputLiteral:
        case ByteTerm::Type::CheckInputLiteral16:
        case ByteTerm::Type::CheckInputCharacter:
        case ByteTerm::Type::CheckInputCharacter16:
            input.uncheckInput(currentTerm().frameLocation);
            BACKTRACK();

        case ByteTerm::Type::PatternCharacterOnce:
        case ByteTerm::Type::PatternCharacterFixed:
        case ByteTerm::Type::PatternCharacterGreedy:
        case ByteTerm::Type::PatternCharacterNonGreedy:
            if (backtrackPatternCharacter(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::PatternCasedCharacterOnce:
        case ByteTerm::Type::PatternCasedCharacterFixed:
        case ByteTerm::Type::PatternCasedCharacterGreedy:
        case ByteTerm::Type::PatternCasedCharacterNonGreedy:
            if (backtrackPatternCasedCharacter(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::CharacterClass:
        case ByteTerm::Type::CharacterClassWithNegativeAssertion:
            if (backtrackCharacterClass(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::CheckInputCapturedCharacterClass:
            input.uncheckInput(currentTerm().frameLocation);
            FALLTHROUGH;
        case ByteTerm::Type::CapturedCharacterClass: {
            unsigned captureOffset = currentTerm().atom.parenthesesWidth << 1;
            output[captureOffset] = offsetNoMatch;
            output[captureOffset + 1] = offsetNoMatch;
            BACKTRACK();
        }
        case ByteTerm::Type::BackReference:
            if (backtrackBackReference(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpattern: {
            JSRegExpResult result = backtrackParentheses(currentTerm(), context);

            if (result == JSRegExpResult::Match) {
                MATCH_NEXT();
            } else if (result != JSRegExpResult::NoMatch)
                return result;

            BACKTRACK();
        }
        case ByteTerm::Type::ParenthesesSubpatternOnceBegin:
            if (backtrackParenthesesOnceBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternOnceEnd:
            if (backtrackParenthesesOnceEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalBegin:
            if (backtrackParenthesesTerminalBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalEnd:
            if (backtrackParenthesesTerminalEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionBegin:
            if (backtrackParentheticalAssertionBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionEnd:
            if (backtrackParentheticalAssertionEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::CheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.uncheckInput(currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::UncheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.checkInput(currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::HaveCheckedInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::DotStarEnclosure:
            ASSERT_UNREACHABLE();
        }

        ASSERT_UNREACHABLE();
        return JSRegExpResult::ErrorNoMatch;
    }

    JSRegExpResult matchNonZeroDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        JSRegExpResult result = matchDisjunction(disjunction, context, btrack);

        if (result == JSRegExpResult::Match) {
            while (context->matchBegin == context->matchEnd) {
                result = matchDisjunction(disjunction, context, true);
                if (result != JSRegExpResult::Match)
                    return result;
            }
            return JSRegExpResult::Match;
        }

        return result;
    }

    // WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function does conditional locking.
    unsigned interpret() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195970
        // [Yarr Interpreter] The interpreter doesn't have checks for stack overflow due to deep recursion
        if (!input.isAvailableInput(0))
            return offsetNoMatch;

        if (pattern->hasEndAnchoredFixedSize() && input.end() >= pattern->m_endAnchoredFixedSize)
            input.setPos(std::max(input.getPos(), input.end() - pattern->m_endAnchoredFixedSize));

        if (pattern->m_fixedPrefixSearch && !pattern->m_fixedPrefixSearch.value()->requiredAtom.isEmpty()
            && !input.containsRequiredAtom(pattern->m_fixedPrefixSearch.value()->requiredAtom))
            return offsetNoMatch;

        using SpecificPattern = BytecodePattern::SpecificPattern;
        if (pattern->m_specificPattern == SpecificPattern::Newlines) {
            while (input.isAvailableInput(1)) {
                char32_t ch = input.peek(0);
                if (ch == '\r' || ch == '\n') {
                    unsigned length = 1;
                    if (ch == '\r' && input.isAvailableInput(2) && input.peek(1) == '\n')
                        ++length;
                    output[0] = input.getPos();
                    output[1] = output[0] + length;
                    return output[0];
                }
                if (pattern->sticky())
                    break;
                input.next();
            }
            return offsetNoMatch;
        }
        if (pattern->m_specificPattern != SpecificPattern::None) {
            auto type = pattern->m_specificPattern;
            bool leading = type == SpecificPattern::LeadingSpacesStar || type == SpecificPattern::LeadingSpacesPlus;
            bool requiresOne = type == SpecificPattern::LeadingSpacesPlus || type == SpecificPattern::TrailingSpacesPlus;
            const auto* spaces = YarrPattern::spacesCharacterClass();
            unsigned start = input.getPos();
            unsigned end = input.end();
            if (leading) {
                if (start)
                    return offsetNoMatch;
                end = 0;
                while (end < input.end() && testCharacterClass(spaces, input.peek(end)))
                    ++end;
            } else {
                start = end;
                while (start > input.getPos() && testCharacterClass(spaces, input.peek(start - input.getPos() - 1)))
                    --start;
            }
            if (requiresOne && start == end)
                return offsetNoMatch;
            output[0] = start;
            output[1] = end;
            return start;
        }

        if (pattern->m_fixedPrefixSearch && !pattern->m_fixedPrefixSearch.value()->atoms.isEmpty()) {
            const auto& search = *pattern->m_fixedPrefixSearch.value();
            if (search.anchoredStart && input.getPos())
                return offsetNoMatch;
            if (search.anchoredEnd && !search.anchoredStart && !pattern->sticky() && input.end() >= search.longestAtomLength)
                input.setPos(std::max(input.getPos(), input.end() - search.longestAtomLength));
            while (input.isAvailableInput(0)) {
                if (!advanceToPossibleStart())
                    return offsetNoMatch;
                // Preserve alternative order at each candidate position,
                // including empty alternatives and strings of different sizes.
                for (const auto& atom : search.atoms) {
                    unsigned length = atom.size();
                    if (!input.isAvailableInput(length)
                        || (search.anchoredEnd && input.getPos() + length != input.end()))
                        continue;
                    unsigned i = 0;
                    for (; i < length && input.peek(i) == atom[i]; ++i) { }
                    if (i == length) {
                        output[0] = input.getPos();
                        output[1] = output[0] + length;
                        return output[0];
                    }
                }
                if (input.atEnd() || search.anchoredStart || pattern->sticky())
                    break;
                input.next();
            }
            return offsetNoMatch;
        }

        // Inspired by V8 RegExpNode::EmitQuickCheck, specialized for a
        // Yarr body whose start filter permits only the anchored position.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        // An anchored body cannot search later starts. Reject its impossible
        // first character before allocating interpreter backtracking state.
        if (pattern->m_startCharFilter.valid && pattern->m_startCharFilter.fixedPosition
            && !advanceToPossibleStart())
            return offsetNoMatch;

        for (unsigned i = 0; i < pattern->m_body->m_numSubpatterns + 1; ++i)
            output[i << 1] = offsetNoMatch;

        for (unsigned i = pattern->m_offsetVectorBaseForNamedCaptures; i < pattern->m_offsetsSize; ++i)
            output[i] = 0;

        allocatorPool = pattern->m_allocator->startAllocator();
        RELEASE_ASSERT(allocatorPool);

        DisjunctionContext* context = allocDisjunctionContext(pattern->m_body.get());
        if (UNLIKELY(!context))
            return offsetNoMatch;

        dataLogLnIf(verbose, "  Interpret input: ", input, "\n  Matching");

        JSRegExpResult result = matchDisjunction(pattern->m_body.get(), context, false);
        if (result == JSRegExpResult::Match) {
            output[0] = context->matchBegin;
            output[1] = context->matchEnd;
        }

        freeDisjunctionContext(context);

        pattern->m_allocator->stopAllocator();

        ASSERT((result == JSRegExpResult::Match) == (output[0] != offsetNoMatch));

        return output[0];
    }

    Interpreter(BytecodePattern* pattern, unsigned* output, const CharType* input, size_t length, unsigned start)
        : pattern(pattern)
        , compileMode(pattern->compileMode())
        , output(output)
        , input(input, length, start, pattern->eitherUnicode())
        , startOffset(start)
        , remainingMatchCount(matchLimit)
    {
    }

private:
    inline bool isLegacyCompilation() const { return compileMode == CompileMode::Legacy; }
    inline bool isUnicodeCompilation() const { return compileMode == CompileMode::Unicode; }
    inline bool isUnicodeSetsCompilation() const { return compileMode == CompileMode::UnicodeSets; }
    inline bool isEitherUnicodeCompilation() const { return isUnicodeCompilation() || isUnicodeSetsCompilation(); }

    inline bool isSafeToRecurse() { return m_stackCheck.isSafeToRecurse(); }

    BytecodePattern* pattern;
    CompileMode compileMode;
    unsigned* output;
    InputStream input;
    StackCheck m_stackCheck;
    BumpPointerPool* allocatorPool { nullptr };
    unsigned startOffset;
    unsigned remainingMatchCount;
};


// Computes the StartCharFilter of a pattern (see YarrInterpreter.h) from the
// parsed form rather than from the bytecode, because PatternTerm still has the
// quantifiers and the nested disjunctions in place.
//
// The result has to be a *superset* of the characters a match can begin with -
// a character missing from it would silently turn a match into a no-match - so
// everything that is not modelled here bails out instead of guessing.
class StartCharFilterBuilder {
public:
    static ::Escargot::Optional<FixedPrefixSearch*> buildFixedPrefixSearch(YarrPattern& pattern)
    {
        ::Escargot::Optional<PatternDisjunction*> body = pattern.m_body;
        if (!body || body.value()->m_alternatives.isEmpty())
            return nullptr;

        FixedPrefixSearch search;
        if (!collectLiteralAlternatives(pattern, search)) {
            search.atoms.clear();
            search.atoms.shrinkToFit();
        }
        // Sticky atoms compare only the requested position.
        if (pattern.sticky())
            return search.atoms.isEmpty() ? nullptr : new FixedPrefixSearch(WTFMove(search));

        unsigned length = FixedPrefixSearch::maxLength;
        for (auto& alternative : body.value()->m_alternatives) {
            if (alternative->onceThrough()) {
                length = 0;
                break;
            }
            StartCharFilter positions[FixedPrefixSearch::maxLength];
            unsigned count = 0;
            unsigned budget = 256;
            collectPrefixAlternative(pattern, *alternative, positions, count, 0, budget);
            length = std::min(length, count);
            for (unsigned i = 0; i < length; ++i) {
                for (unsigned word = 0; word < 8; ++word)
                    search.positions[i].latin1Bitmap[word] |= positions[i].latin1Bitmap[word];
                search.positions[i].mayStartAboveLatin1 |= positions[i].mayStartAboveLatin1;
            }
        }

        bool selective = false;
        for (unsigned i = 0; i < length; ++i)
            selective |= !search.positions[i].mayStartAboveLatin1 || !isFullLatin1Bitmap(search.positions[i]);
        if (length >= 2 && selective) {
            search.length = length;
            // Adapted from V8 BoyerMooreLookahead::EmitSkipInstructions and
            // ChoiceNode::EmitSkipUntilSearchPrelude: a singleton lookahead
            // position permits a SkipUntilChar scan before the exact check.
            // Wider subjects require that position to have no wide members.
            // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
            for (unsigned i = 0; i < length; ++i) {
                unsigned count = 0;
                unsigned character = 0;
                for (unsigned ch = 0; ch < 256; ++ch) {
                    if (search.positions[i].latin1Bitmap[ch >> 5] & (1u << (ch & 31))) {
                        character = ch;
                        ++count;
                    }
                }
                if (count == 1) {
                    search.singleLatin1Character = character;
                    search.singleCharacterOffset = i;
                }
            }
            for (unsigned ch = 0; ch < 256; ++ch) {
                unsigned shift = length;
                for (unsigned i = 0; i + 1 < length; ++i) {
                    if (search.positions[i].latin1Bitmap[ch >> 5] & (1u << (ch & 31)))
                        shift = length - i - 1;
                }
                search.shifts[ch] = shift;
            }
            // V8 QuickCheckDetails::Rationalize uses the bits shared by
            // every admitted character to form a necessary packed check.
            // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
            if (length == sizeof(uint32_t)) {
                uint8_t masks[sizeof(uint32_t)] { };
                uint8_t values[sizeof(uint32_t)] { };
                for (unsigned i = 0; i < length; ++i) {
                    unsigned allBits = 0xff;
                    unsigned anyBits = 0;
                    bool hasCharacter = false;
                    for (unsigned ch = 0; ch < 256; ++ch) {
                        if (!(search.positions[i].latin1Bitmap[ch >> 5] & (1u << (ch & 31))))
                            continue;
                        allBits &= ch;
                        anyBits |= ch;
                        hasCharacter = true;
                    }
                    if (hasCharacter) {
                        masks[i] = static_cast<uint8_t>(~(allBits ^ anyBits));
                        values[i] = static_cast<uint8_t>(allBits & masks[i]);
                    }
                }
                // Build words in the same byte order as the subject load.
                memcpy(&search.packedMask, masks, sizeof(search.packedMask));
                memcpy(&search.packedValue, values, sizeof(search.packedValue));
            }
        }
        // A literal beyond a variable-width prefix still has to occur in
        // every successful match. Require the same mandatory literal in
        // every alternative, including copies split by beginning anchors.
        if (!pattern.eitherUnicode() && search.atoms.isEmpty()) {
            bool first = true;
            for (auto& alternative : body.value()->m_alternatives) {
                Vector<LChar> longest;
                Vector<LChar> candidate;
                for (auto& term : alternative->m_terms) {
                    if (term.type == PatternTerm::Type::PatternCharacter && term.matchDirection() == Forward
                        && !term.ignoreCase() && term.quantityMinCount == 1 && term.quantityMaxCount == 1
                        && term.patternCharacter <= 0xff) {
                        if (candidate.size() < 16)
                            candidate.append(static_cast<LChar>(term.patternCharacter));
                        if (candidate.size() > longest.size())
                            longest = candidate;
                    } else
                        candidate.clear();
                }
                if (longest.size() < 4 || longest.size() <= search.length) {
                    search.requiredAtom.clear();
                    break;
                }
                if (first) {
                    search.requiredAtom = WTFMove(longest);
                    first = false;
                } else if (longest.size() != search.requiredAtom.size()
                    || memcmp(&longest[0], &search.requiredAtom[0], longest.size())) {
                    search.requiredAtom.clear();
                    break;
                }
            }
        }
        if (!search.length && search.atoms.isEmpty() && search.requiredAtom.isEmpty())
            return nullptr;
        return new FixedPrefixSearch(WTFMove(search));
    }

    static bool collectLiteralAlternatives(YarrPattern& pattern, FixedPrefixSearch& search)
    {
        if (pattern.m_numSubpatterns || pattern.m_containsModifiers || pattern.ignoreCase())
            return false;
        auto* disjunction = pattern.m_body;
        unsigned first = 0;
        unsigned last = 0;
        while (disjunction->m_alternatives.size() == 1) {
            const auto& terms = disjunction->m_alternatives[0]->m_terms;
            first = 0;
            last = terms.size();
            if (!pattern.multiline()) {
                if (first < last && terms[first].type == PatternTerm::Type::AssertionBOL) {
                    search.anchoredStart = true;
                    ++first;
                }
                if (first < last && terms[last - 1].type == PatternTerm::Type::AssertionEOL) {
                    search.anchoredEnd = true;
                    --last;
                }
            }
            if (last - first != 1)
                break;
            const auto& term = terms[first];
            if (term.type != PatternTerm::Type::ParenthesesSubpattern || term.m_capture
                || term.m_matchDirection != Forward || term.quantityMinCount != 1 || term.quantityMaxCount != 1)
                break;
            disjunction = term.parentheses.disjunction;
        }
        for (auto& alternative : disjunction->m_alternatives) {
            const auto& terms = alternative->m_terms;
            unsigned begin = disjunction->m_alternatives.size() == 1 ? first : 0;
            unsigned end = disjunction->m_alternatives.size() == 1 ? last : terms.size();
            Vector<UChar> atom;
            for (unsigned i = begin; i < end; ++i) {
                const auto& term = terms[i];
                if (term.type != PatternTerm::Type::PatternCharacter || term.m_currentFlags.contains(Flags::IgnoreCase)
                    || term.m_matchDirection != Forward || term.quantityMinCount != 1 || term.quantityMaxCount != 1
                    || !U_IS_BMP(term.patternCharacter)
                    || (pattern.eitherUnicode() && U_IS_SURROGATE(term.patternCharacter)))
                    return false;
                atom.append(static_cast<UChar>(term.patternCharacter));
            }
            if (pattern.eitherUnicode() && atom.isEmpty())
                return false;
            search.longestAtomLength = std::max(search.longestAtomLength, static_cast<unsigned>(atom.size()));
            search.atoms.append(WTFMove(atom));
        }
        return !search.atoms.isEmpty();
    }

    static bool build(YarrPattern& pattern, StartCharFilter& filter)
    {
        // Unicode patterns advance start offsets by code point, so an offset
        // skipped here could be the trail surrogate of a pair.
        if (pattern.eitherUnicode())
            return false;

        // A sticky pattern only ever tries one start offset.
        if (pattern.sticky())
            return false;

        PatternDisjunction* body = pattern.m_body;
        if (!body || body->m_alternatives.isEmpty())
            return false;

        // Anchored bodies use the filter only at the requested position,
        // before entering bytecode. They must never scan ahead on rejection.
        bool allOnceThrough = true;
        for (auto& alternative : body->m_alternatives) {
            if (!alternative->onceThrough()) {
                allOnceThrough = false;
                break;
            }
        }
        bool canMatchEmpty = false;
        if (!collectDisjunction(body, filter, canMatchEmpty, 0))
            return false;

        // Skipping a start offset is justified by "no alternative can consume a
        // character here", which only implies "no match here" if every match
        // does consume a character.
        if (canMatchEmpty)
            return false;

        // A filter that accepts every character can never skip an offset, so it
        // would be pure overhead.
        if (filter.mayStartAboveLatin1 && isFullLatin1Bitmap(filter))
            return false;

        filter.fixedPosition = allOnceThrough;
        return true;
    }

private:
    // Alternations can nest arbitrarily deep; the limit bounds this recursion at
    // the price of not filtering the few patterns that go deeper.
    static constexpr unsigned maxRecursionDepth = 16;

    // Adapted from V8 ActionNode::GetQuickCheckDetails and
    // ChoiceNode::GetQuickCheckDetails: captures do not consume input, and
    // alternatives contribute the union at each guaranteed prefix position.
    // Continue past a group only when every branch has the same fixed width.
    // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
    static bool collectPrefixAlternative(YarrPattern& pattern, PatternAlternative& alternative,
        StartCharFilter* positions, unsigned& count, unsigned depth, unsigned& budget)
    {
        if (depth >= maxRecursionDepth || alternative.matchDirection() != Forward)
            return false;
        for (auto& term : alternative.m_terms) {
            if (!budget || count == FixedPrefixSearch::maxLength || term.matchDirection() != Forward)
                return false;
            --budget;
            if (term.type == PatternTerm::Type::AssertionBOL || term.type == PatternTerm::Type::AssertionEOL
                || term.type == PatternTerm::Type::AssertionWordBoundary || term.type == PatternTerm::Type::ParentheticalAssertion)
                continue;
            if (!term.quantityMaxCount)
                continue;
            if (!term.quantityMinCount)
                return false;

            StartCharFilter termPositions[FixedPrefixSearch::maxLength];
            unsigned termLength = 1;
            if (term.type == PatternTerm::Type::ParenthesesSubpattern) {
                if (term.invert() || term.parentheses.disjunction->m_alternatives.isEmpty())
                    return false;
                termLength = FixedPrefixSearch::maxLength;
                unsigned fixedLength = 0;
                bool first = true;
                bool complete = true;
                for (auto& branch : term.parentheses.disjunction->m_alternatives) {
                    StartCharFilter branchPositions[FixedPrefixSearch::maxLength];
                    unsigned branchLength = 0;
                    bool branchComplete = collectPrefixAlternative(pattern, *branch, branchPositions, branchLength, depth + 1, budget);
                    complete &= branchComplete && (first || fixedLength == branchLength);
                    if (first) {
                        fixedLength = branchLength;
                        first = false;
                    }
                    termLength = std::min(termLength, branchLength);
                    for (unsigned i = 0; i < termLength; ++i) {
                        for (unsigned word = 0; word < 8; ++word)
                            termPositions[i].latin1Bitmap[word] |= branchPositions[i].latin1Bitmap[word];
                        termPositions[i].mayStartAboveLatin1 |= branchPositions[i].mayStartAboveLatin1;
                    }
                }
                if (!complete) {
                    for (unsigned i = 0; i < termLength && count < FixedPrefixSearch::maxLength; ++i)
                        positions[count++] = termPositions[i];
                    return false;
                }
                // A group with only zero-width terms stays zero-width even
                // when quantified. Do not iterate an unbounded empty repeat.
                if (!termLength)
                    continue;
            } else if (!collectPrefixCharacter(pattern, term, termPositions[0]))
                return false;

            unsigned minCount = term.quantityMinCount;
            for (unsigned repeat = 0; repeat < minCount && count < FixedPrefixSearch::maxLength; ++repeat) {
                for (unsigned i = 0; i < termLength && count < FixedPrefixSearch::maxLength; ++i)
                    positions[count++] = termPositions[i];
                if (count == FixedPrefixSearch::maxLength)
                    return false;
            }
            if (term.quantityMinCount != term.quantityMaxCount)
                return false;
        }
        return true;
    }

    static bool collectPrefixCharacter(YarrPattern& pattern, PatternTerm& term, StartCharFilter& filter)
    {
        if (term.type == PatternTerm::Type::PatternCharacter) {
            char32_t ch = term.patternCharacter;
            if (pattern.eitherUnicode() && (!U_IS_BMP(ch) || U_IS_SURROGATE(ch) || term.ignoreCase()))
                return false;
            if (term.ignoreCase()) {
                if (!isASCII(ch))
                    return false;
                addChar(filter, toASCIILower(ch));
                addChar(filter, toASCIIUpper(ch));
            } else
                addChar(filter, ch);
        } else if (term.type == PatternTerm::Type::CharacterClass) {
            if (pattern.eitherUnicode()) {
                // A Unicode prefix position must consume exactly one
                // code unit. Exclude folding, inversion and surrogate
                // membership instead of treating a pair as two terms.
                const auto& characterClass = *term.characterClass;
                if (term.ignoreCase() || term.invert() || !characterClass.hasOneCharacterSize()
                    || characterClass.hasNonBMPCharacters())
                    return false;
                bool hasSurrogates = false;
                for (auto ch : characterClass.m_matchesUnicode)
                    hasSurrogates |= U_IS_SURROGATE(ch);
                for (auto range : characterClass.m_rangesUnicode)
                    hasSurrogates |= range.begin <= 0xdfff && range.end >= 0xd800;
                if (hasSurrogates)
                    return false;
            }
            // Legacy classes already contain their case folds.
            if (!addCharacterClassTerm(filter, term))
                return false;
        } else
            return false;

        return true;
    }

    static bool isFullLatin1Bitmap(const StartCharFilter& filter)
    {
        for (unsigned i = 0; i < 8; ++i) {
            if (filter.latin1Bitmap[i] != ~static_cast<uint32_t>(0))
                return false;
        }
        return true;
    }

    static void addChar(StartCharFilter& filter, char32_t ch)
    {
        if (ch > 0xFF) {
            filter.mayStartAboveLatin1 = true;
            return;
        }
        filter.latin1Bitmap[ch >> 5] |= static_cast<uint32_t>(1) << (ch & 31);
    }

    static void addRange(StartCharFilter& filter, char32_t begin, char32_t end)
    {
        if (end > 0xFF) {
            filter.mayStartAboveLatin1 = true;
            end = 0xFF;
        }
        for (char32_t ch = begin; ch <= end; ++ch)
            addChar(filter, ch);
    }

    // Fills in exactly the 0x00..0xFF characters the class matches, mirroring
    // Interpreter::testCharacterClass(): it splits the lookup at 0x80 and does
    // not consult m_table. Being exact rather than a superset is what makes it
    // safe to complement this for an inverted class.
    static bool addCharacterClass(StartCharFilter& filter, const CharacterClass* characterClass)
    {
        // Class set strings (/v) can consume multiple code points and cannot
        // contribute a filter for a single prefix position.
        if (characterClass->hasStrings())
            return false;

        if (characterClass->m_anyCharacter) {
            addRange(filter, 0, 0xFF);
            filter.mayStartAboveLatin1 = true;
            return true;
        }

        for (auto match : characterClass->m_matches) {
            if (isASCII(match))
                addChar(filter, match);
        }
        for (auto range : characterClass->m_ranges) {
            if (range.begin <= 0x7F)
                addRange(filter, range.begin, std::min<char32_t>(range.end, 0x7F));
        }
        for (auto match : characterClass->m_matchesUnicode) {
            if (!isASCII(match))
                addChar(filter, match);
        }
        for (auto range : characterClass->m_rangesUnicode) {
            if (range.end >= 0x80)
                addRange(filter, std::max<char32_t>(range.begin, 0x80), range.end);
        }
        return true;
    }

    static bool addCharacterClassTerm(StartCharFilter& filter, PatternTerm& term)
    {
        if (!term.invert())
            return addCharacterClass(filter, term.characterClass);

        StartCharFilter classFilter;
        if (!addCharacterClass(classFilter, term.characterClass))
            return false;

        for (unsigned i = 0; i < 8; ++i)
            filter.latin1Bitmap[i] |= ~classFilter.latin1Bitmap[i];
        // Which characters above Latin1 the complement covers is not tracked.
        filter.mayStartAboveLatin1 = true;
        return true;
    }

    // Unions the first-character set of the alternative into filter. Sets
    // canMatchEmpty when no term of it is guaranteed to consume a character, in
    // which case the alternative can match the empty string at any offset.
    static bool collectAlternative(PatternAlternative* alternative, StartCharFilter& filter, bool& canMatchEmpty, unsigned depth)
    {
        // Only the terms of a lookbehind match backwards, and those are not
        // walked - but check rather than rely on it.
        if (alternative->matchDirection() != Forward)
            return false;

        for (auto& term : alternative->m_terms) {
            if (term.matchDirection() != Forward)
                return false;

            switch (term.type) {
            case PatternTerm::Type::AssertionBOL:
            case PatternTerm::Type::AssertionEOL:
            case PatternTerm::Type::AssertionWordBoundary:
                // Zero-width: it can only narrow down where a match starts.
                continue;

            case PatternTerm::Type::ParentheticalAssertion:
                // Zero-width as well. Ignoring what the assertion itself matches
                // keeps the set a superset, and means a lookbehind needs no
                // special care here.
                continue;

            case PatternTerm::Type::PatternCharacter: {
                char32_t ch = term.patternCharacter;
                if (term.ignoreCase()) {
                    // ByteCompiler::atomPatternCharacter() matches either case of
                    // an ASCII character; above ASCII the folding is ICU's, and
                    // not worth predicting here.
                    if (!isASCII(ch))
                        return false;
                    addChar(filter, toASCIILower(ch));
                    addChar(filter, toASCIIUpper(ch));
                } else
                    addChar(filter, ch);
                break;
            }

            case PatternTerm::Type::CharacterClass:
                // Interpreter::checkCharacterClass() and its variants fold the
                // input character at match time, so for an ignoreCase term the
                // class contents are not a superset of what it matches.
                if (term.ignoreCase())
                    return false;
                if (!addCharacterClassTerm(filter, term))
                    return false;
                break;

            case PatternTerm::Type::ParenthesesSubpattern: {
                if (term.invert())
                    return false;
                // A {0} group matches nothing at all.
                if (!term.quantityMaxCount)
                    continue;
                if (depth >= maxRecursionDepth)
                    return false;

                bool subCanMatchEmpty = false;
                if (!collectDisjunction(term.parentheses.disjunction, filter, subCanMatchEmpty, depth + 1))
                    return false;
                // The group can be passed without consuming anything, so a later
                // term can still be the one that starts the match.
                if (subCanMatchEmpty)
                    continue;
                break;
            }

            case PatternTerm::Type::BackReference:
            case PatternTerm::Type::ForwardReference:
            case PatternTerm::Type::DotStarEnclosure:
                // A reference matches whatever was captured, and DotStarEnclosure
                // stands for a `.*`-wrapped expression: first character unknown.
                return false;
            }

            // Only reached for a term that contributed to the set.
            unsigned minCount = term.quantityMinCount;
            if (!minCount)
                continue;

            // The term must consume a character, so no later term can be the one
            // that starts the match.
            return true;
        }

        canMatchEmpty = true;
        return true;
    }

    static bool collectDisjunction(PatternDisjunction* disjunction, StartCharFilter& filter, bool& canMatchEmpty, unsigned depth)
    {
        if (disjunction->m_alternatives.isEmpty())
            return false;

        for (auto& alternative : disjunction->m_alternatives) {
            bool alternativeCanMatchEmpty = false;
            if (!collectAlternative(alternative.get(), filter, alternativeCanMatchEmpty, depth))
                return false;
            if (alternativeCanMatchEmpty)
                canMatchEmpty = true;
        }
        return true;
    }
};


class ByteCompiler {
    struct ParenthesesStackEntry {
        unsigned beginTerm;
        unsigned savedAlternativeIndex;
        ParenthesesStackEntry(unsigned beginTerm, unsigned savedAlternativeIndex/*, unsigned subpatternId, bool capture = false*/)
            : beginTerm(beginTerm)
            , savedAlternativeIndex(savedAlternativeIndex)
        {
        }
    };

public:
    ByteCompiler(YarrPattern& pattern)
        : m_pattern(pattern)
        , m_currentFlags(pattern.m_flags)
    {
    }

    std::unique_ptr<BytecodePattern> compile(BumpPointerAllocator* allocator, ErrorCode& errorCode)
    {
        if (UNLIKELY(!isSafeToRecurse())) {
            errorCode = ErrorCode::TooManyDisjunctions;
            return nullptr;
        }

        regexBegin(m_pattern.m_numSubpatterns, m_pattern.m_body->m_callFrameSize, m_pattern.m_body->m_alternatives[0]->onceThrough());
        if (auto error = emitDisjunction(m_pattern.m_body, 0, 0)) {
            errorCode = error.value();
            return nullptr;
        }
        regexEnd();

        auto bytecodePattern = makeUnique<BytecodePattern>(WTFMove(m_bodyDisjunction), m_allParenthesesInfo, m_pattern, allocator, m_pattern.offsetVectorBaseForNamedCaptures(), m_pattern.offsetsSize());
        bytecodePattern->m_specificPattern = extractSpacesPattern();
        if (bytecodePattern->m_specificPattern == BytecodePattern::SpecificPattern::None)
            bytecodePattern->m_specificPattern = extractNewlinesPattern();

        StartCharFilter& startCharFilter = bytecodePattern->m_startCharFilter;
        startCharFilter.valid = StartCharFilterBuilder::build(m_pattern, startCharFilter);
        // Adapted from V8 ChoiceNode::EmitSkipUntilSearchPrelude: a
        // singleton leading set permits a character scan on both subject
        // widths when it has no wide members. Other sets keep the full filter.
        // https://github.com/v8/v8/blob/e3e0f1c146fc15721a3e8f539ab412cd70fb1082/src/regexp/regexp-compiler.cc
        if (startCharFilter.valid) {
            unsigned count = 0;
            for (unsigned ch = 0; ch < 256; ++ch) {
                if (startCharFilter.latin1Bitmap[ch >> 5] & (1u << (ch & 31))) {
                    startCharFilter.singleLatin1Character = ch;
                    ++count;
                }
            }
            if (count != 1)
                startCharFilter.singleLatin1Character = 0x100;
        }
        bytecodePattern->m_fixedPrefixSearch = StartCharFilterBuilder::buildFixedPrefixSearch(m_pattern);

        return bytecodePattern;
    }

    BytecodePattern::SpecificPattern extractSpacesPattern()
    {
        using SpecificPattern = BytecodePattern::SpecificPattern;
        if (m_pattern.eitherUnicode() || m_pattern.sticky() || m_pattern.multiline()
            || m_pattern.m_containsModifiers || m_pattern.m_numSubpatterns
            || m_pattern.ignoreCase() || m_pattern.m_body->m_alternatives.size() != 1)
            return SpecificPattern::None;

        const auto& terms = m_pattern.m_body->m_alternatives[0]->m_terms;
        if (terms.size() != 2 && terms.size() != 3)
            return SpecificPattern::None;
        bool leading = terms[0].type == PatternTerm::Type::AssertionBOL;
        if (!leading && terms[terms.size() - 1].type != PatternTerm::Type::AssertionEOL)
            return SpecificPattern::None;

        unsigned first = leading ? 1 : 0;
        unsigned last = terms.size() - (leading ? 0 : 1);
        const auto* spaces = YarrPattern::spacesCharacterClass();
        for (unsigned i = first; i < last; ++i) {
            const auto& term = terms[i];
            if (term.type != PatternTerm::Type::CharacterClass || term.characterClass != spaces
                || term.m_invert || term.m_matchDirection != Forward)
                return SpecificPattern::None;
        }
        const auto& greedy = terms[last - 1];
        if (greedy.quantityType != QuantifierType::Greedy || greedy.quantityMinCount
            || greedy.quantityMaxCount != quantifyInfinite)
            return SpecificPattern::None;
        bool requiresOne = last - first == 2;
        if (requiresOne) {
            const auto& once = terms[first];
            if (once.quantityType != QuantifierType::FixedCount || once.quantityMinCount != 1 || once.quantityMaxCount != 1)
                return SpecificPattern::None;
        }
        if (leading)
            return requiresOne ? SpecificPattern::LeadingSpacesPlus : SpecificPattern::LeadingSpacesStar;
        return requiresOne ? SpecificPattern::TrailingSpacesPlus : SpecificPattern::TrailingSpacesStar;
    }

    BytecodePattern::SpecificPattern extractNewlinesPattern()
    {
        using SpecificPattern = BytecodePattern::SpecificPattern;
        if (m_pattern.m_numSubpatterns || m_pattern.m_containsModifiers)
            return SpecificPattern::None;

        auto* disjunction = m_pattern.m_body;
        // Non-capturing wrappers do not alter this pattern's matching order.
        while (disjunction->m_alternatives.size() == 1) {
            auto& terms = disjunction->m_alternatives[0]->m_terms;
            if (terms.size() != 1)
                return SpecificPattern::None;
            const auto& term = terms[0];
            if (term.type != PatternTerm::Type::ParenthesesSubpattern || term.m_capture
                || term.m_matchDirection != Forward || term.quantityMinCount != 1 || term.quantityMaxCount != 1)
                return SpecificPattern::None;
            disjunction = term.parentheses.disjunction;
        }
        if (disjunction->m_alternatives.size() != 2)
            return SpecificPattern::None;

        auto isCharacter = [](const PatternTerm& term, char32_t ch, bool optional) {
            return term.type == PatternTerm::Type::PatternCharacter && term.patternCharacter == ch
                && term.m_matchDirection == Forward && term.quantityMaxCount == 1
                && term.quantityMinCount == (optional ? 0 : 1)
                && term.quantityType == (optional ? QuantifierType::Greedy : QuantifierType::FixedCount);
        };
        auto isCRLF = [&](const PatternAlternative& alternative) {
            const auto& terms = alternative.m_terms;
            return terms.size() == 2 && isCharacter(terms[0], '\r', false) && isCharacter(terms[1], '\n', true);
        };
        auto isLF = [&](const PatternAlternative& alternative) {
            const auto& terms = alternative.m_terms;
            return terms.size() == 1 && isCharacter(terms[0], '\n', false);
        };
        const auto& first = *disjunction->m_alternatives[0];
        const auto& second = *disjunction->m_alternatives[1];
        if ((isCRLF(first) && isLF(second)) || (isLF(first) && isCRLF(second)))
            return SpecificPattern::Newlines;
        return SpecificPattern::None;
    }

    void checkInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::CheckInput(count, { }));
    }

    void uncheckInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::UncheckInput(count, { }));
    }

    void haveCheckedInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::HaveCheckedInput(count, { }));
    }

    void assertionBOL(unsigned inputPosition, OptionSet<Flags> flags)
    {
        m_bodyDisjunction->terms.append(ByteTerm::BOL(inputPosition, flags));
    }

    void assertionEOL(unsigned inputPosition, OptionSet<Flags> flags)
    {
        m_bodyDisjunction->terms.append(ByteTerm::EOL(inputPosition, flags));
    }

    void assertionWordBoundary(bool invert, MatchDirection matchDirection, unsigned inputPosition, OptionSet<Flags> flags)
    {
        m_bodyDisjunction->terms.append(ByteTerm::WordBoundary(invert, matchDirection, inputPosition, flags));
    }

    struct GuardedClassTerm {
        const CharacterClass* guard;
        const CharacterClass* characterClass;
        bool invert;
        OptionSet<Flags> flags;
    };

    std::optional<GuardedClassTerm> guardedClassFor(PatternTerm& parent)
    {
        if (m_pattern.eitherUnicode() || parent.matchDirection() != Forward || parent.capture()
            || parent.containsAnyCaptures() || parent.parentheses.isTerminal || parent.m_possessive)
            return std::nullopt;
        bool fixedOnce = parent.quantityType == QuantifierType::FixedCount && parent.quantityMinCount == 1
            && parent.quantityMaxCount == 1 && !parent.parentheses.isCopy;
        bool greedy = parent.quantityType == QuantifierType::Greedy && !parent.quantityMinCount;
        if (!fixedOnce && !greedy)
            return std::nullopt;
        auto& alternatives = parent.parentheses.disjunction->m_alternatives;
        if (alternatives.size() != 1 || alternatives[0]->m_terms.size() != 2)
            return std::nullopt;
        auto& assertion = alternatives[0]->m_terms[0];
        auto& consumer = alternatives[0]->m_terms[1];
        if (assertion.type != PatternTerm::Type::ParentheticalAssertion || !assertion.invert()
            || assertion.matchDirection() != Forward || assertion.containsAnyCaptures()
            || assertion.quantityType != QuantifierType::FixedCount || assertion.quantityMaxCount != 1
            || consumer.type != PatternTerm::Type::CharacterClass || consumer.matchDirection() != Forward
            || consumer.quantityType != QuantifierType::FixedCount || consumer.quantityMaxCount != 1
            || assertion.inputPosition != consumer.inputPosition)
            return std::nullopt;
        auto& guardAlternatives = assertion.parentheses.disjunction->m_alternatives;
        if (guardAlternatives.size() != 1 || guardAlternatives[0]->m_terms.size() != 1)
            return std::nullopt;
        auto& guard = guardAlternatives[0]->m_terms[0];
        if (guard.type != PatternTerm::Type::CharacterClass || guard.invert() || guard.matchDirection() != Forward
            || guard.quantityType != QuantifierType::FixedCount || guard.quantityMaxCount != 1
            || guard.m_currentFlags != consumer.m_currentFlags)
            return std::nullopt;
        return GuardedClassTerm { guard.characterClass, consumer.characterClass, consumer.invert(), consumer.m_currentFlags };
    }

    void appendPatternCharacter(ByteTerm term)
    {
        // Fuse before control-flow offsets are finalized. Fixed BMP literals
        // consume one code unit in every mode; Unicode surrogate literals and
        // backward matching keep the scalar path.
        auto literalCharacter = [this](const ByteTerm& candidate, uint16_t& character, uint16_t& mask) {
            mask = 0;
            if (candidate.type == ByteTerm::Type::PatternCharacterOnce && U_IS_BMP(candidate.atom.patternCharacter)
                && (!m_pattern.eitherUnicode() || !U_IS_SURROGATE(candidate.atom.patternCharacter))) {
                character = candidate.atom.patternCharacter;
                return true;
            }
            if (candidate.type == ByteTerm::Type::PatternCasedCharacterOnce) {
                char32_t lo = candidate.atom.casedCharacter.lo;
                char32_t hi = candidate.atom.casedCharacter.hi;
                if (isASCIIAlpha(lo) && (lo ^ hi) == 0x20) {
                    mask = 0x20;
                    character = lo | mask;
                    return true;
                }
            }
            return false;
        };

        uint16_t character, mask;
        if (term.matchDirection() == Forward && literalCharacter(term, character, mask) && !m_bodyDisjunction->terms.isEmpty()) {
            auto& previous = m_bodyDisjunction->terms.last();
            if (previous.type == ByteTerm::Type::CheckInput) {
                unsigned count = previous.checkInputCount;
                previous = term;
                previous.frameLocation = count;
                memset(&previous.literal, 0, sizeof(previous.literal));
                if (character <= 0xff) {
                    previous.type = ByteTerm::Type::CheckInputCharacter;
                    previous.literal.characters[0] = character;
                    previous.literal.masks[0] = mask;
                    previous.literal.length = 1;
                } else {
                    previous.type = ByteTerm::Type::CheckInputCharacter16;
                    previous.literal16.characters[0] = character;
                    previous.literal16.masks[0] = mask;
                    previous.literal16.length = 1;
                }
                return;
            }
            if (previous.matchDirection() == Forward) {
                if (previous.type == ByteTerm::Type::CheckInputCharacter)
                    previous.type = ByteTerm::Type::CheckInputLiteral;
                if (previous.type == ByteTerm::Type::CheckInputCharacter16)
                    previous.type = ByteTerm::Type::CheckInputLiteral16;
                uint16_t previousCharacter, previousMask;
                if (literalCharacter(previous, previousCharacter, previousMask) && previous.inputPosition == term.inputPosition + 1) {
                    bool latin1 = previousCharacter <= 0xff && character <= 0xff;
                    previous.type = latin1 ? ByteTerm::Type::PatternLiteral : ByteTerm::Type::PatternLiteral16;
                    memset(&previous.literal, 0, sizeof(previous.literal));
                    if (latin1) {
                        previous.literal.characters[0] = previousCharacter;
                        previous.literal.masks[0] = previousMask;
                        previous.literal.length = 1;
                    } else {
                        previous.literal16.characters[0] = previousCharacter;
                        previous.literal16.masks[0] = previousMask;
                        previous.literal16.length = 1;
                    }
                }
                bool checkedLiteral = previous.type == ByteTerm::Type::CheckInputLiteral;
                if ((previous.type == ByteTerm::Type::PatternLiteral || checkedLiteral) && character > 0xff
                    && previous.literal.length < 4 && previous.inputPosition == term.inputPosition + previous.literal.length) {
                    unsigned length = previous.literal.length;
                    uint16_t characters[4] { }, masks[4] { };
                    for (unsigned i = 0; i < length; ++i) {
                        characters[i] = previous.literal.characters[i];
                        masks[i] = previous.literal.masks[i];
                    }
                    previous.type = checkedLiteral ? ByteTerm::Type::CheckInputLiteral16 : ByteTerm::Type::PatternLiteral16;
                    memcpy(previous.literal16.characters, characters, sizeof(characters));
                    memcpy(previous.literal16.masks, masks, sizeof(masks));
                    previous.literal16.length = length;
                }
                if ((previous.type == ByteTerm::Type::PatternLiteral || previous.type == ByteTerm::Type::CheckInputLiteral) && character <= 0xff
                    && previous.literal.length < sizeof(previous.literal.characters)
                    && previous.inputPosition == term.inputPosition + previous.literal.length) {
                    unsigned i = previous.literal.length++;
                    previous.literal.characters[i] = character;
                    previous.literal.masks[i] = mask;
                    return;
                }
                if ((previous.type == ByteTerm::Type::PatternLiteral16 || previous.type == ByteTerm::Type::CheckInputLiteral16) && previous.literal16.length < 4
                    && previous.inputPosition == term.inputPosition + previous.literal16.length) {
                    unsigned i = previous.literal16.length++;
                    previous.literal16.characters[i] = character;
                    previous.literal16.masks[i] = mask;
                    return;
                }
            }
        }
        m_bodyDisjunction->terms.append(term);
    }

    void atomPatternCharacter(char32_t ch, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType, OptionSet<Flags> flags)
    {
        if (flags.contains(Flags::IgnoreCase)) {
#if defined(ENABLE_ICU)
            char32_t lo;
            char32_t hi;
            if (ch < 128) {
                lo = tolower(ch);
                hi = toupper(ch);
            } else {
                // Escargot update
                // if ch is ALPHABETIC like latin or greek, we should not apply u_tolower or u_toupper (print('iI\u0130'.replace(/\u0130/gi, '#')))
                auto v = u_getIntPropertyValue(ch, UProperty::UCHAR_ALPHABETIC);
                if (v) {
                    lo = ch;
                    hi = ch;
                } else {
                    lo = u_tolower(ch);
                    hi = u_toupper(ch);
                }
            }
#else
            char32_t lo = tolower(ch);
            char32_t hi = toupper(ch);
#endif

            if (lo != hi) {
                ByteTerm term(lo, hi, inputPosition, frameLocation, quantityMaxCount, quantityType, flags);
                term.m_matchDirection = matchDirection;
                appendPatternCharacter(term);
                return;
            }
        }

        ByteTerm term(ch, inputPosition, frameLocation, quantityMaxCount, quantityType, flags);
        term.m_matchDirection = matchDirection;
        appendPatternCharacter(term);
    }

    void atomCharacterClass(const CharacterClass* characterClass, bool invert, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType, OptionSet<Flags> flags)
    {
        m_bodyDisjunction->terms.append(ByteTerm(characterClass, invert, inputPosition, flags));

        if (quantityType != QuantifierType::FixedCount)
            m_bodyDisjunction->terms.last().atom.quantityMinCount = 0;
        m_bodyDisjunction->terms.last().atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms.last().atom.quantityType = quantityType;
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.last().m_matchDirection = matchDirection;
    }

    void atomBackReference(unsigned subpatternId, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType, OptionSet<Flags> flags)
    {
        ASSERT(subpatternId);

        m_bodyDisjunction->terms.append(ByteTerm::BackReference(subpatternId, matchDirection, inputPosition, flags));

        if (m_pattern.hasDuplicateNamedCaptureGroups()) {
            auto duplicateNamedGroupId = m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId];
            if (duplicateNamedGroupId)
                m_bodyDisjunction->terms.last().atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
        }
        m_bodyDisjunction->terms.last().atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms.last().atom.quantityType = quantityType;
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
    }

    void atomParenthesesOnceBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceBegin, subpatternId, capture, false, matchDirection, inputPosition, m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParenthesesTerminalBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternTerminalBegin, subpatternId, capture, false, matchDirection, inputPosition, m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParenthesesSubpatternBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        // Errrk! - this is a little crazy, we initially generate as a Type::ParenthesesSubpatternOnceBegin,
        // then fix this up at the end! - simplifying this should make it much clearer.
        // https://bugs.webkit.org/show_bug.cgi?id=50136

        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceBegin, subpatternId, capture, false, matchDirection, inputPosition, m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParentheticalAssertionBegin(unsigned subpatternId, bool invert, MatchDirection matchDirection, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm::ParentheticalAssertionBegin(subpatternId, invert, matchDirection, m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(m_currentFlags));
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParentheticalAssertionEnd(unsigned lastSubpatternId, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParentheticalAssertionBegin);

        bool invert = m_bodyDisjunction->terms[beginTerm].invert();
        MatchDirection matchDirection = m_bodyDisjunction->terms[beginTerm].matchDirection();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        m_bodyDisjunction->terms.append(ByteTerm::ParentheticalAssertionEnd(subpatternId, lastSubpatternId, invert, matchDirection, m_currentFlags));
        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;

        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void assertionDotStarEnclosure(bool bolAnchored, bool eolAnchored)
    {
        m_bodyDisjunction->terms.append(ByteTerm::DotStarEnclosure(bolAnchored, eolAnchored, m_currentFlags));
    }

    unsigned popParenthesesStack()
    {
        ASSERT(m_parenthesesStack.size());
        unsigned beginTerm = m_parenthesesStack.last().beginTerm;
        m_currentAlternativeIndex = m_parenthesesStack.last().savedAlternativeIndex;
        m_parenthesesStack.removeLast();

        ASSERT(beginTerm < m_bodyDisjunction->terms.size());
        ASSERT(m_currentAlternativeIndex < m_bodyDisjunction->terms.size());

        return beginTerm;
    }

    void closeAlternative(unsigned beginTerm)
    {
        unsigned origBeginTerm = beginTerm;
        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::AlternativeBegin);
        unsigned endIndex = m_bodyDisjunction->terms.size();

        unsigned frameLocation = m_bodyDisjunction->terms[beginTerm].frameLocation;

        if (!m_bodyDisjunction->terms[beginTerm].alternative.next)
            m_bodyDisjunction->terms.remove(beginTerm);
        else {
            while (m_bodyDisjunction->terms[beginTerm].alternative.next) {
                beginTerm += m_bodyDisjunction->terms[beginTerm].alternative.next;
                ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::AlternativeDisjunction);
                m_bodyDisjunction->terms[beginTerm].alternative.end = endIndex - beginTerm;
                m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
            }

            m_bodyDisjunction->terms[beginTerm].alternative.next = origBeginTerm - beginTerm;

            m_bodyDisjunction->terms.append(ByteTerm::AlternativeEnd(m_currentFlags));
            m_bodyDisjunction->terms[endIndex].frameLocation = frameLocation;
        }
    }

    void closeBodyAlternative()
    {
        unsigned beginTerm = 0;
        unsigned origBeginTerm = 0;
        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::BodyAlternativeBegin);
        unsigned endIndex = m_bodyDisjunction->terms.size();

        unsigned frameLocation = m_bodyDisjunction->terms[beginTerm].frameLocation;

        while (m_bodyDisjunction->terms[beginTerm].alternative.next) {
            beginTerm += m_bodyDisjunction->terms[beginTerm].alternative.next;
            ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::BodyAlternativeDisjunction);
            m_bodyDisjunction->terms[beginTerm].alternative.end = endIndex - beginTerm;
            m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
        }

        m_bodyDisjunction->terms[beginTerm].alternative.next = origBeginTerm - beginTerm;

        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeEnd(m_currentFlags));
        m_bodyDisjunction->terms[endIndex].frameLocation = frameLocation;
    }

    void atomParenthesesSubpatternEnd(unsigned lastSubpatternId, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType, unsigned callFrameSize = 0)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);

        ByteTerm& parenthesesBegin = m_bodyDisjunction->terms[beginTerm];

        auto parenthesesMatchDirection = parenthesesBegin.matchDirection();
        bool capture = parenthesesBegin.capture();
        unsigned subpatternId = parenthesesBegin.subpatternId();

        unsigned numSubpatterns = lastSubpatternId - subpatternId + 1;
        auto parenthesesDisjunction = makeUnique<ByteDisjunction>(numSubpatterns, callFrameSize);

        unsigned firstTermInParentheses = beginTerm + 1;
        parenthesesDisjunction->terms.reserveInitialCapacity(endTerm - firstTermInParentheses + 2);

        parenthesesDisjunction->terms.append(ByteTerm::SubpatternBegin(m_currentFlags));
        for (unsigned termInParentheses = firstTermInParentheses; termInParentheses < endTerm; ++termInParentheses)
            parenthesesDisjunction->terms.append(m_bodyDisjunction->terms[termInParentheses]);
        parenthesesDisjunction->terms.append(ByteTerm::SubpatternEnd(m_currentFlags));

        m_bodyDisjunction->terms.shrink(beginTerm);

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpattern, subpatternId, parenthesesDisjunction.get(), capture, inputPosition, m_currentFlags));
        m_bodyDisjunction->terms.last().m_matchDirection = parenthesesMatchDirection;
        m_allParenthesesInfo.append(WTFMove(parenthesesDisjunction));

        if (m_pattern.hasDuplicateNamedCaptureGroups() && capture) {
            auto duplicateNamedGroupId = m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId];
            if (duplicateNamedGroupId)
                m_bodyDisjunction->terms[beginTerm].atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
        }

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
    }

    void atomParenthesesOnceEnd(unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);

        bool capture = m_bodyDisjunction->terms[beginTerm].capture();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        // A fixed single-class capture has no alternatives or variable-width
        // state to restore. Fuse its capture writes with the class comparison
        // before the enclosing alternative's offsets are finalized.
        if (capture && !m_pattern.eitherUnicode() && !m_pattern.hasDuplicateNamedCaptureGroups()
            && quantityType == QuantifierType::FixedCount && quantityMaxCount == 1
            && m_bodyDisjunction->terms[beginTerm].matchDirection() == Forward && endTerm == beginTerm + 2) {
            ByteTerm term = m_bodyDisjunction->terms[beginTerm + 1];
            if (term.type == ByteTerm::Type::CharacterClass && term.matchDirection() == Forward
                && term.atom.quantityType == QuantifierType::FixedCount && term.atom.quantityMaxCount == 1
                && term.inputPosition == m_bodyDisjunction->terms[beginTerm].inputPosition
                && term.inputPosition == inputPosition + 1) {
                term.type = ByteTerm::Type::CapturedCharacterClass;
                term.atom.parenthesesWidth = subpatternId;
                term.m_capture = true;
                m_bodyDisjunction->terms.shrink(beginTerm);
                if (beginTerm && m_bodyDisjunction->terms.last().type == ByteTerm::Type::CheckInput) {
                    term.type = ByteTerm::Type::CheckInputCapturedCharacterClass;
                    term.frameLocation = m_bodyDisjunction->terms.last().checkInputCount;
                    m_bodyDisjunction->terms.last() = term;
                } else
                    m_bodyDisjunction->terms.append(term);
                return;
            }
        }

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceEnd, subpatternId, capture, false, inputPosition, m_currentFlags));
        if (m_bodyDisjunction->terms[beginTerm].matchDirection() == Backward) {
            // Swap input positions for backward captures.
            m_bodyDisjunction->terms[endTerm].inputPosition = m_bodyDisjunction->terms[beginTerm].inputPosition;
            m_bodyDisjunction->terms[beginTerm].inputPosition = inputPosition;
        }

        if (m_pattern.hasDuplicateNamedCaptureGroups() && m_bodyDisjunction->terms[beginTerm].capture()) {
            auto duplicateNamedGroupId = m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId];
            if (duplicateNamedGroupId) {
                m_bodyDisjunction->terms[endTerm].atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
                m_bodyDisjunction->terms[beginTerm].atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
            }
        }

        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;
        m_bodyDisjunction->terms[endTerm].m_matchDirection = m_bodyDisjunction->terms[beginTerm].matchDirection();

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void atomParenthesesTerminalEnd(unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);

        if (m_bodyDisjunction->terms[beginTerm].matchDirection() == Backward)
            inputPosition = 0;
        bool capture = m_bodyDisjunction->terms[beginTerm].capture();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternTerminalEnd, subpatternId, capture, false, inputPosition, m_currentFlags));
        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;

        if (m_pattern.hasDuplicateNamedCaptureGroups() && m_bodyDisjunction->terms[beginTerm].capture()) {
            auto duplicateNamedGroupId = m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId];
            if (duplicateNamedGroupId) {
                m_bodyDisjunction->terms[endTerm].atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
                m_bodyDisjunction->terms[beginTerm].atom.parenIds.duplicateNamedGroupId = duplicateNamedGroupId;
            }
        }

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void regexBegin(unsigned numSubpatterns, unsigned callFrameSize, bool onceThrough)
    {
        m_bodyDisjunction = makeUnique<ByteDisjunction>(numSubpatterns, callFrameSize);
        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeBegin(onceThrough, m_currentFlags));
        m_bodyDisjunction->terms[0].frameLocation = 0;
        m_currentAlternativeIndex = 0;
    }

    void regexEnd()
    {
        closeBodyAlternative();
    }

    void alternativeBodyDisjunction(bool onceThrough)
    {
        unsigned newAlternativeIndex = m_bodyDisjunction->terms.size();
        m_bodyDisjunction->terms[m_currentAlternativeIndex].alternative.next = newAlternativeIndex - m_currentAlternativeIndex;
        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeDisjunction(onceThrough, m_currentFlags));

        m_currentAlternativeIndex = newAlternativeIndex;
    }

    void alternativeDisjunction()
    {
        unsigned newAlternativeIndex = m_bodyDisjunction->terms.size();
        m_bodyDisjunction->terms[m_currentAlternativeIndex].alternative.next = newAlternativeIndex - m_currentAlternativeIndex;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeDisjunction(m_currentFlags));

        m_currentAlternativeIndex = newAlternativeIndex;
    }

    std::optional<ErrorCode> WARN_UNUSED_RETURN emitDisjunction(PatternDisjunction* disjunction, CheckedUint32 inputCountAlreadyChecked, unsigned parenthesesInputCountAlreadyChecked, MatchDirection matchDirection = Forward)
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return ErrorCode::TooManyDisjunctions;

        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            auto currentCountAlreadyChecked = inputCountAlreadyChecked;

            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();

            if (alt) {
                if (disjunction == m_pattern.m_body)
                    alternativeBodyDisjunction(alternative->onceThrough());
                else
                    alternativeDisjunction();
            }

            unsigned minimumSize = alternative->m_minimumSize;
            ASSERT(matchDirection == Backward || minimumSize >= parenthesesInputCountAlreadyChecked);

            unsigned countToCheck = 0;
            unsigned backwardUncheckAmount = 0;

            if (matchDirection == Forward)
                countToCheck = minimumSize - parenthesesInputCountAlreadyChecked;
            else {
                // Backward case
                unsigned minAlreadyChecked = std::min(disjunction->m_minimumSize, parenthesesInputCountAlreadyChecked);
                if (minimumSize > minAlreadyChecked) {
                    countToCheck = minimumSize - minAlreadyChecked;
                    auto checkedInput = countToCheck + currentCountAlreadyChecked;
                    if (checkedInput.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    haveCheckedInput(checkedInput);

                    if (minimumSize > disjunction->m_minimumSize)
                        backwardUncheckAmount = countToCheck;
                    else
                        backwardUncheckAmount = minimumSize;
                }
            }

            if (countToCheck) {
                if (matchDirection == Forward)
                    checkInput(countToCheck);

                currentCountAlreadyChecked += countToCheck;
                if (currentCountAlreadyChecked.hasOverflowed())
                    return ErrorCode::OffsetTooLarge;
            }

            auto termCount = alternative->m_terms.size();
            for (unsigned i = 0; i < termCount; ++i) {
                unsigned termIndex = matchDirection == Forward ? i : termCount - 1 - i;
                auto& term = alternative->m_terms[termIndex];

                switch (term.type) {
                case PatternTerm::Type::AssertionBOL: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    assertionBOL(currentInputPosition, term.m_currentFlags);
                    break;
                }

                case PatternTerm::Type::AssertionEOL: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    assertionEOL(currentInputPosition, term.m_currentFlags);
                    break;
                }

                case PatternTerm::Type::AssertionWordBoundary: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    assertionWordBoundary(term.invert(), matchDirection, currentInputPosition, term.m_currentFlags);
                    break;
                }

                case PatternTerm::Type::PatternCharacter: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    atomPatternCharacter(term.patternCharacter, matchDirection, currentInputPosition, term.frameLocation, term.quantityMaxCount, term.quantityType, term.m_currentFlags);
                    m_bodyDisjunction->terms.last().m_possessive = term.m_possessive;
                    break;
                }

                case PatternTerm::Type::CharacterClass: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    atomCharacterClass(term.characterClass, term.invert(), matchDirection, currentInputPosition, term.frameLocation, term.quantityMaxCount, term.quantityType, term.m_currentFlags);
                    m_bodyDisjunction->terms.last().m_possessive = term.m_possessive;
                    break;
                }

                case PatternTerm::Type::BackReference: {
                    auto currentInputPosition = currentCountAlreadyChecked - term.inputPosition;
                    if (currentInputPosition.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    atomBackReference(term.backReferenceSubpatternId, matchDirection, currentInputPosition, term.frameLocation, term.quantityMaxCount, term.quantityType, term.m_currentFlags);
                    break;
                }

                case PatternTerm::Type::ForwardReference:
                    break;

                case PatternTerm::Type::ParenthesesSubpattern: {
                    if (matchDirection == Forward) {
                        if (auto guarded = guardedClassFor(term)) {
                            auto inputPosition = currentCountAlreadyChecked - term.inputPosition;
                            if (inputPosition.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                            // Fixed groups store their end offset; zero-minimum
                            // variable groups store their start offset.
                            if (term.quantityType == QuantifierType::FixedCount)
                                inputPosition += 1;
                            if (inputPosition.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                            ByteTerm guardedTerm(guarded->characterClass, guarded->invert, inputPosition, guarded->flags);
                            guardedTerm.type = ByteTerm::Type::CharacterClassWithNegativeAssertion;
                            guardedTerm.atom.secondaryCharacterClass = guarded->guard;
                            guardedTerm.atom.quantityMinCount = term.quantityType == QuantifierType::FixedCount ? 1 : 0;
                            guardedTerm.atom.quantityMaxCount = term.quantityMaxCount;
                            guardedTerm.atom.quantityType = term.quantityType;
                            guardedTerm.frameLocation = term.frameLocation;
                            m_bodyDisjunction->terms.append(guardedTerm);
                            break;
                        }
                    }
                    unsigned disjunctionAlreadyCheckedCount = 0;
                    if (term.quantityMaxCount == 1 && !term.parentheses.isCopy) {
                        unsigned alternativeFrameLocation = term.frameLocation;
                        // For QuantifierType::FixedCount we pre-check the minimum size; for greedy/non-greedy we reserve a slot in the frame.
                        if (term.quantityType == QuantifierType::FixedCount)
                            disjunctionAlreadyCheckedCount = term.parentheses.disjunction->m_minimumSize;
                        else
                            alternativeFrameLocation += YarrStackSpaceForBackTrackInfoParenthesesOnce;
                        auto delegateEndInputOffset = currentCountAlreadyChecked - term.inputPosition;
                        if (delegateEndInputOffset.hasOverflowed())
                            return ErrorCode::OffsetTooLarge;
                        atomParenthesesOnceBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, alternativeFrameLocation);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, disjunctionAlreadyCheckedCount, matchDirection))
                            return error;
                        atomParenthesesOnceEnd(delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType);
                    } else if (term.parentheses.isTerminal) {
                        auto delegateEndInputOffset = currentCountAlreadyChecked - term.inputPosition;
                        if (delegateEndInputOffset.hasOverflowed())
                            return ErrorCode::OffsetTooLarge;
                        atomParenthesesTerminalBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, term.frameLocation + YarrStackSpaceForBackTrackInfoParenthesesTerminal);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, disjunctionAlreadyCheckedCount, matchDirection))
                            return error;
                        atomParenthesesTerminalEnd(delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType);
                    } else {
                        auto delegateEndInputOffset = currentCountAlreadyChecked - term.inputPosition;
                        if (delegateEndInputOffset.hasOverflowed())
                            return ErrorCode::OffsetTooLarge;
                        atomParenthesesSubpatternBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, 0);
                        unsigned inputOffset = 0;
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, inputOffset, matchDirection))
                            return error;
                        atomParenthesesSubpatternEnd(term.parentheses.lastSubpatternId, delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType, term.parentheses.disjunction->m_callFrameSize);
                    }
                    break;
                }

                case PatternTerm::Type::ParentheticalAssertion: {
                    unsigned alternativeFrameLocation = term.frameLocation + YarrStackSpaceForBackTrackInfoParentheticalAssertion;
                    auto positiveInputOffset = currentCountAlreadyChecked - term.inputPosition;
                    if (positiveInputOffset.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    if (term.matchDirection() == Forward) {
                        unsigned uncheckAmount = 0;
                        if (positiveInputOffset > term.parentheses.disjunction->m_minimumSize) {
                            uncheckAmount = positiveInputOffset - term.parentheses.disjunction->m_minimumSize;
                            uncheckInput(uncheckAmount);
                            currentCountAlreadyChecked -= uncheckAmount;
                            if (currentCountAlreadyChecked.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                        }

                        atomParentheticalAssertionBegin(term.parentheses.subpatternId, term.invert(), term.matchDirection(), term.frameLocation, alternativeFrameLocation);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, positiveInputOffset - uncheckAmount, term.matchDirection()))
                            return error;
                        atomParentheticalAssertionEnd(term.parentheses.lastSubpatternId, term.frameLocation, term.quantityMaxCount, term.quantityType);
                        if (uncheckAmount) {
                            checkInput(uncheckAmount);
                            currentCountAlreadyChecked += uncheckAmount;
                            if (currentCountAlreadyChecked.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                        }
                    } else { // Backward
                        CheckedUint32 checkedCountForLookbehind = currentCountAlreadyChecked;
                        ASSERT(checkedCountForLookbehind >= term.inputPosition);
                        checkedCountForLookbehind -= term.inputPosition;
                        if (checkedCountForLookbehind.hasOverflowed())
                            return ErrorCode::OffsetTooLarge;
                        auto minimumSize = term.parentheses.disjunction->m_minimumSize;
                        if (minimumSize) {
                            checkedCountForLookbehind += minimumSize;
                            if (checkedCountForLookbehind.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                            if (checkedCountForLookbehind > currentCountAlreadyChecked && !term.invert()) {
                                // Do a quick check for what is required for the lookbehind.
                                // An inverted lookbehind can "match" without processing any input.
                                haveCheckedInput(checkedCountForLookbehind);
                            }
                        }
                        atomParentheticalAssertionBegin(term.parentheses.subpatternId, term.invert(), term.matchDirection(), term.frameLocation, alternativeFrameLocation);

                        if (auto error = emitDisjunction(term.parentheses.disjunction, checkedCountForLookbehind, positiveInputOffset + minimumSize, term.matchDirection()))
                            return error;
                        atomParentheticalAssertionEnd(term.parentheses.lastSubpatternId, term.frameLocation, term.quantityMaxCount, term.quantityType);
                    }
                    break;
                }

                case PatternTerm::Type::DotStarEnclosure:
                    assertionDotStarEnclosure(term.anchors.bolAnchor, term.anchors.eolAnchor);
                    break;
                }
            }

            if (matchDirection == Backward && backwardUncheckAmount)
                uncheckInput(backwardUncheckAmount);
        }
        return std::nullopt;
    }

private:
    inline bool isSafeToRecurse() { return m_stackCheck.isSafeToRecurse(); }

    YarrPattern& m_pattern;
    std::unique_ptr<ByteDisjunction> m_bodyDisjunction;
    StackCheck m_stackCheck;
    unsigned m_currentAlternativeIndex { 0 };
    Vector<ParenthesesStackEntry> m_parenthesesStack;
    Vector<std::unique_ptr<ByteDisjunction>> m_allParenthesesInfo;
    OptionSet<Flags> m_currentFlags;
};

std::unique_ptr<BytecodePattern> byteCompile(YarrPattern& pattern, BumpPointerAllocator* allocator, ErrorCode& errorCode)
{
    return ByteCompiler(pattern).compile(allocator, errorCode);
}

unsigned interpret(BytecodePattern* bytecode, const LChar* input, unsigned length, unsigned start, unsigned* output)
{
#if defined(ESCARGOT_SMALL_CONFIG)
    // Only the Interpreter<UChar> instantiation is kept under SMALL_CONFIG to avoid
    // duplicating this large template for LChar as well; widen the 8-bit input instead.
    Vector<UChar> widenedInput;
    widenedInput.grow(length);
    for (unsigned i = 0; i < length; i++)
        widenedInput[i] = input[i];
    return Interpreter<UChar>(bytecode, output, widenedInput.data(), length, start).interpret();
#else
    return Interpreter<LChar>(bytecode, output, input, length, start).interpret();
#endif
}

unsigned interpret(BytecodePattern* bytecode, const UChar* input, unsigned length, unsigned start, unsigned* output)
{
    return Interpreter<UChar>(bytecode, output, input, length, start).interpret();
}

// These should be the same for both UChar & LChar.
static_assert(sizeof(BackTrackInfoPatternCharacter) == (YarrStackSpaceForBackTrackInfoPatternCharacter * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoCharacterClass) == (YarrStackSpaceForBackTrackInfoCharacterClass * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoBackReference) == (YarrStackSpaceForBackTrackInfoBackReference * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoAlternative) == (YarrStackSpaceForBackTrackInfoAlternative * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoParentheticalAssertion) == (YarrStackSpaceForBackTrackInfoParentheticalAssertion * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoParenthesesOnce) == (YarrStackSpaceForBackTrackInfoParenthesesOnce * sizeof(uintptr_t)), "");
static_assert(sizeof(BackTrackInfoParenthesesTerminal) == (YarrStackSpaceForBackTrackInfoParenthesesTerminal * sizeof(uintptr_t)), "");
static_assert(sizeof(Interpreter<UChar>::BackTrackInfoParentheses) <= (YarrStackSpaceForBackTrackInfoParentheses * sizeof(uintptr_t)), "");


} }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
