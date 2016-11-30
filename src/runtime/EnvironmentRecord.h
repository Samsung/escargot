#ifndef __Escargot_EnvironmentRecord_h
#define __Escargot_EnvironmentRecord_h

#include "runtime/Value.h"
#include "runtime/String.h"
#include "runtime/AtomicString.h"

namespace Escargot {

class GlobalEnvironmentRecord;
class DeclarativeEnvironmentRecord;
class GlobalObject;
class CodeBlock;

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-environment-records
class EnvironmentRecord : public gc {
protected:
    EnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock)
    {
    }
public:
    struct BindingSlot {
        MAKE_STACK_ALLOCATED();
        EnvironmentRecord* m_record;
        size_t m_index;
        BindingSlot(EnvironmentRecord* record, size_t idx)
            : m_record(record)
            , m_index(idx)
        {

        }
    };

    virtual ~EnvironmentRecord() { }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void createImmutableBinding(ExecutionState& state, const AtomicString& name)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    struct GetBindingValueResult {
        bool m_hasBindingValue;
        Value m_value;
        GetBindingValueResult()
            : m_hasBindingValue(false)
            , m_value(Value(Value::ForceUninitialized))
        {

        }

        GetBindingValueResult(const Value& v)
            : m_hasBindingValue(true)
            , m_value(v)
        {

        }

        GetBindingValueResult(bool has, const Value& v)
            : m_hasBindingValue(has)
            , m_value(v)
        {

        }
    };
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value getBindingValue(ExecutionState& state, const size_t& idx)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool hasSuperBinding()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value getThisBinding()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool hasThisBinding()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // WithBaseObject ()

    virtual bool isGlobalEnvironmentRecord()
    {
        return false;
    }

    virtual bool isObjectEnvironmentRecord()
    {
        return false;
    }

    virtual bool isDeclarativeEnvironmentRecord()
    {
        return false;
    }

    virtual bool isDeclarativeEnvironmentRecordForCatchClause()
    {
        return false;
    }

    GlobalEnvironmentRecord* asGlobalEnvironmentRecord()
    {
        ASSERT(isGlobalEnvironmentRecord());
        return reinterpret_cast<GlobalEnvironmentRecord*>(this);
    }

    DeclarativeEnvironmentRecord* toDeclarativeEnvironmentRecord()
    {
        ASSERT(isDeclarativeEnvironmentRecord());
        return reinterpret_cast<DeclarativeEnvironmentRecord*>(this);
    }

    inline void initInnerFunctionDeclarations(CodeBlock* codeBlock);
protected:
};

class ObjectEnvironmentRecord : public EnvironmentRecord {
public:
    ObjectEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, Object* O)
        : EnvironmentRecord(state, codeBlock)
        , m_bindingObject(O)
    {
    }
    ~ObjectEnvironmentRecord() { }

    Object* bindingObject()
    {
        return m_bindingObject;
    }

    virtual bool isObjectEnvironmentRecord()
    {
        return true;
    }

    virtual bool hasThisBinding()
    {
        return false;
    }

protected:
    Object* m_bindingObject;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-global-environment-records
class GlobalEnvironmentRecord : public EnvironmentRecord {
public:
    GlobalEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, GlobalObject* global);
    ~GlobalEnvironmentRecord() { }

    virtual bool isGlobalEnvironmentRecord()
    {
        return true;
    }

    virtual bool hasThisBinding()
    {
        return true;
    }

    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);
protected:
    GlobalObject* m_globalObject;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-declarative-environment-records
class DeclarativeEnvironmentRecord : public EnvironmentRecord {
public:
    DeclarativeEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock)
        : EnvironmentRecord(state, codeBlock)
    {
    }

    ~DeclarativeEnvironmentRecord()
    {
    }

    virtual bool isDeclarativeEnvironmentRecord()
    {
        return true;
    }

    virtual bool hasThisBinding()
    {
        return false;
    }
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-function-environment-records
class FunctionEnvironmentRecord : public DeclarativeEnvironmentRecord {
    friend class LexicalEnvironment;
public:
    FunctionEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock)
        : DeclarativeEnvironmentRecord(state, codeBlock)
    {
    }

    virtual bool hasThisBinding()
    {
        // we dont use arrow function now. so binding status is alwalys not lexical.
        return true;
    }
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-bindthisvalue
    // void bindThisValue(const ESValue& V);
    // ESValue getThisBinding();

protected:
    // ESValue m_thisValue;
    // ESFunctionObject* m_functionObject; //TODO
    // ESValue m_newTarget; //TODO
};
}
#endif
