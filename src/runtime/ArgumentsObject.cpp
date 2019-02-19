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

namespace Escargot {

size_t g_argumentsObjectTag;

static Value ArgumentsObjectNativeGetter(ExecutionState& state, Object* self, FunctionEnvironmentRecord* targetRecord, InterpretedCodeBlock* codeBlock, AtomicString name)
{
    InterpretedCodeBlock::IdentifierInfo info = codeBlock->identifierInfos()[codeBlock->findName(name)];
    ASSERT(!info.m_needToAllocateOnStack);
    if (info.m_indexForIndexedStorage == SIZE_MAX) {
        return targetRecord->getBindingValue(state, name).m_value;
    }
    return targetRecord->getHeapValueByIndex(info.m_indexForIndexedStorage);
}

static void ArgumentsObjectNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData, FunctionEnvironmentRecord* targetRecord, InterpretedCodeBlock* codeBlock, AtomicString name)
{
    InterpretedCodeBlock::IdentifierInfo info = codeBlock->identifierInfos()[codeBlock->findName(name)];
    ASSERT(!info.m_needToAllocateOnStack);
    if (info.m_indexForIndexedStorage == SIZE_MAX) {
        targetRecord->setMutableBinding(state, name, setterInputData);
        return;
    }
    targetRecord->setHeapValueByIndex(info.m_indexForIndexedStorage, setterInputData);
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
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_codeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_argumentPropertyInfo));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArgumentsObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ArgumentsObject::ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3, true)
{
    g_argumentsObjectTag = *((size_t*)this);

    InterpretedCodeBlock* blk = record->functionObject()->codeBlock()->asInterpretedCodeBlock();
    bool isStrict = blk->isStrict();

    if (isStrict) {
        m_structure = state.context()->defaultStructureForArgumentsObjectInStrictMode();
    } else {
        m_structure = state.context()->defaultStructureForArgumentsObject();
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-10.6
    // Let len be the number of elements in args.
    size_t len = record->argc();
    // Let obj be the result of creating a new ECMAScript object.
    // Set all the internal methods of obj as specified in 8.12.
    // Set the [[Class]] internal property of obj to "Arguments".
    // Let Object be the standard built-in Object constructor (15.2.2).
    // Set the [[Prototype]] internal property of obj to the standard built-in Object prototype object (15.2.4).
    // Object* obj = this;

    // Call the [[DefineOwnProperty]] internal method on obj passing "length", the Property Descriptor {[[Value]]: len, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}, and false as arguments.
    // obj->defineOwnProperty(state, state.context()->staticStrings().length, ObjectPropertyDescriptor(Value(len), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value(len);

    // Let map be the result of creating a new object as if by the expression new Object() where Object is the standard built-in constructor with that name
    // Let mappedNames be an empty List.
    std::vector<AtomicString> mappedNames;
    // Let indx = len - 1.
    int64_t indx = ((int64_t)len - 1);
    // Repeat while indx >= 0,

    m_argumentPropertyInfo.resizeWithUninitializedValues(len);
    m_targetRecord = record;
    m_codeBlock = blk;

    while (indx >= 0) {
        // Let val be the element of args at 0-origined list position indx.
        Value val = record->argv()[indx];
        // Call the [[DefineOwnProperty]] internal method on obj passing ToString(indx), the property descriptor {[[Value]]: val, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false as arguments.
        m_argumentPropertyInfo[indx].first = val;
        m_argumentPropertyInfo[indx].second = AtomicString();

        // If indx is less than the number of elements in names, then
        if ((size_t)indx < blk->parametersInfomation().size()) {
            // Let name be the element of names at 0-origined list position indx.
            AtomicString name = blk->parametersInfomation()[indx].m_name;
            // If strict is false and name is not an element of mappedNames, then
            if (!isStrict && std::find(mappedNames.begin(), mappedNames.end(), name) == mappedNames.end()) {
                // Add name as an element of the list mappedNames.
                mappedNames.push_back(name);
                // Let g be the result of calling the MakeArgGetter abstract operation with arguments name and env.
                // Let p be the result of calling the MakeArgSetter abstract operation with arguments name and env.
                // Set the [[ParameterMap]] internal property of obj to map.
                // Set the [[Get]], [[GetOwnProperty]], [[DefineOwnProperty]], and [[Delete]] internal methods of obj to the definitions provided below.
                // Call the [[DefineOwnProperty]] internal method of map passing ToString(indx), the Property Descriptor {[[Set]]: p, [[Get]]: g, [[Configurable]]: true}, and false as arguments.
                m_argumentPropertyInfo[indx].first = Value();
                m_argumentPropertyInfo[indx].second = name;
            }
        }
        // Let indx = indx - 1
        indx--;
    }

    // If strict is false, then
    if (!isStrict) {
        // Call the [[DefineOwnProperty]] internal method on obj passing "callee", the property descriptor {[[Value]]: func, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}, and false as arguments.
        // obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().callee), ObjectPropertyDescriptor(record->functionObject(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = Value(record->functionObject());

        Value caller;
        ExecutionContext* pec = ec->parent();
        while (pec) {
            if (pec->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                caller = pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
                break;
            }
            pec = pec->parent();
        }
        // obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), ObjectPropertyDescriptor(caller, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = Value(caller);
    } else {
        // Else, strict is true so
        // Let thrower be the [[ThrowTypeError]] function Object (13.2.3).
        auto thrower = state.context()->globalObject()->throwerGetterSetterData();
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "callee", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        // obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().callee), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = Value(thrower);
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "caller", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        // obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = Value(thrower);
    }
}

ObjectGetResult ArgumentsObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx = P.tryToUseAsIndex();
    if (LIKELY(idx != Value::InvalidIndexValue) && idx < m_argumentPropertyInfo.size()) {
        Value val = m_argumentPropertyInfo[idx].first;
        if (!val.isEmpty()) {
            if (m_argumentPropertyInfo[idx].second.string()->length()) {
                return ObjectGetResult(ArgumentsObjectNativeGetter(state, this, m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second), true, true, true);
            } else {
                return ObjectGetResult(val, true, true, true);
            }
        }
    }
    return Object::getOwnProperty(state, P);
}

