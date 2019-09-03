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

#include "Escargot.h"
#include "ArgumentsObject.h"
#include "Context.h"
#include "EnvironmentRecord.h"
#include "Environment.h"
#include "ScriptFunctionObject.h"

namespace Escargot {

static Value ArgumentsObjectNativeGetter(ExecutionState& state, Object* self, FunctionEnvironmentRecord* targetRecord, InterpretedCodeBlock* codeBlock, AtomicString name)
{
    InterpretedCodeBlock::IdentifierInfo info = codeBlock->identifierInfos()[codeBlock->findVarName(name)];
    ASSERT(!info.m_needToAllocateOnStack);
    if (info.m_indexForIndexedStorage == SIZE_MAX) {
        return targetRecord->getBindingValue(state, name).m_value;
    }
    return targetRecord->getHeapValueByIndex(state, info.m_indexForIndexedStorage);
}

static void ArgumentsObjectNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData, FunctionEnvironmentRecord* targetRecord, InterpretedCodeBlock* codeBlock, AtomicString name)
{
    InterpretedCodeBlock::IdentifierInfo info = codeBlock->identifierInfos()[codeBlock->findVarName(name)];
    ASSERT(!info.m_needToAllocateOnStack);
    if (info.m_indexForIndexedStorage == SIZE_MAX) {
        targetRecord->setMutableBinding(state, name, setterInputData);
        return;
    }
    targetRecord->setHeapValueByIndex(state, info.m_indexForIndexedStorage, setterInputData);
}

void* ArgumentsObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArgumentsObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_targetRecord));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_sourceFunctionObject));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_parameterMap));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_modifiedArguments));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArgumentsObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}


ArgumentsObject::ArgumentsObject(ExecutionState& state, ScriptFunctionObject* sourceFunctionObject, size_t argc, Value* argv, FunctionEnvironmentRecord* environmentRecordWillArgumentsObjectBeLocatedIn, bool isMapped)
    : Object(state, isMapped ? ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3 : ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 4, true)
    , m_targetRecord(environmentRecordWillArgumentsObjectBeLocatedIn->isFunctionEnvironmentRecordOnStack() ? nullptr : environmentRecordWillArgumentsObjectBeLocatedIn)
    , m_sourceFunctionObject(sourceFunctionObject)
    , m_argc(argc)
{
    // Let len be the number of elements in argumentsList.
    int len = argc;
    m_parameterMap.resizeWithUninitializedValues(len);

    if (isMapped) {
        m_structure = state.context()->defaultStructureForMappedArgumentsObject();

        // https://www.ecma-international.org/ecma-262/6.0/#sec-createmappedargumentsobject
        // Let numberOfParameters be the number of elements in parameterNames
        int numberOfParameters = m_sourceFunctionObject->codeBlock()->asInterpretedCodeBlock()->parameterNames().size();
        // Let index be 0.
        int index = 0;
        // Repeat while index < len ,
        while (index < len) {
            // Let val be argumentsList[index].
            Value val = argv[index];
            // Perform CreateDataProperty(obj, ToString(index), val).
            m_parameterMap[index].first = val;
            m_parameterMap[index].second = AtomicString();
            // Let index be index + 1
            index++;
        }

        // Perform DefinePropertyOrThrow(obj, "length", PropertyDescriptor{[[Value]]: len, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(len);

        // Let map be ObjectCreate(null). m_parameterMap already allocated
        // Let mappedNames be an empty List.
        std::vector<AtomicString> mappedNames;
        // Let index be numberOfParameters − 1.
        index = numberOfParameters - 1;
        // Repeat while index ≥ 0 ,
        while (index >= 0) {
            // Let name be parameterNames[index].
            AtomicString name = m_sourceFunctionObject->codeBlock()->asInterpretedCodeBlock()->parameterNames()[index];
            // If name is not an element of mappedNames, then
            if (std::find(mappedNames.begin(), mappedNames.end(), name) == mappedNames.end()) {
                // Add name as an element of the list mappedNames.
                mappedNames.push_back(name);
                // If index < len, then
                if (index < len) {
                    m_parameterMap[index].first = Value();
                    m_parameterMap[index].second = name;
                }
            }
            // Let index be index − 1
            index--;
        }

        // Perform DefinePropertyOrThrow(obj, @@iterator, PropertyDescriptor {[[Value]]:%ArrayProto_values%, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = state.context()->globalObject()->arrayPrototypeValues();
        // Perform DefinePropertyOrThrow(obj, "callee", PropertyDescriptor {[[Value]]: func, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = sourceFunctionObject;
    } else {
        m_structure = state.context()->defaultStructureForUnmappedArgumentsObject();

        // https://www.ecma-international.org/ecma-262/6.0/#sec-createunmappedargumentsobject
        // Perform DefinePropertyOrThrow(obj, "length", PropertyDescriptor{[[Value]]: len, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(len);
        // Let index be 0.
        int index = 0;
        // Repeat while index < len,
        while (index < len) {
            // Let val be argumentsList[index].
            Value val = argv[index];
            // Perform CreateDataProperty(obj, ToString(index), val).
            m_parameterMap[index].first = val;
            m_parameterMap[index].second = AtomicString();
            // Let index be index + 1
            index++;
        }

        // Perform DefinePropertyOrThrow(obj, @@iterator, PropertyDescriptor {[[Value]]:%ArrayProto_values%, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = state.context()->globalObject()->arrayPrototypeValues();
        // Perform DefinePropertyOrThrow(obj, "caller", PropertyDescriptor {[[Get]]: %ThrowTypeError%, [[Set]]: %ThrowTypeError%, [[Enumerable]]: false, [[Configurable]]: false}).
        auto thrower = state.context()->globalObject()->throwerGetterSetterData();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = Value(thrower);
        // Perform DefinePropertyOrThrow(obj, "callee", PropertyDescriptor {[[Get]]: %ThrowTypeError%, [[Set]]: %ThrowTypeError%, [[Enumerable]]: false, [[Configurable]]: false}).
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3] = Value(thrower);
    }
}

