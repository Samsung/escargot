// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "String.h"
#include "Value.h"

#include "fast-dtoa.h"
#include "bignum-dtoa.h"

namespace Escargot {

String* String::emptyString;

size_t g_asciiStringTag;
size_t g_latin1StringTag;
size_t g_utf16StringTag;
size_t g_ropeStringTag;
size_t g_stringViewTag;

bool isAllASCII(const char* buf, const size_t& len)
{
    for (unsigned i = 0; i < len; i++) {
        if ((buf[i] & 0x80) != 0) {
            return false;
        }
    }
    return true;
}

bool isAllASCII(const char16_t* buf, const size_t& len)
{
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] >= 128) {
            return false;
        }
    }
    return true;
}

bool isAllLatin1(const char16_t* buf, const size_t& len)
{
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] >= 256) {
            return false;
        }
    }
    return true;
}

bool isIndexString(String* str)
{
    size_t len = str->length();
    for (unsigned i = 0; i < len; i++) {
        char16_t c = str->charAt(i);
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

static const char32_t offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 0x03C82080UL, static_cast<char32_t>(0xFA082080UL), static_cast<char32_t>(0x82082080UL) };

char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen)
{
    unsigned length;
    const char sch = *sequence;
    valid = true;
    if ((sch & 0x80) == 0)
        length = 1;
    else {
        unsigned char ch2 = static_cast<unsigned char>(*(sequence + 1));
        if ((sch & 0xE0) == 0xC0
            && (ch2 & 0xC0) == 0x80)
            length = 2;
        else {
            unsigned char ch3 = static_cast<unsigned char>(*(sequence + 2));
            if ((sch & 0xF0) == 0xE0
                && (ch2 & 0xC0) == 0x80
                && (ch3 & 0xC0) == 0x80)
                length = 3;
            else {
                unsigned char ch4 = static_cast<unsigned char>(*(sequence + 3));
                if ((sch & 0xF8) == 0xF0
                    && (ch2 & 0xC0) == 0x80
                    && (ch3 & 0xC0) == 0x80
                    && (ch4 & 0xC0) == 0x80)
                    length = 4;
                else {
                    valid = false;
                    (*sequence++);
                    return -1;
                }
            }
        }
    }

    charlen = length;
    char32_t ch = 0;
    switch (length) {
    case 4:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // FALLTHROUGH;
    case 3:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // FALLTHROUGH;
    case 2:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // FALLTHROUGH;
    case 1:
        ch += static_cast<unsigned char>(*sequence++);
    }
    return ch - offsetsFromUTF8[length - 1];
}

UTF16StringData utf8StringToUTF16String(const char* buf, const size_t& len)
{
    UTF16StringDataNonGCStd str;
    const char* source = buf;
    int charlen;
    bool valid;
    while (source < buf + len) {
        char32_t ch = readUTF8Sequence(source, valid, charlen);
        if (!valid) { // Invalid sequence
            str += 0xFFFD;
        } else if (((uint32_t)(ch) <= 0xffff)) { // BMP
            if ((((ch)&0xfffff800) == 0xd800)) { // SURROGATE
                str += 0xFFFD;
                source -= (charlen - 1);
            } else {
                str += ch; // normal case
            }
        } else if (((uint32_t)((ch)-0x10000) <= 0xfffff)) { // SUPPLEMENTARY
            str += (char16_t)(((ch) >> 10) + 0xd7c0); // LEAD
            str += (char16_t)(((ch)&0x3ff) | 0xdc00); // TRAIL
        } else {
            str += 0xFFFD;
            source -= (charlen - 1);
        }
    }

    return UTF16StringData(str.data(), str.length());
}

ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len)
{
    ASCIIStringData str;
    str.resizeWithUninitializedValues(len);
    for (unsigned i = 0; i < len; i++) {
        ASSERT(buf[i] < 128);
        str[i] = buf[i];
    }
    return ASCIIStringData(std::move(str));
}

