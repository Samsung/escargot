/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "EnvironmentRecord.h"
#include "runtime/Value.h"
#include "runtime/SmallValue.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "runtime/ModuleNamespaceObject.h"
#include "parser/CodeBlock.h"

namespace Escargot {

GlobalEnvironmentRecord::GlobalEnvironmentRecord(ExecutionState& state, InterpretedCodeBlock* codeBlock, GlobalObject* global, IdentifierRecordVector* globalDeclarativeRecord, SmallValueVector* globalDeclarativeStorage)
    : EnvironmentRecord()
    , m_globalCodeBlock(codeBlock)
    , m_globalObject(global)
    , m_globalDeclarativeRecord(globalDeclarativeRecord)
    , m_globalDeclarativeStorage(globalDeclarativeStorage)
{
    ASSERT(codeBlock == nullptr || codeBlock->parentCodeBlock() == nullptr);
}

void GlobalEnvironmentRecord::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable, bool isVarDeclaration, Optional<InterpretedCodeBlock*> relatedCodeBlock)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == name) {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, name.string(), false, String::emptyString, ErrorObject::Messages::DuplicatedIdentifier);
        }
    }

    if (isVarDeclaration) {
        ASSERT(isMutable);
        auto desc = m_globalObject->getOwnProperty(state, name);
        ObjectPropertyDescriptor::PresentAttribute attribute = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent);
        if (canDelete) {
            attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::ConfigurablePresent);
        } else {
            attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::NonConfigurablePresent);
        }
        if (!desc.hasValue()) {
            m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(Value(), attribute));
        } else {
            // we should update record of property
            ASSERT(relatedCodeBlock);
            auto c = relatedCodeBlock->firstChild();
            while (c) {
                if (c->isFunctionDeclaration() && c->lexicalBlockIndexFunctionLocatedIn() == 0) {
                    if (desc.isDataProperty()) {
                        m_globalObject->defineOwnProperty(state, name, ObjectPropertyDescriptor(desc.value(state, m_globalObject), attribute));
                    } else {
                        m_globalObject->defineOwnProperty(state, name, ObjectPropertyDescriptor(Value(), attribute));
                    }
                    break;
                }
                c = c->nextSibling();
            }
        }
    } else {
        IdentifierRecord r;
        ASSERT(!canDelete);
        r.m_canDelete = false;
        r.m_isMutable = isMutable;
        r.m_isVarDeclaration = false;
        r.m_name = name;

        m_globalDeclarativeRecord->push_back(r);
        m_globalDeclarativeStorage->push_back(SmallValue(SmallValue::EmptyValue));
    }
}

EnvironmentRecord::GetBindingValueResult GlobalEnvironmentRecord::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == name) {
            if (UNLIKELY(m_globalDeclarativeStorage->at(i).isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            return EnvironmentRecord::GetBindingValueResult(true, m_globalDeclarativeStorage->at(i));
        }
    }

    auto result = m_globalObject->get(state, name);
    if (result.hasValue())
        return EnvironmentRecord::GetBindingValueResult(true, result.value(state, m_globalObject));
    else
        return EnvironmentRecord::GetBindingValueResult(false, Value());
}

void GlobalEnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == name) {
            if (UNLIKELY(m_globalDeclarativeStorage->at(i).isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            m_globalDeclarativeStorage->at(i) = V;
            return;
        }
    }

    m_globalObject->setThrowsExceptionWhenStrictMode(state, name, V, m_globalObject);
}

void GlobalEnvironmentRecord::setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& V)
{
    ASSERT(slot.m_index != SIZE_MAX);
    if (slot.m_index != SIZE_MAX - 1) {
        // TDZ check
        if (UNLIKELY(m_globalDeclarativeStorage->at(slot.m_index).isEmpty())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
        }

        // Storing to const variable check
        if (UNLIKELY(!m_globalDeclarativeRecord->at(slot.m_index).m_isMutable)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
        }
        m_globalDeclarativeStorage->at(slot.m_index) = V;
        return;
    }

    m_globalObject->setThrowsExceptionWhenStrictMode(state, name, V, m_globalObject);
}

void GlobalEnvironmentRecord::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == name) {
            m_globalDeclarativeStorage->at(i) = V;
            return;
        }
    }

    auto findResult = m_globalObject->structure()->findProperty(name);
    if (findResult.first == SIZE_MAX) {
        m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
    } else {
        auto desc = findResult.second->m_descriptor;
        if (desc.isDataProperty()) {
            if (desc.isConfigurable() || !desc.isEnumerable()) {
                m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
            } else {
                m_globalObject->setThrowsException(state, name, V, m_globalObject);
            }
        } else {
            m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
        }
    }
}

