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
}

BigIntData::BigIntData(VMInstance* vmInstance, String* src)
{
    const auto& bd = src->bufferAccessData();
    char* buffer;

    if (bd.has8BitContent) {
        buffer = (char*)bd.bufferAs8Bit;
    } else {
        if (!isAllASCII(bd.bufferAs16Bit, bd.length)) {
            bf_init(vmInstance->bfContext(), &m_data);
            bf_set_nan(&m_data);
            return;
        }
        buffer = ALLOCA(bd.length, char, vmInstance);

        for (size_t i = 0; i < bd.length; i++) {
            buffer[i] = bd.uncheckedCharAtFor16Bit(i);
        }
    }

    init(vmInstance, buffer, bd.length, 10);
}

BigIntData::BigIntData(VMInstance* vmInstance, const char* buf, size_t length, int radix)
{
    init(vmInstance, buf, length, radix);
}

void BigIntData::init(VMInstance* vmInstance, const char* buf, size_t length, int radix)
{
    bf_init(vmInstance->bfContext(), &m_data);
    if (!length) {
        bf_set_zero(&m_data, 0);
        return;
    }
    // bf_atof needs zero-terminated string
    char* newBuf = ALLOCA(length + 1, char, vmInstance);
    memcpy(newBuf, buf, length);
    newBuf[length] = 0;
    bf_t a;
    int ret = bf_atof(&m_data, newBuf, NULL, radix, BF_PREC_INF, BF_RNDZ);
    if (ret) {
        bf_set_nan(&m_data);
    }
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
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(BigInt)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BigInt, m_vmInstance));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(BigInt));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void BigInt::initFinalizer()
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        BigInt* self = (BigInt*)obj;
        bf_delete(&self->m_bf);
    },
                                   nullptr, nullptr, nullptr);
}

BigInt::BigInt(VMInstance* vmInstance)
    : m_tag(POINTER_VALUE_BIGINT_TAG_IN_DATA)
    , m_vmInstance(vmInstance)
{
    bf_init(m_vmInstance->bfContext(), &m_bf);
    initFinalizer();
}

BigInt::BigInt(VMInstance* vmInstance, int64_t num)
    : BigInt(vmInstance)
{
    bf_set_si(&m_bf, num);
}

BigInt::BigInt(VMInstance* vmInstance, BigIntData&& n)
    : BigInt(vmInstance)
{
    bf_move(&m_bf, &n.m_data);
}

BigInt::BigInt(VMInstance* vmInstance, bf_t bf)
    : m_tag(POINTER_VALUE_BIGINT_TAG_IN_DATA)
    , m_vmInstance(vmInstance)
    , m_bf(bf)
{
    initFinalizer();
}

Optional<BigInt*> BigInt::parseString(VMInstance* vmInstance, const char* buf, size_t length, int radix)
{
    bf_t a;
    bf_init(vmInstance->bfContext(), &a);
    if (!length) {
        bf_set_zero(&a, 0);
        return new BigInt(vmInstance, a);
    }
    // bf_atof needs zero-terminated string
    char* newBuf = ALLOCA(length + 1, char, vmInstance);
    memcpy(newBuf, buf, length);
    newBuf[length] = 0;
    int ret = bf_atof(&a, newBuf, NULL, radix, BF_PREC_INF, BF_RNDZ);
    if (ret & BF_ST_MEM_ERROR) {
        RELEASE_ASSERT_NOT_REACHED();
    }
    if (ret) {
        bf_delete(&a);
        return nullptr;
    }
    return new BigInt(vmInstance, a);
}

Optional<BigInt*> BigInt::parseString(VMInstance* vmInstance, String* str, int radix)
{
    const auto& bd = str->bufferAccessData();
    char* buffer;

    if (bd.has8BitContent) {
        buffer = (char*)bd.bufferAs8Bit;
    } else {
        if (!isAllASCII(bd.bufferAs16Bit, bd.length)) {
            return nullptr;
        }
        buffer = ALLOCA(bd.length, char, vmInstance);

        for (size_t i = 0; i < bd.length; i++) {
            buffer[i] = bd.uncheckedCharAtFor16Bit(i);
        }
    }

    return parseString(vmInstance, buffer, bd.length, radix);
}

String* BigInt::toString(ExecutionState& state, int radix)
{
    int savedSign = m_bf.sign;
    if (m_bf.expn == BF_EXP_ZERO) {
        m_bf.sign = 0;
    }
    size_t resultLen = 0;
    auto str = bf_ftoa(&resultLen, &m_bf, radix, 0, BF_RNDZ | BF_FTOA_FORMAT_FRAC | BF_FTOA_JS_QUIRKS);
    m_bf.sign = savedSign;

    if (UNLIKELY(!str)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Failed to convert BigInt to string");
    }
    String* ret = String::fromASCII(str, resultLen);
    bf_free(m_vmInstance->bfContext(), str);

    return ret;
}

double BigInt::toNumber() const
{
    double d;
    bf_get_float64(&m_bf, &d, BF_RNDN);
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
    BigIntData bd(m_vmInstance, s);
    return equals(bd);
}

bool BigInt::equals(double b)
{
    return toNumber() == b;
}

bool BigInt::isNaN()
{
    return bf_is_nan(&m_bf);
}

bool BigInt::isInfinity()
{
    return !bf_is_finite(&m_bf);
}

bool BigInt::isZero()
{
    return bf_is_zero(&m_bf);
}

BigInt* BigInt::negativeValue()
{
    if (isNaN() || isZero()) {
        return this;
    }

    bf_t bf;
    bf_init(m_vmInstance->bfContext(), &bf);
    bf_set(&bf, &m_bf);
    bf_neg(&bf);
    return new BigInt(m_vmInstance, bf);
}
}
