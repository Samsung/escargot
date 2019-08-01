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

#include "FunctionObjectInlines.h"

namespace Escargot {

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

// function for derived classes. derived class MUST initlize member variable of FunctionObject.
FunctionObject::FunctionObject(ExecutionState& state, size_t defaultSpace)
    : Object(state, defaultSpace, false)
#ifndef NDEBUG
    , m_codeBlock((CodeBlock*)SIZE_MAX)
    , m_homeObject((Object*)SIZE_MAX)
#endif
{
}

// function for derived classes. derived class MUST initlize member variable of FunctionObject.
ScriptFunctionObject::ScriptFunctionObject(ExecutionState& state, size_t defaultSpace)
    : FunctionObject(state, defaultSpace)
#ifndef NDEBUG
    , m_outerEnvironment((LexicalEnvironment*)SIZE_MAX)
#endif
{
}

ScriptFunctionObject::ScriptFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnv)
    : FunctionObject(state,
                     (codeBlock->isConstructor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)) + ((codeBlock->isStrict() && !codeBlock->hasCallNativeFunctionCode()) ? 2 : 0))
{
    m_codeBlock = codeBlock;
    m_outerEnvironment = outerEnv;
    m_homeObject = nullptr;

    initStructureAndValues(state);
    Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
}

NEVER_INLINE void ScriptFunctionObject::generateByteCodeBlock(ExecutionState& state)
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

Value ScriptFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    ASSERT(!codeBlock()->hasCallNativeFunctionCode());
    return FunctionObjectProcessCallGenerator::processCall<ScriptFunctionObject, false, FunctionObjectThisValueBinder>(state, this, thisValue, argc, argv);
}

Object* ScriptFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget)
{
    ASSERT(!codeBlock()->hasCallNativeFunctionCode());
    // FIXME: newTarget
    CodeBlock* cb = codeBlock();
    FunctionObject* constructor = this;

    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget.isObject());
    ASSERT(newTarget.isConstructor());
    // Let kind be F’s [[ConstructorKind]] internal slot.
    // ConstructorKind kind = constructorKind();

    // FIXME: If kind is "base", then
    // FIXME: Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
    Object* thisArgument = new Object(state);
    if (constructor->getFunctionPrototype(state).isObject()) {
        thisArgument->setPrototype(state, constructor->getFunctionPrototype(state));
    } else {
        thisArgument->setPrototype(state, new Object(state));
    }

    Value result = FunctionObjectProcessCallGenerator::processCall<ScriptFunctionObject, true, FunctionObjectThisValueBinder>(state, this, thisArgument, argc, argv);
    if (result.isObject()) {
        return result.asObject();
    }
    return thisArgument;
}

void ScriptFunctionObject::generateArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* fnRecord, Value* stackStorage, bool isMapped)
{
    AtomicString arguments = state.context()->staticStrings().arguments;
    if (fnRecord->isFunctionEnvironmentRecordNotIndexed()) {
        auto result = fnRecord->hasBinding(state, arguments);
        if (UNLIKELY(result.m_index == SIZE_MAX)) {
            fnRecord->createBinding(state, arguments, false, true);
            result = fnRecord->hasBinding(state, arguments);
        }
        fnRecord->initializeBinding(state, arguments, fnRecord->createArgumentsObject(state, isMapped));
    } else {
        const InterpretedCodeBlock::IdentifierInfoVector& v = fnRecord->functionObject()->codeBlock()->asInterpretedCodeBlock()->identifierInfos();
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == arguments) {
                if (v[i].m_needToAllocateOnStack) {
                    stackStorage[v[i].m_indexForIndexedStorage] = fnRecord->createArgumentsObject(state, isMapped);
                } else {
                    ASSERT(fnRecord->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)fnRecord)->m_heapStorage[v[i].m_indexForIndexedStorage] = fnRecord->createArgumentsObject(state, isMapped);
                }
                break;
            }
        }
    }
}

void ScriptFunctionObject::generateRestParameter(ExecutionState& state, FunctionEnvironmentRecord* record, Value* parameterStorageInStack, const size_t argc, Value* argv)
{
    ArrayObject* newArray;
    size_t parameterLen = (size_t)m_codeBlock->parameterCount();

    if (argc > parameterLen) {
        size_t arrLen = argc - parameterLen;
        newArray = new ArrayObject(state, (double)arrLen);
        for (size_t i = 0; i < arrLen; i++) {
            newArray->setIndexedProperty(state, Value(i), argv[parameterLen + i]);
        }
    } else {
        newArray = new ArrayObject(state);
    }

    InterpretedCodeBlock::FunctionParametersInfo lastInfo = const_cast<InterpretedCodeBlock::FunctionParametersInfoVector&>(m_codeBlock->asInterpretedCodeBlock()->parametersInfomation()).back();
    if (!m_codeBlock->canUseIndexedVariableStorage()) {
        record->initializeBinding(state, lastInfo.m_name, Value(newArray));
    } else if (lastInfo.m_isHeapAllocated) {
        ASSERT(record->isFunctionEnvironmentRecordOnHeap());
        ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[lastInfo.m_index] = newArray;
    } else {
        parameterStorageInStack[lastInfo.m_index] = newArray;
    }
}
}
