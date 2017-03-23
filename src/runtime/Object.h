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

#ifndef __EscargotObject__
#define __EscargotObject__

#include "runtime/ObjectStructure.h"
#include "runtime/PointerValue.h"
#include "runtime/SmallValue.h"
#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

class Object;
class FunctionObject;
class RegExpObject;
class ErrorObject;
#if ESCARGOT_ENABLE_TYPEDARRAY
class ArrayBufferObject;
class ArrayBufferView;
template <typename TypeArg, int elementSize>
class TypedArrayObject;
#endif

extern size_t g_objectRareDataTag;

struct ObjectRareData : public PointerValue {
    bool m_isExtensible : 1;
    bool m_isEverSetAsPrototypeObject : 1;
    bool m_isFastModeArrayObject : 1;
    bool m_isInArrayObjectDefineOwnProperty : 1;
    bool m_hasNonWritableLastIndexRegexpObject : 1;
    const char* m_internalClassName;
    void* m_extraData;
    Object* m_prototype;
#ifdef ESCARGOT_ENABLE_PROMISE
    Object* m_internalSlot;
#endif
    ObjectRareData(Object* obj);

    virtual Type type()
    {
        return ObjectRareDataType;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class ObjectPropertyName {
public:
    ObjectPropertyName(ExecutionState& state, const Value& v)
    {
        if (v.isUInt32()) {
            m_isUIntType = true;
            m_value.m_uint = v.asUInt32();
        } else {
            m_isUIntType = false;
            m_value.m_name = PropertyName(state, v.toString(state));
        }
    }

    ObjectPropertyName(const AtomicString& v)
    {
        m_isUIntType = false;
        m_value.m_name = v;
    }

    ObjectPropertyName(ExecutionState& state, const PropertyName& v)
    {
        m_isUIntType = false;
        m_value.m_name = v;
    }

    bool isUIntType() const
    {
        return m_isUIntType;
    }

    const PropertyName& propertyName() const
    {
        ASSERT(!isUIntType());
        return m_value.m_name;
    }

    const uint32_t& uintValue() const
    {
        ASSERT(isUIntType());
        return m_value.m_uint;
    }

    PropertyName toPropertyName(ExecutionState& state) const
    {
        if (isUIntType()) {
            return PropertyName(state, String::fromDouble(uintValue()));
        }
        return propertyName();
    }

    String* string(ExecutionState& state) const
    {
        return toPropertyName(state).string();
    }

    Value toValue(ExecutionState& state) const
    {
        if (isUIntType()) {
            return Value(uintValue());
        } else {
            return Value(string(state));
        }
    }

protected:
    bool m_isUIntType;
    union ObjectPropertyNameData {
        ObjectPropertyNameData() { m_uint = 0; }
        PropertyName m_name;
        uint32_t m_uint;
    } m_value;
};

class JSGetterSetter : public PointerValue {
    friend class ObjectPropertyDescriptor;
    friend class Object;

public:
    JSGetterSetter(Value getter, Value setter)
        : m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(getter.isEmpty() || getter.isFunction() || getter.isUndefined());
        ASSERT(setter.isEmpty() || setter.isFunction() || setter.isUndefined());
    }

    virtual Type type()
    {
        return JSGetterSetterType;
    }

    virtual bool isJSGetterSetter() const
    {
        return true;
    }

    bool hasGetter() const
    {
        return !m_getter.isEmpty();
    }

    bool hasSetter() const
    {
        return !m_setter.isEmpty();
    }

    Value getter() const
    {
        ASSERT(hasGetter());
        return m_getter;
    }

    Value setter() const
    {
        ASSERT(hasSetter());
        return m_setter;
    }

protected:
    Value m_getter;
    Value m_setter;
};

class ObjectPropertyDescriptor {
    friend class Object;

public:
    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1,
        EnumerablePresent = 1 << 2,
        ConfigurablePresent = 1 << 3,
        NonWritablePresent = 1 << 4,
        NonEnumerablePresent = 1 << 5,
        NonConfigurablePresent = 1 << 6,
        ValuePresent = 1 << 7,
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent | ValuePresent
    };