size_t utf32ToUtf8(char32_t uc, char* UTF8)
{
    size_t tRequiredSize = 0;

    if (uc <= 0x7f) {
        if (NULL != UTF8) {
            UTF8[0] = (char)uc;
            UTF8[1] = (char)'\0';
        }
        tRequiredSize = 1;
    } else if (uc <= 0x7ff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xc0 + uc / (0x01 << 6));
            UTF8[1] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[2] = (char)'\0';
        }
        tRequiredSize = 2;
    } else if (uc <= 0xffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xe0 + uc / (0x01 << 12));
            UTF8[1] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[2] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[3] = (char)'\0';
        }
        tRequiredSize = 3;
    } else if (uc <= 0x1fffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xf0 + uc / (0x01 << 18));
            UTF8[1] = (char)(0x80 + uc / (0x01 << 12) % (0x01 << 12));
            UTF8[2] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[3] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[4] = (char)'\0';
        }
        tRequiredSize = 4;
    } else if (uc <= 0x3ffffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xf8 + uc / (0x01 << 24));
            UTF8[1] = (char)(0x80 + uc / (0x01 << 18) % (0x01 << 18));
            UTF8[2] = (char)(0x80 + uc / (0x01 << 12) % (0x01 << 12));
            UTF8[3] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[4] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[5] = (char)'\0';
        }
        tRequiredSize = 5;
    } else if (uc <= 0x7fffffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xfc + uc / (0x01 << 30));
            UTF8[1] = (char)(0x80 + uc / (0x01 << 24) % (0x01 << 24));
            UTF8[2] = (char)(0x80 + uc / (0x01 << 18) % (0x01 << 18));
            UTF8[3] = (char)(0x80 + uc / (0x01 << 12) % (0x01 << 12));
            UTF8[4] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[5] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[6] = (char)'\0';
        }
        tRequiredSize = 6;
    } else {
        return utf32ToUtf8(0xFFFD, UTF8);
    }

    return tRequiredSize;
}

UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len)
{
    UTF8StringDataNonGCStd str;
    str.reserve(len);
    for (unsigned i = 0; i < len;) {
        if (buf[i] < 128) {
            str += buf[i];
            i++;
        } else {
            char32_t c;
            U16_NEXT(buf, i, len, c);

            char buf[8];
            utf32ToUtf8(c, buf);
            str += buf;
        }
    }
    return UTF8StringData(str.data(), str.length());
}

UTF8StringDataNonGCStd utf16StringToUTF8NonGCString(const char16_t* buf, const size_t& len)
{
    UTF8StringDataNonGCStd str;
    str.reserve(len);
    for (unsigned i = 0; i < len;) {
        if (buf[i] < 128) {
            str += buf[i];
            i++;
        } else {
            char32_t c;
            U16_NEXT(buf, i, len, c);

            char buf[8];
            utf32ToUtf8(c, buf);
            str += buf;
        }
    }
    return str;
}

UTF16StringData ASCIIString::toUTF16StringData() const
{
    UTF16StringData ret;
    size_t len = length();
    ret.resizeWithUninitializedValues(len);
    for (size_t i = 0; i < len; i++) {
        ret[i] = charAt(i);
    }
    return ret;
}

UTF8StringData ASCIIString::toUTF8StringData() const
{
    return m_stringData;
}

UTF8StringDataNonGCStd ASCIIString::toNonGCUTF8StringData() const
{
    return UTF8StringDataNonGCStd(m_stringData.data(), m_stringData.length());
}

UTF16StringData Latin1String::toUTF16StringData() const
{
    UTF16StringData ret;
    size_t len = length();
    ret.resizeWithUninitializedValues(len);
    for (size_t i = 0; i < len; i++) {
        ret[i] = charAt(i);
    }
    return ret;
}

UTF8StringData Latin1String::toUTF8StringData() const
{
    UTF8StringData ret;
    size_t len = length();
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = m_stringData[i]; /* assume that code points above 0xff are impossible since latin-1 is 8-bit */
        if (ch < 0x80) {
            ret.append((char*)&ch, 1);
        } else {
            char buf[8];
            auto len = utf32ToUtf8(ch, buf);
            ret.append(buf, len);
        }
    }
    return ret;
}

UTF8StringDataNonGCStd Latin1String::toNonGCUTF8StringData() const
{
    UTF8StringDataNonGCStd ret;
    size_t len = length();
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = m_stringData[i]; /* assume that code points above 0xff are impossible since latin-1 is 8-bit */
        if (ch < 0x80) {
            ret.append((char*)&ch, 1);
        } else {
            char buf[8];
            auto len = utf32ToUtf8(ch, buf);
            ret.append(buf, len);
        }
    }
    return ret;
}

