/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
#include "runtime/Object.h"
#include "runtime/FunctionObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ArgumentsObject.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

class GlobalObject;
class CodeBlock;
class ArgumentsObject;
class ModuleNamespaceObject;
class GlobalEnvironmentRecord;
class DeclarativeEnvironmentRecord;
class ObjectEnvironmentRecord;
class FunctionEnvironmentRecord;
class ModuleEnvironmentRecord;

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
        bool m_isLexicallyDeclared;
        BindingSlot(EnvironmentRecord* record, size_t idx, bool isLexicallyDeclared)
            : m_record(record)
            , m_index(idx)
            , m_isLexicallyDeclared(isLexicallyDeclared)
        {
        }
    };

    virtual ~EnvironmentRecord() {}
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName)
    {
        return BindingSlot(this, SIZE_MAX, false);
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool isVarDeclaration = true, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr)
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

    virtual Value getSuperBase(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Value getThisBinding(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
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

    virtual bool isModuleEnvironmentRecord()
    {
        return false;
    }

    virtual bool isVarDeclarationTarget()
    {
        return false;
    }

    virtual bool isAllocatedOnHeap()
    {
        return true;
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

    ModuleEnvironmentRecord* asModuleEnvironmentRecord()
    {
        ASSERT(isModuleEnvironmentRecord());
        return reinterpret_cast<ModuleEnvironmentRecord*>(this);
    }
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

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool isVarDeclaration = true, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr) override
    {
        ASSERT(isVarDeclaration);
        auto desc = m_bindingObject->getOwnProperty(state, name);
        if (!desc.hasValue()) {
            ObjectPropertyDescriptor::PresentAttribute attribute = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent);
            if (canDelete)
                attribute = (ObjectPropertyDescriptor::PresentAttribute)(attribute | ObjectPropertyDescriptor::ConfigurablePresent);
            m_bindingObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(Value(), attribute));
        }
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        m_bindingObject->defineOwnPropertyThrowsException(state, name, ObjectPropertyDescriptor(V, ObjectPropertyDescriptor::AllPresent));
    }

    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        // https://tc39.es/ecma262/#sec-object-environment-records-getbindingvalue-n-s
        ObjectPropertyName propertyName(name);
        auto result = m_bindingObject->hasProperty(state, propertyName);

        if (result) {
            // we check conditions same as hasBinding for performance reason
            // directly call getBindingValue instead of checking the result of hasBinding in advance
            Value unscopables = m_bindingObject->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().unscopables)).value(state, m_bindingObject);
            if (UNLIKELY(unscopables.isObject() && unscopables.asObject()->get(state, propertyName).value(state, unscopables).toBoolean(state))) {
                return GetBindingValueResult();
            }

            return GetBindingValueResult(result.value(state, propertyName, m_bindingObject));
        }

        return GetBindingValueResult();
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& name) override
    {
        // https://tc39.es/ecma262/#sec-object-environment-records-hasbinding-n
        ObjectPropertyName propertyName(name);
        auto result = m_bindingObject->hasProperty(state, propertyName);
        if (result) {
            // If envRec.[[IsWithEnvironment]] is false, return true. skipped
            // ObjectEnvironmentRecord is created only for with statement, so `IsWithEnvironment` is always true.
            Value unscopables = m_bindingObject->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().unscopables)).value(state, m_bindingObject);
            if (UNLIKELY(unscopables.isObject() && unscopables.asObject()->get(state, propertyName).value(state, unscopables).toBoolean(state))) {
                return BindingSlot(this, SIZE_MAX, false);
            }

            return BindingSlot(this, SIZE_MAX - 1, false);
        }

        return BindingSlot(this, SIZE_MAX, false);
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
#ifdef ESCARGOT_DEBUGGER
    friend class DebuggerRemote;
