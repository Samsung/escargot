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
    if (UNLIKELY(!state.context()->globalObject()->set(state, name, V))) {
        if (state.inStrictMode()) {
            // TODO strict mode
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}

GlobalEnvironmentRecord::GlobalEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, GlobalObject* global)
    : EnvironmentRecord(state, codeBlock)
{
    ASSERT(codeBlock->parentCodeBlock() == nullptr);
    m_globalObject = global;

    const CodeBlock::IdentifierInfoVector& vec = codeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i ++) {
        ASSERT(vec[i].m_needToAllocateOnStack == false);
        createMutableBinding(state, vec[i].m_name, false);
    }
}

void GlobalEnvironmentRecord::createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete)
{
    if (m_globalObject->findOwnProperty(state, name) == SIZE_MAX) {
        ObjectPropertyDescriptor::PresentAttribute attribute = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent);
        if (canDelete)
            attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::ConfigurablePresent);
        m_globalObject->defineOwnProperty(state, name, Object::ObjectPropertyDescriptorForDefineOwnProperty(Value(), attribute));
    }
}

EnvironmentRecord::GetBindingValueResult GlobalEnvironmentRecord::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    auto result = m_globalObject->get(state, name);
    return EnvironmentRecord::GetBindingValueResult(result.m_hasValue, result.m_value);
}

void GlobalEnvironmentRecord::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    // TODO strict mode
    m_globalObject->set(state, name, V);
}

void FunctionEnvironmentRecordNotIndexed::createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete)
{
    ASSERT(canDelete == false);
    IdentifierRecord record;
    record.m_name = name;
    record.m_value = Value();
    m_vector.pushBack(record);
}
EnvironmentRecord::GetBindingValueResult FunctionEnvironmentRecordNotIndexed::getBindingValue(ExecutionState& state, const AtomicString& name)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i ++) {
        if (m_vector[i].m_name == name) {
            return EnvironmentRecord::GetBindingValueResult(m_vector[i].m_value);
        }
    }
    return GetBindingValueResult();
}

void FunctionEnvironmentRecordNotIndexed::setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
{
    size_t len = m_vector.size();
    for (size_t i = 0; i < len; i ++) {
        if (m_vector[i].m_name == name) {
            m_vector[i].m_value = V;
            return;
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
}


}