UTF16StringData UTF16String::toUTF16StringData() const
{
    return m_stringData;
}

UTF8StringData UTF16String::toUTF8StringData() const
{
    return utf16StringToUTF8String(m_stringData.data(), m_stringData.length());
}

UTF8StringDataNonGCStd UTF16String::toNonGCUTF8StringData() const
{
    return utf16StringToUTF8NonGCString(m_stringData.data(), m_stringData.length());
}

enum Flags {
    NO_FLAGS = 0,
    EMIT_POSITIVE_EXPONENT_SIGN = 1,
    EMIT_TRAILING_DECIMAL_POINT = 2,
    EMIT_TRAILING_ZERO_AFTER_POINT = 4,
    UNIQUE_ZERO = 8
};

void CreateDecimalRepresentation(
    int flags_,
    const char* decimal_digits,
    int length,
    int decimal_point,
    int digits_after_point,
    double_conversion::StringBuilder* result_builder)
{
    // Create a representation that is padded with zeros if needed.
    if (decimal_point <= 0) {
        // "0.00000decimal_rep".
        result_builder->AddCharacter('0');
        if (digits_after_point > 0) {
            result_builder->AddCharacter('.');
            result_builder->AddPadding('0', -decimal_point);
            ASSERT(length <= digits_after_point - (-decimal_point));
            result_builder->AddSubstring(decimal_digits, length);
            int remaining_digits = digits_after_point - (-decimal_point) - length;
            result_builder->AddPadding('0', remaining_digits);
        }
    } else if (decimal_point >= length) {
        // "decimal_rep0000.00000" or "decimal_rep.0000"
        result_builder->AddSubstring(decimal_digits, length);
        result_builder->AddPadding('0', decimal_point - length);
        if (digits_after_point > 0) {
            result_builder->AddCharacter('.');
            result_builder->AddPadding('0', digits_after_point);
        }
    } else {
        // "decima.l_rep000"
        ASSERT(digits_after_point > 0);
        result_builder->AddSubstring(decimal_digits, decimal_point);
        result_builder->AddCharacter('.');
        ASSERT(length - decimal_point <= digits_after_point);
        result_builder->AddSubstring(&decimal_digits[decimal_point],
                                     length - decimal_point);
        int remaining_digits = digits_after_point - (length - decimal_point);
        result_builder->AddPadding('0', remaining_digits);
    }
    if (digits_after_point == 0) {
        if ((flags_ & EMIT_TRAILING_DECIMAL_POINT) != 0) {
            result_builder->AddCharacter('.');
        }
        if ((flags_ & EMIT_TRAILING_ZERO_AFTER_POINT) != 0) {
            result_builder->AddCharacter('0');
        }
    }
}

void CreateExponentialRepresentation(
    int flags_,
    const char* decimal_digits,
    int length,
    int exponent,
    double_conversion::StringBuilder* result_builder)
{
    ASSERT(length != 0);
    result_builder->AddCharacter(decimal_digits[0]);
    if (length != 1) {
        result_builder->AddCharacter('.');
        result_builder->AddSubstring(&decimal_digits[1], length - 1);
    }
    result_builder->AddCharacter('e');
    if (exponent < 0) {
        result_builder->AddCharacter('-');
        exponent = -exponent;
    } else {
        if ((flags_ & EMIT_POSITIVE_EXPONENT_SIGN) != 0) {
            result_builder->AddCharacter('+');
        }
    }
    if (exponent == 0) {
        result_builder->AddCharacter('0');
        return;
    }
    ASSERT(exponent < 1e4);
    const int kMaxExponentLength = 5;
    char buffer[kMaxExponentLength + 1];
    buffer[kMaxExponentLength] = '\0';
    int first_char_pos = kMaxExponentLength;
    while (exponent > 0) {
        buffer[--first_char_pos] = '0' + (exponent % 10);
        exponent /= 10;
    }
    result_builder->AddSubstring(&buffer[first_char_pos],
                                 kMaxExponentLength - first_char_pos);
}

