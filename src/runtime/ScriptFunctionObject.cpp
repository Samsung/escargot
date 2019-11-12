/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "ScriptFunctionObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/GeneratorObject.h"
#include "runtime/Context.h"
#include "runtime/ProxyObject.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/VMInstance.h"
#include "parser/ScriptParser.h"
#include "parser/ast/AST.h"
#include "util/Util.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

ScriptFunctionObject::ScriptFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnv, bool isConstructor, bool isGenerator)
    : ScriptFunctionObject(state, codeBlock, outerEnv,
                           ((isConstructor || isGenerator) ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2)) + (codeBlock->isStrict() ? 2 : 0))
{
    initStructureAndValues(state, isConstructor, isGenerator);

    if (isGenerator) {
        Object::setPrototype(state, state.context()->globalObject()->generator());
    } else {
        Object::setPrototype(state, state.context()->globalObject()->functionPrototype());
    }
}

ScriptFunctionObject::ScriptFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, size_t defaultPropertyCount)
    : FunctionObject(state, defaultPropertyCount)
{
    m_codeBlock = codeBlock;
    m_outerEnvironment = outerEnvironment;

#ifdef NDEBUG
    if (m_outerEnvironment) {
        ASSERT(m_outerEnvironment->isAllocatedOnHeap());
        if (m_outerEnvironment->record()->isDeclarativeEnvironmentRecord() && m_outerEnvironment->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            ASSERT(!m_outerEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->isFunctionEnvironmentRecordOnStack());
        }
    }
#endif
}

