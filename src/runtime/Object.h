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
class ArgumentsObject;
class ArrayBufferObject;
class ArrayBufferView;
class DataViewObject;
template <typename TypeArg, int elementSize>
class TypedArrayObject;

extern size_t g_objectRareDataTag;

struct ObjectRareData : public PointerValue {
    bool m_isExtensible : 1;
    bool m_isEverSetAsPrototypeObject : 1;
    bool m_isFastModeArrayObject : 1;
    bool m_shouldUpdateEnumerateObjectData : 1;
    bool m_isInArrayObjectDefineOwnProperty : 1;
    bool m_hasNonWritableLastIndexRegexpObject : 1;
    void* m_extraData;
    Object* m_prototype;
    Object* m_internalSlot;
    explicit ObjectRareData(Object* obj);

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
            m_value.m_name = PropertyName(state, v);
        }
    }

    ObjectPropertyName(const AtomicString& v)
    {
        m_isUIntType = false;
        m_value.m_name = v;
    }

    ObjectPropertyName(ExecutionState&, const PropertyName& v)
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

    bool isIndexString() const
    {
        if (isUIntType()) {
            return true;
        } else {
            return propertyName().isIndexString();
        }
    }

    ALWAYS_INLINE PropertyName toPropertyName(ExecutionState& state) const
    {
        if (isUIntType()) {
            return toPropertyNameUintCase(state);
        }
        return propertyName();
    }

    uint64_t tryToUseAsIndex() const
    {
        if (LIKELY(isUIntType())) {
            return m_value.m_uint;
        }
        return propertyName().tryToUseAsIndex();
    }

    uint64_t tryToUseAsArrayIndex() const
    {
        if (LIKELY(isUIntType())) {
            return m_value.m_uint;
        }
        return propertyName().tryToUseAsArrayIndex();
    }

    Value toPlainValue(ExecutionState&) const
    {
        if (isUIntType()) {
            return Value(uintValue());
        } else {
            return propertyName().toValue();
        }
    }

    String* toExceptionString() const
    {
        if (isUIntType()) {
            return String::fromDouble(uintValue());
        } else {
            return propertyName().toExceptionString();
        }
    }

private:
    bool m_isUIntType : 1;
    union ObjectPropertyNameData {
        ObjectPropertyNameData() { m_uint = 0; }
        PropertyName m_name;
        uint32_t m_uint;
    } m_value;

    PropertyName toPropertyNameUintCase(ExecutionState& state) const;
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
#ifndef ESCARGOT_32
        ASSERT(hasGetter());
#endif
        return m_getter;
    }

    Value setter() const
    {
#ifndef ESCARGOT_32
        ASSERT(hasSetter());
#endif
        return m_setter;
    }

private:
    SmallValue m_getter;
    SmallValue m_setter;
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
    explicit ObjectPropertyDescriptor(PresentAttribute attribute, bool isDataProperty = false)
        : m_isDataProperty(isDataProperty)
        , m_property(attribute)
    {
        ASSERT(!isValuePresent());
        checkProperty();
    }

    explicit ObjectPropertyDescriptor(ExecutionState& state, Object* obj);

    ~ObjectPropertyDescriptor() {}
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
        if (&desc == this)
            return;

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
    static Object* fromObjectPropertyDescriptor(ExecutionState& state, const ObjectPropertyDescriptor& desc);

private:
    bool isDataProperty() const
    {
        return m_isDataProperty;
    }
    bool m_isDataProperty : 1;
    PresentAttribute m_property;
    union {
        Value m_value;
        JSGetterSetter m_getterSetter;
    };

protected:
    void checkProperty()
    {
        if ((m_property & WritablePresent) && (m_property & NonWritablePresent)) {
            ASSERT_NOT_REACHED();
        }

        if ((m_property & EnumerablePresent) && (m_property & NonEnumerablePresent)) {
            ASSERT_NOT_REACHED();
        }

        if ((m_property & ConfigurablePresent) && (m_property & NonConfigurablePresent)) {
            ASSERT_NOT_REACHED();
        }

        if (!m_isDataProperty && ((m_property & WritablePresent) | (m_property & NonWritablePresent) | (m_property & ValuePresent))) {
            ASSERT_NOT_REACHED();
        }
    }
    MAKE_STACK_ALLOCATED();
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

