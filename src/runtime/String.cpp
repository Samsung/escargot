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

#include "Escargot.h"
#include "String.h"
#include "CompressibleString.h"
#include "Value.h"
#include "ThreadLocal.h"

#include "parser/Lexer.h"

#include "fast-dtoa.h"
#include "bignum-dtoa.h"

namespace Escargot {
std::vector<std::string> split(const std::string& s, char seperator)
{
    std::vector<std::string> output;
    std::string::size_type prevPos = 0, pos = 0;

    while ((pos = s.find(seperator, pos)) != std::string::npos) {
        std::string substring(s.substr(prevPos, pos - prevPos));
        output.push_back(substring);
        prevPos = ++pos;
    }

    output.push_back(s.substr(prevPos, pos - prevPos));
    return output;
}

std::vector<std::string> split(const std::string& str, const std::string& seperator)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(seperator, prev);
        if (pos == std::string::npos) {
            pos = str.length();
        }
        std::string token = str.substr(prev, pos - prev);
        if (!token.empty()) {
            tokens.push_back(token);
        }
        prev = pos + seperator.length();
    } while (pos < str.length() && prev < str.length());

    return tokens;
}

bool isASCIIAlpha(char ch)
{
    return isalpha(ch);
}

bool isASCIIDigit(char ch)
{
    return isdigit(ch);
}

bool isASCIIAlphanumeric(char ch)
{
    return isASCIIAlpha(ch) || isASCIIDigit(ch);
}

bool isAllSpecialCharacters(const std::string& s, bool (*fn)(char))
{
    bool isAllSpecial = true;
    for (size_t i = 0; i < s.size(); i++) {
        if (!fn(s[i])) {
            isAllSpecial = false;
            break;
        }
    }
    return isAllSpecial;
}

bool isAllASCII(const char* buf, const size_t len)
{
    for (unsigned i = 0; i < len; i++) {
        if ((buf[i] & 0x80) != 0) {
            return false;
        }
    }
    return true;
}

bool isAllASCII(const char16_t* buf, const size_t len)
{
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] >= 128) {
            return false;
        }
    }
    return true;
}

bool isAllLatin1(const char16_t* buf, const size_t len)
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
    if (len == 1) {
        char16_t c = str->charAt(0);
        if (c < '0' || c > '9') {
            return false;
        }
    } else {
        char16_t c = str->charAt(0);
        if (c < '1' || c > '9') {
            return false;
        }
        for (unsigned i = 1; i < len; i++) {
            char16_t c = str->charAt(i);
            if (c < '0' || c > '9') {
                return false;
            }
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
                    sequence++;
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
        ch <<= 6; // Fall through.
    case 3:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // Fall through.
    case 2:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // Fall through.
    case 1:
        ch += static_cast<unsigned char>(*sequence++);
    }
    return ch - offsetsFromUTF8[length - 1];
}

UTF16StringDataNonGCStd utf8StringToUTF16StringNonGC(const char* buf, const size_t len)
{
    UTF16StringDataNonGCStd str;
    const char* source = buf;
    int charlen;
    bool valid;
    while (source < buf + len) {
        char32_t ch = readUTF8Sequence(source, valid, charlen);
        if (!valid) { // Invalid sequence
            str += 0xFFFD;
        } else if ((uint32_t)(ch) <= 0xffff) { // BMP
            if (((ch)&0xfffff800) == 0xd800) { // SURROGATE
                str += 0xFFFD;
                source -= (charlen - 1);
            } else {
                str += ch; // normal case
            }
        } else if ((uint32_t)((ch)-0x10000) <= 0xfffff) { // SUPPLEMENTARY
            str += (char16_t)(((ch) >> 10) + 0xd7c0); // LEAD
            str += (char16_t)(((ch)&0x3ff) | 0xdc00); // TRAIL
        } else {
            str += 0xFFFD;
            source -= (charlen - 1);
        }
    }

    return str;
}

UTF16StringData utf8StringToUTF16String(const char* buf, const size_t len)
{
    auto str = utf8StringToUTF16StringNonGC(buf, len);
    return UTF16StringData(str.data(), str.length());
}

ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t len)
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

size_t utf32ToUtf16(char32_t i, char16_t* u)
{
    if (i <= 0xffff) {
        if (i >= 0xd800 && i <= 0xdfff) {
            // illegal conversion
            *u = 0xFFFD;
        } else {
            // normal case
            *u = (char16_t)(i & 0xffff);
        }
        return 1;
    } else if (i <= 0x10ffff) {
        // surrogate pair can encode to 0x10ffff
        // https://en.wikipedia.org/wiki/UTF-16
        i -= 0x10000;
        *u++ = 0xd800 | (i >> 10);
        *u = 0xdc00 | (i & 0x3ff);
        return 2;
    } else {
        // produce error char
        // U+FFFD
        *u = 0xFFFD;
        return 1;
    }
}