ASCIIStringData dtoa(double number)
{
    if (number == 0) {
        return ASCIIStringData("0", 1);
    }
    const int flags = UNIQUE_ZERO | EMIT_POSITIVE_EXPONENT_SIGN;
    bool sign = false;
    if (number < 0) {
        sign = true;
        number = -number;
    }
    // The maximal number of digits that are needed to emit a double in base 10.
    // A higher precision can be achieved by using more digits, but the shortest
    // accurate representation of any double will never use more digits than
    // kBase10MaximalLength.
    // Note that DoubleToAscii null-terminates its input. So the given buffer
    // should be at least kBase10MaximalLength + 1 characters long.
    const int kBase10MaximalLength = 17;
    const int kDecimalRepCapacity = kBase10MaximalLength + 1;
    char decimal_rep[kDecimalRepCapacity];
    int decimal_rep_length;
    int decimal_point;
    double_conversion::Vector<char> vector(decimal_rep, kDecimalRepCapacity);
    bool fast_worked = FastDtoa(number, double_conversion::FAST_DTOA_SHORTEST, 0, vector, &decimal_rep_length, &decimal_point);
    if (!fast_worked) {
        BignumDtoa(number, double_conversion::BIGNUM_DTOA_SHORTEST, 0, vector, &decimal_rep_length, &decimal_point);
        vector[decimal_rep_length] = '\0';
    }

    /* reserve(decimal_rep_length + sign ? 1 : 0);
    if (sign)
        operator +=('-');
    for (unsigned i = 0; i < decimal_rep_length; i ++) {
        operator +=(decimal_rep[i]);
    }*/

    const int bufferLength = 128;
    char buffer[bufferLength];
    double_conversion::StringBuilder builder(buffer, bufferLength);

    int exponent = decimal_point - 1;
    const int decimal_in_shortest_low_ = -6;
    const int decimal_in_shortest_high_ = 21;
    if ((decimal_in_shortest_low_ <= exponent)
        && (exponent < decimal_in_shortest_high_)) {
        CreateDecimalRepresentation(flags, decimal_rep, decimal_rep_length,
                                    decimal_point,
                                    double_conversion::Max(0, decimal_rep_length - decimal_point),
                                    &builder);
    } else {
        CreateExponentialRepresentation(flags, decimal_rep, decimal_rep_length, exponent,
                                        &builder);
    }
    ASCIIStringDataNonGCStd str;
    if (sign)
        str += '-';
    char* buf = builder.Finalize();
    while (*buf) {
        str += *buf;
        buf++;
    }
    return ASCIIStringData(str.data(), str.length());
}

String* String::fromASCII(const char* src)
{
    return new ASCIIString(src, strlen(src));
}

String* String::fromDouble(double v)
{
    return new ASCIIString(std::move(dtoa(v)));
}

String* String::fromUTF8(const char* src, size_t len)
{
    if (isAllASCII(src, len)) {
        return new ASCIIString(src, len);
    } else {
        return new UTF16String(std::move(utf8StringToUTF16String(src, len)));
    }
}

int String::stringCompare(size_t l1, size_t l2, const String* c1, const String* c2)
{
    size_t s = 0;
    const unsigned lmin = l1 < l2 ? l1 : l2;
    unsigned pos = 0;
    while (pos < lmin && c1->charAt(s) == c2->charAt(s)) {
        ++s;
        ++pos;
    }

    if (pos < lmin)
        return (c1->charAt(s) > c2->charAt(s)) ? 1 : -1;

    if (l1 == l2)
        return 0;

    return (l1 > l2) ? 1 : -1;
}

bool String::equals(const String* src) const
{
    auto myData = bufferAccessData();
    auto srcData = src->bufferAccessData();
    if (srcData.length != myData.length) {
        return false;
    }

    bool myIs8Bit = myData.has8BitContent;
    bool srcIs8Bit = srcData.has8BitContent;

    if (LIKELY(myIs8Bit && srcIs8Bit)) {
        return stringEqual((const LChar*)myData.buffer, (const LChar*)srcData.buffer, myData.length);
    } else if (myIs8Bit && !srcIs8Bit) {
        return stringEqual((const char16_t*)srcData.buffer, (const LChar*)myData.buffer, myData.length);
    } else if (!myIs8Bit && srcIs8Bit) {
        return stringEqual((const char16_t*)myData.buffer, (const LChar*)srcData.buffer, myData.length);
    } else {
        return stringEqual((const char16_t*)myData.buffer, (const char16_t*)srcData.buffer, myData.length);
    }
}

