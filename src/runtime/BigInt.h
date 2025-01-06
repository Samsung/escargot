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
    BigIntData(String* src, int radix = 10);
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
    void* operator new(size_t, void* p)
    {
        return p;
    }
    void* operator new[](size_t size) = delete;

    String* toString(int radix = 10);
    double toNumber() const;
    int64_t toInt64() const;
    uint64_t toUint64() const;

    bool equals(const BigInt* b) const;
    bool equals(const BigIntData& b) const;
    bool equals(String* s) const;
    bool equals(double b) const;

    bool lessThan(const BigIntData& b) const;
    bool lessThanEqual(const BigIntData& b) const;
    bool greaterThan(const BigIntData& b) const;
    bool greaterThanEqual(const BigIntData& b) const;

    bool lessThan(const BigInt* b) const;
    bool lessThanEqual(const BigInt* b) const;
    bool greaterThan(const BigInt* b) const;
    bool greaterThanEqual(const BigInt* b) const;

    // Binary BigInt Operations
    BigInt* addition(ExecutionState& state, const BigInt* b) const;
    BigInt* subtraction(ExecutionState& state, const BigInt* b) const;
    BigInt* multiply(ExecutionState& state, const BigInt* b) const;
    BigInt* division(ExecutionState& state, const BigInt* b) const;
    BigInt* remainder(ExecutionState& state, const BigInt* b) const;
    BigInt* pow(ExecutionState& state, const BigInt* b) const;
    BigInt* bitwiseAnd(ExecutionState& state, const BigInt* b) const;
    BigInt* bitwiseOr(ExecutionState& state, const BigInt* b) const;
    BigInt* bitwiseXor(ExecutionState& state, const BigInt* b) const;
    BigInt* leftShift(ExecutionState& state, BigInt* c) const;
    BigInt* rightShift(ExecutionState& state, BigInt* c) const;

    // Unary BigInt Operations
    BigInt* increment(ExecutionState& state) const;
    BigInt* decrement(ExecutionState& state) const;
    BigInt* bitwiseNot(ExecutionState& state) const;
    BigInt* negativeValue(ExecutionState& state);
    BigInt* negativeValue();

    bool isZero() const;
    bool isNaN() const;
    bool isInfinity() const;
    bool isNegative() const;

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