    // for plain data property
    explicit ObjectPropertyDescriptor(const Value& value, PresentAttribute attribute = ObjectPropertyDescriptor::NotPresent)
        : m_isDataProperty(true)
        , m_property((PresentAttribute)(attribute | ValuePresent))
        , m_value(value)
    {
        checkProperty();
    }

    // Accessor descriptor
    explicit ObjectPropertyDescriptor(const JSGetterSetter& jsGetterSetter, PresentAttribute attribute)
        : m_isDataProperty(false)
        , m_property(attribute)
        , m_getterSetter(jsGetterSetter)
    {
        checkProperty();
    }

    // Generic descriptor
    explicit ObjectPropertyDescriptor(PresentAttribute attribute)
        : m_isDataProperty(false)
        , m_property(attribute)
    {
        ASSERT(!isValuePresent());
        checkProperty();
    }

    explicit ObjectPropertyDescriptor(ExecutionState& state, Object* obj);

    ObjectPropertyDescriptor(const ObjectPropertyDescriptor& desc)
    {
        m_property = desc.m_property;
        if (desc.isDataProperty()) {
            m_isDataProperty = true;
            m_value = desc.m_value;
        } else {
            m_isDataProperty = false;
            m_getterSetter = desc.m_getterSetter;
        }
    }

    void operator=(const ObjectPropertyDescriptor& desc)
    {
        m_property = desc.m_property;
        if (desc.isDataProperty()) {
            m_isDataProperty = true;
            m_value = desc.m_value;
        } else {
            m_isDataProperty = false;
            m_getterSetter = desc.m_getterSetter;
        }
    }

    const Value& value() const
    {
        ASSERT(isDataProperty());
        ASSERT(isValuePresent());
        return m_value;
    }

    bool isDataDescriptor() const
    {
        return isValuePresent() || isWritablePresent();
    }

    bool isAccessorDescriptor() const
    {
        return !m_isDataProperty;
    }

    bool isGenericDescriptor() const
    {
        return !isDataDescriptor() && !isAccessorDescriptor();
    }

    const PresentAttribute& property() const
    {
        return m_property;
    }

    bool isWritablePresent() const
    {
        return (m_property & WritablePresent) | (m_property & NonWritablePresent);
    }

    bool isEnumerablePresent() const
    {
        return (m_property & EnumerablePresent) | (m_property & NonEnumerablePresent);
    }

    bool isConfigurablePresent() const
    {
        return (m_property & ConfigurablePresent) | (m_property & NonConfigurablePresent);
    }

    bool isValuePresent() const
    {
        return (m_property & ValuePresent);
    }

    bool isWritable() const
    {
        return (m_property & WritablePresent);
    }

    bool isEnumerable() const
    {
        return (m_property & EnumerablePresent);
    }

    bool isConfigurable() const
    {
        return (m_property & ConfigurablePresent);
    }

    bool isDataWritableEnumerableConfigurable() const
    {
        return isDataProperty() && isValuePresent() && isWritable() && isEnumerable() && isConfigurable();
    }

    bool isValuePresentAlone() const
    {
        return (m_property == ValuePresent);
    }

    bool hasJSGetter() const
    {
        ASSERT(!isDataProperty());
        return m_getterSetter.hasGetter();
    }

    bool hasJSSetter() const
    {
        ASSERT(!isDataProperty());
        return m_getterSetter.hasSetter();
    }

    const JSGetterSetter& getterSetter() const
    {
        ASSERT(!isDataProperty());
        return m_getterSetter;
    }

    void setEnumerable(bool enumerable);
    void setConfigurable(bool configurable);
    void setWritable(bool writable);
    void setValue(const Value& v);

