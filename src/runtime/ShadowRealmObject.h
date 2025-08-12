#ifndef __EscargotShadowRealmObject__
#define __EscargotShadowRealmObject__

#include "runtime/Object.h"
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"

#if defined(ESCARGOT_ENABLE_SHADOWREALM)
namespace Escargot {

class ShadowRealmObject : public DerivedObject {
public:
    explicit ShadowRealmObject(ExecutionState& state);
    explicit ShadowRealmObject(ExecutionState& state, Object* proto);

    Context* realmContext()
    {
        return m_realmContext;
    }

    void setRealmContext(Context* context)
    {
        m_realmContext = context;
    }

    virtual bool isShadowRealmObject() const override
    {
        return true;
    }

private:
    Context* m_realmContext;
};
} // namespace Escargot
#endif // defined(ESCARGOT_ENABLE_SHADOWREALM)
#endif
