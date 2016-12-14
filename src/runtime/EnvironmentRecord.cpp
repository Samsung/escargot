#include "Escargot.h"
#include "EnvironmentRecord.h"
#include "runtime/Value.h"
#include "runtime/SmallValue.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "parser/CodeBlock.h"

namespace Escargot {

void EnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    GlobalObject* o = state.context()->globalObject();
    o->setThrowsExceptionWhenStrictMode(state, name, V, o);
}

GlobalEnvironmentRecord::GlobalEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, GlobalObject* global)
    : EnvironmentRecord(state, codeBlock)
    , m_globalCodeBlock(codeBlock)
{
    ASSERT(codeBlock->parentCodeBlock() == nullptr);
    m_globalObject = global;

    const CodeBlock::IdentifierInfoVector& vec = codeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        ASSERT(vec[i].m_needToAllocateOnStack == false);
        createMutableBinding(state, vec[i].m_name, false);
    }
}

Value GlobalEnvironmentRecord::getThisBinding()
{
    return m_globalObject;
}

void GlobalEnvironmentRecord::createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete)
{
    auto desc = m_globalObject->getOwnProperty(state, name);
    if (!desc.hasValue()) {
        ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute attribute = (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent);
        if (canDelete)
            attribute = (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(attribute | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent);
        m_globalObject->defineOwnPropertyThrowsExceptionWhenStrictMode(state, name, ObjectPropertyDescriptorForDefineOwnProperty(Value(), attribute));
    }
}

EnvironmentRecord::GetBindingValueResult GlobalEnvironmentRecord::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    auto result = m_globalObject->get(state, name, m_globalObject);
    if (result.hasValue())
        return EnvironmentRecord::GetBindingValueResult(true, result.value());
    else
        return EnvironmentRecord::GetBindingValueResult(false, Value());
}

void GlobalEnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    m_globalObject->setThrowsExceptionWhenStrictMode(state, name, V, m_globalObject);
}

void DeclarativeEnvironmentRecordNotIndexded::createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete)
{
    ASSERT(canDelete == false);
    ASSERT(hasBinding(state, name).m_index == SIZE_MAX);
    IdentifierRecord record;
    record.m_name = name;
    record.m_value = Value();
    m_vector.pushBack(record);
}

EnvironmentRecord::GetBindingValueResult DeclarativeEnvironmentRecordNotIndexded::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_vector[i].m_name == name) {
            return EnvironmentRecord::GetBindingValueResult(m_vector[i].m_value);
        }
    }
    return GetBindingValueResult();
}

void DeclarativeEnvironmentRecordNotIndexded::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_vector[i].m_name == name) {
            m_vector[i].m_value = V;
            return;
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
}


void FunctionEnvironmentRecordNotIndexed::createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete)
{
    ASSERT(canDelete == false);
    ASSERT(hasBinding(state, name).m_index == SIZE_MAX);
    IdentifierRecord record;
    record.m_name = name;
    record.m_value = Value();
    m_vector.pushBack(record);
}
EnvironmentRecord::GetBindingValueResult FunctionEnvironmentRecordNotIndexed::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_vector[i].m_name == name) {
            return EnvironmentRecord::GetBindingValueResult(m_vector[i].m_value);
        }
    }
    return GetBindingValueResult();
}

void FunctionEnvironmentRecordNotIndexed::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i++) {
        if (m_vector[i].m_name == name) {
            m_vector[i].m_value = V;
            return;
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
}
}
