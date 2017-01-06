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
DEFINE_FN(Uint16, uint16, 2);
DEFINE_FN(Uint32, uint32, 4);
DEFINE_FN(Float32, float32, 4);
DEFINE_FN(Float64, float64, 8);
}

#endif
