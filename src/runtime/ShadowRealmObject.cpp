/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Global.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/ExecutionState.h"
#include "runtime/ShadowRealmObject.h"
#include "runtime/WrappedFunctionObject.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#if defined(ESCARGOT_ENABLE_SHADOWREALM)

ShadowRealmObject::ShadowRealmObject(ExecutionState& state)
    : DerivedObject(state)
{
}

ShadowRealmObject::ShadowRealmObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

// https://tc39.es/proposal-shadowrealm/#sec-wrappedfunctioncreate
static Value wrappedFunctionCreate(ExecutionState& state, Context* callerRealm, Object* target)
{
    // Let internalSlotsList be the internal slots listed in Table 2, plus [[Prototype]] and [[Extensible]].
    // Let wrapped be MakeBasicObject(internalSlotsList).
    // Set wrapped.[[Prototype]] to callerRealm.[[Intrinsics]].[[%Function.prototype%]].
    // Set wrapped.[[Call]] as described in 2.1.
    // Set wrapped.[[WrappedTargetFunction]] to Target.
    // Set wrapped.[[Realm]] to callerRealm.
    // Let result be Completion(CopyNameAndLength(wrapped, Target)).
    // If result is an abrupt completion, throw a TypeError exception.
    WrappedFunctionObject* wrapped = nullptr;
    try {
        // https://tc39.es/proposal-shadowrealm/#sec-copynameandlength
        double L = 0;
        Value targetName;
        bool targetHasLength = target->hasOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().length));
        if (targetHasLength) {
            Value targetLen = target->get(state, ObjectPropertyName(state.context()->staticStrings().length)).value(state, target);
            if (targetLen.isNumber()) {
                if (targetLen.toNumber(state) == std::numeric_limits<double>::infinity()) {
                    L = std::numeric_limits<double>::infinity();
                } else if (targetLen.toNumber(state) == -std::numeric_limits<double>::infinity()) {
                    L = 0;
                } else {
                    double targetLenAsInt = targetLen.toInteger(state);
                    L = std::max(0.0, targetLenAsInt);
                }
            }
        }
        targetName = target->get(state, ObjectPropertyName(state.context()->staticStrings().name)).value(state, target);
        if (!targetName.isString()) {
            targetName = String::emptyString();
        }

        wrapped = new WrappedFunctionObject(state, target, callerRealm, Value(Value::DoubleToIntConvertibleTestNeeds, L), targetName);
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Copy name and length from target function failed");
    }
    // Return wrapped.
    return wrapped;
}

// https://tc39.es/proposal-shadowrealm/#sec-getwrappedvalue
Value ShadowRealmObject::getWrappedValue(ExecutionState& state, Context* callerRealm, const Value& value)
{
    // If value is an Object, then
    if (value.isObject()) {
        // If IsCallable(value) is false, throw a TypeError exception.
        if (!value.asObject()->isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "The result of ShadowRealm.evaluate must be a callable function");
        }
        // Return ? WrappedFunctionCreate(callerRealm, value).
        return wrappedFunctionCreate(state, callerRealm, value.asObject());
    }
    // Return value.
    return value;
}

