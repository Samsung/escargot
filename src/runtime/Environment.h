#ifndef __Escargot_Environment_h
#define __Escargot_Environment_h

#include "runtime/AtomicString.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

class GlobalObject;

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-lexical-environments
class LexicalEnvironment : public gc {
public:
    LexicalEnvironment(EnvironmentRecord* record, LexicalEnvironment* outerEnvironment)
        : m_record(record)
        , m_outerEnvironment(outerEnvironment)
    {
    }
    EnvironmentRecord* record()
    {
        return m_record;
    }

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        LexicalEnvironment* env = this;
        while (env) {
            if (LIKELY(env->record()->hasBinding(state, name).m_index != SIZE_MAX)) {
                return env->record()->deleteBinding(state, name);
            }
            env = env->outerEnvironment();
        }
        return true;
    }

protected:
    EnvironmentRecord* m_record;
    LexicalEnvironment* m_outerEnvironment;
};
}
#endif