NEVER_INLINE void ScriptFunctionObject::generateByteCodeBlock(ExecutionState& state)
{
    Vector<CodeBlock*, GCUtil::gc_malloc_allocator<CodeBlock*>>& v = state.context()->compiledCodeBlocks();

    auto& currentCodeSizeTotal = state.context()->vmInstance()->compiledByteCodeSize();

    if (currentCodeSizeTotal > FUNCTION_OBJECT_BYTECODE_SIZE_MAX) {
        currentCodeSizeTotal = 0;
        std::vector<CodeBlock*, gc_allocator<CodeBlock*>> codeBlocksInCurrentStack;

        ExecutionState* es = &state;
        while (es) {
            FunctionObject* callee = es->resolveCallee();
            if (callee && callee->codeBlock()->isInterpretedCodeBlock()) {
                InterpretedCodeBlock* cblk = callee->codeBlock()->asInterpretedCodeBlock();
                if (cblk->script() && cblk->byteCodeBlock() && std::find(codeBlocksInCurrentStack.begin(), codeBlocksInCurrentStack.end(), cblk) == codeBlocksInCurrentStack.end()) {
                    codeBlocksInCurrentStack.push_back(cblk);
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

        size_t stackCodeBlocksSize = codeBlocksInCurrentStack.size();
        for (size_t i = 0; i < stackCodeBlocksSize; i++) {
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
    return FunctionObjectProcessCallGenerator::processCall<ScriptFunctionObject, false, false, false, FunctionObjectThisValueBinder, FunctionObjectNewTargetBinder, FunctionObjectReturnValueBinder>(state, this, thisValue, argc, argv, nullptr);
}

class ScriptFunctionObjectObjectThisValueBinderWithConstruct {
public:
    Value operator()(ExecutionState& calleeState, FunctionObject* self, const Value& thisArgument, bool isStrict)
    {
        ASSERT(thisArgument.isObject());
        return thisArgument;
    }
};

class ScriptFunctionObjectReturnValueBinderWithConstruct {
public:
    Value operator()(ExecutionState& state, ScriptFunctionObject* self, const Value& interpreterReturnValue, const Value& thisArgument, FunctionEnvironmentRecord* record)
    {
        // Let result be OrdinaryCallEvaluateBody(F, argumentsList).
        const Value& result = interpreterReturnValue;
        // If result.[[type]] is return, then
        // If Type(result.[[value]]) is Object, return NormalCompletion(result.[[value]]).
        if (result.isObject()) {
            return result;
        }
        // If kind is "base", return NormalCompletion(thisArgument).
        // -> kind is always `base`
        return thisArgument;
    }
};

class ScriptFunctionObjectNewTargetBinderWithConstruct {
public:
    void operator()(ExecutionState& calleeState, FunctionObject* self, Object* newTarget, FunctionEnvironmentRecord* record)
    {
        record->setNewTarget(newTarget);
    }
};

Object* ScriptFunctionObject::construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget)
{
    if (UNLIKELY(!newTarget->isConstructor())) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Not_Constructor_Function, codeBlock()->functionName());
    }
    // Assert: Type(newTarget) is Object.
    ASSERT(newTarget->isObject());
    ASSERT(newTarget->isConstructor());
    // Let kind be F’s [[ConstructorKind]] internal slot.
    ASSERT(constructorKind() == ConstructorKind::Base); // this is always `Base` because we define ScriptClassConsturctor::construct

    CodeBlock* cb = codeBlock();
    FunctionObject* constructor = this;
    // Let thisArgument be OrdinaryCreateFromConstructor(newTarget, "%ObjectPrototype%").
    // OrdinaryCreateFromConstructor -> Let proto be GetPrototypeFromConstructor(constructor, intrinsicDefaultProto).
    // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let proto be Get(constructor, "prototype").
    Value proto = newTarget->get(state, ObjectPropertyName(state.context()->staticStrings().prototype)).value(state, newTarget);

    // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> If Type(proto) is not Object, then
    // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let realm be GetFunctionRealm(constructor).
    // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> ReturnIfAbrupt(realm).
    // OrdinaryCreateFromConstructor -> GetPrototypeFromConstructor -> Let proto be realm’s intrinsic object named intrinsicDefaultProto.
    if (!proto.isObject()) {
        proto = codeBlock()->context()->globalObject()->objectPrototype();
    }

    Object* thisArgument = new Object(state);
    // Set the [[Prototype]] internal slot of obj to proto.
    thisArgument->setPrototype(state, proto);
    // ReturnIfAbrupt(thisArgument).

    return FunctionObjectProcessCallGenerator::processCall<ScriptFunctionObject, true, true, false, ScriptFunctionObjectObjectThisValueBinderWithConstruct, ScriptFunctionObjectNewTargetBinderWithConstruct, ScriptFunctionObjectReturnValueBinderWithConstruct>(state, this, Value(thisArgument), argc, argv, newTarget).asObject();
}

void ScriptFunctionObject::generateArgumentsObject(ExecutionState& state, size_t argc, Value* argv, FunctionEnvironmentRecord* environmentRecordWillArgumentsObjectBeLocatedIn, Value* stackStorage, bool isMapped)
{
    if (environmentRecordWillArgumentsObjectBeLocatedIn->m_argumentsObject->isArgumentsObject()) {
        return;
    }

    auto newArgumentsObject = new ArgumentsObject(state, this, argc, argv, environmentRecordWillArgumentsObjectBeLocatedIn, isMapped);
    environmentRecordWillArgumentsObjectBeLocatedIn->m_argumentsObject = newArgumentsObject;

    AtomicString arguments = state.context()->staticStrings().arguments;
    if (environmentRecordWillArgumentsObjectBeLocatedIn->isFunctionEnvironmentRecordNotIndexed()) {
        auto result = environmentRecordWillArgumentsObjectBeLocatedIn->hasBinding(state, arguments);
        if (UNLIKELY(result.m_index == SIZE_MAX)) {
            environmentRecordWillArgumentsObjectBeLocatedIn->createBinding(state, arguments, false, true);
            result = environmentRecordWillArgumentsObjectBeLocatedIn->hasBinding(state, arguments);
        }
        environmentRecordWillArgumentsObjectBeLocatedIn->initializeBinding(state, arguments, newArgumentsObject);
    } else {
        const InterpretedCodeBlock::IdentifierInfoVector& v = codeBlock()->asInterpretedCodeBlock()->identifierInfos();
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == arguments) {
                if (v[i].m_needToAllocateOnStack) {
                    stackStorage[v[i].m_indexForIndexedStorage] = newArgumentsObject;
                } else {
                    environmentRecordWillArgumentsObjectBeLocatedIn->heapStorage()[v[i].m_indexForIndexedStorage] = newArgumentsObject;
                }
                break;
            }
        }
    }
}
}
