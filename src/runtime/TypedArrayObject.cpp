/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "TypedArrayObject.h"
#include "Context.h"
#include "GlobalObject.h"

namespace Escargot {

#define DEFINE_FN(Type, type, siz)                                                                    \
    template <>                                                                                       \
    void TypedArrayObject<Type##Adaptor, siz>::typedArrayObjectPrototypeFiller(ExecutionState& state) \
    {                                                                                                 \
        setPrototype(state, state.context()->globalObject()->type##ArrayPrototype());                 \
    }                                                                                                 \
    template <>                                                                                       \
    const char* TypedArrayObject<Type##Adaptor, siz>::internalClassProperty()                         \
    {                                                                                                 \
        return #Type "Array";                                                                         \
    }

DEFINE_FN(Int8, int8, 1);
DEFINE_FN(Int16, int16, 2);
DEFINE_FN(Int32, int32, 4);
DEFINE_FN(Uint8, uint8, 1);
DEFINE_FN(Uint8Clamped, uint8Clamped, 1);
DEFINE_FN(Uint16, uint16, 2);
DEFINE_FN(Uint32, uint32, 4);
DEFINE_FN(Float32, float32, 4);
DEFINE_FN(Float64, float64, 8);
}
