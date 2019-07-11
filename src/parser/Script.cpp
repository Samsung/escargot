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

Value Script::execute(ExecutionState& state, bool isEvalMode, bool needNewEnv)
{
    ASSERT(m_topCodeBlock != nullptr);

    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, m_topCodeBlock, state.context()->globalObject(), isEvalMode, !needNewEnv), nullptr);
    ExecutionState newState(state.context());

    if (UNLIKELY(needNewEnv)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        newState.setParent(new ExecutionState(&state, globalEnvironment, m_topCodeBlock->isStrict()));

        EnvironmentRecord* record = new DeclarativeEnvironmentRecordNotIndexed(state, m_topCodeBlock->identifierInfos());
        newState.setLexicalEnvironment(new LexicalEnvironment(record, globalEnvironment), m_topCodeBlock->isStrict());
    } else {
        newState.setLexicalEnvironment(globalEnvironment, m_topCodeBlock->isStrict());
    }

    Value thisValue(state.context()->globalObject());

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

// NOTE: eval by direct call
Value Script::executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool needNewRecord)
{
    ASSERT(m_topCodeBlock != nullptr);

    bool isOnGlobal = true;
    FunctionEnvironmentRecord* fnRecord = nullptr;
    {
        LexicalEnvironment* env = state.lexicalEnvironment();
        while (env) {
            if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                isOnGlobal = false;
                fnRecord = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                break;
            }
            env = env->outerEnvironment();
        }
    }

    EnvironmentRecord* record;
    bool inStrict = false;
    if (UNLIKELY(needNewRecord)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        inStrict = true;
        record = new DeclarativeEnvironmentRecordNotIndexed();
    } else {
        record = state.lexicalEnvironment()->record();
    }

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    EnvironmentRecord* recordToAddVariable = record;
    LexicalEnvironment* e = state.lexicalEnvironment();
    while (!recordToAddVariable->isEvalTarget()) {
        e = e->outerEnvironment();
        recordToAddVariable = e->record();
    }
    for (size_t i = 0; i < len; i++) {
        if (vec[i].m_isVarDeclaration) {
            recordToAddVariable->createBinding(state, vec[i].m_name, inStrict ? false : true, true);
        }
    }
    LexicalEnvironment* newEnvironment = new LexicalEnvironment(record, state.lexicalEnvironment());

    ExecutionState newState(&state, newEnvironment, m_topCodeBlock->isStrict());

    size_t stackStorageSize = m_topCodeBlock->totalStackAllocatedVariableSize();
    size_t identifierOnStackCount = m_topCodeBlock->identifierOnStackCount();
    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = ALLOCA((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + stackStorageSize + literalStorageSize) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    for (size_t i = 0; i < identifierOnStackCount; i++) {
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
