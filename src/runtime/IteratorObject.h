/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotIteratorObject__
#define __EscargotIteratorObject__

#include "runtime/Object.h"

namespace Escargot {

class ArrayIteratorObject;
class StringIteratorObject;
class RegExpStringIteratorObject;
class MapIteratorObject;
class SetIteratorObject;
class IteratorHelperObject;
class IteratorObject;

class IteratorRecord : public PointerValue {
public:
    Object* m_iterator;
    EncodedValue m_nextMethod;
    bool m_done;

    virtual bool isIteratorRecord() const override
    {
        return true;
    }

    IteratorRecord(Object* iterator, EncodedValue nextMethod, bool done)
        : m_iterator(iterator)
        , m_nextMethod(nextMethod)
        , m_done(done)
    {
    }
};

class IteratorObject : public DerivedObject {
public:
    explicit IteratorObject(ExecutionState& state);
    explicit IteratorObject(ExecutionState& state, Object* proto);

    virtual bool isIteratorObject() const override
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

    RegExpStringIteratorObject* asRegExpStringIteratorObject()
    {
        ASSERT(isRegExpStringIteratorObject());
        return (RegExpStringIteratorObject*)this;
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

    IteratorHelperObject* asIteratorHelperObject()
    {
        ASSERT(isIteratorHelperObject());
        return (IteratorHelperObject*)this;
    }

    Value next(ExecutionState& state);

    virtual std::pair<Value, bool> advance(ExecutionState& state) = 0;

    static IteratorRecord* getIterator(ExecutionState& state, const Value& obj, const bool sync = true, const Value& func = Value(Value::EmptyValue));
    static Object* iteratorNext(ExecutionState& state, IteratorRecord* iteratorRecord, const Value& value = Value(Value::EmptyValue));
    static bool iteratorComplete(ExecutionState& state, Object* iterResult);
    static Value iteratorValue(ExecutionState& state, Object* iterResult);
    // this function return empty Optional value instead of Value(false)
    static Optional<Object*> iteratorStep(ExecutionState& state, IteratorRecord* iteratorRecord);
    // return null option value when iterator done
    static Optional<Value> iteratorStepValue(ExecutionState& state, IteratorRecord* iteratorRecord);
    static Value iteratorClose(ExecutionState& state, IteratorRecord* iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType);
    static Object* createIterResultObject(ExecutionState& state, const Value& value, bool done);
    // https://www.ecma-international.org/ecma-262/10.0/#sec-iterabletolist
    static ValueVectorWithInlineStorage iterableToList(ExecutionState& state, const Value& items, Optional<Value> method = Optional<Value>());
    static ValueVector iterableToListOfType(ExecutionState& state, const Value items, const String* elementTypes);
    // https://tc39.es/ecma262/multipage/abstract-operations.html#sec-groupby
    enum GroupByKeyCoercion {
        Property,
        Collection
    };
    struct KeyedGroup : public gc {
        Value key;
        ValueVector elements;
    };
    enum PrimitiveHandling {
        IterateStringPrimitives,
        RejectPrimitives,
    };
    typedef Vector<KeyedGroup*, GCUtil::gc_malloc_allocator<KeyedGroup*>> KeyedGroupVector;
    static KeyedGroupVector groupBy(ExecutionState& state, const Value& items, const Value& callbackfn, GroupByKeyCoercion keyCoercion);
    // https://tc39.es/proposal-iterator-helpers/#sec-getiteratordirect
    static IteratorRecord* getIteratorDirect(ExecutionState& state, Object* obj);
    // https://tc39.es/ecma262/#sec-getiteratorfrommethod
    static IteratorRecord* getIteratorFromMethod(ExecutionState& state, const Value& obj, const Value& method);
    // https://tc39.es/ecma262/#sec-getiteratorflattenable
    static IteratorRecord* getIteratorFlattenable(ExecutionState& state, const Value& obj, PrimitiveHandling primitiveHandling);
};

class IteratorHelperObject : public IteratorObject {
public:
    typedef std::pair<Value, bool> (*IteratorHelperObjectCallback)(ExecutionState& state, IteratorHelperObject* obj, void* data);
    IteratorHelperObject(ExecutionState& state, IteratorHelperObjectCallback callback, IteratorRecord* underlyingIterator, void* data);

    virtual bool isIteratorHelperObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    bool isRunning() const
    {
        return m_isRunning;
    }

    IteratorRecord* underlyingIterator() const
    {
        return m_underlyingIterator;
    }

private:
    bool m_isRunning;
    IteratorHelperObjectCallback m_callback;
    IteratorRecord* m_underlyingIterator;
    void* m_data;
};

class WrapForValidIteratorObject : public DerivedObject {
public:
    explicit WrapForValidIteratorObject(ExecutionState& state, Object* proto, IteratorRecord* iterated)
        : DerivedObject(state, proto)
        , m_iterated(iterated)
    {
    }

    virtual bool isWrapForValidIteratorObject() const override
    {
        return true;
    }

    IteratorRecord* iterated() const
    {
        return m_iterated;
    }

private:
    IteratorRecord* m_iterated;
};

} // namespace Escargot

#endif
