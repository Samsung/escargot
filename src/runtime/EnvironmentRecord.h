#ifndef __Escargot_EnvironmentRecord_h
#define __Escargot_EnvironmentRecord_h

#include "parser/CodeBlock.h"
#include "runtime/AtomicString.h"
#include "runtime/FunctionObject.h"
#include "runtime/Object.h"
#include "runtime/String.h"
#include "runtime/Value.h"

namespace Escargot {

class GlobalEnvironmentRecord;
class DeclarativeEnvironmentRecord;
class FunctionEnvironmentRecord;
class GlobalObject;
class CodeBlock;

struct IdentifierRecord {
    AtomicString m_name;
    Value m_value;
};

typedef Vector<IdentifierRecord, gc_malloc_ignore_off_page_allocator<IdentifierRecord>> IdentifierRecordVector;

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

    virtual ~EnvironmentRecord() {}
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        return BindingSlot(this, SIZE_MAX);
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
    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v)
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
        return GetBindingValueResult();
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

    GlobalEnvironmentRecord* asGlobalEnvironmentRecord()
    {
        ASSERT(isGlobalEnvironmentRecord());
        return reinterpret_cast<GlobalEnvironmentRecord*>(this);
    }

    DeclarativeEnvironmentRecord* asDeclarativeEnvironmentRecord()
    {
        ASSERT(isDeclarativeEnvironmentRecord());
        return reinterpret_cast<DeclarativeEnvironmentRecord*>(this);
    }

protected:
};

class ObjectEnvironmentRecord : public EnvironmentRecord {
public:
    ObjectEnvironmentRecord(ExecutionState& state, Object* O)
        : EnvironmentRecord(state, nullptr)
        , m_bindingObject(O)
    {
    }
    ~ObjectEnvironmentRecord() {}
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

    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name)
    {
        auto result = m_bindingObject->get(state, ObjectPropertyName(name));
        if (result.hasValue()) {
            return GetBindingValueResult(result.value(state, m_bindingObject));
        } else {
            return GetBindingValueResult();
        }
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        auto result = m_bindingObject->get(state, ObjectPropertyName(atomicName));
        if (result.hasValue()) {
            return BindingSlot(this, SIZE_MAX - 1);
        } else {
            return BindingSlot(this, SIZE_MAX);
        }
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v)
    {
        m_bindingObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(name), v, m_bindingObject);
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V)
    {
        m_bindingObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(name), V, m_bindingObject);
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        return m_bindingObject->deleteOwnProperty(state, name);
    }

protected:
    Object* m_bindingObject;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-global-environment-records
class GlobalEnvironmentRecord : public EnvironmentRecord {
public:
    GlobalEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, GlobalObject* global);
    ~GlobalEnvironmentRecord() {}
    virtual bool isGlobalEnvironmentRecord()
    {
        return true;
    }

    virtual bool hasThisBinding()
    {
        return true;
    }

    virtual Value getThisBinding();
    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);
    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name);
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName);

    CodeBlock* globalCodeBlock()
    {
        return m_globalCodeBlock;
    }

protected:
    CodeBlock* m_globalCodeBlock;
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

    virtual bool isFunctionEnvironmentRecord()
    {
        return false;
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexded()
    {
        return false;
    }

    FunctionEnvironmentRecord* asFunctionEnvironmentRecord()
    {
        ASSERT(isFunctionEnvironmentRecord());
        return reinterpret_cast<FunctionEnvironmentRecord*>(this);
    }

    virtual void setHeapValueByIndex(const size_t& idx, const Value& v)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
};

// NOTE
// DeclarativeEnvironmentRecordNotIndexded record does not create binding self likes FunctionEnvironmentRecord
// this record is for catch statement now
class DeclarativeEnvironmentRecordNotIndexded : public DeclarativeEnvironmentRecord {
public:
    DeclarativeEnvironmentRecordNotIndexded(ExecutionState& state)
        : DeclarativeEnvironmentRecord(state, nullptr)
    {
    }

