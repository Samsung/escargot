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
    // do not read this directly, it is empty while the record iterates an array
    // directly; go through iterator() so the iterator object gets materialized
    Optional<Object*> m_iteratorSlot;
    EncodedValue m_nextMethod;
    // set when this record iterates a pristine fast-mode array without an
    // iterator object. the builtin ArrayIterator would do exactly what
    // advanceDirectArray does, so it is only allocated if something actually
    // asks for the iterator object
    Optional<ArrayObject*> m_directArray;
    uint32_t m_directArrayIndex{ 0 };
    bool m_done;
    // true when the record steps without running the generic protocol: either
    // it iterates an array directly, or the iterator is a builtin iterator of
    // the current realm whose looked-up next method is still the untouched
    // builtin. stepping via IteratorObject::advance() is then observably
    // identical to Call(nextMethod, iterator)
    bool m_isFastBuiltinIterator{ false };
    // the spec drops [[IteratedObject]] once the iteration ends, so an array
    // that grows afterwards must not resurrect the iteration
    bool m_directArrayDone{ false };

    virtual bool isIteratorRecord() const override
    {
        return true;
    }

    IteratorRecord(Object* iterator, EncodedValue nextMethod, bool done)
        : m_iteratorSlot(iterator)
        , m_nextMethod(nextMethod)
        , m_done(done)
    {
    }

    // the array is iterated directly; nextMethod must be the builtin
    // %ArrayIteratorPrototype%.next so that materializing later is invisible
    IteratorRecord(ArrayObject* directArray, EncodedValue nextMethod)
        : m_nextMethod(nextMethod)
        , m_directArray(directArray)
        , m_done(false)
        , m_isFastBuiltinIterator(true)
    {
    }

    bool isDirectArrayIteration() const
    {
        return m_directArray.hasValue();
    }

    Object* iterator(ExecutionState& state);
    std::pair<Value, bool> advanceDirectArray(ExecutionState& state);
    // steps a record that m_isFastBuiltinIterator vouches for; must not be
    // called on any other record
    inline std::pair<Value, bool> fastAdvance(ExecutionState& state);
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
    static void tryMarkFastBuiltinIterator(ExecutionState& state, IteratorRecord* record);
    // non-null when iterating `value` with the iterator protocol is observably
    // identical to walking its fast-mode elements directly
    static Optional<ArrayObject*> tryFastArrayIterationSource(ExecutionState& state, const Value& value);
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
    // true when record iterates an array directly and closing it would find no
    // "return" method, so the close is a no-op and the iterator object that the
    // lookup alone would have needed does not have to be built
    static bool directArrayIterationClosesWithoutReturn(ExecutionState& state, IteratorRecord* record);
    static bool arrayIteratorPrototypeChainHasReturn(ExecutionState& state);
};

inline std::pair<Value, bool> IteratorRecord::fastAdvance(ExecutionState& state)
{
    ASSERT(m_isFastBuiltinIterator);
    if (LIKELY(m_directArray.hasValue())) {
        return advanceDirectArray(state);
    }
    return m_iteratorSlot.value()->asIteratorObject()->advance(state);
}

class IteratorHelperObject : public IteratorObject {
public:
    struct IteratorHelperObjectRunningStateChanger {
        IteratorHelperObject& obj;
        IteratorHelperObjectRunningStateChanger(IteratorHelperObject& obj)
            : obj(obj)
        {
            ASSERT(!obj.m_isRunning);
            obj.m_isRunning = true;
        }
        ~IteratorHelperObjectRunningStateChanger()
        {
            obj.m_isRunning = false;
        }
    };

    typedef std::pair<Value, bool> (*IteratorHelperObjectCallback)(ExecutionState& state, IteratorHelperObject* obj, void* data);
    IteratorHelperObject(ExecutionState& state, IteratorHelperObjectCallback callback, Optional<IteratorRecord*> underlyingIterator, void* data);

    virtual bool isIteratorHelperObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    bool isDone() const
    {
        return m_isDone;
    }

    void markIteratorIsDone()
    {
        m_isDone = true;
    }

    bool isRunning() const
    {
        return m_isRunning;
    }

    TightVector<IteratorRecord*, GCUtil::gc_malloc_allocator<IteratorRecord*>>& underlyingIterators()
    {
        return m_underlyingIterators;
    }

private:
    // [[GeneratorState]]
    bool m_isDone;
    bool m_isRunning;

    IteratorHelperObjectCallback m_callback;
    TightVector<IteratorRecord*, GCUtil::gc_malloc_allocator<IteratorRecord*>> m_underlyingIterators;
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