#endif /* ESCARGOT_DEBUGGER */
public:
    GlobalEnvironmentRecord(ExecutionState& state, InterpretedCodeBlock* codeBlock, GlobalObject* global, IdentifierRecordVector* globalDeclarativeRecord, EncodedValueVector* globalDeclarativeStorage);
    ~GlobalEnvironmentRecord() {}
    virtual bool isGlobalEnvironmentRecord() override
    {
        return true;
    }

    virtual bool isVarDeclarationTarget() override
    {
        return true;
    }

    virtual bool hasThisBinding() override
    {
        return true;
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool isVarDeclaration = true, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override;
    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override;
    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

    InterpretedCodeBlock* globalCodeBlock()
    {
        return m_globalCodeBlock;
    }

    GlobalObject* globalObject()
    {
        return m_globalObject;
    }

private:
    InterpretedCodeBlock* m_globalCodeBlock;
    GlobalObject* m_globalObject;
    IdentifierRecordVector* m_globalDeclarativeRecord;
    EncodedValueVector* m_globalDeclarativeStorage;
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
    DeclarativeEnvironmentRecordIndexed(ExecutionState& state, InterpretedCodeBlock::BlockInfo* blockInfo)
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
        m_heapStorage.resize(cnt, EncodedValueVectorElement(EncodedValueVectorElement::EmptyValue));
    }

    ~DeclarativeEnvironmentRecordIndexed()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordIndexed() override
    {
        return true;
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        return false;
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
                return BindingSlot(this, v[i].m_indexForIndexedStorage, true);
            }
        }
        return BindingSlot(this, SIZE_MAX, false);
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override
    {
        // TDZ check only (every indexed const variables are checked on bytecode generation time)
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

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
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
                    ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, v[i].m_name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
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

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        const auto& v = m_blockInfo->m_identifiers;

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                m_heapStorage[v[i].m_indexForIndexedStorage] = V;
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            GC_word objBitmap[GC_BITMAP_SIZE(DeclarativeEnvironmentRecordIndexed)] = { 0 };
            GC_set_bit(objBitmap, GC_WORD_OFFSET(DeclarativeEnvironmentRecordIndexed, m_blockInfo));
            GC_set_bit(objBitmap, GC_WORD_OFFSET(DeclarativeEnvironmentRecordIndexed, m_heapStorage));
            descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(DeclarativeEnvironmentRecordIndexed));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }

private:
    InterpretedCodeBlock::BlockInfo* m_blockInfo;
    EncodedValueVector m_heapStorage;
};

// NOTE
// DeclarativeEnvironmentRecordNotIndexed record does not create binding self likes FunctionEnvironmentRecord
class DeclarativeEnvironmentRecordNotIndexed : public DeclarativeEnvironmentRecord {
#ifdef ESCARGOT_DEBUGGER
    friend class DebuggerRemote;
#endif /* ESCARGOT_DEBUGGER */
public:
    DeclarativeEnvironmentRecordNotIndexed(ExecutionState& state, bool isVarDeclarationTarget = false, bool isCatchClause = false)
        : DeclarativeEnvironmentRecord()
        , m_isVarDeclarationTarget(isVarDeclarationTarget)
        , m_isCatchClause(isCatchClause)
    {
    }

    ~DeclarativeEnvironmentRecordNotIndexed()
    {
    }

    virtual bool isDeclarativeEnvironmentRecordNotIndexed() override
    {
        return true;
    }

    virtual bool isVarDeclarationTarget() override
    {
        return m_isVarDeclarationTarget;
    }

    bool isCatchClause()
    {
        return m_isCatchClause;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override
    {
        for (size_t i = 0; i < m_recordVector.size(); i++) {
            if (m_recordVector[i].m_name == atomicName) {
                return BindingSlot(this, i, !m_recordVector[i].m_isVarDeclaration);
            }
        }
        return BindingSlot(this, SIZE_MAX, false);
    }

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool isVarDeclaration = true, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        // Currently 'canDelete' is always false in DeclarativeEnvironmentRecordNotIndexed::createBinding
        return false;
    }

    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            GC_word objBitmap[GC_BITMAP_SIZE(DeclarativeEnvironmentRecordNotIndexed)] = { 0 };
            GC_set_bit(objBitmap, GC_WORD_OFFSET(DeclarativeEnvironmentRecordNotIndexed, m_heapStorage));
            GC_set_bit(objBitmap, GC_WORD_OFFSET(DeclarativeEnvironmentRecordNotIndexed, m_recordVector));
            descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(DeclarativeEnvironmentRecordNotIndexed));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }

private:
    bool m_isVarDeclarationTarget : 1;
    bool m_isCatchClause : 1;
    EncodedValueVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};

template <bool canBindThisValue, bool hasNewTarget>
struct FunctionEnvironmentRecordPiece;

template <>
struct FunctionEnvironmentRecordPiece<false, false> {
    FunctionEnvironmentRecordPiece() {}
    void bindThisValue(ExecutionState& state, const Value& thisValue)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value getThisBinding(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    }

    Object* newTarget()
    {
        return nullptr;
    }

    void setNewTarget(Object* newTarget)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
};

template <>
struct FunctionEnvironmentRecordPiece<true, false> {
    EncodedValue m_thisValue;

