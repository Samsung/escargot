/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
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
    bool m_canDelete : 1;
    bool m_isMutable : 1;
    bool m_isVarDeclaration : 1;
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

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool canAccessWithoutinitializing = true)
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

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t idx, const Value& v)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    struct GetBindingValueResult {
        bool m_hasBindingValue : 1;
        Value m_value;
        GetBindingValueResult()
            : m_hasBindingValue(false)
            , m_value(Value(Value::ForceUninitialized))
        {
        }

        explicit GetBindingValueResult(const Value& v)
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

    virtual Value getBindingValue(ExecutionState& state, const size_t idx)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual bool hasSuperBinding()
    {
        return false;
    }

    virtual bool hasThisBinding()
    {
        return false;
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

    virtual bool isEvalTarget()
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
    explicit ObjectEnvironmentRecord(Object* O)
        : EnvironmentRecord()
        , m_bindingObject(O)
    {
    }
    ~ObjectEnvironmentRecord() {}
    Object* bindingObject()
    {
        return m_bindingObject;
    }

    virtual bool isObjectEnvironmentRecord() override
    {
        return true;
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool canAccessWithoutinitializing = true) override
    {
        ASSERT(canAccessWithoutinitializing);
        auto desc = m_bindingObject->getOwnProperty(state, name);
        if (!desc.hasValue()) {
            ObjectPropertyDescriptor::PresentAttribute attribute = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent);
            if (canDelete)
                attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::ConfigurablePresent);
            m_bindingObject->defineOwnPropertyThrowsExceptionWhenStrictMode(state, name, ObjectPropertyDescriptor(Value(), attribute));
        }
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        m_bindingObject->defineOwnPropertyThrowsExceptionWhenStrictMode(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        auto result = m_bindingObject->get(state, ObjectPropertyName(name));
        if (result.hasValue()) {
            return GetBindingValueResult(result.value(state, m_bindingObject));
        } else {
            return GetBindingValueResult();
        }
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override
    {
        auto result = m_bindingObject->get(state, ObjectPropertyName(atomicName));
        if (result.hasValue()) {
            return BindingSlot(this, SIZE_MAX - 1);
        } else {
            return BindingSlot(this, SIZE_MAX);
        }
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override
    {
        m_bindingObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(name), v, m_bindingObject);
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        m_bindingObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(name), V, m_bindingObject);
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        return m_bindingObject->deleteOwnProperty(state, name);
    }

private:
    Object* m_bindingObject;
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-global-environment-records
class GlobalEnvironmentRecord : public EnvironmentRecord {
public:
    GlobalEnvironmentRecord(ExecutionState& state, InterpretedCodeBlock* codeBlock, GlobalObject* global, bool isEvalMode = false, bool createBinding = true);
    ~GlobalEnvironmentRecord() {}
    virtual bool isGlobalEnvironmentRecord() override
    {
        return true;
    }

    virtual bool isEvalTarget() override
    {
        return true;
    }

    virtual bool hasThisBinding() override
    {
        return true;
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool canAccessWithoutinitializing = true) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override;
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override;
    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

    CodeBlock* globalCodeBlock()
    {
        return m_globalCodeBlock;
    }

private:
    InterpretedCodeBlock* m_globalCodeBlock;
    GlobalObject* m_globalObject;
};

class DeclarativeEnvironmentRecordNotIndexed;
class DeclarativeEnvironmentRecordIndexed;

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

    virtual bool isDeclarativeEnvironmentRecord() override
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

    virtual bool isDeclarativeEnvironmentRecordIndexed()
    {
        return false;
    }

    FunctionEnvironmentRecord* asFunctionEnvironmentRecord()
    {
        ASSERT(isFunctionEnvironmentRecord());
        return reinterpret_cast<FunctionEnvironmentRecord*>(this);
    }

    DeclarativeEnvironmentRecordNotIndexed* asDeclarativeEnvironmentRecordNotIndexed()
    {
        ASSERT(isDeclarativeEnvironmentRecordNotIndexed());
        return reinterpret_cast<DeclarativeEnvironmentRecordNotIndexed*>(this);
    }

    DeclarativeEnvironmentRecordIndexed* asDeclarativeEnvironmentRecordIndexed()
    {
        ASSERT(isDeclarativeEnvironmentRecordIndexed());
        return reinterpret_cast<DeclarativeEnvironmentRecordIndexed*>(this);
    }

    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
};

class DeclarativeEnvironmentRecordIndexed : public DeclarativeEnvironmentRecord {
public:
    DeclarativeEnvironmentRecordIndexed(InterpretedCodeBlock::BlockInfo* blockInfo)
        : DeclarativeEnvironmentRecord()
        , m_blockInfo(blockInfo)
        , m_heapStorage()
    {
        const auto& v = m_blockInfo->m_identifiers;

        size_t cnt = 0;
        size_t siz = v.size();
        for (size_t i = 0; i < siz; i++) {
            if (!v[i].m_needToAllocateOnStack) {
                cnt++;
            }
        }
        m_heapStorage.resize(cnt, SmallValue(Value(Value::EmptyValue)));
    }

