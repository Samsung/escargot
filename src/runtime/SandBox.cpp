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
    // collect stack trace
    ExecutionContext* ec = state.executionContext();
    while (ec) {
        if (ec->m_programCounter) {
            if (ec->m_lexicalEnvironment) {
                if (ec->m_lexicalEnvironment->record()->isDeclarativeEnvironmentRecord()) {
                    if (ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                        FunctionObject* fn = ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
                        CodeBlock* cb = fn->codeBlock();
                        ByteCodeBlock* b = cb->byteCodeBlock();
                        ByteCode* code = b->peekCode<ByteCode>((size_t)ec->programCounter() - (size_t)b->m_code.data());
                        NodeLOC loc = b->computeNodeLOCFromByteCode(code, cb);
                        if (m_stackTraceData.size() == 0 || m_stackTraceData.back().first != ec) {
                            // TODO
                            // consider native function
                            StackTraceData data;
                            data.loc = loc;
                            data.fileName = cb->script()->fileName();
                            m_stackTraceData.pushBack(std::make_pair(ec, data));
                        }
                    }
                } else if (ec->m_lexicalEnvironment->record()->isGlobalEnvironmentRecord()) {
                    CodeBlock* cb = ec->m_lexicalEnvironment->record()->asGlobalEnvironmentRecord()->globalCodeBlock();
                    ByteCodeBlock* b = cb->byteCodeBlock();
                    ByteCode* code = b->peekCode<ByteCode>((size_t)ec->programCounter() - (size_t)b->m_code.data());
                    NodeLOC loc = b->computeNodeLOCFromByteCode(code, cb);
                    StackTraceData data;
                    data.loc = loc;
                    data.fileName = cb->script()->fileName();
                    m_stackTraceData.pushBack(std::make_pair(ec, data));
                }
            }
        }
        ec = ec->parent();
    }
    throw exception;
}
}
