#include "Escargot.h"
#include "String.h"

#include "fast-dtoa.h"
#include "bignum-dtoa.h"

namespace Escargot {

String* String::emptyString = new(malloc(sizeof (String))) ASCIIString("");

bool isAllASCII(const char* buf, const size_t& len)
{
    for (unsigned i = 0; i < len ; i ++) {
        if ((buf[i] & 0x80) != 0) {
            return false;
        }
    }
    return true;
}

bool isAllASCII(const char16_t* buf, const size_t& len)
{
    for (unsigned i = 0; i < len ; i ++) {
        if (buf[i] >= 128) {
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
    UTF16StringData str;
    const char* source = buf;
    int charlen;
    bool valid;
    while (source < buf + len) {
        char32_t ch = readUTF8Sequence(source, valid, charlen);
        if (!valid) { // Invalid sequence
            str += 0xFFFD;
        } else if (((uint32_t)(ch) <= 0xffff)) { // BMP
            if ((((ch) & 0xfffff800) == 0xd800)) { // SURROGATE
                str += 0xFFFD;
                source -= (charlen - 1);
            } else {
                str += ch; // normal case
            }
        } else if (((uint32_t)((ch) - 0x10000) <= 0xfffff)) { // SUPPLEMENTARY
            str += (char16_t)(((ch) >> 10) + 0xd7c0); // LEAD
            str += (char16_t)(((ch) & 0x3ff) | 0xdc00); // TRAIL
        } else {
            str += 0xFFFD;
            source -= (charlen - 1);
        }
    }

    return UTF16StringData(std::move(str));
}

ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len)
{
    ASCIIStringData str;
    str.reserve(len);
    for (unsigned i = 0 ; i < len ; i ++) {
        ASSERT(buf[i] < 128);
        str += buf[i];
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
            UTF8[0] = (char)(0xf0 + uc / (0x01 <<18));
            UTF8[1] = (char)(0x80 + uc / (0x01 <<12) % (0x01 <<12));
            UTF8[2] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[3] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[4] = (char) '\0';
        }
        tRequiredSize = 4;
    } else if (uc <= 0x3ffffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xf8 + uc / (0x01 <<24));
            UTF8[1] = (char)(0x80 + uc / (0x01 <<18) % (0x01 <<18));
            UTF8[2] = (char)(0x80 + uc / (0x01 <<12) % (0x01 <<12));
            UTF8[3] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[4] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[5] = (char) '\0';
        }
        tRequiredSize = 5;
    } else if (uc <= 0x7fffffff) {
        if (NULL != UTF8) {
            UTF8[0] = (char)(0xfc + uc / (0x01 <<30));
            UTF8[1] = (char)(0x80 + uc / (0x01 <<24) % (0x01 <<24));
            UTF8[2] = (char)(0x80 + uc / (0x01 <<18) % (0x01 <<18));
            UTF8[3] = (char)(0x80 + uc / (0x01 <<12) % (0x01 <<12));
            UTF8[4] = (char)(0x80 + uc / (0x01 << 6) % (0x01 << 6));
            UTF8[5] = (char)(0x80 + uc % (0x01 << 6));
            UTF8[6] = (char) '\0';
        }
        tRequiredSize = 6;
    } else {
        return utf32ToUtf8(0xFFFD, UTF8);
    }

    return tRequiredSize;
}


UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len)
{
    UTF8StringData str;
    str.reserve(len);
    for (unsigned i = 0 ; i < len ;) {
        if (buf[i] < 128) {
            str += buf[i];
            i ++;
        } else {
            char32_t c;
            U16_NEXT(buf, i, len, c);

            char buf[8];
            utf32ToUtf8(c, buf);
            str += buf;
        }
    }
    return UTF8StringData(std::move(str));
}


UTF16StringData ASCIIString::toUTF16StringData() const
{
    UTF16StringData ret;
    size_t len = length();
    ret.reserve(len);
    ret.assign(ASCIIStringData::begin(), ASCIIStringData::end());
    return ret;
}

UTF8StringData ASCIIString::toUTF8StringData() const
{
    UTF8StringData ret;
    size_t len = length();
    ret.reserve(len);
    ret.assign(ASCIIStringData::begin(), ASCIIStringData::end());
    return ret;
}

UTF16StringData UTF16String::toUTF16StringData() const
{
    return *const_cast<UTF16String *>(this)->asUTF16String();
}

UTF8StringData UTF16String::toUTF8StringData() const
{
    return utf16StringToUTF8String(UTF16String::data(), length());
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
    double_conversion::StringBuilder* result_builder) {
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
    double_conversion::StringBuilder* result_builder) {
    ASSERT(length != 0);
    result_builder->AddCharacter(decimal_digits[0]);
    if (length != 1) {
        result_builder->AddCharacter('.');
        result_builder->AddSubstring(&decimal_digits[1], length-1);
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
    ASCIIStringData str;
    if (number == 0) {
        str.append({'0'});
        return std::move(str);
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
    if (sign)
        str += '-';
    char* buf = builder.Finalize();
    while (*buf) {
        str += *buf;
        buf++;
    }
    return std::move(str);
}


}
