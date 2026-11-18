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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef PowerOfTwoInteger_h
#define PowerOfTwoInteger_h

#include <cmath>
#include <cstdint>
#include <limits>

namespace Escargot {

// Accumulate a non-negative integer in a power-of-two radix and round once
// to binary64. The first discarded bit and sticky bit implement ties-to-even.
class PowerOfTwoInteger {
public:
    void append(unsigned digit, unsigned bitsPerDigit)
    {
        for (unsigned i = bitsPerDigit; i > 0; --i) {
            bool bit = (digit >> (i - 1)) & 1;
            if (!m_bitCount && !bit) {
                continue;
            }
            if (m_bitCount < 53) {
                m_significand = (m_significand << 1) | bit;
            } else if (m_bitCount == 53) {
                m_roundBit = bit;
            } else {
                m_stickyBit |= bit;
            }
            // At 1025 bits the number is already beyond binary64 range.
            if (m_bitCount < 1025) {
                ++m_bitCount;
            }
        }
    }

    double toDouble() const
    {
        if (m_bitCount <= 53) {
            return static_cast<double>(m_significand);
        }
        if (m_bitCount > 1024) {
            return std::numeric_limits<double>::infinity();
        }
        uint64_t rounded = m_significand;
        if (m_roundBit && (m_stickyBit || (rounded & 1))) {
            ++rounded;
        }
        return std::ldexp(static_cast<double>(rounded), static_cast<int>(m_bitCount) - 53);
    }

private:
    uint64_t m_significand = 0;
    unsigned m_bitCount = 0;
    bool m_roundBit = false;
    bool m_stickyBit = false;
};

} // namespace Escargot

#endif