    ~DeclarativeEnvironmentRecordIndexed()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordIndexed() override
    {
        return true;
    }

    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        if (UNLIKELY(m_heapStorage[idx].isEmpty())) {
            throwReferenceError(state, idx);
        }
        m_heapStorage[idx] = v;
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = m_blockInfo->m_identifiers;

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return GetBindingValueResult(DeclarativeEnvironmentRecordIndexed::getHeapValueByIndex(state, v[i].m_indexForIndexedStorage));
            }
        }
        return GetBindingValueResult();
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = m_blockInfo->m_identifiers;
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return BindingSlot(this, v[i].m_indexForIndexedStorage);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override
    {
        if (UNLIKELY(m_heapStorage[slot.m_index].isEmpty())) {
            throwReferenceError(state, slot.m_index);
        }
        m_heapStorage[slot.m_index] = v;
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        if (UNLIKELY(m_heapStorage[idx].isEmpty())) {
            throwReferenceError(state, idx);
        }
        m_heapStorage[idx] = v;
    }

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx) override
    {
        auto v = m_heapStorage[idx];
        if (UNLIKELY(v.isEmpty())) {
            throwReferenceError(state, idx);
        }
        return v;
    }

    void throwReferenceError(ExecutionState& state, const size_t idx)
    {
        const auto& v = m_blockInfo->m_identifiers;
        size_t cnt = 0;
        for (size_t i = 0; i < v.size(); i++) {
            if (!v[i].m_needToAllocateOnStack) {
                if (cnt == idx) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, v[i].m_name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
                }
                cnt++;
            }
        }
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        const auto& v = m_blockInfo->m_identifiers;

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                DeclarativeEnvironmentRecordIndexed::setMutableBindingByIndex(state, v[i].m_indexForIndexedStorage, V);
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    InterpretedCodeBlock::BlockInfo* m_blockInfo;
    SmallValueVector m_heapStorage;
};

// NOTE
// DeclarativeEnvironmentRecordNotIndexed record does not create binding self likes FunctionEnvironmentRecord
// this record is for strict eval now
class DeclarativeEnvironmentRecordNotIndexed : public DeclarativeEnvironmentRecord {
public:
    DeclarativeEnvironmentRecordNotIndexed()
        : DeclarativeEnvironmentRecord()
        , m_heapStorage()
    {
    }

    DeclarativeEnvironmentRecordNotIndexed(EnvironmentRecord* env)
        : DeclarativeEnvironmentRecord()
    {
        DeclarativeEnvironmentRecordNotIndexed* declEnv = (DeclarativeEnvironmentRecordNotIndexed*)env->asDeclarativeEnvironmentRecord();
        m_heapStorage = declEnv->m_heapStorage;
        m_recordVector = declEnv->m_recordVector;
    }

    // this constructor is for strict eval
    DeclarativeEnvironmentRecordNotIndexed(ExecutionState& state, const InterpretedCodeBlock::IdentifierInfoVector& vec)
        : DeclarativeEnvironmentRecord()
        , m_heapStorage()
    {
        for (size_t i = 0; i < vec.size(); i++) {
            createBinding(state, vec[i].m_name, false, vec[i].m_isMutable, true);
        }
    }

    ~DeclarativeEnvironmentRecordNotIndexed()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexed() override
    {
        return true;
    }

    virtual bool isEvalTarget() override
    {
        return true;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override
    {
        for (size_t i = 0; i < m_recordVector.size(); i++) {
            if (m_recordVector[i].m_name == atomicName) {
                return BindingSlot(this, i);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool canAccessWithoutinitializing = true) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        // Currently 'canDelete' is always false in DeclarativeEnvironmentRecordNotIndexed::createBinding
        return false;
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

private:
    SmallValueVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};

// NOTE
// this record is for catch statement
class DeclarativeEnvironmentRecordNotIndexedForCatch : public DeclarativeEnvironmentRecordNotIndexed {
public:
    DeclarativeEnvironmentRecordNotIndexedForCatch()
        : DeclarativeEnvironmentRecordNotIndexed()
    {
    }

    virtual bool isEvalTarget() override
    {
        return false;
    }

protected:
};

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-function-environment-records
class FunctionEnvironmentRecord : public DeclarativeEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class FunctionObject;
    friend class ByteCodeInterpreter;

public:
    enum ThisBindingStatus {
        Lexical,
        Initialized,
        Uninitialized,
    };

    ALWAYS_INLINE explicit FunctionEnvironmentRecord(FunctionObject* function)
        : DeclarativeEnvironmentRecord()
        , m_functionObject(function)
        , m_newTarget()
        , m_thisValue()
    {
        if (function->thisMode() == FunctionObject::ThisMode::Lexical) {
            m_thisBindingStatus = ThisBindingStatus::Lexical;
        } else {
            m_thisBindingStatus = ThisBindingStatus::Uninitialized;
        }
    }

    virtual bool isFunctionEnvironmentRecord() override
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

    void bindThisValue(ExecutionState& state, Value thisValue)
    {
        ASSERT(m_thisBindingStatus != ThisBindingStatus::Lexical);

        if (m_thisBindingStatus == ThisBindingStatus::Initialized) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, errorMessage_Initialized_This_Binding);
        }

        m_thisValue = thisValue;

        m_thisBindingStatus = ThisBindingStatus::Initialized;
    }

