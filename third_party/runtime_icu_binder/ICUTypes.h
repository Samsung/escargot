// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1996-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/
/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the MIT License (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License
 * at
 *
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef __ICUTypes__
#define __ICUTypes__

#include <inttypes.h>

typedef char16_t UChar;
typedef int32_t UChar32;
typedef int8_t UBool;
typedef double UDate;

#define U_CALLCONV

/**
 * Is this code point a Unicode noncharacter?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_UNICODE_NONCHAR(c) \
    ((c) >= 0xfdd0 && ((c) <= 0xfdef || ((c)&0xfffe) == 0xfffe) && (c) <= 0x10ffff)

/**
 * Is c a Unicode code point value (0..U+10ffff)
 * that can be assigned a character?
 *
 * Code points that are not characters include:
 * - single surrogate code points (U+d800..U+dfff, 2048 code points)
 * - the last two code points on each plane (U+__fffe and U+__ffff, 34 code points)
 * - U+fdd0..U+fdef (new with Unicode 3.1, 32 code points)
 * - the highest Unicode code point value is U+10ffff
 *
 * This means that all code points below U+d800 are character code points,
 * and that boundary is tested first for performance.
 *
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_UNICODE_CHAR(c) \
    ((uint32_t)(c) < 0xd800 || (0xdfff < (c) && (c) <= 0x10ffff && !U_IS_UNICODE_NONCHAR(c)))

/**
 * Is this code point a BMP code point (U+0000..U+ffff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.8
 */
#define U_IS_BMP(c) ((uint32_t)(c) <= 0xffff)

/**
 * Is this code point a supplementary code point (U+10000..U+10ffff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.8
 */
#define U_IS_SUPPLEMENTARY(c) ((uint32_t)((c)-0x10000) <= 0xfffff)

/**
 * Is this code point a lead surrogate (U+d800..U+dbff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_LEAD(c) (((c)&0xfffffc00) == 0xd800)

/**
 * Is this code point a trail surrogate (U+dc00..U+dfff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_TRAIL(c) (((c)&0xfffffc00) == 0xdc00)

/**
 * Is this code point a surrogate (U+d800..U+dfff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_SURROGATE(c) (((c)&0xfffff800) == 0xd800)

/**
 * Assuming c is a surrogate code point (U_IS_SURROGATE(c)),
 * is it a lead surrogate?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_SURROGATE_LEAD(c) (((c)&0x400) == 0)

/**
 * Assuming c is a surrogate code point (U_IS_SURROGATE(c)),
 * is it a trail surrogate?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 4.2
 */
#define U_IS_SURROGATE_TRAIL(c) (((c)&0x400) != 0)

/**
 * Does this code unit alone encode a code point (BMP, not a surrogate)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SINGLE(c) !U_IS_SURROGATE(c)

/**
 * Is this code unit a lead surrogate (U+d800..U+dbff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_LEAD(c) (((c)&0xfffffc00) == 0xd800)

/**
 * Is this code unit a trail surrogate (U+dc00..U+dfff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_TRAIL(c) (((c)&0xfffffc00) == 0xdc00)

/**
 * Is this code unit a surrogate (U+d800..U+dfff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SURROGATE(c) U_IS_SURROGATE(c)

/**
 * Assuming c is a surrogate code point (U16_IS_SURROGATE(c)),
 * is it a lead surrogate?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SURROGATE_LEAD(c) (((c)&0x400) == 0)

/**
 * Assuming c is a surrogate code point (U16_IS_SURROGATE(c)),
 * is it a trail surrogate?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 4.2
 */
#define U16_IS_SURROGATE_TRAIL(c) (((c)&0x400) != 0)

/**
 * Helper constant for U16_GET_SUPPLEMENTARY.
 * @internal
 */
#define U16_SURROGATE_OFFSET ((0xd800 << 10UL) + 0xdc00 - 0x10000)

/**
 * Get a supplementary code point value (U+10000..U+10ffff)
 * from its lead and trail surrogates.
 * The result is undefined if the input values are not
 * lead and trail surrogates.
 *
 * @param lead lead surrogate (U+d800..U+dbff)
 * @param trail trail surrogate (U+dc00..U+dfff)
 * @return supplementary code point (U+10000..U+10ffff)
 * @stable ICU 2.4
 */
#define U16_GET_SUPPLEMENTARY(lead, trail) \
    (((UChar32)(lead) << 10UL) + (UChar32)(trail)-U16_SURROGATE_OFFSET)


/**
 * Get the lead surrogate (0xd800..0xdbff) for a
 * supplementary code point (0x10000..0x10ffff).
 * @param supplementary 32-bit code point (U+10000..U+10ffff)
 * @return lead surrogate (U+d800..U+dbff) for supplementary
 * @stable ICU 2.4
 */
#define U16_LEAD(supplementary) (UChar)(((supplementary) >> 10) + 0xd7c0)

/**
 * Get the trail surrogate (0xdc00..0xdfff) for a
 * supplementary code point (0x10000..0x10ffff).
 * @param supplementary 32-bit code point (U+10000..U+10ffff)
 * @return trail surrogate (U+dc00..U+dfff) for supplementary
 * @stable ICU 2.4
 */
#define U16_TRAIL(supplementary) (UChar)(((supplementary)&0x3ff) | 0xdc00)

/**
 * How many 16-bit code units are used to encode this Unicode code point? (1 or 2)
 * The result is not defined if c is not a Unicode code point (U+0000..U+10ffff).
 * @param c 32-bit code point
 * @return 1 or 2
 * @stable ICU 2.4
 */
#define U16_LENGTH(c) ((uint32_t)(c) <= 0xffff ? 1 : 2)

/**
 * The maximum number of 16-bit code units per Unicode code point (U+0000..U+10ffff).
 * @return 2
 * @stable ICU 2.4
 */
#define U16_MAX_LENGTH 2

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * The offset may point to either the lead or trail surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the adjacent matching surrogate as well.
 * The result is undefined if the offset points to a single, unpaired surrogate.
 * Iteration through a string is more efficient with U16_NEXT_UNSAFE or U16_NEXT.
 *
 * @param s const UChar * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U16_GET
 * @stable ICU 2.4
 */
#define U16_GET_UNSAFE(s, i, c)                                 \
    {                                                           \
        (c) = (s)[i];                                           \
        if (U16_IS_SURROGATE(c)) {                              \
            if (U16_IS_SURROGATE_LEAD(c)) {                     \
                (c) = U16_GET_SUPPLEMENTARY((c), (s)[(i) + 1]); \
            } else {                                            \
                (c) = U16_GET_SUPPLEMENTARY((s)[(i)-1], (c));   \
            }                                                   \
        }                                                       \
    }

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The offset may point to either the lead or trail surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the adjacent matching surrogate as well.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * If the offset points to a single, unpaired surrogate, then
 * c is set to that unpaired surrogate.
 * Iteration through a string is more efficient with U16_NEXT_UNSAFE or U16_NEXT.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<=i<length
 * @param length string length
 * @param c output UChar32 variable
 * @see U16_GET_UNSAFE
 * @stable ICU 2.4
 */
#define U16_GET(s, start, i, length, c)                                         \
    {                                                                           \
        (c) = (s)[i];                                                           \
        if (U16_IS_SURROGATE(c)) {                                              \
            uint16_t __c2;                                                      \
            if (U16_IS_SURROGATE_LEAD(c)) {                                     \
                if ((i) + 1 != (length) && U16_IS_TRAIL(__c2 = (s)[(i) + 1])) { \
                    (c) = U16_GET_SUPPLEMENTARY((c), __c2);                     \
                }                                                               \
            } else {                                                            \
                if ((i) > (start) && U16_IS_LEAD(__c2 = (s)[(i)-1])) {          \
                    (c) = U16_GET_SUPPLEMENTARY(__c2, (c));                     \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The offset may point to either the lead or trail surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the adjacent matching surrogate as well.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * If the offset points to a single, unpaired surrogate, then
 * c is set to U+FFFD.
 * Iteration through a string is more efficient with U16_NEXT_UNSAFE or U16_NEXT_OR_FFFD.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<=i<length
 * @param length string length
 * @param c output UChar32 variable
 * @see U16_GET_UNSAFE
 * @draft ICU 60
 */
#define U16_GET_OR_FFFD(s, start, i, length, c)                                 \
    {                                                                           \
        (c) = (s)[i];                                                           \
        if (U16_IS_SURROGATE(c)) {                                              \
            uint16_t __c2;                                                      \
            if (U16_IS_SURROGATE_LEAD(c)) {                                     \
                if ((i) + 1 != (length) && U16_IS_TRAIL(__c2 = (s)[(i) + 1])) { \
                    (c) = U16_GET_SUPPLEMENTARY((c), __c2);                     \
                } else {                                                        \
                    (c) = 0xfffd;                                               \
                }                                                               \
            } else {                                                            \
                if ((i) > (start) && U16_IS_LEAD(__c2 = (s)[(i)-1])) {          \
                    (c) = U16_GET_SUPPLEMENTARY(__c2, (c));                     \
                } else {                                                        \
                    (c) = 0xfffd;                                               \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * The offset may point to the lead surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the following trail surrogate as well.
 * If the offset points to a trail surrogate, then that itself
 * will be returned as the code point.
 * The result is undefined if the offset points to a single, unpaired lead surrogate.
 *
 * @param s const UChar * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U16_NEXT
 * @stable ICU 2.4
 */
#define U16_NEXT_UNSAFE(s, i, c)                          \
    {                                                     \
        (c) = (s)[(i)++];                                 \
        if (U16_IS_LEAD(c)) {                             \
            (c) = U16_GET_SUPPLEMENTARY((c), (s)[(i)++]); \
        }                                                 \
    }

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * The offset may point to the lead surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the following trail surrogate as well.
 * If the offset points to a trail surrogate or
 * to a single, unpaired lead surrogate, then c is set to that unpaired surrogate.
 *
 * @param s const UChar * string
 * @param i string offset, must be i<length
 * @param length string length
 * @param c output UChar32 variable
 * @see U16_NEXT_UNSAFE
 * @stable ICU 2.4
 */
#define U16_NEXT(s, i, length, c)                                   \
    {                                                               \
        (c) = (s)[(i)++];                                           \
        if (U16_IS_LEAD(c)) {                                       \
            uint16_t __c2;                                          \
            if ((i) != (length) && U16_IS_TRAIL(__c2 = (s)[(i)])) { \
                ++(i);                                              \
                (c) = U16_GET_SUPPLEMENTARY((c), __c2);             \
            }                                                       \
        }                                                           \
    }

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * The offset may point to the lead surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the following trail surrogate as well.
 * If the offset points to a trail surrogate or
 * to a single, unpaired lead surrogate, then c is set to U+FFFD.
 *
 * @param s const UChar * string
 * @param i string offset, must be i<length
 * @param length string length
 * @param c output UChar32 variable
 * @see U16_NEXT_UNSAFE
 * @draft ICU 60
 */
#define U16_NEXT_OR_FFFD(s, i, length, c)                                                       \
    {                                                                                           \
        (c) = (s)[(i)++];                                                                       \
        if (U16_IS_SURROGATE(c)) {                                                              \
            uint16_t __c2;                                                                      \
            if (U16_IS_SURROGATE_LEAD(c) && (i) != (length) && U16_IS_TRAIL(__c2 = (s)[(i)])) { \
                ++(i);                                                                          \
                (c) = U16_GET_SUPPLEMENTARY((c), __c2);                                         \
            } else {                                                                            \
                (c) = 0xfffd;                                                                   \
            }                                                                                   \
        }                                                                                       \
    }

/**
 * Append a code point to a string, overwriting 1 or 2 code units.
 * The offset points to the current end of the string contents
 * and is advanced (post-increment).
 * "Unsafe" macro, assumes a valid code point and sufficient space in the string.
 * Otherwise, the result is undefined.
 *
 * @param s const UChar * string buffer
 * @param i string offset
 * @param c code point to append
 * @see U16_APPEND
 * @stable ICU 2.4
 */
#define U16_APPEND_UNSAFE(s, i, c)                         \
    {                                                      \
        if ((uint32_t)(c) <= 0xffff) {                     \
            (s)[(i)++] = (uint16_t)(c);                    \
        } else {                                           \
            (s)[(i)++] = (uint16_t)(((c) >> 10) + 0xd7c0); \
            (s)[(i)++] = (uint16_t)(((c)&0x3ff) | 0xdc00); \
        }                                                  \
    }

/**
 * Append a code point to a string, overwriting 1 or 2 code units.
 * The offset points to the current end of the string contents
 * and is advanced (post-increment).
 * "Safe" macro, checks for a valid code point.
 * If a surrogate pair is written, checks for sufficient space in the string.
 * If the code point is not valid or a trail surrogate does not fit,
 * then isError is set to TRUE.
 *
 * @param s const UChar * string buffer
 * @param i string offset, must be i<capacity
 * @param capacity size of the string buffer
 * @param c code point to append
 * @param isError output UBool set to TRUE if an error occurs, otherwise not modified
 * @see U16_APPEND_UNSAFE
 * @stable ICU 2.4
 */
#define U16_APPEND(s, i, capacity, c, isError)                          \
    {                                                                   \
        if ((uint32_t)(c) <= 0xffff) {                                  \
            (s)[(i)++] = (uint16_t)(c);                                 \
        } else if ((uint32_t)(c) <= 0x10ffff && (i) + 1 < (capacity)) { \
            (s)[(i)++] = (uint16_t)(((c) >> 10) + 0xd7c0);              \
            (s)[(i)++] = (uint16_t)(((c)&0x3ff) | 0xdc00);              \
        } else /* c>0x10ffff or not enough space */ {                   \
            (isError) = TRUE;                                           \
        }                                                               \
    }

/**
 * Advance the string offset from one code point boundary to the next.
 * (Post-incrementing iteration.)
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @see U16_FWD_1
 * @stable ICU 2.4
 */
#define U16_FWD_1_UNSAFE(s, i)         \
    {                                  \
        if (U16_IS_LEAD((s)[(i)++])) { \
            ++(i);                     \
        }                              \
    }

/**
 * Advance the string offset from one code point boundary to the next.
 * (Post-incrementing iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const UChar * string
 * @param i string offset, must be i<length
 * @param length string length
 * @see U16_FWD_1_UNSAFE
 * @stable ICU 2.4
 */
#define U16_FWD_1(s, i, length)                                                   \
    {                                                                             \
        if (U16_IS_LEAD((s)[(i)++]) && (i) != (length) && U16_IS_TRAIL((s)[i])) { \
            ++(i);                                                                \
        }                                                                         \
    }

/**
 * Advance the string offset from one code point boundary to the n-th next one,
 * i.e., move forward by n code points.
 * (Post-incrementing iteration.)
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @param n number of code points to skip
 * @see U16_FWD_N
 * @stable ICU 2.4
 */
#define U16_FWD_N_UNSAFE(s, i, n)   \
    {                               \
        int32_t __N = (n);          \
        while (__N > 0) {           \
            U16_FWD_1_UNSAFE(s, i); \
            --__N;                  \
        }                           \
    }

/**
 * Advance the string offset from one code point boundary to the n-th next one,
 * i.e., move forward by n code points.
 * (Post-incrementing iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const UChar * string
 * @param i int32_t string offset, must be i<length
 * @param length int32_t string length
 * @param n number of code points to skip
 * @see U16_FWD_N_UNSAFE
 * @stable ICU 2.4
 */
#define U16_FWD_N(s, i, length, n)                                             \
    {                                                                          \
        int32_t __N = (n);                                                     \
        while (__N > 0 && ((i) < (length) || ((length) < 0 && (s)[i] != 0))) { \
            U16_FWD_1(s, i, length);                                           \
            --__N;                                                             \
        }                                                                      \
    }

/**
 * Adjust a random-access offset to a code point boundary
 * at the start of a code point.
 * If the offset points to the trail surrogate of a surrogate pair,
 * then the offset is decremented.
 * Otherwise, it is not modified.
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @see U16_SET_CP_START
 * @stable ICU 2.4
 */
#define U16_SET_CP_START_UNSAFE(s, i) \
    {                                 \
        if (U16_IS_TRAIL((s)[i])) {   \
            --(i);                    \
        }                             \
    }

/**
 * Adjust a random-access offset to a code point boundary
 * at the start of a code point.
 * If the offset points to the trail surrogate of a surrogate pair,
 * then the offset is decremented.
 * Otherwise, it is not modified.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<=i
 * @see U16_SET_CP_START_UNSAFE
 * @stable ICU 2.4
 */
#define U16_SET_CP_START(s, start, i)                                           \
    {                                                                           \
        if (U16_IS_TRAIL((s)[i]) && (i) > (start) && U16_IS_LEAD((s)[(i)-1])) { \
            --(i);                                                              \
        }                                                                       \
    }

/* definitions with backward iteration -------------------------------------- */

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a trail surrogate unit
 * for a supplementary code point, then the macro will read
 * the preceding lead surrogate as well.
 * If the offset is behind a lead surrogate, then that itself
 * will be returned as the code point.
 * The result is undefined if the offset is behind a single, unpaired trail surrogate.
 *
 * @param s const UChar * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U16_PREV
 * @stable ICU 2.4
 */
#define U16_PREV_UNSAFE(s, i, c)                          \
    {                                                     \
        (c) = (s)[--(i)];                                 \
        if (U16_IS_TRAIL(c)) {                            \
            (c) = U16_GET_SUPPLEMENTARY((s)[--(i)], (c)); \
        }                                                 \
    }

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a trail surrogate unit
 * for a supplementary code point, then the macro will read
 * the preceding lead surrogate as well.
 * If the offset is behind a lead surrogate or behind a single, unpaired
 * trail surrogate, then c is set to that unpaired surrogate.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<i
 * @param c output UChar32 variable
 * @see U16_PREV_UNSAFE
 * @stable ICU 2.4
 */
#define U16_PREV(s, start, i, c)                                   \
    {                                                              \
        (c) = (s)[--(i)];                                          \
        if (U16_IS_TRAIL(c)) {                                     \
            uint16_t __c2;                                         \
            if ((i) > (start) && U16_IS_LEAD(__c2 = (s)[(i)-1])) { \
                --(i);                                             \
                (c) = U16_GET_SUPPLEMENTARY(__c2, (c));            \
            }                                                      \
        }                                                          \
    }

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a trail surrogate unit
 * for a supplementary code point, then the macro will read
 * the preceding lead surrogate as well.
 * If the offset is behind a lead surrogate or behind a single, unpaired
 * trail surrogate, then c is set to U+FFFD.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<i
 * @param c output UChar32 variable
 * @see U16_PREV_UNSAFE
 * @draft ICU 60
 */
#define U16_PREV_OR_FFFD(s, start, i, c)                                                        \
    {                                                                                           \
        (c) = (s)[--(i)];                                                                       \
        if (U16_IS_SURROGATE(c)) {                                                              \
            uint16_t __c2;                                                                      \
            if (U16_IS_SURROGATE_TRAIL(c) && (i) > (start) && U16_IS_LEAD(__c2 = (s)[(i)-1])) { \
                --(i);                                                                          \
                (c) = U16_GET_SUPPLEMENTARY(__c2, (c));                                         \
            } else {                                                                            \
                (c) = 0xfffd;                                                                   \
            }                                                                                   \
        }                                                                                       \
    }

/**
 * Move the string offset from one code point boundary to the previous one.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @see U16_BACK_1
 * @stable ICU 2.4
 */
#define U16_BACK_1_UNSAFE(s, i)         \
    {                                   \
        if (U16_IS_TRAIL((s)[--(i)])) { \
            --(i);                      \
        }                               \
    }

/**
 * Move the string offset from one code point boundary to the previous one.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<i
 * @see U16_BACK_1_UNSAFE
 * @stable ICU 2.4
 */
#define U16_BACK_1(s, start, i)                                                     \
    {                                                                               \
        if (U16_IS_TRAIL((s)[--(i)]) && (i) > (start) && U16_IS_LEAD((s)[(i)-1])) { \
            --(i);                                                                  \
        }                                                                           \
    }

/**
 * Move the string offset from one code point boundary to the n-th one before it,
 * i.e., move backward by n code points.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @param n number of code points to skip
 * @see U16_BACK_N
 * @stable ICU 2.4
 */
#define U16_BACK_N_UNSAFE(s, i, n)   \
    {                                \
        int32_t __N = (n);           \
        while (__N > 0) {            \
            U16_BACK_1_UNSAFE(s, i); \
            --__N;                   \
        }                            \
    }

/**
 * Move the string offset from one code point boundary to the n-th one before it,
 * i.e., move backward by n code points.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * @param s const UChar * string
 * @param start start of string
 * @param i string offset, must be start<i
 * @param n number of code points to skip
 * @see U16_BACK_N_UNSAFE
 * @stable ICU 2.4
 */
#define U16_BACK_N(s, start, i, n)         \
    {                                      \
        int32_t __N = (n);                 \
        while (__N > 0 && (i) > (start)) { \
            U16_BACK_1(s, start, i);       \
            --__N;                         \
        }                                  \
    }

/**
 * Adjust a random-access offset to a code point boundary after a code point.
 * If the offset is behind the lead surrogate of a surrogate pair,
 * then the offset is incremented.
 * Otherwise, it is not modified.
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-16.
 *
 * @param s const UChar * string
 * @param i string offset
 * @see U16_SET_CP_LIMIT
 * @stable ICU 2.4
 */
#define U16_SET_CP_LIMIT_UNSAFE(s, i)  \
    {                                  \
        if (U16_IS_LEAD((s)[(i)-1])) { \
            ++(i);                     \
        }                              \
    }

/**
 * Adjust a random-access offset to a code point boundary after a code point.
 * If the offset is behind the lead surrogate of a surrogate pair,
 * then the offset is incremented.
 * Otherwise, it is not modified.
 * The input offset may be the same as the string length.
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const UChar * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, start<=i<=length
 * @param length int32_t string length
 * @see U16_SET_CP_LIMIT_UNSAFE
 * @stable ICU 2.4
 */
#define U16_SET_CP_LIMIT(s, start, i, length)                                                                       \
    {                                                                                                               \
        if ((start) < (i) && ((i) < (length) || (length) < 0) && U16_IS_LEAD((s)[(i)-1]) && U16_IS_TRAIL((s)[i])) { \
            ++(i);                                                                                                  \
        }                                                                                                           \
    }


#define UCHAR_MAX_VALUE 0x10ffff

typedef enum UErrorCode {
    /* The ordering of U_ERROR_INFO_START Vs U_USING_FALLBACK_WARNING looks weird
     * and is that way because VC++ debugger displays first encountered constant,
     * which is not the what the code is used for
     */

    U_USING_FALLBACK_WARNING = -128, /**< A resource bundle lookup returned a fallback result (not an error) */

    U_ERROR_WARNING_START = -128, /**< Start of information results (semantically successful) */

    U_USING_DEFAULT_WARNING = -127, /**< A resource bundle lookup returned a result from the root locale (not an error) */

    U_SAFECLONE_ALLOCATED_WARNING = -126, /**< A SafeClone operation required allocating memory (informational only) */

    U_STATE_OLD_WARNING = -125, /**< ICU has to use compatibility layer to construct the service. Expect performance/memory usage degradation. Consider upgrading */

    U_STRING_NOT_TERMINATED_WARNING = -124, /**< An output string could not be NUL-terminated because output length==destCapacity. */

    U_SORT_KEY_TOO_SHORT_WARNING = -123, /**< Number of levels requested in getBound is higher than the number of levels in the sort key */

    U_AMBIGUOUS_ALIAS_WARNING = -122, /**< This converter alias can go to different converter implementations */

    U_DIFFERENT_UCA_VERSION = -121, /**< ucol_open encountered a mismatch between UCA version and collator image version, so the collator was constructed from rules. No impact to further function */

    U_PLUGIN_CHANGED_LEVEL_WARNING = -120, /**< A plugin caused a level change. May not be an error, but later plugins may not load. */

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UErrorCode warning value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_ERROR_WARNING_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    U_ZERO_ERROR = 0, /**< No error, no warning. */

    U_ILLEGAL_ARGUMENT_ERROR = 1, /**< Start of codes indicating failure */
    U_MISSING_RESOURCE_ERROR = 2, /**< The requested resource cannot be found */
    U_INVALID_FORMAT_ERROR = 3, /**< Data format is not what is expected */
    U_FILE_ACCESS_ERROR = 4, /**< The requested file cannot be found */
    U_INTERNAL_PROGRAM_ERROR = 5, /**< Indicates a bug in the library code */
    U_MESSAGE_PARSE_ERROR = 6, /**< Unable to parse a message (message format) */
    U_MEMORY_ALLOCATION_ERROR = 7, /**< Memory allocation error */
    U_INDEX_OUTOFBOUNDS_ERROR = 8, /**< Trying to access the index that is out of bounds */
    U_PARSE_ERROR = 9, /**< Equivalent to Java ParseException */
    U_INVALID_CHAR_FOUND = 10, /**< Character conversion: Unmappable input sequence. In other APIs: Invalid character. */
    U_TRUNCATED_CHAR_FOUND = 11, /**< Character conversion: Incomplete input sequence. */
    U_ILLEGAL_CHAR_FOUND = 12, /**< Character conversion: Illegal input sequence/combination of input units. */
    U_INVALID_TABLE_FORMAT = 13, /**< Conversion table file found, but corrupted */
    U_INVALID_TABLE_FILE = 14, /**< Conversion table file not found */
    U_BUFFER_OVERFLOW_ERROR = 15, /**< A result would not fit in the supplied buffer */
    U_UNSUPPORTED_ERROR = 16, /**< Requested operation not supported in current context */
    U_RESOURCE_TYPE_MISMATCH = 17, /**< an operation is requested over a resource that does not support it */
    U_ILLEGAL_ESCAPE_SEQUENCE = 18, /**< ISO-2022 illlegal escape sequence */
    U_UNSUPPORTED_ESCAPE_SEQUENCE = 19, /**< ISO-2022 unsupported escape sequence */
    U_NO_SPACE_AVAILABLE = 20, /**< No space available for in-buffer expansion for Arabic shaping */
    U_CE_NOT_FOUND_ERROR = 21, /**< Currently used only while setting variable top, but can be used generally */
    U_PRIMARY_TOO_LONG_ERROR = 22, /**< User tried to set variable top to a primary that is longer than two bytes */
    U_STATE_TOO_OLD_ERROR = 23, /**< ICU cannot construct a service from this state, as it is no longer supported */
    U_TOO_MANY_ALIASES_ERROR = 24, /**< There are too many aliases in the path to the requested resource.
                                             It is very possible that a circular alias definition has occured */
    U_ENUM_OUT_OF_SYNC_ERROR = 25, /**< UEnumeration out of sync with underlying collection */
    U_INVARIANT_CONVERSION_ERROR = 26, /**< Unable to convert a UChar* string to char* with the invariant converter. */
    U_INVALID_STATE_ERROR = 27, /**< Requested operation can not be completed with ICU in its current state */
    U_COLLATOR_VERSION_MISMATCH = 28, /**< Collator version is not compatible with the base version */
    U_USELESS_COLLATOR_ERROR = 29, /**< Collator is options only and no base is specified */
    U_NO_WRITE_PERMISSION = 30, /**< Attempt to modify read-only or constant data. */

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest standard error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_STANDARD_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    /*
     * Error codes in the range 0x10000 0x10100 are reserved for Transliterator.
     */
    U_BAD_VARIABLE_DEFINITION = 0x10000, /**< Missing '$' or duplicate variable name */
    U_PARSE_ERROR_START = 0x10000, /**< Start of Transliterator errors */
    U_MALFORMED_RULE, /**< Elements of a rule are misplaced */
    U_MALFORMED_SET, /**< A UnicodeSet pattern is invalid*/
    U_MALFORMED_SYMBOL_REFERENCE, /**< UNUSED as of ICU 2.4 */
    U_MALFORMED_UNICODE_ESCAPE, /**< A Unicode escape pattern is invalid*/
    U_MALFORMED_VARIABLE_DEFINITION, /**< A variable definition is invalid */
    U_MALFORMED_VARIABLE_REFERENCE, /**< A variable reference is invalid */
    U_MISMATCHED_SEGMENT_DELIMITERS, /**< UNUSED as of ICU 2.4 */
    U_MISPLACED_ANCHOR_START, /**< A start anchor appears at an illegal position */
    U_MISPLACED_CURSOR_OFFSET, /**< A cursor offset occurs at an illegal position */
    U_MISPLACED_QUANTIFIER, /**< A quantifier appears after a segment close delimiter */
    U_MISSING_OPERATOR, /**< A rule contains no operator */
    U_MISSING_SEGMENT_CLOSE, /**< UNUSED as of ICU 2.4 */
    U_MULTIPLE_ANTE_CONTEXTS, /**< More than one ante context */
    U_MULTIPLE_CURSORS, /**< More than one cursor */
    U_MULTIPLE_POST_CONTEXTS, /**< More than one post context */
    U_TRAILING_BACKSLASH, /**< A dangling backslash */
    U_UNDEFINED_SEGMENT_REFERENCE, /**< A segment reference does not correspond to a defined segment */
    U_UNDEFINED_VARIABLE, /**< A variable reference does not correspond to a defined variable */
    U_UNQUOTED_SPECIAL, /**< A special character was not quoted or escaped */
    U_UNTERMINATED_QUOTE, /**< A closing single quote is missing */
    U_RULE_MASK_ERROR, /**< A rule is hidden by an earlier more general rule */
    U_MISPLACED_COMPOUND_FILTER, /**< A compound filter is in an invalid location */
    U_MULTIPLE_COMPOUND_FILTERS, /**< More than one compound filter */
    U_INVALID_RBT_SYNTAX, /**< A "::id" rule was passed to the RuleBasedTransliterator parser */
    U_INVALID_PROPERTY_PATTERN, /**< UNUSED as of ICU 2.4 */
    U_MALFORMED_PRAGMA, /**< A 'use' pragma is invlalid */
    U_UNCLOSED_SEGMENT, /**< A closing ')' is missing */
    U_ILLEGAL_CHAR_IN_SEGMENT, /**< UNUSED as of ICU 2.4 */
    U_VARIABLE_RANGE_EXHAUSTED, /**< Too many stand-ins generated for the given variable range */
    U_VARIABLE_RANGE_OVERLAP, /**< The variable range overlaps characters used in rules */
    U_ILLEGAL_CHARACTER, /**< A special character is outside its allowed context */
    U_INTERNAL_TRANSLITERATOR_ERROR, /**< Internal transliterator system error */
    U_INVALID_ID, /**< A "::id" rule specifies an unknown transliterator */
    U_INVALID_FUNCTION, /**< A "&fn()" rule specifies an unknown transliterator */
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal Transliterator error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_PARSE_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    /*
     * Error codes in the range 0x10100 0x10200 are reserved for the formatting API.
     */
    U_UNEXPECTED_TOKEN = 0x10100, /**< Syntax error in format pattern */
    U_FMT_PARSE_ERROR_START = 0x10100, /**< Start of format library errors */
    U_MULTIPLE_DECIMAL_SEPARATORS, /**< More than one decimal separator in number pattern */
    U_MULTIPLE_DECIMAL_SEPERATORS = U_MULTIPLE_DECIMAL_SEPARATORS, /**< Typo: kept for backward compatibility. Use U_MULTIPLE_DECIMAL_SEPARATORS */
    U_MULTIPLE_EXPONENTIAL_SYMBOLS, /**< More than one exponent symbol in number pattern */
    U_MALFORMED_EXPONENTIAL_PATTERN, /**< Grouping symbol in exponent pattern */
    U_MULTIPLE_PERCENT_SYMBOLS, /**< More than one percent symbol in number pattern */
    U_MULTIPLE_PERMILL_SYMBOLS, /**< More than one permill symbol in number pattern */
    U_MULTIPLE_PAD_SPECIFIERS, /**< More than one pad symbol in number pattern */
    U_PATTERN_SYNTAX_ERROR, /**< Syntax error in format pattern */
    U_ILLEGAL_PAD_POSITION, /**< Pad symbol misplaced in number pattern */
    U_UNMATCHED_BRACES, /**< Braces do not match in message pattern */
    U_UNSUPPORTED_PROPERTY, /**< UNUSED as of ICU 2.4 */
    U_UNSUPPORTED_ATTRIBUTE, /**< UNUSED as of ICU 2.4 */
    U_ARGUMENT_TYPE_MISMATCH, /**< Argument name and argument index mismatch in MessageFormat functions */
    U_DUPLICATE_KEYWORD, /**< Duplicate keyword in PluralFormat */
    U_UNDEFINED_KEYWORD, /**< Undefined Plural keyword */
    U_DEFAULT_KEYWORD_MISSING, /**< Missing DEFAULT rule in plural rules */
    U_DECIMAL_NUMBER_SYNTAX_ERROR, /**< Decimal number syntax error */
    U_FORMAT_INEXACT_ERROR, /**< Cannot format a number exactly and rounding mode is ROUND_UNNECESSARY @stable ICU 4.8 */
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal formatting API error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_FMT_PARSE_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    /*
     * Error codes in the range 0x10200 0x102ff are reserved for BreakIterator.
     */
    U_BRK_INTERNAL_ERROR = 0x10200, /**< An internal error (bug) was detected.             */
    U_BRK_ERROR_START = 0x10200, /**< Start of codes indicating Break Iterator failures */
    U_BRK_HEX_DIGITS_EXPECTED, /**< Hex digits expected as part of a escaped char in a rule. */
    U_BRK_SEMICOLON_EXPECTED, /**< Missing ';' at the end of a RBBI rule.            */
    U_BRK_RULE_SYNTAX, /**< Syntax error in RBBI rule.                        */
    U_BRK_UNCLOSED_SET, /**< UnicodeSet witing an RBBI rule missing a closing ']'.  */
    U_BRK_ASSIGN_ERROR, /**< Syntax error in RBBI rule assignment statement.   */
    U_BRK_VARIABLE_REDFINITION, /**< RBBI rule $Variable redefined.                    */
    U_BRK_MISMATCHED_PAREN, /**< Mis-matched parentheses in an RBBI rule.          */
    U_BRK_NEW_LINE_IN_QUOTED_STRING, /**< Missing closing quote in an RBBI rule.            */
    U_BRK_UNDEFINED_VARIABLE, /**< Use of an undefined $Variable in an RBBI rule.    */
    U_BRK_INIT_ERROR, /**< Initialization failure.  Probable missing ICU Data. */
    U_BRK_RULE_EMPTY_SET, /**< Rule contains an empty Unicode Set.               */
    U_BRK_UNRECOGNIZED_OPTION, /**< !!option in RBBI rules not recognized.            */
    U_BRK_MALFORMED_RULE_TAG, /**< The {nnn} tag on a rule is mal formed             */
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal BreakIterator error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_BRK_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    /*
     * Error codes in the range 0x10300-0x103ff are reserved for regular expression related errors.
     */
    U_REGEX_INTERNAL_ERROR = 0x10300, /**< An internal error (bug) was detected.              */
    U_REGEX_ERROR_START = 0x10300, /**< Start of codes indicating Regexp failures          */
    U_REGEX_RULE_SYNTAX, /**< Syntax error in regexp pattern.                    */
    U_REGEX_INVALID_STATE, /**< RegexMatcher in invalid state for requested operation */
    U_REGEX_BAD_ESCAPE_SEQUENCE, /**< Unrecognized backslash escape sequence in pattern  */
    U_REGEX_PROPERTY_SYNTAX, /**< Incorrect Unicode property                         */
    U_REGEX_UNIMPLEMENTED, /**< Use of regexp feature that is not yet implemented. */
    U_REGEX_MISMATCHED_PAREN, /**< Incorrectly nested parentheses in regexp pattern.  */
    U_REGEX_NUMBER_TOO_BIG, /**< Decimal number is too large.                       */
    U_REGEX_BAD_INTERVAL, /**< Error in {min,max} interval                        */
    U_REGEX_MAX_LT_MIN, /**< In {min,max}, max is less than min.                */
    U_REGEX_INVALID_BACK_REF, /**< Back-reference to a non-existent capture group.    */
    U_REGEX_INVALID_FLAG, /**< Invalid value for match mode flags.                */
    U_REGEX_LOOK_BEHIND_LIMIT, /**< Look-Behind pattern matches must have a bounded maximum length.    */
    U_REGEX_SET_CONTAINS_STRING, /**< Regexps cannot have UnicodeSets containing strings.*/
#ifndef U_HIDE_DEPRECATED_API
    U_REGEX_OCTAL_TOO_BIG, /**< Octal character constants must be <= 0377. @deprecated ICU 54. This error cannot occur. */
#endif /* U_HIDE_DEPRECATED_API */
    U_REGEX_MISSING_CLOSE_BRACKET = U_REGEX_SET_CONTAINS_STRING + 2, /**< Missing closing bracket on a bracket expression. */
    U_REGEX_INVALID_RANGE, /**< In a character range [x-y], x is greater than y.   */
    U_REGEX_STACK_OVERFLOW, /**< Regular expression backtrack stack overflow.       */
    U_REGEX_TIME_OUT, /**< Maximum allowed match time exceeded                */
    U_REGEX_STOPPED_BY_CALLER, /**< Matching operation aborted by user callback fn.    */
    U_REGEX_PATTERN_TOO_BIG, /**< Pattern exceeds limits on size or complexity. @stable ICU 55 */
    U_REGEX_INVALID_CAPTURE_GROUP_NAME, /**< Invalid capture group name. @stable ICU 55 */
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal regular expression error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_REGEX_ERROR_LIMIT = U_REGEX_STOPPED_BY_CALLER + 3,
#endif // U_HIDE_DEPRECATED_API

    /*
     * Error codes in the range 0x10400-0x104ff are reserved for IDNA related error codes.
     */
    U_IDNA_PROHIBITED_ERROR = 0x10400,
    U_IDNA_ERROR_START = 0x10400,
    U_IDNA_UNASSIGNED_ERROR,
    U_IDNA_CHECK_BIDI_ERROR,
    U_IDNA_STD3_ASCII_RULES_ERROR,
    U_IDNA_ACE_PREFIX_ERROR,
    U_IDNA_VERIFICATION_ERROR,
    U_IDNA_LABEL_TOO_LONG_ERROR,
    U_IDNA_ZERO_LENGTH_LABEL_ERROR,
    U_IDNA_DOMAIN_NAME_TOO_LONG_ERROR,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal IDNA error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_IDNA_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API
    /*
     * Aliases for StringPrep
     */
    U_STRINGPREP_PROHIBITED_ERROR = U_IDNA_PROHIBITED_ERROR,
    U_STRINGPREP_UNASSIGNED_ERROR = U_IDNA_UNASSIGNED_ERROR,
    U_STRINGPREP_CHECK_BIDI_ERROR = U_IDNA_CHECK_BIDI_ERROR,

    /*
     * Error codes in the range 0x10500-0x105ff are reserved for Plugin related error codes.
     */
    U_PLUGIN_ERROR_START = 0x10500, /**< Start of codes indicating plugin failures */
    U_PLUGIN_TOO_HIGH = 0x10500, /**< The plugin's level is too high to be loaded right now. */
    U_PLUGIN_DIDNT_SET_LEVEL, /**< The plugin didn't call uplug_setPlugLevel in response to a QUERY */
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal plug-in error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_PLUGIN_ERROR_LIMIT,
#endif // U_HIDE_DEPRECATED_API

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal error code.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_ERROR_LIMIT = U_PLUGIN_ERROR_LIMIT
#endif // U_HIDE_DEPRECATED_API
} UErrorCode;

static inline UBool U_FAILURE(UErrorCode code) { return (UBool)(code > U_ZERO_ERROR); }
static inline UBool U_SUCCESS(UErrorCode code) { return (UBool)(code <= U_ZERO_ERROR); }
// utext.h
struct UText;

// uparseerr.h
/**
 * \file
 * \brief C API: Parse Error Information
 */
/**
 * The capacity of the context strings in UParseError.
 * @stable ICU 2.0
 */
enum { U_PARSE_CONTEXT_LEN = 16 };

/**
 * A UParseError struct is used to returned detailed information about
 * parsing errors.  It is used by ICU parsing engines that parse long
 * rules, patterns, or programs, where the text being parsed is long
 * enough that more information than a UErrorCode is needed to
 * localize the error.
 *
 * <p>The line, offset, and context fields are optional; parsing
 * engines may choose not to use to use them.
 *
 * <p>The preContext and postContext strings include some part of the
 * context surrounding the error.  If the source text is "let for=7"
 * and "for" is the error (e.g., because it is a reserved word), then
 * some examples of what a parser might produce are the following:
 *
 * <pre>
 * preContext   postContext
 * ""           ""            The parser does not support context
 * "let "       "=7"          Pre- and post-context only
 * "let "       "for=7"       Pre- and post-context and error text
 * ""           "for"         Error text only
 * </pre>
 *
 * <p>Examples of engines which use UParseError (or may use it in the
 * future) are Transliterator, RuleBasedBreakIterator, and
 * RegexPattern.
 *
 * @stable ICU 2.0
 */
typedef struct UParseError {
    /**
     * The line on which the error occured.  If the parser uses this
     * field, it sets it to the line number of the source text line on
     * which the error appears, which will be be a value >= 1.  If the
     * parse does not support line numbers, the value will be <= 0.
     * @stable ICU 2.0
     */
    int32_t line;

    /**
     * The character offset to the error.  If the line field is >= 1,
     * then this is the offset from the start of the line.  Otherwise,
     * this is the offset from the start of the text.  If the parser
     * does not support this field, it will have a value < 0.
     * @stable ICU 2.0
     */
    int32_t offset;

    /**
     * Textual context before the error.  Null-terminated.  The empty
     * string if not supported by parser.
     * @stable ICU 2.0
     */
    UChar preContext[U_PARSE_CONTEXT_LEN];

    /**
     * The error itself and/or textual context after the error.
     * Null-terminated.  The empty string if not supported by parser.
     * @stable ICU 2.0
     */
    UChar postContext[U_PARSE_CONTEXT_LEN];

} UParseError;

// vzone.h
struct VZone;

// udisplaycontext.h

/**
 * \file
 * \brief C API: Display context types (enum values)
 */

/**
 * Display context types, for getting values of a particular setting.
 * Note, the specific numeric values are internal and may change.
 * @stable ICU 51
 */
enum UDisplayContextType {
    /**
     * Type to retrieve the dialect handling setting, e.g.
     * UDISPCTX_STANDARD_NAMES or UDISPCTX_DIALECT_NAMES.
     * @stable ICU 51
     */
    UDISPCTX_TYPE_DIALECT_HANDLING = 0,
    /**
     * Type to retrieve the capitalization context setting, e.g.
     * UDISPCTX_CAPITALIZATION_NONE, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, etc.
     * @stable ICU 51
     */
    UDISPCTX_TYPE_CAPITALIZATION = 1,
    /**
     * Type to retrieve the display length setting, e.g.
     * UDISPCTX_LENGTH_FULL, UDISPCTX_LENGTH_SHORT.
     * @stable ICU 54
     */
    UDISPCTX_TYPE_DISPLAY_LENGTH = 2,
    /**
     * Type to retrieve the substitute handling setting, e.g.
     * UDISPCTX_SUBSTITUTE, UDISPCTX_NO_SUBSTITUTE.
     * @stable ICU 58
     */
    UDISPCTX_TYPE_SUBSTITUTE_HANDLING = 3
};
/**
*  @stable ICU 51
*/
typedef enum UDisplayContextType UDisplayContextType;

/**
 * Display context settings.
 * Note, the specific numeric values are internal and may change.
 * @stable ICU 51
 */
enum UDisplayContext {
    /**
     * ================================
     * DIALECT_HANDLING can be set to one of UDISPCTX_STANDARD_NAMES or
     * UDISPCTX_DIALECT_NAMES. Use UDisplayContextType UDISPCTX_TYPE_DIALECT_HANDLING
     * to get the value.
     */
    /**
     * A possible setting for DIALECT_HANDLING:
     * use standard names when generating a locale name,
     * e.g. en_GB displays as 'English (United Kingdom)'.
     * @stable ICU 51
     */
    UDISPCTX_STANDARD_NAMES = (UDISPCTX_TYPE_DIALECT_HANDLING << 8) + 0,
    /**
     * A possible setting for DIALECT_HANDLING:
     * use dialect names, when generating a locale name,
     * e.g. en_GB displays as 'British English'.
     * @stable ICU 51
     */
    UDISPCTX_DIALECT_NAMES = (UDISPCTX_TYPE_DIALECT_HANDLING << 8) + 1,
    /**
     * ================================
     * CAPITALIZATION can be set to one of UDISPCTX_CAPITALIZATION_NONE,
     * UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU, or
     * UDISPCTX_CAPITALIZATION_FOR_STANDALONE.
     * Use UDisplayContextType UDISPCTX_TYPE_CAPITALIZATION to get the value.
     */
    /**
     * The capitalization context to be used is unknown (this is the default value).
     * @stable ICU 51
     */
    UDISPCTX_CAPITALIZATION_NONE = (UDISPCTX_TYPE_CAPITALIZATION << 8) + 0,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for the middle of a sentence.
     * @stable ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE = (UDISPCTX_TYPE_CAPITALIZATION << 8) + 1,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for the beginning of a sentence.
     * @stable ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE = (UDISPCTX_TYPE_CAPITALIZATION << 8) + 2,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for a user-interface list or menu item.
     * @stable ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU = (UDISPCTX_TYPE_CAPITALIZATION << 8) + 3,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for stand-alone usage such as an
     * isolated name on a calendar page.
     * @stable ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_STANDALONE = (UDISPCTX_TYPE_CAPITALIZATION << 8) + 4,
    /**
     * ================================
     * DISPLAY_LENGTH can be set to one of UDISPCTX_LENGTH_FULL or
     * UDISPCTX_LENGTH_SHORT. Use UDisplayContextType UDISPCTX_TYPE_DISPLAY_LENGTH
     * to get the value.
     */
    /**
     * A possible setting for DISPLAY_LENGTH:
     * use full names when generating a locale name,
     * e.g. "United States" for US.
     * @stable ICU 54
     */
    UDISPCTX_LENGTH_FULL = (UDISPCTX_TYPE_DISPLAY_LENGTH << 8) + 0,
    /**
     * A possible setting for DISPLAY_LENGTH:
     * use short names when generating a locale name,
     * e.g. "U.S." for US.
     * @stable ICU 54
     */
    UDISPCTX_LENGTH_SHORT = (UDISPCTX_TYPE_DISPLAY_LENGTH << 8) + 1,
    /**
     * ================================
     * SUBSTITUTE_HANDLING can be set to one of UDISPCTX_SUBSTITUTE or
     * UDISPCTX_NO_SUBSTITUTE. Use UDisplayContextType UDISPCTX_TYPE_SUBSTITUTE_HANDLING
     * to get the value.
     */
    /**
     * A possible setting for SUBSTITUTE_HANDLING:
     * Returns a fallback value (e.g., the input code) when no data is available.
     * This is the default value.
     * @stable ICU 58
     */
    UDISPCTX_SUBSTITUTE = (UDISPCTX_TYPE_SUBSTITUTE_HANDLING << 8) + 0,
    /**
     * A possible setting for SUBSTITUTE_HANDLING:
     * Returns a null value when no data is available.
     * @stable ICU 58
     */
    UDISPCTX_NO_SUBSTITUTE = (UDISPCTX_TYPE_SUBSTITUTE_HANDLING << 8) + 1

};
/**
*  @stable ICU 51
*/
typedef enum UDisplayContext UDisplayContext;

// udat.h
/** A date formatter.
 *  For usage in C programs.
 *  @stable ICU 2.6
 */
typedef void *UDateFormat;

/** The possible date/time format styles
 *  @stable ICU 2.6
 */
typedef enum UDateFormatStyle {
    /** Full style */
    UDAT_FULL,
    /** Long style */
    UDAT_LONG,
    /** Medium style */
    UDAT_MEDIUM,
    /** Short style */
    UDAT_SHORT,
    /** Default style */
    UDAT_DEFAULT = UDAT_MEDIUM,

    /** Bitfield for relative date */
    UDAT_RELATIVE = (1 << 7),

    UDAT_FULL_RELATIVE = UDAT_FULL | UDAT_RELATIVE,

    UDAT_LONG_RELATIVE = UDAT_LONG | UDAT_RELATIVE,

    UDAT_MEDIUM_RELATIVE = UDAT_MEDIUM | UDAT_RELATIVE,

    UDAT_SHORT_RELATIVE = UDAT_SHORT | UDAT_RELATIVE,


    /** No style */
    UDAT_NONE = -1,

    /**
     * Use the pattern given in the parameter to udat_open
     * @see udat_open
     * @stable ICU 50
     */
    UDAT_PATTERN = -2,

#ifndef U_HIDE_INTERNAL_API
    /** @internal alias to UDAT_PATTERN */
    UDAT_IGNORE = UDAT_PATTERN
#endif /* U_HIDE_INTERNAL_API */
} UDateFormatStyle;

/* Skeletons for dates. */

/**
 * Constant for date skeleton with year.
 * @stable ICU 4.0
 */
#define UDAT_YEAR "y"
/**
 * Constant for date skeleton with quarter.
 * @stable ICU 51
 */
#define UDAT_QUARTER "QQQQ"
/**
 * Constant for date skeleton with abbreviated quarter.
 * @stable ICU 51
 */
#define UDAT_ABBR_QUARTER "QQQ"
/**
 * Constant for date skeleton with year and quarter.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_QUARTER "yQQQQ"
/**
 * Constant for date skeleton with year and abbreviated quarter.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_ABBR_QUARTER "yQQQ"
/**
 * Constant for date skeleton with month.
 * @stable ICU 4.0
 */
#define UDAT_MONTH "MMMM"
/**
 * Constant for date skeleton with abbreviated month.
 * @stable ICU 4.0
 */
#define UDAT_ABBR_MONTH "MMM"
/**
 * Constant for date skeleton with numeric month.
 * @stable ICU 4.0
 */
#define UDAT_NUM_MONTH "M"
/**
 * Constant for date skeleton with year and month.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_MONTH "yMMMM"
/**
 * Constant for date skeleton with year and abbreviated month.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_ABBR_MONTH "yMMM"
/**
 * Constant for date skeleton with year and numeric month.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_NUM_MONTH "yM"
/**
 * Constant for date skeleton with day.
 * @stable ICU 4.0
 */
#define UDAT_DAY "d"
/**
 * Constant for date skeleton with year, month, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_MONTH_DAY "yMMMMd"
/**
 * Constant for date skeleton with year, abbreviated month, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_ABBR_MONTH_DAY "yMMMd"
/**
 * Constant for date skeleton with year, numeric month, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_NUM_MONTH_DAY "yMd"
/**
 * Constant for date skeleton with weekday.
 * @stable ICU 51
 */
#define UDAT_WEEKDAY "EEEE"
/**
 * Constant for date skeleton with abbreviated weekday.
 * @stable ICU 51
 */
#define UDAT_ABBR_WEEKDAY "E"
/**
 * Constant for date skeleton with year, month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_MONTH_WEEKDAY_DAY "yMMMMEEEEd"
/**
 * Constant for date skeleton with year, abbreviated month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_ABBR_MONTH_WEEKDAY_DAY "yMMMEd"
/**
 * Constant for date skeleton with year, numeric month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_YEAR_NUM_MONTH_WEEKDAY_DAY "yMEd"
/**
 * Constant for date skeleton with long month and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_MONTH_DAY "MMMMd"
/**
 * Constant for date skeleton with abbreviated month and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_ABBR_MONTH_DAY "MMMd"
/**
 * Constant for date skeleton with numeric month and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_NUM_MONTH_DAY "Md"
/**
 * Constant for date skeleton with month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_MONTH_WEEKDAY_DAY "MMMMEEEEd"
/**
 * Constant for date skeleton with abbreviated month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_ABBR_MONTH_WEEKDAY_DAY "MMMEd"
/**
 * Constant for date skeleton with numeric month, weekday, and day.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_NUM_MONTH_WEEKDAY_DAY "MEd"

/* Skeletons for times. */

/**
 * Constant for date skeleton with hour, with the locale's preferred hour format (12 or 24).
 * @stable ICU 4.0
 */
#define UDAT_HOUR "j"
/**
 * Constant for date skeleton with hour in 24-hour presentation.
 * @stable ICU 51
 */
#define UDAT_HOUR24 "H"
/**
 * Constant for date skeleton with minute.
 * @stable ICU 51
 */
#define UDAT_MINUTE "m"
/**
 * Constant for date skeleton with hour and minute, with the locale's preferred hour format (12 or 24).
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_HOUR_MINUTE "jm"
/**
 * Constant for date skeleton with hour and minute in 24-hour presentation.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_HOUR24_MINUTE "Hm"
/**
 * Constant for date skeleton with second.
 * @stable ICU 51
 */
#define UDAT_SECOND "s"
/**
 * Constant for date skeleton with hour, minute, and second,
 * with the locale's preferred hour format (12 or 24).
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_HOUR_MINUTE_SECOND "jms"
/**
 * Constant for date skeleton with hour, minute, and second in
 * 24-hour presentation.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_HOUR24_MINUTE_SECOND "Hms"
/**
 * Constant for date skeleton with minute and second.
 * Used in combinations date + time, date + time + zone, or time + zone.
 * @stable ICU 4.0
 */
#define UDAT_MINUTE_SECOND "ms"

/* Skeletons for time zones. */

/**
 * Constant for <i>generic location format</i>, such as Los Angeles Time;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_LOCATION_TZ "VVVV"
/**
 * Constant for <i>generic non-location format</i>, such as Pacific Time;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_GENERIC_TZ "vvvv"
/**
 * Constant for <i>generic non-location format</i>, abbreviated if possible, such as PT;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_ABBR_GENERIC_TZ "v"
/**
 * Constant for <i>specific non-location format</i>, such as Pacific Daylight Time;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_SPECIFIC_TZ "zzzz"
/**
 * Constant for <i>specific non-location format</i>, abbreviated if possible, such as PDT;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_ABBR_SPECIFIC_TZ "z"
/**
 * Constant for <i>localized GMT/UTC format</i>, such as GMT+8:00 or HPG-8:00;
 * used in combinations date + time + zone, or time + zone.
 * @see <a href="http://unicode.org/reports/tr35/#Date_Format_Patterns">LDML Date Format Patterns</a>
 * @see <a href="http://unicode.org/reports/tr35/#Time_Zone_Fallback">LDML Time Zone Fallback</a>
 * @stable ICU 51
 */
#define UDAT_ABBR_UTC_TZ "ZZZZ"

/* deprecated skeleton constants */

#ifndef U_HIDE_DEPRECATED_API
/**
 * Constant for date skeleton with standalone month.
 * @deprecated ICU 50 Use UDAT_MONTH instead.
 */
#define UDAT_STANDALONE_MONTH "LLLL"
/**
 * Constant for date skeleton with standalone abbreviated month.
 * @deprecated ICU 50 Use UDAT_ABBR_MONTH instead.
 */
#define UDAT_ABBR_STANDALONE_MONTH "LLL"

/**
 * Constant for date skeleton with hour, minute, and generic timezone.
 * @deprecated ICU 50 Use instead UDAT_HOUR_MINUTE UDAT_ABBR_GENERIC_TZ or some other timezone presentation.
 */
#define UDAT_HOUR_MINUTE_GENERIC_TZ "jmv"
/**
 * Constant for date skeleton with hour, minute, and timezone.
 * @deprecated ICU 50 Use instead UDAT_HOUR_MINUTE UDAT_ABBR_SPECIFIC_TZ or some other timezone presentation.
 */
#define UDAT_HOUR_MINUTE_TZ "jmz"
/**
 * Constant for date skeleton with hour and generic timezone.
 * @deprecated ICU 50 Use instead UDAT_HOUR UDAT_ABBR_GENERIC_TZ or some other timezone presentation.
 */
#define UDAT_HOUR_GENERIC_TZ "jv"
/**
 * Constant for date skeleton with hour and timezone.
 * @deprecated ICU 50 Use instead UDAT_HOUR UDAT_ABBR_SPECIFIC_TZ or some other timezone presentation.
 */
#define UDAT_HOUR_TZ "jz"
#endif /* U_HIDE_DEPRECATED_API */

/**
 * FieldPosition and UFieldPosition selectors for format fields
 * defined by DateFormat and UDateFormat.
 * @stable ICU 3.0
 */
typedef enum UDateFormatField {
    /**
     * FieldPosition and UFieldPosition selector for 'G' field alignment,
     * corresponding to the UCAL_ERA field.
     * @stable ICU 3.0
     */
    UDAT_ERA_FIELD = 0,

    /**
     * FieldPosition and UFieldPosition selector for 'y' field alignment,
     * corresponding to the UCAL_YEAR field.
     * @stable ICU 3.0
     */
    UDAT_YEAR_FIELD = 1,

    /**
     * FieldPosition and UFieldPosition selector for 'M' field alignment,
     * corresponding to the UCAL_MONTH field.
     * @stable ICU 3.0
     */
    UDAT_MONTH_FIELD = 2,

    /**
     * FieldPosition and UFieldPosition selector for 'd' field alignment,
     * corresponding to the UCAL_DATE field.
     * @stable ICU 3.0
     */
    UDAT_DATE_FIELD = 3,

    /**
     * FieldPosition and UFieldPosition selector for 'k' field alignment,
     * corresponding to the UCAL_HOUR_OF_DAY field.
     * UDAT_HOUR_OF_DAY1_FIELD is used for the one-based 24-hour clock.
     * For example, 23:59 + 01:00 results in 24:59.
     * @stable ICU 3.0
     */
    UDAT_HOUR_OF_DAY1_FIELD = 4,

    /**
     * FieldPosition and UFieldPosition selector for 'H' field alignment,
     * corresponding to the UCAL_HOUR_OF_DAY field.
     * UDAT_HOUR_OF_DAY0_FIELD is used for the zero-based 24-hour clock.
     * For example, 23:59 + 01:00 results in 00:59.
     * @stable ICU 3.0
     */
    UDAT_HOUR_OF_DAY0_FIELD = 5,

    /**
     * FieldPosition and UFieldPosition selector for 'm' field alignment,
     * corresponding to the UCAL_MINUTE field.
     * @stable ICU 3.0
     */
    UDAT_MINUTE_FIELD = 6,

    /**
     * FieldPosition and UFieldPosition selector for 's' field alignment,
     * corresponding to the UCAL_SECOND field.
     * @stable ICU 3.0
     */
    UDAT_SECOND_FIELD = 7,

    /**
     * FieldPosition and UFieldPosition selector for 'S' field alignment,
     * corresponding to the UCAL_MILLISECOND field.
     *
     * Note: Time formats that use 'S' can display a maximum of three
     * significant digits for fractional seconds, corresponding to millisecond
     * resolution and a fractional seconds sub-pattern of SSS. If the
     * sub-pattern is S or SS, the fractional seconds value will be truncated
     * (not rounded) to the number of display places specified. If the
     * fractional seconds sub-pattern is longer than SSS, the additional
     * display places will be filled with zeros.
     * @stable ICU 3.0
     */
    UDAT_FRACTIONAL_SECOND_FIELD = 8,

    /**
     * FieldPosition and UFieldPosition selector for 'E' field alignment,
     * corresponding to the UCAL_DAY_OF_WEEK field.
     * @stable ICU 3.0
     */
    UDAT_DAY_OF_WEEK_FIELD = 9,

    /**
     * FieldPosition and UFieldPosition selector for 'D' field alignment,
     * corresponding to the UCAL_DAY_OF_YEAR field.
     * @stable ICU 3.0
     */
    UDAT_DAY_OF_YEAR_FIELD = 10,

    /**
     * FieldPosition and UFieldPosition selector for 'F' field alignment,
     * corresponding to the UCAL_DAY_OF_WEEK_IN_MONTH field.
     * @stable ICU 3.0
     */
    UDAT_DAY_OF_WEEK_IN_MONTH_FIELD = 11,

    /**
     * FieldPosition and UFieldPosition selector for 'w' field alignment,
     * corresponding to the UCAL_WEEK_OF_YEAR field.
     * @stable ICU 3.0
     */
    UDAT_WEEK_OF_YEAR_FIELD = 12,

    /**
     * FieldPosition and UFieldPosition selector for 'W' field alignment,
     * corresponding to the UCAL_WEEK_OF_MONTH field.
     * @stable ICU 3.0
     */
    UDAT_WEEK_OF_MONTH_FIELD = 13,

    /**
     * FieldPosition and UFieldPosition selector for 'a' field alignment,
     * corresponding to the UCAL_AM_PM field.
     * @stable ICU 3.0
     */
    UDAT_AM_PM_FIELD = 14,

    /**
     * FieldPosition and UFieldPosition selector for 'h' field alignment,
     * corresponding to the UCAL_HOUR field.
     * UDAT_HOUR1_FIELD is used for the one-based 12-hour clock.
     * For example, 11:30 PM + 1 hour results in 12:30 AM.
     * @stable ICU 3.0
     */
    UDAT_HOUR1_FIELD = 15,

    /**
     * FieldPosition and UFieldPosition selector for 'K' field alignment,
     * corresponding to the UCAL_HOUR field.
     * UDAT_HOUR0_FIELD is used for the zero-based 12-hour clock.
     * For example, 11:30 PM + 1 hour results in 00:30 AM.
     * @stable ICU 3.0
     */
    UDAT_HOUR0_FIELD = 16,

    /**
     * FieldPosition and UFieldPosition selector for 'z' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET and
     * UCAL_DST_OFFSET fields.
     * @stable ICU 3.0
     */
    UDAT_TIMEZONE_FIELD = 17,

    /**
     * FieldPosition and UFieldPosition selector for 'Y' field alignment,
     * corresponding to the UCAL_YEAR_WOY field.
     * @stable ICU 3.0
     */
    UDAT_YEAR_WOY_FIELD = 18,

    /**
     * FieldPosition and UFieldPosition selector for 'e' field alignment,
     * corresponding to the UCAL_DOW_LOCAL field.
     * @stable ICU 3.0
     */
    UDAT_DOW_LOCAL_FIELD = 19,

    /**
     * FieldPosition and UFieldPosition selector for 'u' field alignment,
     * corresponding to the UCAL_EXTENDED_YEAR field.
     * @stable ICU 3.0
     */
    UDAT_EXTENDED_YEAR_FIELD = 20,

    /**
     * FieldPosition and UFieldPosition selector for 'g' field alignment,
     * corresponding to the UCAL_JULIAN_DAY field.
     * @stable ICU 3.0
     */
    UDAT_JULIAN_DAY_FIELD = 21,

    /**
     * FieldPosition and UFieldPosition selector for 'A' field alignment,
     * corresponding to the UCAL_MILLISECONDS_IN_DAY field.
     * @stable ICU 3.0
     */
    UDAT_MILLISECONDS_IN_DAY_FIELD = 22,

    /**
     * FieldPosition and UFieldPosition selector for 'Z' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET and
     * UCAL_DST_OFFSET fields.
     * @stable ICU 3.0
     */
    UDAT_TIMEZONE_RFC_FIELD = 23,

    /**
     * FieldPosition and UFieldPosition selector for 'v' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET field.
     * @stable ICU 3.4
     */
    UDAT_TIMEZONE_GENERIC_FIELD = 24,
    /**
     * FieldPosition selector for 'c' field alignment,
     * corresponding to the {@link #UCAL_DOW_LOCAL} field.
     * This displays the stand alone day name, if available.
     * @stable ICU 3.4
     */
    UDAT_STANDALONE_DAY_FIELD = 25,

    /**
     * FieldPosition selector for 'L' field alignment,
     * corresponding to the {@link #UCAL_MONTH} field.
     * This displays the stand alone month name, if available.
     * @stable ICU 3.4
     */
    UDAT_STANDALONE_MONTH_FIELD = 26,

    /**
     * FieldPosition selector for "Q" field alignment,
     * corresponding to quarters. This is implemented
     * using the {@link #UCAL_MONTH} field. This
     * displays the quarter.
     * @stable ICU 3.6
     */
    UDAT_QUARTER_FIELD = 27,

    /**
     * FieldPosition selector for the "q" field alignment,
     * corresponding to stand-alone quarters. This is
     * implemented using the {@link #UCAL_MONTH} field.
     * This displays the stand-alone quarter.
     * @stable ICU 3.6
     */
    UDAT_STANDALONE_QUARTER_FIELD = 28,

    /**
     * FieldPosition and UFieldPosition selector for 'V' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET field.
     * @stable ICU 3.8
     */
    UDAT_TIMEZONE_SPECIAL_FIELD = 29,

    /**
     * FieldPosition selector for "U" field alignment,
     * corresponding to cyclic year names. This is implemented
     * using the {@link #UCAL_YEAR} field. This displays
     * the cyclic year name, if available.
     * @stable ICU 49
     */
    UDAT_YEAR_NAME_FIELD = 30,

    /**
     * FieldPosition selector for 'O' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET and UCAL_DST_OFFSETfields.
     * This displays the localized GMT format.
     * @stable ICU 51
     */
    UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD = 31,

    /**
     * FieldPosition selector for 'X' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET and UCAL_DST_OFFSETfields.
     * This displays the ISO 8601 local time offset format or UTC indicator ("Z").
     * @stable ICU 51
     */
    UDAT_TIMEZONE_ISO_FIELD = 32,

    /**
     * FieldPosition selector for 'x' field alignment,
     * corresponding to the UCAL_ZONE_OFFSET and UCAL_DST_OFFSET fields.
     * This displays the ISO 8601 local time offset format.
     * @stable ICU 51
     */
    UDAT_TIMEZONE_ISO_LOCAL_FIELD = 33,

#ifndef U_HIDE_INTERNAL_API
    /**
     * FieldPosition and UFieldPosition selector for 'r' field alignment,
     * no directly corresponding UCAL_ field.
     * @internal ICU 53
     */
    UDAT_RELATED_YEAR_FIELD = 34,
#endif /* U_HIDE_INTERNAL_API */

    /**
     * FieldPosition selector for 'b' field alignment.
     * Displays midnight and noon for 12am and 12pm, respectively, if available;
     * otherwise fall back to AM / PM.
     * @stable ICU 57
     */
    UDAT_AM_PM_MIDNIGHT_NOON_FIELD = 35,

    /* FieldPosition selector for 'B' field alignment.
     * Displays flexible day periods, such as "in the morning", if available.
     * @stable ICU 57
     */
    UDAT_FLEXIBLE_DAY_PERIOD_FIELD = 36,

#ifndef U_HIDE_INTERNAL_API
    /**
     * FieldPosition and UFieldPosition selector for time separator,
     * no corresponding UCAL_ field. No pattern character is currently
     * defined for this.
     * @internal
     */
    UDAT_TIME_SEPARATOR_FIELD = 37,
#endif /* U_HIDE_INTERNAL_API */

#ifndef U_HIDE_DEPRECATED_API
    /**
     * Number of FieldPosition and UFieldPosition selectors for
     * DateFormat and UDateFormat.
     * Valid selectors range from 0 to UDAT_FIELD_COUNT-1.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDAT_FIELD_COUNT = 38
#endif /* U_HIDE_DEPRECATED_API */
} UDateFormatField;

/**
 * DateFormat boolean attributes
 *
 * @stable ICU 53
 */
typedef enum UDateFormatBooleanAttribute {
    /**
     * indicates whether whitespace is allowed. Includes trailing dot tolerance.
     * @stable ICU 53
     */
    UDAT_PARSE_ALLOW_WHITESPACE = 0,
    /**
     * indicates tolerance of numeric data when String data may be assumed. eg: UDAT_YEAR_NAME_FIELD,
     * UDAT_STANDALONE_MONTH_FIELD, UDAT_DAY_OF_WEEK_FIELD
     * @stable ICU 53
     */
    UDAT_PARSE_ALLOW_NUMERIC = 1,
    /**
     * indicates tolerance of a partial literal match
     * e.g. accepting "--mon-02-march-2011" for a pattern of "'--: 'EEE-WW-MMMM-yyyy"
     * @stable ICU 56
     */
    UDAT_PARSE_PARTIAL_LITERAL_MATCH = 2,
    /**
     * indicates tolerance of pattern mismatch between input data and specified format pattern.
     * e.g. accepting "September" for a month pattern of MMM ("Sep")
     * @stable ICU 56
     */
    UDAT_PARSE_MULTIPLE_PATTERNS_FOR_MATCH = 3,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of DateFormat object. */
    /**
     * One more than the highest normal UDateFormatBooleanAttribute value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDAT_BOOLEAN_ATTRIBUTE_COUNT = 4
} UDateFormatBooleanAttribute;

/**
 * The possible types of date format symbols
 * @stable ICU 2.6
 */
typedef enum UDateFormatSymbolType {
    /** The era names, for example AD */
    UDAT_ERAS,
    /** The month names, for example February */
    UDAT_MONTHS,
    /** The short month names, for example Feb. */
    UDAT_SHORT_MONTHS,
    /** The CLDR-style format "wide" weekday names, for example Monday */
    UDAT_WEEKDAYS,
    /**
     * The CLDR-style format "abbreviated" (not "short") weekday names, for example "Mon."
     * For the CLDR-style format "short" weekday names, use UDAT_SHORTER_WEEKDAYS.
     */
    UDAT_SHORT_WEEKDAYS,
    /** The AM/PM names, for example AM */
    UDAT_AM_PMS,
    /** The localized characters */
    UDAT_LOCALIZED_CHARS,
    /** The long era names, for example Anno Domini */
    UDAT_ERA_NAMES,
    /** The narrow month names, for example F */
    UDAT_NARROW_MONTHS,
    /** The CLDR-style format "narrow" weekday names, for example "M" */
    UDAT_NARROW_WEEKDAYS,
    /** Standalone context versions of months */
    UDAT_STANDALONE_MONTHS,
    UDAT_STANDALONE_SHORT_MONTHS,
    UDAT_STANDALONE_NARROW_MONTHS,
    /** The CLDR-style stand-alone "wide" weekday names */
    UDAT_STANDALONE_WEEKDAYS,
    /**
     * The CLDR-style stand-alone "abbreviated" (not "short") weekday names.
     * For the CLDR-style stand-alone "short" weekday names, use UDAT_STANDALONE_SHORTER_WEEKDAYS.
     */
    UDAT_STANDALONE_SHORT_WEEKDAYS,
    /** The CLDR-style stand-alone "narrow" weekday names */
    UDAT_STANDALONE_NARROW_WEEKDAYS,
    /** The quarters, for example 1st Quarter */
    UDAT_QUARTERS,
    /** The short quarter names, for example Q1 */
    UDAT_SHORT_QUARTERS,
    /** Standalone context versions of quarters */
    UDAT_STANDALONE_QUARTERS,
    UDAT_STANDALONE_SHORT_QUARTERS,
    /**
     * The CLDR-style short weekday names, e.g. "Su", Mo", etc.
     * These are named "SHORTER" to contrast with the constants using _SHORT_
     * above, which actually get the CLDR-style *abbreviated* versions of the
     * corresponding names.
     * @stable ICU 51
     */
    UDAT_SHORTER_WEEKDAYS,
    /**
     * Standalone version of UDAT_SHORTER_WEEKDAYS.
     * @stable ICU 51
     */
    UDAT_STANDALONE_SHORTER_WEEKDAYS,
    /**
     * Cyclic year names (only supported for some calendars, and only for FORMAT usage;
     * udat_setSymbols not supported for UDAT_CYCLIC_YEARS_WIDE)
     * @stable ICU 54
     */
    UDAT_CYCLIC_YEARS_WIDE,
    /**
     * Cyclic year names (only supported for some calendars, and only for FORMAT usage)
     * @stable ICU 54
     */
    UDAT_CYCLIC_YEARS_ABBREVIATED,
    /**
     * Cyclic year names (only supported for some calendars, and only for FORMAT usage;
     * udat_setSymbols not supported for UDAT_CYCLIC_YEARS_NARROW)
     * @stable ICU 54
     */
    UDAT_CYCLIC_YEARS_NARROW,
    /**
     * Calendar zodiac  names (only supported for some calendars, and only for FORMAT usage;
     * udat_setSymbols not supported for UDAT_ZODIAC_NAMES_WIDE)
     * @stable ICU 54
     */
    UDAT_ZODIAC_NAMES_WIDE,
    /**
     * Calendar zodiac  names (only supported for some calendars, and only for FORMAT usage)
     * @stable ICU 54
     */
    UDAT_ZODIAC_NAMES_ABBREVIATED,
    /**
     * Calendar zodiac  names (only supported for some calendars, and only for FORMAT usage;
     * udat_setSymbols not supported for UDAT_ZODIAC_NAMES_NARROW)
     * @stable ICU 54
     */
    UDAT_ZODIAC_NAMES_NARROW
} UDateFormatSymbolType;

struct UDateFormatSymbols;
/** Date format symbols.
 *  For usage in C programs.
 *  @stable ICU 2.6
 */
typedef struct UDateFormatSymbols UDateFormatSymbols;

// umisc.h
/** A struct representing a range of text containing a specific field
 *  @stable ICU 2.0
 */
typedef struct UFieldPosition {
    /**
   * The field
   * @stable ICU 2.0
   */
    int32_t field;
    /**
   * The start of the text range containing field
   * @stable ICU 2.0
   */
    int32_t beginIndex;
    /**
   * The limit of the text range containing field
   * @stable ICU 2.0
   */
    int32_t endIndex;
} UFieldPosition;

// ufieldpositer.h

/**
 * Opaque UFieldPositionIterator object for use in C.
 * @stable ICU 55
 */
struct UFieldPositionIterator;
typedef struct UFieldPositionIterator UFieldPositionIterator; /**< C typedef for struct UFieldPositionIterator. @stable ICU 55 */

// unorm2.h
struct UNormalizer2;

// uscript.h
typedef enum UScriptCode {
    /*
     * Note: UScriptCode constants and their ISO script code comments
     * are parsed by preparseucd.py.
     * It matches lines like
     *     USCRIPT_<Unicode Script value name> = <integer>,  / * <ISO script code> * /
     */

    /** @stable ICU 2.2 */
    USCRIPT_INVALID_CODE = -1,
    /** @stable ICU 2.2 */
    USCRIPT_COMMON = 0, /* Zyyy */
    /** @stable ICU 2.2 */
    USCRIPT_INHERITED = 1,
    /* Zinh */ /* "Code for inherited script", for non-spacing combining marks; also Qaai */
    /** @stable ICU 2.2 */
    USCRIPT_ARABIC
    = 2, /* Arab */
    /** @stable ICU 2.2 */
    USCRIPT_ARMENIAN = 3, /* Armn */
    /** @stable ICU 2.2 */
    USCRIPT_BENGALI = 4, /* Beng */
    /** @stable ICU 2.2 */
    USCRIPT_BOPOMOFO = 5, /* Bopo */
    /** @stable ICU 2.2 */
    USCRIPT_CHEROKEE = 6, /* Cher */
    /** @stable ICU 2.2 */
    USCRIPT_COPTIC = 7, /* Copt */
    /** @stable ICU 2.2 */
    USCRIPT_CYRILLIC = 8, /* Cyrl */
    /** @stable ICU 2.2 */
    USCRIPT_DESERET = 9, /* Dsrt */
    /** @stable ICU 2.2 */
    USCRIPT_DEVANAGARI = 10, /* Deva */
    /** @stable ICU 2.2 */
    USCRIPT_ETHIOPIC = 11, /* Ethi */
    /** @stable ICU 2.2 */
    USCRIPT_GEORGIAN = 12, /* Geor */
    /** @stable ICU 2.2 */
    USCRIPT_GOTHIC = 13, /* Goth */
    /** @stable ICU 2.2 */
    USCRIPT_GREEK = 14, /* Grek */
    /** @stable ICU 2.2 */
    USCRIPT_GUJARATI = 15, /* Gujr */
    /** @stable ICU 2.2 */
    USCRIPT_GURMUKHI = 16, /* Guru */
    /** @stable ICU 2.2 */
    USCRIPT_HAN = 17, /* Hani */
    /** @stable ICU 2.2 */
    USCRIPT_HANGUL = 18, /* Hang */
    /** @stable ICU 2.2 */
    USCRIPT_HEBREW = 19, /* Hebr */
    /** @stable ICU 2.2 */
    USCRIPT_HIRAGANA = 20, /* Hira */
    /** @stable ICU 2.2 */
    USCRIPT_KANNADA = 21, /* Knda */
    /** @stable ICU 2.2 */
    USCRIPT_KATAKANA = 22, /* Kana */
    /** @stable ICU 2.2 */
    USCRIPT_KHMER = 23, /* Khmr */
    /** @stable ICU 2.2 */
    USCRIPT_LAO = 24, /* Laoo */
    /** @stable ICU 2.2 */
    USCRIPT_LATIN = 25, /* Latn */
    /** @stable ICU 2.2 */
    USCRIPT_MALAYALAM = 26, /* Mlym */
    /** @stable ICU 2.2 */
    USCRIPT_MONGOLIAN = 27, /* Mong */
    /** @stable ICU 2.2 */
    USCRIPT_MYANMAR = 28, /* Mymr */
    /** @stable ICU 2.2 */
    USCRIPT_OGHAM = 29, /* Ogam */
    /** @stable ICU 2.2 */
    USCRIPT_OLD_ITALIC = 30, /* Ital */
    /** @stable ICU 2.2 */
    USCRIPT_ORIYA = 31, /* Orya */
    /** @stable ICU 2.2 */
    USCRIPT_RUNIC = 32, /* Runr */
    /** @stable ICU 2.2 */
    USCRIPT_SINHALA = 33, /* Sinh */
    /** @stable ICU 2.2 */
    USCRIPT_SYRIAC = 34, /* Syrc */
    /** @stable ICU 2.2 */
    USCRIPT_TAMIL = 35, /* Taml */
    /** @stable ICU 2.2 */
    USCRIPT_TELUGU = 36, /* Telu */
    /** @stable ICU 2.2 */
    USCRIPT_THAANA = 37, /* Thaa */
    /** @stable ICU 2.2 */
    USCRIPT_THAI = 38, /* Thai */
    /** @stable ICU 2.2 */
    USCRIPT_TIBETAN = 39, /* Tibt */
    /** Canadian_Aboriginal script. @stable ICU 2.6 */
    USCRIPT_CANADIAN_ABORIGINAL = 40, /* Cans */
    /** Canadian_Aboriginal script (alias). @stable ICU 2.2 */
    USCRIPT_UCAS = USCRIPT_CANADIAN_ABORIGINAL,
    /** @stable ICU 2.2 */
    USCRIPT_YI = 41, /* Yiii */
    /* New scripts in Unicode 3.2 */
    /** @stable ICU 2.2 */
    USCRIPT_TAGALOG = 42, /* Tglg */
    /** @stable ICU 2.2 */
    USCRIPT_HANUNOO = 43, /* Hano */
    /** @stable ICU 2.2 */
    USCRIPT_BUHID = 44, /* Buhd */
    /** @stable ICU 2.2 */
    USCRIPT_TAGBANWA = 45, /* Tagb */

    /* New scripts in Unicode 4 */
    /** @stable ICU 2.6 */
    USCRIPT_BRAILLE = 46, /* Brai */
    /** @stable ICU 2.6 */
    USCRIPT_CYPRIOT = 47, /* Cprt */
    /** @stable ICU 2.6 */
    USCRIPT_LIMBU = 48, /* Limb */
    /** @stable ICU 2.6 */
    USCRIPT_LINEAR_B = 49, /* Linb */
    /** @stable ICU 2.6 */
    USCRIPT_OSMANYA = 50, /* Osma */
    /** @stable ICU 2.6 */
    USCRIPT_SHAVIAN = 51, /* Shaw */
    /** @stable ICU 2.6 */
    USCRIPT_TAI_LE = 52, /* Tale */
    /** @stable ICU 2.6 */
    USCRIPT_UGARITIC = 53, /* Ugar */

    /** New script code in Unicode 4.0.1 @stable ICU 3.0 */
    USCRIPT_KATAKANA_OR_HIRAGANA = 54, /*Hrkt */

    /* New scripts in Unicode 4.1 */
    /** @stable ICU 3.4 */
    USCRIPT_BUGINESE = 55, /* Bugi */
    /** @stable ICU 3.4 */
    USCRIPT_GLAGOLITIC = 56, /* Glag */
    /** @stable ICU 3.4 */
    USCRIPT_KHAROSHTHI = 57, /* Khar */
    /** @stable ICU 3.4 */
    USCRIPT_SYLOTI_NAGRI = 58, /* Sylo */
    /** @stable ICU 3.4 */
    USCRIPT_NEW_TAI_LUE = 59, /* Talu */
    /** @stable ICU 3.4 */
    USCRIPT_TIFINAGH = 60, /* Tfng */
    /** @stable ICU 3.4 */
    USCRIPT_OLD_PERSIAN = 61, /* Xpeo */

    /* New script codes from Unicode and ISO 15924 */
    /** @stable ICU 3.6 */
    USCRIPT_BALINESE = 62, /* Bali */
    /** @stable ICU 3.6 */
    USCRIPT_BATAK = 63, /* Batk */
    /** @stable ICU 3.6 */
    USCRIPT_BLISSYMBOLS = 64, /* Blis */
    /** @stable ICU 3.6 */
    USCRIPT_BRAHMI = 65, /* Brah */
    /** @stable ICU 3.6 */
    USCRIPT_CHAM = 66, /* Cham */
    /** @stable ICU 3.6 */
    USCRIPT_CIRTH = 67, /* Cirt */
    /** @stable ICU 3.6 */
    USCRIPT_OLD_CHURCH_SLAVONIC_CYRILLIC = 68, /* Cyrs */
    /** @stable ICU 3.6 */
    USCRIPT_DEMOTIC_EGYPTIAN = 69, /* Egyd */
    /** @stable ICU 3.6 */
    USCRIPT_HIERATIC_EGYPTIAN = 70, /* Egyh */
    /** @stable ICU 3.6 */
    USCRIPT_EGYPTIAN_HIEROGLYPHS = 71, /* Egyp */
    /** @stable ICU 3.6 */
    USCRIPT_KHUTSURI = 72, /* Geok */
    /** @stable ICU 3.6 */
    USCRIPT_SIMPLIFIED_HAN = 73, /* Hans */
    /** @stable ICU 3.6 */
    USCRIPT_TRADITIONAL_HAN = 74, /* Hant */
    /** @stable ICU 3.6 */
    USCRIPT_PAHAWH_HMONG = 75, /* Hmng */
    /** @stable ICU 3.6 */
    USCRIPT_OLD_HUNGARIAN = 76, /* Hung */
    /** @stable ICU 3.6 */
    USCRIPT_HARAPPAN_INDUS = 77, /* Inds */
    /** @stable ICU 3.6 */
    USCRIPT_JAVANESE = 78, /* Java */
    /** @stable ICU 3.6 */
    USCRIPT_KAYAH_LI = 79, /* Kali */
    /** @stable ICU 3.6 */
    USCRIPT_LATIN_FRAKTUR = 80, /* Latf */
    /** @stable ICU 3.6 */
    USCRIPT_LATIN_GAELIC = 81, /* Latg */
    /** @stable ICU 3.6 */
    USCRIPT_LEPCHA = 82, /* Lepc */
    /** @stable ICU 3.6 */
    USCRIPT_LINEAR_A = 83, /* Lina */
    /** @stable ICU 4.6 */
    USCRIPT_MANDAIC = 84, /* Mand */
    /** @stable ICU 3.6 */
    USCRIPT_MANDAEAN = USCRIPT_MANDAIC,
    /** @stable ICU 3.6 */
    USCRIPT_MAYAN_HIEROGLYPHS = 85, /* Maya */
    /** @stable ICU 4.6 */
    USCRIPT_MEROITIC_HIEROGLYPHS = 86, /* Mero */
    /** @stable ICU 3.6 */
    USCRIPT_MEROITIC = USCRIPT_MEROITIC_HIEROGLYPHS,
    /** @stable ICU 3.6 */
    USCRIPT_NKO = 87, /* Nkoo */
    /** @stable ICU 3.6 */
    USCRIPT_ORKHON = 88, /* Orkh */
    /** @stable ICU 3.6 */
    USCRIPT_OLD_PERMIC = 89, /* Perm */
    /** @stable ICU 3.6 */
    USCRIPT_PHAGS_PA = 90, /* Phag */
    /** @stable ICU 3.6 */
    USCRIPT_PHOENICIAN = 91, /* Phnx */
    /** @stable ICU 52 */
    USCRIPT_MIAO = 92, /* Plrd */
    /** @stable ICU 3.6 */
    USCRIPT_PHONETIC_POLLARD = USCRIPT_MIAO,
    /** @stable ICU 3.6 */
    USCRIPT_RONGORONGO = 93, /* Roro */
    /** @stable ICU 3.6 */
    USCRIPT_SARATI = 94, /* Sara */
    /** @stable ICU 3.6 */
    USCRIPT_ESTRANGELO_SYRIAC = 95, /* Syre */
    /** @stable ICU 3.6 */
    USCRIPT_WESTERN_SYRIAC = 96, /* Syrj */
    /** @stable ICU 3.6 */
    USCRIPT_EASTERN_SYRIAC = 97, /* Syrn */
    /** @stable ICU 3.6 */
    USCRIPT_TENGWAR = 98, /* Teng */
    /** @stable ICU 3.6 */
    USCRIPT_VAI = 99, /* Vaii */
    /** @stable ICU 3.6 */
    USCRIPT_VISIBLE_SPEECH = 100, /* Visp */
    /** @stable ICU 3.6 */
    USCRIPT_CUNEIFORM = 101, /* Xsux */
    /** @stable ICU 3.6 */
    USCRIPT_UNWRITTEN_LANGUAGES = 102, /* Zxxx */
    /** @stable ICU 3.6 */
    USCRIPT_UNKNOWN = 103,
    /* Zzzz */ /* Unknown="Code for uncoded script", for unassigned code points */

    /** @stable ICU 3.8 */
    USCRIPT_CARIAN
    = 104, /* Cari */
    /** @stable ICU 3.8 */
    USCRIPT_JAPANESE = 105, /* Jpan */
    /** @stable ICU 3.8 */
    USCRIPT_LANNA = 106, /* Lana */
    /** @stable ICU 3.8 */
    USCRIPT_LYCIAN = 107, /* Lyci */
    /** @stable ICU 3.8 */
    USCRIPT_LYDIAN = 108, /* Lydi */
    /** @stable ICU 3.8 */
    USCRIPT_OL_CHIKI = 109, /* Olck */
    /** @stable ICU 3.8 */
    USCRIPT_REJANG = 110, /* Rjng */
    /** @stable ICU 3.8 */
    USCRIPT_SAURASHTRA = 111, /* Saur */
    /** Sutton SignWriting @stable ICU 3.8 */
    USCRIPT_SIGN_WRITING = 112, /* Sgnw */
    /** @stable ICU 3.8 */
    USCRIPT_SUNDANESE = 113, /* Sund */
    /** @stable ICU 3.8 */
    USCRIPT_MOON = 114, /* Moon */
    /** @stable ICU 3.8 */
    USCRIPT_MEITEI_MAYEK = 115, /* Mtei */

    /** @stable ICU 4.0 */
    USCRIPT_IMPERIAL_ARAMAIC = 116, /* Armi */
    /** @stable ICU 4.0 */
    USCRIPT_AVESTAN = 117, /* Avst */
    /** @stable ICU 4.0 */
    USCRIPT_CHAKMA = 118, /* Cakm */
    /** @stable ICU 4.0 */
    USCRIPT_KOREAN = 119, /* Kore */
    /** @stable ICU 4.0 */
    USCRIPT_KAITHI = 120, /* Kthi */
    /** @stable ICU 4.0 */
    USCRIPT_MANICHAEAN = 121, /* Mani */
    /** @stable ICU 4.0 */
    USCRIPT_INSCRIPTIONAL_PAHLAVI = 122, /* Phli */
    /** @stable ICU 4.0 */
    USCRIPT_PSALTER_PAHLAVI = 123, /* Phlp */
    /** @stable ICU 4.0 */
    USCRIPT_BOOK_PAHLAVI = 124, /* Phlv */
    /** @stable ICU 4.0 */
    USCRIPT_INSCRIPTIONAL_PARTHIAN = 125, /* Prti */
    /** @stable ICU 4.0 */
    USCRIPT_SAMARITAN = 126, /* Samr */
    /** @stable ICU 4.0 */
    USCRIPT_TAI_VIET = 127, /* Tavt */
    /** @stable ICU 4.0 */
    USCRIPT_MATHEMATICAL_NOTATION = 128, /* Zmth */
    /** @stable ICU 4.0 */
    USCRIPT_SYMBOLS = 129, /* Zsym */

    /** @stable ICU 4.4 */
    USCRIPT_BAMUM = 130, /* Bamu */
    /** @stable ICU 4.4 */
    USCRIPT_LISU = 131, /* Lisu */
    /** @stable ICU 4.4 */
    USCRIPT_NAKHI_GEBA = 132, /* Nkgb */
    /** @stable ICU 4.4 */
    USCRIPT_OLD_SOUTH_ARABIAN = 133, /* Sarb */

    /** @stable ICU 4.6 */
    USCRIPT_BASSA_VAH = 134, /* Bass */
    /** @stable ICU 54 */
    USCRIPT_DUPLOYAN = 135, /* Dupl */
#ifndef U_HIDE_DEPRECATED_API
    /** @deprecated ICU 54 Typo, use USCRIPT_DUPLOYAN */
    USCRIPT_DUPLOYAN_SHORTAND = USCRIPT_DUPLOYAN,
#endif /* U_HIDE_DEPRECATED_API */
    /** @stable ICU 4.6 */
    USCRIPT_ELBASAN = 136, /* Elba */
    /** @stable ICU 4.6 */
    USCRIPT_GRANTHA = 137, /* Gran */
    /** @stable ICU 4.6 */
    USCRIPT_KPELLE = 138, /* Kpel */
    /** @stable ICU 4.6 */
    USCRIPT_LOMA = 139, /* Loma */
    /** Mende Kikakui @stable ICU 4.6 */
    USCRIPT_MENDE = 140, /* Mend */
    /** @stable ICU 4.6 */
    USCRIPT_MEROITIC_CURSIVE = 141, /* Merc */
    /** @stable ICU 4.6 */
    USCRIPT_OLD_NORTH_ARABIAN = 142, /* Narb */
    /** @stable ICU 4.6 */
    USCRIPT_NABATAEAN = 143, /* Nbat */
    /** @stable ICU 4.6 */
    USCRIPT_PALMYRENE = 144, /* Palm */
    /** @stable ICU 54 */
    USCRIPT_KHUDAWADI = 145, /* Sind */
    /** @stable ICU 4.6 */
    USCRIPT_SINDHI = USCRIPT_KHUDAWADI,
    /** @stable ICU 4.6 */
    USCRIPT_WARANG_CITI = 146, /* Wara */

    /** @stable ICU 4.8 */
    USCRIPT_AFAKA = 147, /* Afak */
    /** @stable ICU 4.8 */
    USCRIPT_JURCHEN = 148, /* Jurc */
    /** @stable ICU 4.8 */
    USCRIPT_MRO = 149, /* Mroo */
    /** @stable ICU 4.8 */
    USCRIPT_NUSHU = 150, /* Nshu */
    /** @stable ICU 4.8 */
    USCRIPT_SHARADA = 151, /* Shrd */
    /** @stable ICU 4.8 */
    USCRIPT_SORA_SOMPENG = 152, /* Sora */
    /** @stable ICU 4.8 */
    USCRIPT_TAKRI = 153, /* Takr */
    /** @stable ICU 4.8 */
    USCRIPT_TANGUT = 154, /* Tang */
    /** @stable ICU 4.8 */
    USCRIPT_WOLEAI = 155, /* Wole */

    /** @stable ICU 49 */
    USCRIPT_ANATOLIAN_HIEROGLYPHS = 156, /* Hluw */
    /** @stable ICU 49 */
    USCRIPT_KHOJKI = 157, /* Khoj */
    /** @stable ICU 49 */
    USCRIPT_TIRHUTA = 158, /* Tirh */

    /** @stable ICU 52 */
    USCRIPT_CAUCASIAN_ALBANIAN = 159, /* Aghb */
    /** @stable ICU 52 */
    USCRIPT_MAHAJANI = 160, /* Mahj */

    /** @stable ICU 54 */
    USCRIPT_AHOM = 161, /* Ahom */
    /** @stable ICU 54 */
    USCRIPT_HATRAN = 162, /* Hatr */
    /** @stable ICU 54 */
    USCRIPT_MODI = 163, /* Modi */
    /** @stable ICU 54 */
    USCRIPT_MULTANI = 164, /* Mult */
    /** @stable ICU 54 */
    USCRIPT_PAU_CIN_HAU = 165, /* Pauc */
    /** @stable ICU 54 */
    USCRIPT_SIDDHAM = 166, /* Sidd */

    /** @stable ICU 58 */
    USCRIPT_ADLAM = 167, /* Adlm */
    /** @stable ICU 58 */
    USCRIPT_BHAIKSUKI = 168, /* Bhks */
    /** @stable ICU 58 */
    USCRIPT_MARCHEN = 169, /* Marc */
    /** @stable ICU 58 */
    USCRIPT_NEWA = 170, /* Newa */
    /** @stable ICU 58 */
    USCRIPT_OSAGE = 171, /* Osge */

    /** @stable ICU 58 */
    USCRIPT_HAN_WITH_BOPOMOFO = 172, /* Hanb */
    /** @stable ICU 58 */
    USCRIPT_JAMO = 173, /* Jamo */
    /** @stable ICU 58 */
    USCRIPT_SYMBOLS_EMOJI = 174, /* Zsye */

    /** @stable ICU 60 */
    USCRIPT_MASARAM_GONDI = 175, /* Gonm */
    /** @stable ICU 60 */
    USCRIPT_SOYOMBO = 176, /* Soyo */
    /** @stable ICU 60 */
    USCRIPT_ZANABAZAR_SQUARE = 177, /* Zanb */

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UScriptCode value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_SCRIPT).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    USCRIPT_CODE_LIMIT = 178
#endif // U_HIDE_DEPRECATED_API
} UScriptCode;

// uchar.h
typedef enum UProperty {
    /*
     * Note: UProperty constants are parsed by preparseucd.py.
     * It matches lines like
     *     UCHAR_<Unicode property name>=<integer>,
     */

    /*  Note: Place UCHAR_ALPHABETIC before UCHAR_BINARY_START so that
    debuggers display UCHAR_ALPHABETIC as the symbolic name for 0,
    rather than UCHAR_BINARY_START.  Likewise for other *_START
    identifiers. */

    /** Binary property Alphabetic. Same as u_isUAlphabetic, different from u_isalpha.
        Lu+Ll+Lt+Lm+Lo+Nl+Other_Alphabetic @stable ICU 2.1 */
    UCHAR_ALPHABETIC = 0,
    /** First constant for binary Unicode properties. @stable ICU 2.1 */
    UCHAR_BINARY_START = UCHAR_ALPHABETIC,
    /** Binary property ASCII_Hex_Digit. 0-9 A-F a-f @stable ICU 2.1 */
    UCHAR_ASCII_HEX_DIGIT = 1,
    /** Binary property Bidi_Control.
        Format controls which have specific functions
        in the Bidi Algorithm. @stable ICU 2.1 */
    UCHAR_BIDI_CONTROL = 2,
    /** Binary property Bidi_Mirrored.
        Characters that may change display in RTL text.
        Same as u_isMirrored.
        See Bidi Algorithm, UTR 9. @stable ICU 2.1 */
    UCHAR_BIDI_MIRRORED = 3,
    /** Binary property Dash. Variations of dashes. @stable ICU 2.1 */
    UCHAR_DASH = 4,
    /** Binary property Default_Ignorable_Code_Point (new in Unicode 3.2).
        Ignorable in most processing.
        <2060..206F, FFF0..FFFB, E0000..E0FFF>+Other_Default_Ignorable_Code_Point+(Cf+Cc+Cs-White_Space) @stable ICU 2.1 */
    UCHAR_DEFAULT_IGNORABLE_CODE_POINT = 5,
    /** Binary property Deprecated (new in Unicode 3.2).
        The usage of deprecated characters is strongly discouraged. @stable ICU 2.1 */
    UCHAR_DEPRECATED = 6,
    /** Binary property Diacritic. Characters that linguistically modify
        the meaning of another character to which they apply. @stable ICU 2.1 */
    UCHAR_DIACRITIC = 7,
    /** Binary property Extender.
        Extend the value or shape of a preceding alphabetic character,
        e.g., length and iteration marks. @stable ICU 2.1 */
    UCHAR_EXTENDER = 8,
    /** Binary property Full_Composition_Exclusion.
        CompositionExclusions.txt+Singleton Decompositions+
        Non-Starter Decompositions. @stable ICU 2.1 */
    UCHAR_FULL_COMPOSITION_EXCLUSION = 9,
    /** Binary property Grapheme_Base (new in Unicode 3.2).
        For programmatic determination of grapheme cluster boundaries.
        [0..10FFFF]-Cc-Cf-Cs-Co-Cn-Zl-Zp-Grapheme_Link-Grapheme_Extend-CGJ @stable ICU 2.1 */
    UCHAR_GRAPHEME_BASE = 10,
    /** Binary property Grapheme_Extend (new in Unicode 3.2).
        For programmatic determination of grapheme cluster boundaries.
        Me+Mn+Mc+Other_Grapheme_Extend-Grapheme_Link-CGJ @stable ICU 2.1 */
    UCHAR_GRAPHEME_EXTEND = 11,
    /** Binary property Grapheme_Link (new in Unicode 3.2).
        For programmatic determination of grapheme cluster boundaries. @stable ICU 2.1 */
    UCHAR_GRAPHEME_LINK = 12,
    /** Binary property Hex_Digit.
        Characters commonly used for hexadecimal numbers. @stable ICU 2.1 */
    UCHAR_HEX_DIGIT = 13,
    /** Binary property Hyphen. Dashes used to mark connections
        between pieces of words, plus the Katakana middle dot. @stable ICU 2.1 */
    UCHAR_HYPHEN = 14,
    /** Binary property ID_Continue.
        Characters that can continue an identifier.
        DerivedCoreProperties.txt also says "NOTE: Cf characters should be filtered out."
        ID_Start+Mn+Mc+Nd+Pc @stable ICU 2.1 */
    UCHAR_ID_CONTINUE = 15,
    /** Binary property ID_Start.
        Characters that can start an identifier.
        Lu+Ll+Lt+Lm+Lo+Nl @stable ICU 2.1 */
    UCHAR_ID_START = 16,
    /** Binary property Ideographic.
        CJKV ideographs. @stable ICU 2.1 */
    UCHAR_IDEOGRAPHIC = 17,
    /** Binary property IDS_Binary_Operator (new in Unicode 3.2).
        For programmatic determination of
        Ideographic Description Sequences. @stable ICU 2.1 */
    UCHAR_IDS_BINARY_OPERATOR = 18,
    /** Binary property IDS_Trinary_Operator (new in Unicode 3.2).
        For programmatic determination of
        Ideographic Description Sequences. @stable ICU 2.1 */
    UCHAR_IDS_TRINARY_OPERATOR = 19,
    /** Binary property Join_Control.
        Format controls for cursive joining and ligation. @stable ICU 2.1 */
    UCHAR_JOIN_CONTROL = 20,
    /** Binary property Logical_Order_Exception (new in Unicode 3.2).
        Characters that do not use logical order and
        require special handling in most processing. @stable ICU 2.1 */
    UCHAR_LOGICAL_ORDER_EXCEPTION = 21,
    /** Binary property Lowercase. Same as u_isULowercase, different from u_islower.
        Ll+Other_Lowercase @stable ICU 2.1 */
    UCHAR_LOWERCASE = 22,
    /** Binary property Math. Sm+Other_Math @stable ICU 2.1 */
    UCHAR_MATH = 23,
    /** Binary property Noncharacter_Code_Point.
        Code points that are explicitly defined as illegal
        for the encoding of characters. @stable ICU 2.1 */
    UCHAR_NONCHARACTER_CODE_POINT = 24,
    /** Binary property Quotation_Mark. @stable ICU 2.1 */
    UCHAR_QUOTATION_MARK = 25,
    /** Binary property Radical (new in Unicode 3.2).
        For programmatic determination of
        Ideographic Description Sequences. @stable ICU 2.1 */
    UCHAR_RADICAL = 26,
    /** Binary property Soft_Dotted (new in Unicode 3.2).
        Characters with a "soft dot", like i or j.
        An accent placed on these characters causes
        the dot to disappear. @stable ICU 2.1 */
    UCHAR_SOFT_DOTTED = 27,
    /** Binary property Terminal_Punctuation.
        Punctuation characters that generally mark
        the end of textual units. @stable ICU 2.1 */
    UCHAR_TERMINAL_PUNCTUATION = 28,
    /** Binary property Unified_Ideograph (new in Unicode 3.2).
        For programmatic determination of
        Ideographic Description Sequences. @stable ICU 2.1 */
    UCHAR_UNIFIED_IDEOGRAPH = 29,
    /** Binary property Uppercase. Same as u_isUUppercase, different from u_isupper.
        Lu+Other_Uppercase @stable ICU 2.1 */
    UCHAR_UPPERCASE = 30,
    /** Binary property White_Space.
        Same as u_isUWhiteSpace, different from u_isspace and u_isWhitespace.
        Space characters+TAB+CR+LF-ZWSP-ZWNBSP @stable ICU 2.1 */
    UCHAR_WHITE_SPACE = 31,
    /** Binary property XID_Continue.
        ID_Continue modified to allow closure under
        normalization forms NFKC and NFKD. @stable ICU 2.1 */
    UCHAR_XID_CONTINUE = 32,
    /** Binary property XID_Start. ID_Start modified to allow
        closure under normalization forms NFKC and NFKD. @stable ICU 2.1 */
    UCHAR_XID_START = 33,
    /** Binary property Case_Sensitive. Either the source of a case
        mapping or _in_ the target of a case mapping. Not the same as
        the general category Cased_Letter. @stable ICU 2.6 */
    UCHAR_CASE_SENSITIVE = 34,
    /** Binary property STerm (new in Unicode 4.0.1).
        Sentence Terminal. Used in UAX #29: Text Boundaries
        (http://www.unicode.org/reports/tr29/)
        @stable ICU 3.0 */
    UCHAR_S_TERM = 35,
    /** Binary property Variation_Selector (new in Unicode 4.0.1).
        Indicates all those characters that qualify as Variation Selectors.
        For details on the behavior of these characters,
        see StandardizedVariants.html and 15.6 Variation Selectors.
        @stable ICU 3.0 */
    UCHAR_VARIATION_SELECTOR = 36,
    /** Binary property NFD_Inert.
        ICU-specific property for characters that are inert under NFD,
        i.e., they do not interact with adjacent characters.
        See the documentation for the Normalizer2 class and the
        Normalizer2::isInert() method.
        @stable ICU 3.0 */
    UCHAR_NFD_INERT = 37,
    /** Binary property NFKD_Inert.
        ICU-specific property for characters that are inert under NFKD,
        i.e., they do not interact with adjacent characters.
        See the documentation for the Normalizer2 class and the
        Normalizer2::isInert() method.
        @stable ICU 3.0 */
    UCHAR_NFKD_INERT = 38,
    /** Binary property NFC_Inert.
        ICU-specific property for characters that are inert under NFC,
        i.e., they do not interact with adjacent characters.
        See the documentation for the Normalizer2 class and the
        Normalizer2::isInert() method.
        @stable ICU 3.0 */
    UCHAR_NFC_INERT = 39,
    /** Binary property NFKC_Inert.
        ICU-specific property for characters that are inert under NFKC,
        i.e., they do not interact with adjacent characters.
        See the documentation for the Normalizer2 class and the
        Normalizer2::isInert() method.
        @stable ICU 3.0 */
    UCHAR_NFKC_INERT = 40,
    /** Binary Property Segment_Starter.
        ICU-specific property for characters that are starters in terms of
        Unicode normalization and combining character sequences.
        They have ccc=0 and do not occur in non-initial position of the
        canonical decomposition of any character
        (like a-umlaut in NFD and a Jamo T in an NFD(Hangul LVT)).
        ICU uses this property for segmenting a string for generating a set of
        canonically equivalent strings, e.g. for canonical closure while
        processing collation tailoring rules.
        @stable ICU 3.0 */
    UCHAR_SEGMENT_STARTER = 41,
    /** Binary property Pattern_Syntax (new in Unicode 4.1).
        See UAX #31 Identifier and Pattern Syntax
        (http://www.unicode.org/reports/tr31/)
        @stable ICU 3.4 */
    UCHAR_PATTERN_SYNTAX = 42,
    /** Binary property Pattern_White_Space (new in Unicode 4.1).
        See UAX #31 Identifier and Pattern Syntax
        (http://www.unicode.org/reports/tr31/)
        @stable ICU 3.4 */
    UCHAR_PATTERN_WHITE_SPACE = 43,
    /** Binary property alnum (a C/POSIX character class).
        Implemented according to the UTS #18 Annex C Standard Recommendation.
        See the uchar.h file documentation.
        @stable ICU 3.4 */
    UCHAR_POSIX_ALNUM = 44,
    /** Binary property blank (a C/POSIX character class).
        Implemented according to the UTS #18 Annex C Standard Recommendation.
        See the uchar.h file documentation.
        @stable ICU 3.4 */
    UCHAR_POSIX_BLANK = 45,
    /** Binary property graph (a C/POSIX character class).
        Implemented according to the UTS #18 Annex C Standard Recommendation.
        See the uchar.h file documentation.
        @stable ICU 3.4 */
    UCHAR_POSIX_GRAPH = 46,
    /** Binary property print (a C/POSIX character class).
        Implemented according to the UTS #18 Annex C Standard Recommendation.
        See the uchar.h file documentation.
        @stable ICU 3.4 */
    UCHAR_POSIX_PRINT = 47,
    /** Binary property xdigit (a C/POSIX character class).
        Implemented according to the UTS #18 Annex C Standard Recommendation.
        See the uchar.h file documentation.
        @stable ICU 3.4 */
    UCHAR_POSIX_XDIGIT = 48,
    /** Binary property Cased. For Lowercase, Uppercase and Titlecase characters. @stable ICU 4.4 */
    UCHAR_CASED = 49,
    /** Binary property Case_Ignorable. Used in context-sensitive case mappings. @stable ICU 4.4 */
    UCHAR_CASE_IGNORABLE = 50,
    /** Binary property Changes_When_Lowercased. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_LOWERCASED = 51,
    /** Binary property Changes_When_Uppercased. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_UPPERCASED = 52,
    /** Binary property Changes_When_Titlecased. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_TITLECASED = 53,
    /** Binary property Changes_When_Casefolded. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_CASEFOLDED = 54,
    /** Binary property Changes_When_Casemapped. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_CASEMAPPED = 55,
    /** Binary property Changes_When_NFKC_Casefolded. @stable ICU 4.4 */
    UCHAR_CHANGES_WHEN_NFKC_CASEFOLDED = 56,
    /**
     * Binary property Emoji.
     * See http://www.unicode.org/reports/tr51/#Emoji_Properties
     *
     * @stable ICU 57
     */
    UCHAR_EMOJI = 57,
    /**
     * Binary property Emoji_Presentation.
     * See http://www.unicode.org/reports/tr51/#Emoji_Properties
     *
     * @stable ICU 57
     */
    UCHAR_EMOJI_PRESENTATION = 58,
    /**
     * Binary property Emoji_Modifier.
     * See http://www.unicode.org/reports/tr51/#Emoji_Properties
     *
     * @stable ICU 57
     */
    UCHAR_EMOJI_MODIFIER = 59,
    /**
     * Binary property Emoji_Modifier_Base.
     * See http://www.unicode.org/reports/tr51/#Emoji_Properties
     *
     * @stable ICU 57
     */
    UCHAR_EMOJI_MODIFIER_BASE = 60,
    /**
     * Binary property Emoji_Component.
     * See http://www.unicode.org/reports/tr51/#Emoji_Properties
     *
     * @stable ICU 60
     */
    UCHAR_EMOJI_COMPONENT = 61,
    /**
     * Binary property Regional_Indicator.
     * @stable ICU 60
     */
    UCHAR_REGIONAL_INDICATOR = 62,
    /**
     * Binary property Prepended_Concatenation_Mark.
     * @stable ICU 60
     */
    UCHAR_PREPENDED_CONCATENATION_MARK = 63,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for binary Unicode properties.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_BINARY_LIMIT,
#endif // U_HIDE_DEPRECATED_API

    /** Enumerated property Bidi_Class.
        Same as u_charDirection, returns UCharDirection values. @stable ICU 2.2 */
    UCHAR_BIDI_CLASS = 0x1000,
    /** First constant for enumerated/integer Unicode properties. @stable ICU 2.2 */
    UCHAR_INT_START = UCHAR_BIDI_CLASS,
    /** Enumerated property Block.
        Same as ublock_getCode, returns UBlockCode values. @stable ICU 2.2 */
    UCHAR_BLOCK = 0x1001,
    /** Enumerated property Canonical_Combining_Class.
        Same as u_getCombiningClass, returns 8-bit numeric values. @stable ICU 2.2 */
    UCHAR_CANONICAL_COMBINING_CLASS = 0x1002,
    /** Enumerated property Decomposition_Type.
        Returns UDecompositionType values. @stable ICU 2.2 */
    UCHAR_DECOMPOSITION_TYPE = 0x1003,
    /** Enumerated property East_Asian_Width.
        See http://www.unicode.org/reports/tr11/
        Returns UEastAsianWidth values. @stable ICU 2.2 */
    UCHAR_EAST_ASIAN_WIDTH = 0x1004,
    /** Enumerated property General_Category.
        Same as u_charType, returns UCharCategory values. @stable ICU 2.2 */
    UCHAR_GENERAL_CATEGORY = 0x1005,
    /** Enumerated property Joining_Group.
        Returns UJoiningGroup values. @stable ICU 2.2 */
    UCHAR_JOINING_GROUP = 0x1006,
    /** Enumerated property Joining_Type.
        Returns UJoiningType values. @stable ICU 2.2 */
    UCHAR_JOINING_TYPE = 0x1007,
    /** Enumerated property Line_Break.
        Returns ULineBreak values. @stable ICU 2.2 */
    UCHAR_LINE_BREAK = 0x1008,
    /** Enumerated property Numeric_Type.
        Returns UNumericType values. @stable ICU 2.2 */
    UCHAR_NUMERIC_TYPE = 0x1009,
    /** Enumerated property Script.
        Same as uscript_getScript, returns UScriptCode values. @stable ICU 2.2 */
    UCHAR_SCRIPT = 0x100A,
    /** Enumerated property Hangul_Syllable_Type, new in Unicode 4.
        Returns UHangulSyllableType values. @stable ICU 2.6 */
    UCHAR_HANGUL_SYLLABLE_TYPE = 0x100B,
    /** Enumerated property NFD_Quick_Check.
        Returns UNormalizationCheckResult values. @stable ICU 3.0 */
    UCHAR_NFD_QUICK_CHECK = 0x100C,
    /** Enumerated property NFKD_Quick_Check.
        Returns UNormalizationCheckResult values. @stable ICU 3.0 */
    UCHAR_NFKD_QUICK_CHECK = 0x100D,
    /** Enumerated property NFC_Quick_Check.
        Returns UNormalizationCheckResult values. @stable ICU 3.0 */
    UCHAR_NFC_QUICK_CHECK = 0x100E,
    /** Enumerated property NFKC_Quick_Check.
        Returns UNormalizationCheckResult values. @stable ICU 3.0 */
    UCHAR_NFKC_QUICK_CHECK = 0x100F,
    /** Enumerated property Lead_Canonical_Combining_Class.
        ICU-specific property for the ccc of the first code point
        of the decomposition, or lccc(c)=ccc(NFD(c)[0]).
        Useful for checking for canonically ordered text;
        see UNORM_FCD and http://www.unicode.org/notes/tn5/#FCD .
        Returns 8-bit numeric values like UCHAR_CANONICAL_COMBINING_CLASS. @stable ICU 3.0 */
    UCHAR_LEAD_CANONICAL_COMBINING_CLASS = 0x1010,
    /** Enumerated property Trail_Canonical_Combining_Class.
        ICU-specific property for the ccc of the last code point
        of the decomposition, or tccc(c)=ccc(NFD(c)[last]).
        Useful for checking for canonically ordered text;
        see UNORM_FCD and http://www.unicode.org/notes/tn5/#FCD .
        Returns 8-bit numeric values like UCHAR_CANONICAL_COMBINING_CLASS. @stable ICU 3.0 */
    UCHAR_TRAIL_CANONICAL_COMBINING_CLASS = 0x1011,
    /** Enumerated property Grapheme_Cluster_Break (new in Unicode 4.1).
        Used in UAX #29: Text Boundaries
        (http://www.unicode.org/reports/tr29/)
        Returns UGraphemeClusterBreak values. @stable ICU 3.4 */
    UCHAR_GRAPHEME_CLUSTER_BREAK = 0x1012,
    /** Enumerated property Sentence_Break (new in Unicode 4.1).
        Used in UAX #29: Text Boundaries
        (http://www.unicode.org/reports/tr29/)
        Returns USentenceBreak values. @stable ICU 3.4 */
    UCHAR_SENTENCE_BREAK = 0x1013,
    /** Enumerated property Word_Break (new in Unicode 4.1).
        Used in UAX #29: Text Boundaries
        (http://www.unicode.org/reports/tr29/)
        Returns UWordBreakValues values. @stable ICU 3.4 */
    UCHAR_WORD_BREAK = 0x1014,
    /** Enumerated property Bidi_Paired_Bracket_Type (new in Unicode 6.3).
        Used in UAX #9: Unicode Bidirectional Algorithm
        (http://www.unicode.org/reports/tr9/)
        Returns UBidiPairedBracketType values. @stable ICU 52 */
    UCHAR_BIDI_PAIRED_BRACKET_TYPE = 0x1015,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for enumerated/integer Unicode properties.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_INT_LIMIT = 0x1016,
#endif // U_HIDE_DEPRECATED_API

    /** Bitmask property General_Category_Mask.
        This is the General_Category property returned as a bit mask.
        When used in u_getIntPropertyValue(c), same as U_MASK(u_charType(c)),
        returns bit masks for UCharCategory values where exactly one bit is set.
        When used with u_getPropertyValueName() and u_getPropertyValueEnum(),
        a multi-bit mask is used for sets of categories like "Letters".
        Mask values should be cast to uint32_t.
        @stable ICU 2.4 */
    UCHAR_GENERAL_CATEGORY_MASK = 0x2000,
    /** First constant for bit-mask Unicode properties. @stable ICU 2.4 */
    UCHAR_MASK_START = UCHAR_GENERAL_CATEGORY_MASK,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for bit-mask Unicode properties.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_MASK_LIMIT = 0x2001,
#endif // U_HIDE_DEPRECATED_API

    /** Double property Numeric_Value.
        Corresponds to u_getNumericValue. @stable ICU 2.4 */
    UCHAR_NUMERIC_VALUE = 0x3000,
    /** First constant for double Unicode properties. @stable ICU 2.4 */
    UCHAR_DOUBLE_START = UCHAR_NUMERIC_VALUE,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for double Unicode properties.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_DOUBLE_LIMIT = 0x3001,
#endif // U_HIDE_DEPRECATED_API

    /** String property Age.
        Corresponds to u_charAge. @stable ICU 2.4 */
    UCHAR_AGE = 0x4000,
    /** First constant for string Unicode properties. @stable ICU 2.4 */
    UCHAR_STRING_START = UCHAR_AGE,
    /** String property Bidi_Mirroring_Glyph.
        Corresponds to u_charMirror. @stable ICU 2.4 */
    UCHAR_BIDI_MIRRORING_GLYPH = 0x4001,
    /** String property Case_Folding.
        Corresponds to u_strFoldCase in ustring.h. @stable ICU 2.4 */
    UCHAR_CASE_FOLDING = 0x4002,
#ifndef U_HIDE_DEPRECATED_API
    /** Deprecated string property ISO_Comment.
        Corresponds to u_getISOComment. @deprecated ICU 49 */
    UCHAR_ISO_COMMENT = 0x4003,
#endif /* U_HIDE_DEPRECATED_API */
    /** String property Lowercase_Mapping.
        Corresponds to u_strToLower in ustring.h. @stable ICU 2.4 */
    UCHAR_LOWERCASE_MAPPING = 0x4004,
    /** String property Name.
        Corresponds to u_charName. @stable ICU 2.4 */
    UCHAR_NAME = 0x4005,
    /** String property Simple_Case_Folding.
        Corresponds to u_foldCase. @stable ICU 2.4 */
    UCHAR_SIMPLE_CASE_FOLDING = 0x4006,
    /** String property Simple_Lowercase_Mapping.
        Corresponds to u_tolower. @stable ICU 2.4 */
    UCHAR_SIMPLE_LOWERCASE_MAPPING = 0x4007,
    /** String property Simple_Titlecase_Mapping.
        Corresponds to u_totitle. @stable ICU 2.4 */
    UCHAR_SIMPLE_TITLECASE_MAPPING = 0x4008,
    /** String property Simple_Uppercase_Mapping.
        Corresponds to u_toupper. @stable ICU 2.4 */
    UCHAR_SIMPLE_UPPERCASE_MAPPING = 0x4009,
    /** String property Titlecase_Mapping.
        Corresponds to u_strToTitle in ustring.h. @stable ICU 2.4 */
    UCHAR_TITLECASE_MAPPING = 0x400A,
#ifndef U_HIDE_DEPRECATED_API
    /** String property Unicode_1_Name.
        This property is of little practical value.
        Beginning with ICU 49, ICU APIs return an empty string for this property.
        Corresponds to u_charName(U_UNICODE_10_CHAR_NAME). @deprecated ICU 49 */
    UCHAR_UNICODE_1_NAME = 0x400B,
#endif /* U_HIDE_DEPRECATED_API */
    /** String property Uppercase_Mapping.
        Corresponds to u_strToUpper in ustring.h. @stable ICU 2.4 */
    UCHAR_UPPERCASE_MAPPING = 0x400C,
    /** String property Bidi_Paired_Bracket (new in Unicode 6.3).
        Corresponds to u_getBidiPairedBracket. @stable ICU 52 */
    UCHAR_BIDI_PAIRED_BRACKET = 0x400D,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for string Unicode properties.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_STRING_LIMIT = 0x400E,
#endif // U_HIDE_DEPRECATED_API

    /** Miscellaneous property Script_Extensions (new in Unicode 6.0).
        Some characters are commonly used in multiple scripts.
        For more information, see UAX #24: http://www.unicode.org/reports/tr24/.
        Corresponds to uscript_hasScript and uscript_getScriptExtensions in uscript.h.
        @stable ICU 4.6 */
    UCHAR_SCRIPT_EXTENSIONS = 0x7000,
    /** First constant for Unicode properties with unusual value types. @stable ICU 4.6 */
    UCHAR_OTHER_PROPERTY_START = UCHAR_SCRIPT_EXTENSIONS,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the last constant for Unicode properties with unusual value types.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCHAR_OTHER_PROPERTY_LIMIT = 0x7001,
#endif // U_HIDE_DEPRECATED_API

    /** Represents a nonexistent or invalid property or property value. @stable ICU 2.4 */
    UCHAR_INVALID_CODE = -1
} UProperty;

#define U_MASK(x) ((uint32_t)1 << (x))
#define U_GET_GC_MASK(c) U_MASK(u_charType(c))

/**
 * Data for enumerated Unicode general category types.
 * See http://www.unicode.org/Public/UNIDATA/UnicodeData.html .
 * @stable ICU 2.0
 */
typedef enum UCharCategory {
    /*
     * Note: UCharCategory constants and their API comments are parsed by preparseucd.py.
     * It matches pairs of lines like
     *     / ** <Unicode 2-letter General_Category value> comment... * /
     *     U_<[A-Z_]+> = <integer>,
     */

    /** Non-category for unassigned and non-character code points. @stable ICU 2.0 */
    U_UNASSIGNED = 0,
    /** Cn "Other, Not Assigned (no characters in [UnicodeData.txt] have this property)" (same as U_UNASSIGNED!) @stable ICU 2.0 */
    U_GENERAL_OTHER_TYPES = 0,
    /** Lu @stable ICU 2.0 */
    U_UPPERCASE_LETTER = 1,
    /** Ll @stable ICU 2.0 */
    U_LOWERCASE_LETTER = 2,
    /** Lt @stable ICU 2.0 */
    U_TITLECASE_LETTER = 3,
    /** Lm @stable ICU 2.0 */
    U_MODIFIER_LETTER = 4,
    /** Lo @stable ICU 2.0 */
    U_OTHER_LETTER = 5,
    /** Mn @stable ICU 2.0 */
    U_NON_SPACING_MARK = 6,
    /** Me @stable ICU 2.0 */
    U_ENCLOSING_MARK = 7,
    /** Mc @stable ICU 2.0 */
    U_COMBINING_SPACING_MARK = 8,
    /** Nd @stable ICU 2.0 */
    U_DECIMAL_DIGIT_NUMBER = 9,
    /** Nl @stable ICU 2.0 */
    U_LETTER_NUMBER = 10,
    /** No @stable ICU 2.0 */
    U_OTHER_NUMBER = 11,
    /** Zs @stable ICU 2.0 */
    U_SPACE_SEPARATOR = 12,
    /** Zl @stable ICU 2.0 */
    U_LINE_SEPARATOR = 13,
    /** Zp @stable ICU 2.0 */
    U_PARAGRAPH_SEPARATOR = 14,
    /** Cc @stable ICU 2.0 */
    U_CONTROL_CHAR = 15,
    /** Cf @stable ICU 2.0 */
    U_FORMAT_CHAR = 16,
    /** Co @stable ICU 2.0 */
    U_PRIVATE_USE_CHAR = 17,
    /** Cs @stable ICU 2.0 */
    U_SURROGATE = 18,
    /** Pd @stable ICU 2.0 */
    U_DASH_PUNCTUATION = 19,
    /** Ps @stable ICU 2.0 */
    U_START_PUNCTUATION = 20,
    /** Pe @stable ICU 2.0 */
    U_END_PUNCTUATION = 21,
    /** Pc @stable ICU 2.0 */
    U_CONNECTOR_PUNCTUATION = 22,
    /** Po @stable ICU 2.0 */
    U_OTHER_PUNCTUATION = 23,
    /** Sm @stable ICU 2.0 */
    U_MATH_SYMBOL = 24,
    /** Sc @stable ICU 2.0 */
    U_CURRENCY_SYMBOL = 25,
    /** Sk @stable ICU 2.0 */
    U_MODIFIER_SYMBOL = 26,
    /** So @stable ICU 2.0 */
    U_OTHER_SYMBOL = 27,
    /** Pi @stable ICU 2.0 */
    U_INITIAL_PUNCTUATION = 28,
    /** Pf @stable ICU 2.0 */
    U_FINAL_PUNCTUATION = 29,
    /**
     * One higher than the last enum UCharCategory constant.
     * This numeric value is stable (will not change), see
     * http://www.unicode.org/policies/stability_policy.html#Property_Value
     *
     * @stable ICU 2.0
     */
    U_CHAR_CATEGORY_COUNT
} UCharCategory;

/**
 * U_GC_XX_MASK constants are bit flags corresponding to Unicode
 * general category values.
 * For each category, the nth bit is set if the numeric value of the
 * corresponding UCharCategory constant is n.
 *
 * There are also some U_GC_Y_MASK constants for groups of general categories
 * like L for all letter categories.
 *
 * @see u_charType
 * @see U_GET_GC_MASK
 * @see UCharCategory
 * @stable ICU 2.1
 */
#define U_GC_CN_MASK U_MASK(U_GENERAL_OTHER_TYPES)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_LU_MASK U_MASK(U_UPPERCASE_LETTER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_LL_MASK U_MASK(U_LOWERCASE_LETTER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_LT_MASK U_MASK(U_TITLECASE_LETTER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_LM_MASK U_MASK(U_MODIFIER_LETTER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_LO_MASK U_MASK(U_OTHER_LETTER)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_MN_MASK U_MASK(U_NON_SPACING_MARK)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_ME_MASK U_MASK(U_ENCLOSING_MARK)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_MC_MASK U_MASK(U_COMBINING_SPACING_MARK)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_ND_MASK U_MASK(U_DECIMAL_DIGIT_NUMBER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_NL_MASK U_MASK(U_LETTER_NUMBER)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_NO_MASK U_MASK(U_OTHER_NUMBER)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_ZS_MASK U_MASK(U_SPACE_SEPARATOR)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_ZL_MASK U_MASK(U_LINE_SEPARATOR)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_ZP_MASK U_MASK(U_PARAGRAPH_SEPARATOR)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_CC_MASK U_MASK(U_CONTROL_CHAR)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_CF_MASK U_MASK(U_FORMAT_CHAR)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_CO_MASK U_MASK(U_PRIVATE_USE_CHAR)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_CS_MASK U_MASK(U_SURROGATE)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PD_MASK U_MASK(U_DASH_PUNCTUATION)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PS_MASK U_MASK(U_START_PUNCTUATION)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PE_MASK U_MASK(U_END_PUNCTUATION)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PC_MASK U_MASK(U_CONNECTOR_PUNCTUATION)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PO_MASK U_MASK(U_OTHER_PUNCTUATION)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_SM_MASK U_MASK(U_MATH_SYMBOL)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_SC_MASK U_MASK(U_CURRENCY_SYMBOL)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_SK_MASK U_MASK(U_MODIFIER_SYMBOL)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_SO_MASK U_MASK(U_OTHER_SYMBOL)

/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PI_MASK U_MASK(U_INITIAL_PUNCTUATION)
/** Mask constant for a UCharCategory. @stable ICU 2.1 */
#define U_GC_PF_MASK U_MASK(U_FINAL_PUNCTUATION)


/** Mask constant for multiple UCharCategory bits (L Letters). @stable ICU 2.1 */
#define U_GC_L_MASK \
    (U_GC_LU_MASK | U_GC_LL_MASK | U_GC_LT_MASK | U_GC_LM_MASK | U_GC_LO_MASK)

/** Mask constant for multiple UCharCategory bits (LC Cased Letters). @stable ICU 2.1 */
#define U_GC_LC_MASK \
    (U_GC_LU_MASK | U_GC_LL_MASK | U_GC_LT_MASK)

/** Mask constant for multiple UCharCategory bits (M Marks). @stable ICU 2.1 */
#define U_GC_M_MASK (U_GC_MN_MASK | U_GC_ME_MASK | U_GC_MC_MASK)

/** Mask constant for multiple UCharCategory bits (N Numbers). @stable ICU 2.1 */
#define U_GC_N_MASK (U_GC_ND_MASK | U_GC_NL_MASK | U_GC_NO_MASK)

/** Mask constant for multiple UCharCategory bits (Z Separators). @stable ICU 2.1 */
#define U_GC_Z_MASK (U_GC_ZS_MASK | U_GC_ZL_MASK | U_GC_ZP_MASK)

/** Mask constant for multiple UCharCategory bits (C Others). @stable ICU 2.1 */
#define U_GC_C_MASK \
    (U_GC_CN_MASK | U_GC_CC_MASK | U_GC_CF_MASK | U_GC_CO_MASK | U_GC_CS_MASK)

/** Mask constant for multiple UCharCategory bits (P Punctuation). @stable ICU 2.1 */
#define U_GC_P_MASK \
    (U_GC_PD_MASK | U_GC_PS_MASK | U_GC_PE_MASK | U_GC_PC_MASK | U_GC_PO_MASK | U_GC_PI_MASK | U_GC_PF_MASK)

/** Mask constant for multiple UCharCategory bits (S Symbols). @stable ICU 2.1 */
#define U_GC_S_MASK (U_GC_SM_MASK | U_GC_SC_MASK | U_GC_SK_MASK | U_GC_SO_MASK)

/**
 * This specifies the language directional property of a character set.
 * @stable ICU 2.0
 */
typedef enum UCharDirection {
    /*
     * Note: UCharDirection constants and their API comments are parsed by preparseucd.py.
     * It matches pairs of lines like
     *     / ** <Unicode 1..3-letter Bidi_Class value> comment... * /
     *     U_<[A-Z_]+> = <integer>,
     */

    /** L @stable ICU 2.0 */
    U_LEFT_TO_RIGHT = 0,
    /** R @stable ICU 2.0 */
    U_RIGHT_TO_LEFT = 1,
    /** EN @stable ICU 2.0 */
    U_EUROPEAN_NUMBER = 2,
    /** ES @stable ICU 2.0 */
    U_EUROPEAN_NUMBER_SEPARATOR = 3,
    /** ET @stable ICU 2.0 */
    U_EUROPEAN_NUMBER_TERMINATOR = 4,
    /** AN @stable ICU 2.0 */
    U_ARABIC_NUMBER = 5,
    /** CS @stable ICU 2.0 */
    U_COMMON_NUMBER_SEPARATOR = 6,
    /** B @stable ICU 2.0 */
    U_BLOCK_SEPARATOR = 7,
    /** S @stable ICU 2.0 */
    U_SEGMENT_SEPARATOR = 8,
    /** WS @stable ICU 2.0 */
    U_WHITE_SPACE_NEUTRAL = 9,
    /** ON @stable ICU 2.0 */
    U_OTHER_NEUTRAL = 10,
    /** LRE @stable ICU 2.0 */
    U_LEFT_TO_RIGHT_EMBEDDING = 11,
    /** LRO @stable ICU 2.0 */
    U_LEFT_TO_RIGHT_OVERRIDE = 12,
    /** AL @stable ICU 2.0 */
    U_RIGHT_TO_LEFT_ARABIC = 13,
    /** RLE @stable ICU 2.0 */
    U_RIGHT_TO_LEFT_EMBEDDING = 14,
    /** RLO @stable ICU 2.0 */
    U_RIGHT_TO_LEFT_OVERRIDE = 15,
    /** PDF @stable ICU 2.0 */
    U_POP_DIRECTIONAL_FORMAT = 16,
    /** NSM @stable ICU 2.0 */
    U_DIR_NON_SPACING_MARK = 17,
    /** BN @stable ICU 2.0 */
    U_BOUNDARY_NEUTRAL = 18,
    /** FSI @stable ICU 52 */
    U_FIRST_STRONG_ISOLATE = 19,
    /** LRI @stable ICU 52 */
    U_LEFT_TO_RIGHT_ISOLATE = 20,
    /** RLI @stable ICU 52 */
    U_RIGHT_TO_LEFT_ISOLATE = 21,
    /** PDI @stable ICU 52 */
    U_POP_DIRECTIONAL_ISOLATE = 22,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest UCharDirection value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_BIDI_CLASS).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_CHAR_DIRECTION_COUNT
#endif // U_HIDE_DEPRECATED_API
} UCharDirection;

/**
 * Bidi Paired Bracket Type constants.
 *
 * @see UCHAR_BIDI_PAIRED_BRACKET_TYPE
 * @stable ICU 52
 */
typedef enum UBidiPairedBracketType {
    /*
     * Note: UBidiPairedBracketType constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_BPT_<Unicode Bidi_Paired_Bracket_Type value name>
     */

    /** Not a paired bracket. @stable ICU 52 */
    U_BPT_NONE,
    /** Open paired bracket. @stable ICU 52 */
    U_BPT_OPEN,
    /** Close paired bracket. @stable ICU 52 */
    U_BPT_CLOSE,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UBidiPairedBracketType value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_BIDI_PAIRED_BRACKET_TYPE).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_BPT_COUNT /* 3 */
#endif // U_HIDE_DEPRECATED_API
} UBidiPairedBracketType;

/**
 * Constants for Unicode blocks, see the Unicode Data file Blocks.txt
 * @stable ICU 2.0
 */
enum UBlockCode {
    /*
     * Note: UBlockCode constants are parsed by preparseucd.py.
     * It matches lines like
     *     UBLOCK_<Unicode Block value name> = <integer>,
     */

    /** New No_Block value in Unicode 4. @stable ICU 2.6 */
    UBLOCK_NO_BLOCK = 0,
    /*[none]*/ /* Special range indicating No_Block */

    /** @stable ICU 2.0 */
    UBLOCK_BASIC_LATIN
    = 1, /*[0000]*/

    /** @stable ICU 2.0 */
    UBLOCK_LATIN_1_SUPPLEMENT = 2, /*[0080]*/

    /** @stable ICU 2.0 */
    UBLOCK_LATIN_EXTENDED_A = 3, /*[0100]*/

    /** @stable ICU 2.0 */
    UBLOCK_LATIN_EXTENDED_B = 4, /*[0180]*/

    /** @stable ICU 2.0 */
    UBLOCK_IPA_EXTENSIONS = 5, /*[0250]*/

    /** @stable ICU 2.0 */
    UBLOCK_SPACING_MODIFIER_LETTERS = 6, /*[02B0]*/

    /** @stable ICU 2.0 */
    UBLOCK_COMBINING_DIACRITICAL_MARKS = 7, /*[0300]*/

    /**
     * Unicode 3.2 renames this block to "Greek and Coptic".
     * @stable ICU 2.0
     */
    UBLOCK_GREEK = 8, /*[0370]*/

    /** @stable ICU 2.0 */
    UBLOCK_CYRILLIC = 9, /*[0400]*/

    /** @stable ICU 2.0 */
    UBLOCK_ARMENIAN = 10, /*[0530]*/

    /** @stable ICU 2.0 */
    UBLOCK_HEBREW = 11, /*[0590]*/

    /** @stable ICU 2.0 */
    UBLOCK_ARABIC = 12, /*[0600]*/

    /** @stable ICU 2.0 */
    UBLOCK_SYRIAC = 13, /*[0700]*/

    /** @stable ICU 2.0 */
    UBLOCK_THAANA = 14, /*[0780]*/

    /** @stable ICU 2.0 */
    UBLOCK_DEVANAGARI = 15, /*[0900]*/

    /** @stable ICU 2.0 */
    UBLOCK_BENGALI = 16, /*[0980]*/

    /** @stable ICU 2.0 */
    UBLOCK_GURMUKHI = 17, /*[0A00]*/

    /** @stable ICU 2.0 */
    UBLOCK_GUJARATI = 18, /*[0A80]*/

    /** @stable ICU 2.0 */
    UBLOCK_ORIYA = 19, /*[0B00]*/

    /** @stable ICU 2.0 */
    UBLOCK_TAMIL = 20, /*[0B80]*/

    /** @stable ICU 2.0 */
    UBLOCK_TELUGU = 21, /*[0C00]*/

    /** @stable ICU 2.0 */
    UBLOCK_KANNADA = 22, /*[0C80]*/

    /** @stable ICU 2.0 */
    UBLOCK_MALAYALAM = 23, /*[0D00]*/

    /** @stable ICU 2.0 */
    UBLOCK_SINHALA = 24, /*[0D80]*/

    /** @stable ICU 2.0 */
    UBLOCK_THAI = 25, /*[0E00]*/

    /** @stable ICU 2.0 */
    UBLOCK_LAO = 26, /*[0E80]*/

    /** @stable ICU 2.0 */
    UBLOCK_TIBETAN = 27, /*[0F00]*/

    /** @stable ICU 2.0 */
    UBLOCK_MYANMAR = 28, /*[1000]*/

    /** @stable ICU 2.0 */
    UBLOCK_GEORGIAN = 29, /*[10A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_HANGUL_JAMO = 30, /*[1100]*/

    /** @stable ICU 2.0 */
    UBLOCK_ETHIOPIC = 31, /*[1200]*/

    /** @stable ICU 2.0 */
    UBLOCK_CHEROKEE = 32, /*[13A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS = 33, /*[1400]*/

    /** @stable ICU 2.0 */
    UBLOCK_OGHAM = 34, /*[1680]*/

    /** @stable ICU 2.0 */
    UBLOCK_RUNIC = 35, /*[16A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_KHMER = 36, /*[1780]*/

    /** @stable ICU 2.0 */
    UBLOCK_MONGOLIAN = 37, /*[1800]*/

    /** @stable ICU 2.0 */
    UBLOCK_LATIN_EXTENDED_ADDITIONAL = 38, /*[1E00]*/

    /** @stable ICU 2.0 */
    UBLOCK_GREEK_EXTENDED = 39, /*[1F00]*/

    /** @stable ICU 2.0 */
    UBLOCK_GENERAL_PUNCTUATION = 40, /*[2000]*/

    /** @stable ICU 2.0 */
    UBLOCK_SUPERSCRIPTS_AND_SUBSCRIPTS = 41, /*[2070]*/

    /** @stable ICU 2.0 */
    UBLOCK_CURRENCY_SYMBOLS = 42, /*[20A0]*/

    /**
     * Unicode 3.2 renames this block to "Combining Diacritical Marks for Symbols".
     * @stable ICU 2.0
     */
    UBLOCK_COMBINING_MARKS_FOR_SYMBOLS = 43, /*[20D0]*/

    /** @stable ICU 2.0 */
    UBLOCK_LETTERLIKE_SYMBOLS = 44, /*[2100]*/

    /** @stable ICU 2.0 */
    UBLOCK_NUMBER_FORMS = 45, /*[2150]*/

    /** @stable ICU 2.0 */
    UBLOCK_ARROWS = 46, /*[2190]*/

    /** @stable ICU 2.0 */
    UBLOCK_MATHEMATICAL_OPERATORS = 47, /*[2200]*/

    /** @stable ICU 2.0 */
    UBLOCK_MISCELLANEOUS_TECHNICAL = 48, /*[2300]*/

    /** @stable ICU 2.0 */
    UBLOCK_CONTROL_PICTURES = 49, /*[2400]*/

    /** @stable ICU 2.0 */
    UBLOCK_OPTICAL_CHARACTER_RECOGNITION = 50, /*[2440]*/

    /** @stable ICU 2.0 */
    UBLOCK_ENCLOSED_ALPHANUMERICS = 51, /*[2460]*/

    /** @stable ICU 2.0 */
    UBLOCK_BOX_DRAWING = 52, /*[2500]*/

    /** @stable ICU 2.0 */
    UBLOCK_BLOCK_ELEMENTS = 53, /*[2580]*/

    /** @stable ICU 2.0 */
    UBLOCK_GEOMETRIC_SHAPES = 54, /*[25A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_MISCELLANEOUS_SYMBOLS = 55, /*[2600]*/

    /** @stable ICU 2.0 */
    UBLOCK_DINGBATS = 56, /*[2700]*/

    /** @stable ICU 2.0 */
    UBLOCK_BRAILLE_PATTERNS = 57, /*[2800]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_RADICALS_SUPPLEMENT = 58, /*[2E80]*/

    /** @stable ICU 2.0 */
    UBLOCK_KANGXI_RADICALS = 59, /*[2F00]*/

    /** @stable ICU 2.0 */
    UBLOCK_IDEOGRAPHIC_DESCRIPTION_CHARACTERS = 60, /*[2FF0]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION = 61, /*[3000]*/

    /** @stable ICU 2.0 */
    UBLOCK_HIRAGANA = 62, /*[3040]*/

    /** @stable ICU 2.0 */
    UBLOCK_KATAKANA = 63, /*[30A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_BOPOMOFO = 64, /*[3100]*/

    /** @stable ICU 2.0 */
    UBLOCK_HANGUL_COMPATIBILITY_JAMO = 65, /*[3130]*/

    /** @stable ICU 2.0 */
    UBLOCK_KANBUN = 66, /*[3190]*/

    /** @stable ICU 2.0 */
    UBLOCK_BOPOMOFO_EXTENDED = 67, /*[31A0]*/

    /** @stable ICU 2.0 */
    UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS = 68, /*[3200]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_COMPATIBILITY = 69, /*[3300]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A = 70, /*[3400]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS = 71, /*[4E00]*/

    /** @stable ICU 2.0 */
    UBLOCK_YI_SYLLABLES = 72, /*[A000]*/

    /** @stable ICU 2.0 */
    UBLOCK_YI_RADICALS = 73, /*[A490]*/

    /** @stable ICU 2.0 */
    UBLOCK_HANGUL_SYLLABLES = 74, /*[AC00]*/

    /** @stable ICU 2.0 */
    UBLOCK_HIGH_SURROGATES = 75, /*[D800]*/

    /** @stable ICU 2.0 */
    UBLOCK_HIGH_PRIVATE_USE_SURROGATES = 76, /*[DB80]*/

    /** @stable ICU 2.0 */
    UBLOCK_LOW_SURROGATES = 77, /*[DC00]*/

    /**
     * Same as UBLOCK_PRIVATE_USE.
     * Until Unicode 3.1.1, the corresponding block name was "Private Use",
     * and multiple code point ranges had this block.
     * Unicode 3.2 renames the block for the BMP PUA to "Private Use Area" and
     * adds separate blocks for the supplementary PUAs.
     *
     * @stable ICU 2.0
     */
    UBLOCK_PRIVATE_USE_AREA = 78, /*[E000]*/
    /**
     * Same as UBLOCK_PRIVATE_USE_AREA.
     * Until Unicode 3.1.1, the corresponding block name was "Private Use",
     * and multiple code point ranges had this block.
     * Unicode 3.2 renames the block for the BMP PUA to "Private Use Area" and
     * adds separate blocks for the supplementary PUAs.
     *
     * @stable ICU 2.0
     */
    UBLOCK_PRIVATE_USE = UBLOCK_PRIVATE_USE_AREA,

    /** @stable ICU 2.0 */
    UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS = 79, /*[F900]*/

    /** @stable ICU 2.0 */
    UBLOCK_ALPHABETIC_PRESENTATION_FORMS = 80, /*[FB00]*/

    /** @stable ICU 2.0 */
    UBLOCK_ARABIC_PRESENTATION_FORMS_A = 81, /*[FB50]*/

    /** @stable ICU 2.0 */
    UBLOCK_COMBINING_HALF_MARKS = 82, /*[FE20]*/

    /** @stable ICU 2.0 */
    UBLOCK_CJK_COMPATIBILITY_FORMS = 83, /*[FE30]*/

    /** @stable ICU 2.0 */
    UBLOCK_SMALL_FORM_VARIANTS = 84, /*[FE50]*/

    /** @stable ICU 2.0 */
    UBLOCK_ARABIC_PRESENTATION_FORMS_B = 85, /*[FE70]*/

    /** @stable ICU 2.0 */
    UBLOCK_SPECIALS = 86, /*[FFF0]*/

    /** @stable ICU 2.0 */
    UBLOCK_HALFWIDTH_AND_FULLWIDTH_FORMS = 87, /*[FF00]*/

    /* New blocks in Unicode 3.1 */

    /** @stable ICU 2.0 */
    UBLOCK_OLD_ITALIC = 88, /*[10300]*/
    /** @stable ICU 2.0 */
    UBLOCK_GOTHIC = 89, /*[10330]*/
    /** @stable ICU 2.0 */
    UBLOCK_DESERET = 90, /*[10400]*/
    /** @stable ICU 2.0 */
    UBLOCK_BYZANTINE_MUSICAL_SYMBOLS = 91, /*[1D000]*/
    /** @stable ICU 2.0 */
    UBLOCK_MUSICAL_SYMBOLS = 92, /*[1D100]*/
    /** @stable ICU 2.0 */
    UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS = 93, /*[1D400]*/
    /** @stable ICU 2.0 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B = 94, /*[20000]*/
    /** @stable ICU 2.0 */
    UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT = 95, /*[2F800]*/
    /** @stable ICU 2.0 */
    UBLOCK_TAGS = 96, /*[E0000]*/

    /* New blocks in Unicode 3.2 */

    /** @stable ICU 3.0  */
    UBLOCK_CYRILLIC_SUPPLEMENT = 97, /*[0500]*/
    /**
     * Unicode 4.0.1 renames the "Cyrillic Supplementary" block to "Cyrillic Supplement".
     * @stable ICU 2.2
     */
    UBLOCK_CYRILLIC_SUPPLEMENTARY = UBLOCK_CYRILLIC_SUPPLEMENT,
    /** @stable ICU 2.2 */
    UBLOCK_TAGALOG = 98, /*[1700]*/
    /** @stable ICU 2.2 */
    UBLOCK_HANUNOO = 99, /*[1720]*/
    /** @stable ICU 2.2 */
    UBLOCK_BUHID = 100, /*[1740]*/
    /** @stable ICU 2.2 */
    UBLOCK_TAGBANWA = 101, /*[1760]*/
    /** @stable ICU 2.2 */
    UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A = 102, /*[27C0]*/
    /** @stable ICU 2.2 */
    UBLOCK_SUPPLEMENTAL_ARROWS_A = 103, /*[27F0]*/
    /** @stable ICU 2.2 */
    UBLOCK_SUPPLEMENTAL_ARROWS_B = 104, /*[2900]*/
    /** @stable ICU 2.2 */
    UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B = 105, /*[2980]*/
    /** @stable ICU 2.2 */
    UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS = 106, /*[2A00]*/
    /** @stable ICU 2.2 */
    UBLOCK_KATAKANA_PHONETIC_EXTENSIONS = 107, /*[31F0]*/
    /** @stable ICU 2.2 */
    UBLOCK_VARIATION_SELECTORS = 108, /*[FE00]*/
    /** @stable ICU 2.2 */
    UBLOCK_SUPPLEMENTARY_PRIVATE_USE_AREA_A = 109, /*[F0000]*/
    /** @stable ICU 2.2 */
    UBLOCK_SUPPLEMENTARY_PRIVATE_USE_AREA_B = 110, /*[100000]*/

    /* New blocks in Unicode 4 */

    /** @stable ICU 2.6 */
    UBLOCK_LIMBU = 111, /*[1900]*/
    /** @stable ICU 2.6 */
    UBLOCK_TAI_LE = 112, /*[1950]*/
    /** @stable ICU 2.6 */
    UBLOCK_KHMER_SYMBOLS = 113, /*[19E0]*/
    /** @stable ICU 2.6 */
    UBLOCK_PHONETIC_EXTENSIONS = 114, /*[1D00]*/
    /** @stable ICU 2.6 */
    UBLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS = 115, /*[2B00]*/
    /** @stable ICU 2.6 */
    UBLOCK_YIJING_HEXAGRAM_SYMBOLS = 116, /*[4DC0]*/
    /** @stable ICU 2.6 */
    UBLOCK_LINEAR_B_SYLLABARY = 117, /*[10000]*/
    /** @stable ICU 2.6 */
    UBLOCK_LINEAR_B_IDEOGRAMS = 118, /*[10080]*/
    /** @stable ICU 2.6 */
    UBLOCK_AEGEAN_NUMBERS = 119, /*[10100]*/
    /** @stable ICU 2.6 */
    UBLOCK_UGARITIC = 120, /*[10380]*/
    /** @stable ICU 2.6 */
    UBLOCK_SHAVIAN = 121, /*[10450]*/
    /** @stable ICU 2.6 */
    UBLOCK_OSMANYA = 122, /*[10480]*/
    /** @stable ICU 2.6 */
    UBLOCK_CYPRIOT_SYLLABARY = 123, /*[10800]*/
    /** @stable ICU 2.6 */
    UBLOCK_TAI_XUAN_JING_SYMBOLS = 124, /*[1D300]*/
    /** @stable ICU 2.6 */
    UBLOCK_VARIATION_SELECTORS_SUPPLEMENT = 125, /*[E0100]*/

    /* New blocks in Unicode 4.1 */

    /** @stable ICU 3.4 */
    UBLOCK_ANCIENT_GREEK_MUSICAL_NOTATION = 126, /*[1D200]*/
    /** @stable ICU 3.4 */
    UBLOCK_ANCIENT_GREEK_NUMBERS = 127, /*[10140]*/
    /** @stable ICU 3.4 */
    UBLOCK_ARABIC_SUPPLEMENT = 128, /*[0750]*/
    /** @stable ICU 3.4 */
    UBLOCK_BUGINESE = 129, /*[1A00]*/
    /** @stable ICU 3.4 */
    UBLOCK_CJK_STROKES = 130, /*[31C0]*/
    /** @stable ICU 3.4 */
    UBLOCK_COMBINING_DIACRITICAL_MARKS_SUPPLEMENT = 131, /*[1DC0]*/
    /** @stable ICU 3.4 */
    UBLOCK_COPTIC = 132, /*[2C80]*/
    /** @stable ICU 3.4 */
    UBLOCK_ETHIOPIC_EXTENDED = 133, /*[2D80]*/
    /** @stable ICU 3.4 */
    UBLOCK_ETHIOPIC_SUPPLEMENT = 134, /*[1380]*/
    /** @stable ICU 3.4 */
    UBLOCK_GEORGIAN_SUPPLEMENT = 135, /*[2D00]*/
    /** @stable ICU 3.4 */
    UBLOCK_GLAGOLITIC = 136, /*[2C00]*/
    /** @stable ICU 3.4 */
    UBLOCK_KHAROSHTHI = 137, /*[10A00]*/
    /** @stable ICU 3.4 */
    UBLOCK_MODIFIER_TONE_LETTERS = 138, /*[A700]*/
    /** @stable ICU 3.4 */
    UBLOCK_NEW_TAI_LUE = 139, /*[1980]*/
    /** @stable ICU 3.4 */
    UBLOCK_OLD_PERSIAN = 140, /*[103A0]*/
    /** @stable ICU 3.4 */
    UBLOCK_PHONETIC_EXTENSIONS_SUPPLEMENT = 141, /*[1D80]*/
    /** @stable ICU 3.4 */
    UBLOCK_SUPPLEMENTAL_PUNCTUATION = 142, /*[2E00]*/
    /** @stable ICU 3.4 */
    UBLOCK_SYLOTI_NAGRI = 143, /*[A800]*/
    /** @stable ICU 3.4 */
    UBLOCK_TIFINAGH = 144, /*[2D30]*/
    /** @stable ICU 3.4 */
    UBLOCK_VERTICAL_FORMS = 145, /*[FE10]*/

    /* New blocks in Unicode 5.0 */

    /** @stable ICU 3.6 */
    UBLOCK_NKO = 146, /*[07C0]*/
    /** @stable ICU 3.6 */
    UBLOCK_BALINESE = 147, /*[1B00]*/
    /** @stable ICU 3.6 */
    UBLOCK_LATIN_EXTENDED_C = 148, /*[2C60]*/
    /** @stable ICU 3.6 */
    UBLOCK_LATIN_EXTENDED_D = 149, /*[A720]*/
    /** @stable ICU 3.6 */
    UBLOCK_PHAGS_PA = 150, /*[A840]*/
    /** @stable ICU 3.6 */
    UBLOCK_PHOENICIAN = 151, /*[10900]*/
    /** @stable ICU 3.6 */
    UBLOCK_CUNEIFORM = 152, /*[12000]*/
    /** @stable ICU 3.6 */
    UBLOCK_CUNEIFORM_NUMBERS_AND_PUNCTUATION = 153, /*[12400]*/
    /** @stable ICU 3.6 */
    UBLOCK_COUNTING_ROD_NUMERALS = 154, /*[1D360]*/

    /* New blocks in Unicode 5.1 */

    /** @stable ICU 4.0 */
    UBLOCK_SUNDANESE = 155, /*[1B80]*/
    /** @stable ICU 4.0 */
    UBLOCK_LEPCHA = 156, /*[1C00]*/
    /** @stable ICU 4.0 */
    UBLOCK_OL_CHIKI = 157, /*[1C50]*/
    /** @stable ICU 4.0 */
    UBLOCK_CYRILLIC_EXTENDED_A = 158, /*[2DE0]*/
    /** @stable ICU 4.0 */
    UBLOCK_VAI = 159, /*[A500]*/
    /** @stable ICU 4.0 */
    UBLOCK_CYRILLIC_EXTENDED_B = 160, /*[A640]*/
    /** @stable ICU 4.0 */
    UBLOCK_SAURASHTRA = 161, /*[A880]*/
    /** @stable ICU 4.0 */
    UBLOCK_KAYAH_LI = 162, /*[A900]*/
    /** @stable ICU 4.0 */
    UBLOCK_REJANG = 163, /*[A930]*/
    /** @stable ICU 4.0 */
    UBLOCK_CHAM = 164, /*[AA00]*/
    /** @stable ICU 4.0 */
    UBLOCK_ANCIENT_SYMBOLS = 165, /*[10190]*/
    /** @stable ICU 4.0 */
    UBLOCK_PHAISTOS_DISC = 166, /*[101D0]*/
    /** @stable ICU 4.0 */
    UBLOCK_LYCIAN = 167, /*[10280]*/
    /** @stable ICU 4.0 */
    UBLOCK_CARIAN = 168, /*[102A0]*/
    /** @stable ICU 4.0 */
    UBLOCK_LYDIAN = 169, /*[10920]*/
    /** @stable ICU 4.0 */
    UBLOCK_MAHJONG_TILES = 170, /*[1F000]*/
    /** @stable ICU 4.0 */
    UBLOCK_DOMINO_TILES = 171, /*[1F030]*/

    /* New blocks in Unicode 5.2 */

    /** @stable ICU 4.4 */
    UBLOCK_SAMARITAN = 172, /*[0800]*/
    /** @stable ICU 4.4 */
    UBLOCK_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS_EXTENDED = 173, /*[18B0]*/
    /** @stable ICU 4.4 */
    UBLOCK_TAI_THAM = 174, /*[1A20]*/
    /** @stable ICU 4.4 */
    UBLOCK_VEDIC_EXTENSIONS = 175, /*[1CD0]*/
    /** @stable ICU 4.4 */
    UBLOCK_LISU = 176, /*[A4D0]*/
    /** @stable ICU 4.4 */
    UBLOCK_BAMUM = 177, /*[A6A0]*/
    /** @stable ICU 4.4 */
    UBLOCK_COMMON_INDIC_NUMBER_FORMS = 178, /*[A830]*/
    /** @stable ICU 4.4 */
    UBLOCK_DEVANAGARI_EXTENDED = 179, /*[A8E0]*/
    /** @stable ICU 4.4 */
    UBLOCK_HANGUL_JAMO_EXTENDED_A = 180, /*[A960]*/
    /** @stable ICU 4.4 */
    UBLOCK_JAVANESE = 181, /*[A980]*/
    /** @stable ICU 4.4 */
    UBLOCK_MYANMAR_EXTENDED_A = 182, /*[AA60]*/
    /** @stable ICU 4.4 */
    UBLOCK_TAI_VIET = 183, /*[AA80]*/
    /** @stable ICU 4.4 */
    UBLOCK_MEETEI_MAYEK = 184, /*[ABC0]*/
    /** @stable ICU 4.4 */
    UBLOCK_HANGUL_JAMO_EXTENDED_B = 185, /*[D7B0]*/
    /** @stable ICU 4.4 */
    UBLOCK_IMPERIAL_ARAMAIC = 186, /*[10840]*/
    /** @stable ICU 4.4 */
    UBLOCK_OLD_SOUTH_ARABIAN = 187, /*[10A60]*/
    /** @stable ICU 4.4 */
    UBLOCK_AVESTAN = 188, /*[10B00]*/
    /** @stable ICU 4.4 */
    UBLOCK_INSCRIPTIONAL_PARTHIAN = 189, /*[10B40]*/
    /** @stable ICU 4.4 */
    UBLOCK_INSCRIPTIONAL_PAHLAVI = 190, /*[10B60]*/
    /** @stable ICU 4.4 */
    UBLOCK_OLD_TURKIC = 191, /*[10C00]*/
    /** @stable ICU 4.4 */
    UBLOCK_RUMI_NUMERAL_SYMBOLS = 192, /*[10E60]*/
    /** @stable ICU 4.4 */
    UBLOCK_KAITHI = 193, /*[11080]*/
    /** @stable ICU 4.4 */
    UBLOCK_EGYPTIAN_HIEROGLYPHS = 194, /*[13000]*/
    /** @stable ICU 4.4 */
    UBLOCK_ENCLOSED_ALPHANUMERIC_SUPPLEMENT = 195, /*[1F100]*/
    /** @stable ICU 4.4 */
    UBLOCK_ENCLOSED_IDEOGRAPHIC_SUPPLEMENT = 196, /*[1F200]*/
    /** @stable ICU 4.4 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_C = 197, /*[2A700]*/

    /* New blocks in Unicode 6.0 */

    /** @stable ICU 4.6 */
    UBLOCK_MANDAIC = 198, /*[0840]*/
    /** @stable ICU 4.6 */
    UBLOCK_BATAK = 199, /*[1BC0]*/
    /** @stable ICU 4.6 */
    UBLOCK_ETHIOPIC_EXTENDED_A = 200, /*[AB00]*/
    /** @stable ICU 4.6 */
    UBLOCK_BRAHMI = 201, /*[11000]*/
    /** @stable ICU 4.6 */
    UBLOCK_BAMUM_SUPPLEMENT = 202, /*[16800]*/
    /** @stable ICU 4.6 */
    UBLOCK_KANA_SUPPLEMENT = 203, /*[1B000]*/
    /** @stable ICU 4.6 */
    UBLOCK_PLAYING_CARDS = 204, /*[1F0A0]*/
    /** @stable ICU 4.6 */
    UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS = 205, /*[1F300]*/
    /** @stable ICU 4.6 */
    UBLOCK_EMOTICONS = 206, /*[1F600]*/
    /** @stable ICU 4.6 */
    UBLOCK_TRANSPORT_AND_MAP_SYMBOLS = 207, /*[1F680]*/
    /** @stable ICU 4.6 */
    UBLOCK_ALCHEMICAL_SYMBOLS = 208, /*[1F700]*/
    /** @stable ICU 4.6 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_D = 209, /*[2B740]*/

    /* New blocks in Unicode 6.1 */

    /** @stable ICU 49 */
    UBLOCK_ARABIC_EXTENDED_A = 210, /*[08A0]*/
    /** @stable ICU 49 */
    UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS = 211, /*[1EE00]*/
    /** @stable ICU 49 */
    UBLOCK_CHAKMA = 212, /*[11100]*/
    /** @stable ICU 49 */
    UBLOCK_MEETEI_MAYEK_EXTENSIONS = 213, /*[AAE0]*/
    /** @stable ICU 49 */
    UBLOCK_MEROITIC_CURSIVE = 214, /*[109A0]*/
    /** @stable ICU 49 */
    UBLOCK_MEROITIC_HIEROGLYPHS = 215, /*[10980]*/
    /** @stable ICU 49 */
    UBLOCK_MIAO = 216, /*[16F00]*/
    /** @stable ICU 49 */
    UBLOCK_SHARADA = 217, /*[11180]*/
    /** @stable ICU 49 */
    UBLOCK_SORA_SOMPENG = 218, /*[110D0]*/
    /** @stable ICU 49 */
    UBLOCK_SUNDANESE_SUPPLEMENT = 219, /*[1CC0]*/
    /** @stable ICU 49 */
    UBLOCK_TAKRI = 220, /*[11680]*/

    /* New blocks in Unicode 7.0 */

    /** @stable ICU 54 */
    UBLOCK_BASSA_VAH = 221, /*[16AD0]*/
    /** @stable ICU 54 */
    UBLOCK_CAUCASIAN_ALBANIAN = 222, /*[10530]*/
    /** @stable ICU 54 */
    UBLOCK_COPTIC_EPACT_NUMBERS = 223, /*[102E0]*/
    /** @stable ICU 54 */
    UBLOCK_COMBINING_DIACRITICAL_MARKS_EXTENDED = 224, /*[1AB0]*/
    /** @stable ICU 54 */
    UBLOCK_DUPLOYAN = 225, /*[1BC00]*/
    /** @stable ICU 54 */
    UBLOCK_ELBASAN = 226, /*[10500]*/
    /** @stable ICU 54 */
    UBLOCK_GEOMETRIC_SHAPES_EXTENDED = 227, /*[1F780]*/
    /** @stable ICU 54 */
    UBLOCK_GRANTHA = 228, /*[11300]*/
    /** @stable ICU 54 */
    UBLOCK_KHOJKI = 229, /*[11200]*/
    /** @stable ICU 54 */
    UBLOCK_KHUDAWADI = 230, /*[112B0]*/
    /** @stable ICU 54 */
    UBLOCK_LATIN_EXTENDED_E = 231, /*[AB30]*/
    /** @stable ICU 54 */
    UBLOCK_LINEAR_A = 232, /*[10600]*/
    /** @stable ICU 54 */
    UBLOCK_MAHAJANI = 233, /*[11150]*/
    /** @stable ICU 54 */
    UBLOCK_MANICHAEAN = 234, /*[10AC0]*/
    /** @stable ICU 54 */
    UBLOCK_MENDE_KIKAKUI = 235, /*[1E800]*/
    /** @stable ICU 54 */
    UBLOCK_MODI = 236, /*[11600]*/
    /** @stable ICU 54 */
    UBLOCK_MRO = 237, /*[16A40]*/
    /** @stable ICU 54 */
    UBLOCK_MYANMAR_EXTENDED_B = 238, /*[A9E0]*/
    /** @stable ICU 54 */
    UBLOCK_NABATAEAN = 239, /*[10880]*/
    /** @stable ICU 54 */
    UBLOCK_OLD_NORTH_ARABIAN = 240, /*[10A80]*/
    /** @stable ICU 54 */
    UBLOCK_OLD_PERMIC = 241, /*[10350]*/
    /** @stable ICU 54 */
    UBLOCK_ORNAMENTAL_DINGBATS = 242, /*[1F650]*/
    /** @stable ICU 54 */
    UBLOCK_PAHAWH_HMONG = 243, /*[16B00]*/
    /** @stable ICU 54 */
    UBLOCK_PALMYRENE = 244, /*[10860]*/
    /** @stable ICU 54 */
    UBLOCK_PAU_CIN_HAU = 245, /*[11AC0]*/
    /** @stable ICU 54 */
    UBLOCK_PSALTER_PAHLAVI = 246, /*[10B80]*/
    /** @stable ICU 54 */
    UBLOCK_SHORTHAND_FORMAT_CONTROLS = 247, /*[1BCA0]*/
    /** @stable ICU 54 */
    UBLOCK_SIDDHAM = 248, /*[11580]*/
    /** @stable ICU 54 */
    UBLOCK_SINHALA_ARCHAIC_NUMBERS = 249, /*[111E0]*/
    /** @stable ICU 54 */
    UBLOCK_SUPPLEMENTAL_ARROWS_C = 250, /*[1F800]*/
    /** @stable ICU 54 */
    UBLOCK_TIRHUTA = 251, /*[11480]*/
    /** @stable ICU 54 */
    UBLOCK_WARANG_CITI = 252, /*[118A0]*/

    /* New blocks in Unicode 8.0 */

    /** @stable ICU 56 */
    UBLOCK_AHOM = 253, /*[11700]*/
    /** @stable ICU 56 */
    UBLOCK_ANATOLIAN_HIEROGLYPHS = 254, /*[14400]*/
    /** @stable ICU 56 */
    UBLOCK_CHEROKEE_SUPPLEMENT = 255, /*[AB70]*/
    /** @stable ICU 56 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_E = 256, /*[2B820]*/
    /** @stable ICU 56 */
    UBLOCK_EARLY_DYNASTIC_CUNEIFORM = 257, /*[12480]*/
    /** @stable ICU 56 */
    UBLOCK_HATRAN = 258, /*[108E0]*/
    /** @stable ICU 56 */
    UBLOCK_MULTANI = 259, /*[11280]*/
    /** @stable ICU 56 */
    UBLOCK_OLD_HUNGARIAN = 260, /*[10C80]*/
    /** @stable ICU 56 */
    UBLOCK_SUPPLEMENTAL_SYMBOLS_AND_PICTOGRAPHS = 261, /*[1F900]*/
    /** @stable ICU 56 */
    UBLOCK_SUTTON_SIGNWRITING = 262, /*[1D800]*/

    /* New blocks in Unicode 9.0 */

    /** @stable ICU 58 */
    UBLOCK_ADLAM = 263, /*[1E900]*/
    /** @stable ICU 58 */
    UBLOCK_BHAIKSUKI = 264, /*[11C00]*/
    /** @stable ICU 58 */
    UBLOCK_CYRILLIC_EXTENDED_C = 265, /*[1C80]*/
    /** @stable ICU 58 */
    UBLOCK_GLAGOLITIC_SUPPLEMENT = 266, /*[1E000]*/
    /** @stable ICU 58 */
    UBLOCK_IDEOGRAPHIC_SYMBOLS_AND_PUNCTUATION = 267, /*[16FE0]*/
    /** @stable ICU 58 */
    UBLOCK_MARCHEN = 268, /*[11C70]*/
    /** @stable ICU 58 */
    UBLOCK_MONGOLIAN_SUPPLEMENT = 269, /*[11660]*/
    /** @stable ICU 58 */
    UBLOCK_NEWA = 270, /*[11400]*/
    /** @stable ICU 58 */
    UBLOCK_OSAGE = 271, /*[104B0]*/
    /** @stable ICU 58 */
    UBLOCK_TANGUT = 272, /*[17000]*/
    /** @stable ICU 58 */
    UBLOCK_TANGUT_COMPONENTS = 273, /*[18800]*/

    // New blocks in Unicode 10.0

    /** @stable ICU 60 */
    UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_F = 274, /*[2CEB0]*/
    /** @stable ICU 60 */
    UBLOCK_KANA_EXTENDED_A = 275, /*[1B100]*/
    /** @stable ICU 60 */
    UBLOCK_MASARAM_GONDI = 276, /*[11D00]*/
    /** @stable ICU 60 */
    UBLOCK_NUSHU = 277, /*[1B170]*/
    /** @stable ICU 60 */
    UBLOCK_SOYOMBO = 278, /*[11A50]*/
    /** @stable ICU 60 */
    UBLOCK_SYRIAC_SUPPLEMENT = 279, /*[0860]*/
    /** @stable ICU 60 */
    UBLOCK_ZANABAZAR_SQUARE = 280, /*[11A00]*/

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UBlockCode value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_BLOCK).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UBLOCK_COUNT = 281,
#endif // U_HIDE_DEPRECATED_API

    /** @stable ICU 2.0 */
    UBLOCK_INVALID_CODE = -1
};

/** @stable ICU 2.0 */
typedef enum UBlockCode UBlockCode;

/**
 * East Asian Width constants.
 *
 * @see UCHAR_EAST_ASIAN_WIDTH
 * @see u_getIntPropertyValue
 * @stable ICU 2.2
 */
typedef enum UEastAsianWidth {
    /*
     * Note: UEastAsianWidth constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_EA_<Unicode East_Asian_Width value name>
     */

    U_EA_NEUTRAL, /*[N]*/
    U_EA_AMBIGUOUS, /*[A]*/
    U_EA_HALFWIDTH, /*[H]*/
    U_EA_FULLWIDTH, /*[F]*/
    U_EA_NARROW, /*[Na]*/
    U_EA_WIDE, /*[W]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UEastAsianWidth value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_EAST_ASIAN_WIDTH).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_EA_COUNT
#endif // U_HIDE_DEPRECATED_API
} UEastAsianWidth;

/**
 * Selector constants for u_charName().
 * u_charName() returns the "modern" name of a
 * Unicode character; or the name that was defined in
 * Unicode version 1.0, before the Unicode standard merged
 * with ISO-10646; or an "extended" name that gives each
 * Unicode code point a unique name.
 *
 * @see u_charName
 * @stable ICU 2.0
 */
typedef enum UCharNameChoice {
    /** Unicode character name (Name property). @stable ICU 2.0 */
    U_UNICODE_CHAR_NAME,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * The Unicode_1_Name property value which is of little practical value.
     * Beginning with ICU 49, ICU APIs return an empty string for this name choice.
     * @deprecated ICU 49
     */
    U_UNICODE_10_CHAR_NAME,
#endif /* U_HIDE_DEPRECATED_API */
    /** Standard or synthetic character name. @stable ICU 2.0 */
    U_EXTENDED_CHAR_NAME = U_UNICODE_CHAR_NAME + 2,
    /** Corrected name from NameAliases.txt. @stable ICU 4.4 */
    U_CHAR_NAME_ALIAS,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UCharNameChoice value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_CHAR_NAME_CHOICE_COUNT
#endif // U_HIDE_DEPRECATED_API
} UCharNameChoice;

/**
 * Selector constants for u_getPropertyName() and
 * u_getPropertyValueName().  These selectors are used to choose which
 * name is returned for a given property or value.  All properties and
 * values have a long name.  Most have a short name, but some do not.
 * Unicode allows for additional names, beyond the long and short
 * name, which would be indicated by U_LONG_PROPERTY_NAME + i, where
 * i=1, 2,...
 *
 * @see u_getPropertyName()
 * @see u_getPropertyValueName()
 * @stable ICU 2.4
 */
typedef enum UPropertyNameChoice {
    U_SHORT_PROPERTY_NAME,
    U_LONG_PROPERTY_NAME,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UPropertyNameChoice value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_PROPERTY_NAME_CHOICE_COUNT
#endif // U_HIDE_DEPRECATED_API
} UPropertyNameChoice;

/**
 * Decomposition Type constants.
 *
 * @see UCHAR_DECOMPOSITION_TYPE
 * @stable ICU 2.2
 */
typedef enum UDecompositionType {
    /*
     * Note: UDecompositionType constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_DT_<Unicode Decomposition_Type value name>
     */

    U_DT_NONE, /*[none]*/
    U_DT_CANONICAL, /*[can]*/
    U_DT_COMPAT, /*[com]*/
    U_DT_CIRCLE, /*[enc]*/
    U_DT_FINAL, /*[fin]*/
    U_DT_FONT, /*[font]*/
    U_DT_FRACTION, /*[fra]*/
    U_DT_INITIAL, /*[init]*/
    U_DT_ISOLATED, /*[iso]*/
    U_DT_MEDIAL, /*[med]*/
    U_DT_NARROW, /*[nar]*/
    U_DT_NOBREAK, /*[nb]*/
    U_DT_SMALL, /*[sml]*/
    U_DT_SQUARE, /*[sqr]*/
    U_DT_SUB, /*[sub]*/
    U_DT_SUPER, /*[sup]*/
    U_DT_VERTICAL, /*[vert]*/
    U_DT_WIDE, /*[wide]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UDecompositionType value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_DECOMPOSITION_TYPE).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_DT_COUNT /* 18 */
#endif // U_HIDE_DEPRECATED_API
} UDecompositionType;

/**
 * Joining Type constants.
 *
 * @see UCHAR_JOINING_TYPE
 * @stable ICU 2.2
 */
typedef enum UJoiningType {
    /*
     * Note: UJoiningType constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_JT_<Unicode Joining_Type value name>
     */

    U_JT_NON_JOINING, /*[U]*/
    U_JT_JOIN_CAUSING, /*[C]*/
    U_JT_DUAL_JOINING, /*[D]*/
    U_JT_LEFT_JOINING, /*[L]*/
    U_JT_RIGHT_JOINING, /*[R]*/
    U_JT_TRANSPARENT, /*[T]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UJoiningType value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_JOINING_TYPE).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_JT_COUNT /* 6 */
#endif // U_HIDE_DEPRECATED_API
} UJoiningType;

/**
 * Joining Group constants.
 *
 * @see UCHAR_JOINING_GROUP
 * @stable ICU 2.2
 */
typedef enum UJoiningGroup {
    /*
     * Note: UJoiningGroup constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_JG_<Unicode Joining_Group value name>
     */

    U_JG_NO_JOINING_GROUP,
    U_JG_AIN,
    U_JG_ALAPH,
    U_JG_ALEF,
    U_JG_BEH,
    U_JG_BETH,
    U_JG_DAL,
    U_JG_DALATH_RISH,
    U_JG_E,
    U_JG_FEH,
    U_JG_FINAL_SEMKATH,
    U_JG_GAF,
    U_JG_GAMAL,
    U_JG_HAH,
    U_JG_TEH_MARBUTA_GOAL, /**< @stable ICU 4.6 */
    U_JG_HAMZA_ON_HEH_GOAL = U_JG_TEH_MARBUTA_GOAL,
    U_JG_HE,
    U_JG_HEH,
    U_JG_HEH_GOAL,
    U_JG_HETH,
    U_JG_KAF,
    U_JG_KAPH,
    U_JG_KNOTTED_HEH,
    U_JG_LAM,
    U_JG_LAMADH,
    U_JG_MEEM,
    U_JG_MIM,
    U_JG_NOON,
    U_JG_NUN,
    U_JG_PE,
    U_JG_QAF,
    U_JG_QAPH,
    U_JG_REH,
    U_JG_REVERSED_PE,
    U_JG_SAD,
    U_JG_SADHE,
    U_JG_SEEN,
    U_JG_SEMKATH,
    U_JG_SHIN,
    U_JG_SWASH_KAF,
    U_JG_SYRIAC_WAW,
    U_JG_TAH,
    U_JG_TAW,
    U_JG_TEH_MARBUTA,
    U_JG_TETH,
    U_JG_WAW,
    U_JG_YEH,
    U_JG_YEH_BARREE,
    U_JG_YEH_WITH_TAIL,
    U_JG_YUDH,
    U_JG_YUDH_HE,
    U_JG_ZAIN,
    U_JG_FE, /**< @stable ICU 2.6 */
    U_JG_KHAPH, /**< @stable ICU 2.6 */
    U_JG_ZHAIN, /**< @stable ICU 2.6 */
    U_JG_BURUSHASKI_YEH_BARREE, /**< @stable ICU 4.0 */
    U_JG_FARSI_YEH, /**< @stable ICU 4.4 */
    U_JG_NYA, /**< @stable ICU 4.4 */
    U_JG_ROHINGYA_YEH, /**< @stable ICU 49 */
    U_JG_MANICHAEAN_ALEPH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_AYIN, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_BETH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_DALETH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_DHAMEDH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_FIVE, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_GIMEL, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_HETH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_HUNDRED, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_KAPH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_LAMEDH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_MEM, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_NUN, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_ONE, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_PE, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_QOPH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_RESH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_SADHE, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_SAMEKH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_TAW, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_TEN, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_TETH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_THAMEDH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_TWENTY, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_WAW, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_YODH, /**< @stable ICU 54 */
    U_JG_MANICHAEAN_ZAYIN, /**< @stable ICU 54 */
    U_JG_STRAIGHT_WAW, /**< @stable ICU 54 */
    U_JG_AFRICAN_FEH, /**< @stable ICU 58 */
    U_JG_AFRICAN_NOON, /**< @stable ICU 58 */
    U_JG_AFRICAN_QAF, /**< @stable ICU 58 */

    U_JG_MALAYALAM_BHA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_JA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_LLA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_LLLA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_NGA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_NNA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_NNNA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_NYA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_RA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_SSA, /**< @stable ICU 60 */
    U_JG_MALAYALAM_TTA, /**< @stable ICU 60 */

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UJoiningGroup value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_JOINING_GROUP).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_JG_COUNT
#endif // U_HIDE_DEPRECATED_API
} UJoiningGroup;

/**
 * Grapheme Cluster Break constants.
 *
 * @see UCHAR_GRAPHEME_CLUSTER_BREAK
 * @stable ICU 3.4
 */
typedef enum UGraphemeClusterBreak {
    /*
     * Note: UGraphemeClusterBreak constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_GCB_<Unicode Grapheme_Cluster_Break value name>
     */

    U_GCB_OTHER = 0, /*[XX]*/
    U_GCB_CONTROL = 1, /*[CN]*/
    U_GCB_CR = 2, /*[CR]*/
    U_GCB_EXTEND = 3, /*[EX]*/
    U_GCB_L = 4, /*[L]*/
    U_GCB_LF = 5, /*[LF]*/
    U_GCB_LV = 6, /*[LV]*/
    U_GCB_LVT = 7, /*[LVT]*/
    U_GCB_T = 8, /*[T]*/
    U_GCB_V = 9, /*[V]*/
    /** @stable ICU 4.0 */
    U_GCB_SPACING_MARK = 10,
    /*[SM]*/ /* from here on: new in Unicode 5.1/ICU 4.0 */
    /** @stable ICU 4.0 */
    U_GCB_PREPEND
    = 11, /*[PP]*/
    /** @stable ICU 50 */
    U_GCB_REGIONAL_INDICATOR = 12,
    /*[RI]*/ /* new in Unicode 6.2/ICU 50 */
    /** @stable ICU 58 */
    U_GCB_E_BASE
    = 13,
    /*[EB]*/ /* from here on: new in Unicode 9.0/ICU 58 */
    /** @stable ICU 58 */
    U_GCB_E_BASE_GAZ
    = 14, /*[EBG]*/
    /** @stable ICU 58 */
    U_GCB_E_MODIFIER = 15, /*[EM]*/
    /** @stable ICU 58 */
    U_GCB_GLUE_AFTER_ZWJ = 16, /*[GAZ]*/
    /** @stable ICU 58 */
    U_GCB_ZWJ = 17, /*[ZWJ]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UGraphemeClusterBreak value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_GRAPHEME_CLUSTER_BREAK).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_GCB_COUNT = 18
#endif // U_HIDE_DEPRECATED_API
} UGraphemeClusterBreak;

/**
 * Word Break constants.
 * (UWordBreak is a pre-existing enum type in ubrk.h for word break status tags.)
 *
 * @see UCHAR_WORD_BREAK
 * @stable ICU 3.4
 */
typedef enum UWordBreakValues {
    /*
     * Note: UWordBreakValues constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_WB_<Unicode Word_Break value name>
     */

    U_WB_OTHER = 0, /*[XX]*/
    U_WB_ALETTER = 1, /*[LE]*/
    U_WB_FORMAT = 2, /*[FO]*/
    U_WB_KATAKANA = 3, /*[KA]*/
    U_WB_MIDLETTER = 4, /*[ML]*/
    U_WB_MIDNUM = 5, /*[MN]*/
    U_WB_NUMERIC = 6, /*[NU]*/
    U_WB_EXTENDNUMLET = 7, /*[EX]*/
    /** @stable ICU 4.0 */
    U_WB_CR = 8,
    /*[CR]*/ /* from here on: new in Unicode 5.1/ICU 4.0 */
    /** @stable ICU 4.0 */
    U_WB_EXTEND
    = 9, /*[Extend]*/
    /** @stable ICU 4.0 */
    U_WB_LF = 10, /*[LF]*/
    /** @stable ICU 4.0 */
    U_WB_MIDNUMLET = 11, /*[MB]*/
    /** @stable ICU 4.0 */
    U_WB_NEWLINE = 12, /*[NL]*/
    /** @stable ICU 50 */
    U_WB_REGIONAL_INDICATOR = 13,
    /*[RI]*/ /* new in Unicode 6.2/ICU 50 */
    /** @stable ICU 52 */
    U_WB_HEBREW_LETTER
    = 14,
    /*[HL]*/ /* from here on: new in Unicode 6.3/ICU 52 */
    /** @stable ICU 52 */
    U_WB_SINGLE_QUOTE
    = 15, /*[SQ]*/
    /** @stable ICU 52 */
    U_WB_DOUBLE_QUOTE = 16, /*[DQ]*/
    /** @stable ICU 58 */
    U_WB_E_BASE = 17,
    /*[EB]*/ /* from here on: new in Unicode 9.0/ICU 58 */
    /** @stable ICU 58 */
    U_WB_E_BASE_GAZ
    = 18, /*[EBG]*/
    /** @stable ICU 58 */
    U_WB_E_MODIFIER = 19, /*[EM]*/
    /** @stable ICU 58 */
    U_WB_GLUE_AFTER_ZWJ = 20, /*[GAZ]*/
    /** @stable ICU 58 */
    U_WB_ZWJ = 21, /*[ZWJ]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UWordBreakValues value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_WORD_BREAK).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_WB_COUNT = 22
#endif // U_HIDE_DEPRECATED_API
} UWordBreakValues;

/**
 * Sentence Break constants.
 *
 * @see UCHAR_SENTENCE_BREAK
 * @stable ICU 3.4
 */
typedef enum USentenceBreak {
    /*
     * Note: USentenceBreak constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_SB_<Unicode Sentence_Break value name>
     */

    U_SB_OTHER = 0, /*[XX]*/
    U_SB_ATERM = 1, /*[AT]*/
    U_SB_CLOSE = 2, /*[CL]*/
    U_SB_FORMAT = 3, /*[FO]*/
    U_SB_LOWER = 4, /*[LO]*/
    U_SB_NUMERIC = 5, /*[NU]*/
    U_SB_OLETTER = 6, /*[LE]*/
    U_SB_SEP = 7, /*[SE]*/
    U_SB_SP = 8, /*[SP]*/
    U_SB_STERM = 9, /*[ST]*/
    U_SB_UPPER = 10, /*[UP]*/
    U_SB_CR = 11,
    /*[CR]*/ /* from here on: new in Unicode 5.1/ICU 4.0 */
    U_SB_EXTEND
    = 12, /*[EX]*/
    U_SB_LF = 13, /*[LF]*/
    U_SB_SCONTINUE = 14, /*[SC]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal USentenceBreak value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_SENTENCE_BREAK).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_SB_COUNT = 15
#endif // U_HIDE_DEPRECATED_API
} USentenceBreak;

/**
 * Line Break constants.
 *
 * @see UCHAR_LINE_BREAK
 * @stable ICU 2.2
 */
typedef enum ULineBreak {
    /*
     * Note: ULineBreak constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_LB_<Unicode Line_Break value name>
     */

    U_LB_UNKNOWN = 0, /*[XX]*/
    U_LB_AMBIGUOUS = 1, /*[AI]*/
    U_LB_ALPHABETIC = 2, /*[AL]*/
    U_LB_BREAK_BOTH = 3, /*[B2]*/
    U_LB_BREAK_AFTER = 4, /*[BA]*/
    U_LB_BREAK_BEFORE = 5, /*[BB]*/
    U_LB_MANDATORY_BREAK = 6, /*[BK]*/
    U_LB_CONTINGENT_BREAK = 7, /*[CB]*/
    U_LB_CLOSE_PUNCTUATION = 8, /*[CL]*/
    U_LB_COMBINING_MARK = 9, /*[CM]*/
    U_LB_CARRIAGE_RETURN = 10, /*[CR]*/
    U_LB_EXCLAMATION = 11, /*[EX]*/
    U_LB_GLUE = 12, /*[GL]*/
    U_LB_HYPHEN = 13, /*[HY]*/
    U_LB_IDEOGRAPHIC = 14, /*[ID]*/
    /** Renamed from the misspelled "inseperable" in Unicode 4.0.1/ICU 3.0 @stable ICU 3.0 */
    U_LB_INSEPARABLE = 15, /*[IN]*/
    U_LB_INSEPERABLE = U_LB_INSEPARABLE,
    U_LB_INFIX_NUMERIC = 16, /*[IS]*/
    U_LB_LINE_FEED = 17, /*[LF]*/
    U_LB_NONSTARTER = 18, /*[NS]*/
    U_LB_NUMERIC = 19, /*[NU]*/
    U_LB_OPEN_PUNCTUATION = 20, /*[OP]*/
    U_LB_POSTFIX_NUMERIC = 21, /*[PO]*/
    U_LB_PREFIX_NUMERIC = 22, /*[PR]*/
    U_LB_QUOTATION = 23, /*[QU]*/
    U_LB_COMPLEX_CONTEXT = 24, /*[SA]*/
    U_LB_SURROGATE = 25, /*[SG]*/
    U_LB_SPACE = 26, /*[SP]*/
    U_LB_BREAK_SYMBOLS = 27, /*[SY]*/
    U_LB_ZWSPACE = 28, /*[ZW]*/
    /** @stable ICU 2.6 */
    U_LB_NEXT_LINE = 29,
    /*[NL]*/ /* from here on: new in Unicode 4/ICU 2.6 */
    /** @stable ICU 2.6 */
    U_LB_WORD_JOINER
    = 30, /*[WJ]*/
    /** @stable ICU 3.4 */
    U_LB_H2 = 31,
    /*[H2]*/ /* from here on: new in Unicode 4.1/ICU 3.4 */
    /** @stable ICU 3.4 */
    U_LB_H3
    = 32, /*[H3]*/
    /** @stable ICU 3.4 */
    U_LB_JL = 33, /*[JL]*/
    /** @stable ICU 3.4 */
    U_LB_JT = 34, /*[JT]*/
    /** @stable ICU 3.4 */
    U_LB_JV = 35, /*[JV]*/
    /** @stable ICU 4.4 */
    U_LB_CLOSE_PARENTHESIS = 36,
    /*[CP]*/ /* new in Unicode 5.2/ICU 4.4 */
    /** @stable ICU 49 */
    U_LB_CONDITIONAL_JAPANESE_STARTER
    = 37,
    /*[CJ]*/ /* new in Unicode 6.1/ICU 49 */
    /** @stable ICU 49 */
    U_LB_HEBREW_LETTER
    = 38,
    /*[HL]*/ /* new in Unicode 6.1/ICU 49 */
    /** @stable ICU 50 */
    U_LB_REGIONAL_INDICATOR
    = 39,
    /*[RI]*/ /* new in Unicode 6.2/ICU 50 */
    /** @stable ICU 58 */
    U_LB_E_BASE
    = 40,
    /*[EB]*/ /* from here on: new in Unicode 9.0/ICU 58 */
    /** @stable ICU 58 */
    U_LB_E_MODIFIER
    = 41, /*[EM]*/
    /** @stable ICU 58 */
    U_LB_ZWJ = 42, /*[ZWJ]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal ULineBreak value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_LINE_BREAK).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_LB_COUNT = 43
#endif // U_HIDE_DEPRECATED_API
} ULineBreak;

/**
 * Numeric Type constants.
 *
 * @see UCHAR_NUMERIC_TYPE
 * @stable ICU 2.2
 */
typedef enum UNumericType {
    /*
     * Note: UNumericType constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_NT_<Unicode Numeric_Type value name>
     */

    U_NT_NONE, /*[None]*/
    U_NT_DECIMAL, /*[de]*/
    U_NT_DIGIT, /*[di]*/
    U_NT_NUMERIC, /*[nu]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UNumericType value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_NUMERIC_TYPE).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_NT_COUNT
#endif // U_HIDE_DEPRECATED_API
} UNumericType;

/**
 * Hangul Syllable Type constants.
 *
 * @see UCHAR_HANGUL_SYLLABLE_TYPE
 * @stable ICU 2.6
 */
typedef enum UHangulSyllableType {
    /*
     * Note: UHangulSyllableType constants are parsed by preparseucd.py.
     * It matches lines like
     *     U_HST_<Unicode Hangul_Syllable_Type value name>
     */

    U_HST_NOT_APPLICABLE, /*[NA]*/
    U_HST_LEADING_JAMO, /*[L]*/
    U_HST_VOWEL_JAMO, /*[V]*/
    U_HST_TRAILING_JAMO, /*[T]*/
    U_HST_LV_SYLLABLE, /*[LV]*/
    U_HST_LVT_SYLLABLE, /*[LVT]*/
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UHangulSyllableType value.
     * The highest value is available via u_getIntPropertyMaxValue(UCHAR_HANGUL_SYLLABLE_TYPE).
     *
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    U_HST_COUNT
#endif // U_HIDE_DEPRECATED_API
} UHangulSyllableType;


// unumsys.h
struct UNumberingSystem;

// uenum.h
struct UEnumeration;

// ucol.h
/** A collator.
*  For usage in C programs.
*/
struct UCollator;
/** structure representing a collator object instance
 * @stable ICU 2.0
 */
typedef struct UCollator UCollator;


/**
 * UCOL_LESS is returned if source string is compared to be less than target
 * string in the ucol_strcoll() method.
 * UCOL_EQUAL is returned if source string is compared to be equal to target
 * string in the ucol_strcoll() method.
 * UCOL_GREATER is returned if source string is compared to be greater than
 * target string in the ucol_strcoll() method.
 * @see ucol_strcoll()
 * <p>
 * Possible values for a comparison result
 * @stable ICU 2.0
 */
typedef enum {
    /** string a == string b */
    UCOL_EQUAL = 0,
    /** string a > string b */
    UCOL_GREATER = 1,
    /** string a < string b */
    UCOL_LESS = -1
} UCollationResult;


/** Enum containing attribute values for controling collation behavior.
 * Here are all the allowable values. Not every attribute can take every value. The only
 * universal value is UCOL_DEFAULT, which resets the attribute value to the predefined
 * value for that locale
 * @stable ICU 2.0
 */
typedef enum {
    /** accepted by most attributes */
    UCOL_DEFAULT = -1,

    /** Primary collation strength */
    UCOL_PRIMARY = 0,
    /** Secondary collation strength */
    UCOL_SECONDARY = 1,
    /** Tertiary collation strength */
    UCOL_TERTIARY = 2,
    /** Default collation strength */
    UCOL_DEFAULT_STRENGTH = UCOL_TERTIARY,
    UCOL_CE_STRENGTH_LIMIT,
    /** Quaternary collation strength */
    UCOL_QUATERNARY = 3,
    /** Identical collation strength */
    UCOL_IDENTICAL = 15,
    UCOL_STRENGTH_LIMIT,

    /** Turn the feature off - works for UCOL_FRENCH_COLLATION,
      UCOL_CASE_LEVEL, UCOL_HIRAGANA_QUATERNARY_MODE
      & UCOL_DECOMPOSITION_MODE*/
    UCOL_OFF = 16,
    /** Turn the feature on - works for UCOL_FRENCH_COLLATION,
      UCOL_CASE_LEVEL, UCOL_HIRAGANA_QUATERNARY_MODE
      & UCOL_DECOMPOSITION_MODE*/
    UCOL_ON = 17,

    /** Valid for UCOL_ALTERNATE_HANDLING. Alternate handling will be shifted */
    UCOL_SHIFTED = 20,
    /** Valid for UCOL_ALTERNATE_HANDLING. Alternate handling will be non ignorable */
    UCOL_NON_IGNORABLE = 21,

    /** Valid for UCOL_CASE_FIRST -
      lower case sorts before upper case */
    UCOL_LOWER_FIRST = 24,
    /** upper case sorts before lower case */
    UCOL_UPPER_FIRST = 25,

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UColAttributeValue value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCOL_ATTRIBUTE_VALUE_COUNT
#endif /* U_HIDE_DEPRECATED_API */
} UColAttributeValue;

/**
 * Enum containing the codes for reordering segments of the collation table that are not script
 * codes. These reordering codes are to be used in conjunction with the script codes.
 * @see ucol_getReorderCodes
 * @see ucol_setReorderCodes
 * @see ucol_getEquivalentReorderCodes
 * @see UScriptCode
 * @stable ICU 4.8
 */
typedef enum {
    /**
    * A special reordering code that is used to specify the default
    * reordering codes for a locale.
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_DEFAULT = -1,
    /**
    * A special reordering code that is used to specify no reordering codes.
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_NONE = USCRIPT_UNKNOWN,
    /**
    * A special reordering code that is used to specify all other codes used for
    * reordering except for the codes lised as UColReorderCode values and those
    * listed explicitly in a reordering.
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_OTHERS = USCRIPT_UNKNOWN,
    /**
    * Characters with the space property.
    * This is equivalent to the rule value "space".
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_SPACE = 0x1000,
    /**
    * The first entry in the enumeration of reordering groups. This is intended for use in
    * range checking and enumeration of the reorder codes.
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_FIRST = UCOL_REORDER_CODE_SPACE,
    /**
    * Characters with the punctuation property.
    * This is equivalent to the rule value "punct".
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_PUNCTUATION = 0x1001,
    /**
    * Characters with the symbol property.
    * This is equivalent to the rule value "symbol".
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_SYMBOL = 0x1002,
    /**
    * Characters with the currency property.
    * This is equivalent to the rule value "currency".
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_CURRENCY = 0x1003,
    /**
    * Characters with the digit property.
    * This is equivalent to the rule value "digit".
    * @stable ICU 4.8
    */
    UCOL_REORDER_CODE_DIGIT = 0x1004,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UColReorderCode value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCOL_REORDER_CODE_LIMIT = 0x1005
#endif /* U_HIDE_DEPRECATED_API */
} UColReorderCode;

/**
 * Base letter represents a primary difference.  Set comparison
 * level to UCOL_PRIMARY to ignore secondary and tertiary differences.
 * Use this to set the strength of a Collator object.
 * Example of primary difference, "abc" &lt; "abd"
 *
 * Diacritical differences on the same base letter represent a secondary
 * difference.  Set comparison level to UCOL_SECONDARY to ignore tertiary
 * differences. Use this to set the strength of a Collator object.
 * Example of secondary difference, "&auml;" >> "a".
 *
 * Uppercase and lowercase versions of the same character represents a
 * tertiary difference.  Set comparison level to UCOL_TERTIARY to include
 * all comparison differences. Use this to set the strength of a Collator
 * object.
 * Example of tertiary difference, "abc" &lt;&lt;&lt; "ABC".
 *
 * Two characters are considered "identical" when they have the same
 * unicode spellings.  UCOL_IDENTICAL.
 * For example, "&auml;" == "&auml;".
 *
 * UCollationStrength is also used to determine the strength of sort keys
 * generated from UCollator objects
 * These values can be now found in the UColAttributeValue enum.
 * @stable ICU 2.0
 **/
typedef UColAttributeValue UCollationStrength;

/** Attributes that collation service understands. All the attributes can take UCOL_DEFAULT
 * value, as well as the values specific to each one.
 * @stable ICU 2.0
 */
typedef enum {
    /** Attribute for direction of secondary weights - used in Canadian French.
      * Acceptable values are UCOL_ON, which results in secondary weights
      * being considered backwards and UCOL_OFF which treats secondary
      * weights in the order they appear.
      * @stable ICU 2.0
      */
    UCOL_FRENCH_COLLATION,
    /** Attribute for handling variable elements.
      * Acceptable values are UCOL_NON_IGNORABLE (default)
      * which treats all the codepoints with non-ignorable
      * primary weights in the same way,
      * and UCOL_SHIFTED which causes codepoints with primary
      * weights that are equal or below the variable top value
      * to be ignored on primary level and moved to the quaternary
      * level.
      * @stable ICU 2.0
      */
    UCOL_ALTERNATE_HANDLING,
    /** Controls the ordering of upper and lower case letters.
      * Acceptable values are UCOL_OFF (default), which orders
      * upper and lower case letters in accordance to their tertiary
      * weights, UCOL_UPPER_FIRST which forces upper case letters to
      * sort before lower case letters, and UCOL_LOWER_FIRST which does
      * the opposite.
      * @stable ICU 2.0
      */
    UCOL_CASE_FIRST,
    /** Controls whether an extra case level (positioned before the third
      * level) is generated or not. Acceptable values are UCOL_OFF (default),
      * when case level is not generated, and UCOL_ON which causes the case
      * level to be generated. Contents of the case level are affected by
      * the value of UCOL_CASE_FIRST attribute. A simple way to ignore
      * accent differences in a string is to set the strength to UCOL_PRIMARY
      * and enable case level.
      * @stable ICU 2.0
      */
    UCOL_CASE_LEVEL,
    /** Controls whether the normalization check and necessary normalizations
      * are performed. When set to UCOL_OFF (default) no normalization check
      * is performed. The correctness of the result is guaranteed only if the
      * input data is in so-called FCD form (see users manual for more info).
      * When set to UCOL_ON, an incremental check is performed to see whether
      * the input data is in the FCD form. If the data is not in the FCD form,
      * incremental NFD normalization is performed.
      * @stable ICU 2.0
      */
    UCOL_NORMALIZATION_MODE,
    /** An alias for UCOL_NORMALIZATION_MODE attribute.
      * @stable ICU 2.0
      */
    UCOL_DECOMPOSITION_MODE = UCOL_NORMALIZATION_MODE,
    /** The strength attribute. Can be either UCOL_PRIMARY, UCOL_SECONDARY,
      * UCOL_TERTIARY, UCOL_QUATERNARY or UCOL_IDENTICAL. The usual strength
      * for most locales (except Japanese) is tertiary.
      *
      * Quaternary strength
      * is useful when combined with shifted setting for alternate handling
      * attribute and for JIS X 4061 collation, when it is used to distinguish
      * between Katakana and Hiragana.
      * Otherwise, quaternary level
      * is affected only by the number of non-ignorable code points in
      * the string.
      *
      * Identical strength is rarely useful, as it amounts
      * to codepoints of the NFD form of the string.
      * @stable ICU 2.0
      */
    UCOL_STRENGTH,
#ifndef U_HIDE_DEPRECATED_API
    /** When turned on, this attribute positions Hiragana before all
      * non-ignorables on quaternary level This is a sneaky way to produce JIS
      * sort order.
      *
      * This attribute was an implementation detail of the CLDR Japanese tailoring.
      * Since ICU 50, this attribute is not settable any more via API functions.
      * Since CLDR 25/ICU 53, explicit quaternary relations are used
      * to achieve the same Japanese sort order.
      *
      * @deprecated ICU 50 Implementation detail, cannot be set via API, was removed from implementation.
      */
    UCOL_HIRAGANA_QUATERNARY_MODE = UCOL_STRENGTH + 1,
#endif /* U_HIDE_DEPRECATED_API */
    /**
      * When turned on, this attribute makes
      * substrings of digits sort according to their numeric values.
      *
      * This is a way to get '100' to sort AFTER '2'. Note that the longest
      * digit substring that can be treated as a single unit is
      * 254 digits (not counting leading zeros). If a digit substring is
      * longer than that, the digits beyond the limit will be treated as a
      * separate digit substring.
      *
      * A "digit" in this sense is a code point with General_Category=Nd,
      * which does not include circled numbers, roman numerals, etc.
      * Only a contiguous digit substring is considered, that is,
      * non-negative integers without separators.
      * There is no support for plus/minus signs, decimals, exponents, etc.
      *
      * @stable ICU 2.8
      */
    UCOL_NUMERIC_COLLATION = UCOL_STRENGTH + 2,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of RuleBasedCollator object. */
    /**
     * One more than the highest normal UColAttribute value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCOL_ATTRIBUTE_COUNT
} UColAttribute;

/** Options for retrieving the rule string
 *  @stable ICU 2.0
 */
typedef enum {
    /**
   * Retrieves the tailoring rules only.
   * Same as calling the version of getRules() without UColRuleOption.
   * @stable ICU 2.0
   */
    UCOL_TAILORING_ONLY,
    /**
   * Retrieves the "UCA rules" concatenated with the tailoring rules.
   * The "UCA rules" are an <i>approximation</i> of the root collator's sort order.
   * They are almost never used or useful at runtime and can be removed from the data.
   * See http://userguide.icu-project.org/collation/customization#TOC-Building-on-Existing-Locales
   * @stable ICU 2.0
   */
    UCOL_FULL_RULES
} UColRuleOption;

// uiter.h
struct UCharIterator;
typedef struct UCharIterator UCharIterator; /**< C typedef for struct UCharIterator. @stable ICU 2.1 */

/**
 * Origin constants for UCharIterator.getIndex() and UCharIterator.move().
 * @see UCharIteratorMove
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef enum UCharIteratorOrigin {
    UITER_START,
    UITER_CURRENT,
    UITER_LIMIT,
    UITER_ZERO,
    UITER_LENGTH
} UCharIteratorOrigin;

/** Constants for UCharIterator. @stable ICU 2.6 */
enum {
    /**
     * Constant value that may be returned by UCharIteratorMove
     * indicating that the final UTF-16 index is not known, but that the move succeeded.
     * This can occur when moving relative to limit or length, or
     * when moving relative to the current index after a setState()
     * when the current UTF-16 index is not known.
     *
     * It would be very inefficient to have to count from the beginning of the text
     * just to get the current/limit/length index after moving relative to it.
     * The actual index can be determined with getIndex(UITER_CURRENT)
     * which will count the UChars if necessary.
     *
     * @stable ICU 2.6
     */
    UITER_UNKNOWN_INDEX = -2
};


/**
 * Constant for UCharIterator getState() indicating an error or
 * an unknown state.
 * Returned by uiter_getState()/UCharIteratorGetState
 * when an error occurs.
 * Also, some UCharIterator implementations may not be able to return
 * a valid state for each position. This will be clearly documented
 * for each such iterator (none of the public ones here).
 *
 * @stable ICU 2.6
 */
#define UITER_NO_STATE ((uint32_t)0xffffffff)

/**
 * Function type declaration for UCharIterator.getIndex().
 *
 * Gets the current position, or the start or limit of the
 * iteration range.
 *
 * This function may perform slowly for UITER_CURRENT after setState() was called,
 * or for UITER_LENGTH, because an iterator implementation may have to count
 * UChars if the underlying storage is not UTF-16.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @param origin get the 0, start, limit, length, or current index
 * @return the requested index, or U_SENTINEL in an error condition
 *
 * @see UCharIteratorOrigin
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef int32_t U_CALLCONV
UCharIteratorGetIndex(UCharIterator *iter, UCharIteratorOrigin origin);

/**
 * Function type declaration for UCharIterator.move().
 *
 * Use iter->move(iter, index, UITER_ZERO) like CharacterIterator::setIndex(index).
 *
 * Moves the current position relative to the start or limit of the
 * iteration range, or relative to the current position itself.
 * The movement is expressed in numbers of code units forward
 * or backward by specifying a positive or negative delta.
 * Out of bounds movement will be pinned to the start or limit.
 *
 * This function may perform slowly for moving relative to UITER_LENGTH
 * because an iterator implementation may have to count the rest of the
 * UChars if the native storage is not UTF-16.
 *
 * When moving relative to the limit or length, or
 * relative to the current position after setState() was called,
 * move() may return UITER_UNKNOWN_INDEX (-2) to avoid an inefficient
 * determination of the actual UTF-16 index.
 * The actual index can be determined with getIndex(UITER_CURRENT)
 * which will count the UChars if necessary.
 * See UITER_UNKNOWN_INDEX for details.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @param delta can be positive, zero, or negative
 * @param origin move relative to the 0, start, limit, length, or current index
 * @return the new index, or U_SENTINEL on an error condition,
 *         or UITER_UNKNOWN_INDEX when the index is not known.
 *
 * @see UCharIteratorOrigin
 * @see UCharIterator
 * @see UITER_UNKNOWN_INDEX
 * @stable ICU 2.1
 */
typedef int32_t U_CALLCONV
UCharIteratorMove(UCharIterator *iter, int32_t delta, UCharIteratorOrigin origin);

/**
 * Function type declaration for UCharIterator.hasNext().
 *
 * Check if current() and next() can still
 * return another code unit.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return boolean value for whether current() and next() can still return another code unit
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef UBool U_CALLCONV
UCharIteratorHasNext(UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.hasPrevious().
 *
 * Check if previous() can still return another code unit.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return boolean value for whether previous() can still return another code unit
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef UBool U_CALLCONV
UCharIteratorHasPrevious(UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.current().
 *
 * Return the code unit at the current position,
 * or U_SENTINEL if there is none (index is at the limit).
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return the current code unit
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef UChar32 U_CALLCONV
UCharIteratorCurrent(UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.next().
 *
 * Return the code unit at the current index and increment
 * the index (post-increment, like s[i++]),
 * or return U_SENTINEL if there is none (index is at the limit).
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return the current code unit (and post-increment the current index)
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef UChar32 U_CALLCONV
UCharIteratorNext(UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.previous().
 *
 * Decrement the index and return the code unit from there
 * (pre-decrement, like s[--i]),
 * or return U_SENTINEL if there is none (index is at the start).
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return the previous code unit (after pre-decrementing the current index)
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef UChar32 U_CALLCONV
UCharIteratorPrevious(UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.reservedFn().
 * Reserved for future use.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @param something some integer argument
 * @return some integer
 *
 * @see UCharIterator
 * @stable ICU 2.1
 */
typedef int32_t U_CALLCONV
UCharIteratorReserved(UCharIterator *iter, int32_t something);

/**
 * Function type declaration for UCharIterator.getState().
 *
 * Get the "state" of the iterator in the form of a single 32-bit word.
 * It is recommended that the state value be calculated to be as small as
 * is feasible. For strings with limited lengths, fewer than 32 bits may
 * be sufficient.
 *
 * This is used together with setState()/UCharIteratorSetState
 * to save and restore the iterator position more efficiently than with
 * getIndex()/move().
 *
 * The iterator state is defined as a uint32_t value because it is designed
 * for use in ucol_nextSortKeyPart() which provides 32 bits to store the state
 * of the character iterator.
 *
 * With some UCharIterator implementations (e.g., UTF-8),
 * getting and setting the UTF-16 index with existing functions
 * (getIndex(UITER_CURRENT) followed by move(pos, UITER_ZERO)) is possible but
 * relatively slow because the iterator has to "walk" from a known index
 * to the requested one.
 * This takes more time the farther it needs to go.
 *
 * An opaque state value allows an iterator implementation to provide
 * an internal index (UTF-8: the source byte array index) for
 * fast, constant-time restoration.
 *
 * After calling setState(), a getIndex(UITER_CURRENT) may be slow because
 * the UTF-16 index may not be restored as well, but the iterator can deliver
 * the correct text contents and move relative to the current position
 * without performance degradation.
 *
 * Some UCharIterator implementations may not be able to return
 * a valid state for each position, in which case they return UITER_NO_STATE instead.
 * This will be clearly documented for each such iterator (none of the public ones here).
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @return the state word
 *
 * @see UCharIterator
 * @see UCharIteratorSetState
 * @see UITER_NO_STATE
 * @stable ICU 2.6
 */
typedef uint32_t U_CALLCONV
UCharIteratorGetState(const UCharIterator *iter);

/**
 * Function type declaration for UCharIterator.setState().
 *
 * Restore the "state" of the iterator using a state word from a getState() call.
 * The iterator object need not be the same one as for which getState() was called,
 * but it must be of the same type (set up using the same uiter_setXYZ function)
 * and it must iterate over the same string
 * (binary identical regardless of memory address).
 * For more about the state word see UCharIteratorGetState.
 *
 * After calling setState(), a getIndex(UITER_CURRENT) may be slow because
 * the UTF-16 index may not be restored as well, but the iterator can deliver
 * the correct text contents and move relative to the current position
 * without performance degradation.
 *
 * @param iter the UCharIterator structure ("this pointer")
 * @param state the state word from a getState() call
 *              on a same-type, same-string iterator
 * @param pErrorCode Must be a valid pointer to an error code value,
 *                   which must not indicate a failure before the function call.
 *
 * @see UCharIterator
 * @see UCharIteratorGetState
 * @stable ICU 2.6
 */
typedef void U_CALLCONV
UCharIteratorSetState(UCharIterator *iter, uint32_t state, UErrorCode *pErrorCode);

struct UCharIterator {
    /**
     * (protected) Pointer to string or wrapped object or similar.
     * Not used by caller.
     * @stable ICU 2.1
     */
    const void *context;

    /**
     * (protected) Length of string or similar.
     * Not used by caller.
     * @stable ICU 2.1
     */
    int32_t length;

    /**
     * (protected) Start index or similar.
     * Not used by caller.
     * @stable ICU 2.1
     */
    int32_t start;

    /**
     * (protected) Current index or similar.
     * Not used by caller.
     * @stable ICU 2.1
     */
    int32_t index;

    /**
     * (protected) Limit index or similar.
     * Not used by caller.
     * @stable ICU 2.1
     */
    int32_t limit;

    /**
     * (protected) Used by UTF-8 iterators and possibly others.
     * @stable ICU 2.1
     */
    int32_t reservedField;

    /**
     * (public) Returns the current position or the
     * start or limit index of the iteration range.
     *
     * @see UCharIteratorGetIndex
     * @stable ICU 2.1
     */
    UCharIteratorGetIndex *getIndex;

    /**
     * (public) Moves the current position relative to the start or limit of the
     * iteration range, or relative to the current position itself.
     * The movement is expressed in numbers of code units forward
     * or backward by specifying a positive or negative delta.
     *
     * @see UCharIteratorMove
     * @stable ICU 2.1
     */
    UCharIteratorMove *move;

    /**
     * (public) Check if current() and next() can still
     * return another code unit.
     *
     * @see UCharIteratorHasNext
     * @stable ICU 2.1
     */
    UCharIteratorHasNext *hasNext;

    /**
     * (public) Check if previous() can still return another code unit.
     *
     * @see UCharIteratorHasPrevious
     * @stable ICU 2.1
     */
    UCharIteratorHasPrevious *hasPrevious;

    /**
     * (public) Return the code unit at the current position,
     * or U_SENTINEL if there is none (index is at the limit).
     *
     * @see UCharIteratorCurrent
     * @stable ICU 2.1
     */
    UCharIteratorCurrent *current;

    /**
     * (public) Return the code unit at the current index and increment
     * the index (post-increment, like s[i++]),
     * or return U_SENTINEL if there is none (index is at the limit).
     *
     * @see UCharIteratorNext
     * @stable ICU 2.1
     */
    UCharIteratorNext *next;

    /**
     * (public) Decrement the index and return the code unit from there
     * (pre-decrement, like s[--i]),
     * or return U_SENTINEL if there is none (index is at the start).
     *
     * @see UCharIteratorPrevious
     * @stable ICU 2.1
     */
    UCharIteratorPrevious *previous;

    /**
     * (public) Reserved for future use. Currently NULL.
     *
     * @see UCharIteratorReserved
     * @stable ICU 2.1
     */
    UCharIteratorReserved *reservedFn;

    /**
     * (public) Return the state of the iterator, to be restored later with setState().
     * This function pointer is NULL if the iterator does not implement it.
     *
     * @see UCharIteratorGet
     * @stable ICU 2.6
     */
    UCharIteratorGetState *getState;

    /**
     * (public) Restore the iterator state from the state word from a call
     * to getState().
     * This function pointer is NULL if the iterator does not implement it.
     *
     * @see UCharIteratorSet
     * @stable ICU 2.6
     */
    UCharIteratorSetState *setState;
};

// ucal.h
/**
 * The time zone ID reserved for unknown time zone.
 * @stable ICU 4.8
 */
#define UCAL_UNKNOWN_ZONE_ID "Etc/Unknown"

/** A calendar.
 *  For usage in C programs.
 * @stable ICU 2.0
 */
typedef void *UCalendar;

/** Possible types of UCalendars
 * @stable ICU 2.0
 */
enum UCalendarType {
    /**
   * Despite the name, UCAL_TRADITIONAL designates the locale's default calendar,
   * which may be the Gregorian calendar or some other calendar.
   * @stable ICU 2.0
   */
    UCAL_TRADITIONAL,
    /**
   * A better name for UCAL_TRADITIONAL.
   * @stable ICU 4.2
   */
    UCAL_DEFAULT = UCAL_TRADITIONAL,
    /**
   * Unambiguously designates the Gregorian calendar for the locale.
   * @stable ICU 2.0
   */
    UCAL_GREGORIAN
};

/** @stable ICU 2.0 */
typedef enum UCalendarType UCalendarType;

/** Possible fields in a UCalendar
 * @stable ICU 2.0
 */
enum UCalendarDateFields {
    /**
   * Field number indicating the era, e.g., AD or BC in the Gregorian (Julian) calendar.
   * This is a calendar-specific value.
   * @stable ICU 2.6
   */
    UCAL_ERA,

    /**
   * Field number indicating the year. This is a calendar-specific value.
   * @stable ICU 2.6
   */
    UCAL_YEAR,

    /**
   * Field number indicating the month. This is a calendar-specific value.
   * The first month of the year is
   * <code>JANUARY</code>; the last depends on the number of months in a year.
   * @see #UCAL_JANUARY
   * @see #UCAL_FEBRUARY
   * @see #UCAL_MARCH
   * @see #UCAL_APRIL
   * @see #UCAL_MAY
   * @see #UCAL_JUNE
   * @see #UCAL_JULY
   * @see #UCAL_AUGUST
   * @see #UCAL_SEPTEMBER
   * @see #UCAL_OCTOBER
   * @see #UCAL_NOVEMBER
   * @see #UCAL_DECEMBER
   * @see #UCAL_UNDECIMBER
   * @stable ICU 2.6
   */
    UCAL_MONTH,

    /**
   * Field number indicating the
   * week number within the current year.  The first week of the year, as
   * defined by <code>UCAL_FIRST_DAY_OF_WEEK</code> and <code>UCAL_MINIMAL_DAYS_IN_FIRST_WEEK</code>
   * attributes, has value 1.  Subclasses define
   * the value of <code>UCAL_WEEK_OF_YEAR</code> for days before the first week of
   * the year.
   * @see ucal_getAttribute
   * @see ucal_setAttribute
   * @stable ICU 2.6
   */
    UCAL_WEEK_OF_YEAR,

    /**
   * Field number indicating the
   * week number within the current month.  The first week of the month, as
   * defined by <code>UCAL_FIRST_DAY_OF_WEEK</code> and <code>UCAL_MINIMAL_DAYS_IN_FIRST_WEEK</code>
   * attributes, has value 1.  Subclasses define
   * the value of <code>WEEK_OF_MONTH</code> for days before the first week of
   * the month.
   * @see ucal_getAttribute
   * @see ucal_setAttribute
   * @see #UCAL_FIRST_DAY_OF_WEEK
   * @see #UCAL_MINIMAL_DAYS_IN_FIRST_WEEK
   * @stable ICU 2.6
   */
    UCAL_WEEK_OF_MONTH,

    /**
   * Field number indicating the
   * day of the month. This is a synonym for <code>DAY_OF_MONTH</code>.
   * The first day of the month has value 1.
   * @see #UCAL_DAY_OF_MONTH
   * @stable ICU 2.6
   */
    UCAL_DATE,

    /**
   * Field number indicating the day
   * number within the current year.  The first day of the year has value 1.
   * @stable ICU 2.6
   */
    UCAL_DAY_OF_YEAR,

    /**
   * Field number indicating the day
   * of the week.  This field takes values <code>SUNDAY</code>,
   * <code>MONDAY</code>, <code>TUESDAY</code>, <code>WEDNESDAY</code>,
   * <code>THURSDAY</code>, <code>FRIDAY</code>, and <code>SATURDAY</code>.
   * @see #UCAL_SUNDAY
   * @see #UCAL_MONDAY
   * @see #UCAL_TUESDAY
   * @see #UCAL_WEDNESDAY
   * @see #UCAL_THURSDAY
   * @see #UCAL_FRIDAY
   * @see #UCAL_SATURDAY
   * @stable ICU 2.6
   */
    UCAL_DAY_OF_WEEK,

    /**
   * Field number indicating the
   * ordinal number of the day of the week within the current month. Together
   * with the <code>DAY_OF_WEEK</code> field, this uniquely specifies a day
   * within a month.  Unlike <code>WEEK_OF_MONTH</code> and
   * <code>WEEK_OF_YEAR</code>, this field's value does <em>not</em> depend on
   * <code>getFirstDayOfWeek()</code> or
   * <code>getMinimalDaysInFirstWeek()</code>.  <code>DAY_OF_MONTH 1</code>
   * through <code>7</code> always correspond to <code>DAY_OF_WEEK_IN_MONTH
   * 1</code>; <code>8</code> through <code>15</code> correspond to
   * <code>DAY_OF_WEEK_IN_MONTH 2</code>, and so on.
   * <code>DAY_OF_WEEK_IN_MONTH 0</code> indicates the week before
   * <code>DAY_OF_WEEK_IN_MONTH 1</code>.  Negative values count back from the
   * end of the month, so the last Sunday of a month is specified as
   * <code>DAY_OF_WEEK = SUNDAY, DAY_OF_WEEK_IN_MONTH = -1</code>.  Because
   * negative values count backward they will usually be aligned differently
   * within the month than positive values.  For example, if a month has 31
   * days, <code>DAY_OF_WEEK_IN_MONTH -1</code> will overlap
   * <code>DAY_OF_WEEK_IN_MONTH 5</code> and the end of <code>4</code>.
   * @see #UCAL_DAY_OF_WEEK
   * @see #UCAL_WEEK_OF_MONTH
   * @stable ICU 2.6
   */
    UCAL_DAY_OF_WEEK_IN_MONTH,

    /**
   * Field number indicating
   * whether the <code>HOUR</code> is before or after noon.
   * E.g., at 10:04:15.250 PM the <code>AM_PM</code> is <code>PM</code>.
   * @see #UCAL_AM
   * @see #UCAL_PM
   * @see #UCAL_HOUR
   * @stable ICU 2.6
   */
    UCAL_AM_PM,

    /**
   * Field number indicating the
   * hour of the morning or afternoon. <code>HOUR</code> is used for the 12-hour
   * clock.
   * E.g., at 10:04:15.250 PM the <code>HOUR</code> is 10.
   * @see #UCAL_AM_PM
   * @see #UCAL_HOUR_OF_DAY
   * @stable ICU 2.6
   */
    UCAL_HOUR,

    /**
   * Field number indicating the
   * hour of the day. <code>HOUR_OF_DAY</code> is used for the 24-hour clock.
   * E.g., at 10:04:15.250 PM the <code>HOUR_OF_DAY</code> is 22.
   * @see #UCAL_HOUR
   * @stable ICU 2.6
   */
    UCAL_HOUR_OF_DAY,

    /**
   * Field number indicating the
   * minute within the hour.
   * E.g., at 10:04:15.250 PM the <code>UCAL_MINUTE</code> is 4.
   * @stable ICU 2.6
   */
    UCAL_MINUTE,

    /**
   * Field number indicating the
   * second within the minute.
   * E.g., at 10:04:15.250 PM the <code>UCAL_SECOND</code> is 15.
   * @stable ICU 2.6
   */
    UCAL_SECOND,

    /**
   * Field number indicating the
   * millisecond within the second.
   * E.g., at 10:04:15.250 PM the <code>UCAL_MILLISECOND</code> is 250.
   * @stable ICU 2.6
   */
    UCAL_MILLISECOND,

    /**
   * Field number indicating the
   * raw offset from GMT in milliseconds.
   * @stable ICU 2.6
   */
    UCAL_ZONE_OFFSET,

    /**
   * Field number indicating the
   * daylight savings offset in milliseconds.
   * @stable ICU 2.6
   */
    UCAL_DST_OFFSET,

    /**
   * Field number
   * indicating the extended year corresponding to the
   * <code>UCAL_WEEK_OF_YEAR</code> field.  This may be one greater or less
   * than the value of <code>UCAL_EXTENDED_YEAR</code>.
   * @stable ICU 2.6
   */
    UCAL_YEAR_WOY,

    /**
   * Field number
   * indicating the localized day of week.  This will be a value from 1
   * to 7 inclusive, with 1 being the localized first day of the week.
   * @stable ICU 2.6
   */
    UCAL_DOW_LOCAL,

    /**
   * Year of this calendar system, encompassing all supra-year fields. For example,
   * in Gregorian/Julian calendars, positive Extended Year values indicate years AD,
   *  1 BC = 0 extended, 2 BC = -1 extended, and so on.
   * @stable ICU 2.8
   */
    UCAL_EXTENDED_YEAR,

    /**
   * Field number
   * indicating the modified Julian day number.  This is different from
   * the conventional Julian day number in two regards.  First, it
   * demarcates days at local zone midnight, rather than noon GMT.
   * Second, it is a local number; that is, it depends on the local time
   * zone.  It can be thought of as a single number that encompasses all
   * the date-related fields.
   * @stable ICU 2.8
   */
    UCAL_JULIAN_DAY,

    /**
   * Ranges from 0 to 23:59:59.999 (regardless of DST).  This field behaves <em>exactly</em>
   * like a composite of all time-related fields, not including the zone fields.  As such,
   * it also reflects discontinuities of those fields on DST transition days.  On a day
   * of DST onset, it will jump forward.  On a day of DST cessation, it will jump
   * backward.  This reflects the fact that it must be combined with the DST_OFFSET field
   * to obtain a unique local time value.
   * @stable ICU 2.8
   */
    UCAL_MILLISECONDS_IN_DAY,

    /**
   * Whether or not the current month is a leap month (0 or 1). See the Chinese calendar for
   * an example of this.
   */
    UCAL_IS_LEAP_MONTH,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of Calendar, DateFormat, and other objects */
    /**
     * One more than the highest normal UCalendarDateFields value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCAL_FIELD_COUNT,

    /**
   * Field number indicating the
   * day of the month. This is a synonym for <code>UCAL_DATE</code>.
   * The first day of the month has value 1.
   * @see #UCAL_DATE
   * Synonym for UCAL_DATE
   * @stable ICU 2.8
   **/
    UCAL_DAY_OF_MONTH = UCAL_DATE
};

/** @stable ICU 2.0 */
typedef enum UCalendarDateFields UCalendarDateFields;
/**
     * Useful constant for days of week. Note: Calendar day-of-week is 1-based. Clients
     * who create locale resources for the field of first-day-of-week should be aware of
     * this. For instance, in US locale, first-day-of-week is set to 1, i.e., UCAL_SUNDAY.
     */
/** Possible days of the week in a UCalendar
 * @stable ICU 2.0
 */
enum UCalendarDaysOfWeek {
    /** Sunday */
    UCAL_SUNDAY = 1,
    /** Monday */
    UCAL_MONDAY,
    /** Tuesday */
    UCAL_TUESDAY,
    /** Wednesday */
    UCAL_WEDNESDAY,
    /** Thursday */
    UCAL_THURSDAY,
    /** Friday */
    UCAL_FRIDAY,
    /** Saturday */
    UCAL_SATURDAY
};

/** @stable ICU 2.0 */
typedef enum UCalendarDaysOfWeek UCalendarDaysOfWeek;

/** Possible months in a UCalendar. Note: Calendar month is 0-based.
 * @stable ICU 2.0
 */
enum UCalendarMonths {
    /** January */
    UCAL_JANUARY,
    /** February */
    UCAL_FEBRUARY,
    /** March */
    UCAL_MARCH,
    /** April */
    UCAL_APRIL,
    /** May */
    UCAL_MAY,
    /** June */
    UCAL_JUNE,
    /** July */
    UCAL_JULY,
    /** August */
    UCAL_AUGUST,
    /** September */
    UCAL_SEPTEMBER,
    /** October */
    UCAL_OCTOBER,
    /** November */
    UCAL_NOVEMBER,
    /** December */
    UCAL_DECEMBER,
    /** Value of the <code>UCAL_MONTH</code> field indicating the
    * thirteenth month of the year. Although the Gregorian calendar
    * does not use this value, lunar calendars do.
    */
    UCAL_UNDECIMBER
};

/** @stable ICU 2.0 */
typedef enum UCalendarMonths UCalendarMonths;

/** Possible AM/PM values in a UCalendar
 * @stable ICU 2.0
 */
enum UCalendarAMPMs {
    /** AM */
    UCAL_AM,
    /** PM */
    UCAL_PM
};

/** @stable ICU 2.0 */
typedef enum UCalendarAMPMs UCalendarAMPMs;

/**
 * System time zone type constants used by filtering zones
 * in ucal_openTimeZoneIDEnumeration.
 * @see ucal_openTimeZoneIDEnumeration
 * @stable ICU 4.8
 */
enum USystemTimeZoneType {
    /**
     * Any system zones.
     * @stable ICU 4.8
     */
    UCAL_ZONE_TYPE_ANY,
    /**
     * Canonical system zones.
     * @stable ICU 4.8
     */
    UCAL_ZONE_TYPE_CANONICAL,
    /**
     * Canonical system zones associated with actual locations.
     * @stable ICU 4.8
     */
    UCAL_ZONE_TYPE_CANONICAL_LOCATION
};

/** @stable ICU 4.8 */
typedef enum USystemTimeZoneType USystemTimeZoneType;

// udatpg.h
typedef void *UDateTimePatternGenerator;

/**
 * Field number constants for udatpg_getAppendItemFormats() and similar functions.
 * These constants are separate from UDateFormatField despite semantic overlap
 * because some fields are merged for the date/time pattern generator.
 * @stable ICU 3.8
 */
typedef enum UDateTimePatternField {
    /** @stable ICU 3.8 */
    UDATPG_ERA_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_QUARTER_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEK_OF_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEK_OF_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEKDAY_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_OF_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAYPERIOD_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_HOUR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_MINUTE_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_SECOND_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_FRACTIONAL_SECOND_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_ZONE_FIELD,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of DateTimePatternGenerator object. */
    /**
     * One more than the highest normal UDateTimePatternField value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDATPG_FIELD_COUNT
} UDateTimePatternField;

#ifndef U_HIDE_DRAFT_API
/**
 * Field display name width constants for udatpg_getFieldDisplayName().
 * @draft ICU 61
 */
typedef enum UDateTimePGDisplayWidth {
    /** @draft ICU 61 */
    UDATPG_WIDE,
    /** @draft ICU 61 */
    UDATPG_ABBREVIATED,
    /** @draft ICU 61 */
    UDATPG_NARROW
} UDateTimePGDisplayWidth;
#endif // U_HIDE_DRAFT_API

/**
 * Masks to control forcing the length of specified fields in the returned
 * pattern to match those in the skeleton (when this would not happen
 * otherwise). These may be combined to force the length of multiple fields.
 * Used with udatpg_getBestPatternWithOptions, udatpg_replaceFieldTypesWithOptions.
 * @stable ICU 4.4
 */
typedef enum UDateTimePatternMatchOptions {
    /** @stable ICU 4.4 */
    UDATPG_MATCH_NO_OPTIONS = 0,
    /** @stable ICU 4.4 */
    UDATPG_MATCH_HOUR_FIELD_LENGTH = 1 << UDATPG_HOUR_FIELD,
#ifndef U_HIDE_INTERNAL_API
    /** @internal ICU 4.4 */
    UDATPG_MATCH_MINUTE_FIELD_LENGTH = 1 << UDATPG_MINUTE_FIELD,
    /** @internal ICU 4.4 */
    UDATPG_MATCH_SECOND_FIELD_LENGTH = 1 << UDATPG_SECOND_FIELD,
#endif /* U_HIDE_INTERNAL_API */
    /** @stable ICU 4.4 */
    UDATPG_MATCH_ALL_FIELDS_LENGTH = (1 << UDATPG_FIELD_COUNT) - 1
} UDateTimePatternMatchOptions;

/**
 * Status return values from udatpg_addPattern().
 * @stable ICU 3.8
 */
typedef enum UDateTimePatternConflict {
    /** @stable ICU 3.8 */
    UDATPG_NO_CONFLICT,
    /** @stable ICU 3.8 */
    UDATPG_BASE_CONFLICT,
    /** @stable ICU 3.8 */
    UDATPG_CONFLICT,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UDateTimePatternConflict value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDATPG_CONFLICT_COUNT
#endif // U_HIDE_DEPRECATED_API
} UDateTimePatternConflict;

// unum.h
typedef void *UNumberFormat;

/** The possible number format styles.
 *  @stable ICU 2.0
 */
typedef enum UNumberFormatStyle {
    /**
     * Decimal format defined by a pattern string.
     * @stable ICU 3.0
     */
    UNUM_PATTERN_DECIMAL = 0,
    /**
     * Decimal format ("normal" style).
     * @stable ICU 2.0
     */
    UNUM_DECIMAL = 1,
    /**
     * Currency format (generic).
     * Defaults to UNUM_CURRENCY_STANDARD style
     * (using currency symbol, e.g., "$1.00", with non-accounting
     * style for negative values e.g. using minus sign).
     * The specific style may be specified using the -cf- locale key.
     * @stable ICU 2.0
     */
    UNUM_CURRENCY = 2,
    /**
     * Percent format
     * @stable ICU 2.0
     */
    UNUM_PERCENT = 3,
    /**
     * Scientific format
     * @stable ICU 2.1
     */
    UNUM_SCIENTIFIC = 4,
    /**
     * Spellout rule-based format. The default ruleset can be specified/changed using
     * unum_setTextAttribute with UNUM_DEFAULT_RULESET; the available public rulesets
     * can be listed using unum_getTextAttribute with UNUM_PUBLIC_RULESETS.
     * @stable ICU 2.0
     */
    UNUM_SPELLOUT = 5,
    /**
     * Ordinal rule-based format . The default ruleset can be specified/changed using
     * unum_setTextAttribute with UNUM_DEFAULT_RULESET; the available public rulesets
     * can be listed using unum_getTextAttribute with UNUM_PUBLIC_RULESETS.
     * @stable ICU 3.0
     */
    UNUM_ORDINAL = 6,
    /**
     * Duration rule-based format
     * @stable ICU 3.0
     */
    UNUM_DURATION = 7,
    /**
     * Numbering system rule-based format
     * @stable ICU 4.2
     */
    UNUM_NUMBERING_SYSTEM = 8,
    /**
     * Rule-based format defined by a pattern string.
     * @stable ICU 3.0
     */
    UNUM_PATTERN_RULEBASED = 9,
    /**
     * Currency format with an ISO currency code, e.g., "USD1.00".
     * @stable ICU 4.8
     */
    UNUM_CURRENCY_ISO = 10,
    /**
     * Currency format with a pluralized currency name,
     * e.g., "1.00 US dollar" and "3.00 US dollars".
     * @stable ICU 4.8
     */
    UNUM_CURRENCY_PLURAL = 11,
    /**
     * Currency format for accounting, e.g., "($3.00)" for
     * negative currency amount instead of "-$3.00" ({@link #UNUM_CURRENCY}).
     * Overrides any style specified using -cf- key in locale.
     * @stable ICU 53
     */
    UNUM_CURRENCY_ACCOUNTING = 12,
    /**
     * Currency format with a currency symbol given CASH usage, e.g.,
     * "NT$3" instead of "NT$3.23".
     * @stable ICU 54
     */
    UNUM_CASH_CURRENCY = 13,
    /**
     * Decimal format expressed using compact notation
     * (short form, corresponds to UNumberCompactStyle=UNUM_SHORT)
     * e.g. "23K", "45B"
     * @stable ICU 56
     */
    UNUM_DECIMAL_COMPACT_SHORT = 14,
    /**
     * Decimal format expressed using compact notation
     * (long form, corresponds to UNumberCompactStyle=UNUM_LONG)
     * e.g. "23 thousand", "45 billion"
     * @stable ICU 56
     */
    UNUM_DECIMAL_COMPACT_LONG = 15,
    /**
     * Currency format with a currency symbol, e.g., "$1.00",
     * using non-accounting style for negative values (e.g. minus sign).
     * Overrides any style specified using -cf- key in locale.
     * @stable ICU 56
     */
    UNUM_CURRENCY_STANDARD = 16,

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UNumberFormatStyle value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UNUM_FORMAT_STYLE_COUNT = 17,
#endif /* U_HIDE_DEPRECATED_API */

    /**
     * Default format
     * @stable ICU 2.0
     */
    UNUM_DEFAULT = UNUM_DECIMAL,
    /**
     * Alias for UNUM_PATTERN_DECIMAL
     * @stable ICU 3.0
     */
    UNUM_IGNORE = UNUM_PATTERN_DECIMAL
} UNumberFormatStyle;

/** The possible number format rounding modes.
 *
 * <p>
 * For more detail on rounding modes, see:
 * http://userguide.icu-project.org/formatparse/numbers/rounding-modes
 *
 * @stable ICU 2.0
 */
typedef enum UNumberFormatRoundingMode {
    UNUM_ROUND_CEILING,
    UNUM_ROUND_FLOOR,
    UNUM_ROUND_DOWN,
    UNUM_ROUND_UP,
    /**
     * Half-even rounding
     * @stable, ICU 3.8
     */
    UNUM_ROUND_HALFEVEN,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * Half-even rounding, misspelled name
     * @deprecated, ICU 3.8
     */
    UNUM_FOUND_HALFEVEN = UNUM_ROUND_HALFEVEN,
#endif /* U_HIDE_DEPRECATED_API */
    UNUM_ROUND_HALFDOWN = UNUM_ROUND_HALFEVEN + 1,
    UNUM_ROUND_HALFUP,
    /**
      * ROUND_UNNECESSARY reports an error if formatted result is not exact.
      * @stable ICU 4.8
      */
    UNUM_ROUND_UNNECESSARY
} UNumberFormatRoundingMode;

/** The possible number format pad positions.
 *  @stable ICU 2.0
 */
typedef enum UNumberFormatPadPosition {
    UNUM_PAD_BEFORE_PREFIX,
    UNUM_PAD_AFTER_PREFIX,
    UNUM_PAD_BEFORE_SUFFIX,
    UNUM_PAD_AFTER_SUFFIX
} UNumberFormatPadPosition;

/**
 * Constants for specifying short or long format.
 * @stable ICU 51
 */
typedef enum UNumberCompactStyle {
    /** @stable ICU 51 */
    UNUM_SHORT,
    /** @stable ICU 51 */
    UNUM_LONG
    /** @stable ICU 51 */
} UNumberCompactStyle;

/**
 * Constants for specifying currency spacing
 * @stable ICU 4.8
 */
enum UCurrencySpacing {
    /** @stable ICU 4.8 */
    UNUM_CURRENCY_MATCH,
    /** @stable ICU 4.8 */
    UNUM_CURRENCY_SURROUNDING_MATCH,
    /** @stable ICU 4.8 */
    UNUM_CURRENCY_INSERT,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of DecimalFormatSymbols object. */
    /**
     * One more than the highest normal UCurrencySpacing value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UNUM_CURRENCY_SPACING_COUNT
};
typedef enum UCurrencySpacing UCurrencySpacing; /**< @stable ICU 4.8 */


/**
 * FieldPosition and UFieldPosition selectors for format fields
 * defined by NumberFormat and UNumberFormat.
 * @stable ICU 49
 */
typedef enum UNumberFormatFields {
    /** @stable ICU 49 */
    UNUM_INTEGER_FIELD,
    /** @stable ICU 49 */
    UNUM_FRACTION_FIELD,
    /** @stable ICU 49 */
    UNUM_DECIMAL_SEPARATOR_FIELD,
    /** @stable ICU 49 */
    UNUM_EXPONENT_SYMBOL_FIELD,
    /** @stable ICU 49 */
    UNUM_EXPONENT_SIGN_FIELD,
    /** @stable ICU 49 */
    UNUM_EXPONENT_FIELD,
    /** @stable ICU 49 */
    UNUM_GROUPING_SEPARATOR_FIELD,
    /** @stable ICU 49 */
    UNUM_CURRENCY_FIELD,
    /** @stable ICU 49 */
    UNUM_PERCENT_FIELD,
    /** @stable ICU 49 */
    UNUM_PERMILL_FIELD,
    /** @stable ICU 49 */
    UNUM_SIGN_FIELD,
    /** @stable ICU 64 */
    UNUM_MEASURE_UNIT_FIELD,
    /** @stable ICU 64 */
    UNUM_COMPACT_FIELD,

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UNumberFormatFields value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UNUM_FIELD_COUNT = UNUM_SIGN_FIELD + 3
#endif /* U_HIDE_DEPRECATED_API */
} UNumberFormatFields;


/** The possible UNumberFormat text attributes @stable ICU 2.0*/
typedef enum UNumberFormatTextAttribute {
    /** Positive prefix */
    UNUM_POSITIVE_PREFIX,
    /** Positive suffix */
    UNUM_POSITIVE_SUFFIX,
    /** Negative prefix */
    UNUM_NEGATIVE_PREFIX,
    /** Negative suffix */
    UNUM_NEGATIVE_SUFFIX,
    /** The character used to pad to the format width. */
    UNUM_PADDING_CHARACTER,
    /** The ISO currency code */
    UNUM_CURRENCY_CODE,
    /**
   * The default rule set, such as "%spellout-numbering-year:", "%spellout-cardinal:",
   * "%spellout-ordinal-masculine-plural:", "%spellout-ordinal-feminine:", or
   * "%spellout-ordinal-neuter:". The available public rulesets can be listed using
   * unum_getTextAttribute with UNUM_PUBLIC_RULESETS. This is only available with
   * rule-based formatters.
   * @stable ICU 3.0
   */
    UNUM_DEFAULT_RULESET,
    /**
   * The public rule sets.  This is only available with rule-based formatters.
   * This is a read-only attribute.  The public rulesets are returned as a
   * single string, with each ruleset name delimited by ';' (semicolon). See the
   * CLDR LDML spec for more information about RBNF rulesets:
   * http://www.unicode.org/reports/tr35/tr35-numbers.html#Rule-Based_Number_Formatting
   * @stable ICU 3.0
   */
    UNUM_PUBLIC_RULESETS
} UNumberFormatTextAttribute;

/* The UNumberFormatAttributeValue type cannot be #ifndef U_HIDE_INTERNAL_API, needed for .h variable declaration */
/**
 * @internal
 */
typedef enum UNumberFormatAttributeValue {
#ifndef U_HIDE_INTERNAL_API
    /** @internal */
    UNUM_NO = 0,
    /** @internal */
    UNUM_YES = 1,
    /** @internal */
    UNUM_MAYBE = 2
#else
    /** @internal */
    UNUM_FORMAT_ATTRIBUTE_VALUE_HIDDEN
#endif /* U_HIDE_INTERNAL_API */
} UNumberFormatAttributeValue;

/** The possible UNumberFormat numeric attributes @stable ICU 2.0 */
typedef enum UNumberFormatAttribute {
    /** Parse integers only */
    UNUM_PARSE_INT_ONLY,
    /** Use grouping separator */
    UNUM_GROUPING_USED,
    /** Always show decimal point */
    UNUM_DECIMAL_ALWAYS_SHOWN,
    /** Maximum integer digits */
    UNUM_MAX_INTEGER_DIGITS,
    /** Minimum integer digits */
    UNUM_MIN_INTEGER_DIGITS,
    /** Integer digits */
    UNUM_INTEGER_DIGITS,
    /** Maximum fraction digits */
    UNUM_MAX_FRACTION_DIGITS,
    /** Minimum fraction digits */
    UNUM_MIN_FRACTION_DIGITS,
    /** Fraction digits */
    UNUM_FRACTION_DIGITS,
    /** Multiplier */
    UNUM_MULTIPLIER,
    /** Grouping size */
    UNUM_GROUPING_SIZE,
    /** Rounding Mode */
    UNUM_ROUNDING_MODE,
    /** Rounding increment */
    UNUM_ROUNDING_INCREMENT,
    /** The width to which the output of <code>format()</code> is padded. */
    UNUM_FORMAT_WIDTH,
    /** The position at which padding will take place. */
    UNUM_PADDING_POSITION,
    /** Secondary grouping size */
    UNUM_SECONDARY_GROUPING_SIZE,
    /** Use significant digits
   * @stable ICU 3.0 */
    UNUM_SIGNIFICANT_DIGITS_USED,
    /** Minimum significant digits
   * @stable ICU 3.0 */
    UNUM_MIN_SIGNIFICANT_DIGITS,
    /** Maximum significant digits
   * @stable ICU 3.0 */
    UNUM_MAX_SIGNIFICANT_DIGITS,
    /** Lenient parse mode used by rule-based formats.
   * @stable ICU 3.0
   */
    UNUM_LENIENT_PARSE,
#if UCONFIG_HAVE_PARSEALLINPUT
    /** Consume all input. (may use fastpath). Set to UNUM_YES (require fastpath), UNUM_NO (skip fastpath), or UNUM_MAYBE (heuristic).
   * This is an internal ICU API. Do not use.
   * @internal
   */
    UNUM_PARSE_ALL_INPUT = 20,
#endif
    /**
    * Scale, which adjusts the position of the
    * decimal point when formatting.  Amounts will be multiplied by 10 ^ (scale)
    * before they are formatted.  The default value for the scale is 0 ( no adjustment ).
    *
    * <p>Example: setting the scale to 3, 123 formats as "123,000"
    * <p>Example: setting the scale to -4, 123 formats as "0.0123"
    *
   * @stable ICU 51 */
    UNUM_SCALE = 21,
#ifndef U_HIDE_INTERNAL_API
    /**
   * Minimum grouping digits, technology preview.
   * See DecimalFormat::getMinimumGroupingDigits().
   *
   * @internal technology preview
   */
    UNUM_MINIMUM_GROUPING_DIGITS = 22,
/* TODO: test C API when it becomes @draft */
#endif /* U_HIDE_INTERNAL_API */

    /**
   * if this attribute is set to 0, it is set to UNUM_CURRENCY_STANDARD purpose,
   * otherwise it is UNUM_CURRENCY_CASH purpose
   * Default: 0 (UNUM_CURRENCY_STANDARD purpose)
   * @stable ICU 54
   */
    UNUM_CURRENCY_USAGE = 23,

    /* The following cannot be #ifndef U_HIDE_INTERNAL_API, needed in .h file variable declararions */
    /** One below the first bitfield-boolean item.
   * All items after this one are stored in boolean form.
   * @internal */
    UNUM_MAX_NONBOOLEAN_ATTRIBUTE = 0x0FFF,

    /** If 1, specifies that if setting the "max integer digits" attribute would truncate a value, set an error status rather than silently truncating.
   * For example,  formatting the value 1234 with 4 max int digits would succeed, but formatting 12345 would fail. There is no effect on parsing.
   * Default: 0 (not set)
   * @stable ICU 50
   */
    UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS = 0x1000,
    /**
   * if this attribute is set to 1, specifies that, if the pattern doesn't contain an exponent, the exponent will not be parsed. If the pattern does contain an exponent, this attribute has no effect.
   * Has no effect on formatting.
   * Default: 0 (unset)
   * @stable ICU 50
   */
    UNUM_PARSE_NO_EXPONENT,

    /**
   * if this attribute is set to 1, specifies that, if the pattern contains a
   * decimal mark the input is required to have one. If this attribute is set to 0,
   * specifies that input does not have to contain a decimal mark.
   * Has no effect on formatting.
   * Default: 0 (unset)
   * @stable ICU 54
   */
    UNUM_PARSE_DECIMAL_MARK_REQUIRED = 0x1002,

    /* The following cannot be #ifndef U_HIDE_INTERNAL_API, needed in .h file variable declararions */
    /** Limit of boolean attributes.
   * @internal */
    UNUM_LIMIT_BOOLEAN_ATTRIBUTE = 0x1003
} UNumberFormatAttribute;

// uloc.h

/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_CHINESE "zh"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_ENGLISH "en"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_FRENCH "fr"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_GERMAN "de"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_ITALIAN "it"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_JAPANESE "ja"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_KOREAN "ko"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_SIMPLIFIED_CHINESE "zh_CN"
/** Useful constant for this language. @stable ICU 2.0 */
#define ULOC_TRADITIONAL_CHINESE "zh_TW"

/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_CANADA "en_CA"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_CANADA_FRENCH "fr_CA"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_CHINA "zh_CN"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_PRC "zh_CN"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_FRANCE "fr_FR"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_GERMANY "de_DE"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_ITALY "it_IT"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_JAPAN "ja_JP"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_KOREA "ko_KR"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_TAIWAN "zh_TW"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_UK "en_GB"
/** Useful constant for this country/region. @stable ICU 2.0 */
#define ULOC_US "en_US"

/**
 * Useful constant for the maximum size of the language part of a locale ID.
 * (including the terminating NULL).
 * @stable ICU 2.0
 */
#define ULOC_LANG_CAPACITY 12

/**
 * Useful constant for the maximum size of the country part of a locale ID
 * (including the terminating NULL).
 * @stable ICU 2.0
 */
#define ULOC_COUNTRY_CAPACITY 4
/**
 * Useful constant for the maximum size of the whole locale ID
 * (including the terminating NULL and all keywords).
 * @stable ICU 2.0
 */
#define ULOC_FULLNAME_CAPACITY 157

/**
 * Useful constant for the maximum size of the script part of a locale ID
 * (including the terminating NULL).
 * @stable ICU 2.8
 */
#define ULOC_SCRIPT_CAPACITY 6

/**
 * Useful constant for the maximum size of keywords in a locale
 * @stable ICU 2.8
 */
#define ULOC_KEYWORDS_CAPACITY 96

/**
 * Useful constant for the maximum total size of keywords and their values in a locale
 * @stable ICU 2.8
 */
#define ULOC_KEYWORD_AND_VALUES_CAPACITY 100

/**
 * Invariant character separating keywords from the locale string
 * @stable ICU 2.8
 */
#define ULOC_KEYWORD_SEPARATOR '@'

/**
  * Unicode code point for '@' separating keywords from the locale string.
  * @see ULOC_KEYWORD_SEPARATOR
  * @stable ICU 4.6
  */
#define ULOC_KEYWORD_SEPARATOR_UNICODE 0x40

/**
 * Invariant character for assigning value to a keyword
 * @stable ICU 2.8
 */
#define ULOC_KEYWORD_ASSIGN '='

/**
  * Unicode code point for '=' for assigning value to a keyword.
  * @see ULOC_KEYWORD_ASSIGN
  * @stable ICU 4.6
  */
#define ULOC_KEYWORD_ASSIGN_UNICODE 0x3D

/**
 * Invariant character separating keywords
 * @stable ICU 2.8
 */
#define ULOC_KEYWORD_ITEM_SEPARATOR ';'

/**
  * Unicode code point for ';' separating keywords
  * @see ULOC_KEYWORD_ITEM_SEPARATOR
  * @stable ICU 4.6
  */
#define ULOC_KEYWORD_ITEM_SEPARATOR_UNICODE 0x3B

/**
 * Constants for *_getLocale()
 * Allow user to select whether she wants information on
 * requested, valid or actual locale.
 * For example, a collator for "en_US_CALIFORNIA" was
 * requested. In the current state of ICU (2.0),
 * the requested locale is "en_US_CALIFORNIA",
 * the valid locale is "en_US" (most specific locale supported by ICU)
 * and the actual locale is "root" (the collation data comes unmodified
 * from the UCA)
 * The locale is considered supported by ICU if there is a core ICU bundle
 * for that locale (although it may be empty).
 * @stable ICU 2.1
 */
typedef enum {
    /** This is locale the data actually comes from
   * @stable ICU 2.1
   */
    ULOC_ACTUAL_LOCALE = 0,
    /** This is the most specific locale supported by ICU
   * @stable ICU 2.1
   */
    ULOC_VALID_LOCALE = 1,

#ifndef U_HIDE_DEPRECATED_API
    /** This is the requested locale
   *  @deprecated ICU 2.8
   */
    ULOC_REQUESTED_LOCALE = 2,

    /**
     * One more than the highest normal ULocDataLocaleType value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    ULOC_DATA_LOCALE_TYPE_LIMIT = 3
#endif // U_HIDE_DEPRECATED_API
} ULocDataLocaleType;

// ubrk.h
struct UBreakIterator;

/** The possible types of text boundaries.  @stable ICU 2.0 */
typedef enum UBreakIteratorType {
    /** Character breaks  @stable ICU 2.0 */
    UBRK_CHARACTER = 0,
    /** Word breaks @stable ICU 2.0 */
    UBRK_WORD = 1,
    /** Line breaks @stable ICU 2.0 */
    UBRK_LINE = 2,
    /** Sentence breaks @stable ICU 2.0 */
    UBRK_SENTENCE = 3,

#ifndef U_HIDE_DEPRECATED_API
    /**
   * Title Case breaks
   * The iterator created using this type locates title boundaries as described for
   * Unicode 3.2 only. For Unicode 4.0 and above title boundary iteration,
   * please use Word Boundary iterator.
   *
   * @deprecated ICU 2.8 Use the word break iterator for titlecasing for Unicode 4 and later.
   */
    UBRK_TITLE = 4,
    /**
     * One more than the highest normal UBreakIteratorType value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UBRK_COUNT = 5
#endif // U_HIDE_DEPRECATED_API
} UBreakIteratorType;

/** Value indicating all text boundaries have been returned.
 *  @stable ICU 2.0
 */
#define UBRK_DONE ((int32_t)-1)


/**
 *  Enum constants for the word break tags returned by
 *  getRuleStatus().  A range of values is defined for each category of
 *  word, to allow for further subdivisions of a category in future releases.
 *  Applications should check for tag values falling within the range, rather
 *  than for single individual values.
 *
 * The numeric values of all of these constants are stable (will not change).
 *
 * @stable ICU 2.2
*/
typedef enum UWordBreak {
    /** Tag value for "words" that do not fit into any of other categories.
     *  Includes spaces and most punctuation. */
    UBRK_WORD_NONE = 0,
    /** Upper bound for tags for uncategorized words. */
    UBRK_WORD_NONE_LIMIT = 100,
    /** Tag value for words that appear to be numbers, lower limit.    */
    UBRK_WORD_NUMBER = 100,
    /** Tag value for words that appear to be numbers, upper limit.    */
    UBRK_WORD_NUMBER_LIMIT = 200,
    /** Tag value for words that contain letters, excluding
     *  hiragana, katakana or ideographic characters, lower limit.    */
    UBRK_WORD_LETTER = 200,
    /** Tag value for words containing letters, upper limit  */
    UBRK_WORD_LETTER_LIMIT = 300,
    /** Tag value for words containing kana characters, lower limit */
    UBRK_WORD_KANA = 300,
    /** Tag value for words containing kana characters, upper limit */
    UBRK_WORD_KANA_LIMIT = 400,
    /** Tag value for words containing ideographic characters, lower limit */
    UBRK_WORD_IDEO = 400,
    /** Tag value for words containing ideographic characters, upper limit */
    UBRK_WORD_IDEO_LIMIT = 500
} UWordBreak;

/**
 *  Enum constants for the line break tags returned by getRuleStatus().
 *  A range of values is defined for each category of
 *  word, to allow for further subdivisions of a category in future releases.
 *  Applications should check for tag values falling within the range, rather
 *  than for single individual values.
 *
 * The numeric values of all of these constants are stable (will not change).
 *
 * @stable ICU 2.8
*/
typedef enum ULineBreakTag {
    /** Tag value for soft line breaks, positions at which a line break
      *  is acceptable but not required                */
    UBRK_LINE_SOFT = 0,
    /** Upper bound for soft line breaks.              */
    UBRK_LINE_SOFT_LIMIT = 100,
    /** Tag value for a hard, or mandatory line break  */
    UBRK_LINE_HARD = 100,
    /** Upper bound for hard line breaks.              */
    UBRK_LINE_HARD_LIMIT = 200
} ULineBreakTag;


/**
 *  Enum constants for the sentence break tags returned by getRuleStatus().
 *  A range of values is defined for each category of
 *  sentence, to allow for further subdivisions of a category in future releases.
 *  Applications should check for tag values falling within the range, rather
 *  than for single individual values.
 *
 * The numeric values of all of these constants are stable (will not change).
 *
 * @stable ICU 2.8
*/
typedef enum USentenceBreakTag {
    /** Tag value for for sentences  ending with a sentence terminator
      * ('.', '?', '!', etc.) character, possibly followed by a
      * hard separator (CR, LF, PS, etc.)
      */
    UBRK_SENTENCE_TERM = 0,
    /** Upper bound for tags for sentences ended by sentence terminators.    */
    UBRK_SENTENCE_TERM_LIMIT = 100,
    /** Tag value for for sentences that do not contain an ending
      * sentence terminator ('.', '?', '!', etc.) character, but
      * are ended only by a hard separator (CR, LF, PS, etc.) or end of input.
      */
    UBRK_SENTENCE_SEP = 100,
    /** Upper bound for tags for sentences ended by a separator.              */
    UBRK_SENTENCE_SEP_LIMIT = 200
    /** Tag value for a hard, or mandatory line break  */
} USentenceBreakTag;

// ucnv_err.h

/** Forward declaring the UConverter structure. @stable ICU 2.0 */
struct UConverter;

/** @stable ICU 2.0 */
typedef struct UConverter UConverter;

/**
 * FROM_U, TO_U context options for sub callback
 * @stable ICU 2.0
 */
#define UCNV_SUB_STOP_ON_ILLEGAL "i"

/**
 * FROM_U, TO_U context options for skip callback
 * @stable ICU 2.0
 */
#define UCNV_SKIP_STOP_ON_ILLEGAL "i"

/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to ICU (%UXXXX)
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_ICU NULL
/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to JAVA (\\uXXXX)
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_JAVA "J"
/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to C (\\uXXXX \\UXXXXXXXX)
 * TO_U_CALLBACK_ESCAPE option to escape the character value accoding to C (\\xXXXX)
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_C "C"
/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to XML Decimal escape \htmlonly(&amp;#DDDD;)\endhtmlonly
 * TO_U_CALLBACK_ESCAPE context option to escape the character value accoding to XML Decimal escape \htmlonly(&amp;#DDDD;)\endhtmlonly
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_XML_DEC "D"
/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to XML Hex escape \htmlonly(&amp;#xXXXX;)\endhtmlonly
 * TO_U_CALLBACK_ESCAPE context option to escape the character value accoding to XML Hex escape \htmlonly(&amp;#xXXXX;)\endhtmlonly
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_XML_HEX "X"
/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to Unicode (U+XXXXX)
 * @stable ICU 2.0
 */
#define UCNV_ESCAPE_UNICODE "U"

/**
 * FROM_U_CALLBACK_ESCAPE context option to escape the code unit according to CSS2 conventions (\\HH..H<space>, that is,
 * a backslash, 1..6 hex digits, and a space)
 * @stable ICU 4.0
 */
#define UCNV_ESCAPE_CSS2 "S"

/**
 * The process condition code to be used with the callbacks.
 * Codes which are greater than UCNV_IRREGULAR should be
 * passed on to any chained callbacks.
 * @stable ICU 2.0
 */
typedef enum {
    UCNV_UNASSIGNED = 0, /**< The code point is unassigned.
                             The error code U_INVALID_CHAR_FOUND will be set. */
    UCNV_ILLEGAL = 1, /**< The code point is illegal. For example,
                             \\x81\\x2E is illegal in SJIS because \\x2E
                             is not a valid trail byte for the \\x81
                             lead byte.
                             Also, starting with Unicode 3.0.1, non-shortest byte sequences
                             in UTF-8 (like \\xC1\\xA1 instead of \\x61 for U+0061)
                             are also illegal, not just irregular.
                             The error code U_ILLEGAL_CHAR_FOUND will be set. */
    UCNV_IRREGULAR = 2, /**< The codepoint is not a regular sequence in
                             the encoding. For example, \\xED\\xA0\\x80..\\xED\\xBF\\xBF
                             are irregular UTF-8 byte sequences for single surrogate
                             code points.
                             The error code U_INVALID_CHAR_FOUND will be set. */
    UCNV_RESET = 3, /**< The callback is called with this reason when a
                             'reset' has occured. Callback should reset all
                             state. */
    UCNV_CLOSE = 4, /**< Called when the converter is closed. The
                             callback should release any allocated memory.*/
    UCNV_CLONE = 5 /**< Called when ucnv_safeClone() is called on the
                              converter. the pointer available as the
                              'context' is an alias to the original converters'
                              context pointer. If the context must be owned
                              by the new converter, the callback must clone
                              the data and call ucnv_setFromUCallback
                              (or setToUCallback) with the correct pointer.
                              @stable ICU 2.2
                           */
} UConverterCallbackReason;


/**
 * The structure for the fromUnicode callback function parameter.
 * @stable ICU 2.0
 */
typedef struct {
    uint16_t size; /**< The size of this struct. @stable ICU 2.0 */
    UBool flush; /**< The internal state of converter will be reset and data flushed if set to TRUE. @stable ICU 2.0    */
    UConverter *converter; /**< Pointer to the converter that is opened and to which this struct is passed as an argument. @stable ICU 2.0  */
    const UChar *source; /**< Pointer to the source source buffer. @stable ICU 2.0    */
    const UChar *sourceLimit; /**< Pointer to the limit (end + 1) of source buffer. @stable ICU 2.0    */
    char *target; /**< Pointer to the target buffer. @stable ICU 2.0    */
    const char *targetLimit; /**< Pointer to the limit (end + 1) of target buffer. @stable ICU 2.0     */
    int32_t *offsets; /**< Pointer to the buffer that recieves the offsets. *offset = blah ; offset++;. @stable ICU 2.0  */
} UConverterFromUnicodeArgs;


/**
 * The structure for the toUnicode callback function parameter.
 * @stable ICU 2.0
 */
typedef struct {
    uint16_t size; /**< The size of this struct   @stable ICU 2.0 */
    UBool flush; /**< The internal state of converter will be reset and data flushed if set to TRUE. @stable ICU 2.0   */
    UConverter *converter; /**< Pointer to the converter that is opened and to which this struct is passed as an argument. @stable ICU 2.0 */
    const char *source; /**< Pointer to the source source buffer. @stable ICU 2.0    */
    const char *sourceLimit; /**< Pointer to the limit (end + 1) of source buffer. @stable ICU 2.0    */
    UChar *target; /**< Pointer to the target buffer. @stable ICU 2.0    */
    const UChar *targetLimit; /**< Pointer to the limit (end + 1) of target buffer. @stable ICU 2.0     */
    int32_t *offsets; /**< Pointer to the buffer that recieves the offsets. *offset = blah ; offset++;. @stable ICU 2.0  */
} UConverterToUnicodeArgs;

// ucnv.h
/** Maximum length of a converter name including the terminating NULL @stable ICU 2.0 */
#define UCNV_MAX_CONVERTER_NAME_LENGTH 60
/** Maximum length of a converter name including path and terminating NULL @stable ICU 2.0 */
#define UCNV_MAX_FULL_FILE_NAME_LENGTH (600 + UCNV_MAX_CONVERTER_NAME_LENGTH)

/** Shift in for EBDCDIC_STATEFUL and iso2022 states @stable ICU 2.0 */
#define UCNV_SI 0x0F
/** Shift out for EBDCDIC_STATEFUL and iso2022 states @stable ICU 2.0 */
#define UCNV_SO 0x0E

/**
 * Enum for specifying basic types of converters
 * @see ucnv_getType
 * @stable ICU 2.0
 */
typedef enum {
    /** @stable ICU 2.0 */
    UCNV_UNSUPPORTED_CONVERTER = -1,
    /** @stable ICU 2.0 */
    UCNV_SBCS = 0,
    /** @stable ICU 2.0 */
    UCNV_DBCS = 1,
    /** @stable ICU 2.0 */
    UCNV_MBCS = 2,
    /** @stable ICU 2.0 */
    UCNV_LATIN_1 = 3,
    /** @stable ICU 2.0 */
    UCNV_UTF8 = 4,
    /** @stable ICU 2.0 */
    UCNV_UTF16_BigEndian = 5,
    /** @stable ICU 2.0 */
    UCNV_UTF16_LittleEndian = 6,
    /** @stable ICU 2.0 */
    UCNV_UTF32_BigEndian = 7,
    /** @stable ICU 2.0 */
    UCNV_UTF32_LittleEndian = 8,
    /** @stable ICU 2.0 */
    UCNV_EBCDIC_STATEFUL = 9,
    /** @stable ICU 2.0 */
    UCNV_ISO_2022 = 10,

    /** @stable ICU 2.0 */
    UCNV_LMBCS_1 = 11,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_2,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_3,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_4,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_5,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_6,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_8,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_11,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_16,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_17,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_18,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_19,
    /** @stable ICU 2.0 */
    UCNV_LMBCS_LAST = UCNV_LMBCS_19,
    /** @stable ICU 2.0 */
    UCNV_HZ,
    /** @stable ICU 2.0 */
    UCNV_SCSU,
    /** @stable ICU 2.0 */
    UCNV_ISCII,
    /** @stable ICU 2.0 */
    UCNV_US_ASCII,
    /** @stable ICU 2.0 */
    UCNV_UTF7,
    /** @stable ICU 2.2 */
    UCNV_BOCU1,
    /** @stable ICU 2.2 */
    UCNV_UTF16,
    /** @stable ICU 2.2 */
    UCNV_UTF32,
    /** @stable ICU 2.2 */
    UCNV_CESU8,
    /** @stable ICU 2.4 */
    UCNV_IMAP_MAILBOX,
    /** @stable ICU 4.8 */
    UCNV_COMPOUND_TEXT,

    /* Number of converter types for which we have conversion routines. */
    UCNV_NUMBER_OF_SUPPORTED_CONVERTER_TYPES
} UConverterType;

/**
 * Enum for specifying which platform a converter ID refers to.
 * The use of platform/CCSID is not recommended. See ucnv_openCCSID().
 *
 * @see ucnv_getPlatform
 * @see ucnv_openCCSID
 * @see ucnv_getCCSID
 * @stable ICU 2.0
 */
typedef enum {
    UCNV_UNKNOWN = -1,
    UCNV_IBM = 0
} UConverterPlatform;

// ucsdet.h

struct UCharsetDetector;
/**
  * Structure representing a charset detector
  * @stable ICU 3.6
  */
typedef struct UCharsetDetector UCharsetDetector;

struct UCharsetMatch;
/**
  *  Opaque structure representing a match that was identified
  *  from a charset detection operation.
  *  @stable ICU 3.6
  */
typedef struct UCharsetMatch UCharsetMatch;

// ubidi.h

/**
 * UBiDiLevel is the type of the level values in this
 * Bidi implementation.
 * It holds an embedding level and indicates the visual direction
 * by its bit&nbsp;0 (even/odd value).<p>
 *
 * It can also hold non-level values for the
 * <code>paraLevel</code> and <code>embeddingLevels</code>
 * arguments of <code>ubidi_setPara()</code>; there:
 * <ul>
 * <li>bit&nbsp;7 of an <code>embeddingLevels[]</code>
 * value indicates whether the using application is
 * specifying the level of a character to <i>override</i> whatever the
 * Bidi implementation would resolve it to.</li>
 * <li><code>paraLevel</code> can be set to the
 * pseudo-level values <code>UBIDI_DEFAULT_LTR</code>
 * and <code>UBIDI_DEFAULT_RTL</code>.</li>
 * </ul>
 *
 * @see ubidi_setPara
 *
 * <p>The related constants are not real, valid level values.
 * <code>UBIDI_DEFAULT_XXX</code> can be used to specify
 * a default for the paragraph level for
 * when the <code>ubidi_setPara()</code> function
 * shall determine it but there is no
 * strongly typed character in the input.<p>
 *
 * Note that the value for <code>UBIDI_DEFAULT_LTR</code> is even
 * and the one for <code>UBIDI_DEFAULT_RTL</code> is odd,
 * just like with normal LTR and RTL level values -
 * these special values are designed that way. Also, the implementation
 * assumes that UBIDI_MAX_EXPLICIT_LEVEL is odd.
 *
 * @see UBIDI_DEFAULT_LTR
 * @see UBIDI_DEFAULT_RTL
 * @see UBIDI_LEVEL_OVERRIDE
 * @see UBIDI_MAX_EXPLICIT_LEVEL
 * @stable ICU 2.0
 */
typedef uint8_t UBiDiLevel;

/** Paragraph level setting.<p>
 *
 * Constant indicating that the base direction depends on the first strong
 * directional character in the text according to the Unicode Bidirectional
 * Algorithm. If no strong directional character is present,
 * then set the paragraph level to 0 (left-to-right).<p>
 *
 * If this value is used in conjunction with reordering modes
 * <code>UBIDI_REORDER_INVERSE_LIKE_DIRECT</code> or
 * <code>UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL</code>, the text to reorder
 * is assumed to be visual LTR, and the text after reordering is required
 * to be the corresponding logical string with appropriate contextual
 * direction. The direction of the result string will be RTL if either
 * the righmost or leftmost strong character of the source text is RTL
 * or Arabic Letter, the direction will be LTR otherwise.<p>
 *
 * If reordering option <code>UBIDI_OPTION_INSERT_MARKS</code> is set, an RLM may
 * be added at the beginning of the result string to ensure round trip
 * (that the result string, when reordered back to visual, will produce
 * the original source text).
 * @see UBIDI_REORDER_INVERSE_LIKE_DIRECT
 * @see UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL
 * @stable ICU 2.0
 */
#define UBIDI_DEFAULT_LTR 0xfe

/** Paragraph level setting.<p>
 *
 * Constant indicating that the base direction depends on the first strong
 * directional character in the text according to the Unicode Bidirectional
 * Algorithm. If no strong directional character is present,
 * then set the paragraph level to 1 (right-to-left).<p>
 *
 * If this value is used in conjunction with reordering modes
 * <code>UBIDI_REORDER_INVERSE_LIKE_DIRECT</code> or
 * <code>UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL</code>, the text to reorder
 * is assumed to be visual LTR, and the text after reordering is required
 * to be the corresponding logical string with appropriate contextual
 * direction. The direction of the result string will be RTL if either
 * the righmost or leftmost strong character of the source text is RTL
 * or Arabic Letter, or if the text contains no strong character;
 * the direction will be LTR otherwise.<p>
 *
 * If reordering option <code>UBIDI_OPTION_INSERT_MARKS</code> is set, an RLM may
 * be added at the beginning of the result string to ensure round trip
 * (that the result string, when reordered back to visual, will produce
 * the original source text).
 * @see UBIDI_REORDER_INVERSE_LIKE_DIRECT
 * @see UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL
 * @stable ICU 2.0
 */
#define UBIDI_DEFAULT_RTL 0xff

/**
 * Maximum explicit embedding level.
 * (The maximum resolved level can be up to <code>UBIDI_MAX_EXPLICIT_LEVEL+1</code>).
 * @stable ICU 2.0
 */
#define UBIDI_MAX_EXPLICIT_LEVEL 125

/** Bit flag for level input.
 *  Overrides directional properties.
 * @stable ICU 2.0
 */
#define UBIDI_LEVEL_OVERRIDE 0x80

/**
 * Special value which can be returned by the mapping functions when a logical
 * index has no corresponding visual index or vice-versa. This may happen
 * for the logical-to-visual mapping of a Bidi control when option
 * <code>#UBIDI_OPTION_REMOVE_CONTROLS</code> is specified. This can also happen
 * for the visual-to-logical mapping of a Bidi mark (LRM or RLM) inserted
 * by option <code>#UBIDI_OPTION_INSERT_MARKS</code>.
 * @see ubidi_getVisualIndex
 * @see ubidi_getVisualMap
 * @see ubidi_getLogicalIndex
 * @see ubidi_getLogicalMap
 * @stable ICU 3.6
 */
#define UBIDI_MAP_NOWHERE (-1)

/**
 * <code>UBiDiDirection</code> values indicate the text direction.
 * @stable ICU 2.0
 */
enum UBiDiDirection {
    /** Left-to-right text. This is a 0 value.
   * <ul>
   * <li>As return value for <code>ubidi_getDirection()</code>, it means
   *     that the source string contains no right-to-left characters, or
   *     that the source string is empty and the paragraph level is even.
   * <li> As return value for <code>ubidi_getBaseDirection()</code>, it
   *      means that the first strong character of the source string has
   *      a left-to-right direction.
   * </ul>
   * @stable ICU 2.0
   */
    UBIDI_LTR,
    /** Right-to-left text. This is a 1 value.
   * <ul>
   * <li>As return value for <code>ubidi_getDirection()</code>, it means
   *     that the source string contains no left-to-right characters, or
   *     that the source string is empty and the paragraph level is odd.
   * <li> As return value for <code>ubidi_getBaseDirection()</code>, it
   *      means that the first strong character of the source string has
   *      a right-to-left direction.
   * </ul>
   * @stable ICU 2.0
   */
    UBIDI_RTL,
    /** Mixed-directional text.
   * <p>As return value for <code>ubidi_getDirection()</code>, it means
   *    that the source string contains both left-to-right and
   *    right-to-left characters.
   * @stable ICU 2.0
   */
    UBIDI_MIXED,
    /** No strongly directional text.
   * <p>As return value for <code>ubidi_getBaseDirection()</code>, it means
   *    that the source string is missing or empty, or contains neither left-to-right
   *    nor right-to-left characters.
   * @stable ICU 4.6
   */
    UBIDI_NEUTRAL
};

/** @stable ICU 2.0 */
typedef enum UBiDiDirection UBiDiDirection;

/**
 * Forward declaration of the <code>UBiDi</code> structure for the declaration of
 * the API functions. Its fields are implementation-specific.<p>
 * This structure holds information about a paragraph (or multiple paragraphs)
 * of text with Bidi-algorithm-related details, or about one line of
 * such a paragraph.<p>
 * Reordering can be done on a line, or on one or more paragraphs which are
 * then interpreted each as one single line.
 * @stable ICU 2.0
 */
struct UBiDi;

/** @stable ICU 2.0 */
typedef struct UBiDi UBiDi;

// upluralrules.h
/**
 * Type of plurals and PluralRules.
 * @stable ICU 50
 */
enum UPluralType {
    /**
     * Plural rules for cardinal numbers: 1 file vs. 2 files.
     * @stable ICU 50
     */
    UPLURAL_TYPE_CARDINAL,
    /**
     * Plural rules for ordinal numbers: 1st file, 2nd file, 3rd file, 4th file, etc.
     * @stable ICU 50
     */
    UPLURAL_TYPE_ORDINAL,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UPluralType value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UPLURAL_TYPE_COUNT
#endif /* U_HIDE_DEPRECATED_API */
};
/**
 * @stable ICU 50
 */
typedef enum UPluralType UPluralType;

/**
 * Opaque UPluralRules object for use in C programs.
 * @stable ICU 4.8
 */
struct UPluralRules;
typedef struct UPluralRules UPluralRules; /**< C typedef for struct UPluralRules. @stable ICU 4.8 */

// ures.h
/**
 * UResourceBundle is an opaque type for handles for resource bundles in C APIs.
 * @stable ICU 2.0
 */
struct UResourceBundle;

/**
 * @stable ICU 2.0
 */
typedef struct UResourceBundle UResourceBundle;

/**
 * Numeric constants for types of resource items.
 * @see ures_getType
 * @stable ICU 2.0
 */
typedef enum {
    /** Resource type constant for "no resource". @stable ICU 2.6 */
    URES_NONE = -1,

    /** Resource type constant for 16-bit Unicode strings. @stable ICU 2.6 */
    URES_STRING = 0,

    /** Resource type constant for binary data. @stable ICU 2.6 */
    URES_BINARY = 1,

    /** Resource type constant for tables of key-value pairs. @stable ICU 2.6 */
    URES_TABLE = 2,

    /**
     * Resource type constant for aliases;
     * internally stores a string which identifies the actual resource
     * storing the data (can be in a different resource bundle).
     * Resolved internally before delivering the actual resource through the API.
     * @stable ICU 2.6
     */
    URES_ALIAS = 3,

    /**
     * Resource type constant for a single 28-bit integer, interpreted as
     * signed or unsigned by the ures_getInt() or ures_getUInt() function.
     * @see ures_getInt
     * @see ures_getUInt
     * @stable ICU 2.6
     */
    URES_INT = 7,

    /** Resource type constant for arrays of resources. @stable ICU 2.6 */
    URES_ARRAY = 8,

    /**
     * Resource type constant for vectors of 32-bit integers.
     * @see ures_getIntVector
     * @stable ICU 2.6
     */
    URES_INT_VECTOR = 14,
#ifndef U_HIDE_DEPRECATED_API
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_NONE = URES_NONE,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_STRING = URES_STRING,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_BINARY = URES_BINARY,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_TABLE = URES_TABLE,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_ALIAS = URES_ALIAS,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_INT = URES_INT,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_ARRAY = URES_ARRAY,
    /** @deprecated ICU 2.6 Use the URES_ constant instead. */
    RES_INT_VECTOR = URES_INT_VECTOR,
    /** @deprecated ICU 2.6 Not used. */
    RES_RESERVED = 15,

    /**
     * One more than the highest normal UResType value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    URES_LIMIT = 16
#endif // U_HIDE_DEPRECATED_API
} UResType;

// unumberformatter.h
/**
 * An enum declaring how to render units, including currencies. Example outputs when formatting 123 USD and 123
 * meters in <em>en-CA</em>:
 *
 * <p>
 * <ul>
 * <li>NARROW*: "$123.00" and "123 m"
 * <li>SHORT: "US$ 123.00" and "123 m"
 * <li>FULL_NAME: "123.00 US dollars" and "123 meters"
 * <li>ISO_CODE: "USD 123.00" and undefined behavior
 * <li>HIDDEN: "123.00" and "123"
 * </ul>
 *
 * <p>
 * This enum is similar to {@link UMeasureFormatWidth}.
 *
 * @stable ICU 60
 */
typedef enum UNumberUnitWidth {
    /**
     * Print an abbreviated version of the unit name. Similar to SHORT, but always use the shortest available
     * abbreviation or symbol. This option can be used when the context hints at the identity of the unit. For more
     * information on the difference between NARROW and SHORT, see SHORT.
     *
     * <p>
     * In CLDR, this option corresponds to the "Narrow" format for measure units and the "¤¤¤¤¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
    UNUM_UNIT_WIDTH_NARROW,

    /**
     * Print an abbreviated version of the unit name. Similar to NARROW, but use a slightly wider abbreviation or
     * symbol when there may be ambiguity. This is the default behavior.
     *
     * <p>
     * For example, in <em>es-US</em>, the SHORT form for Fahrenheit is "{0} °F", but the NARROW form is "{0}°",
     * since Fahrenheit is the customary unit for temperature in that locale.
     *
     * <p>
     * In CLDR, this option corresponds to the "Short" format for measure units and the "¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
    UNUM_UNIT_WIDTH_SHORT,

    /**
     * Print the full name of the unit, without any abbreviations.
     *
     * <p>
     * In CLDR, this option corresponds to the default format for measure units and the "¤¤¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
    UNUM_UNIT_WIDTH_FULL_NAME,

    /**
     * Use the three-digit ISO XXX code in place of the symbol for displaying currencies. The behavior of this
     * option is currently undefined for use with measure units.
     *
     * <p>
     * In CLDR, this option corresponds to the "¤¤" placeholder for currencies.
     *
     * @stable ICU 60
     */
    UNUM_UNIT_WIDTH_ISO_CODE,

    /**
     * Format the number according to the specified unit, but do not display the unit. For currencies, apply
     * monetary symbols and formats as with SHORT, but omit the currency symbol. For measure units, the behavior is
     * equivalent to not specifying the unit at all.
     *
     * @stable ICU 60
     */
    UNUM_UNIT_WIDTH_HIDDEN,

    /**
     * One more than the highest UNumberUnitWidth value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
    UNUM_UNIT_WIDTH_COUNT
} UNumberUnitWidth;

/**
 * An enum declaring the strategy for when and how to display grouping separators (i.e., the
 * separator, often a comma or period, after every 2-3 powers of ten). The choices are several
 * pre-built strategies for different use cases that employ locale data whenever possible. Example
 * outputs for 1234 and 1234567 in <em>en-IN</em>:
 *
 * <ul>
 * <li>OFF: 1234 and 12345
 * <li>MIN2: 1234 and 12,34,567
 * <li>AUTO: 1,234 and 12,34,567
 * <li>ON_ALIGNED: 1,234 and 12,34,567
 * <li>THOUSANDS: 1,234 and 1,234,567
 * </ul>
 *
 * <p>
 * The default is AUTO, which displays grouping separators unless the locale data says that grouping
 * is not customary. To force grouping for all numbers greater than 1000 consistently across locales,
 * use ON_ALIGNED. On the other hand, to display grouping less frequently than the default, use MIN2
 * or OFF. See the docs of each option for details.
 *
 * <p>
 * Note: This enum specifies the strategy for grouping sizes. To set which character to use as the
 * grouping separator, use the "symbols" setter.
 *
 * @stable ICU 63
 */
typedef enum UNumberGroupingStrategy {
    /**
     * Do not display grouping separators in any locale.
     *
     * @stable ICU 61
     */
    UNUM_GROUPING_OFF,

    /**
     * Display grouping using locale defaults, except do not show grouping on values smaller than
     * 10000 (such that there is a <em>minimum of two digits</em> before the first separator).
     *
     * <p>
     * Note that locales may restrict grouping separators to be displayed only on 1 million or
     * greater (for example, ee and hu) or disable grouping altogether (for example, bg currency).
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
    UNUM_GROUPING_MIN2,

    /**
     * Display grouping using the default strategy for all locales. This is the default behavior.
     *
     * <p>
     * Note that locales may restrict grouping separators to be displayed only on 1 million or
     * greater (for example, ee and hu) or disable grouping altogether (for example, bg currency).
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
    UNUM_GROUPING_AUTO,

    /**
     * Always display the grouping separator on values of at least 1000.
     *
     * <p>
     * This option ignores the locale data that restricts or disables grouping, described in MIN2 and
     * AUTO. This option may be useful to normalize the alignment of numbers, such as in a
     * spreadsheet.
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
    UNUM_GROUPING_ON_ALIGNED,

    /**
     * Use the Western defaults: groups of 3 and enabled for all numbers 1000 or greater. Do not use
     * locale data for determining the grouping strategy.
     *
     * @stable ICU 61
     */
    UNUM_GROUPING_THOUSANDS

#ifndef U_HIDE_INTERNAL_API
    ,
    /**
     * One more than the highest UNumberGroupingStrategy value.
     *
     * @internal ICU 62: The numeric value may change over time; see ICU ticket #12420.
     */
    UNUM_GROUPING_COUNT
#endif /* U_HIDE_INTERNAL_API */

} UNumberGroupingStrategy;

/**
 * An enum declaring how to denote positive and negative numbers. Example outputs when formatting
 * 123, 0, and -123 in <em>en-US</em>:
 *
 * <ul>
 * <li>AUTO: "123", "0", and "-123"
 * <li>ALWAYS: "+123", "+0", and "-123"
 * <li>NEVER: "123", "0", and "123"
 * <li>ACCOUNTING: "$123", "$0", and "($123)"
 * <li>ACCOUNTING_ALWAYS: "+$123", "+$0", and "($123)"
 * <li>EXCEPT_ZERO: "+123", "0", and "-123"
 * <li>ACCOUNTING_EXCEPT_ZERO: "+$123", "$0", and "($123)"
 * </ul>
 *
 * <p>
 * The exact format, including the position and the code point of the sign, differ by locale.
 *
 * @stable ICU 60
 */
typedef enum UNumberSignDisplay {
    /**
     * Show the minus sign on negative numbers, and do not show the sign on positive numbers. This is the default
     * behavior.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_AUTO,

    /**
     * Show the minus sign on negative numbers and the plus sign on positive numbers, including zero.
     * To hide the sign on zero, see {@link UNUM_SIGN_EXCEPT_ZERO}.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ALWAYS,

    /**
     * Do not show the sign on positive or negative numbers.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_NEVER,

    /**
     * Use the locale-dependent accounting format on negative numbers, and do not show the sign on positive numbers.
     *
     * <p>
     * The accounting format is defined in CLDR and varies by locale; in many Western locales, the format is a pair
     * of parentheses around the number.
     *
     * <p>
     * Note: Since CLDR defines the accounting format in the monetary context only, this option falls back to the
     * AUTO sign display strategy when formatting without a currency unit. This limitation may be lifted in the
     * future.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ACCOUNTING,

    /**
     * Use the locale-dependent accounting format on negative numbers, and show the plus sign on
     * positive numbers, including zero. For more information on the accounting format, see the
     * ACCOUNTING sign display strategy. To hide the sign on zero, see
     * {@link UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO}.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ACCOUNTING_ALWAYS,

    /**
     * Show the minus sign on negative numbers and the plus sign on positive numbers. Do not show a
     * sign on zero, numbers that round to zero, or NaN.
     *
     * @stable ICU 61
     */
    UNUM_SIGN_EXCEPT_ZERO,

    /**
     * Use the locale-dependent accounting format on negative numbers, and show the plus sign on
     * positive numbers. Do not show a sign on zero, numbers that round to zero, or NaN. For more
     * information on the accounting format, see the ACCOUNTING sign display strategy.
     *
     * @stable ICU 61
     */
    UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO,

    /**
     * One more than the highest UNumberSignDisplay value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
    UNUM_SIGN_COUNT
} UNumberSignDisplay;

/**
 * An enum declaring how to render the decimal separator.
 *
 * <p>
 * <ul>
 * <li>UNUM_DECIMAL_SEPARATOR_AUTO: "1", "1.1"
 * <li>UNUM_DECIMAL_SEPARATOR_ALWAYS: "1.", "1.1"
 * </ul>
 *
 * @stable ICU 60
 */
typedef enum UNumberDecimalSeparatorDisplay {
    /**
     * Show the decimal separator when there are one or more digits to display after the separator, and do not show
     * it otherwise. This is the default behavior.
     *
     * @stable ICU 60
     */
    UNUM_DECIMAL_SEPARATOR_AUTO,

    /**
     * Always show the decimal separator, even if there are no digits to display after the separator.
     *
     * @stable ICU 60
     */
    UNUM_DECIMAL_SEPARATOR_ALWAYS,

    /**
     * One more than the highest UNumberDecimalSeparatorDisplay value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
    UNUM_DECIMAL_SEPARATOR_COUNT
} UNumberDecimalSeparatorDisplay;

struct UNumberFormatter;
/**
 * C-compatible version of icu::number::LocalizedNumberFormatter.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @stable ICU 62
 */
typedef struct UNumberFormatter UNumberFormatter;

struct UFormattedNumber;
/**
 * C-compatible version of icu::number::FormattedNumber.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @stable ICU 62
 */
typedef struct UFormattedNumber UFormattedNumber;

// ucurr.h
/**
 * Currency Usage used for Decimal Format
 * @stable ICU 54
 */
enum UCurrencyUsage {
    /**
     * a setting to specify currency usage which determines currency digit
     * and rounding for standard usage, for example: "50.00 NT$"
     * used as DEFAULT value
     * @stable ICU 54
     */
    UCURR_USAGE_STANDARD = 0,
    /**
     * a setting to specify currency usage which determines currency digit
     * and rounding for cash usage, for example: "50 NT$"
     * @stable ICU 54
     */
    UCURR_USAGE_CASH = 1,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One higher than the last enum UCurrencyUsage constant.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UCURR_USAGE_COUNT = 2
#endif // U_HIDE_DEPRECATED_API
};
typedef enum UCurrencyUsage UCurrencyUsage;

/**
 * Selector constants for ucurr_getName().
 *
 * @see ucurr_getName
 * @stable ICU 2.6
 */
typedef enum UCurrNameStyle {
    /**
     * Selector for ucurr_getName indicating a symbolic name for a
     * currency, such as "$" for USD.
     * @stable ICU 2.6
     */
    UCURR_SYMBOL_NAME,

    /**
     * Selector for ucurr_getName indicating the long name for a
     * currency, such as "US Dollar" for USD.
     * @stable ICU 2.6
     */
    UCURR_LONG_NAME

#ifndef U_HIDE_DRAFT_API
    ,
    /**
     * Selector for getName() indicating the narrow currency symbol.
     * The narrow currency symbol is similar to the regular currency
     * symbol, but it always takes the shortest form: for example,
     * "$" instead of "US$" for USD in en-CA.
     *
     * @draft ICU 61
     */
    UCURR_NARROW_SYMBOL_NAME
#endif // U_HIDE_DRAFT_API
} UCurrNameStyle;

/**
 * @stable ICU 2.6
 */
typedef const void *UCurrRegistryKey;

// uformattedvalue.h
/**
 * All possible field categories in ICU. Every entry in this enum corresponds
 * to another enum that exists in ICU.
 *
 * In the APIs that take a UFieldCategory, an int32_t type is used. Field
 * categories having any of the top four bits turned on are reserved as
 * private-use for external APIs implementing FormattedValue. This means that
 * categories 2^28 and higher or below zero (with the highest bit turned on)
 * are private-use and will not be used by ICU in the future.
 *
 * @draft ICU 64
 */
typedef enum UFieldCategory {
    /**
     * For an undefined field category.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_UNDEFINED = 0,

    /**
     * For fields in UDateFormatField (udat.h), from ICU 3.0.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_DATE,

    /**
     * For fields in UNumberFormatFields (unum.h), from ICU 49.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_NUMBER,

    /**
     * For fields in UListFormatterField (ulistformatter.h), from ICU 63.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_LIST,

    /**
     * For fields in URelativeDateTimeFormatterField (ureldatefmt.h), from ICU 64.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_RELATIVE_DATETIME,

    /**
     * Reserved for possible future fields in UDateIntervalFormatField.
     *
     * @internal
     */
    UFIELD_CATEGORY_DATE_INTERVAL,

#ifndef U_HIDE_INTERNAL_API
    /** @internal */
    UFIELD_CATEGORY_COUNT,
#endif /* U_HIDE_INTERNAL_API */

    /**
     * Category for spans in a list.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_LIST_SPAN = 0x1000 + UFIELD_CATEGORY_LIST,

    /**
     * Category for spans in a date interval.
     *
     * @draft ICU 64
     */
    UFIELD_CATEGORY_DATE_INTERVAL_SPAN = 0x1000 + UFIELD_CATEGORY_DATE_INTERVAL,

} UFieldCategory;


struct UConstrainedFieldPosition;
/**
 * Represents a span of a string containing a given field.
 *
 * This struct differs from UFieldPosition in the following ways:
 *
 *   1. It has information on the field category.
 *   2. It allows you to set constraints to use when iterating over field positions.
 *   3. It is used for the newer FormattedValue APIs.
 *
 * @draft ICU 64
 */
typedef struct UConstrainedFieldPosition UConstrainedFieldPosition;

struct UFormattedValue;
/**
 * An abstract formatted value: a string with associated field attributes.
 * Many formatters format to types compatible with UFormattedValue.
 *
 * @draft ICU 64
 */
typedef struct UFormattedValue UFormattedValue;

// ureldatefmt.h

/**
 * The formatting style
 * @stable ICU 54
 */
typedef enum UDateRelativeDateTimeFormatterStyle {
    /**
   * Everything spelled out.
   * @stable ICU 54
   */
    UDAT_STYLE_LONG,

    /**
   * Abbreviations used when possible.
   * @stable ICU 54
   */
    UDAT_STYLE_SHORT,

    /**
   * Use the shortest possible form.
   * @stable ICU 54
   */
    UDAT_STYLE_NARROW,

#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UDateRelativeDateTimeFormatterStyle value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDAT_STYLE_COUNT
#endif /* U_HIDE_DEPRECATED_API */
} UDateRelativeDateTimeFormatterStyle;

/**
 * Represents the unit for formatting a relative date. e.g "in 5 days"
 * or "next year"
 * @stable ICU 57
 */
typedef enum URelativeDateTimeUnit {
    /**
     * Specifies that relative unit is year, e.g. "last year",
     * "in 5 years".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_YEAR,
    /**
     * Specifies that relative unit is quarter, e.g. "last quarter",
     * "in 5 quarters".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_QUARTER,
    /**
     * Specifies that relative unit is month, e.g. "last month",
     * "in 5 months".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_MONTH,
    /**
     * Specifies that relative unit is week, e.g. "last week",
     * "in 5 weeks".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_WEEK,
    /**
     * Specifies that relative unit is day, e.g. "yesterday",
     * "in 5 days".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_DAY,
    /**
     * Specifies that relative unit is hour, e.g. "1 hour ago",
     * "in 5 hours".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_HOUR,
    /**
     * Specifies that relative unit is minute, e.g. "1 minute ago",
     * "in 5 minutes".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_MINUTE,
    /**
     * Specifies that relative unit is second, e.g. "1 second ago",
     * "in 5 seconds".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_SECOND,
    /**
     * Specifies that relative unit is Sunday, e.g. "last Sunday",
     * "this Sunday", "next Sunday", "in 5 Sundays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_SUNDAY,
    /**
     * Specifies that relative unit is Monday, e.g. "last Monday",
     * "this Monday", "next Monday", "in 5 Mondays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_MONDAY,
    /**
     * Specifies that relative unit is Tuesday, e.g. "last Tuesday",
     * "this Tuesday", "next Tuesday", "in 5 Tuesdays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_TUESDAY,
    /**
     * Specifies that relative unit is Wednesday, e.g. "last Wednesday",
     * "this Wednesday", "next Wednesday", "in 5 Wednesdays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_WEDNESDAY,
    /**
     * Specifies that relative unit is Thursday, e.g. "last Thursday",
     * "this Thursday", "next Thursday", "in 5 Thursdays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_THURSDAY,
    /**
     * Specifies that relative unit is Friday, e.g. "last Friday",
     * "this Friday", "next Friday", "in 5 Fridays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_FRIDAY,
    /**
     * Specifies that relative unit is Saturday, e.g. "last Saturday",
     * "this Saturday", "next Saturday", "in 5 Saturdays".
     * @stable ICU 57
     */
    UDAT_REL_UNIT_SATURDAY,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal URelativeDateTimeUnit value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDAT_REL_UNIT_COUNT
#endif /* U_HIDE_DEPRECATED_API */
} URelativeDateTimeUnit;

#ifndef U_HIDE_DRAFT_API
/**
 * FieldPosition and UFieldPosition selectors for format fields
 * defined by RelativeDateTimeFormatter.
 * @draft ICU 64
 */
typedef enum URelativeDateTimeFormatterField {
    /**
     * Represents a literal text string, like "tomorrow" or "days ago".
     * @draft ICU 64
     */
    UDAT_REL_LITERAL_FIELD,
    /**
     * Represents a number quantity, like "3" in "3 days ago".
     * @draft ICU 64
     */
    UDAT_REL_NUMERIC_FIELD,
} URelativeDateTimeFormatterField;
#endif // U_HIDE_DRAFT_API


/**
 * Opaque URelativeDateTimeFormatter object for use in C programs.
 * @stable ICU 57
 */
struct URelativeDateTimeFormatter;
typedef struct URelativeDateTimeFormatter URelativeDateTimeFormatter; /**< C typedef for struct URelativeDateTimeFormatter. @stable ICU 57 */

#ifndef U_HIDE_DRAFT_API
struct UFormattedRelativeDateTime;
/**
 * Opaque struct to contain the results of a URelativeDateTimeFormatter operation.
 * @draft ICU 64
 */
typedef struct UFormattedRelativeDateTime UFormattedRelativeDateTime;

#endif

// uldnames.h
/**
 * Enum used in LocaleDisplayNames::createInstance.
 * @stable ICU 4.4
 */
typedef enum {
    /**
     * Use standard names when generating a locale name,
     * e.g. en_GB displays as 'English (United Kingdom)'.
     * @stable ICU 4.4
     */
    ULDN_STANDARD_NAMES = 0,
    /**
     * Use dialect names, when generating a locale name,
     * e.g. en_GB displays as 'British English'.
     * @stable ICU 4.4
     */
    ULDN_DIALECT_NAMES
} UDialectHandling;

/**
 * Opaque C service object type for the locale display names API
 * @stable ICU 4.4
 */
struct ULocaleDisplayNames;

/**
 * C typedef for struct ULocaleDisplayNames.
 * @stable ICU 4.4
 */
typedef struct ULocaleDisplayNames ULocaleDisplayNames;

// uversion.h

/** Maximum length of the copyright string.
 *  @stable ICU 2.4
 */
#define U_COPYRIGHT_STRING_LENGTH 128

/** An ICU version consists of up to 4 numbers from 0..255.
 *  @stable ICU 2.4
 */
#define U_MAX_VERSION_LENGTH 4

/** In a string, ICU version fields are delimited by dots.
 *  @stable ICU 2.4
 */
#define U_VERSION_DELIMITER '.'

/** The maximum length of an ICU version string.
 *  @stable ICU 2.4
 */
#define U_MAX_VERSION_STRING_LENGTH 20

/** The binary form of a version on ICU APIs is an array of 4 uint8_t.
 *  To compare two versions, use memcmp(v1,v2,sizeof(UVersionInfo)).
 *  @stable ICU 2.4
 */
typedef uint8_t UVersionInfo[U_MAX_VERSION_LENGTH];

#endif