bool GlobalEnvironmentRecord::deleteBinding(ExecutionState& state, const AtomicString& name)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == name) {
            return false;
        }
    }

    return m_globalObject->deleteOwnProperty(state, name);
}

EnvironmentRecord::BindingSlot GlobalEnvironmentRecord::hasBinding(ExecutionState& state, const AtomicString& atomicName)
{
    for (size_t i = 0; i < m_globalDeclarativeRecord->size(); i++) {
        if (m_globalDeclarativeRecord->at(i).m_name == atomicName) {
            return BindingSlot(this, i, true);
        }
    }

    auto result = m_globalObject->get(state, ObjectPropertyName(atomicName));
    if (result.hasValue()) {
        return BindingSlot(this, SIZE_MAX - 1, false);
    } else {
        return BindingSlot(this, SIZE_MAX, false);
    }
}

void DeclarativeEnvironmentRecordNotIndexed::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable, bool isVarDeclaration, Optional<InterpretedCodeBlock*> relatedCodeBlock)
{
    if (isVarDeclaration) {
        ASSERT(m_isVarDeclarationTarget);
    }

    auto hasBindingResult = hasBinding(state, name);
    if (UNLIKELY(hasBindingResult.m_index != SIZE_MAX)) {
        if (!m_recordVector[hasBindingResult.m_index].m_isVarDeclaration || !isVarDeclaration) {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, name.string(), false, String::emptyString, ErrorObject::Messages::DuplicatedIdentifier);
        }
        return;
    }
    IdentifierRecord record;
    record.m_name = name;
    record.m_canDelete = false;
    record.m_isMutable = isMutable;
    record.m_isVarDeclaration = isVarDeclaration;
    m_recordVector.pushBack(record);
    if (isVarDeclaration) {
        m_heapStorage.pushBack(Value());
    } else {
        m_heapStorage.pushBack(Value(Value::EmptyValue));
    }
}

EnvironmentRecord::GetBindingValueResult DeclarativeEnvironmentRecordNotIndexed::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            if (UNLIKELY(m_heapStorage[i].isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            return EnvironmentRecord::GetBindingValueResult(m_heapStorage[i]);
        }
    }
    return GetBindingValueResult();
}

void DeclarativeEnvironmentRecordNotIndexed::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            if (UNLIKELY(m_heapStorage[i].isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            if (UNLIKELY(!m_recordVector[i].m_isMutable)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
            }
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void DeclarativeEnvironmentRecordNotIndexed::setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v)
{
    // TDZ check
    if (UNLIKELY(m_heapStorage[slot.m_index].isEmpty())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
    }

    // Storing to const variable check
    if (UNLIKELY(!m_recordVector[slot.m_index].m_isMutable)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
    }
    m_heapStorage[slot.m_index] = v;
}

void DeclarativeEnvironmentRecordNotIndexed::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    for (size_t i = 0; i < m_recordVector.size(); i++) {
        if (m_recordVector[i].m_name == name) {
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

template <bool canBindThisValue, bool hasNewTarget>
FunctionEnvironmentRecordOnHeap<canBindThisValue, hasNewTarget>::FunctionEnvironmentRecordOnHeap(FunctionObject* function)
    : FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>(function)
    , m_heapStorage(function->codeBlock()->asInterpretedCodeBlock()->identifierOnHeapCount())
{
}

template <bool canBindThisValue, bool hasNewTarget>
void FunctionEnvironmentRecordOnHeap<canBindThisValue, hasNewTarget>::setMutableBindingByBindingSlot(ExecutionState& state, const EnvironmentRecord::BindingSlot& slot, const AtomicString& name, const Value& v)
{
    // Storing to const variable check only (TDZ check is already done by bytecode generation)
    const auto& recordInfo = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->codeBlock()->asInterpretedCodeBlock()->identifierInfos();
    if (UNLIKELY(!recordInfo[slot.m_index].m_isMutable)) {
        if (state.inStrictMode()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
        }
        return;
    }
    m_heapStorage[slot.m_index] = v;
}

template <bool canBindThisValue, bool hasNewTarget>
FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::FunctionEnvironmentRecordNotIndexed(FunctionObject* function)
    : FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>(function)
    , m_heapStorage()
{
    const InterpretedCodeBlock::IdentifierInfoVector& vec = function->codeBlock()->asInterpretedCodeBlock()->identifierInfos();
    size_t len = vec.size();
    m_recordVector.resizeWithUninitializedValues(len);
    m_heapStorage.resizeWithUninitializedValues(len);

    for (size_t i = 0; i < len; i++) {
        IdentifierRecord record;
        record.m_name = vec[i].m_name;
        record.m_canDelete = false;
        record.m_isVarDeclaration = true;
        record.m_isMutable = vec[i].m_isMutable;
        m_recordVector[i] = record;
        m_heapStorage[i] = Value();
    }
}

template <bool canBindThisValue, bool hasNewTarget>
void FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable, bool isVarDeclaration, Optional<InterpretedCodeBlock*> relatedCodeBlock)
{
    ASSERT(isVarDeclaration);

    size_t idx = hasBinding(state, name).m_index;
    if (idx == SIZE_MAX) {
        IdentifierRecord record;
        record.m_name = name;
        record.m_canDelete = canDelete;
        record.m_isMutable = isMutable;
        record.m_isVarDeclaration = true;
        m_recordVector.pushBack(record);
        m_heapStorage.pushBack(Value());
    } else {
        m_recordVector[idx].m_isMutable = isMutable;
    }
}

template <bool canBindThisValue, bool hasNewTarget>
EnvironmentRecord::GetBindingValueResult FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            return EnvironmentRecord::GetBindingValueResult(m_heapStorage[i]);
        }
    }
    return EnvironmentRecord::GetBindingValueResult();
}