private:
    bool m_hasValue : 1;
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    bool m_isDataProperty : 1;
    union {
        Value m_value;
        JSGetterSetter* m_jsGetterSetter;
    };

protected:
    Value valueSlowCase(ExecutionState& state, const Value& receiver) const;
};

#define ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER 0
#define ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE

extern size_t g_objectTag;

enum class EnumerableOwnPropertiesType {
    Key,
    Value,
    KeyAndValue
};


enum class ElementTypes : uint8_t {
    Undefined = 1,
    Null = 1 << 1,
    Boolean = 1 << 2,
    String = 1 << 3,
    Symbol = 1 << 4,
    Number = 1 << 5,
    Object = 1 << 6,
    ALL = Undefined | Null | Boolean | String | Symbol | Number | Object
};

class Object : public PointerValue {
    friend class VMInstance;
    friend class GlobalObject;
    friend class ByteCodeInterpreter;
    friend struct ObjectRareData;
    static Object* createBuiltinObjectPrototype(ExecutionState& state);

public:
    explicit Object(ExecutionState& state);
    static Object* createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function);

    virtual bool isObject() const
    {
        return true;
    }

    virtual bool isOrdinary() const
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

    RegExpObject* asRegExpObject(ExecutionState& state)
    {
        ASSERT(isRegExpObject(state));
        return (RegExpObject*)this;
    }

    String* optionString(ExecutionState& state);

    ArrayBufferObject* asArrayBufferObject()
    {
        ASSERT(isArrayBufferObject());
        return (ArrayBufferObject*)this;
    }

    ArrayBufferView* asArrayBufferView()
    {
        ASSERT(isArrayBufferView());
        return (ArrayBufferView*)this;
    }

    DataViewObject* asDataViewObject()
    {
        ASSERT(isDataViewObject());
        return (DataViewObject*)this;
    }

    ArgumentsObject* asArgumentsObject()
    {
        ASSERT(isArgumentsObject());
        return (ArgumentsObject*)this;
    }

    template <typename TypedArrayType>
    TypedArrayType* asTypedArrayObject()
    {
        ASSERT(isTypedArrayObject());
        return (TypedArrayType*)this;
    }

    bool isConcatSpreadable(ExecutionState& state);

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-isextensiblie
    virtual bool isExtensible(ExecutionState&)
    {
        return rareData() == nullptr ? true : rareData()->m_isExtensible;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-preventextensions
    virtual bool preventExtensions(ExecutionState&)
    {
        ensureObjectRareData()->m_isExtensible = false;
        return true;
    }

    // internal [[prototype]]
    virtual Value getPrototype(ExecutionState&)
    {
        if (LIKELY(m_prototype != nullptr)) {
            if (UNLIKELY(g_objectRareDataTag == *(size_t*)m_prototype)) {
                Object* prototype = rareData()->m_prototype;
                if (prototype == nullptr) {
                    return Value(Value::Null);
                }
                return prototype;
            }
            return m_prototype;
        }

        return Value(Value::Null);
    }

    // internal [[prototype]]
    virtual Object* getPrototypeObject(ExecutionState&)
    {
        Object* prototype = m_prototype;

        if (UNLIKELY(m_prototype != nullptr && g_objectRareDataTag == *(size_t*)prototype)) {
            prototype = rareData()->m_prototype;
        }
        return prototype;
    }

    // internal [[prototype]]
    virtual bool setPrototype(ExecutionState& state, const Value& proto);

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    // callback function should skip un-Enumerable property if needs
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint64_t length(ExecutionState& state);
    double lengthES6(ExecutionState& state);

    bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        return getOwnProperty(state, propertyName).hasValue();
    }

    virtual bool hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName);

    virtual ValueVector ownPropertyKeys(ExecutionState& state);

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P);
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);

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

    // returns existence of index
    static bool nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double len, const bool skipUndefined, double& nextIndex);
    static bool nextIndexBackward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined, double& nextIndex);

    virtual void sort(ExecutionState& state, const std::function<bool(const Value& a, const Value& b)>& comp);

    virtual bool isInlineCacheable()
    {
        return true;
    }

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
        return "Object";
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

    Object* ensureInternalSlot(ExecutionState& state)
    {
        ensureObjectRareData();
        if (!internalSlot()) {
            setInternalSlot(new Object(state));
            internalSlot()->setPrototype(state, Value(Value::Null));
        }
        return internalSlot();
    }

    Object* internalSlot()
    {
        ASSERT(rareData());
        return rareData()->m_internalSlot;
    }

    bool hasInternalSlot()
    {
        return rareData() && rareData()->m_internalSlot;
    }

    void setInternalSlot(Object* object)
    {
        ensureObjectRareData()->m_internalSlot = object;
    }

    static bool ordinaryHasProperty(ExecutionState& state, Object* object, const ObjectPropertyName& propertyName);
    static Value get(ExecutionState& state, const Value& value, const ObjectPropertyName& propertyName);
    static Value getMethod(ExecutionState& state, const Value& object, const ObjectPropertyName& propertyName);
    static Value call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, NULLABLE Value* argv);
    static Object* construct(ExecutionState& state, const Value& constructor, const size_t argc, NULLABLE Value* argv, Object* newTarget = nullptr);
    static bool hasInstance(ExecutionState& state, const Value& C, const Value O);

    static void throwCannotDefineError(ExecutionState& state, const PropertyName& P);
    static void throwCannotWriteError(ExecutionState& state, const PropertyName& P);
    static void throwCannotDeleteError(ExecutionState& state, const PropertyName& P);
    static ArrayObject* createArrayFromList(ExecutionState& state, const size_t size, Value* buffer);
    static ArrayObject* createArrayFromList(ExecutionState& state, ValueVector& elements);
    static ValueVector createListFromArrayLike(ExecutionState& state, Value obj, uint8_t types = (uint8_t)ElementTypes::ALL);
    static ValueVector enumerableOwnProperties(ExecutionState& state, Object* object, EnumerableOwnPropertiesType kind);

    // this function differ with defineOwnProperty.
    // !hasOwnProperty(state, P) is needed for success
    bool defineNativeDataAccessorProperty(ExecutionState& state, const ObjectPropertyName& P, ObjectPropertyNativeGetterSetterData* data, const Value& objectInternalData);

    IteratorObject* values(ExecutionState& state);
    IteratorObject* keys(ExecutionState& state);
    IteratorObject* entries(ExecutionState& state);

    Value speciesConstructor(ExecutionState& state, const Value& defaultConstructor);