ObjectHasPropertyResult ArgumentsObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    uint64_t index = P.tryToUseAsIndex();
    if (LIKELY(!isModifiedArgument(index) && isMatchedArgument(index))) {
        return ObjectHasPropertyResult(ObjectGetResult(getIndexedPropertyValueQuickly(state, index), true, true, true));
    }

    return Object::hasProperty(state, P);
}

ObjectGetResult ArgumentsObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    uint64_t index = P.tryToUseAsIndex();
    if (LIKELY(!isModifiedArgument(index) && isMatchedArgument(index))) {
        return ObjectGetResult(getIndexedPropertyValueQuickly(state, index), true, true, true);
    }

    ObjectGetResult result = Object::getOwnProperty(state, P);

    if (isMatchedArgument(index)) {
        ASSERT(result.hasValue());
        return ObjectGetResult(getIndexedPropertyValueQuickly(state, index), result.isWritable(), result.isEnumerable(), result.isConfigurable());
    }
    return result;
}

bool ArgumentsObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    uint64_t index = P.tryToUseAsIndex();
    ObjectPropertyDescriptor newDesc(desc);

    if (isMatchedArgument(index)) {
        if (desc.isValuePresent()) {
            setIndexedPropertyValueQuickly(state, index, desc.value());
        }

        if (desc.isWritable() && desc.isConfigurable() && desc.isEnumerable() && !desc.isAccessorDescriptor() && !isModifiedArgument(index)) {
            return true;
        }

        if (!isModifiedArgument(index)) {
            // first, initialize property on Object's property slot
            // we should unset extensible limit
            setModifiedArgument(index);

            bool extensibleBefore = isExtensible(state);
            if (!extensibleBefore) {
                rareData()->m_isExtensible = true;
            }
            ObjectPropertyDescriptor initDesc(getIndexedPropertyValueQuickly(state, index), ObjectPropertyDescriptor::AllPresent);
            Object::defineOwnProperty(state, P, initDesc);
            if (!extensibleBefore) {
                rareData()->m_isExtensible = false;
            }
        }

        if ((desc.isWritablePresent() && !desc.isWritable()) || desc.isAccessorDescriptor()) {
            if (!desc.isAccessorDescriptor()) {
                newDesc.setValue(getIndexedPropertyValueQuickly(state, index));
            }
            // unmap arguments
            m_parameterMap[index].first = Value(Value::EmptyValue);
            setModifiedArgument(index);
        }
    }

    return Object::defineOwnProperty(state, P, newDesc);
}

bool ArgumentsObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    uint64_t index = P.tryToUseAsIndex();
    bool hasPropertyInObject = isModifiedArgument(index) || !isMatchedArgument(index);
    bool deleted = true;

    if (hasPropertyInObject) {
        deleted = Object::deleteOwnProperty(state, P);
    }

    if (deleted) {
        if (isMatchedArgument(index)) {
            m_parameterMap[index].first = Value(Value::EmptyValue);
        }
        setModifiedArgument(index);
    }

    return deleted;
}

// TODO
void ArgumentsObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    for (size_t i = 0; i < m_parameterMap.size(); i++) {
        if (isMatchedArgument(i) && !callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent), data)) {
            return;
        }
    }
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

