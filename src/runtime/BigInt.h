/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotBigInt__
#define __EscargotBigInt__

#include "runtime/PointerValue.h"

namespace Escargot {

class BigInt;
class VMInstance;

class BigIntData {
    friend class BigInt;

public:
    BigIntData(VMInstance* vmInstance, const double& d);
    BigIntData(VMInstance* vmInstance, String* src);
    BigIntData(VMInstance* vmInstance, const char* buf, size_t length, int radix = 10);
    BigIntData(BigIntData&& src);
    BigIntData(const BigIntData& src) = delete;
    BigIntData operator=(const BigIntData& src) = delete;
    ~BigIntData();

    bool lessThan(BigInt* b) const;
    bool lessThanEqual(BigInt* b) const;
    bool greaterThan(BigInt* b) const;
    bool greaterThanEqual(BigInt* b) const;

    bool isNaN();
    bool isInfinity();

private:
    void init(VMInstance* vmInstance, const char* buf, size_t length, int radix);
    bf_t m_data;
};

class BigInt : public PointerValue {
    friend class BigIntData;

public:
    BigInt(VMInstance* vmInstance, int64_t num);
    BigInt(VMInstance* vmInstance, BigIntData&& n);
    BigInt(VMInstance* vmInstance, bf_t num);

    static Optional<BigInt*> parseString(VMInstance* vmInstance, const char* buf, size_t length, int radix = 10);
    static Optional<BigInt*> parseString(VMInstance* vmInstance, String* str, int radix = 10);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    String* toString(ExecutionState& state, int radix = 10);
    double toNumber() const;
    bool equals(BigInt* b);
    bool equals(const BigIntData& b);
    bool equals(String* s);
    bool equals(double b);

    bool lessThan(const BigIntData& b);
    bool lessThanEqual(const BigIntData& b);
    bool greaterThan(const BigIntData& b);
    bool greaterThanEqual(const BigIntData& b);

    bool lessThan(BigInt* b);
    bool lessThanEqual(BigInt* b);
    bool greaterThan(BigInt* b);
    bool greaterThanEqual(BigInt* b);

    BigInt* addition(BigInt* b);
    BigInt* subtraction(BigInt* b);
    BigInt* multiply(BigInt* b);
    BigInt* division(BigInt* b);
    BigInt* remainder(BigInt* b);
    BigInt* pow(BigInt* b);
    BigInt* bitwiseAnd(BigInt* b);
    BigInt* bitwiseOr(BigInt* b);
    BigInt* bitwiseXor(BigInt* b);
    BigInt* increment();
    BigInt* decrement();
    BigInt* bitwiseNot();

    BigInt* leftShift(BigInt* c);
    BigInt* rightShift(BigInt* c);

    bool isZero();
    bool isNaN();
    bool isInfinity();
    bool isNegative();

    BigInt* negativeValue();

    bf_t* bf()
    {
        return &m_bf;
    }

private:
    BigInt(VMInstance* vmInstance);

    void initFinalizer();

    size_t m_tag;
    VMInstance* m_vmInstance;
    bf_t m_bf;
};
}

#endif
