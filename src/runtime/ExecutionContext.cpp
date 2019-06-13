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
#include "ExecutionContext.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "FunctionObject.h"

namespace Escargot {

FunctionObject* ExecutionContext::resolveCallee()
{
    LexicalEnvironment* env = m_lexicalEnvironment;
    while (env) {
        if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            return env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
        }
        env = env->outerEnvironment();
    }
    return nullptr;
}

Value ExecutionContext::getNewTarget()
{
    EnvironmentRecord* envRec = getThisEnvironment();

    ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    FunctionEnvironmentRecord* funcEnvRec = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();

    return funcEnvRec->newTarget() ? funcEnvRec->newTarget() : Value();
}

EnvironmentRecord* ExecutionContext::getThisEnvironment()
{
    LexicalEnvironment* lex = m_lexicalEnvironment;
    LexicalEnvironment* prevLex = nullptr;

    while (true) {
        EnvironmentRecord* envRec = lex->record();

        if (envRec->hasThisBinding()) {
            return envRec;
        }

        lex = lex->outerEnvironment();
    }
}

Value ExecutionContext::makeSuperPropertyReference(ExecutionState& state)
{
    EnvironmentRecord* envRec = getThisEnvironment();

    if (!envRec->hasSuperBinding()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, errorMessage_No_Super_Binding);
    }

    ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    FunctionEnvironmentRecord* funcEnvRec = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
    Value bv = funcEnvRec->getSuperBase(state).toObject(state);

    return bv;
}

Value ExecutionContext::getSuperConstructor(ExecutionState& state)
{
    EnvironmentRecord* envRec = getThisEnvironment();

    ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());

    FunctionEnvironmentRecord* funcEnvRec = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();

    Value activeFunction = funcEnvRec->functionObject() ? funcEnvRec->functionObject() : Value();

    Value superConstructor = activeFunction.asObject()->getPrototype(state);

    if (superConstructor.isConstructor() == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_No_Super_Binding);
    }

    return superConstructor;
}
}
