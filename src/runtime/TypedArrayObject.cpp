/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if ESCARGOT_ENABLE_TYPEDARRAY
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

#endif