protected:
    Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea);
    void initPlainObject(ExecutionState& state);
    ObjectRareData* rareData() const
    {
        if ((size_t)m_prototype > 2 && g_objectRareDataTag == *((size_t*)(m_prototype))) {
            return (ObjectRareData*)m_prototype;
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
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this, m_values[idx]);
        }
    }

    ALWAYS_INLINE bool setOwnDataPropertyUtilForObjectInner(ExecutionState& state, size_t idx, const ObjectStructureItem& item, const Value& newValue)
    {
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            m_values[idx] = newValue;
            return true;
        } else {
            return item.m_descriptor.nativeGetterSetterData()->m_setter(state, this, m_values[idx], newValue);
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
                return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this, m_values[idx]);
            }
        } else {
            return getOwnPropertyUtilForObjectAccCase(state, idx, receiver);
        }
    }

    bool setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver);
    ALWAYS_INLINE bool setOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            return setOwnDataPropertyUtilForObject(state, idx, newValue);
        } else {
            return setOwnPropertyUtilForObjectAccCase(state, idx, newValue, receiver);
        }
    }

    ALWAYS_INLINE void setOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
    {
        if (UNLIKELY(!setOwnPropertyUtilForObject(state, idx, newValue, receiver) && state.inStrictMode())) {
            throwCannotWriteError(state, m_structure->readProperty(state, idx).m_propertyName);
        }
    }

    void markAsPrototypeObject(ExecutionState& state);
    void deleteOwnProperty(ExecutionState& state, size_t idx);
};
}

#endif
