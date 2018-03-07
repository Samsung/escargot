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
}