    FunctionEnvironmentRecordPiece()
        : m_thisValue(EncodedValue::EmptyValue)
    {
    }

    void bindThisValue(ExecutionState& state, const Value& thisValue)
    {
        if (!m_thisValue.isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, ErrorObject::Messages::Initialized_This_Binding);
        }

        m_thisValue = thisValue;
    }

    Value getThisBinding(ExecutionState& state)
    {
        if (m_thisValue.isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, ErrorObject::Messages::UnInitialized_This_Binding);
        }
        return m_thisValue;
    }

    Object* newTarget()
    {
        return nullptr;
    }

    void setNewTarget(Object* newTarget)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
};

template <>
struct FunctionEnvironmentRecordPiece<false, true> {
    Object* m_newTarget;

    FunctionEnvironmentRecordPiece()
        : m_newTarget(nullptr)
    {
    }

    void bindThisValue(ExecutionState& state, const Value& thisValue)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    Value getThisBinding(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    }

    Object* newTarget()
    {
        return m_newTarget;
    }

    void setNewTarget(Object* newTarget)
    {
        m_newTarget = newTarget;
    }
};

template <>
struct FunctionEnvironmentRecordPiece<true, true> {
    Object* m_newTarget;
    EncodedValue m_thisValue;

    FunctionEnvironmentRecordPiece()
        : m_newTarget(nullptr)
        , m_thisValue(EncodedValue::EmptyValue)
    {
    }

    void bindThisValue(ExecutionState& state, const Value& thisValue)
    {
        if (!m_thisValue.isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, ErrorObject::Messages::Initialized_This_Binding);
        }

        m_thisValue = thisValue;
    }

    Value getThisBinding(ExecutionState& state)
    {
        if (m_thisValue.isEmpty()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, ErrorObject::Messages::UnInitialized_This_Binding);
        }
        return m_thisValue;
    }

    Object* newTarget()
    {
        return m_newTarget;
    }

    void setNewTarget(Object* newTarget)
    {
        m_newTarget = newTarget;
    }
};

class FunctionEnvironmentRecord : public DeclarativeEnvironmentRecord {
    friend class ScriptFunctionObject;

public:
    FunctionEnvironmentRecord(ScriptFunctionObject* function)
        : DeclarativeEnvironmentRecord()
        , m_functionObject(function)
    {
    }

    ScriptFunctionObject::ThisMode thisMode()
    {
        return functionObject()->thisMode();
    }

    virtual bool isFunctionEnvironmentRecord() override
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

    Object* homeObject()
    {
        return functionObject()->homeObject();
    }

    Optional<ArgumentsObject*> argumentsObject()
    {
        if (m_argumentsObject->isArgumentsObject()) {
            return m_argumentsObject;
        }
        return nullptr;
    }

    ArgumentsObject* uncheckedArgumentsObject()
    {
        return m_argumentsObject;
    }

    ScriptFunctionObject* functionObject()
    {
        if (UNLIKELY(m_argumentsObject->isArgumentsObject())) {
            return m_argumentsObject->sourceFunctionObject();
        }
        return m_functionObject;
    }

    virtual bool hasSuperBinding() override
    {
        if (thisMode() == ScriptFunctionObject::ThisMode::Lexical) {
            return false;
        }

        return homeObject() != nullptr;
    }

    virtual bool hasThisBinding() override
    {
        return thisMode() != ScriptFunctionObject::ThisMode::Lexical;
    }

    virtual Value getSuperBase(ExecutionState& state) override
    {
        if (homeObject() == nullptr) {
            return Value();
        }

        return homeObject()->getPrototype(state);
    }

    virtual void bindThisValue(ExecutionState& state, const Value& thisValue)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual Object* newTarget()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual void setNewTarget(Object* newTarget)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

#ifdef ESCARGOT_DEBUGGER
    virtual IdentifierRecordVector* getRecordVector()
    {
        return NULL;
    }
#endif /* ESCARGOT_DEBUGGER */

private:
    // ArgumentsObject is constructed on EnsureArgumentsObject opcode
    union {
        ScriptFunctionObject* m_functionObject;
        ArgumentsObject* m_argumentsObject;
    };
};

template <bool canBindThisValue, bool hasNewTarget>
class FunctionEnvironmentRecordWithExtraData : public FunctionEnvironmentRecord {
    friend class LexicalEnvironment;
    friend class FunctionObject;
    friend class ByteCodeInterpreter;

public:
    ALWAYS_INLINE explicit FunctionEnvironmentRecordWithExtraData(ScriptFunctionObject* function)
        : FunctionEnvironmentRecord(function)
    {
    }