bool ArgumentsObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx = P.tryToUseAsIndex();
    if (LIKELY(idx != Value::InvalidIndexValue) && idx < m_argumentPropertyInfo.size()) {
        Value val = m_argumentPropertyInfo[idx].first;
        if (!val.isEmpty()) {
            if (desc.isDataWritableEnumerableConfigurable() || desc.isValuePresentAlone()) {
                if (m_argumentPropertyInfo[idx].second.string()->length()) {
                    ArgumentsObjectNativeSetter(state, this, desc.value(), m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second);
                    return true;
                } else {
                    m_argumentPropertyInfo[idx].first = desc.value();
                    return true;
                }
            } else {
                if (m_argumentPropertyInfo[idx].second.string()->length() && desc.isDataDescriptor()) {
                    if (desc.isValuePresent())
                        ArgumentsObjectNativeSetter(state, this, desc.value(), m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second);
                }
                ObjectPropertyDescriptor descCpy(desc);
                if (!desc.isAccessorDescriptor() && !desc.isValuePresent()) {
                    if (m_argumentPropertyInfo[idx].second.string()->length())
                        descCpy.setValue(ArgumentsObjectNativeGetter(state, this, m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second));
                    else
                        descCpy.setValue(m_argumentPropertyInfo[idx].first);
                }

                m_argumentPropertyInfo[idx].first = Value(Value::EmptyValue);

                ObjectPropertyDescriptor newDesc(descCpy);
                newDesc.setWritable(true);
                newDesc.setConfigurable(true);
                newDesc.setEnumerable(true);

                if (desc.isWritablePresent()) {
                    newDesc.setWritable(desc.isWritable());
                }
                if (desc.isEnumerablePresent()) {
                    newDesc.setEnumerable(desc.isEnumerable());
                }
                if (desc.isConfigurablePresent()) {
                    newDesc.setConfigurable(desc.isConfigurable());
                }

                bool extensibleBefore = isExtensible(state);
                if (!extensibleBefore) {
                    rareData()->m_isExtensible = true;
                }
                bool ret = Object::defineOwnProperty(state, P, newDesc);
                if (!extensibleBefore) {
                    preventExtensions(state);
                }
                return ret;
            }
        }
    }
    return Object::defineOwnProperty(state, P, desc);
}

bool ArgumentsObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx = P.tryToUseAsIndex();
    if (LIKELY(idx != Value::InvalidIndexValue) && idx < m_argumentPropertyInfo.size()) {
        Value val = m_argumentPropertyInfo[idx].first;
        if (!val.isEmpty()) {
            m_argumentPropertyInfo[idx].first = Value(Value::EmptyValue);
            return true;
        }
    }
    return Object::deleteOwnProperty(state, P);
}

void ArgumentsObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    for (size_t i = 0; i < m_argumentPropertyInfo.size(); i++) {
        Value v = m_argumentPropertyInfo[i].first;
        if (!v.isEmpty()) {
            if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent), data)) {
                return;
            }
        }
    }
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

ObjectGetResult ArgumentsObject::getIndexedProperty(ExecutionState& state, const Value& property)
{
    Value::ValueIndex idx = property.tryToUseAsIndex(state);
    if (LIKELY(idx != Value::InvalidIndexValue) && idx < m_argumentPropertyInfo.size()) {
        Value val = m_argumentPropertyInfo[idx].first;
        if (!val.isEmpty()) {
            if (m_argumentPropertyInfo[idx].second.string()->length()) {
                return ObjectGetResult(ArgumentsObjectNativeGetter(state, this, m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second), true, true, true);
            } else {
                return ObjectGetResult(val, true, true, true);
            }
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

bool ArgumentsObject::setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
{
    Value::ValueIndex idx = property.tryToUseAsIndex(state);
    if (LIKELY(idx != Value::InvalidIndexValue) && idx < m_argumentPropertyInfo.size()) {
        Value val = m_argumentPropertyInfo[idx].first;
        if (!val.isEmpty()) {
            if (m_argumentPropertyInfo[idx].second.string()->length()) {
                ArgumentsObjectNativeSetter(state, this, value, m_targetRecord, m_codeBlock, m_argumentPropertyInfo[idx].second);
                return true;
            } else {
                m_argumentPropertyInfo[idx].first = value;
                return true;
            }
        }
    }
    return set(state, ObjectPropertyName(state, property), value, this);
}
}
