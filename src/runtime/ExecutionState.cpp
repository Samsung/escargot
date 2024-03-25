/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "ExecutionState.h"
#include "Context.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "FunctionObject.h"
#include "NativeFunctionObject.h"
#include "ScriptClassConstructorFunctionObject.h"

namespace Escargot {

ExecutionState::ExecutionState()
    : m_context(nullptr)
    , m_lexicalEnvironment(nullptr)
    , m_programCounter(nullptr)
    , m_parent(0)
    , m_hasRareData(false)
    , m_inStrictMode(false)
    , m_isNativeFunctionObjectExecutionContext(false)
    , m_inExecutionStopState(false)
#if defined(ESCARGOT_ENABLE_TEST)
    , m_onTry(false)
    , m_onCatch(false)
    , m_onFinally(false)
#endif
#if defined(ENABLE_TCO)
    , m_inTCO(false)
#endif
    , m_argc(0)
    , m_argv(nullptr)
{
}

void ExecutionState::throwException(const Value& e)
{
    context()->throwException(*this, e);
}

FunctionObject* ExecutionState::resolveCallee()
{
    ExecutionState* es = this;
    while (es) {
        if (es->lexicalEnvironment()) {
            EnvironmentRecord* record = es->lexicalEnvironment()->record();
            if (record->isDeclarativeEnvironmentRecord() && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                return record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
            }
        } else if (es->isNativeFunctionObjectExecutionContext()) {
            return es->m_calledNativeFunctionObject;
        }

        es = es->parent();
    }

    return nullptr;
}

Optional<Script*> ExecutionState::resolveOuterScript()
{
    // get outer script through lexical environment chain
    ExecutionState* es = this->parent();
    while (es) {
        if (es->lexicalEnvironment()) {
            EnvironmentRecord* record = es->lexicalEnvironment()->record();
            if (record->isDeclarativeEnvironmentRecord()) {
                DeclarativeEnvironmentRecord* declarativeRecord = record->asDeclarativeEnvironmentRecord();
                if (declarativeRecord->isFunctionEnvironmentRecord()) {
                    return declarativeRecord->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->asInterpretedCodeBlock()->script();
                } else if (declarativeRecord->isModuleEnvironmentRecord()) {
                    return declarativeRecord->asModuleEnvironmentRecord()->script();
                }
            } else if (record->isGlobalEnvironmentRecord()) {
                return record->asGlobalEnvironmentRecord()->globalCodeBlock()->script();
            }
        }

        es = es->parent();
    }

    return nullptr;
}

LexicalEnvironment* ExecutionState::mostNearestFunctionLexicalEnvironment()
{
    ASSERT(resolveCallee() && !resolveCallee()->isNativeFunctionObject());

    ExecutionState* es = this;
    while (true) {
        if (es->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && es->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            break;
        }
        es = es->parent();
    }
    return es->lexicalEnvironment();
}

Optional<LexicalEnvironment*> ExecutionState::mostNearestHeapAllocatedLexicalEnvironment()
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

Optional<Object*> ExecutionState::mostNearestHomeObject()
{
    LexicalEnvironment* env = m_lexicalEnvironment;

    while (env) {
        auto rec = env->record();
        if (rec->isDeclarativeEnvironmentRecord() && rec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            auto homeObject = rec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->homeObject();
            if (homeObject) {
                return homeObject;
            }
        }
        env = env->outerEnvironment();
    }
    return nullptr;
}

Object* ExecutionState::convertHomeObjectIntoPrivateMemberContextObject(Object* o)
{
    if (o->isScriptClassConstructorPrototypeObject()) {
        o = o->asScriptClassConstructorPrototypeObject()->constructor();
    }
    return o;
}

Object* ExecutionState::findPrivateMemberContextObject()
{
    auto o = mostNearestHomeObject();
    if (!o) {
        ErrorObject::throwBuiltinError(*this, ErrorCode::TypeError, "Cannot read/write private member here");
        return nullptr;
    }
    return convertHomeObjectIntoPrivateMemberContextObject(o.value());
}

Object* ExecutionState::getNewTarget()
{
    EnvironmentRecord* envRec = getThisEnvironment();

    if (UNLIKELY(!envRec->isDeclarativeEnvironmentRecord())) {
        return nullptr;
    }

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

Value ExecutionState::thisValue()
{
    LexicalEnvironment* lex = m_lexicalEnvironment;
    while (lex) {
        EnvironmentRecord* envRec = lex->record();
        if (envRec->hasThisBinding()) {
            if (envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                auto fnRecord = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                ScriptFunctionObject* f = fnRecord->functionObject();
                if (f->codeBlock()->isInterpretedCodeBlock() && f->codeBlock()->asInterpretedCodeBlock()->needsToLoadThisBindingFromEnvironment()) {
                    return fnRecord->getThisBinding(*this);
                } else {
                    return f;
                }
            } else if (envRec->isGlobalEnvironmentRecord()) {
                if (envRec->asGlobalEnvironmentRecord()->globalCodeBlock()->isStrict()) {
                    return Value();
                } else {
                    return envRec->asGlobalEnvironmentRecord()->globalObject();
                }
            } else if (envRec->isModuleEnvironmentRecord()) {
                return Value();
            }
        }

        lex = lex->outerEnvironment();
    }
    return m_context->globalObject();
}

Value ExecutionState::makeSuperPropertyReference()
{
    // Let env be GetThisEnvironment( ).
    EnvironmentRecord* env = getThisEnvironment();

    // If env.HasSuperBinding() is false, throw a ReferenceError exception.
    if (!env->hasSuperBinding()) {
        ErrorObject::throwBuiltinError(*this, ErrorCode::ReferenceError, ErrorObject::Messages::No_Super_Binding);
    }

    // Let actualThis be env.GetThisBinding().
    // ReturnIfAbrupt(actualThis).
    // auto actualThis = env->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(*this);

    // Let baseValue be env.GetSuperBase().
    auto baseValue = env->getSuperBase(*this);
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
        ErrorObject::throwBuiltinError(*this, ErrorCode::TypeError, ErrorObject::Messages::No_Super_Binding);
    }

    // Return superConstructor.
    return superConstructor;
}

ExecutionPauser* ExecutionState::executionPauser()
{
    ExecutionState* p = this;
    while (true) {
        auto ps = p->pauseSource();
        if (ps) {
            return ps.value();
        }
        p = p->parent();
    }
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
            if (record->isGlobalEnvironmentRecord() || record->isModuleEnvironmentRecord()) {
                // class variable initializer can call {GlobalEnvironment, ModuleEnvironment}
                // so we should check above of {GlobalEnvironment, ModuleEnvironment}
                if (state->hasRareData() && state->rareData()->m_pauseSource) {
                    return true;
                }
            } else if (record->isDeclarativeEnvironmentRecord() && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                return record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptGeneratorFunctionObject()
                    || record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptAsyncFunctionObject()
                    || record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptAsyncGeneratorFunctionObject();
            }
        }
        state = state->parent();
    }

    return false;
}
} // namespace Escargot