    ObjectStructurePropertyDescriptor toObjectStructurePropertyDescriptor() const;
    static ObjectPropertyDescriptor fromObjectStructurePropertyDescriptor(const ObjectStructurePropertyDescriptor& desc, const Value& value);

private:
    bool isDataProperty() const
    {
        return m_isDataProperty;
    }

protected:
    void checkProperty()
    {
        if ((m_property & WritablePresent)) {
            if ((m_property & NonWritablePresent)) {
                ASSERT_NOT_REACHED();
            }
        }

        if ((m_property & EnumerablePresent)) {
            if ((m_property & NonEnumerablePresent)) {
                ASSERT_NOT_REACHED();
            }
        }

        if ((m_property & ConfigurablePresent)) {
            if ((m_property & NonConfigurablePresent)) {
                ASSERT_NOT_REACHED();
            }
        }

        if (!m_isDataProperty) {
            if ((m_property & WritablePresent) | (m_property & NonWritablePresent) | (m_property & ValuePresent)) {
                ASSERT_NOT_REACHED();
            }
        }
    }
    MAKE_STACK_ALLOCATED();
    bool m_isDataProperty : 1;
    PresentAttribute m_property;
    union {
        Value m_value;
        JSGetterSetter m_getterSetter;
    };
};

class ObjectGetResult {
public:
    ObjectGetResult()
        : m_hasValue(false)
        , m_isWritable(false)
        , m_isEnumerable(false)
        , m_isConfigurable(false)
        , m_isDataProperty(true)
        , m_value(Value())
    {
    }

    ObjectGetResult(const Value& v, bool isWritable, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(true)
        , m_value(v)
    {
    }

    ObjectGetResult(JSGetterSetter* getterSetter, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(false)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(false)
    {
        m_jsGetterSetter = getterSetter;
    }

    Value value(ExecutionState& state, const Value& receiver) const
    {
        if (LIKELY(m_isDataProperty))
            return m_value;
        return valueSlowCase(state, receiver);
    }

    JSGetterSetter* jsGetterSetter()
    {
        ASSERT(!m_isDataProperty);
        return m_jsGetterSetter;
    }

    bool hasValue() const
    {
        return m_hasValue;
    }

    bool isWritable() const
    {
        ASSERT(hasValue());
        ASSERT(isDataProperty());
        return m_isWritable;
    }

    bool isEnumerable() const
    {
        ASSERT(hasValue());
        return m_isEnumerable;
    }

    bool isConfigurable() const
    {
        ASSERT(hasValue());
        return m_isConfigurable;
    }

    bool isDataProperty() const
    {
        ASSERT(hasValue());
        return m_isDataProperty;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.10.4
    Value toPropertyDescriptor(ExecutionState& state, const Value& receiver);

protected:
    bool m_hasValue : 1;
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    bool m_isDataProperty : 1;
    union {
        Value m_value;
        JSGetterSetter* m_jsGetterSetter;
    };
    Value valueSlowCase(ExecutionState& state, const Value& receiver) const;
};

#define ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER 0
#define ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE

extern size_t g_objectTag;

class Object : public PointerValue {
    friend class Context;
    friend class GlobalObject;
    friend class ByteCodeInterpreter;
    friend class ObjectRareData;
    static Object* createBuiltinObjectPrototype(ExecutionState& state);

public:
    Object(ExecutionState& state);
    static Object* createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function);
    virtual Type type()
    {
        return ObjectType;
    }

    virtual bool isObject() const
    {
        return true;
    }

    ErrorObject* asErrorObject()
    {
        ASSERT(isErrorObject());
        return (ErrorObject*)this;
    }

    ArrayObject* asArrayObject()
    {
        ASSERT(isArrayObject());
        return (ArrayObject*)this;
    }

    RegExpObject* asRegExpObject()
    {
        ASSERT(isRegExpObject());
        return (RegExpObject*)this;
    }

#if ESCARGOT_ENABLE_TYPEDARRAY
    ArrayBufferObject* asArrayBufferObject()
    {
        ASSERT(isArrayBufferObject());
        return (ArrayBufferObject*)this;
    }

    ArrayBufferView* asArrayBufferView()
    {
        ASSERT(isTypedArrayObject());
        return (ArrayBufferView*)this;
    }

    template <typename TypedArrayType>
    TypedArrayType* asTypedArrayObject()
    {
        ASSERT(isTypedArrayObject());
        return (TypedArrayType*)this;
    }
#endif

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-isextensible
    bool isExtensible()
    {
        return rareData() == nullptr ? true : rareData()->m_isExtensible;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-preventextensions
    void preventExtensions()
    {
        ensureObjectRareData()->m_isExtensible = false;
    }

    Value getPrototype(ExecutionState& state)
    {
        if (LIKELY((size_t)m_prototype > 2)) {
            if (UNLIKELY(g_objectRareDataTag == *((size_t*)(m_prototype)))) {
                Object* e = rareData()->m_prototype;
                if ((size_t)e > 2) {
                    return e;
                } else if (e == nullptr) {
                    return Value(Value::Null);
                } else {
                    ASSERT((size_t)e == 1);
                    return Value();
                }
            }
            return m_prototype;
        } else if (m_prototype == nullptr) {
            return Value(Value::Null);
        } else {
            ASSERT((size_t)m_prototype == 1);
            return Value();
        }
    }

    void setPrototype(ExecutionState& state, const Value& value);

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    // enumeration every property!
    // callback function should skip un-Enumerable property if needs
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint64_t length(ExecutionState& state);

    bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        return getOwnProperty(state, propertyName).hasValue();
    }

    bool hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        return get(state, propertyName).hasValue();
    }

    ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P);

    bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    void setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    void setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    void defineOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
    {
        if (!defineOwnProperty(state, P, desc)) {
            throwCannotDefineError(state, P.toPropertyName(state));
        }
    }

    void defineOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
    {
        if (!defineOwnProperty(state, P, desc) && state.inStrictMode()) {
            throwCannotDefineError(state, P.toPropertyName(state));
        }
    }

    void deleteOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P)
    {
        if (!deleteOwnProperty(state, P)) {
            throwCannotDefineError(state, P.toPropertyName(state));
        }
    }

    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value);
    void setIndexedPropertyThrowsException(ExecutionState& state, const Value& property, const Value& value)
    {
        if (!setIndexedProperty(state, property, value)) {
            throwCannotDefineError(state, PropertyName(state, property.toString(state)));
        }
    }

    void markThisObjectDontNeedStructureTransitionTable(ExecutionState& state)
    {
        ASSERT(structure()->inTransitionMode());
        m_structure = m_structure->convertToWithFastAccess(state);
    }

    static double nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double len, const bool skipUndefined);
    static double nextIndexBackward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined);

    virtual void sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp);

    ObjectRareData* ensureObjectRareData()
    {
        if (rareData() == nullptr) {
            m_prototype = (Object*)(new ObjectRareData(this));
        }
        return rareData();
    }

    bool isEverSetAsPrototypeObject() const
    {
        if (LIKELY(rareData() == nullptr)) {
            return false;
        } else {
            return rareData()->m_isEverSetAsPrototypeObject;
        }
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        if (LIKELY(rareData() == nullptr) || LIKELY(rareData()->m_internalClassName == nullptr))
            return "Object";
        return rareData()->m_internalClassName;
    }

    void giveInternalClassProperty(const char* name)
    {
        ensureObjectRareData()->m_internalClassName = name;
    }

    void* extraData()
    {
        if (rareData()) {
            return rareData()->m_extraData;
        }
        return nullptr;
    }

    void setExtraData(void* e)
    {
        ensureObjectRareData()->m_extraData = e;
    }

#ifdef ESCARGOT_ENABLE_PROMISE
    Object* ensureInternalSlot(ExecutionState& state)
    {
        ensureObjectRareData();
        if (!internalSlot())
            setInternalSlot(new Object(state));
        return internalSlot();
    }

    Object* internalSlot()
    {
        ASSERT(rareData());
        return rareData()->m_internalSlot;
    }

    void setInternalSlot(Object* object)
    {
        ensureObjectRareData()->m_internalSlot = object;
    }
