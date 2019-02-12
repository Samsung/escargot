/*
 *  Copyright (C) 2016 Samsung Electronics Co., Ltd. All Rights Reserved
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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
 *
 */

#include "Escargot.h"
#include "NumberObject.h"
#include "Context.h"

namespace Escargot {

NumberObject::NumberObject(ExecutionState& state, double value)
    : Object(state)
    , m_primitiveValue(value)
{
    setPrototype(state, state.context()->globalObject()->numberPrototype());
}

void* NumberObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(NumberObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(NumberObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(NumberObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(NumberObject, m_values));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(NumberObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

///////////////////////////////////////////////////////////////////////////////
//// CODE FROM JAVASCRIPTCORE /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// decompose 'number' to its sign, exponent, and mantissa components.
// The result is interpreted as:
//     (sign ? -1 : 1) * pow(2, exponent) * (mantissa / (1 << 52))
inline void decomposeDouble(double number, bool& sign, int32_t& exponent, uint64_t& mantissa)
{
    ASSERT(std::isfinite(number));

    sign = std::signbit(number);

    uint64_t bits = bitwise_cast<uint64_t>(number);
    exponent = (static_cast<int32_t>(bits >> 52) & 0x7ff) - 0x3ff;
    mantissa = bits & 0xFFFFFFFFFFFFFull;

    // Check for zero/denormal values; if so, adjust the exponent,
    // if not insert the implicit, omitted leading 1 bit.
    if (exponent == -0x3ff)
        exponent = mantissa ? -0x3fe : 0;
    else
        mantissa |= 0x10000000000000ull;
}

// This is used in converting the integer part of a number to a string.
class BigInteger {
public:
    explicit BigInteger(double number)
    {
        m_values.reserve(36);
        ASSERT(std::isfinite(number) && !std::signbit(number));
        ASSERT(number == floor(number));

        bool sign;
        int32_t exponent;
        uint64_t mantissa;
        decomposeDouble(number, sign, exponent, mantissa);
        ASSERT(!sign && exponent >= 0);

        int32_t zeroBits = exponent - 52;

        if (zeroBits < 0) {
            mantissa >>= -zeroBits;
            zeroBits = 0;
        }

        while (zeroBits >= 32) {
            m_values.push_back(0);
            zeroBits -= 32;
        }

        // Left align the 53 bits of the mantissa within 96 bits.
        uint32_t values[3];
        values[0] = static_cast<uint32_t>(mantissa);
        values[1] = static_cast<uint32_t>(mantissa >> 32);
        values[2] = 0;
        // Shift based on the remainder of the exponent.
        if (zeroBits) {
            values[2] = values[1] >> (32 - zeroBits);
            values[1] = (values[1] << zeroBits) | (values[0] >> (32 - zeroBits));
            values[0] = (values[0] << zeroBits);
        }
        m_values.push_back(values[0]);
        m_values.push_back(values[1]);
        m_values.push_back(values[2]);

        // Canonicalize; remove all trailing zeros.
        while (m_values.size() && !m_values.back())
            m_values.pop_back();
    }

    uint32_t divide(uint32_t divisor)
    {
        uint32_t carry = 0;

        for (size_t i = m_values.size(); i;) {
            --i;
            uint64_t dividend = (static_cast<uint64_t>(carry) << 32) + static_cast<uint64_t>(m_values[i]);

            uint64_t result = dividend / static_cast<uint64_t>(divisor);
            ASSERT(result == static_cast<uint32_t>(result));
            uint64_t remainder = dividend % static_cast<uint64_t>(divisor);
            ASSERT(remainder == static_cast<uint32_t>(remainder));

            m_values[i] = static_cast<uint32_t>(result);
            carry = static_cast<uint32_t>(remainder);
        }

        // Canonicalize; remove all trailing zeros.
        while (m_values.size() && !m_values.back())
            m_values.pop_back();

        return carry;
    }

    bool operator!() { return !m_values.size(); }
private:
    std::vector<uint32_t> m_values;
};

// Would be nice if this was a static const member, but the OS X linker
// seems to want a symbol in the binary in that case...
#define oneGreaterThanMaxUInt16 0x10000

// A uint16_t with an infinite precision fraction. Upon overflowing
// the uint16_t range, this class will clamp to oneGreaterThanMaxUInt16.
// This is used in converting the fraction part of a number to a string.
class Uint16WithFraction {
public:
    explicit Uint16WithFraction(double number, uint16_t divideByExponent = 0)
    {
        m_values.reserve(36);
        ASSERT(number && std::isfinite(number) && !std::signbit(number));

        // Check for values out of uint16_t range.
        if (number >= oneGreaterThanMaxUInt16) {
            m_values.push_back(oneGreaterThanMaxUInt16);
            m_leadingZeros = 0;
            return;
        }

        // Append the units to m_values.
        double integerPart = floor(number);
        m_values.push_back(static_cast<uint32_t>(integerPart));

        bool sign;
        int32_t exponent;
        uint64_t mantissa;
        decomposeDouble(number - integerPart, sign, exponent, mantissa);
        ASSERT(!sign && exponent < 0);
        exponent -= divideByExponent;

        int32_t zeroBits = -exponent;
        --zeroBits;

        // Append the append words for to m_values.
        while (zeroBits >= 32) {
            m_values.push_back(0);
            zeroBits -= 32;
        }

        // Left align the 53 bits of the mantissa within 96 bits.
        uint32_t values[3];
        values[0] = static_cast<uint32_t>(mantissa >> 21);
        values[1] = static_cast<uint32_t>(mantissa << 11);
        values[2] = 0;
        // Shift based on the remainder of the exponent.
        if (zeroBits) {
            values[2] = values[1] << (32 - zeroBits);
            values[1] = (values[1] >> zeroBits) | (values[0] << (32 - zeroBits));
            values[0] = (values[0] >> zeroBits);
        }
        m_values.push_back(values[0]);
        m_values.push_back(values[1]);
        m_values.push_back(values[2]);

        // Canonicalize; remove any trailing zeros.
        while (m_values.size() > 1 && !m_values.back())
            m_values.pop_back();

        // Count the number of leading zero, this is useful in optimizing multiplies.
        m_leadingZeros = 0;
        while (m_leadingZeros < m_values.size() && !m_values[m_leadingZeros])
            ++m_leadingZeros;
    }

    Uint16WithFraction& operator*=(uint16_t multiplier)
    {
        ASSERT(checkConsistency());

        // iteratate backwards over the fraction until we reach the leading zeros,
        // passing the carry from one calculation into the next.
        uint64_t accumulator = 0;
        for (size_t i = m_values.size(); i > m_leadingZeros;) {
            --i;
            accumulator += static_cast<uint64_t>(m_values[i]) * static_cast<uint64_t>(multiplier);
            m_values[i] = static_cast<uint32_t>(accumulator);
            accumulator >>= 32;
        }

        if (!m_leadingZeros) {
            // With a multiplicand and multiplier in the uint16_t range, this cannot carry
            // (even allowing for the infinity value).
            ASSERT(!accumulator);
            // Check for overflow & clamp to 'infinity'.
            if (m_values[0] >= oneGreaterThanMaxUInt16) {
                ASSERT(m_values.size() <= 1);
                m_values.shrink_to_fit();
                m_values[0] = oneGreaterThanMaxUInt16;
                m_leadingZeros = 0;
                return *this;
            }
        } else if (accumulator) {
            // Check for carry from the last multiply, if so overwrite last leading zero.
            m_values[--m_leadingZeros] = static_cast<uint32_t>(accumulator);
            // The limited range of the multiplier should mean that even if we carry into
            // the units, we don't need to check for overflow of the uint16_t range.
            ASSERT(m_values[0] < oneGreaterThanMaxUInt16);
        }

        // Multiplication by an even value may introduce trailing zeros; if so, clean them
        // up. (Keeping the value in a normalized form makes some of the comparison operations
        // more efficient).
        while (m_values.size() > 1 && !m_values.back())
            m_values.pop_back();
        ASSERT(checkConsistency());
        return *this;
    }

    bool operator<(const Uint16WithFraction& other)
    {
        ASSERT(checkConsistency());
        ASSERT(other.checkConsistency());

        // Iterate over the common lengths of arrays.
        size_t minSize = std::min(m_values.size(), other.m_values.size());
        for (size_t index = 0; index < minSize; ++index) {
            // If we find a value that is not equal, compare and return.
            uint32_t fromThis = m_values[index];
            uint32_t fromOther = other.m_values[index];
            if (fromThis != fromOther)
                return fromThis < fromOther;
        }
        // If these numbers have the same lengths, they are equal,
        // otherwise which ever number has a longer fraction in larger.
        return other.m_values.size() > minSize;
    }

    // Return the floor (non-fractional portion) of the number, clearing this to zero,
    // leaving the fractional part unchanged.
    uint32_t floorAndSubtract()
    {
        // 'floor' is simple the integer portion of the value.
        uint32_t floor = m_values[0];

        // If floor is non-zero,
        if (floor) {
            m_values[0] = 0;
            m_leadingZeros = 1;
            while (m_leadingZeros < m_values.size() && !m_values[m_leadingZeros])
                ++m_leadingZeros;
        }

        return floor;
    }

    // Compare this value to 0.5, returns -1 for less than, 0 for equal, 1 for greater.
    int comparePoint5()
    {
        ASSERT(checkConsistency());
        // If units != 0, this is greater than 0.5.
        if (m_values[0])
            return 1;
        // If size == 1 this value is 0, hence < 0.5.
        if (m_values.size() == 1)
            return -1;
        // Compare to 0.5.
        if (m_values[1] > 0x80000000ul)
            return 1;
        if (m_values[1] < 0x80000000ul)
            return -1;
        // Check for more words - since normalized numbers have no trailing zeros, if
        // there are more that two digits we can assume at least one more is non-zero,
        // and hence the value is > 0.5.
        return m_values.size() > 2 ? 1 : 0;
    }

    // Return true if the sum of this plus addend would be greater than 1.
    bool sumGreaterThanOne(const Uint16WithFraction& addend)
    {
        ASSERT(checkConsistency());
        ASSERT(addend.checkConsistency());

        // First, sum the units. If the result is greater than one, return true.
        // If equal to one, return true if either number has a fractional part.
        uint32_t sum = m_values[0] + addend.m_values[0];
        if (sum)
            return sum > 1 || std::max(m_values.size(), addend.m_values.size()) > 1;

        // We could still produce a result greater than zero if addition of the next
        // word from the fraction were to carry, leaving a result > 0.

        // Iterate over the common lengths of arrays.
        size_t minSize = std::min(m_values.size(), addend.m_values.size());
        for (size_t index = 1; index < minSize; ++index) {
            // Sum the next word from this & the addend.
            uint32_t fromThis = m_values[index];
            uint32_t fromAddend = addend.m_values[index];
            sum = fromThis + fromAddend;

            // Check for overflow. If so, check whether the remaining result is non-zero,
            // or if there are any further words in the fraction.
            if (sum < fromThis)
                return sum || (index + 1) < std::max(m_values.size(), addend.m_values.size());

            // If the sum is uint32_t max, then we would carry a 1 if addition of the next
            // digits in the number were to overflow.
            if (sum != 0xFFFFFFFF)
                return false;
        }
        return false;
    }

private:
    bool checkConsistency() const
    {
        // All values should have at least one value.
        return (m_values.size())
            // The units value must be a uint16_t, or the value is the overflow value.
            && (m_values[0] < oneGreaterThanMaxUInt16 || (m_values[0] == oneGreaterThanMaxUInt16 && m_values.size() == 1))
            // There should be no trailing zeros (unless this value is zero!).
            && (m_values.back() || m_values.size() == 1);
    }

    // The internal storage of the number. This vector is always at least one entry in size,
    // with the first entry holding the portion of the number greater than zero. The first
    // value always hold a value in the uint16_t range, or holds the value oneGreaterThanMaxUInt16 to
    // indicate the value has overflowed to >= 0x10000. If the units value is oneGreaterThanMaxUInt16,
    // there can be no fraction (size must be 1).
    //
    // Subsequent values in the array represent portions of the fractional part of this number.
    // The total value of the number is the sum of (m_values[i] / pow(2^32, i)), for each i
    // in the array. The vector should contain no trailing zeros, except for the value '0',
    // represented by a vector contianing a single zero value. These constraints are checked
    // by 'checkConsistency()', above.
    //
    // The inline capacity of the vector is set to be able to contain any IEEE double (1 for
    // the units column, 32 for zeros introduced due to an exponent up to -3FE, and 2 for
    // bits taken from the mantissa).
    std::vector<uint32_t> m_values;

    // Cache a count of the number of leading zeros in m_values. We can use this to optimize
    // methods that would otherwise need visit all words in the vector, e.g. multiplication.
    size_t m_leadingZeros;
};

// Mapping from integers 0..35 to digit identifying this value, for radix 2..36.
static const char radixDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

char* NumberObject::toStringWithRadix(ExecutionState& state, RadixBuffer& buffer, double number, unsigned radix)
{
    ASSERT(std::isfinite(number));
    ASSERT(radix >= 2 && radix <= 36);

    // Position the decimal point at the center of the string, set
    // the startOfResultString pointer to point at the decimal point.
    char* decimalPoint = buffer + sizeof(buffer) / 2;
    char* startOfResultString = decimalPoint;

    // Extract the sign.
    bool isNegative = number < 0;
    if (std::signbit(number))
        number = -number;
    double integerPart = floor(number);

    // We use this to test for odd values in odd radix bases.
    // Where the base is even, (e.g. 10), to determine whether a value is even we need only
    // consider the least significant digit. For example, 124 in base 10 is even, because '4'
    // is even. if the radix is odd, then the radix raised to an integer power is also odd.
    // E.g. in base 5, 124 represents (1 * 125 + 2 * 25 + 4 * 5). Since each digit in the value
    // is multiplied by an odd number, the result is even if the sum of all digits is even.
    //
    // For the integer portion of the result, we only need test whether the integer value is
    // even or odd. For each digit of the fraction added, we should invert our idea of whether
    // the number is odd if the new digit is odd.
    //
    // Also initialize digit to this value; for even radix values we only need track whether
    // the last individual digit was odd.
    bool integerPartIsOdd = integerPart <= static_cast<double>(0x1FFFFFFFFFFFFFull) && static_cast<int64_t>(integerPart) & 1;
    ASSERT(integerPartIsOdd == static_cast<bool>(fmod(integerPart, 2)));
    bool isOddInOddRadix = integerPartIsOdd;
    uint32_t digit = integerPartIsOdd;

    // Check if the value has a fractional part to convert.
    double fractionPart = number - integerPart;
    if (fractionPart) {
        // Write the decimal point now.
        *decimalPoint = '.';

        // Higher precision representation of the fractional part.
        Uint16WithFraction fraction(fractionPart);

        bool needsRoundingUp = false;
        char* endOfResultString = decimalPoint + 1;

        // Calculate the delta from the current number to the next & previous possible IEEE numbers.
        double nextNumber = nextafter(number, std::numeric_limits<double>::infinity());
        double lastNumber = nextafter(number, -std::numeric_limits<double>::infinity());
        ASSERT(std::isfinite(nextNumber) && !std::signbit(nextNumber));
        ASSERT(std::isfinite(lastNumber) && !std::signbit(lastNumber));
        double deltaNextDouble = nextNumber - number;
        double deltaLastDouble = number - lastNumber;
        ASSERT(std::isfinite(deltaNextDouble) && !std::signbit(deltaNextDouble));
        ASSERT(std::isfinite(deltaLastDouble) && !std::signbit(deltaLastDouble));

        // We track the delta from the current value to the next, to track how many digits of the
        // fraction we need to write. For example, if the value we are converting is precisely
        // 1.2345, so far we have written the digits "1.23" to a string leaving a remainder of
        // 0.45, and we want to determine whether we can round off, or whether we need to keep
        // appending digits ('4'). We can stop adding digits provided that then next possible
        // lower IEEE value is further from 1.23 than the remainder we'd be rounding off (0.45),
        // which is to say, less than 1.2255. Put another way, the delta between the prior
        // possible value and this number must be more than 2x the remainder we'd be rounding off
        // (or more simply half the delta between numbers must be greater than the remainder).
        //
        // Similarly we need track the delta to the next possible value, to dertermine whether
        // to round up. In almost all cases (other than at exponent boundaries) the deltas to
        // prior and subsequent values are identical, so we don't need track then separately.
        if (deltaNextDouble != deltaLastDouble) {
            // Since the deltas are different track them separately. Pre-multiply by 0.5.
            Uint16WithFraction halfDeltaNext(deltaNextDouble, 1);
            Uint16WithFraction halfDeltaLast(deltaLastDouble, 1);

            while (true) {
                // examine the remainder to determine whether we should be considering rounding
                // up or down. If remainder is precisely 0.5 rounding is to even.
                int dComparePoint5 = fraction.comparePoint5();
                if (dComparePoint5 > 0 || (!dComparePoint5 && (radix & 1 ? isOddInOddRadix : digit & 1))) {
                    // Check for rounding up; are we closer to the value we'd round off to than
                    // the next IEEE value would be?
                    if (fraction.sumGreaterThanOne(halfDeltaNext)) {
                        needsRoundingUp = true;
                        break;
                    }
                } else {
                    // Check for rounding down; are we closer to the value we'd round off to than
                    // the prior IEEE value would be?
                    if (fraction < halfDeltaLast)
                        break;
                }

                ASSERT(endOfResultString < (buffer + sizeof(buffer) - 1));
                // Write a digit to the string.
                fraction *= radix;
                digit = fraction.floorAndSubtract();
                *endOfResultString++ = radixDigits[digit];
                // Keep track whether the portion written is currently even, if the radix is odd.
                if (digit & 1)
                    isOddInOddRadix = !isOddInOddRadix;

                // Shift the fractions by radix.
                halfDeltaNext *= radix;
                halfDeltaLast *= radix;
            }
        } else {
            // This code is identical to that above, except since deltaNextDouble != deltaLastDouble
            // we don't need to track these two values separately.
            Uint16WithFraction halfDelta(deltaNextDouble, 1);

            while (true) {
                int dComparePoint5 = fraction.comparePoint5();
                if (dComparePoint5 > 0 || (!dComparePoint5 && (radix & 1 ? isOddInOddRadix : digit & 1))) {
                    if (fraction.sumGreaterThanOne(halfDelta)) {
                        needsRoundingUp = true;
                        break;
                    }
                } else if (fraction < halfDelta)
                    break;

                ASSERT(endOfResultString < (buffer + sizeof(buffer) - 1));
                fraction *= radix;
                digit = fraction.floorAndSubtract();
                if (digit & 1)
                    isOddInOddRadix = !isOddInOddRadix;
                *endOfResultString++ = radixDigits[digit];

                halfDelta *= radix;
            }
        }

        // Check if the fraction needs rounding off (flag set in the loop writing digits, above).
        if (needsRoundingUp) {
            // Whilst the last digit is the maximum in the current radix, remove it.
            // e.g. rounding up the last digit in "12.3999" is the same as rounding up the
            // last digit in "12.3" - both round up to "12.4".
            while (endOfResultString[-1] == radixDigits[radix - 1])
                --endOfResultString;

            // Radix digits are sequential in ascii/unicode, except for '9' and 'a'.
            // E.g. the first 'if' case handles rounding 67.89 to 67.8a in base 16.
            // The 'else if' case handles rounding of all other digits.
            if (endOfResultString[-1] == '9')
                endOfResultString[-1] = 'a';
            else if (endOfResultString[-1] != '.')
                ++endOfResultString[-1];
            else {
                // One other possibility - there may be no digits to round up in the fraction
                // (or all may be been rounded off already), in which case we may need to
                // round into the integer portion of the number. Remove the decimal point.
                --endOfResultString;
                // In order to get here there must have been a non-zero fraction, in which case
                // there must be at least one bit of the value's mantissa not in use in the
                // integer part of the number. As such, adding to the integer part should not
                // be able to lose precision.
                ASSERT((integerPart + 1) - integerPart == 1);
                ++integerPart;
            }
        } else {
            // We only need to check for trailing zeros if the value does not get rounded up.
            while (endOfResultString[-1] == '0')
                --endOfResultString;
        }

        *endOfResultString = '\0';
        ASSERT(endOfResultString < buffer + sizeof(buffer));
    } else
        *decimalPoint = '\0';

    BigInteger units(integerPart);

    // Always loop at least once, to emit at least '0'.
    do {
        ASSERT(buffer < startOfResultString);

        // Read a single digit and write it to the front of the string.
        // Divide by radix to remove one digit from the value.
        digit = units.divide(radix);
        *--startOfResultString = radixDigits[digit];
    } while (!!units);

    // If the number is negative, prepend '-'.
    if (isNegative)
        *--startOfResultString = '-';
    ASSERT(buffer <= startOfResultString);

    return startOfResultString;
}

///////////////////////////////////////////////////////////////////////////////
//// CODE FROM JAVASCRIPTCORE ENDS ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}