    ~DeclarativeEnvironmentRecordNotIndexded()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexded()
    {
        return true;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        for (size_t i = 0; i < m_vector.size(); i++) {
            if (m_vector[i].m_name == atomicName) {
                return BindingSlot(this, i);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }

    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v)
    {
        m_vector[idx].m_value = v;
    }

protected:
    IdentifierRecordVector m_vector;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-function-environment-records
class FunctionEnvironmentRecord : public DeclarativeEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    ALWAYS_INLINE FunctionEnvironmentRecord(ExecutionState& state, const Value& receiver, FunctionObject* function, size_t argc, Value* argv, bool isNewExpression)
        : DeclarativeEnvironmentRecord(state, function->codeBlock())
        , m_isNewExpression(isNewExpression)
        , m_thisValue(receiver)
        , m_functionObject(function)
        , m_argc(argc)
        , m_argv(argv)
    {
    }

    virtual bool hasThisBinding()
    {
        // we dont use arrow function now. so binding status is alwalys not lexical.
        return true;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-bindthisvalue
    // void bindThisValue(const ESValue& V);

    virtual bool isFunctionEnvironmentRecord()
    {
        return true;
    }

    virtual bool isFunctionEnvironmentRecordOnStack()
    {
        return false;
    }

    virtual bool isFunctionEnvironmentRecordOnHeap()
    {
        return false;
    }

    virtual bool isFunctionEnvironmentRecordNotIndexed()
    {
        return false;
    }

    virtual Value getThisBinding()
    {
        return m_thisValue;
    }

    bool isNewExpression()
    {
        return m_isNewExpression;
    }

    FunctionObject* functionObject()
    {
        return m_functionObject;
    }

    size_t argc()
    {
        return m_argc;
    }

    Value* argv()
    {
        return m_argv;
    }

protected:
    bool m_isNewExpression;
    Value m_thisValue;
    FunctionObject* m_functionObject;
    size_t m_argc;
    Value* m_argv;
    // ESValue m_newTarget; //TODO
};

class FunctionEnvironmentRecordOnStack : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    ALWAYS_INLINE FunctionEnvironmentRecordOnStack(ExecutionState& state, const Value& receiver, FunctionObject* function, size_t argc, Value* argv, bool isNewExpression)
        : FunctionEnvironmentRecord(state, receiver, function, argc, argv, isNewExpression)
    {
    }

    virtual bool isFunctionEnvironmentRecordOnStack()
    {
        return true;
    }
};

class FunctionEnvironmentRecordOnHeap : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class ByteCodeInterpreter;

public:
    ALWAYS_INLINE FunctionEnvironmentRecordOnHeap(ExecutionState& state, const Value& receiver, FunctionObject* function, size_t argc, Value* argv, bool isNewExpression)
        : FunctionEnvironmentRecord(state, receiver, function, argc, argv, isNewExpression)
        , m_heapStorage(function->codeBlock()->identifierOnHeapCount())
    {
    }

    virtual bool isFunctionEnvironmentRecordOnHeap()
    {
        return true;
    }

    virtual void setHeapValueByIndex(const size_t& idx, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

protected:
    ValueVector m_heapStorage;
};

class FunctionEnvironmentRecordNotIndexed : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    ALWAYS_INLINE FunctionEnvironmentRecordNotIndexed(ExecutionState& state, const Value& receiver, FunctionObject* function, size_t argc, Value* argv, bool isNewExpression)
        : FunctionEnvironmentRecord(state, receiver, function, argc, argv, isNewExpression)
    {
        const CodeBlock::IdentifierInfoVector& vec = function->codeBlock()->identifierInfos();
        size_t len = vec.size();
        for (size_t i = 0; i < len; i++) {
            createMutableBinding(state, vec[i].m_name, false);
        }
    }

    virtual bool isFunctionEnvironmentRecordNotIndexed()
    {
        return true;
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v)
    {
        m_vector[idx].m_value = v;
    }

    virtual void setHeapValueByIndex(const size_t& idx, const Value& v)
    {
        m_vector[idx].m_value = v;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        for (size_t i = 0; i < m_vector.size(); i++) {
            if (m_vector[i].m_name == atomicName) {
                return BindingSlot(this, i);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }


    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);

protected:
    IdentifierRecordVector m_vector;
};
}
#endif
