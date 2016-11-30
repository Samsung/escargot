#include "Escargot.h"
#include "FunctionObject.h"
#include "ExecutionContext.h"
#include "Context.h"
#include "parser/ScriptParser.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, bool isConstructor)
    : Object(state)
    , m_isConstructor(isConstructor)
    , m_codeBlock(codeBlock)
{
    if (m_isConstructor) {
        m_structure = state.context()->defaultStructureForFunctionObject();
        m_values.pushBack(Value(new Object(state)));
        m_values.pushBack(Value(codeBlock->functionName().string()));
        m_values.pushBack(Value(codeBlock->functionParameters().size()));
    } else {
        m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
        m_values.pushBack(Value(codeBlock->functionName().string()));
        m_values.pushBack(Value(codeBlock->functionParameters().size()));
    }
}

Value FunctionObject::getFunctionPrototypeSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, state.context()->staticStrings().prototype);
}

Value FunctionObject::call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv)
{
    Context* ctx = state.context();

    // prepare ByteCodeBlock if needed
    if (!m_codeBlock->byteCodeBlock()) {
        Node* ast;
        if (m_codeBlock->cachedASTNode()) {
            ast = m_codeBlock->cachedASTNode();
        } else {
            ast = state.context()->scriptParser().parseFunction(m_codeBlock);
        }

        ByteCodeGenerator g;
        g.generateByteCode(ctx, m_codeBlock, ast);
    }

    // prepare env, ec
    bool isStrict = m_codeBlock->isStrict();
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* env;
    ExecutionContext* ec;
    size_t stackStorageSize = m_codeBlock->identifierOnStackCount();
    if (m_codeBlock->canAllocateEnvironmentOnStack()) {
        // no capture, very simple case
        record = new(alloca(sizeof(LexicalEnvironment))) FunctionEnvironmentRecordOnStack(state, receiver, this);
        env = new(alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());
        ec = new(alloca(sizeof(ExecutionContext))) ExecutionContext(ctx, env, isStrict);
    } else {
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            record = new FunctionEnvironmentRecordOnHeap(state, receiver, this);
        } else {
            record = new FunctionEnvironmentRecordNotIndexed(state, receiver, this);
        }
        env = new LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());
        ec = new ExecutionContext(ctx, env, isStrict);
    }

    ec->giveStackStorage(ALLOCA(stackStorageSize * sizeof(Value), Value, state));
    Value* stackStorage = ec->stackStorage();
    for (size_t i = 0; i < stackStorageSize; i ++) {
        stackStorage[i] = Value();
    }
    Value resultValue;
    ExecutionState newState(ctx, ec, &resultValue);

    // TODO binding function name

    // prepare parameters
    const CodeBlock::FunctionParametersInfoVector& info = m_codeBlock->parametersInfomation();
    size_t parameterCopySize = std::min(argc, info.size());

    if (UNLIKELY(m_codeBlock->needsComplexParameterCopy())) {
        for (size_t i = 0; i < parameterCopySize; i ++) {
            if (info[i].m_isHeapAllocated) {
                record->setHeapValueByIndex(info[i].m_index, argv[i]);
            } else {
                stackStorage[info[i].m_index] = argv[i];
            }
        }
    } else {
        for (size_t i = 0; i < parameterCopySize; i ++) {
            stackStorage[i] = argv[i];
        }
    }

    // run function
    ByteCodeIntrepreter::interpret(newState, m_codeBlock);

    return resultValue;
}

}
