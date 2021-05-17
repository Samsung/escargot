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

#include "Escargot.h"
#include "BigInt.h"
#include "VMInstance.h"
#include "ErrorObject.h"

namespace Escargot {

BigIntData::~BigIntData()
{
    bf_delete(&m_data);
}

BigIntData::BigIntData(BigIntData&& src)
{
    bf_init(src.m_data.ctx, &m_data);
    bf_move(&m_data, &src.m_data);
    bf_init(m_data.ctx, &src.m_data);
}

BigIntData::BigIntData(const double& d)
{
    bf_init(VMInstance::bfContext(), &m_data);
    bf_set_float64(&m_data, d);
}

BigIntData::BigIntData(String* src)
{
    const auto& bd = src->bufferAccessData();
    char* buffer;

    if (bd.has8BitContent) {
        buffer = (char*)bd.bufferAs8Bit;
    } else {
        if (!isAllASCII(bd.bufferAs16Bit, bd.length)) {
            bf_init(VMInstance::bfContext(), &m_data);
            bf_set_nan(&m_data);
            return;
        }
        buffer = ALLOCA(bd.length, char, nullptr);

        for (size_t i = 0; i < bd.length; i++) {
            buffer[i] = bd.uncheckedCharAtFor16Bit(i);
        }
    }

    init(buffer, bd.length, 10);
}

BigIntData::BigIntData(const char* buf, size_t length, int radix)
{
    init(buf, length, radix);
}

void BigIntData::init(const char* buf, size_t length, int radix)
{
    bf_init(VMInstance::bfContext(), &m_data);
    if (!length) {
        bf_set_zero(&m_data, 0);
        return;
    }
    // bf_atof needs zero-terminated string
    char* newBuf = ALLOCA(length + 1, char, nullptr);
    for (size_t i = 0; i < length; i++) {
        if (UNLIKELY(buf[i] == '.')) {
            bf_set_nan(&m_data);
            return;
        }
        if (UNLIKELY(buf[i] == 'e' || buf[i] == 'E')) {
            if (length < 3 || buf[0] != '0' || buf[1] != 'x') {
                bf_set_nan(&m_data);
                return;
            }
        }
        if (UNLIKELY((buf[i] == 'B' || buf[i] == 'b' || buf[i] == 'O' || buf[i] == 'o' || buf[i] == 'x' || buf[i] == 'X') && buf[0] == '-')) {
            bf_set_nan(&m_data);
            return;
        }
    }
    memcpy(newBuf, buf, length);
    newBuf[length] = 0;
    bf_t a;
    const char* testEnd = nullptr;
    int ret = bf_atof(&m_data, newBuf, &testEnd, radix == 10 ? 0 : radix, BF_PREC_INF, BF_RNDZ | BF_ATOF_BIN_OCT);
    if (ret) {
        bf_set_nan(&m_data);
    }
    if (testEnd != &newBuf[length]) {
        bf_set_nan(&m_data);
    }
}

bool BigIntData::lessThan(BigInt* b) const
{
    return bf_cmp_lt(&m_data, &b->m_bf);
}

bool BigIntData::lessThanEqual(BigInt* b) const
{
    return bf_cmp_le(&m_data, &b->m_bf);
}

bool BigIntData::greaterThan(BigInt* b) const
{
    return bf_cmp(&m_data, &b->m_bf) > 0;
}

bool BigIntData::greaterThanEqual(BigInt* b) const
{
    return bf_cmp(&m_data, &b->m_bf) >= 0;
}

bool BigIntData::isNaN()
{
    return bf_is_nan(&m_data);
}

bool BigIntData::isInfinity()
{
    return !bf_is_finite(&m_data);
}

void* BigInt::operator new(size_t size)
{
    return GC_MALLOC_ATOMIC(size);
}

void BigInt::initFinalizer()
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        BigInt* self = (BigInt*)obj;
        bf_delete(&self->m_bf);
    },
                                   nullptr, nullptr, nullptr);
}

BigInt::BigInt()
    : m_tag(POINTER_VALUE_BIGINT_TAG_IN_DATA)
{
    bf_init(VMInstance::bfContext(), &m_bf);
    initFinalizer();
}

static void setBigInt(bf_t* bf, uint64_t num)
{
    // FIXME (there is bug in bf_set_ui)
    if (UNLIKELY(num > std::numeric_limits<uint32_t>::max())) {
        uint32_t sub = num >> 32;
        bf_set_ui(bf, sub);
        bf_mul_2exp(bf, 32, BF_PREC_INF, BF_RNDZ);
        bf_add_si(bf, bf, num & 0xffffffff, BF_PREC_INF, BF_RNDZ);
    } else {
        bf_set_ui(bf, num);
    }
}

