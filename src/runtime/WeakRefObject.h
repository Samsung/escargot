/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotWeakRefObject__
#define __EscargotWeakRefObject__

#include "runtime/Object.h"

namespace Escargot {

class WeakRefObject : public Object {
public:
    explicit WeakRefObject(ExecutionState& state, Object* target);
    explicit WeakRefObject(ExecutionState& state, Object* proto, Object* target);

    virtual bool isWeakRefObject() const
    {
        return true;
    }

    Value getTarget()
    {
        if (m_target.hasValue()) {
            return m_target.value();
        }
        return Value();
    }

    bool deleteOperation(ExecutionState& state);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    Optional<Object*> m_target;
};
} // namespace Escargot

#endif
