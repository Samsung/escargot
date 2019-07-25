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
#include "ArrayObject.h"
#include "FunctionObject.h"
#include "GeneratorObject.h"
#include "Context.h"
#include "parser/ScriptParser.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/VMInstance.h"
#include "parser/ast/AST.h"
#include "util/Util.h"
#include "runtime/ProxyObject.h"

namespace Escargot {

size_t g_functionObjectTag;

void FunctionObject::initStructureAndValues(ExecutionState& state)
{
    if (isConstructor()) {
        m_structure = state.context()->defaultStructureForFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(Object::createFunctionPrototypeObject(state, this)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->parameterCount()));
    } else {
        m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->parameterCount()));
    }
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2, false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(nullptr)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(false)
{
    ASSERT(!isConstructor());
    initStructureAndValues(state);
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin)
    : Object(state, codeBlock->isConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(nullptr)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(codeBlock->isConstructor())
{
    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    if (isConstructor())
        m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : Object(state, info.m_isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(false)
{
    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3, false)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(true)
{
    ASSERT(isConstructor());
    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnv)
    : Object(state,
             (codeBlock->isConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)) + ((codeBlock->isStrict() && !codeBlock->hasCallNativeFunctionCode()) ? 2 : 0),
             false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(outerEnv)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(false)
{
    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltinProxyConstructor)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2, false)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
    , m_homeObject(nullptr)
    , m_constructorKind(ConstructorKind::Base)
    , m_isBuiltin(true)
{
    ASSERT(isConstructor());
    // The Proxy constructor does not have a prototype property
    m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->parameterCount()));
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

NEVER_INLINE void FunctionObject::generateByteCodeBlock(ExecutionState& state)
{
    Vector<CodeBlock*, GCUtil::gc_malloc_ignore_off_page_allocator<CodeBlock*>>& v = state.context()->compiledCodeBlocks();

    auto& currentCodeSizeTotal = state.context()->vmInstance()->compiledByteCodeSize();

    if (currentCodeSizeTotal > FUNCTION_OBJECT_BYTECODE_SIZE_MAX) {
        currentCodeSizeTotal = 0;
        std::vector<CodeBlock*, gc_allocator<CodeBlock*>> codeBlocksInCurrentStack;

        ExecutionState* es = &state;
        while (es != nullptr) {
            auto env = es->lexicalEnvironment();
            if (env != nullptr && env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                if (env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->isInterpretedCodeBlock()) {
                    InterpretedCodeBlock* cblk = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->asInterpretedCodeBlock();
                    if (cblk->script() && cblk->byteCodeBlock() && std::find(codeBlocksInCurrentStack.begin(), codeBlocksInCurrentStack.end(), cblk) == codeBlocksInCurrentStack.end()) {
                        codeBlocksInCurrentStack.push_back(cblk);
                    }
                }
            }
            es = es->parent();
        }

        for (size_t i = 0; i < v.size(); i++) {
            if (std::find(codeBlocksInCurrentStack.begin(), codeBlocksInCurrentStack.end(), v[i]) == codeBlocksInCurrentStack.end()) {
                v[i]->m_byteCodeBlock = nullptr;
            }
        }

        v.clear();
        v.resizeWithUninitializedValues(codeBlocksInCurrentStack.size());

        for (size_t i = 0; i < codeBlocksInCurrentStack.size(); i++) {
            v[i] = codeBlocksInCurrentStack[i];
            currentCodeSizeTotal += v[i]->m_byteCodeBlock->memoryAllocatedSize();
        }
    }
    ASSERT(!m_codeBlock->hasCallNativeFunctionCode());

    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (state.stackBase() - currentStackBase);
#else
    size_t stackRemainApprox = STACK_LIMIT_FROM_BASE - (currentStackBase - state.stackBase());
#endif


    state.context()->scriptParser().generateFunctionByteCode(state, m_codeBlock->asInterpretedCodeBlock(), stackRemainApprox);

    v.pushBack(m_codeBlock);

    currentCodeSizeTotal += m_codeBlock->m_byteCodeBlock->memoryAllocatedSize();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
Object* FunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget)
{
    // FIXME: newTarget
    CodeBlock* cb = codeBlock();
    FunctionObject* constructor = this;

    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget.isObject());
    ASSERT(newTarget.isConstructor());
    // Let kind be F’s [[ConstructorKind]] internal slot.
    ConstructorKind kind = constructorKind();
    Value result;

    if (cb->hasCallNativeFunctionCode()) {
        // Builtin function
        result = processBuiltinFunctionCall(state, Value(Value::EmptyValue), argc, argv, true, newTarget);

        ASSERT(result.isObject());
        return result.asObject();
    } else {
        // FIXME: If kind is "base", then
        // FIXME: Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
        Object* thisArgument = new Object(state);
        if (constructor->getFunctionPrototype(state).isObject()) {
            thisArgument->setPrototype(state, constructor->getFunctionPrototype(state));
        } else {
            thisArgument->setPrototype(state, new Object(state));
        }

        result = processCall(state, thisArgument, argc, argv, true);
        if (result.isObject()) {
            return result.asObject();
        }
        return thisArgument;
    }
}