    void bindThisValue(ExecutionState& state, const Value& thisValue) override
    {
        m_piece.bindThisValue(state, thisValue);
    }

    Value getThisBinding(ExecutionState& state) override
    {
        return m_piece.getThisBinding(state);
    }

    Object* newTarget() override
    {
        return m_piece.newTarget();
    }

    void setNewTarget(Object* newTarget) override
    {
        m_piece.setNewTarget(newTarget);
    }

protected:
    FunctionEnvironmentRecordPiece<canBindThisValue, hasNewTarget> m_piece;
};

template <bool canBindThisValue, bool hasNewTarget>
class FunctionEnvironmentRecordOnStack : public FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget> {
public:
    FunctionEnvironmentRecordOnStack(ScriptFunctionObject* function)
        : FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>(function)
    {
    }

    virtual bool isFunctionEnvironmentRecordOnStack() override
    {
        return true;
    }

    virtual bool isAllocatedOnHeap() override
    {
        return false;
    }
};

template <bool canBindThisValue, bool hasNewTarget>
class FunctionEnvironmentRecordOnHeap : public FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget> {
    friend class LexicalEnvironment;
    friend class ByteCodeInterpreter;
    friend class ScriptFunctionObject;

public:
    FunctionEnvironmentRecordOnHeap(ScriptFunctionObject* function);

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

    virtual EnvironmentRecord::GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return EnvironmentRecord::GetBindingValueResult(m_heapStorage[v[i].m_indexForIndexedStorage]);
            }
        }
        return EnvironmentRecord::GetBindingValueResult();
    }

    virtual EnvironmentRecord::BindingSlot hasBinding(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return EnvironmentRecord::BindingSlot(this, v[i].m_indexForIndexedStorage, false);
            }
        }
        return EnvironmentRecord::BindingSlot(this, SIZE_MAX, false);
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        return false;
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const EnvironmentRecord::BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_heapStorage[idx] = v;
    }

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_heapStorage[idx] = v;
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                m_heapStorage[v[i].m_indexForIndexedStorage] = V;
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    EncodedValueTightVector m_heapStorage;
};

template <bool canBindThisValue, bool hasNewTarget, size_t inlineStorageSize>
class FunctionEnvironmentRecordOnHeapWithInlineStorage : public FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget> {
    friend class LexicalEnvironment;
    friend class ByteCodeInterpreter;
    friend class ScriptFunctionObject;

public:
    FunctionEnvironmentRecordOnHeapWithInlineStorage(ScriptFunctionObject* function);

    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_inlineStorage[idx] = v;
    }

    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx) override
    {
        return m_inlineStorage[idx];
    }

    virtual EnvironmentRecord::GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return EnvironmentRecord::GetBindingValueResult(m_inlineStorage[v[i].m_indexForIndexedStorage]);
            }
        }
        return EnvironmentRecord::GetBindingValueResult();
    }

    virtual EnvironmentRecord::BindingSlot hasBinding(ExecutionState& state, const AtomicString& name) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                return EnvironmentRecord::BindingSlot(this, v[i].m_indexForIndexedStorage, false);
            }
        }
        return EnvironmentRecord::BindingSlot(this, SIZE_MAX, false);
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        return false;
    }

    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const EnvironmentRecord::BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_inlineStorage[idx] = v;
    }

    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override
    {
        m_inlineStorage[idx] = v;
    }

    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override
    {
        const auto& v = FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget>::functionObject()->interpretedCodeBlock()->identifierInfos();

        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_name == name) {
                m_inlineStorage[v[i].m_indexForIndexedStorage] = V;
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    EncodedValue m_inlineStorage[inlineStorageSize];
};

template <bool canBindThisValue, bool hasNewTarget>
class FunctionEnvironmentRecordNotIndexed : public FunctionEnvironmentRecordWithExtraData<canBindThisValue, hasNewTarget> {
    friend class LexicalEnvironment;

public:
    FunctionEnvironmentRecordNotIndexed(ScriptFunctionObject* function);

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

    virtual bool isVarDeclarationTarget() override
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

    virtual EnvironmentRecord::BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override
    {
        size_t len = m_recordVector.size();
        for (size_t i = 0; i < len; i++) {
            if (m_recordVector[i].m_name == atomicName) {
                return EnvironmentRecord::BindingSlot(this, i, false);
            }
        }
        return EnvironmentRecord::BindingSlot(this, SIZE_MAX, false);
    }

#ifdef ESCARGOT_DEBUGGER
    virtual IdentifierRecordVector* getRecordVector()
    {
        return &m_recordVector;
    }
#endif /* ESCARGOT_DEBUGGER */

    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete = false, bool isMutable = true, bool isVarDeclaration = true, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr) override;
    virtual EnvironmentRecord::GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const EnvironmentRecord::BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;

