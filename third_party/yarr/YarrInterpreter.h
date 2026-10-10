/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "YarrErrorCode.h"
#include "YarrFlags.h"
#include "YarrPattern.h"

namespace WTF {
class BumpPointerAllocator;
}
using WTF::BumpPointerAllocator;

namespace JSC { namespace Yarr {

class ByteDisjunction;

struct ByteTerm {
    union {
        struct {
            union {
                char32_t patternCharacter;
                struct {
                    char32_t lo;
                    char32_t hi;
                } casedCharacter;
                const CharacterClass* characterClass;
                struct {
                    unsigned subpatternId;
                    unsigned duplicateNamedGroupId;
                } parenIds;
                struct {
                    unsigned firstSubpatternId;
                    unsigned lastSubpatternId;
                } assertionIds;
            };
            union {
                ByteDisjunction* parenthesesDisjunction;
                // Non-null for guarded character-class instructions.
                const CharacterClass* secondaryCharacterClass;
                unsigned parenthesesWidth;
            };
            QuantifierType quantityType;
            unsigned quantityMinCount;
            unsigned quantityMaxCount;
        } atom;
        struct {
            int next;
            int end;
            bool onceThrough;
        } alternative;
        struct {
            bool m_bol : 1;
            bool m_eol : 1;
        } anchors;
        struct {
            uint8_t characters[8];
            uint8_t masks[8];
            unsigned length;
        } literal;
        struct {
            uint16_t characters[4];
            uint16_t masks[4];
            unsigned length;
        } literal16;
        unsigned checkInputCount;
    };
    unsigned frameLocation { 0 };
    enum class Type : uint8_t {
        BodyAlternativeBegin,
        BodyAlternativeDisjunction,
        BodyAlternativeEnd,
        AlternativeBegin,
        AlternativeDisjunction,
        AlternativeEnd,
        SubpatternBegin,
        SubpatternEnd,
        AssertionBOL,
        AssertionEOL,
        AssertionWordBoundary,
        // Character Types
        PatternCharacterOnce,
        PatternCharacterFixed,
        PatternCharacterGreedy,
        PatternCharacterNonGreedy,
        PatternLiteral,
        PatternLiteral16,
        // The input check count occupies the otherwise unused frameLocation.
        CheckInputLiteral,
        CheckInputLiteral16,
        CheckInputCharacter,
        CheckInputCharacter16,
        // Cased Characeter Types
        PatternCasedCharacterOnce,
        PatternCasedCharacterFixed,
        PatternCasedCharacterGreedy,
        PatternCasedCharacterNonGreedy,
        CharacterClass,
        CapturedCharacterClass,
        CheckInputCapturedCharacterClass,
        BackReference,
        ParenthesesSubpattern,
        ParenthesesSubpatternOnceBegin,
        ParenthesesSubpatternOnceEnd,
        ParenthesesSubpatternTerminalBegin,
        ParenthesesSubpatternTerminalEnd,
        ParentheticalAssertionBegin,
        ParentheticalAssertionEnd,
        CheckInput,
        UncheckInput,
        HaveCheckedInput,
        DotStarEnclosure,
        CharacterClassWithNegativeAssertion,
    };
    Type type;
    OptionSet<Flags> m_flags;
    bool m_capture : 1;
    bool m_invert : 1;
    MatchDirection m_matchDirection : 1;
    bool m_possessive { false };
    unsigned inputPosition { 0 };

    ByteTerm(char32_t ch, unsigned inputPos, unsigned frameLocation, Checked<unsigned> quantityCount, QuantifierType quantityType, OptionSet<Flags> flags)
        : frameLocation(frameLocation)
        , m_flags(flags)
        , m_capture(false)
        , m_invert(false)
        , m_matchDirection(Forward)
        , inputPosition(inputPos)
    {
        atom.patternCharacter = ch;
        atom.quantityType = quantityType;
        atom.quantityMinCount = quantityCount;
        atom.quantityMaxCount = quantityCount;

        switch (quantityType) {
        case QuantifierType::FixedCount:
            type = (quantityCount == 1) ? ByteTerm::Type::PatternCharacterOnce : ByteTerm::Type::PatternCharacterFixed;
            break;
        case QuantifierType::Greedy:
            atom.quantityMinCount = 0;
            type = ByteTerm::Type::PatternCharacterGreedy;
            break;
        case QuantifierType::NonGreedy:
            atom.quantityMinCount = 0;
            type = ByteTerm::Type::PatternCharacterNonGreedy;
            break;
        }
    }