static Value execute(ExecutionState& state, Script* script, bool isExecuteOnEvalFunction, bool inStrictMode, Context* callerContext, Context* evalContext)
{
    InterpretedCodeBlock* topCodeBlock = script->topCodeBlock();
    ByteCodeBlock* byteCodeBlock = topCodeBlock->byteCodeBlock();

    ExecutionState* newState;
    if (byteCodeBlock->needsExtendedExecutionState()) {
        newState = new (alloca(sizeof(ExtendedExecutionState))) ExtendedExecutionState(evalContext);
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(evalContext);
    }

    ExecutionState* codeExecutionState = newState;

    EnvironmentRecord* globalRecord = new GlobalEnvironmentRecord(*newState, topCodeBlock, evalContext->globalObject(), evalContext->globalDeclarativeRecord(), evalContext->globalDeclarativeStorage());
    LexicalEnvironment* globalLexicalEnvironment = new LexicalEnvironment(globalRecord, nullptr);
    newState->setLexicalEnvironment(globalLexicalEnvironment, topCodeBlock->isStrict());

    EnvironmentRecord* globalVariableRecord = globalRecord;

    EnvironmentRecord* newVariableRecord = new DeclarativeEnvironmentRecordNotIndexed(*newState, true);
    ExecutionState* newVariableState = new ExtendedExecutionState(evalContext);
    newVariableState->setLexicalEnvironment(new LexicalEnvironment(newVariableRecord, globalLexicalEnvironment), topCodeBlock->isStrict());
    newVariableState->setParent(newState);
    codeExecutionState = newVariableState;

    const InterpretedCodeBlock::IdentifierInfoVector& identifierVector = topCodeBlock->identifierInfos();
    size_t identifierVectorLen = identifierVector.size();

    const auto& globalLexicalVector = topCodeBlock->blockInfo(0)->identifiers();
    size_t globalLexicalVectorLen = globalLexicalVector.size();

    {
        VirtualIdDisabler d(evalContext); // we should create binding even there is virtual ID

        for (size_t i = 0; i < globalLexicalVectorLen; i++) {
            codeExecutionState->lexicalEnvironment()->record()->createBinding(*codeExecutionState, globalLexicalVector[i].m_name, false, globalLexicalVector[i].m_isMutable, false);
        }

        for (size_t i = 0; i < identifierVectorLen; i++) {
            // https://www.ecma-international.org/ecma-262/5.1/#sec-10.5
            // Step 2. If code is eval code, then let configurableBindings be true.
            if (identifierVector[i].m_isVarDeclaration) {
                globalVariableRecord->createBinding(*codeExecutionState, identifierVector[i].m_name, isExecuteOnEvalFunction, identifierVector[i].m_isMutable, true, topCodeBlock);
            }
        }
    }

    Value thisValue(evalContext->globalObjectProxy());

    const size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    const size_t registerFileSize = byteCodeBlock->m_requiredTotalRegisterNumber;
    ASSERT(registerFileSize == byteCodeBlock->m_requiredOperandRegisterNumber + topCodeBlock->totalStackAllocatedVariableSize() + literalStorageSize);

    Value* registerFile;
    registerFile = CustomAllocator<Value>().allocate(registerFileSize);
    // we need to reset allocated memory because customAllocator read it
    memset(static_cast<void*>(registerFile), 0, sizeof(Value) * registerFileSize);
    registerFile[0] = Value();

    Value* stackStorage = registerFile + byteCodeBlock->m_requiredOperandRegisterNumber;
    stackStorage[0] = thisValue;

    Value* literalStorage = stackStorage + topCodeBlock->totalStackAllocatedVariableSize();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue;
#ifdef ESCARGOT_DEBUGGER
    // set the next(first) breakpoint to be stopped in a newer script execution
    evalContext->setAsAlwaysStopState();
#endif
    resultValue = Interpreter::interpret(codeExecutionState, byteCodeBlock, reinterpret_cast<size_t>(byteCodeBlock->m_code.data()), registerFile);
    clearStack<512>();

    // we give up program bytecodeblock after first excution for reducing memory usage
    topCodeBlock->setByteCodeBlock(nullptr);

    return resultValue;
}

Value ShadowRealmObject::performShadowRealmEval(ExecutionState& state, Value& sourceText, Context* callerRealm, Context* evalRealm)
{
    ScriptParser parser(evalRealm);
    bool strictFromOutside = false;
    Script* script = parser.initializeScript(nullptr, 0, sourceText.asString(), evalRealm->staticStrings().lazyEvalCode().string(), nullptr, false, true, false, false, strictFromOutside, false, false, false, true).scriptThrowsExceptionIfParseError(state);

    ExtendedExecutionState stateForNewGlobal(evalRealm);
    Value result;
    try {
        result = execute(stateForNewGlobal, script, true, script->topCodeBlock()->isStrict(), callerRealm, evalRealm);
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ShadowRealm.evaluate failed");
    }
    return getWrappedValue(state, callerRealm, result);
}

#endif

} // namespace Escargot
