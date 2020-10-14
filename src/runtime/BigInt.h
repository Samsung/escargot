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

class BigInt : public PointerValue {
public:
    BigInt(VMInstance* vmInstance, int64_t num);
    static Optional<BigInt*> parseString(VMInstance* vmInstance, const char* buf, size_t length, int radix = 10);
    static Optional<BigInt*> parseString(VMInstance* vmInstance, String* str, int radix = 10);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    String* toString(ExecutionState& state, int radix = 10);
    bool hasNonZeroValue() const;
    double toNumber() const;

private:
    BigInt(VMInstance* vmInstance, bf_t bf);
    BigInt(VMInstance* vmInstance);

    void initFinalizer();

    size_t m_tag;
    VMInstance* m_vmInstance;
    bf_t m_bf;
};
}

#endif
