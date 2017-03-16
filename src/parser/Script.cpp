/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "parser/ast/Node.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/SandBox.h"
#include "util/Util.h"
#include "parser/ast/AST.h"

namespace Escargot {

Value Script::execute(ExecutionState& state, bool isEvalMode, bool needNewEnv, bool isOnGlobal)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    m_topCodeBlock->m_cachedASTNode = nullptr;
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, ((ProgramNode*)programNode)->scopeContext(), isEvalMode, isOnGlobal);
    delete programNode;

    LexicalEnvironment* env;
    ExecutionContext* prevEc;
    {
        CodeBlock* globalCodeBlock = (needNewEnv) ? nullptr : m_topCodeBlock;
        LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, globalCodeBlock, state.context()->globalObject(), isEvalMode), nullptr);
        if (UNLIKELY(needNewEnv)) {
            // NOTE: ES5 10.4.2.1 eval in strict mode
            prevEc = new ExecutionContext(state.context(), state.executionContext(), globalEnvironment, m_topCodeBlock->isStrict());
            EnvironmentRecord* record = new DeclarativeEnvironmentRecordNotIndexed(state, m_topCodeBlock->identifierInfos());
            env = new LexicalEnvironment(record, globalEnvironment);
        } else {
            env = globalEnvironment;
            prevEc = nullptr;
        }
    }

    ExecutionContext ec(state.context(), prevEc, env, m_topCodeBlock->isStrict());
    Value thisValue(state.context()->globalObject());
    ExecutionState newState(&state, &ec);

    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = (Value*)alloca((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + 1 + literalStorageSize) * sizeof(Value));
    registerFile[0] = Value();
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    stackStorage[0] = thisValue;
    Value* literalStorage = stackStorage + 1;
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    size_t unused;
    Value resultValue = ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile, &unused);
    clearStack<512>();

    return resultValue;
}

Script::ScriptSandboxExecuteResult Script::sandboxExecute(Context* ctx)
{
    ScriptSandboxExecuteResult result;
    SandBox sb(ctx);
    ExecutionState stateForInit(ctx);

    auto sandBoxResult = sb.run([&]() -> Value {
        return execute(stateForInit);
    });
    result.result = sandBoxResult.result;
    result.msgStr = sandBoxResult.msgStr;
    result.error.errorValue = sandBoxResult.error;
    if (!sandBoxResult.error.isEmpty()) {
        for (size_t i = 0; i < sandBoxResult.stackTraceData.size(); i++) {
            ScriptSandboxExecuteResult::Error::StackTrace t;
            t.fileName = sandBoxResult.stackTraceData[i].fileName;
            t.line = sandBoxResult.stackTraceData[i].loc.line;
            t.column = sandBoxResult.stackTraceData[i].loc.column;
            result.error.stackTrace.pushBack(t);
        }
    }
    return result;
}

// NOTE: eval by direct call
Value Script::executeLocal(ExecutionState& state, Value thisValue, CodeBlock* parentCodeBlock, bool isEvalMode, bool needNewRecord)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);
    m_topCodeBlock->m_cachedASTNode = nullptr;

    bool isOnGlobal = true;
    FunctionEnvironmentRecord* fnRecord;
    {
        LexicalEnvironment* env = state.executionContext()->lexicalEnvironment();
        while (env) {
            if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                isOnGlobal = false;
                fnRecord = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                break;
            }
            env = env->outerEnvironment();
        }
    }

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, ((ProgramNode*)programNode)->scopeContext(), isEvalMode, isOnGlobal);

    delete programNode;

    EnvironmentRecord* record;
    if (UNLIKELY(needNewRecord)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        record = new DeclarativeEnvironmentRecordNotIndexed(state, m_topCodeBlock->identifierInfos());
    } else {
        record = state.executionContext()->lexicalEnvironment()->record();
    }

    const CodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        if (record->hasBinding(state, vec[i].m_name).m_index == SIZE_MAX) {
            record->createMutableBinding(state, vec[i].m_name, true);
        }
    }
    LexicalEnvironment* newEnvironment = new LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());

    ExecutionContext ec(state.context(), state.executionContext(), newEnvironment, m_topCodeBlock->isStrict());
    ExecutionState newState(&state, &ec);

    size_t stackStorageSize = m_topCodeBlock->identifierOnStackCount();
    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = ALLOCA((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + stackStorageSize + literalStorageSize) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }
    Value* literalStorage = stackStorage + stackStorageSize;
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    stackStorage[0] = thisValue;

    if (!isOnGlobal && m_topCodeBlock->usesArgumentsObject()) {
        AtomicString arguments = state.context()->staticStrings().arguments;
        if (fnRecord->hasBinding(newState, arguments).m_index == SIZE_MAX) {
            fnRecord->functionObject()->generateArgumentsObject(newState, fnRecord, nullptr);
        }
    }

    size_t unused;
    Value resultValue = ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile, &unused);
    clearStack<512>();

    return resultValue;
}
}
