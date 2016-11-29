#ifndef __Escargot_Environment_h
#define __Escargot_Environment_h

#include "runtime/Value.h"
#include "runtime/String.h"
#include "runtime/AtomicString.h"

namespace Escargot {

class GlobalObject;

struct IdentifierInfo {
    AtomicString m_name;
    enum Origin { Parameter, FunctionDeclaration, FunctionExpression, VariableDeclarator };
    struct {
        bool m_isHeapAllocated:1;
        bool m_bindingIsImmutable:1;
        Origin m_origin:2;
    } m_flags;

    IdentifierInfo(AtomicString name, Origin origin)
        : m_name(name)
    {
        m_flags.m_isHeapAllocated = false;
        m_flags.m_bindingIsImmutable = false;
        m_flags.m_origin = origin;
    }
};

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

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-newfunctionenvironment
    LexicalEnvironment* newFunctionEnvironment(FunctionObject* fn, Value newTarget);

protected:
    EnvironmentRecord* m_record;
    LexicalEnvironment* m_outerEnvironment;
};

}
#endif
