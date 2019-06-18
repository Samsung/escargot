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
    RefPtr<Node> programNode = m_topCodeBlock->cachedASTNode();
    if (m_topCodeBlock->m_cachedASTNode) {
        m_topCodeBlock->m_cachedASTNode->deref();
    }
    m_topCodeBlock->m_cachedASTNode = nullptr;
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    m_topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), m_topCodeBlock, programNode.get(), ((ProgramNode*)programNode.get())->scopeContext(), isEvalMode, isOnGlobal);

    LexicalEnvironment* env;
    ExecutionContext* prevEc;
    {
        InterpretedCodeBlock* globalCodeBlock = m_topCodeBlock;
        LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, globalCodeBlock, state.context()->globalObject(), isEvalMode, !needNewEnv), nullptr);
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

    Value resultValue = ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);
    clearStack<512>();

    return resultValue;
}

Script::ScriptSandboxExecuteResult Script::sandboxExecute(ExecutionState& state)
{
    ScriptSandboxExecuteResult result;
    SandBox sb(state.context());
    ExecutionState stateForInit(state.context(), &state, nullptr);

    auto sandBoxResult = sb.run([&]() -> Value {
        return execute(stateForInit, false, false, true);
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
Value Script::executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isEvalMode, bool needNewRecord)
{
    RefPtr<Node> programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);
    if (m_topCodeBlock->m_cachedASTNode) {
        m_topCodeBlock->m_cachedASTNode->deref();
    }
    m_topCodeBlock->m_cachedASTNode = nullptr;

    bool isOnGlobal = true;
    FunctionEnvironmentRecord* fnRecord = nullptr;
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

    m_topCodeBlock->m_byteCodeBlock = ByteCodeGenerator::generateByteCode(state.context(), m_topCodeBlock, programNode.get(), ((ProgramNode*)programNode.get())->scopeContext(), isEvalMode, isOnGlobal);

    EnvironmentRecord* record;
    bool inStrict = false;
    if (UNLIKELY(needNewRecord)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        inStrict = true;
        record = new DeclarativeEnvironmentRecordNotIndexed();
    } else {
        record = state.executionContext()->lexicalEnvironment()->record();
    }

    const CodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    EnvironmentRecord* recordToAddVariable = record;
    LexicalEnvironment* e = state.executionContext()->lexicalEnvironment();
    while (!recordToAddVariable->isEvalTarget()) {
        e = e->outerEnvironment();
        recordToAddVariable = e->record();
    }
    for (size_t i = 0; i < len; i++) {
        recordToAddVariable->createBinding(state, vec[i].m_name, inStrict ? false : true, true);
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

    Value resultValue = ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);
    clearStack<512>();

    return resultValue;
}
}
