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

class BigIntData {
    friend class BigInt;

public:
    BigIntData(const double& d);
    BigIntData(String* src);
    BigIntData(const char* buf, size_t length, int radix = 10);
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
    void init(const char* buf, size_t length, int radix);
    bf_t m_data;
};

class BigInt : public PointerValue {
    friend class BigIntData;

public:
    BigInt(int64_t num);
    BigInt(uint64_t num);
    BigInt(BigIntData&& n);
    BigInt(bf_t num);

    static Optional<BigInt*> parseString(const char* buf, size_t length, int radix = 10);
    static Optional<BigInt*> parseString(String* str, int radix = 10);
    static void throwBFException(ExecutionState& state, int status);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    String* toString(int radix = 10);
    double toNumber() const;
    int64_t toInt64() const;
    uint64_t toUint64() const;

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

    // Binary BigInt Operations
    BigInt* addition(ExecutionState& state, BigInt* b);
    BigInt* subtraction(ExecutionState& state, BigInt* b);
    BigInt* multiply(ExecutionState& state, BigInt* b);
    BigInt* division(ExecutionState& state, BigInt* b);
    BigInt* remainder(ExecutionState& state, BigInt* b);
    BigInt* pow(ExecutionState& state, BigInt* b);
    BigInt* bitwiseAnd(ExecutionState& state, BigInt* b);
    BigInt* bitwiseOr(ExecutionState& state, BigInt* b);
    BigInt* bitwiseXor(ExecutionState& state, BigInt* b);
    BigInt* leftShift(ExecutionState& state, BigInt* c);
    BigInt* rightShift(ExecutionState& state, BigInt* c);

    // Unary BigInt Operations
    BigInt* increment(ExecutionState& state);
    BigInt* decrement(ExecutionState& state);
    BigInt* bitwiseNot(ExecutionState& state);
    BigInt* negativeValue(ExecutionState& state);
    BigInt* negativeValue();

    bool isZero();
    bool isNaN();
    bool isInfinity();
    bool isNegative();

    bf_t* bf()
    {
        return &m_bf;
    }

private:
    BigInt();

    void initFinalizer();

    size_t m_typeTag;
    bf_t m_bf;
};
} // namespace Escargot

#endif