BigInt::BigInt(int64_t num)
    : BigInt()
{
    int sign = num < 0;
    if (sign) {
        num = -num;
    }
    setBigInt(&m_bf, num);
    if (sign) {
        m_bf.sign = 1;
    }
}

BigInt::BigInt(uint64_t num)
    : BigInt()
{
    setBigInt(&m_bf, num);
}

BigInt::BigInt(BigIntData&& n)
    : BigInt()
{
    bf_move(&m_bf, &n.m_data);
    bf_init(m_bf.ctx, &n.m_data);
}

BigInt::BigInt(bf_t bf)
    : m_tag(POINTER_VALUE_BIGINT_TAG_IN_DATA)
    , m_bf(bf)
{
    initFinalizer();
}

Optional<BigInt*> BigInt::parseString(const char* buf, size_t length, int radix)
{
    BigIntData dat(buf, length, radix);
    if (dat.isNaN()) {
        return nullptr;
    }
    return new BigInt(std::move(dat));
}

Optional<BigInt*> BigInt::parseString(String* str, int radix)
{
    const auto& bd = str->bufferAccessData();
    char* buffer;

    if (bd.has8BitContent) {
        buffer = (char*)bd.bufferAs8Bit;
    } else {
        if (!isAllASCII(bd.bufferAs16Bit, bd.length)) {
            return nullptr;
        }
        buffer = ALLOCA(bd.length, char, nullptr);

        for (size_t i = 0; i < bd.length; i++) {
            buffer[i] = bd.uncheckedCharAtFor16Bit(i);
        }
    }

    return parseString(buffer, bd.length, radix);
}

void BigInt::throwBFException(ExecutionState& state, int status)
{
    const char* message = nullptr;
    if (status & BF_ST_MEM_ERROR) {
        message = ErrorObject::Messages::OutOfMemory;
    } else if (status & BF_ST_DIVIDE_ZERO) {
        message = ErrorObject::Messages::DivisionByZero;
    } else if (status & BF_ST_INVALID_OP) {
        message = ErrorObject::Messages::GlobalObject_RangeError;
    } else {
        message = ErrorObject::Messages::Overflow;
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, message);
    return;
}

String* BigInt::toString(int radix)
{
    int savedSign = m_bf.sign;
    if (m_bf.expn == BF_EXP_ZERO) {
        m_bf.sign = 0;
    }
    size_t resultLen = 0;
    auto str = bf_ftoa(&resultLen, &m_bf, radix, 0, BF_RNDZ | BF_FTOA_FORMAT_FRAC | BF_FTOA_JS_QUIRKS);
    m_bf.sign = savedSign;

    ASSERT(str);
    if (UNLIKELY(!str)) {
        return String::emptyString;
    } else {
        String* ret = String::fromASCII(str, resultLen);
        bf_free(VMInstance::bfContext(), str);
        return ret;
    }
}

double BigInt::toNumber() const
{
    double d;
    bf_get_float64(&m_bf, &d, BF_RNDN);
    return d;
}

int64_t BigInt::toInt64() const
{
    int64_t d;
    bf_get_int64(&d, &m_bf, BF_GET_INT_MOD);
    return d;
}

uint64_t BigInt::toUint64() const
{
    uint64_t d;
    bf_get_uint64(&d, &m_bf, BF_GET_INT_MOD);
    return d;
}


bool BigInt::equals(BigInt* b)
{
    return bf_cmp_eq(&m_bf, &b->m_bf);
}

bool BigInt::equals(const BigIntData& b)
{
    return bf_cmp_eq(&m_bf, &b.m_data);
}

bool BigInt::equals(String* s)
{
    BigIntData bd(s);
    return equals(bd);
}

bool BigInt::equals(double b)
{
    BigIntData bd(b);
    return equals(bd);
}

bool BigInt::lessThan(const BigIntData& b)
{
    return bf_cmp_lt(&m_bf, &b.m_data);
}

bool BigInt::lessThanEqual(const BigIntData& b)
{
    return bf_cmp_le(&m_bf, &b.m_data);
}

bool BigInt::greaterThan(const BigIntData& b)
{
    return bf_cmp(&m_bf, &b.m_data) > 0;
}

bool BigInt::greaterThanEqual(const BigIntData& b)
{
    return bf_cmp(&m_bf, &b.m_data) >= 0;
}

bool BigInt::lessThan(BigInt* b)
{
    return bf_cmp_lt(&m_bf, &b->m_bf);
}

bool BigInt::lessThanEqual(BigInt* b)
{
    return bf_cmp_le(&m_bf, &b->m_bf);
}

bool BigInt::greaterThan(BigInt* b)
{
    return bf_cmp(&m_bf, &b->m_bf) > 0;
}