uint64_t String::tryToUseAsArrayIndex() const
{
    uint32_t number = 0;
    auto data = bufferAccessData();
    const size_t& len = data.length;

    if (UNLIKELY(len == 0)) {
        return Value::InvalidArrayIndexValue;
    }

    char16_t first;
    if (LIKELY(data.has8BitContent)) {
        first = ((LChar*)data.buffer)[0];
    } else {
        first = ((char16_t*)data.buffer)[0];
    }

    if (len > 1) {
        if (first == '0') {
            return Value::InvalidArrayIndexValue;
        }
    }

    for (unsigned i = 0; i < len; i++) {
        char16_t c;
        if (LIKELY(data.has8BitContent)) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c < '0' || c > '9') {
            return Value::InvalidArrayIndexValue;
        } else {
            uint32_t cnum = c - '0';
            if (number > (Value::InvalidArrayIndexValue - cnum) / 10)
                return Value::InvalidArrayIndexValue;
            number = number * 10 + cnum;
        }
    }
    return number;
}

uint64_t String::tryToUseAsIndex() const
{
    uint32_t number = 0;
    auto data = bufferAccessData();
    const size_t& len = data.length;

    if (UNLIKELY(len == 0)) {
        return Value::InvalidIndexValue;
    }

    char16_t first;
    if (LIKELY(data.has8BitContent)) {
        first = ((LChar*)data.buffer)[0];
    } else {
        first = ((char16_t*)data.buffer)[0];
    }

    if (len > 1) {
        if (first == '0') {
            return Value::InvalidIndexValue;
        }
    }

    for (unsigned i = 0; i < len; i++) {
        char16_t c;
        if (LIKELY(data.has8BitContent)) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c < '0' || c > '9') {
            return Value::InvalidIndexValue;
        } else {
            uint32_t cnum = c - '0';
            if (number > (Value::InvalidIndexValue - cnum) / 10)
                return Value::InvalidIndexValue;
            number = number * 10 + cnum;
        }
    }
    return number;
}

size_t String::find(String* str, size_t pos)
{
    const size_t srcStrLen = str->length();
    const size_t size = length();

    if (srcStrLen == 0)
        return pos <= size ? pos : SIZE_MAX;

    if (srcStrLen <= size) {
        char32_t src0 = str->charAt(0);
        auto data = bufferAccessData();
        if (data.has8BitContent) {
            for (; pos <= size - srcStrLen; ++pos) {
                if (((const LChar*)data.buffer)[pos] == src0) {
                    bool same = true;
                    for (size_t k = 1; k < srcStrLen; k++) {
                        if (((const LChar*)data.buffer)[pos + k] != str->charAt(k)) {
                            same = false;
                            break;
                        }
                    }
                    if (same)
                        return pos;
                }
            }
        } else {
            for (; pos <= size - srcStrLen; ++pos) {
                if (((const char16_t*)data.buffer)[pos] == src0) {
                    bool same = true;
                    for (size_t k = 1; k < srcStrLen; k++) {
                        if (((const char16_t*)data.buffer)[pos + k] != str->charAt(k)) {
                            same = false;
                            break;
                        }
                    }
                    if (same)
                        return pos;
                }
            }
        }
    }
    return SIZE_MAX;
}

size_t String::rfind(String* str, size_t pos)
{
    const size_t srcStrLen = str->length();
    const size_t size = length();
    if (srcStrLen == 0)
        return pos <= size ? pos : -1;
    if (srcStrLen <= size) {
        do {
            bool same = true;
            if (pos >= size) {
                continue;
            }
            for (size_t k = 0; k < srcStrLen; k++) {
                if (charAt(pos + k) != str->charAt(k)) {
                    same = false;
                    break;
                }
            }
            if (same)
                return pos;
        } while (pos-- > 0);
    }
    return SIZE_MAX;
}

String* String::subString(size_t from, size_t to)
{
    if (to - from > STRING_SUB_STRING_MIN_VIEW_LENGTH) {
        StringView* str = new StringView(this, from, to);
        return str;
    }
    StringBuilder builder;
    builder.appendSubString(this, from, to);
    return builder.finalize();
}


void* ASCIIString::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ASCIIString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASCIIString, m_stringData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ASCIIString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* Latin1String::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(Latin1String)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(Latin1String, m_stringData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ASCIIString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* UTF16String::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(UTF16String)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(UTF16String, m_stringData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(UTF16String));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