Value FunctionObject::processBuiltinFunctionCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, Value* argv, bool isNewExpression, const Value& newTarget)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY((state.stackBase() - currentStackBase) > STACK_LIMIT_FROM_BASE)) {
#else
    if (UNLIKELY((currentStackBase - state.stackBase()) > STACK_LIMIT_FROM_BASE)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    Context* ctx = m_codeBlock->context();
    bool isStrict = m_codeBlock->isStrict();
    bool isSuperCall = state.isOnGoingSuperCall();

    if (UNLIKELY(!isNewExpression && functionKind() == FunctionKind::ClassConstructor && !isSuperCall)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Class constructor cannot be invoked without 'new'");
    }

    if (UNLIKELY(isSuperCall && isBuiltin() && !isNewExpression)) {
        Value returnValue = Object::construct(state, this, argc, argv);
        returnValue.asObject()->setPrototype(state, receiverSrc.toObject(state)->getPrototype(state));
        return returnValue;
    }

    CallNativeFunctionData* code = m_codeBlock->nativeFunctionData();
    FunctionEnvironmentRecordSimple record(this);
    LexicalEnvironment env(&record, outerEnvironment());

    size_t len = m_codeBlock->parameterCount();
    if (argc < len) {
        Value* newArgv = (Value*)alloca(sizeof(Value) * len);
        for (size_t i = 0; i < argc; i++) {
            newArgv[i] = argv[i];
        }
        for (size_t i = argc; i < len; i++) {
            newArgv[i] = Value();
        }
        argv = newArgv;
    }

    Value receiver = receiverSrc;
    // prepare receiver
    if (UNLIKELY(!isStrict)) {
        if (receiver.isUndefinedOrNull()) {
            receiver = ctx->globalObject();
        } else {
            receiver = receiver.toObject(state);
        }
    }

    // FIXME bind this value and newTarget
    record.bindThisValue(state, receiver);
    if (isNewExpression) {
        record.setNewTarget(newTarget);
    }

    ExecutionState newState(ctx, &state, &env, isStrict, &receiver);
    try {
        Value returnValue = code->m_fn(newState, receiver, argc, argv, isNewExpression);
        if (UNLIKELY(isSuperCall)) {
            state.getThisEnvironment()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->bindThisValue(state, returnValue);
            state.setOnGoingSuperCall(false);
            if (returnValue.isObject() && !isBuiltin()) {
                returnValue.asObject()->setPrototype(state, receiver.toObject(state)->getPrototype(state));
            }
        }
        return returnValue;
    } catch (const Value& v) {
        ByteCodeInterpreter::processException(newState, v, SIZE_MAX);
        return Value();
    }
}

Value FunctionObject::processCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, NULLABLE Value* argv, bool isNewExpression)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY((state.stackBase() - currentStackBase) > STACK_LIMIT_FROM_BASE)) {
#else
    if (UNLIKELY((currentStackBase - state.stackBase()) > STACK_LIMIT_FROM_BASE)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    ASSERT(m_codeBlock->isInterpretedCodeBlock());

    Context* ctx = m_codeBlock->context();
    bool isStrict = m_codeBlock->isStrict();
    bool isSuperCall = state.isOnGoingSuperCall();

    if (UNLIKELY(!isNewExpression && functionKind() == FunctionKind::ClassConstructor && !isSuperCall)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Class constructor cannot be invoked without 'new'");
    }

    if (UNLIKELY(isSuperCall && isBuiltin() && !isNewExpression)) {
        Value returnValue = Object::construct(state, this, argc, argv);
        returnValue.asObject()->setPrototype(state, receiverSrc.toObject(state)->getPrototype(state));
        return returnValue;
    }

    // prepare ByteCodeBlock if needed
    if (UNLIKELY(m_codeBlock->asInterpretedCodeBlock()->byteCodeBlock() == nullptr)) {
        generateByteCodeBlock(state);
    }

    ByteCodeBlock* blk = m_codeBlock->asInterpretedCodeBlock()->byteCodeBlock();

    size_t registerSize = blk->m_requiredRegisterFileSizeInValueSize;
    size_t identifierOnStackCount = m_codeBlock->asInterpretedCodeBlock()->identifierOnStackCount();
    size_t stackStorageSize = m_codeBlock->asInterpretedCodeBlock()->totalStackAllocatedVariableSize();
    size_t literalStorageSize = blk->m_numeralLiteralData.size();
    Value* literalStorageSrc = blk->m_numeralLiteralData.data();
    size_t parameterCopySize = std::min(argc, (size_t)m_codeBlock->parameterCount());

    // prepare env, ec
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* lexEnv;

    if (LIKELY(m_codeBlock->canAllocateEnvironmentOnStack())) {
        // no capture, very simple case
        record = new (alloca(sizeof(FunctionEnvironmentRecordSimple))) FunctionEnvironmentRecordSimple(this);
        lexEnv = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, outerEnvironment());
    } else {
        if (LIKELY(m_codeBlock->canUseIndexedVariableStorage())) {
            record = new FunctionEnvironmentRecordOnHeap(this, argc, argv);
        } else {
            if (LIKELY(!m_codeBlock->needsVirtualIDOperation())) {
                record = new FunctionEnvironmentRecordNotIndexed(this, argc, argv);
            } else {
                record = new FunctionEnvironmentRecordNotIndexedWithVirtualID(this, argc, argv);
            }
        }
        lexEnv = new LexicalEnvironment(record, outerEnvironment());
    }

    if (receiverSrc.isObject()) {
        record->setNewTarget(receiverSrc.asObject());
    }

    Value* registerFile;

    if (UNLIKELY(m_codeBlock->isGenerator())) {
        if (isNewExpression) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Generator cannot be invoked with 'new'");
        }
        registerFile = (Value*)GC_MALLOC((registerSize + stackStorageSize + literalStorageSize) * sizeof(Value));
    } else {
        registerFile = (Value*)alloca((registerSize + stackStorageSize + literalStorageSize) * sizeof(Value));
    }

    Value* stackStorage = registerFile + registerSize;

    {
        Value* literalStorage = stackStorage + stackStorageSize;
        for (size_t i = 0; i < literalStorageSize; i++) {
            literalStorage[i] = literalStorageSrc[i];
        }
    }

    // prepare receiver
    if (UNLIKELY(m_codeBlock->isArrowFunctionExpression())) {
        stackStorage[0] = ctx->globalObject();
    } else {
        if (!isStrict) {
            if (receiverSrc.isUndefinedOrNull()) {
                stackStorage[0] = ctx->globalObject();
            } else {
                stackStorage[0] = receiverSrc.toObject(state);
            }
        } else {
            stackStorage[0] = receiverSrc;
        }
    }

    if (constructorKind() == ConstructorKind::Base && thisMode() != ThisMode::Lexical) {
        record->bindThisValue(state, stackStorage[0]);
    }

    // binding function name
    stackStorage[1] = this;
    if (UNLIKELY(m_codeBlock->m_isFunctionNameSaveOnHeap)) {
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[0] = this;
        } else {
            record->initializeBinding(state, m_codeBlock->functionName(), this);
        }
    }

    if (UNLIKELY(m_codeBlock->m_isFunctionNameExplicitlyDeclared)) {
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            if (UNLIKELY(m_codeBlock->m_isFunctionNameSaveOnHeap)) {
                ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[0] = Value();
            } else {
                stackStorage[1] = Value();
            }
        } else {
            record->initializeBinding(state, m_codeBlock->functionName(), Value());
        }
    }

    // prepare parameters
    if (UNLIKELY(m_codeBlock->needsComplexParameterCopy())) {
        for (size_t i = 2; i < identifierOnStackCount; i++) {
            stackStorage[i] = Value();
        }

#ifndef NDEBUG
        for (size_t i = identifierOnStackCount; i < stackStorageSize; i++) {
            stackStorage[i] = Value(Value::EmptyValue);
        }
#endif
        if (!m_codeBlock->canUseIndexedVariableStorage()) {
            const InterpretedCodeBlock::FunctionParametersInfoVector& info = m_codeBlock->asInterpretedCodeBlock()->parametersInfomation();
            for (size_t i = 0; i < parameterCopySize; i++) {
                record->initializeBinding(state, info[i].m_name, argv[i]);
            }
            for (size_t i = parameterCopySize; i < info.size(); i++) {
                record->initializeBinding(state, info[i].m_name, Value());
            }

            // Handle rest param
            if (m_codeBlock->m_hasRestElement) {
                size_t argListLen = (size_t)m_codeBlock->parameterCount();
                size_t arrayLen = argc - parameterCopySize;
                ArrayObject* newArray = new ArrayObject(state, (double)arrayLen);
                for (size_t i = 0; i < arrayLen; i++) {
                    newArray->setIndexedProperty(state, Value(i), argv[argListLen + i]);
                }
                record->initializeBinding(state, const_cast<InterpretedCodeBlock::FunctionParametersInfoVector&>(info).back().m_name, Value(newArray));
            }
        } else {
            Value* parameterStorageInStack = stackStorage + 2;
            const InterpretedCodeBlock::FunctionParametersInfoVector& info = m_codeBlock->asInterpretedCodeBlock()->parametersInfomation();
            for (size_t i = 0; i < info.size(); i++) {
                Value val(Value::ForceUninitialized);
                // NOTE: consider the special case with duplicated parameter names (**test262: S10.2.1_A3)
                if (i < argc)
                    val = argv[i];
                else if (info[i].m_index >= (int32_t)argc)
                    continue;
                else
                    val = Value();
                if (info[i].m_isHeapAllocated) {
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[info[i].m_index] = val;
                } else {
                    parameterStorageInStack[info[i].m_index] = val;
                }
            }

            // Handle rest param
            if (m_codeBlock->m_hasRestElement) {
                size_t argListLen = (size_t)m_codeBlock->parameterCount();
                size_t arrayLen = argc - parameterCopySize;
                ArrayObject* newArray = new ArrayObject(state, (double)arrayLen);
                for (size_t i = 0; i < arrayLen; i++) {
                    newArray->setIndexedProperty(state, Value(i), argv[argListLen + i]);
                }
                InterpretedCodeBlock::FunctionParametersInfo lastInfo = const_cast<InterpretedCodeBlock::FunctionParametersInfoVector&>(info).back();
                if (lastInfo.m_isHeapAllocated) {
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[lastInfo.m_index] = newArray;
                } else {
                    parameterStorageInStack[lastInfo.m_index] = newArray;
                }
            }
        }
    } else {
        Value* parameterStorageInStack = stackStorage + 2;
        for (size_t i = 0; i < parameterCopySize; i++) {
            parameterStorageInStack[i] = argv[i];
        }

        for (size_t i = parameterCopySize + 2; i < identifierOnStackCount; i++) {
            stackStorage[i] = Value();
        }

#ifndef NDEBUG
        for (size_t i = identifierOnStackCount; i < stackStorageSize; i++) {
            stackStorage[i] = Value(Value::EmptyValue);
        }
#endif

        // Handle rest param
        if (m_codeBlock->m_hasRestElement) {
            size_t argListLen = (size_t)m_codeBlock->parameterCount();
            size_t arrayLen = argc - parameterCopySize;
            ArrayObject* newArray = new ArrayObject(state, (double)arrayLen);
            for (size_t i = 0; i < arrayLen; i++) {
                newArray->setIndexedProperty(state, Value(i), argv[argListLen + i]);
            }
            parameterStorageInStack[argListLen] = newArray;
        }
    }

    if (UNLIKELY(m_codeBlock->isGenerator())) {
        ExecutionState* newState = new ExecutionState(ctx, &state, lexEnv, isStrict, registerFile);

        if (UNLIKELY(m_codeBlock->usesArgumentsObject())) {
            generateArgumentsObject(*newState, record, stackStorage);
        }

        GeneratorObject* gen = new GeneratorObject(state, newState, blk);
        newState->setGeneratorTarget(gen);
        return gen;
    }

    ExecutionState newState(ctx, &state, lexEnv, isStrict, registerFile);

    if (UNLIKELY(m_codeBlock->usesArgumentsObject())) {
        generateArgumentsObject(newState, record, stackStorage);
    }

    // run function
    const Value returnValue = ByteCodeInterpreter::interpret(newState, blk, 0, registerFile);
    if (UNLIKELY(blk->m_shouldClearStack))
        clearStack<512>();

    if (UNLIKELY(isSuperCall)) {
        if (returnValue.isNull()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InvalidDerivedConstructorReturnValue);
        }

        FunctionEnvironmentRecord* thisEnvironmentRecord = state.getThisEnvironment()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
        if (returnValue.isObject()) {
            thisEnvironmentRecord->bindThisValue(state, returnValue);
            returnValue.asObject()->setPrototype(state, receiverSrc.toObject(state)->getPrototype(state));
        } else {
            thisEnvironmentRecord->bindThisValue(state, stackStorage[0]);
        }
        state.setOnGoingSuperCall(false);
    }

    return returnValue;
}

void FunctionObject::generateArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* fnRecord, Value* stackStorage)
{
    AtomicString arguments = state.context()->staticStrings().arguments;
    if (fnRecord->isFunctionEnvironmentRecordNotIndexed()) {
        auto result = fnRecord->hasBinding(state, arguments);
        if (UNLIKELY(result.m_index == SIZE_MAX)) {
            fnRecord->createBinding(state, arguments, false, true);
            result = fnRecord->hasBinding(state, arguments);
        }
        fnRecord->initializeBinding(state, arguments, fnRecord->createArgumentsObject(state));
    } else {
        const InterpretedCodeBlock::IdentifierInfoVector& v = fnRecord->functionObject()->codeBlock()->asInterpretedCodeBlock()->identifierInfos();
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == arguments) {
                if (v[i].m_needToAllocateOnStack) {
                    stackStorage[v[i].m_indexForIndexedStorage] = fnRecord->createArgumentsObject(state);
                } else {
                    ASSERT(fnRecord->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)fnRecord)->m_heapStorage[v[i].m_indexForIndexedStorage] = fnRecord->createArgumentsObject(state);
                }
                break;
            }
        }
    }
}
}
