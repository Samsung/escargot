/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotDisposableObject__
#define __EscargotDisposableObject__

#include "runtime/Object.h"

namespace Escargot {

struct DisposableResourceRecord : public PointerValue {
    struct Record {
        EncodedValue m_resourceValue;
        EncodedValue m_disposbleMethod;
        bool m_isAsyncDisposableResource;
        Record(EncodedValue resourceValue, EncodedValue disposbleMethod, bool isAsyncDisposableResource)
            : m_resourceValue(resourceValue)
            , m_disposbleMethod(disposbleMethod)
            , m_isAsyncDisposableResource(isAsyncDisposableResource)
        {
        }
    };
    bool m_needsAwait;
    bool m_hasAwaited;
    EncodedValue m_error;
    Vector<Record, GCUtil::gc_malloc_allocator<Record>> m_records;

    DisposableResourceRecord()
        : m_needsAwait(false)
        , m_hasAwaited(false)
        , m_error(EncodedValue::EmptyValue)
    {
    }

    virtual bool isDisposableResourceRecord() const override
    {
        return true;
    }
};

DisposableResourceRecord::Record createDisposableResource(ExecutionState& state, Value V, bool isAsync, Optional<Value> method);

class DisposableStackObject : public DerivedObject {
public:
    explicit DisposableStackObject(ExecutionState& state, Object* proto)
        : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , m_isDisposed(false)
        , m_record(new DisposableResourceRecord())
    {
    }

    virtual bool isDisposableStackObject() const override
    {
        return true;
    }

    bool disposed() const
    {
        return m_isDisposed;
    }

    Value use(ExecutionState& state, const Value& value);
    void dispose(ExecutionState& state);
    Value adopt(ExecutionState& state, const Value& value, const Value& onDispose);
    void defer(ExecutionState& state, const Value& onDispose);
    DisposableStackObject* move(ExecutionState& state);

protected:
    bool m_isDisposed; // [[DisposableState]]
    DisposableResourceRecord* m_record;
};

} // namespace Escargot

#endif
