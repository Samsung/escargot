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

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-lexical-environment-operations
    Value getThisBinding()
    {
        LexicalEnvironment* env = this;
        while (true) {
            if (LIKELY(env->record()->hasThisBinding())) {
                return env->record()->getThisBinding();
            }
            env = env->outerEnvironment();
        }
    }

protected:
    EnvironmentRecord* m_record;
    LexicalEnvironment* m_outerEnvironment;
};
}
#endif