    Value getThisBinding(ExecutionState& state)
    {
        ASSERT(m_thisBindingStatus != ThisBindingStatus::Lexical);

        if (m_thisBindingStatus == ThisBindingStatus::Uninitialized) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, errorMessage_UnInitialized_This_Binding);
        }

        return m_thisValue;
    }

    virtual bool hasSuperBinding() override
    {
        if (m_thisBindingStatus == ThisBindingStatus::Lexical) {
            return false;
        }

        return homeObject() != nullptr;
    }

    virtual bool hasThisBinding() override
    {
        return m_thisBindingStatus != ThisBindingStatus::Lexical;
    }

    Value getSuperBase(ExecutionState& state)
    {
        if (homeObject() == nullptr) {
            return Value();
        }

        return homeObject()->getPrototype(state);
    }

    FunctionObject* functionObject()
    {
        return m_functionObject;
    }

    Object* homeObject()
    {
        return m_functionObject->homeObject();
    }

    Value newTarget()
    {
        return m_newTarget;
    }

    void setNewTarget(const Value& newTarget)
    {
        m_newTarget = newTarget;
    }

    virtual size_t argc()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value* argv()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value createArgumentsObject(ExecutionState& state)
    {
        return new ArgumentsObject(state, this);
    }

protected:
    FunctionObject* m_functionObject;
    SmallValue m_newTarget;
    ThisBindingStatus m_thisBindingStatus;
    SmallValue m_thisValue;
};

class FunctionEnvironmentRecordSimple : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    ALWAYS_INLINE explicit FunctionEnvironmentRecordSimple(FunctionObject* function)
        : FunctionEnvironmentRecord(function)
    {
    }
};

class FunctionEnvironmentRecordOnHeap : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class ByteCodeInterpreter;
    friend class FunctionObject;

public:
    FunctionEnvironmentRecordOnHeap(FunctionObject* function, size_t argc, Value* argv);

    virtual bool isFunctionEnvironmentRecordOnHeap() override
    {
        return true;
    }

    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_heapStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx) override
    {
        return m_heapStorage[idx];
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = m_functionObject->codeBlock()->asInterpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return GetBindingValueResult(m_heapStorage[v[i].m_indexForIndexedStorage]);
            }
        }
        return GetBindingValueResult();
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = m_functionObject->codeBlock()->asInterpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return BindingSlot(this, v[i].m_indexForIndexedStorage);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override
    {
        m_heapStorage[slot.m_index] = v;
    }

    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_heapStorage[idx] = v;
    }

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v)
    {
        m_heapStorage[idx] = v;
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        const auto& v = m_functionObject->codeBlock()->asInterpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                m_heapStorage[v[i].m_indexForIndexedStorage] = V;
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual size_t argc() override
    {
        return m_argc;
    }

    virtual Value* argv() override
    {
        return m_argv;
    }

private:
    size_t m_argc;
    Value* m_argv;
    SmallValueTightVector m_heapStorage;
};

class FunctionEnvironmentRecordNotIndexed : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;

public:
    FunctionEnvironmentRecordNotIndexed(FunctionObject* function, size_t argc, Value* argv);

    virtual bool isFunctionEnvironmentRecordNotIndexed() override
    {
        return true;
    }

    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_heapStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx) override
    {
        return m_heapStorage[idx];
    }

    virtual bool isEvalTarget() override
    {
        return true;
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& atomicName) override
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

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override
    {
        size_t len = m_recordVector.size();
        for (size_t i = 0; i < len; i++) {
            if (m_recordVector[i].m_name == atomicName) {
                return BindingSlot(this, i);
            }
        }
        return BindingSlot(this, SIZE_MAX);
    }


    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool canAccessWithoutinitializing = true) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;

    virtual size_t argc() override
    {
        return m_argc;
    }

    virtual Value* argv() override
    {
        return m_argv;
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

private:
    size_t m_argc;
    Value* m_argv;
    SmallValueTightVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};

class FunctionEnvironmentRecordNotIndexedWithVirtualID : public FunctionEnvironmentRecordNotIndexed {
public:
    FunctionEnvironmentRecordNotIndexedWithVirtualID(FunctionObject* function, size_t argc, Value* argv)
        : FunctionEnvironmentRecordNotIndexed(function, argc, argv)
    {
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
};
}
#endif
