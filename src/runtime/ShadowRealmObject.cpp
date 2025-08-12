#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/ShadowRealmObject.h"

#if defined(ESCARGOT_ENABLE_SHADOWREALM)
namespace Escargot {

ShadowRealmObject::ShadowRealmObject(ExecutionState& state)
    : DerivedObject(state)
{
}

ShadowRealmObject::ShadowRealmObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

} // namespace Escargot
#endif
