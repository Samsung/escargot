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
#include "ExecutionState.h"
#include "Context.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "FunctionObject.h"

namespace Escargot {

void ExecutionState::throwException(const Value& e)
{
    context()->throwException(*this, e);
}

ExecutionStateRareData* ExecutionState::ensureRareData()
{
    if (m_parent & 1) {
        ExecutionState* p = parent();
        m_rareData = new ExecutionStateRareData();
        m_rareData->m_parent = p;
    }
    return rareData();
}

FunctionObject* ExecutionState::resolveCallee()
{
    ExecutionState* es = this;
    while (es) {
        if (es->lexicalEnvironment()) {
            if (es->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && es->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                return es->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
            }
        } else if (es->m_isNativeFunctionObjectExecutionContext) {
            return es->m_calledNativeFunctionObject;
        }

        es = es->parent();
    }

    return nullptr;
}

LexicalEnvironment* ExecutionState::mostNearestHeapAllocatedLexicalEnvironment()
{
    LexicalEnvironment* env = m_lexicalEnvironment;

    while (env) {
        if (env->record() && env->record()->isAllocatedOnHeap()) {
            return env;
        }
        env = env->outerEnvironment();
    }

    return nullptr;
}

Object* ExecutionState::getNewTarget()
{
    EnvironmentRecord* envRec = getThisEnvironment();

    ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    FunctionEnvironmentRecord* funcEnvRec = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();

    return funcEnvRec->newTarget();
}

EnvironmentRecord* ExecutionState::getThisEnvironment()
{
    LexicalEnvironment* lex = m_lexicalEnvironment;

    while (true) {
        EnvironmentRecord* envRec = lex->record();
        if (envRec->hasThisBinding()) {
            return envRec;
        }

        lex = lex->outerEnvironment();
    }
}

Value ExecutionState::makeSuperPropertyReference()
{
    // Let env be GetThisEnvironment( ).
    EnvironmentRecord* env = getThisEnvironment();
    ASSERT(env->isDeclarativeEnvironmentRecord() && env->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    // If env.HasSuperBinding() is false, throw a ReferenceError exception.
    if (!env->hasSuperBinding()) {
        ErrorObject::throwBuiltinError(*this, ErrorObject::Code::ReferenceError, errorMessage_No_Super_Binding);
    }

    // Let actualThis be env.GetThisBinding().
    // ReturnIfAbrupt(actualThis).
    // auto actualThis = env->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(*this);

    // Let baseValue be env.GetSuperBase().
    auto baseValue = env->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getSuperBase(*this);
    // Let bv be RequireObjectCoercible(baseValue).
    // ReturnIfAbrupt(bv).
    // Return a value of type Reference that is a Super Reference whose base value is bv, whose referenced name is propertyKey, whose thisValue is actualThis, and whose strict reference flag is strict.
    return baseValue.toObject(*this);
}

Value ExecutionState::getSuperConstructor()
{
    // Let envRec be GetThisEnvironment( ).
    EnvironmentRecord* envRec = getThisEnvironment();
    // Assert: envRec is a function Environment Record.
    ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    FunctionEnvironmentRecord* funcEnvRec = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();

    // Let activeFunction be envRec.[[FunctionObject]].
    FunctionObject* activeFunction = funcEnvRec->functionObject();

    // Let superConstructor be activeFunction.[[GetPrototypeOf]]().
    // ReturnIfAbrupt(superConstructor).
    Value superConstructor = activeFunction->getPrototype(*this);

    // If IsConstructor(superConstructor) is false, throw a TypeError exception.
    if (!superConstructor.isConstructor()) {
        ErrorObject::throwBuiltinError(*this, ErrorObject::Code::TypeError, errorMessage_No_Super_Binding);
    }

    // Return superConstructor.
    return superConstructor;
}

// callee is (generator || async) && isNotInEvalCode
bool ExecutionState::inPauserScope()
{
    ExecutionState* state = this;

    while (state) {
        if (state->isLocalEvalCode()) {
            return false;
        }
        if (state->lexicalEnvironment()) {
            auto env = state->lexicalEnvironment();
            auto record = env->record();
            if (record->isGlobalEnvironmentRecord()) {
                return false;
            }
            if (record->isDeclarativeEnvironmentRecord() && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                return record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptGeneratorFunctionObject() || record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptAsyncFunctionObject();
            }
        }
        state = state->parent();
    }

    return false;
}
}
