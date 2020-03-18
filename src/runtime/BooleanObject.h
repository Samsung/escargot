/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotBooleanObject__
#define __EscargotBooleanObject__

#include "runtime/Object.h"

namespace Escargot {

class BooleanObject : public Object {
public:
    explicit BooleanObject(ExecutionState& state, bool value = false);
    explicit BooleanObject(ExecutionState& state, Object* proto, bool value = false);

    bool primitiveValue()
    {
        return m_primitiveValue;
    }

    void setPrimitiveValue(ExecutionState& state, bool val)
    {
        m_primitiveValue = val;
    }

    virtual bool isBooleanObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty(ExecutionState& state)
    {
        return "Boolean";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    bool m_primitiveValue : 1;
};
}

#endif
