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

    virtual bool isNumberObject()
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

protected:
    double m_primitiveValue;
};
}

#endif