template <bool canBindThisValue, bool hasNewTarget>
void FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            if (UNLIKELY(!m_recordVector[i].m_isMutable)) {
                if (state.inStrictMode())
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
                return;
            }
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

template <bool canBindThisValue, bool hasNewTarget>
void FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::setMutableBindingByBindingSlot(ExecutionState& state, const EnvironmentRecord::BindingSlot& slot, const AtomicString& name, const Value& v)
{
    // Storing to const variable check only (TDZ check is already done by bytecode generation)
    if (UNLIKELY(!m_recordVector[slot.m_index].m_isMutable)) {
        if (state.inStrictMode())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
        return;
    }
    m_heapStorage[slot.m_index] = v;
}

template <bool canBindThisValue, bool hasNewTarget>
void FunctionEnvironmentRecordNotIndexed<canBindThisValue, hasNewTarget>::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    for (size_t i = 0; i < m_recordVector.size(); i++) {
        if (m_recordVector[i].m_name == name) {
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

EnvironmentRecord::GetBindingValueResult FunctionEnvironmentRecordNotIndexedWithVirtualID::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    if (UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
        Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, name.string());
        if (!virtialIdResult.isEmpty())
            return EnvironmentRecord::GetBindingValueResult(virtialIdResult);
    }

    return FunctionEnvironmentRecordNotIndexed::getBindingValue(state, name);
}

template class FunctionEnvironmentRecordOnStack<false, false>;
template class FunctionEnvironmentRecordOnStack<true, true>;
template class FunctionEnvironmentRecordOnStack<false, true>;

template class FunctionEnvironmentRecordOnHeap<false, false>;
template class FunctionEnvironmentRecordOnHeap<true, true>;
template class FunctionEnvironmentRecordOnHeap<false, true>;

template class FunctionEnvironmentRecordNotIndexed<false, false>;
template class FunctionEnvironmentRecordNotIndexed<true, true>;
template class FunctionEnvironmentRecordNotIndexed<false, true>;

EnvironmentRecord::BindingSlot ModuleEnvironmentRecord::hasBinding(ExecutionState& state, const AtomicString& atomicName)
{
    for (size_t i = 0; i < m_moduleBindings.size(); i++) {
        if (m_moduleBindings[i].m_localName == atomicName) {
            return BindingSlot(this, i, !m_moduleBindings[i].m_isVarDeclaration);
        }
    }
    return BindingSlot(this, SIZE_MAX, false);
}

void ModuleEnvironmentRecord::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable, bool isVarDeclaration, Optional<InterpretedCodeBlock*> relatedCodeBlock)
{
    auto hasBindingResult = hasBinding(state, name);
    if (UNLIKELY(hasBindingResult.m_index != SIZE_MAX)) {
        if (!m_moduleBindings[hasBindingResult.m_index].m_isVarDeclaration || !isVarDeclaration) {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, name.string(), false, String::emptyString, ErrorObject::Messages::DuplicatedIdentifier);
        }
        return;
    }
    ModuleBindingRecord record;
    record.m_localName = name;
    record.m_isMutable = isMutable;
    record.m_isVarDeclaration = isVarDeclaration;
    record.m_targetRecord = nullptr;
    record.m_value = isVarDeclaration ? Value() : Value(Value::EmptyValue);
    m_moduleBindings.pushBack(record);
}

