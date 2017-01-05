#if ESCARGOT_ENABLE_TYPEDARRAY
#include "Escargot.h"
#include "ArrayBufferObject.h"
#include "Context.h"

namespace Escargot {

ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
    , m_data(nullptr)
    , m_bytelength(0)
{
    setPrototype(state, state.context()->globalObject()->arrayBufferPrototype());
}

void ArrayBufferObject::allocateBuffer(size_t bytelength)
{
    ASSERT(isDetachedBuffer());

    m_data = (uint8_t*)calloc(1, bytelength);
    m_bytelength = bytelength;

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj,
                                            void*) {
        ArrayBufferObject* self = (ArrayBufferObject*)obj;
        free(self->m_data);
    },
                                   nullptr, nullptr, nullptr);
}
}

#endif
