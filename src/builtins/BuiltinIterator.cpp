/*
 * Copyright (c) 2024-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

// https://tc39.es/proposal-iterator-helpers/#sec-iterator-constructor
static Value builtinIteratorConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined or the active function object, throw a TypeError exception.
    if (!newTarget) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }
    if (newTarget->isFunctionObject() && newTarget->asFunctionObject()->codeBlock()->context()->globalObject()->iterator() == newTarget.value()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Abstract class Iterator is not constructable");
    }

    // Return ? OrdinaryCreateFromConstructor(NewTarget, "%Iterator.prototype%").
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->iteratorPrototype();
    });
    return new Object(state, proto);
}

#define RESOLVE_THIS_BINDING_TO_WRAP_FOR_VALID_ITERATOR(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                    \
    if (!thisValue.isObject() || !thisValue.asObject()->isWrapForValidIteratorObject()) {                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    WrapForValidIteratorObject* NAME = thisValue.asObject()->asWrapForValidIteratorObject();

// https://tc39.es/proposal-iterator-helpers/#sec-wrapforvaliditeratorprototype.next
static Value builtinWrapForValidIteratorPrototypeNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be this value.
    // Perform ? RequireInternalSlot(O, [[Iterated]]).
    RESOLVE_THIS_BINDING_TO_WRAP_FOR_VALID_ITERATOR(O, Iterator, next);

    // Let iteratorRecord be O.[[Iterated]].
    IteratorRecord* iteratorRecord = O->iterated();
    // Return ? Call(iteratorRecord.[[NextMethod]], iteratorRecord.[[Iterator]]).
    return Object::call(state, iteratorRecord->m_nextMethod, iteratorRecord->m_iterator, 0, nullptr);
}

// https://tc39.es/proposal-iterator-helpers/#sec-wrapforvaliditeratorprototype.return
static Value builtinWrapForValidIteratorPrototypeReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be this value.
    // Perform ? RequireInternalSlot(O, [[Iterated]]).
    RESOLVE_THIS_BINDING_TO_WRAP_FOR_VALID_ITERATOR(O, Iterator, stringReturn);

    // Let iterator be O.[[Iterated]].[[Iterator]].
    // Assert: iterator is an Object.
    auto iterator = O->iterated()->m_iterator;

    // Let returnMethod be ? GetMethod(iterator, "return").
    auto returnMethod = Object::getMethod(state, iterator, state.context()->staticStrings().stringReturn);

    // If returnMethod is undefined, then
    if (returnMethod.isUndefined()) {
        // Return CreateIterResultObject(undefined, true).
        return IteratorObject::createIterResultObject(state, Value(), true);
    }
    // Return ? Call(returnMethod, iterator).
    return Object::call(state, returnMethod, iterator, 0, nullptr);
}

#define RESOLVE_THIS_BINDING_TO_ITERATOR_HELPER(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                            \
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorHelperObject()) {                                                                                                                                                                    \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver); \
    }                                                                                                                                                                                                                                                  \
    IteratorHelperObject* NAME = thisValue.asObject()->asIteratorObject()->asIteratorHelperObject();

// https://tc39.es/proposal-iterator-helpers/#sec-%iteratorhelperprototype%.next
static Value builtinIteratorHelperPrototypeNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Return ? GeneratorResume(this value, undefined, "Iterator Helper").
    RESOLVE_THIS_BINDING_TO_ITERATOR_HELPER(obj, Iterator, next);
    return obj->next(state);
}

// https://tc39.es/proposal-iterator-helpers/#sec-wrapforvaliditeratorprototype.return
static Value builtinIteratorHelperPrototypeReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be this value.
    // Perform ? RequireInternalSlot(O, [[UnderlyingIterator]]).
    RESOLVE_THIS_BINDING_TO_ITERATOR_HELPER(O, Iterator, stringReturn);
    // Assert: O has a [[GeneratorState]] slot.
    // If O.[[GeneratorState]] is suspended-start, then
    if (!O->underlyingIterator()->m_done) {
        // Set O.[[GeneratorState]] to completed.
        O->underlyingIterator()->m_done = true;
        // NOTE: Once a generator enters the completed state it never leaves it and its associated execution context is never resumed. Any execution state associated with O can be discarded at this point.
        // Perform ? IteratorClose(O.[[UnderlyingIterator]], NormalCompletion(unused)).
        IteratorObject::iteratorClose(state, O->underlyingIterator(), Value(), false);
        // Return CreateIterResultObject(undefined, true).
        return IteratorObject::createIterResultObject(state, Value(), true);
    }
    // Let C be Completion { [[Type]]: return, [[Value]]: undefined, [[Target]]: empty }.
    // Return ? GeneratorResumeAbrupt(O, C, "Iterator Helper").
    return IteratorObject::createIterResultObject(state, Value(), true);
}

// https://tc39.es/proposal-iterator-helpers/#sec-getiteratordirect
IteratorRecord* getIteratorDirect(ExecutionState& state, Object* obj)
{
    // Let nextMethod be ? Get(obj, "next").
    Value nextMethod = obj->get(state, ObjectPropertyName(state.context()->staticStrings().next)).value(state, obj);
    // Let iteratorRecord be Record { [[Iterator]]: obj, [[NextMethod]]: nextMethod, [[Done]]: false }.
    IteratorRecord* record = new IteratorRecord(obj, nextMethod, false);
    // Return iteratorRecord.
    return record;
}

struct IteratorMapData : public gc {
    StorePositiveNumberAsOddNumber counter;
    Value mapper;

    IteratorMapData(Value mapper)
        : counter(0)
        , mapper(mapper)
    {
    }
};

static std::pair<Value, bool> iteratorMapClosure(ExecutionState& state, IteratorHelperObject* obj, void* data)
{
    // Let closure be a new Abstract Closure with no parameters that captures iterated and mapper and performs the following steps when called:
    // Let counter be 0.
    // Repeat,
    //   Let value be ? IteratorStepValue(iterated).
    //   If value is done, return undefined.
    //   Let mapped be Completion(Call(mapper, undefined, « value, 𝔽(counter) »)).
    //   IfAbruptCloseIterator(mapped, iterated).
    //   Let completion be Completion(Yield(mapped)).
    //   IfAbruptCloseIterator(completion, iterated).
    //   Set counter to counter + 1.
    IteratorMapData* closureData = reinterpret_cast<IteratorMapData*>(data);
    IteratorRecord* iterated = obj->underlyingIterator();
    size_t counter = closureData->counter;
    auto value = IteratorObject::iteratorStepValue(state, iterated);
    if (!value) {
        iterated->m_done = true;
        return std::make_pair(Value(), true);
    }
    Value argv[2] = { value.value(), Value(closureData->counter) };
    Value mapped;
    try {
        mapped = Object::call(state, closureData->mapper, Value(), 2, argv);
    } catch (const Value& e) {
        IteratorObject::iteratorClose(state, iterated, e, true);
    }
    closureData->counter = StorePositiveNumberAsOddNumber(counter + 1);
    return std::make_pair(mapped, false);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.map
static Value builtinIteratorMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    const Value& O = thisValue;
    // If O is not an Object, throw a TypeError exception.
    if (!O.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "this value is not Object");
    }
    // If IsCallable(mapper) is false, throw a TypeError exception.
    const Value& mapper = argv[0];
    if (!mapper.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "mapper is not callable");
    }

    // Let iterated be ? GetIteratorDirect(O).
    IteratorRecord* iterated = getIteratorDirect(state, O.asObject());
    // Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // Set result.[[UnderlyingIterator]] to iterated.
    IteratorHelperObject* result = new IteratorHelperObject(state, iteratorMapClosure, iterated, new IteratorMapData(mapper));
    // Return result.
    return result;
}

static Value builtinGenericIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isGenericIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, String::fromASCII("Iterator"), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    IteratorObject* iter = thisValue.asObject()->asIteratorObject();
    return iter->next(state);
}

void GlobalObject::initializeIterator(ExecutionState& state)
{
    installIterator(state);
}

void GlobalObject::installIterator(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    m_asyncIteratorPrototype = new PrototypeObject(state);
    m_asyncIteratorPrototype->setGlobalIntrinsicObject(state, true);
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asynciteratorprototype-asynciterator
    m_asyncIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().asyncIterator),
                                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.asyncIterator]")), builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)),
                                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iteratorPrototype = new PrototypeObject(state);
    m_iteratorPrototype->setGlobalIntrinsicObject(state, true);
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-%iteratorprototype%-@@iterator
    m_iteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                                          ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.iterator]")), builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)),
                                                                                   (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_iteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                          ObjectPropertyDescriptor(Value(strings->Iterator.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_genericIteratorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_genericIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_genericIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->next),
                                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->next, builtinGenericIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_genericIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                                 ObjectPropertyDescriptor(Value(String::fromASCII("Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iterator = new NativeFunctionObject(state, NativeFunctionInfo(strings->Iterator, builtinIteratorConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_iterator->setGlobalIntrinsicObject(state);

    // https://tc39.es/proposal-iterator-helpers/#sec-iterator.prototype
    m_iterator->setFunctionPrototype(state, m_iteratorPrototype);

    // https://tc39.es/proposal-iterator-helpers/#sec-wrapforvaliditeratorprototype-object
    m_wrapForValidIteratorPrototype = new Object(state, m_iteratorPrototype);
    m_wrapForValidIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_wrapForValidIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->next),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->next, builtinWrapForValidIteratorPrototypeNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_wrapForValidIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->stringReturn),
                                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->stringReturn, builtinWrapForValidIteratorPrototypeReturn, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iteratorHelperPrototype = new Object(state, m_iteratorPrototype);
    m_iteratorHelperPrototype->setGlobalIntrinsicObject(state, true);

    m_iteratorHelperPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                       ObjectPropertyDescriptor(String::fromASCII("Iterator Helper"), ObjectPropertyDescriptor::ConfigurablePresent));

    m_iteratorHelperPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->next),
                                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->next, builtinIteratorHelperPrototypeNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iteratorHelperPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->stringReturn),
                                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->stringReturn, builtinIteratorHelperPrototypeReturn, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_iteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->map),
                                                 ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->map, builtinIteratorMap, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    directDefineOwnProperty(state, ObjectPropertyName(strings->Iterator),
                            ObjectPropertyDescriptor(m_iterator, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