#endif

    static void throwCannotDefineError(ExecutionState& state, const PropertyName& P);
    static void throwCannotWriteError(ExecutionState& state, const PropertyName& P);
    static void throwCannotDeleteError(ExecutionState& state, const PropertyName& P);

    // this function is always success
    // conditon of useing this function is
    // ASSERT(!hasOwnProperty(state, P));
    // ASSERT(isExtensible());
    // ASSERT(!isArrayObject());
    // ASSERT(!isStringObject());
    void defineNativeGetterSetterDataProperty(ExecutionState& state, const ObjectPropertyName& P, ObjectPropertyNativeGetterSetterData* data, const Value& objectInternalData);

protected:
    Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea);
    void initPlainObject(ExecutionState& state);
    ObjectRareData* rareData() const
    {
        if ((size_t)m_prototype > 2) {
            if ((g_objectRareDataTag == *((size_t*)(m_prototype)))) {
                return (ObjectRareData*)m_prototype;
            }
        }
        return nullptr;
    }
    ObjectStructure* m_structure;
    Object* m_prototype;
    TightVectorWithNoSize<SmallValue, GCUtil::gc_malloc_ignore_off_page_allocator<SmallValue>> m_values;

    COMPILE_ASSERT(sizeof(TightVectorWithNoSize<SmallValue, GCUtil::gc_malloc_ignore_off_page_allocator<SmallValue>>) == sizeof(size_t) * 1, "");

    ObjectStructure* structure() const
    {
        return m_structure;
    }


    ALWAYS_INLINE Value uncheckedGetOwnDataProperty(ExecutionState& state, size_t idx)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        return m_values[idx];
    }

    ALWAYS_INLINE void uncheckedSetOwnDataProperty(ExecutionState& state, size_t idx, const Value& newValue)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        m_values[idx] = newValue;
    }

    ALWAYS_INLINE Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx)
    {
        return getOwnDataPropertyUtilForObject(state, idx, this);
    }

    ALWAYS_INLINE Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& receiver)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            return m_values[idx];
        } else {
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this);
        }
    }

    ALWAYS_INLINE bool setOwnDataPropertyUtilForObjectInner(ExecutionState& state, size_t idx, const ObjectStructureItem& item, const Value& newValue)
    {
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            m_values[idx] = newValue;
            return true;
        } else {
            bool ret = item.m_descriptor.nativeGetterSetterData()->m_setter(state, this, newValue);
            return ret;
        }
    }

    ALWAYS_INLINE bool setOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isWritable())) {
            return setOwnDataPropertyUtilForObjectInner(state, idx, item, newValue);
        } else {
            return false;
        }
    }

    Value getOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& receiver);
    ALWAYS_INLINE Value getOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
                return m_values[idx];
            } else {
                return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this);
            }
        } else {
            return getOwnPropertyUtilForObjectAccCase(state, idx, receiver);
        }
    }

    bool setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue);
    ALWAYS_INLINE bool setOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            return setOwnDataPropertyUtilForObject(state, idx, newValue);
        } else {
            return setOwnPropertyUtilForObjectAccCase(state, idx, newValue);
        }
    }

    ALWAYS_INLINE void setOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, size_t idx, const Value& newValue)
    {
        if (UNLIKELY(!setOwnPropertyUtilForObject(state, idx, newValue) && state.inStrictMode())) {
            throwCannotWriteError(state, m_structure->readProperty(state, idx).m_propertyName);
        }
    }

    void markAsPrototypeObject(ExecutionState& state);
    void deleteOwnProperty(ExecutionState& state, size_t idx);

    Object* getPrototypeObject()
    {
        if (LIKELY((size_t)m_prototype > 2)) {
            if (UNLIKELY(g_objectRareDataTag == *((size_t*)(m_prototype)))) {
                Object* e = rareData()->m_prototype;
                if ((size_t)e > 2) {
                    return e;
                } else if (e == nullptr) {
                    return nullptr;
                } else {
                    ASSERT((size_t)e == 1);
                    return nullptr;
                }
            }
            return m_prototype;
        } else if (m_prototype == nullptr) {
            return nullptr;
        } else {
            ASSERT((size_t)m_prototype == 1);
            return nullptr;
        }
    }
};

COMPILE_ASSERT(sizeof(Object) >= sizeof(String), "");
}

#endif