    ByteTerm(char32_t lo, char32_t hi, unsigned inputPos, unsigned frameLocation, Checked<unsigned> quantityCount, QuantifierType quantityType, OptionSet<Flags> flags)
        : frameLocation(frameLocation)
        , m_flags(flags)
        , m_capture(false)
        , m_invert(false)
        , m_matchDirection(Forward)
        , inputPosition(inputPos)
    {
        switch (quantityType) {
        case QuantifierType::FixedCount:
            type = (quantityCount == 1) ? ByteTerm::Type::PatternCasedCharacterOnce : ByteTerm::Type::PatternCasedCharacterFixed;
            atom.quantityMinCount = quantityCount;
            break;
        case QuantifierType::Greedy:
            type = ByteTerm::Type::PatternCasedCharacterGreedy;
            atom.quantityMinCount = 0;
            break;
        case QuantifierType::NonGreedy:
            type = ByteTerm::Type::PatternCasedCharacterNonGreedy;
            atom.quantityMinCount = 0;
            break;
        }

        atom.casedCharacter.lo = lo;
        atom.casedCharacter.hi = hi;
        atom.quantityType = quantityType;
        atom.quantityMaxCount = quantityCount;
    }

    ByteTerm(const CharacterClass* characterClass, bool invert, unsigned inputPos, OptionSet<Flags> flags)
        : type(ByteTerm::Type::CharacterClass)
        , m_flags(flags)
        , m_capture(false)
        , m_invert(invert)
        , m_matchDirection(Forward)
        , inputPosition(inputPos)
    {
        atom.characterClass = characterClass;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, ByteDisjunction* parenthesesInfo, bool capture, unsigned inputPos, OptionSet<Flags> flags)
        : type(type)
        , m_flags(flags)
        , m_capture(capture)
        , m_invert(false)
        , m_matchDirection(Forward)
        , inputPosition(inputPos)
    {
        atom.parenIds.subpatternId = subpatternId;
        atom.parenIds.duplicateNamedGroupId = 0;
        atom.parenthesesDisjunction = parenthesesInfo;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }
    
    ByteTerm(Type type, OptionSet<Flags> flags, bool invert = false)
        : type(type)
        , m_flags(flags)
        , m_capture(false)
        , m_invert(invert)
        , m_matchDirection(Forward)
    {
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, bool capture, bool invert, unsigned inputPos, OptionSet<Flags> flags)
        : type(type)
        , m_flags(flags)
        , m_capture(capture)
        , m_invert(invert)
        , m_matchDirection(Forward)
        , inputPosition(inputPos)
    {
        atom.parenIds.subpatternId = subpatternId;
        atom.parenIds.duplicateNamedGroupId = 0;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, bool capture, bool invert, MatchDirection matchDirection, unsigned inputPos, OptionSet<Flags> flags)
        : type(type)
        , m_flags(flags)
        , m_capture(capture)
        , m_invert(invert)
        , m_matchDirection(matchDirection)
        , inputPosition(inputPos)
    {
        atom.parenIds.subpatternId = subpatternId;
        atom.parenIds.duplicateNamedGroupId = 0;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    static ByteTerm BOL(unsigned inputPos, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AssertionBOL, flags);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm CheckInput(Checked<unsigned> count, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::CheckInput, flags);
        term.checkInputCount = count;
        return term;
    }

    static ByteTerm UncheckInput(Checked<unsigned> count, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::UncheckInput, flags);
        term.checkInputCount = count;
        return term;
    }
    
    static ByteTerm HaveCheckedInput(Checked<unsigned> count, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::HaveCheckedInput, flags);
        term.checkInputCount = count;
        return term;
    }

