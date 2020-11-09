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

#ifndef __EscargotBigIntObject__
#define __EscargotBigIntObject__

#include "runtime/Object.h"
#include "runtime/BigInt.h"

namespace Escargot {

class BigInt;

class BigIntObject : public Object {
public:
    explicit BigIntObject(ExecutionState& state, BigInt* b);
    explicit BigIntObject(ExecutionState& state, Object* proto, BigInt* b);

    BigInt* primitiveValue()
    {
        return m_primitiveValue;
    }

    virtual bool isBigIntObject() const override
    {
        return true;
    }

    void setBigIntValue(ExecutionState& state, BigInt* data)
    {
        m_primitiveValue = data;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    BigInt* m_primitiveValue;
};
}

#endif
