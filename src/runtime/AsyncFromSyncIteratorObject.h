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

#ifndef __EscargotAsyncFromSyncIteratorObject__
#define __EscargotAsyncFromSyncIteratorObject__

#include "runtime/Context.h"
#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class IteratorRecord;

// https://www.ecma-international.org/ecma-262/10.0/#sec-async-from-sync-iterator-objects
class AsyncFromSyncIteratorObject : public DerivedObject {
public:
    AsyncFromSyncIteratorObject(ExecutionState& state, Object* proto, IteratorRecord* syncIteratorRecord);

    // https://www.ecma-international.org/ecma-262/10.0/#sec-createasyncfromsynciterator
    static IteratorRecord* createAsyncFromSyncIterator(ExecutionState& state, IteratorRecord* syncIteratorRecord)
    {
        return IteratorObject::getIterator(state, new AsyncFromSyncIteratorObject(state, state.context()->globalObject()->asyncFromSyncIteratorPrototype(), syncIteratorRecord), false);
    }

    virtual bool isAsyncFromSyncIteratorObject() const override
    {
        return true;
    }

    IteratorRecord* syncIteratorRecord() const
    {
        return m_syncIteratorRecord;
    }

private:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(AsyncFromSyncIteratorObject, m_syncIteratorRecord));
    }

    // [[SyncIteratorRecord]]
    IteratorRecord* m_syncIteratorRecord;
};
} // namespace Escargot

#endif