void ModuleEnvironmentRecord::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    for (size_t i = 0; i < m_moduleBindings.size(); i++) {
        if (m_moduleBindings[i].m_localName == name) {
            ASSERT(m_moduleBindings[i].m_targetRecord == nullptr);
            m_moduleBindings[i].m_value = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void ModuleEnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_moduleBindings.size();
    for (size_t i = 0; i < len; i++) {
        if (m_moduleBindings[i].m_localName == name) {
            writeCheck(state, i);
            m_moduleBindings[i].m_value = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void ModuleEnvironmentRecord::setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v)
{
    ASSERT(m_moduleBindings[slot.m_index].m_targetRecord == nullptr);
    writeCheck(state, slot.m_index);
    m_moduleBindings[slot.m_index].m_value = v;
}

void ModuleEnvironmentRecord::setMutableBindingByIndex(ExecutionState& state, const size_t i, const Value& v)
{
    ASSERT(m_moduleBindings[i].m_targetRecord == nullptr);
    writeCheck(state, i);
    m_moduleBindings[i].m_value = v;
}

void ModuleEnvironmentRecord::initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v)
{
    ASSERT(m_moduleBindings[idx].m_targetRecord == nullptr);
    m_moduleBindings[idx].m_value = v;
}

EnvironmentRecord::GetBindingValueResult ModuleEnvironmentRecord::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_moduleBindings.size();
    for (size_t i = 0; i < len; i++) {
        if (m_moduleBindings[i].m_localName == name) {
            if (m_moduleBindings[i].m_targetRecord) {
                return m_moduleBindings[i].m_targetRecord->getBindingValue(state, m_moduleBindings[i].m_targetBindingName);
            } else {
                readCheck(state, i);
                return GetBindingValueResult(m_moduleBindings[i].m_value);
            }
        }
    }
    return GetBindingValueResult();
}

Value ModuleEnvironmentRecord::getBindingValue(ExecutionState& state, const size_t i)
{
    if (m_moduleBindings[i].m_targetRecord) {
        return m_moduleBindings[i].m_targetRecord->getBindingValue(state, m_moduleBindings[i].m_targetBindingName).m_value;
    } else {
        readCheck(state, i);
        return m_moduleBindings[i].m_value;
    }
}

void ModuleEnvironmentRecord::setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v)
{
    writeCheck(state, idx);
    ASSERT(m_moduleBindings[idx].m_targetRecord == nullptr);
    m_moduleBindings[idx].m_value = v;
}

Value ModuleEnvironmentRecord::getHeapValueByIndex(ExecutionState& state, const size_t i)
{
    if (m_moduleBindings[i].m_targetRecord) {
        return m_moduleBindings[i].m_targetRecord->getBindingValue(state, m_moduleBindings[i].m_targetBindingName).m_value;
    } else {
        readCheck(state, i);
        return m_moduleBindings[i].m_value;
    }
}

void ModuleEnvironmentRecord::createImportBinding(ExecutionState& state, AtomicString localName, ModuleEnvironmentRecord* targetRecord, AtomicString targetBindingName)
{
    // Let envRec be the module Environment Record for which the method was invoked.
    // Assert: envRec does not already have a binding for N.
    // Assert: M is a Module Record.
    // Assert: When M.[[Environment]] is instantiated it will have a direct binding for N2.
    // Create an immutable indirect binding in envRec for N that references M and N2 as its target binding and record that the binding is initialized.
    // Return NormalCompletion(empty).
    size_t len = m_moduleBindings.size();
    for (size_t i = 0; i < len; i++) {
        if (m_moduleBindings[i].m_localName == localName) {
            ASSERT(m_moduleBindings[i].m_targetRecord == nullptr);
            m_moduleBindings[i].m_targetRecord = targetRecord;
            m_moduleBindings[i].m_targetBindingName = targetBindingName;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

ModuleNamespaceObject* ModuleEnvironmentRecord::namespaceObject(ExecutionState& state)
{
    if (!m_namespaceObject) {
        m_namespaceObject = new ModuleNamespaceObject(state, this);
    }

    return m_namespaceObject;
}
}
