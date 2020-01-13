/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotIteratorObject__
#define __EscargotIteratorObject__

#include "runtime/Object.h"

namespace Escargot {

class ArrayIteratorObject;
class StringIteratorObject;
class MapIteratorObject;
class SetIteratorObject;
class IteratorObject;

class IteratorObject : public Object {
public:
    explicit IteratorObject(ExecutionState& state);

    virtual bool isIteratorObject() const
    {
        return true;
    }

    ArrayIteratorObject* asArrayIteratorObject()
    {
        ASSERT(isArrayIteratorObject());
        return (ArrayIteratorObject*)this;
    }

    StringIteratorObject* asStringIteratorObject()
    {
        ASSERT(isStringIteratorObject());
        return (StringIteratorObject*)this;
    }

    MapIteratorObject* asMapIteratorObject()
    {
        ASSERT(isMapIteratorObject());
        return (MapIteratorObject*)this;
    }

    SetIteratorObject* asSetIteratorObject()
    {
        ASSERT(isSetIteratorObject());
        return (SetIteratorObject*)this;
    }

    Value next(ExecutionState& state);

    virtual const char* internalClassProperty()
    {
        return "Iterator";
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    static Value getIterator(ExecutionState& state, const Value& obj, const bool sync = true, const Value& func = Value(Value::EmptyValue));
    static Value iteratorNext(ExecutionState& state, const Value& iteratorRecord, const Value& value = Value(Value::EmptyValue));
    static bool iteratorComplete(ExecutionState& state, const Value& iterResult);
    static Value iteratorValue(ExecutionState& state, const Value& iterResult);
    static Value iteratorStep(ExecutionState& state, const Value& iteratorRecord);
    static Value iteratorClose(ExecutionState& state, const Value& iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType);
    static Value createIterResultObject(ExecutionState& state, const Value& value, bool done);

    // TODO
    static Value createListIteratorRecord(ExecutionState& state, const Value& list);
    // static Value asyncIteratorClose(ExecutionState& state, const Value& iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType);

protected:
};
}

#endif
