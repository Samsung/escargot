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
#include "EnvironmentRecord.h"
#include "runtime/Value.h"
#include "runtime/SmallValue.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "parser/CodeBlock.h"

namespace Escargot {

GlobalEnvironmentRecord::GlobalEnvironmentRecord(ExecutionState& state, InterpretedCodeBlock* codeBlock, GlobalObject* global, bool isEvalMode, bool createBinding)
    : EnvironmentRecord()
    , m_globalCodeBlock(codeBlock)
    , m_globalObject(global)
{
    ASSERT(codeBlock == nullptr || codeBlock->parentCodeBlock() == nullptr);

    if (createBinding) {
        const InterpretedCodeBlock::IdentifierInfoVector& vec = codeBlock->identifierInfos();
        size_t len = vec.size();
        for (size_t i = 0; i < len; i++) {
            // https://www.ecma-international.org/ecma-262/5.1/#sec-10.5
            // Step 2. If code is eval code, then let configurableBindings be true.
            this->createBinding(state, vec[i].m_name, isEvalMode, vec[i].m_isMutable);
        }
    }
}

void GlobalEnvironmentRecord::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable)
{
    ASSERT(isMutable);
    auto desc = m_globalObject->getOwnProperty(state, name);
    ObjectPropertyDescriptor::PresentAttribute attribute = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent);
    if (canDelete)
        attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::ConfigurablePresent);
    if (!desc.hasValue()) {
        m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(Value(), attribute));
    }
}

EnvironmentRecord::GetBindingValueResult GlobalEnvironmentRecord::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    auto result = m_globalObject->get(state, name);
    if (result.hasValue())
        return EnvironmentRecord::GetBindingValueResult(true, result.value(state, m_globalObject));
    else
        return EnvironmentRecord::GetBindingValueResult(false, Value());
}

void GlobalEnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    m_globalObject->setThrowsExceptionWhenStrictMode(state, name, V, m_globalObject);
}

void GlobalEnvironmentRecord::setMutableBindingByIndex(ExecutionState& state, const size_t idx, const AtomicString& name, const Value& V)
{
    m_globalObject->setThrowsExceptionWhenStrictMode(state, name, V, m_globalObject);
}

void GlobalEnvironmentRecord::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t t = m_globalObject->structure()->findProperty(name);
    if (t == SIZE_MAX) {
        m_globalObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
    } else {
        auto desc = m_globalObject->structure()->readProperty(state, t).m_descriptor;
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
    return m_globalObject->deleteOwnProperty(state, name);
}

EnvironmentRecord::BindingSlot GlobalEnvironmentRecord::hasBinding(ExecutionState& state, const AtomicString& atomicName)
{
    auto result = m_globalObject->get(state, ObjectPropertyName(atomicName));
    if (result.hasValue()) {
        return BindingSlot(this, SIZE_MAX - 1);
    } else {
        return BindingSlot(this, SIZE_MAX);
    }
}

FunctionEnvironmentRecordOnHeap::FunctionEnvironmentRecordOnHeap(FunctionObject* function, size_t argc, Value* argv)
    : FunctionEnvironmentRecord(function)
    , m_argc(argc)
    , m_argv(argv)
    , m_heapStorage(function->codeBlock()->asInterpretedCodeBlock()->identifierOnHeapCount())
{
}

FunctionEnvironmentRecordNotIndexed::FunctionEnvironmentRecordNotIndexed(FunctionObject* function, size_t argc, Value* argv)
    : FunctionEnvironmentRecord(function)
    , m_argc(argc)
    , m_argv(argv)
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
        record.m_isMutable = vec[i].m_isMutable;
        m_recordVector[i] = record;
        m_heapStorage[i] = Value();
    }
}

void DeclarativeEnvironmentRecordNotIndexed::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable)
{
    ASSERT(!canDelete);
    ASSERT(hasBinding(state, name).m_index == SIZE_MAX);
    IdentifierRecord record;
    record.m_name = name;
    record.m_canDelete = false;
    record.m_isMutable = isMutable;
    m_recordVector.pushBack(record);
    m_heapStorage.pushBack(Value());
}

EnvironmentRecord::GetBindingValueResult DeclarativeEnvironmentRecordNotIndexed::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
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
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void DeclarativeEnvironmentRecordNotIndexed::setMutableBindingByIndex(ExecutionState& state, const size_t idx, const AtomicString& name, const Value& v)
{
    m_heapStorage[idx] = v;
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

void FunctionEnvironmentRecordNotIndexed::createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable)
{
    size_t idx = hasBinding(state, name).m_index;
    if (idx == SIZE_MAX) {
        IdentifierRecord record;
        record.m_name = name;
        record.m_canDelete = canDelete;
        record.m_isMutable = isMutable;
        m_recordVector.pushBack(record);
        m_heapStorage.pushBack(Value());
    } else {
        m_recordVector[idx].m_isMutable = isMutable;
    }
}
EnvironmentRecord::GetBindingValueResult FunctionEnvironmentRecordNotIndexed::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            return EnvironmentRecord::GetBindingValueResult(m_heapStorage[i]);
        }
    }
    return GetBindingValueResult();
}

void FunctionEnvironmentRecordNotIndexed::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_recordVector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_recordVector[i].m_name == name) {
            if (UNLIKELY(!m_recordVector[i].m_isMutable)) {
                if (state.inStrictMode())
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable);
                return;
            }
            m_heapStorage[i] = V;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void FunctionEnvironmentRecordNotIndexed::setMutableBindingByIndex(ExecutionState& state, const size_t idx, const AtomicString& name, const Value& v)
{
    if (UNLIKELY(!m_recordVector[idx].m_isMutable)) {
        if (state.inStrictMode())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable);
        return;
    }
    m_heapStorage[idx] = v;
}

void FunctionEnvironmentRecordNotIndexed::initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
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
}