ObjectGetResult ArgumentsObject::getIndexedProperty(ExecutionState& state, const Value& property)
{
    Value::ValueIndex index = property.tryToUseAsIndex(state);
    if (LIKELY(!isModifiedArgument(index) && isMatchedArgument(index))) {
        return ObjectGetResult(getIndexedPropertyValueQuickly(state, index), true, true, true);
    }

    ObjectGetResult result = Object::get(state, ObjectPropertyName(state, property));

    if (isMatchedArgument(index)) {
        ASSERT(result.hasValue());
        return ObjectGetResult(getIndexedPropertyValueQuickly(state, index), result.isWritable(), result.isEnumerable(), result.isConfigurable());
    }
    return result;
}

ObjectHasPropertyResult ArgumentsObject::hasIndexedProperty(ExecutionState& state, const Value& propertyName)
{
    Value::ValueIndex index = propertyName.tryToUseAsIndex(state);
    if (LIKELY(!isModifiedArgument(index) && isMatchedArgument(index))) {
        return ObjectHasPropertyResult(ObjectGetResult(getIndexedPropertyValueQuickly(state, index), true, true, true));
    }

    if (isMatchedArgument(index)) {
        ObjectGetResult result = Object::getOwnProperty(state, ObjectPropertyName(state, propertyName));
        ASSERT(result.hasValue());
        return ObjectHasPropertyResult(ObjectGetResult(getIndexedPropertyValueQuickly(state, index), result.isWritable(), result.isEnumerable(), result.isConfigurable()));
    }
    return hasProperty(state, ObjectPropertyName(state, propertyName));
}

bool ArgumentsObject::set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver)
{
    if (UNLIKELY(!receiver.isObject() || receiver.asObject() != this)) {
        return Object::set(state, propertyName, v, receiver);
    }

    Value::ValueIndex index = propertyName.tryToUseAsIndex();
    if (LIKELY(isMatchedArgument(index))) {
        setIndexedPropertyValueQuickly(state, index, v);
        return true;
    }

    return Object::set(state, propertyName, v, receiver);
}

bool ArgumentsObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    Value::ValueIndex index = property.tryToUseAsIndex(state);
    if (LIKELY(isMatchedArgument(index))) {
        setIndexedPropertyValueQuickly(state, index, value);
        return true;
    }

    return Object::set(state, ObjectPropertyName(state, property), value, this);
}

Value ArgumentsObject::getIndexedPropertyValueQuickly(ExecutionState& state, uint64_t index)
{
    ASSERT((index != Value::InvalidIndexValue) && (index < m_parameterMap.size()));
    ASSERT(!m_parameterMap[index].first.isEmpty());

    if (m_parameterMap[index].second.string()->length()) {
        // when there was parameter on source function,
        // source function must uses heap env record
        ASSERT(m_targetRecord != nullptr);
        return ArgumentsObjectNativeGetter(state, this, m_targetRecord, m_sourceFunctionObject->codeBlock()->asInterpretedCodeBlock(), m_parameterMap[index].second);
    } else {
        return m_parameterMap[index].first;
    }
}

void ArgumentsObject::setIndexedPropertyValueQuickly(ExecutionState& state, uint64_t index, const Value& value)
{
    ASSERT((index != Value::InvalidIndexValue) && (index < m_parameterMap.size()));
    ASSERT(!m_parameterMap[index].first.isEmpty());

    if (m_parameterMap[index].second.string()->length()) {
        // when there was parameter on source function,
        // source function must uses heap env record
        ASSERT(m_targetRecord != nullptr);
        ArgumentsObjectNativeSetter(state, this, value, m_targetRecord, m_sourceFunctionObject->codeBlock()->asInterpretedCodeBlock(), m_parameterMap[index].second);
    } else {
        return m_parameterMap[index].first = value;
    }
}

bool ArgumentsObject::isModifiedArgument(uint64_t index)
{
    if (m_modifiedArguments.size() == 0) {
        return false;
    }
    if (LIKELY(index != Value::InvalidIndexValue) && index < m_argc) {
        return m_modifiedArguments[index];
    }
    return false;
}

void ArgumentsObject::setModifiedArgument(uint64_t index)
{
    if (LIKELY(index != Value::InvalidIndexValue)) {
        if (m_modifiedArguments.size() == 0) {
            m_modifiedArguments.resize(m_argc, false);
        }
        if (index < m_argc) {
            m_modifiedArguments[index] = true;
        }
    }
}

bool ArgumentsObject::isMatchedArgument(uint64_t index)
{
    if (LIKELY(index != Value::InvalidIndexValue) && index < m_parameterMap.size()) {
        return !m_parameterMap[index].first.isEmpty();
    }
    return false;
}
}
