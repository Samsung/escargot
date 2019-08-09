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
#include "runtime/ScriptFunctionObject.h"
#include "util/Util.h"
#include "parser/ast/AST.h"

namespace Escargot {

Value Script::execute(ExecutionState& state, bool isExecuteOnEvalFunction, bool inStrictMode)
{
    ExecutionState newState(state.context());
    ExecutionState* codeExecutionState = &newState;

    EnvironmentRecord* globalRecord = new GlobalEnvironmentRecord(state, m_topCodeBlock, state.context()->globalObject(), &state.context()->globalDeclarativeRecord(), &state.context()->globalDeclarativeStorage());
    LexicalEnvironment* globalLexicalEnvironment = new LexicalEnvironment(globalRecord, nullptr);
    newState.setLexicalEnvironment(globalLexicalEnvironment, m_topCodeBlock->isStrict());

    if (inStrictMode && isExecuteOnEvalFunction) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        EnvironmentRecord* newVariableRecord = new DeclarativeEnvironmentRecordNotIndexed(state, true);
        ExecutionState* newVariableState = new ExecutionState(state.context());
        newVariableState->setLexicalEnvironment(new LexicalEnvironment(newVariableRecord, globalLexicalEnvironment), m_topCodeBlock->isStrict());
        newVariableState->setParent(&newState);
        codeExecutionState = newVariableState;
    }

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        // https://www.ecma-international.org/ecma-262/5.1/#sec-10.5
        // Step 2. If code is eval code, then let configurableBindings be true.
        if (vec[i].m_isVarDeclaration) {
            codeExecutionState->lexicalEnvironment()->record()->createBinding(*codeExecutionState, vec[i].m_name, isExecuteOnEvalFunction, vec[i].m_isMutable, true);
        }
    }

    if (!isExecuteOnEvalFunction) {
        const auto& globalLexicalVector = m_topCodeBlock->blockInfo(0)->m_identifiers;
        size_t len = globalLexicalVector.size();
        for (size_t i = 0; i < len; i++) {
            codeExecutionState->lexicalEnvironment()->record()->createBinding(*codeExecutionState, globalLexicalVector[i].m_name, false, globalLexicalVector[i].m_isMutable, false);
        }
    }

    Value thisValue(state.context()->globalObject());

    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = (Value*)ALLOCA((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + 1 + literalStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth()) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    stackStorage[0] = thisValue;
    Value* literalStorage = stackStorage + 1 + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth();
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue = ByteCodeInterpreter::interpret(*codeExecutionState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);
    clearStack<512>();

    return resultValue;
}

// NOTE: eval by direct call
Value Script::executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isStrictModeOutside, bool isEvalCodeOnFunction)
{
    ASSERT(m_topCodeBlock != nullptr);

    EnvironmentRecord* record;
    bool inStrict = false;
    if (UNLIKELY(isStrictModeOutside)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        inStrict = true;
        record = new DeclarativeEnvironmentRecordNotIndexed(state, true);
    } else {
        record = state.lexicalEnvironment()->record();
    }

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t vecLen = vec.size();

    // test there was let on block scope
    LexicalEnvironment* e = state.lexicalEnvironment();
    while (e) {
        if (e->record()->isDeclarativeEnvironmentRecord() && e->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            break;
        }
        if (e->record()->isGlobalEnvironmentRecord()) {
            break;
        }

        for (size_t i = 0; i < vecLen; i++) {
            if (vec[i].m_isVarDeclaration) {
                auto slot = e->record()->hasBinding(state, vec[i].m_name);
                if (slot.m_isLexicallyDeclared && slot.m_index != SIZE_MAX) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, vec[i].m_name.string(), false, String::emptyString, errorMessage_DuplicatedIdentifier);
                }
            }
        }

        e = e->outerEnvironment();
    }

    EnvironmentRecord* recordToAddVariable = record;
    e = state.lexicalEnvironment();
    while (!recordToAddVariable->isVarDeclarationTarget()) {
        e = e->outerEnvironment();
        recordToAddVariable = e->record();
    }
    for (size_t i = 0; i < vecLen; i++) {
        if (vec[i].m_isVarDeclaration) {
            recordToAddVariable->createBinding(state, vec[i].m_name, inStrict ? false : true, true);
        }
    }

    LexicalEnvironment* newEnvironment = new LexicalEnvironment(record, state.lexicalEnvironment());
    ExecutionState newState(&state, newEnvironment, m_topCodeBlock->isStrict());

    size_t stackStorageSize = m_topCodeBlock->totalStackAllocatedVariableSize();
    size_t identifierOnStackCount = m_topCodeBlock->identifierOnStackCount();
    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = ALLOCA((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + stackStorageSize + literalStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth()) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    for (size_t i = 0; i < identifierOnStackCount; i++) {
        stackStorage[i] = Value();
    }
    Value* literalStorage = stackStorage + stackStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth();
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    stackStorage[0] = thisValue;

    if (isEvalCodeOnFunction && m_topCodeBlock->usesArgumentsObject()) {
        AtomicString arguments = state.context()->staticStrings().arguments;

        FunctionEnvironmentRecord* fnRecord = nullptr;
        {
            LexicalEnvironment* env = state.lexicalEnvironment();
            while (env) {
                if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                    fnRecord = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                    break;
                }
                env = env->outerEnvironment();
            }
        }

        if (fnRecord->hasBinding(newState, arguments).m_index == SIZE_MAX && state.callee()->isScriptFunctionObject()) {
            // FIXME check if formal parameters does not contain a rest parameter, any binding patterns, or any initializers.
            bool isMapped = !state.callee()->codeBlock()->hasArgumentInitializers() && !inStrict;
            state.callee()->asScriptFunctionObject()->generateArgumentsObject(newState, state.argc(), state.argv(), fnRecord, nullptr, isMapped);
        }
    }

    Value resultValue = ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);
    clearStack<512>();

    return resultValue;
}
}
