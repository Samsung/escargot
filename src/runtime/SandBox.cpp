#include "Escargot.h"
#include "SandBox.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "parser/Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

SandBox::SandBoxResult SandBox::run(const std::function<Value()>& scriptRunner)
{
    SandBox::SandBoxResult result;
    try {
        result.result = scriptRunner();
    } catch (const Value& err) {
        result.error = err;
        for (size_t i = 0; i < m_stackTraceData.size(); i++) {
            result.stackTraceData.pushBack(m_stackTraceData[i].second);
        }
    }
    return result;
}

void SandBox::throwException(ExecutionState& state, Value exception)
{
    throw exception;
}
}
