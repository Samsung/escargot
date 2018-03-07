/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#ifndef __EscargotToStringRecursionPreventer__
#define __EscargotToStringRecursionPreventer__

#include "util/Vector.h"
#include "runtime/String.h"
#include "runtime/Object.h"
#include "runtime/ExecutionState.h"

namespace Escargot {

class ToStringRecursionPreventer : public gc {
public:
    void pushItem(Object* obj)
    {
        ASSERT(canInvokeToString(obj));
        m_registeredItems.push_back(obj);
    }

    Object* pop() // returns item for debug
    {
        Object* ret = m_registeredItems.back();
        m_registeredItems.pop_back();
        return ret;
    }

    bool canInvokeToString(Object* obj)
    {
        if (VectorUtil::findInVector(m_registeredItems, obj) == VectorUtil::invalidIndex) {
            return true;
        }
        return false;
    }

protected:
    Vector<Object*, GCUtil::gc_malloc_ignore_off_page_allocator<Object*>> m_registeredItems;
};

class ToStringRecursionPreventerItemAutoHolder {
public:
    ToStringRecursionPreventerItemAutoHolder(ExecutionState& state, Object* obj);
    ~ToStringRecursionPreventerItemAutoHolder();

protected:
    ToStringRecursionPreventer* m_preventer;
#ifndef NDEBUG
    Object* m_object;
#endif
};
}

#endif
