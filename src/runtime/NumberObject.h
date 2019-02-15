/*
 *  Copyright (C) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotNumberObject__
#define __EscargotNumberObject__

#include "runtime/Object.h"

namespace Escargot {

class NumberObject : public Object {
public:
    NumberObject(ExecutionState& state, double value = 0);

    double primitiveValue()
    {
        return m_primitiveValue;
    }

    void setPrimitiveValue(ExecutionState& state, double data)
    {
        m_primitiveValue = data;
    }

    virtual bool isNumberObject() const
    {
        return true;
    }

    // The largest finite floating point number is 1.mantissa * 2^(0x7fe-0x3ff).
    // Since 2^N in binary is a one bit followed by N zero bits. 1 * 2^3ff requires
    // at most 1024 characters to the left of a decimal point, in base 2 (1025 if
    // we include a minus sign). For the fraction, a value with an exponent of 0
    // has up to 52 bits to the right of the decimal point. Each decrement of the
    // exponent down to a minimum of -0x3fe adds an additional digit to the length
    // of the fraction. As such the maximum fraction size is 1075 (1076 including
    // a point). We pick a buffer size such that can simply place the point in the
    // center of the buffer, and are guaranteed to have enough space in each direction
    // fo any number of digits an IEEE number may require to represent.
    typedef char RadixBuffer[2180];
    static char* toStringWithRadix(ExecutionState& state, RadixBuffer& buffer, double number, unsigned radix);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Number";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    double m_primitiveValue;
};
}

#endif
