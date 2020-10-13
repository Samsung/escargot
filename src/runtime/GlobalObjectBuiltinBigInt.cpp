/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "BigIntObject.h"
#include "NativeFunctionObject.h"

namespace Escargot {

Value builtinBigIntConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is not undefined, throw a TypeError exception.
    if (newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "illegal constructor BigInt");
    }
    // Return a new unique Symbol value whose [[Description]] value is descString.
    return new BigInt(state.context()->vmInstance(), 0);
}

void GlobalObject::installBigInt(ExecutionState& state)
{
    m_bigInt = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().BigInt, builtinBigIntConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_bigInt->setGlobalIntrinsicObject(state);

    m_bigIntPrototype = new Object(state);
    m_bigIntPrototype->setGlobalIntrinsicObject(state, true);

    m_bigInt->setFunctionPrototype(state, m_bigIntPrototype);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().BigInt),
                      ObjectPropertyDescriptor(m_bigInt, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