bool BigInt::greaterThanEqual(BigInt* b)
{
    return bf_cmp(&m_bf, &b->m_bf) >= 0;
}

BigInt* BigInt::addition(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_add(&r, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::subtraction(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_sub(&r, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::multiply(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_mul(&r, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::division(ExecutionState& state, BigInt* b)
{
    bf_t r, rem;
    bf_init(VMInstance::bfContext(), &r);
    bf_init(VMInstance::bfContext(), &rem);
    int ret = bf_divrem(&r, &rem, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ,
                        BF_RNDZ);
    bf_delete(&rem);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::remainder(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_rem(&r, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ,
                     BF_RNDZ)
        & BF_ST_INVALID_OP;
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::pow(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_pow(&r, &m_bf, &b->m_bf, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::bitwiseAnd(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_logic_and(&r, &m_bf, &b->m_bf);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::bitwiseOr(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_logic_or(&r, &m_bf, &b->m_bf);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::bitwiseXor(ExecutionState& state, BigInt* b)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_logic_xor(&r, &m_bf, &b->m_bf);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::leftShift(ExecutionState& state, BigInt* src)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);

    slimb_t v2;
#if defined(ESCARGOT_32)
    bf_get_int32(&v2, src->bf(), 0);
    if (v2 == std::numeric_limits<int32_t>::min()) {
        v2 = std::numeric_limits<int32_t>::min() + 1;
    }
#else
    bf_get_int64(&v2, src->bf(), 0);
    if (v2 == std::numeric_limits<int64_t>::min())
        v2 = std::numeric_limits<int64_t>::min() + 1;
#endif
    // if (op == OP_sar)
    //     v2 = -v2;
    int ret = bf_set(&r, &m_bf);
    ret |= bf_mul_2exp(&r, v2, BF_PREC_INF, BF_RNDZ);
    if (v2 < 0) {
        ret |= bf_rint(&r, BF_RNDD) & (BF_ST_OVERFLOW | BF_ST_MEM_ERROR);
    }
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::rightShift(ExecutionState& state, BigInt* src)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);

    slimb_t v2;
#if defined(ESCARGOT_32)
    bf_get_int32(&v2, src->bf(), 0);
    if (v2 == std::numeric_limits<int32_t>::min()) {
        v2 = std::numeric_limits<int32_t>::min() + 1;
    }
#else
    bf_get_int64(&v2, src->bf(), 0);
    if (v2 == std::numeric_limits<int64_t>::min())
        v2 = std::numeric_limits<int64_t>::min() + 1;
#endif
    v2 = -v2;
    int ret = bf_set(&r, &m_bf);
    ret |= bf_mul_2exp(&r, v2, BF_PREC_INF, BF_RNDZ);
    if (v2 < 0) {
        ret |= bf_rint(&r, BF_RNDD) & (BF_ST_OVERFLOW | BF_ST_MEM_ERROR);
    }
    UNUSED_VARIABLE(ret);
    // FIXME check overflow for rightshift operation
    /*
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    */
    return new BigInt(r);
}

BigInt* BigInt::increment(ExecutionState& state)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_add_si(&r, &m_bf, 1, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::decrement(ExecutionState& state)
{
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_add_si(&r, &m_bf, -1, BF_PREC_INF, BF_RNDZ);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::bitwiseNot(ExecutionState& state)
{
    // The abstract operation BigInt::bitwiseNOT with an argument x of BigInt type returns the one's complement of x; that is, -x - 1.
    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_add_si(&r, &m_bf, 1, BF_PREC_INF, BF_RNDZ);
    bf_neg(&r);

    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::negativeValue(ExecutionState& state)
{
    if (isNaN() || isZero()) {
        return this;
    }

    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_set(&r, &m_bf);
    bf_neg(&r);
    if (UNLIKELY(ret)) {
        bf_delete(&r);
        throwBFException(state, ret);
    }
    return new BigInt(r);
}

BigInt* BigInt::negativeValue()
{
    // used only for parsing
    // any exception is not allowed
    if (isNaN() || isZero()) {
        return this;
    }

    bf_t r;
    bf_init(VMInstance::bfContext(), &r);
    int ret = bf_set(&r, &m_bf);
    bf_neg(&r);
    ASSERT(!ret);
    return new BigInt(r);
}

bool BigInt::isZero()
{
    return bf_is_zero(&m_bf);
}

bool BigInt::isNaN()
{
    return bf_is_nan(&m_bf);
}

bool BigInt::isInfinity()
{
    return !bf_is_finite(&m_bf);
}

bool BigInt::isNegative()
{
    return m_bf.sign;
}

} // namespace Escargot
