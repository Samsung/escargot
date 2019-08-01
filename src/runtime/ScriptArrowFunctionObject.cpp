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
#include "ScriptArrowFunctionObject.h"

#include "FunctionObjectInlines.h"

namespace Escargot {

class ScriptArrowFunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& state, Context* context, ScriptArrowFunctionObject* self, const Value& receiverSrc, bool isStrict)
    {
        return self->thisValue();
    }
};

Value ScriptArrowFunctionObject::call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
{
    return FunctionObjectProcessCallGenerator::processCall<ScriptArrowFunctionObject, false, ScriptArrowFunctionObjectThisValueBinder>(state, this, thisValue, argc, argv);
}
}