private:
    EncodedValueTightVector m_heapStorage;
    IdentifierRecordVector m_recordVector;
};

class FunctionEnvironmentRecordNotIndexedWithVirtualID : public FunctionEnvironmentRecordNotIndexed<true, true> {
public:
    FunctionEnvironmentRecordNotIndexedWithVirtualID(ScriptFunctionObject* function)
        : FunctionEnvironmentRecordNotIndexed(function)
    {
    }

    virtual EnvironmentRecord::GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
};

class ModuleEnvironmentRecord : public DeclarativeEnvironmentRecord {
#ifdef ESCARGOT_DEBUGGER
    friend class DebuggerRemote;
#endif /* ESCARGOT_DEBUGGER */
public:
    struct ModuleBindingRecord {
        bool m_isMutable;
        bool m_isVarDeclaration;
        AtomicString m_localName;
        EncodedValue m_value;
        ModuleEnvironmentRecord* m_targetRecord;
        AtomicString m_targetBindingName;
    };

    typedef Vector<ModuleBindingRecord, GCUtil::gc_malloc_allocator<ModuleBindingRecord>> ModuleBindingRecordVector;

    ModuleEnvironmentRecord(Script* script)
        : DeclarativeEnvironmentRecord()
        , m_script(script)
    {
    }
    virtual bool isModuleEnvironmentRecord() override
    {
        return true;
    }

    virtual bool isVarDeclarationTarget() override
    {
        return true;
    }

    virtual bool hasThisBinding() override
    {
        return true;
    }

    virtual bool deleteBinding(ExecutionState& state, const AtomicString& name) override
    {
        return false;
    }

    virtual BindingSlot hasBinding(ExecutionState& state, const AtomicString& atomicName) override;
    virtual void createBinding(ExecutionState& state, const AtomicString& name, bool canDelete, bool isMutable, bool isVarDeclaration, Optional<InterpretedCodeBlock*> relatedCodeBlock = nullptr) override;
    virtual void initializeBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBinding(ExecutionState& state, const AtomicString& name, const Value& V) override;
    virtual void setMutableBindingByBindingSlot(ExecutionState& state, const BindingSlot& slot, const AtomicString& name, const Value& v) override;
    virtual void setMutableBindingByIndex(ExecutionState& state, const size_t i, const Value& v) override;
    virtual void initializeBindingByIndex(ExecutionState& state, const size_t idx, const Value& v) override;
    virtual GetBindingValueResult getBindingValue(ExecutionState& state, const AtomicString& name) override;
    virtual Value getBindingValue(ExecutionState& state, const size_t i) override;
    virtual void setHeapValueByIndex(ExecutionState& state, const size_t idx, const Value& v) override;
    virtual Value getHeapValueByIndex(ExecutionState& state, const size_t idx) override;
    Script* script()
    {
        return m_script;
    }

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            GC_word objBitmap[GC_BITMAP_SIZE(ModuleEnvironmentRecord)] = { 0 };
            GC_set_bit(objBitmap, GC_WORD_OFFSET(ModuleEnvironmentRecord, m_moduleBindings));
            GC_set_bit(objBitmap, GC_WORD_OFFSET(ModuleEnvironmentRecord, m_script));
            descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ModuleEnvironmentRecord));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }

    void createImportBinding(ExecutionState& state, AtomicString localName, ModuleEnvironmentRecord* targetRecord, AtomicString targetBindingName);

protected:
    const ModuleBindingRecordVector& moduleBindings()
    {
        return m_moduleBindings;
    }

    void readCheck(ExecutionState& state, const size_t i)
    {
        if (UNLIKELY(m_moduleBindings[i].m_value.isEmpty())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, m_moduleBindings[i].m_localName.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
        }
    }

    void writeCheck(ExecutionState& state, const size_t i)
    {
        if (UNLIKELY(!m_moduleBindings[i].m_isMutable)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, m_moduleBindings[i].m_localName);
        }

        if (UNLIKELY(!m_moduleBindings[i].m_isVarDeclaration && m_moduleBindings[i].m_value.isEmpty())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, m_moduleBindings[i].m_localName.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
        }
    }

    ModuleBindingRecordVector m_moduleBindings;
    Script* m_script;
};
} // namespace Escargot
#endif