bool isWellFormed(const char16_t*& utf16, const char16_t* bufferEnd)
{
    if (utf16[0] >= 0xd800 && utf16[0] <= 0xdbff) {
        if (utf16 + 1 < bufferEnd) {
            if (utf16[1] >= 0xdc00 && utf16[1] <= 0xdfff) {
                utf16 += 2;
                return true;
            } else {
                utf16 += 1;
                return false;
            }
        } else {
            utf16 += 1;
            return false;
        }
    } else if (utf16[0] >= 0xdc00 && utf16[0] <= 0xdfff) {
        utf16 += 1;
        return false;
    } else {
        utf16 += 1;
        return true;
    }
}

bool StringBufferAccessData::equals16Bit(const char16_t* c1, const char* c2, size_t len)
{
    while (len > 0) {
        if (*c1++ != *c2++) {
            return false;
        }
        len--;
    }

    return true;
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
    return UTF8StringData((const char*)ASCIIString::characters8(), ASCIIString::length());
}

UTF8StringDataNonGCStd ASCIIString::toNonGCUTF8StringData(int options) const
{
    return UTF8StringDataNonGCStd((const char*)ASCIIString::characters8(), ASCIIString::length());
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
        uint8_t ch = m_bufferData.uncheckedCharAtFor8Bit(i); /* assume that code points above 0xff are impossible since latin-1 is 8-bit */
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

UTF8StringDataNonGCStd Latin1String::toNonGCUTF8StringData(int options) const
{
    UTF8StringDataNonGCStd ret;
    size_t len = length();
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = m_bufferData.uncheckedCharAtFor8Bit(i); /* assume that code points above 0xff are impossible since latin-1 is 8-bit */
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
    return UTF16StringData(UTF16String::characters16(), UTF16String::length());
}

UTF8StringData UTF16String::toUTF8StringData() const
{
    return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
}

UTF8StringDataNonGCStd UTF16String::toNonGCUTF8StringData(int options) const
{
    return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>(options);
}

enum Flags : unsigned {
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

ASCIIStringDataNonGCStd dtoa(double number)
{
    if (number == 0) {
        return "0";
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
    return str;
}

#define LATIN1_LARGE_INLINE_BUFFER(F) \
    F(1)                              \
    F(2)                              \
    F(3)                              \
    F(4)                              \
    F(5)                              \
    F(6)                              \
    F(7)                              \
    F(8)                              \
    F(9)                              \
    F(10)                             \
    F(11)                             \
    F(12)                             \
    F(13)                             \
    F(14)                             \
    F(15)                             \
    F(16)                             \
    F(17)                             \
    F(18)                             \
    F(19)                             \
    F(20)                             \
    F(21)                             \
    F(22)                             \
    F(23)                             \
    F(24)

#define LATIN1_LARGE_INLINE_BUFFER_DEFINE(N) \
    template class Latin1StringWithLargeInlineBuffer<N>;

LATIN1_LARGE_INLINE_BUFFER(LATIN1_LARGE_INLINE_BUFFER_DEFINE)

String* String::fromASCII(const char* src, size_t len)
{
    if (len <= String::StringBufferData::bufferPointerAsArraySize) {
        return new ASCIIStringWithInlineBuffer(src, len);
    } else {
        switch (len) {
#define LATIN1_LARGE_INLINE_BUFFER_SWITCH_ASCII(N) \
    case N:                                        \
        return new Latin1StringWithLargeInlineBuffer<N>(reinterpret_cast<const LChar*>(src), len);
            LATIN1_LARGE_INLINE_BUFFER(LATIN1_LARGE_INLINE_BUFFER_SWITCH_ASCII)
        default:
            return new ASCIIString(src, len);
        }
    }
}

String* String::fromLatin1(const LChar* src, size_t len)
{
    if (len <= String::StringBufferData::bufferPointerAsArraySize) {
        return new Latin1StringWithInlineBuffer(src, len);
    } else {
        switch (len) {
#define LATIN1_LARGE_INLINE_BUFFER_SWITCH(N) \
    case N:                                  \
        return new Latin1StringWithLargeInlineBuffer<N>(src, len);
            LATIN1_LARGE_INLINE_BUFFER(LATIN1_LARGE_INLINE_BUFFER_SWITCH)
        default:
            return new Latin1String(src, len);
        }
    }
}

String* String::fromLatin1(const char16_t* src, size_t len)
{
    if (len <= LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE) {
        LChar* dest = static_cast<LChar*>(alloca(len));
        for (size_t i = 0; i < len; i++) {
            ASSERT(src[i] < 256);
            dest[i] = src[i];
        }
        return String::fromLatin1(dest, len);
    } else {
        return new Latin1String(src, len);
    }
}

String* String::fromDouble(double v)
{
    auto s = dtoa(v);
    return String::fromASCII(s.data(), s.length());
}

String* String::fromUTF8(const char* src, size_t len, bool maybeASCII)
{
    if (maybeASCII && isAllASCII(src, len)) {
        return String::fromASCII(src, len);
    } else {
        auto s = utf8StringToUTF16String(src, len);
        return new UTF16String(std::move(s));
    }
}

#if defined(ENABLE_COMPRESSIBLE_STRING)
String* String::fromUTF8ToCompressibleString(VMInstance* instance, const char* src, size_t len, bool maybeASCII)
{
    if (maybeASCII && isAllASCII(src, len)) {
        return new CompressibleString(instance, src, len);
    } else {
        auto s = utf8StringToUTF16StringNonGC(src, len);
        return new CompressibleString(instance, s.data(), s.length());
    }
}
#endif

String* String::fromCharCode(char32_t code)
{
    if (code < 128) {
        char c = (char)code;
        return new ASCIIStringWithInlineBuffer(&c, 1);
    } else if (code < 0x10000) {
        char16_t buf = code;
        return new UTF16StringWithInlineBuffer(&buf, 1);
    } else {
        char16_t buf[2];
        buf[0] = (char16_t)(0xD800 + ((code - 0x10000) >> 10));
        buf[1] = (char16_t)(0xDC00 + ((code - 0x10000) & 1023));
        return new UTF16StringWithInlineBuffer(buf, 2);
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
    if (length() != src->length()) {
        return false;
    }

    const auto& myData = bufferAccessData();
    const auto& srcData = src->bufferAccessData();

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

static uint64_t convertToIndexNumber(const String* str, uint64_t maxLimit)
{
    uint64_t number = 0;
    const size_t& len = str->length();

    if (UNLIKELY(len == 0)) {
        return maxLimit;
    }

    const auto& data = str->bufferAccessData();

    char16_t first;
    if (LIKELY(data.has8BitContent)) {
        first = ((LChar*)data.buffer)[0];
    } else {
        first = ((char16_t*)data.buffer)[0];
    }

    if (len > 1 && first == '0') {
        return maxLimit;
    }

    for (unsigned i = 0; i < len; i++) {
        char16_t c;
        if (LIKELY(data.has8BitContent)) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c < '0' || c > '9') {
            return maxLimit;
        } else {
            uint64_t cnum = c - '0';
            if (UNLIKELY(number > (maxLimit - cnum) / 10)) {
                return maxLimit;
            }
            number = number * 10 + cnum;
        }
    }
    return number;
}

uint64_t String::tryToUseAsIndex() const
{
    return convertToIndexNumber(this, Value::InvalidIndexValue);
}

uint32_t String::tryToUseAsIndex32() const
{
    return convertToIndexNumber(this, static_cast<uint64_t>(Value::InvalidIndex32Value));
}

uint32_t String::tryToUseAsIndexProperty() const
{
    return tryToUseAsIndex32();
}

bool String::isWellFormed() const
{
    auto bad = bufferAccessData();
    if (bad.has8BitContent) {
        return true;
    }
    auto utf16 = bad.bufferAs16Bit;
    auto end = bad.bufferAs16Bit + bad.length;
    while (utf16 < end) {
        if (!::Escargot::isWellFormed(utf16, end)) {
            return false;
        }
    }
    return true;
}

String* String::toWellFormed()
{
    if (isWellFormed()) {
        return this;
    }
    auto bad = bufferAccessData();
    auto utf16 = bad.bufferAs16Bit;
    auto end = bad.bufferAs16Bit + bad.length;
    UTF16StringData result;
    while (utf16 < end) {
        auto start = utf16;
        if (::Escargot::isWellFormed(utf16, end)) {
            while (start < utf16) {
                result.pushBack(*start);
                start++;
            }
        } else {
            result.pushBack(0xfffd);
        }
    }
    return new UTF16String(std::move(result));
}

size_t String::find(String* str, size_t pos) const
{
    const size_t srcStrLen = str->length();
    const size_t size = length();

    if (srcStrLen == 0)
        return pos <= size ? pos : SIZE_MAX;

    if (srcStrLen <= size) {
        char32_t src0 = str->charAt(0);
        const auto& data = bufferAccessData();
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

size_t String::find(const char* str, size_t srcStrLen, size_t pos) const
{
    const size_t size = length();

    if (srcStrLen == 0)
        return pos <= size ? pos : SIZE_MAX;

    if (srcStrLen <= size) {
        char32_t src0 = str[0];
        const auto& data = bufferAccessData();
        if (data.has8BitContent) {
            for (; pos <= size - srcStrLen; ++pos) {
                if (((const LChar*)data.buffer)[pos] == src0) {
                    bool same = true;
                    for (size_t k = 1; k < srcStrLen; k++) {
                        if (((const LChar*)data.buffer)[pos + k] != str[k]) {
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
                        if (((const char16_t*)data.buffer)[pos + k] != str[k]) {
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

String* String::substring(size_t from, size_t to)
{
    if (UNLIKELY(from >= to)) {
        return String::emptyString();
    }
#if !defined(ESCARGOT_SMALL_CONFIG)
    if (to - from > STRING_SUB_STRING_MIN_VIEW_LENGTH) {
        StringView* str = new StringView(this, from, to);
        return str;
    }
#endif
    StringBuilder builder;
    builder.appendSubString(this, from, to);
    return builder.finalize();
}

// https://tc39.es/ecma262/multipage/text-processing.html#sec-advancestringindex
uint64_t String::advanceStringIndex(uint64_t index, bool unicode)
{
    // Assert: index ≤ 2**53 - 1.
    ASSERT(index <= (1ULL << 53) - 1);

    // If unicode is false, return index + 1.
    if (!unicode) {
        return index + 1;
    }

    // Let length be the length of S.
    size_t length = this->length();

    // If index + 1 ≥ length, return index + 1.
    if (index + 1 >= length) {
        return index + 1;
    }

    // Let cp be CodePointAt(S, index).
    auto cp = codePointAt(index);
    // Return index + cp.[[CodeUnitCount]].
    return index + cp.codeUnitCount;
}

// https://tc39.es/ecma262/multipage/ecmascript-language-source-code.html#sec-codepointat
String::CodePointAtResult String::codePointAt(size_t position)
{
    auto bad = bufferAccessData();
    // Let size be the length of string.
    const auto& size = bad.length;
    // Assert: position ≥ 0 and position < size.
    // Let first be the code unit at index position within string.
    char16_t first = bad.charAt(position);
    // Let cp be the code point whose numeric value is the numeric value of first.
    char32_t cp = first;
    // If first is neither a leading surrogate nor a trailing surrogate, then
    if (!U16_IS_LEAD(cp) && !U16_IS_TRAIL(cp)) {
        // Return the Record { [[CodePoint]]: cp, [[CodeUnitCount]]: 1, [[IsUnpairedSurrogate]]: false }.
        return { cp, 1, false };
    }
    // If first is a trailing surrogate or position + 1 = size, then
    if (U16_IS_TRAIL(first) || (position + 1) == size) {
        // Return the Record { [[CodePoint]]: cp, [[CodeUnitCount]]: 1, [[IsUnpairedSurrogate]]: true }.
        return { cp, 1, true };
    }
    // Let second be the code unit at index position + 1 within string.
    char16_t second = bad.charAt(position + 1);
    // If second is not a trailing surrogate, then
    if (!U16_IS_TRAIL(second)) {
        // Return the Record { [[CodePoint]]: cp, [[CodeUnitCount]]: 1, [[IsUnpairedSurrogate]]: true }.
        return { cp, 1, true };
    }
    // Set cp to UTF16SurrogatePairToCodePoint(first, second).
    cp = U16_GET_SUPPLEMENTARY(first, second);
    // Return the Record { [[CodePoint]]: cp, [[CodeUnitCount]]: 2, [[IsUnpairedSurrogate]]: false }.
    return { cp, 2, false };
}

bool String::isAllSpecialCharacters(bool (*fn)(char))
{
    auto bad = bufferAccessData();
    for (size_t i = 0; i < bad.length; i++) {
        char32_t c = bad.charAt(i);
        if (c > 127) {
            return false;
        }
        if (!fn((char)c)) {
            return false;
        }
    }

    return true;
}

String* String::trim(String::StringTrimWhere where)
{
    int64_t stringLength = (int64_t)length();
    int64_t s = 0;
    int64_t e = stringLength - 1;

    if (where == TrimStart || where == TrimBoth) {
        for (s = 0; s < stringLength; s++) {
            if (!EscargotLexer::isWhiteSpaceOrLineTerminator(charAt(s)))
                break;
        }
    }

    if (where == TrimEnd || where == TrimBoth) {
        for (e = stringLength - 1; e >= s; e--) {
            if (!EscargotLexer::isWhiteSpaceOrLineTerminator(charAt(e)))
                break;
        }
    }

    if (s == 0 && e == stringLength - 1) {
        return this;
    }
    return substring(s, e + 1);
}


void* UTF16String::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(UTF16String)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(UTF16String, m_bufferData.buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(UTF16String));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

String* String::getSubstitution(ExecutionState& state, String* matched, String* str, size_t position, StringVector& captures, Value namedCapture, String* replacement)
{
    ASSERT(matched != nullptr);
    ASSERT(str != nullptr);
    ASSERT(replacement != nullptr);
    ASSERT(matched->isString() == true);
    size_t matchLenght = matched->length();
    size_t stringLength = str->length();
    ASSERT(position <= stringLength);
    size_t tailPos = position + matchLenght;
    size_t m = captures.size();
    StringBuilder builder;
    bool twodigit = false;
    Object* namedCaptureObj = nullptr;
    if (!namedCapture.isUndefined()) {
        namedCaptureObj = namedCapture.toObject(state);
    }

    // dollar replace
    for (size_t i = 0; i < replacement->length(); i++) {
        if (replacement->charAt(i) == '$' && (i + 1) < replacement->length()) {
            char16_t temp = replacement->charAt(i + 1);
            if (temp == '$') {
                builder.appendChar('$', &state);
            } else if (temp == '&') {
                builder.appendSubString(matched, 0, matchLenght, &state);
            } else if (temp == '`') {
                builder.appendSubString(str, 0, position, &state);
            } else if (temp == '\'') {
                if (tailPos >= stringLength) {
                    tailPos = stringLength;
                }
                builder.appendSubString(str, tailPos, stringLength, &state);
            } else if (temp >= '0' && temp <= '9') {
                size_t n = temp - '0' - 1;
                if (i + 2 < replacement->length() && replacement->charAt(i + 2) >= '0' && replacement->charAt(i + 2) <= '9') {
                    char16_t temp1 = replacement->charAt(i + 2);
                    n = n + 1;
                    n *= 10;
                    n += temp1 - '0' - 1;
                    twodigit = true;
                    if (n >= m) {
                        twodigit = false;
                        n = temp - '0' - 1;
                    }
                }
                if (n < m) {
                    String* capturesN = captures[n];
                    builder.appendString(capturesN, &state);
                    if (twodigit) {
                        i++;
                        twodigit = false;
                    }
                } else {
                    builder.appendChar('$', &state);
                    builder.appendChar(temp, &state);
                }
            } else if (temp == '<' && !namedCapture.isUndefined()) {
                size_t namedCaptureEnd = i + 1;
                bool ValidNamedCapturedGroup = false;
                while (namedCaptureEnd < replacement->length()) {
                    if (replacement->charAt(namedCaptureEnd) == '>') {
                        ValidNamedCapturedGroup = true;
                        break;
                    }
                    namedCaptureEnd++;
                }
                if (ValidNamedCapturedGroup && namedCaptureObj) {
                    String* groupName = replacement->substring((i + 2), (namedCaptureEnd));
                    Value capture = namedCaptureObj->get(state, ObjectPropertyName(state, groupName)).value(state, Value(0));
                    if (!capture.isUndefined()) {
                        builder.appendString(capture.toString(state), &state);
                    }
                    i = namedCaptureEnd - 1;
                } else {
                    builder.appendChar('$', &state);
                    builder.appendChar(temp, &state);
                }

            } else {
                builder.appendChar('$', &state);
                builder.appendChar(temp, &state);
            }
            i++;
        } else {
            builder.appendChar(replacement->charAt(i), &state);
        }
    }
    return builder.finalize(&state);
}

bool isupper(char32_t ch)
{
    return (ch >= 'A' && ch <= 'Z');
}

bool islower(char32_t ch)
{
    return (ch >= 'a' && ch <= 'z');
}

char32_t tolower(char32_t c)
{
    return isupper(c) ? (c) - 'A' + 'a' : c;
}

char32_t toupper(char32_t c)
{
    return islower(c) ? c - 'a' + 'A' : c;
}

bool isspace(char32_t c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ? 1 : 0);
}

bool isdigit(char32_t ch)
{
    return (ch >= '0' && ch <= '9');
}
} // namespace Escargot
