/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __Escargot_EnvironmentRecord_h
#define __Escargot_EnvironmentRecord_h

#include "parser/CodeBlock.h"
#include "runtime/AtomicString.h"
#include "runtime/FunctionObject.h"
#include "runtime/Object.h"
#include "runtime/ArgumentsObject.h"
#include "runtime/String.h"
#include "runtime/Value.h"

namespace Escargot {

class GlobalEnvironmentRecord;
class DeclarativeEnvironmentRecord;
class ObjectEnvironmentRecord;
class FunctionEnvironmentRecord;
class GlobalObject;
class CodeBlock;
class ArgumentsObject;

struct IdentifierRecord {
    AtomicString m_name;
    bool m_canDelete;
};

typedef Vector<IdentifierRecord, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<IdentifierRecord>> IdentifierRecordVector;

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-environment-records
class EnvironmentRecord : public gc {
protected:
    EnvironmentRecord()
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

    ObjectEnvironmentRecord* asObjectEnvironmentRecord()
    {
        ASSERT(isObjectEnvironmentRecord());
        return reinterpret_cast<ObjectEnvironmentRecord*>(this);
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
    ObjectEnvironmentRecord(Object* O)
        : EnvironmentRecord()
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
    GlobalEnvironmentRecord(ExecutionState& state, CodeBlock* codeBlock, GlobalObject* global, bool isEvalMode = false);
    ~GlobalEnvironmentRecord() {}
    virtual bool isGlobalEnvironmentRecord()
    {
        return true;
    }

    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);
    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name);
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName);
    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v);

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
    DeclarativeEnvironmentRecord()
        : EnvironmentRecord()
    {
    }

    ~DeclarativeEnvironmentRecord()
    {
    }

    virtual bool isDeclarativeEnvironmentRecord()
    {
        return true;
    }

    virtual bool isFunctionEnvironmentRecord()
    {
        return false;
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexed()
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
// DeclarativeEnvironmentRecordNotIndexed record does not create binding self likes FunctionEnvironmentRecord
// this record is for catch statement and strict eval now
class DeclarativeEnvironmentRecordNotIndexed : public DeclarativeEnvironmentRecord {
public:
    DeclarativeEnvironmentRecordNotIndexed()
        : DeclarativeEnvironmentRecord()
        , m_heapStorage()
    {
    }

    // this constructor is for strict eval
    DeclarativeEnvironmentRecordNotIndexed(ExecutionState& state, const CodeBlock::IdentifierInfoVector& vec)
        : DeclarativeEnvironmentRecord()
        , m_heapStorage()
    {
        for (size_t i = 0; i < vec.size(); i++) {
            createMutableBinding(state, vec[i].m_name, false);
        }
    }

    ~DeclarativeEnvironmentRecordNotIndexed()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexed()
    {
        return true;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        for (size_t i = 0; i < m_recordVector.size(); i++) {
            if (m_recordVector[i].m_name == atomicName) {
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
        m_heapStorage[idx] = v;
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        // Currently 'canDelete' is always false in DeclarativeEnvironmentRecordNotIndexed::createMutableBinding
        return false;
    }

protected:
    SmallValueVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-function-environment-records
class FunctionEnvironmentRecord : public DeclarativeEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class FunctionObject;
    friend class ByteCodeInterpreter;

public:
    ALWAYS_INLINE FunctionEnvironmentRecord(FunctionObject* function)
        : DeclarativeEnvironmentRecord()
        , m_functionObject(function)
    {
    }

    virtual bool isFunctionEnvironmentRecord()
    {
        return true;
    }

    virtual bool isFunctionEnvironmentRecordOnHeap()
    {
        return false;
    }

    virtual bool isFunctionEnvironmentRecordNotIndexed()
    {
        return false;
    }

    virtual Value getHeapValueByIndex(const size_t& idx)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    FunctionObject* functionObject()
    {
        return m_functionObject;
    }

    virtual size_t argc()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value* argv()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value createArgumentsObject(ExecutionState& state, ExecutionContext* ec)
    {
        return new ArgumentsObject(state, this, ec);
    }

protected:
    FunctionObject* m_functionObject;
};

class FunctionEnvironmentRecordSimple : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    ALWAYS_INLINE FunctionEnvironmentRecordSimple(FunctionObject* function)
        : FunctionEnvironmentRecord(function)
    {
    }
};

class FunctionEnvironmentRecordOnHeap : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class ByteCodeInterpreter;

public:
    FunctionEnvironmentRecordOnHeap(FunctionObject* function, size_t argc, Value* argv);

    virtual bool isFunctionEnvironmentRecordOnHeap()
    {
        return true;
    }

    virtual void setHeapValueByIndex(const size_t& idx, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(const size_t& idx)
    {
        return m_heapStorage[idx];
    }

    virtual size_t argc()
    {
        return m_argc;
    }

    virtual Value* argv()
    {
        return m_argv;
    }

protected:
    size_t m_argc;
    Value* m_argv;
    SmallValueVector m_heapStorage;
};

class FunctionEnvironmentRecordNotIndexed : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    FunctionEnvironmentRecordNotIndexed(FunctionObject* function, size_t argc, Value* argv);

    virtual bool isFunctionEnvironmentRecordNotIndexed()
    {
        return true;
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t& idx, const AtomicString& name, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

    virtual void setHeapValueByIndex(const size_t& idx, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(const size_t& idx)
    {
        return m_heapStorage[idx];
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        size_t len = m_recordVector.size();
        for (size_t i = 0; i < len; i++) {
            if (m_recordVector[i].m_name == atomicName) {
                if (!m_recordVector[i].m_canDelete) {
                    return false;
                }
                m_recordVector.erase(i);
                m_heapStorage.erase(i);
                return true;
            }
        }
        return false;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        size_t len = m_recordVector.size();
        for (size_t i = 0; i < len; i++) {
            if (m_recordVector[i].m_name == atomicName) {
                return BindingSlot(this, i);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }


    virtual void createMutableBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false);
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name);
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V);

    virtual size_t argc()
    {
        return m_argc;
    }

    virtual Value* argv()
    {
        return m_argv;
    }

protected:
    size_t m_argc;
    Value* m_argv;
    SmallValueVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};
}
#endif