    static ByteTerm EOL(unsigned inputPos, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AssertionEOL, flags);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm WordBoundary(bool invert, MatchDirection matchDirection, unsigned inputPos, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AssertionWordBoundary, flags, invert);
        term.m_matchDirection = matchDirection;
        term.inputPosition = inputPos;
        return term;
    }
    
    static ByteTerm BackReference(unsigned subpatternId, MatchDirection matchDirection, unsigned inputPos, OptionSet<Flags> flags)
    {
        return ByteTerm(Type::BackReference, subpatternId, false, false, matchDirection, inputPos, flags);
    }

    static ByteTerm BodyAlternativeBegin(bool onceThrough, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::BodyAlternativeBegin, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = onceThrough;
        return term;
    }

    static ByteTerm BodyAlternativeDisjunction(bool onceThrough, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::BodyAlternativeDisjunction, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = onceThrough;
        return term;
    }

    static ByteTerm BodyAlternativeEnd(OptionSet<Flags> flags)
    {
        ByteTerm term(Type::BodyAlternativeEnd, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeBegin(OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AlternativeBegin, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeDisjunction(OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AlternativeDisjunction, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeEnd(OptionSet<Flags> flags)
    {
        ByteTerm term(Type::AlternativeEnd, flags);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm SubpatternBegin(OptionSet<Flags> flags)
    {
        return ByteTerm(Type::SubpatternBegin, flags);
    }

    static ByteTerm SubpatternEnd(OptionSet<Flags> flags)
    {
        return ByteTerm(Type::SubpatternEnd, flags);
    }

    static ByteTerm ParentheticalAssertionBegin(unsigned firstSubpatternId, bool invert, MatchDirection matchDirection, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::ParentheticalAssertionBegin, flags);
        term.atom.assertionIds.firstSubpatternId = firstSubpatternId;
        term.m_invert = invert;
        term.m_matchDirection = matchDirection;
        return term;
    }

    static ByteTerm ParentheticalAssertionEnd(unsigned firstSubpatternId, unsigned lastSubpatternId, bool invert, MatchDirection matchDirection, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::ParentheticalAssertionEnd, flags);
        term.atom.assertionIds.firstSubpatternId = firstSubpatternId;
        term.atom.assertionIds.lastSubpatternId = lastSubpatternId;
        term.m_invert = invert;
        term.m_matchDirection = matchDirection;
        return term;
    }

    static ByteTerm DotStarEnclosure(bool bolAnchor, bool eolAnchor, OptionSet<Flags> flags)
    {
        ByteTerm term(Type::DotStarEnclosure, flags);
        term.anchors.m_bol = bolAnchor;
        term.anchors.m_eol = eolAnchor;
        return term;
    }

    bool isCharacterType()
    {
        return type >= Type::PatternCharacterOnce && type <= Type::PatternCharacterNonGreedy;
    }

    bool isCasedCharacterType()
    {
        return type >= Type::PatternCasedCharacterOnce && type <= Type::PatternCasedCharacterNonGreedy;
    }

    bool isCharacterClass()
    {
        return type == Type::CharacterClass || type == Type::CapturedCharacterClass
            || type == Type::CheckInputCapturedCharacterClass
            || type == Type::CharacterClassWithNegativeAssertion;
    }

    bool containsAnyCaptures()
    {
        ASSERT(this->type == Type::ParentheticalAssertionBegin
            || this->type == Type::ParentheticalAssertionEnd);
        return lastSubpatternId() >= firstSubpatternId();
    }

    unsigned subpatternId()
    {
        return atom.parenIds.subpatternId;
    }

    unsigned duplicateNamedGroupId()
    {
        return atom.parenIds.duplicateNamedGroupId;
    }

    unsigned firstSubpatternId()
    {
        return atom.assertionIds.firstSubpatternId;
    }

    unsigned lastSubpatternId()
    {
        return atom.assertionIds.lastSubpatternId;
    }
    bool invert()
    {
        return m_invert;
    }

    MatchDirection matchDirection()
    {
        return m_matchDirection;
    }

    bool capture()
    {
        return m_capture;
    }

    bool ignoreCase()
    {
        return m_flags.contains(Flags::IgnoreCase);
    }

    bool multiline()
    {
        return m_flags.contains(Flags::Multiline);
    }

    bool dotAll()
    {
        return m_flags.contains(Flags::DotAll);
    }
};

static_assert(sizeof(ByteTerm::literal) <= sizeof(ByteTerm::atom), "Literal terms must fit the existing bytecode payload");
static_assert(sizeof(ByteTerm::literal16) <= sizeof(ByteTerm::atom), "UTF-16 literal terms must fit the existing bytecode payload");

class ByteDisjunction {
    WTF_MAKE_TZONE_ALLOCATED(ByteDisjunction);
public:
    ByteDisjunction(unsigned numSubpatterns, unsigned frameSize)
        : m_numSubpatterns(numSubpatterns)
        , m_frameSize(frameSize)
    {
    }

    size_t estimatedSizeInBytes() const { return terms.capacity() * sizeof(ByteTerm); }

    Vector<ByteTerm> terms;
    unsigned m_numSubpatterns;
    unsigned m_frameSize;
};

// A conservative superset of the characters a match of the body disjunction can
// begin with. The interpreter uses it to skip start offsets that cannot begin a
// match at all, instead of retrying every alternative of the body there - see
// Interpreter::advanceToPossibleStart(). It is computed once per pattern by
// StartCharFilterBuilder (YarrInterpreter.cpp); anything not fully understood
// there leaves the filter invalid, which keeps matching unchanged.
struct StartCharFilter {
    // Characters 0x00..0xFF that can begin a match. The whole Latin1 range has to
    // be covered, not just ASCII, because for 8-bit subject strings this bitmap
    // alone decides.
    // Stored as 32-bit words rather than uint64_t: BytecodePattern is allocated
    // via the GC (GC_finalized_atomic_malloc), which on 32-bit targets only
    // guarantees word (4-byte) alignment - see ALIGNMENT in gcconfig.h. An
    // 8-byte member here would let the compiler emit an aligned 64-bit store
    // for the in-class initializer below, which SIGBUSes on ARM when the GC
    // allocation isn't 8-byte aligned.
    uint32_t latin1Bitmap[8] { 0, 0, 0, 0, 0, 0, 0, 0 };
    // Whether a match can begin with a character above 0xFF. For non-unicode
    // patterns a supplementary character is two terms, so this covers its lead
    // surrogate.
    bool mayStartAboveLatin1 { false };
    bool valid { false };
    bool fixedPosition { false };
    uint16_t singleLatin1Character { 0x100 };
};

struct FixedPrefixSearch {
    static constexpr unsigned maxLength = 4;
    StartCharFilter positions[maxLength];
    uint8_t shifts[256] { };
    unsigned length { 0 };
    uint32_t packedMask { 0 };
    uint32_t packedValue { 0 };
    uint16_t singleLatin1Character { 0x100 };
    uint8_t singleCharacterOffset { 0 };
    // Capture-free, case-sensitive literal alternatives bypass bytecode.
    Vector<Vector<UChar>> atoms;
    // A required literal may occur after a variable-width prefix.
    Vector<LChar> requiredAtom;
    unsigned longestAtomLength { 0 };
    bool anchoredStart { false };
    bool anchoredEnd { false };
};

struct BytecodePattern : public gc {
    WTF_MAKE_TZONE_ALLOCATED(BytecodePattern);
public:
    enum class SpecificPattern : uint8_t {
        None,
        LeadingSpacesStar,
        LeadingSpacesPlus,
        TrailingSpacesStar,
        TrailingSpacesPlus,
        Newlines,
    };

    static void bytecodePatternClear(void* obj, void* cd)
    {
        BytecodePattern* self = reinterpret_cast<BytecodePattern*>(obj);
        self->~BytecodePattern();
    }

    void* operator new(size_t size)
    {
        constexpr static GC_finalizer_closure data = { bytecodePatternClear, nullptr };
        return GC_finalized_atomic_malloc(size, &data);
    }

    BytecodePattern(std::unique_ptr<ByteDisjunction> body, Vector<std::unique_ptr<ByteDisjunction>>& parenthesesInfoToAdopt, YarrPattern& pattern, BumpPointerAllocator* allocator, unsigned offsetVectorBaseForNamedCaptures, unsigned offsetsSize)
        : m_body(WTFMove(body))
        , m_flags(pattern.m_flags)
        , m_allocator(allocator)
        , m_offsetVectorBaseForNamedCaptures(offsetVectorBaseForNamedCaptures)
        , m_offsetsSize(offsetsSize)
        , m_duplicateNamedGroupForSubpatternId(pattern.m_duplicateNamedGroupForSubpatternId)
    {
        m_body->terms.shrinkToFit();

        newlineCharacterClass = pattern.newlineCharacterClass();
        if (eitherUnicode())
            ignoreCaseWordcharCharacterClass = pattern.wordUnicodeIgnoreCaseCharCharacterClass();
        else
            ignoreCaseWordcharCharacterClass = pattern.wordcharCharacterClass();
        wordcharCharacterClass = pattern.wordcharCharacterClass();

        m_allParenthesesInfo.swap(parenthesesInfoToAdopt);
        m_allParenthesesInfo.shrinkToFit();

        m_userCharacterClasses.swap(pattern.m_userCharacterClasses);
        m_userCharacterClasses.shrinkToFit();

        for (auto& characterClass : m_userCharacterClasses)
            characterClass->initializeLatin1Bitmap();

        m_numDuplicateNamedCaptureGroups = pattern.m_numDuplicateNamedCaptureGroups;
        m_endAnchoredFixedSize = pattern.m_endAnchoredFixedSize;
    }

    ~BytecodePattern()
    {
        if (m_fixedPrefixSearch)
            delete m_fixedPrefixSearch.value();
    }

    size_t estimatedSizeInBytes() const { return m_body->estimatedSizeInBytes(); }

    bool hasDuplicateNamedCaptureGroups() const { return !!m_numDuplicateNamedCaptureGroups; }
    bool hasEndAnchoredFixedSize() const { return m_endAnchoredFixedSize != YarrPattern::endAnchoredFixedSizeNotSet; }

    unsigned offsetForDuplicateNamedGroupId(unsigned duplicateNamedGroupId)
    {
        ASSERT(duplicateNamedGroupId);
        return m_offsetVectorBaseForNamedCaptures + duplicateNamedGroupId - 1;
    }

    CompileMode compileMode() const
    {
        if (unicode())
            return CompileMode::Unicode;

        if (unicodeSets())
            return CompileMode::UnicodeSets;

        return CompileMode::Legacy;
    }

    bool ignoreCase() const { return m_flags.contains(Flags::IgnoreCase); }
    bool multiline() const { return m_flags.contains(Flags::Multiline); }
    bool hasIndices() const { return m_flags.contains(Flags::HasIndices); }
    bool sticky() const { return m_flags.contains(Flags::Sticky); }
    bool unicode() const { return m_flags.contains(Flags::Unicode); }
    bool unicodeSets() const { return m_flags.contains(Flags::UnicodeSets); }
    bool eitherUnicode() const { return unicode() || unicodeSets(); }
    bool dotAll() const { return m_flags.contains(Flags::DotAll); }

    std::unique_ptr<ByteDisjunction> m_body;
    OptionSet<Flags> m_flags;
    SpecificPattern m_specificPattern { SpecificPattern::None };
    // Each BytecodePattern is associated with a RegExp, each RegExp is associated
    // with a VM.  Cache a pointer to our VM's m_regExpAllocator.
    BumpPointerAllocator* m_allocator;

    unsigned m_numDuplicateNamedCaptureGroups;
    unsigned m_endAnchoredFixedSize { YarrPattern::endAnchoredFixedSizeNotSet };
    unsigned m_offsetVectorBaseForNamedCaptures;
    unsigned m_offsetsSize;
    Vector<unsigned> m_duplicateNamedGroupForSubpatternId;

    const CharacterClass* newlineCharacterClass;
    const CharacterClass* wordcharCharacterClass;
    const CharacterClass* ignoreCaseWordcharCharacterClass;
    StartCharFilter m_startCharFilter;
    ::Escargot::Optional<FixedPrefixSearch*> m_fixedPrefixSearch;

private:
    Vector<std::unique_ptr<ByteDisjunction>> m_allParenthesesInfo;
    Vector<std::unique_ptr<CharacterClass>> m_userCharacterClasses;
};

JS_EXPORT_PRIVATE std::unique_ptr<BytecodePattern> byteCompile(YarrPattern&, BumpPointerAllocator*, ErrorCode&);
unsigned interpret(BytecodePattern*, const LChar* input, unsigned length, unsigned start, unsigned* output);
unsigned interpret(BytecodePattern*, const UChar* input, unsigned length, unsigned start, unsigned* output);

} } // namespace JSC::Yarr
