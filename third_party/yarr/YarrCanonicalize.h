/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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

#pragma once

#include <stdint.h>
#include "WTFBridge.h"
#if defined(ENABLE_ICU)
#include "SimpleCaseFoldingTable.h"
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Yarr {

// This set of data provides information for each code point as to the set of code points
// that it should match under the ES6 case insensitive RegExp matching rules, specified in 21.2.2.8.2.
// The static range tables and character sets that used to back this have been replaced with
// runtime ICU lookups: uset_closeOver(USET_CASE_INSENSITIVE) yields the case equivalence
// class of a code point, and u_foldCase()/u_toupper() implement Canonicalize().

enum class CanonicalMode { UCS2, Unicode };

// A case equivalence class. The largest class in Unicode holds 4 code points
// (e.g. 'k', 'K', U+212A KELVIN SIGN), so this is generously sized.
struct CanonicalEquivalents {
    static constexpr unsigned maxSize = 8;
    char32_t chars[maxSize];
    unsigned size;

    const char32_t* begin() const { return chars; }
    const char32_t* end() const { return chars + size; }
};

#if defined(ENABLE_ICU)

// USET_CASE_INSENSITIVE from icu4c's uset.h. That header is not visible in builds
// that bind ICU at runtime (only the USet type is forward declared), so the value
// is spelled out here.
static const int32_t usetCaseInsensitive = 2;

// Returns the code point that ch shares a simple case folding with when its full
// case folding is a multi code point string (U+1FD3 / U+0390, U+FB05 / U+FB06,
// ...), or ch itself. ECMAScript canonicalizes with the simple mapping, so the
// two have to match each other, but the runtime ICU does not always report the
// link -- see tools/code_generators/generateSimpleCaseFoldingTable.py.
inline char32_t simpleCaseFoldingCounterpart(char32_t ch)
{
    unsigned low = 0;
    unsigned high = simpleCaseFoldingPairCount;
    while (low < high) {
        unsigned middle = low + (high - low) / 2;
        if (simpleCaseFoldingPairs[middle].code < ch)
            low = middle + 1;
        else if (simpleCaseFoldingPairs[middle].code > ch)
            high = middle;
        else
            return simpleCaseFoldingPairs[middle].counterpart;
    }
    return ch;
}

// Returns the canonical form of ch under the given mode.
// - Unicode mode: simple case folding (scf), per ES2015+ 21.2.2.8.2 step 1.
// - UCS2 mode: String.prototype.toUpperCase, but a non-ASCII code point may not
//   canonicalize to an ASCII one (21.2.2.8.2 step 3).
inline char32_t canonicalize(char32_t ch, CanonicalMode canonicalMode = CanonicalMode::UCS2)
{
    if (canonicalMode == CanonicalMode::Unicode)
        return u_foldCase(ch, 0);

    // If the character is already uppercase, toUpperCase returns it unchanged.
    if (u_isupper(ch))
        return ch;
    char32_t upper = u_toupper(ch);
    if (ch >= 128 && upper < 128)
        return ch;
    return upper;
}

// Returns every code point that ch must match under case insensitive comparison,
// including ch itself. ICU's case closure is a superset of the ECMAScript rules for
// UCS2 mode (e.g. it links 'k' with U+212A, which toUpperCase does not), so the
// closure is filtered by Canonicalize() equality.
inline CanonicalEquivalents canonicalEquivalents(char32_t ch, CanonicalMode canonicalMode)
{
    CanonicalEquivalents result;
    result.size = 0;
    result.chars[result.size++] = ch;

    auto append = [&result](char32_t cp) {
        if (result.size >= CanonicalEquivalents::maxSize)
            return;
        for (unsigned i = 0; i < result.size; i++) {
            if (result.chars[i] == cp)
                return;
        }
        result.chars[result.size++] = cp;
    };

    USet* set = uset_openEmpty();
    if (set) {
        uset_add(set, ch);
        uset_closeOver(set, usetCaseInsensitive);

        char32_t canonical = canonicalize(ch, canonicalMode);
        int32_t itemCount = uset_getItemCount(set);
        for (int32_t i = 0; i < itemCount; i++) {
            UChar32 start = 0;
            UChar32 end = 0;
            UErrorCode status = U_ZERO_ERROR;
            // A non-zero length means the item is a multi code point string (full case
            // folding, e.g. "ss" for U+00DF); those never take part in single character
            // matching, so skip them.
            if (uset_getItem(set, i, &start, &end, nullptr, 0, &status) != 0 || U_FAILURE(status))
                continue;
            for (UChar32 cp = start; cp <= end; cp++) {
                if (canonicalize(cp, canonicalMode) == canonical)
                    append(cp);
            }
        }
        uset_close(set);
    }

    // A code point whose full case folding is a multi code point string is not
    // reliably linked to its simple folding counterpart by uset_closeOver(), so
    // add that from the generated table instead.
    if (canonicalMode == CanonicalMode::Unicode)
        append(simpleCaseFoldingCounterpart(ch));

    return result;
}

// Calls addChar() for every code point outside [lo, hi] that must also match when
// [lo, hi] is used case insensitively.
template <typename AddCharFunc>
inline void forEachCaseEquivalentOutsideRange(char32_t lo, char32_t hi, CanonicalMode canonicalMode, AddCharFunc addChar)
{
    USet* set = uset_openEmpty();
    if (set) {
        uset_addRange(set, lo, hi);
        uset_closeOver(set, usetCaseInsensitive);

        int32_t itemCount = uset_getItemCount(set);
        for (int32_t i = 0; i < itemCount; i++) {
            UChar32 start = 0;
            UChar32 end = 0;
            UErrorCode status = U_ZERO_ERROR;
            if (uset_getItem(set, i, &start, &end, nullptr, 0, &status) != 0 || U_FAILURE(status))
                continue;
            for (UChar32 cp = start; cp <= end; cp++) {
                char32_t ch = static_cast<char32_t>(cp);
                if (ch >= lo && ch <= hi)
                    continue;
                // The closure is a superset of the ECMAScript equivalence in UCS2 mode,
                // so only keep code points that really are equivalent to a member of
                // [lo, hi]. The equivalence class of ch is small, so this is cheap.
                bool equivalent = false;
                for (char32_t candidate : canonicalEquivalents(ch, canonicalMode)) {
                    if (candidate >= lo && candidate <= hi) {
                        equivalent = true;
                        break;
                    }
                }
                if (equivalent)
                    addChar(ch);
            }
        }
        uset_close(set);
    }

    // Same gap as in canonicalEquivalents(): the closure may not link a code point
    // to its simple folding counterpart, so walk the generated table as well.
    if (canonicalMode == CanonicalMode::Unicode) {
        for (unsigned i = 0; i < simpleCaseFoldingPairCount; i++) {
            char32_t code = simpleCaseFoldingPairs[i].code;
            char32_t counterpart = simpleCaseFoldingPairs[i].counterpart;
            if (code >= lo && code <= hi && (counterpart < lo || counterpart > hi))
                addChar(counterpart);
        }
    }
}

// Returns true if no other code point can match this value.
inline bool isCanonicallyUnique(char32_t ch, CanonicalMode canonicalMode = CanonicalMode::UCS2)
{
    return canonicalEquivalents(ch, canonicalMode).size == 1;
}

// Returns true if values are equal, under the canonicalization rules.
inline bool areCanonicallyEquivalent(char32_t a, char32_t b, CanonicalMode canonicalMode = CanonicalMode::UCS2)
{
    return canonicalize(a, canonicalMode) == canonicalize(b, canonicalMode);
}

#else
// noicu fallback: only ASCII case folding
inline char32_t canonicalize(char32_t ch, CanonicalMode = CanonicalMode::UCS2)
{
    if (ch >= 'a' && ch <= 'z')
        return ch - 32;
    return ch;
}

inline CanonicalEquivalents canonicalEquivalents(char32_t ch, CanonicalMode)
{
    CanonicalEquivalents result;
    result.size = 0;
    result.chars[result.size++] = ch;
    if (ch >= 'A' && ch <= 'Z')
        result.chars[result.size++] = ch + 32;
    else if (ch >= 'a' && ch <= 'z')
        result.chars[result.size++] = ch - 32;
    return result;
}

template <typename AddCharFunc>
inline void forEachCaseEquivalentOutsideRange(char32_t lo, char32_t hi, CanonicalMode canonicalMode, AddCharFunc addChar)
{
    for (char32_t ch = lo; ch <= hi && ch <= 'z'; ch++) {
        for (char32_t candidate : canonicalEquivalents(ch, canonicalMode)) {
            if (candidate < lo || candidate > hi)
                addChar(candidate);
        }
    }
}

inline bool isCanonicallyUnique(char32_t ch, CanonicalMode = CanonicalMode::UCS2)
{
    return !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));
}

inline bool areCanonicallyEquivalent(char32_t a, char32_t b, CanonicalMode = CanonicalMode::UCS2)
{
    return canonicalize(a) == canonicalize(b);
}
#endif

} } // JSC::Yarr

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
