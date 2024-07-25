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

#ifndef __EscargotObject__
#define __EscargotObject__

#include "runtime/PointerValue.h"
#include "runtime/ObjectStructure.h"
#include "runtime/ObjectPrivateMemberStructure.h"
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
class ExecutionPauser;

#define OBJECT_PROPERTY_NAME_UINT32_VIAS 2
#define MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME (std::numeric_limits<uint32_t>::max() >> OBJECT_PROPERTY_NAME_UINT32_VIAS)

typedef TightVectorWithNoSizeUseGCRealloc<EncodedValue, GCUtil::gc_malloc_allocator<EncodedValue>> ObjectPrivateFieldValueVector;

struct ObjectPrivateMemberDataChain : public gc {
    Object* m_contextObject;
    ObjectPrivateMemberStructure* m_privateMemberStructure;
    ObjectPrivateFieldValueVector m_privateMemberValues;
    Optional<ObjectPrivateMemberDataChain*> m_nextPiece;

    ObjectPrivateMemberDataChain(Object* contextObject, ObjectPrivateMemberStructure* privateMemberStructure)
        : m_contextObject(contextObject)
        , m_privateMemberStructure(privateMemberStructure)
    {
    }
};

struct ObjectExtendedExtraData : public gc {
    void* m_extraData;
    size_t m_removedFinalizerCount;
    TightVector<std::pair<FinalizerFunction, void*>, GCUtil::gc_malloc_atomic_allocator<std::pair<FinalizerFunction, void*>>> m_finalizer;
    Optional<ObjectPrivateMemberDataChain*> m_privateMemberChain;
    Optional<FunctionObject*> m_meaningfulConstructor;
    ObjectExtendedExtraData(void* e)
        : m_extraData(e)
        , m_removedFinalizerCount(0)
    {
    }
};

struct ObjectRareData : public PointerValue {
    bool m_isExtensible : 1;
    bool m_isEverSetAsPrototypeObject : 1;
    bool m_isArrayObjectLengthWritable : 1;
    bool m_isSpreadArrayObject : 1;
    bool m_isFinalizerRegistered : 1;
    bool m_isInlineCacheable : 1;
    bool m_hasExtendedExtraData : 1;
#if defined(ESCARGOT_ENABLE_TEST)
    bool m_isHTMLDDA : 1;
#endif
    uint8_t m_arrayObjectFastModeBufferExpandCount : 8;
    union {
        void* m_extraData;
        ObjectExtendedExtraData* m_extendedExtraData;
    };
    Object* m_prototype;
    union {
        Object* m_internalSlot;
        StorePositiveNumberAsOddNumber m_arrayObjectFastModeBufferCapacity;
    };
    explicit ObjectRareData(Object* obj);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class ObjectPropertyName {
public:
    ObjectPropertyName(ExecutionState& state, const Value& v)
    {
        if (v.isUInt32()) {
            uint32_t value = v.asUInt32();
            if (LIKELY(value <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME)) {
                setUIntValue(value);
                return;
            }
        }
        setNameValue(state, v);
    }

    explicit ObjectPropertyName(ExecutionState& state, const int64_t& v)
    {
        if (v >= 0 && v <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME) {
            setUIntValue((uint32_t)v);
        } else {
            setNameValue(state, Value(v));
        }
    }

#if defined(OS_DARWIN)
    // DARWIN clang defines size_t type as unsigned long instead of uint type
    explicit ObjectPropertyName(ExecutionState& state, const size_t& v)
    {
        if (v <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME) {
            setUIntValue((uint32_t)v);
        } else {
            setNameValue(state, Value(v));
        }
    }
#endif

    explicit ObjectPropertyName(ExecutionState& state, const uint64_t& v)
    {
        if (v <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME) {
            setUIntValue((uint32_t)v);
        } else {
            setNameValue(state, Value(v));
        }
    }

    explicit ObjectPropertyName(ExecutionState& state, const uint32_t& v)
    {
        if (v <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME) {
            setUIntValue(v);
        } else {
            setNameValue(state, Value(v));
        }
    }

    ObjectPropertyName(Symbol* symbol)
    {
        m_name = ObjectStructurePropertyName(symbol);
        ASSERT(!isUIntType());
    }

    ObjectPropertyName(const AtomicString& v)
    {
        m_name = v;
        ASSERT(!isUIntType());
    }

    ObjectPropertyName(ExecutionState&, const ObjectStructurePropertyName& v)
    {
        m_name = v;
        ASSERT(!isUIntType());
    }

    ObjectPropertyName(const ObjectStructurePropertyName& v)
    {
        m_name = v;
        ASSERT(!isUIntType());
    }

    inline bool isUIntType() const
    {
        return m_uintData & OBJECT_PROPERTY_NAME_UINT32_VIAS;
    }

    inline bool isStringType() const
    {
        if (LIKELY(isUIntType() || objectStructurePropertyName().isPlainString())) {
            return true;
        }
        return false;
    }

    inline uint32_t uintValue() const
    {
        ASSERT(isUIntType());
        return (m_uintData >> OBJECT_PROPERTY_NAME_UINT32_VIAS);
    }

    const ObjectStructurePropertyName& objectStructurePropertyName() const
    {
        ASSERT(!isUIntType());
        return m_name;
    }

    bool isIndexString() const
    {
        if (isUIntType()) {
            return true;
        } else {
            return objectStructurePropertyName().isIndexString();
        }
    }

    ALWAYS_INLINE ObjectStructurePropertyName toObjectStructurePropertyName(ExecutionState& state) const
    {
        if (isUIntType()) {
            return toObjectStructurePropertyNameUintCase(state);
        }
        return objectStructurePropertyName();
    }

    double canonicalNumericIndexString(ExecutionState& state) const
    {
        if (LIKELY(isUIntType())) {
            return uintValue();
        }
        return objectStructurePropertyName().canonicalNumericIndexString(state);
    }

    uint32_t tryToUseAsIndexProperty() const
    {
        // index property uses 32bit unsigned integer
        if (LIKELY(isUIntType())) {
            return uintValue();
        }
        return objectStructurePropertyName().tryToUseAsIndexProperty();
    }

    Value toPlainValue() const
    {
        if (isUIntType()) {
            return Value(uintValue());
        } else {
            return objectStructurePropertyName().toValue();
        }
    }

    String* toExceptionString() const
    {
        if (isUIntType()) {
            return String::fromDouble(uintValue());
        } else {
            return objectStructurePropertyName().toExceptionString();
        }
    }

    Value toPropertyKeyValue() const
    {
        if (isUIntType()) {
            return Value(String::fromDouble(uintValue()));
        } else {
            return objectStructurePropertyName().toValue();
        }
    }

private:
    union {
        ObjectStructurePropertyName m_name;
        uint32_t m_uintData;
    };

    inline void setUIntValue(uint32_t value)
    {
        ASSERT(value <= MAXIMUM_UINT_FOR_32BIT_PROPERTY_NAME);
        m_uintData = (value << OBJECT_PROPERTY_NAME_UINT32_VIAS) | OBJECT_PROPERTY_NAME_UINT32_VIAS;
    }

    inline void setNameValue(ExecutionState& state, const Value& v)
    {
        m_name = ObjectStructurePropertyName(state, v);
        ASSERT(!isUIntType());
    }

    ObjectStructurePropertyName toObjectStructurePropertyNameUintCase(ExecutionState& state) const;
};

typedef VectorWithInlineStorage<48, ObjectPropertyName, GCUtil::gc_malloc_allocator<ObjectPropertyName>> ObjectPropertyNameVector;

class JSGetterSetter : public PointerValue {
    friend class ObjectPropertyDescriptor;
    friend class Object;
    friend class DerivedObject;

public:
    JSGetterSetter(Value getter, Value setter)
        : m_getter(getter)
        , m_setter(setter)
    {
        ASSERT(getter.isEmpty() || getter.isCallable() || getter.isUndefined());
        ASSERT(setter.isEmpty() || setter.isCallable() || setter.isUndefined());
    }

    virtual bool isJSGetterSetter() const override
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

    void setGetter(Value g)
    {
        ASSERT(g.isEmpty() || g.isCallable() || g.isUndefined());
        m_getter = g;
    }

    void setSetter(Value g)
    {
        ASSERT(g.isEmpty() || g.isCallable() || g.isUndefined());
        m_setter = g;
    }

private:
    EncodedValue m_getter;
    EncodedValue m_setter;
};

class ObjectPropertyDescriptor {
    friend class Object;
    friend class DerivedObject;

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

    explicit ObjectPropertyDescriptor()
        : m_isDataProperty(false)
        , m_property((PresentAttribute)(NotPresent))
        , m_value(Value())
    {
        // this default constructor is used to create a temporal invalid ObjectPropertyDescriptor
    }

    // for plain data property
    explicit ObjectPropertyDescriptor(const Value& value, PresentAttribute attribute = ObjectPropertyDescriptor::NotPresent)
        : m_isDataProperty(true)
        , m_property((PresentAttribute)(attribute | ValuePresent))
        , m_value(value)
    {
        ASSERT(checkProperty());
    }

    // Accessor descriptor
    explicit ObjectPropertyDescriptor(const JSGetterSetter& jsGetterSetter, PresentAttribute attribute)
        : m_isDataProperty(false)
        , m_property(attribute)
        , m_getterSetter(jsGetterSetter)
    {
        ASSERT(checkProperty());
    }

    // Generic descriptor
    explicit ObjectPropertyDescriptor(PresentAttribute attribute, bool isDataProperty = false)
        : m_isDataProperty(isDataProperty)
        , m_property(attribute)
    {
        ASSERT(!isValuePresent());
        ASSERT(checkProperty());
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
    static void completePropertyDescriptor(ObjectPropertyDescriptor& desc);

#ifndef NDEBUG
    bool checkProperty() const
    {
        if ((m_property & WritablePresent) && (m_property & NonWritablePresent)) {
            return false;
        }

        if ((m_property & EnumerablePresent) && (m_property & NonEnumerablePresent)) {
            return false;
        }

        if ((m_property & ConfigurablePresent) && (m_property & NonConfigurablePresent)) {
            return false;
        }

        if (!m_isDataProperty && ((m_property & WritablePresent) | (m_property & NonWritablePresent) | (m_property & ValuePresent))) {
            return false;
        }

        return true;
    }
#endif

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
};

class ObjectGetResult {
public:
    ObjectGetResult()
        : m_hasValue(false)
        , m_isWritable(false)
        , m_isEnumerable(false)
        , m_isConfigurable(false)
        , m_isDataProperty(true)
        , m_isDataAccessorProperty(false)
        , m_value()
    {
    }

    ObjectGetResult(const Value& v, bool isWritable, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(true)
        , m_isDataAccessorProperty(false)
        , m_value(v)
    {
    }

    ObjectGetResult(ObjectPropertyNativeGetterSetterData* nativeGetterSetter, Object* self, const EncodedValue& internalData, bool isWritable, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(true)
        , m_isDataAccessorProperty(true)
    {
        m_nativeGetterSetterData.m_nativeGetterSetterData = nativeGetterSetter;
        m_nativeGetterSetterData.m_object = self;
        m_nativeGetterSetterData.m_internalData = internalData;
    }

    ObjectGetResult(JSGetterSetter* getterSetter, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(false)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(false)
        , m_isDataAccessorProperty(true)
        , m_jsGetterSetter(getterSetter)
    {
    }

    Value value(ExecutionState& state, const Value& receiver) const
    {
        if (LIKELY(!m_isDataAccessorProperty))
            return m_value;
        return valueSlowCase(state, receiver);
    }

    JSGetterSetter* jsGetterSetter() const
    {
        ASSERT(!isDataProperty());
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

    bool isDataAccessorProperty() const
    {
        ASSERT(hasValue());
        return m_isDataAccessorProperty;
    }

    ObjectPropertyDescriptor convertToPropertyDescriptor(ExecutionState& state, const Value& receiver);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.10.4
    Value fromPropertyDescriptor(ExecutionState& state, const Value& receiver);

    ObjectPropertyNativeGetterSetterData* nativeGetterSetterData()
    {
        ASSERT(isDataAccessorProperty() && isDataProperty());
        return m_nativeGetterSetterData.m_nativeGetterSetterData;
    }

private:
    bool m_hasValue : 1;
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    // this includes DataAccessorProperty.
    // if isDataAccessorProperty is true, isDataProperty is always true
    bool m_isDataProperty : 1;
    bool m_isDataAccessorProperty : 1;
    union {
        Value m_value;
        struct {
            ObjectPropertyNativeGetterSetterData* m_nativeGetterSetterData;
            Object* m_object;
            EncodedValue m_internalData;
        } m_nativeGetterSetterData;
        JSGetterSetter* m_jsGetterSetter;
    };

protected:
    Value valueSlowCase(ExecutionState& state, const Value& receiver) const;
};

class ObjectHasPropertyResult {
public:
    ObjectHasPropertyResult()
        : m_hasOrdinaryObjectGetResult(true)
        , m_result(ObjectGetResult())
    {
    }

    ObjectHasPropertyResult(const ObjectGetResult& result)
        : m_hasOrdinaryObjectGetResult(true)
        , m_result(result)
    {
    }

    // in this case, hasProperty always returns true
    ObjectHasPropertyResult(Value (*valueHandler)(ExecutionState& state, const ObjectPropertyName& P, void* handlerData), void* handlerData)
        : m_hasOrdinaryObjectGetResult(false)
    {
        m_handler.m_handlerData = handlerData;
        m_handler.m_valueHandler = valueHandler;
    }

    operator bool() const
    {
        return hasProperty();
    }

    bool hasProperty() const
    {
        if (LIKELY(m_hasOrdinaryObjectGetResult)) {
            return m_result.hasValue();
        } else {
            return true;
        }
    }

    Value value(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver)
    {
        if (LIKELY(m_hasOrdinaryObjectGetResult)) {
            return m_result.value(state, receiver);
        } else {
            return m_handler.m_valueHandler(state, P, m_handler.m_handlerData);
        }
    }

private:
    bool m_hasOrdinaryObjectGetResult;
    union {
        ObjectGetResult m_result;
        struct {
            void* m_handlerData;
            Value (*m_valueHandler)(ExecutionState& state, const ObjectPropertyName& P, void* handlerData);
        } m_handler;
    };
};

#define ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER 0

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
    BigInt = 1 << 6,
    Object = 1 << 7,
    ALL = Undefined | Null | Boolean | String | Symbol | Number | BigInt | Object
};

class Object : public PointerValue {
    friend class ObjectRef;
    friend class Global;
    friend class GlobalObject;
    friend class Interpreter;
    friend class InterpreterSlowPath;
    friend class EnumerateObjectWithDestruction;
    friend class EnumerateObjectWithIteration;
    friend struct ObjectRareData;
    friend class Template;
    friend class ObjectTemplate;

public:
    explicit Object(ExecutionState& state);
    explicit Object(ExecutionState& state, Object* proto);
    enum PrototypeIsNullTag { PrototypeIsNull };
    explicit Object(ExecutionState& state, PrototypeIsNullTag); // I added new function for reducing checking null for prototype
    // create huge object
    explicit Object(ExecutionState& state, size_t propertyCount,
                    std::pair<Value, Value> (*keyAndValueCallback)(ExecutionState& state, void* data), void* callbackData,
                    bool isWritable, bool isEnumerable, bool isConfigurable);

    static Object* createBuiltinObjectPrototype(ExecutionState& state);
    static Object* createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function);

    virtual bool isOrdinary() const
    {
        return true;
    }

    bool isConcatSpreadable(ExecutionState& state);

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-isextensiblie
    virtual bool isExtensible(ExecutionState&)
    {
        return hasRareData() ? rareData()->m_isExtensible : true;
    }

    // represent that this Object has its own enumeration function other than Object::enumeration
    virtual bool hasOwnEnumeration() const
    {
        return false;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-preventextensions
    virtual bool preventExtensions(ExecutionState&)
    {
        markThisObjectDontNeedStructureTransitionTable();
        ensureRareData()->m_isExtensible = false;
        return true;
    }

    // internal [[prototype]]
    virtual Value getPrototype(ExecutionState&)
    {
        Object* prototype = m_prototype;
        if (UNLIKELY(hasRareData())) {
            prototype = rareData()->m_prototype;
        }
        return prototype ? prototype : Value(Value::Null);
    }

    // internal [[prototype]]
    virtual Object* getPrototypeObject(ExecutionState&)
    {
        Object* prototype = m_prototype;
        if (UNLIKELY(hasRareData())) {
            prototype = rareData()->m_prototype;
        }
        return prototype;
    }

    Optional<Object*> rawInternalPrototypeObject()
    {
        Object* prototype = m_prototype;

        if (UNLIKELY(hasRareData())) {
            prototype = rareData()->m_prototype;
        }
        return prototype;
    }

    Optional<Value> readConstructorSlotWithoutState()
    {
        size_t l = m_structure->propertyCount();
        const ObjectStructureItem* items = m_structure->properties();

        for (size_t i = 0; i < l; i++) {
            if (items[i].m_propertyName.isPlainString() && items[i].m_propertyName.plainString()->equals("constructor")) {
                if (items[i].m_descriptor.isDataProperty()) {
                    return Value(m_values[i]);
                }
                break;
            }
        }

        return Optional<Value>();
    }

    String* constructorName(ExecutionState& state);

    // internal [[prototype]]
    virtual bool setPrototype(ExecutionState& state, const Value& proto);

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P);
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P);
    // callback function should skip un-Enumerable property if needs
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true);
    // ToLength(Get(obj, "length"))
    virtual uint64_t length(ExecutionState& state);

    // https://www.ecma-international.org/ecma-262/10.0/#sec-isarray
    bool isArray(ExecutionState& state);

    virtual bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        return getOwnProperty(state, propertyName).hasValue();
    }

    virtual Value getOwnPropertyDescriptor(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        ObjectGetResult desc = getOwnProperty(state, propertyName);
        return desc.fromPropertyDescriptor(state, this);
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-hasproperty
    // basically there is no difference [[hasProperty]] and [[get]]
    // but on ProxyObject, we need to call "has" handler. that's all
    // if you need testing key existence. convert ObjectHasPropertyResult into bool ex) if (O->hasProperty(...)) {}
    // I define return type as ObjectHasPropertyResult for removeing search same object twice by same key, same time.
    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName);
    virtual ObjectHasPropertyResult hasIndexedProperty(ExecutionState& state, const Value& propertyName);

    typedef VectorWithInlineStorage<32, Value, GCUtil::gc_malloc_allocator<Value>> OwnPropertyKeyVector;
    typedef VectorWithInlineStorage<32, std::pair<Value, ObjectStructurePropertyDescriptor>, GCUtil::gc_malloc_allocator<std::pair<Value, ObjectStructurePropertyDescriptor>>> OwnPropertyKeyAndDescVector;
    virtual OwnPropertyKeyVector ownPropertyKeys(ExecutionState& state);
    virtual bool canUseOwnPropertyKeysFastPath()
    {
        return true;
    }
    OwnPropertyKeyAndDescVector ownPropertyKeysFastPath(ExecutionState& state);

    ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P)
    {
        return get(state, P, Value(this));
    }
    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver);
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    bool isRegExp(ExecutionState& state);

    void setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    void setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver);
    void defineOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
    {
        if (!defineOwnProperty(state, P, desc)) {
            throwCannotDefineError(state, P.toObjectStructurePropertyName(state));
        }
    }

    void deleteOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P)
    {
        if (!deleteOwnProperty(state, P)) {
            throwCannotDefineError(state, P.toObjectStructurePropertyName(state));
        }
    }

    ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property)
    {
        return getIndexedProperty(state, property, this);
    }
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property, const Value& receiver);

    // this function returns value instead of ObjectGetResult
    // can be used for interpreter
    virtual Value getIndexedPropertyValue(ExecutionState& state, const Value& property, const Value& receiver)
    {
        return getIndexedProperty(state, property, receiver).value(state, receiver);
    }

    bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value)
    {
        return setIndexedProperty(state, property, value, this);
    }
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value, const Value& receiver);
    void setIndexedPropertyThrowsException(ExecutionState& state, const Value& property, const Value& value)
    {
        if (!setIndexedProperty(state, property, value, this)) {
            throwCannotDefineError(state, ObjectStructurePropertyName(state, property.toString(state)));
        }
    }

    void addPrivateField(ExecutionState& state, Object* contextObject, AtomicString propertyName, const Value& value);
    void addPrivateMethod(ExecutionState& state, Object* contextObject, AtomicString propertyName, FunctionObject* fn);
    void addPrivateAccessor(ExecutionState& state, Object* contextObject, AtomicString propertyName, FunctionObject* callback, bool isGetter, bool isSetter);
    Value getPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, bool shouldReferOuterClass = true);
    void setPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, const Value& value, bool shouldReferOuterClass = true);
    bool hasPrivateMember(ExecutionState& state, Object* contextObject, AtomicString propertyName, bool shouldReferOuterClass = true);

    void markThisObjectDontNeedStructureTransitionTable()
    {
        m_structure = m_structure->convertToNonTransitionStructure();
    }

    static void nextIndexForward(ExecutionState& state, Object* obj, const int64_t cur, const int64_t len, int64_t& nextIndex);
    static void nextIndexBackward(ExecutionState& state, Object* obj, const int64_t cur, const int64_t end, int64_t& nextIndex);

    virtual void sort(ExecutionState& state, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp);
    virtual void toSorted(ExecutionState& state, Object* target, uint64_t length, const std::function<bool(const Value& a, const Value& b)>& comp);

    virtual bool isInlineCacheable()
    {
        if (UNLIKELY(hasRareData())) {
            return rareData()->m_isInlineCacheable;
        }
        return true;
    }

    ObjectRareData* ensureRareData()
    {
        if (!hasRareData()) {
            m_prototype = (Object*)(new ObjectRareData(this));
        }
        return rareData();
    }

    inline bool hasRareData() const
    {
        return (m_prototype != nullptr && m_prototype->isObjectRareData());
    }

    virtual bool isEverSetAsPrototypeObject() const
    {
        if (LIKELY(!hasRareData())) {
            return false;
        } else {
            return rareData()->m_isEverSetAsPrototypeObject;
        }
    }

    bool isSpreadArray() const
    {
        if (LIKELY(!hasRareData())) {
            return false;
        }
        return isArrayObject() && rareData()->m_isSpreadArrayObject;
    }

    void* extraData()
    {
        if (hasRareData()) {
            auto rd = ensureRareData();
            if (UNLIKELY(rd->m_hasExtendedExtraData)) {
                return rd->m_extendedExtraData->m_extraData;
            }
            return rd->m_extraData;
        }
        return nullptr;
    }

    void setExtraData(void* e)
    {
        auto rd = ensureRareData();
        if (UNLIKELY(rd->m_hasExtendedExtraData)) {
            rd->m_extendedExtraData->m_extraData = e;
        } else {
            rd->m_extraData = e;
        }
    }

#if defined(ESCARGOT_ENABLE_TEST)
    bool isHTMLDDA()
    {
        return hasRareData() ? rareData()->m_isHTMLDDA : false;
    }

    void setIsHTMLDDA()
    {
        ensureRareData()->m_isHTMLDDA = true;
    }
#endif

    Object* ensureInternalSlot(ExecutionState& state)
    {
        ASSERT(!isArrayObject());
        ensureRareData();
        if (!internalSlot()) {
            setInternalSlot(new Object(state, Object::PrototypeIsNull));
        }
        return internalSlot();
    }

    Object* internalSlot()
    {
        ASSERT(!isArrayObject());
        ASSERT(hasRareData());
        return rareData()->m_internalSlot;
    }

    bool hasInternalSlot()
    {
        if (isArrayObject()) {
            return false;
        }
        return hasRareData() && rareData()->m_internalSlot;
    }

    void setInternalSlot(Object* object)
    {
        ASSERT(!isArrayObject());
        ensureRareData()->m_internalSlot = object;
    }

    virtual Context* getFunctionRealm(ExecutionState& state)
    {
        // https://www.ecma-international.org/ecma-262/6.0/#sec-getfunctionrealm
        return state.context();
    }

    static Value call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, Value* argv);
    static Value construct(ExecutionState& state, const Value& constructor, const size_t argc, Value* argv, Object* newTarget = nullptr);
    static bool setIntegrityLevel(ExecutionState& state, Object* O, bool isSealed);
    static bool testIntegrityLevel(ExecutionState& state, Object* O, bool isSealed);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-getmethod
    static Value getMethod(ExecutionState& state, const Value& O, const ObjectPropertyName& propertyName);
    Optional<Object*> getMethod(ExecutionState& state, const ObjectPropertyName& propertyName); // returns nullptr or callable

    // https://www.ecma-international.org/ecma-262/#sec-getv
    static Value getV(ExecutionState& state, const Value& O, const ObjectPropertyName& propertyName);

    static Object* getPrototypeFromConstructor(ExecutionState& state, Object* constructor, Object* (*intrinsicDefaultProtoGetter)(ExecutionState& state, Context* constructorRealm));

    bool hasInstance(ExecutionState& state, const Value O);

    static bool isCompatiblePropertyDescriptor(ExecutionState& state, bool extensible, const ObjectPropertyDescriptor& desc, const ObjectGetResult& current);

    static void throwCannotDefineError(ExecutionState& state, const ObjectStructurePropertyName& P);
    static void throwCannotWriteError(ExecutionState& state, const ObjectStructurePropertyName& P);
    static void throwCannotDeleteError(ExecutionState& state, const ObjectStructurePropertyName& P);
    static ArrayObject* createArrayFromList(ExecutionState& state, const uint64_t& size, const Value* buffer);
    static ArrayObject* createArrayFromList(ExecutionState& state, const ValueVector& elements);
    static ValueVector createListFromArrayLike(ExecutionState& state, Value obj, uint8_t types = static_cast<uint8_t>(ElementTypes::ALL));
    static ValueVectorWithInlineStorage enumerableOwnProperties(ExecutionState& state, Object* object, EnumerableOwnPropertiesType kind);

    // this function differ with defineOwnProperty.
    // !hasOwnProperty(state, P) is needed for success
    bool defineNativeDataAccessorProperty(ExecutionState& state, const ObjectPropertyName& P, ObjectPropertyNativeGetterSetterData* data, const Value& objectInternalData);

    IteratorObject* values(ExecutionState& state);
    IteratorObject* keys(ExecutionState& state);
    IteratorObject* entries(ExecutionState& state);

    Value speciesConstructor(ExecutionState& state, const Value& defaultConstructor);
    virtual void markAsPrototypeObject(ExecutionState& state);

    ALWAYS_INLINE Value uncheckedGetOwnDataProperty(size_t idx)
    {
        ASSERT(m_structure->readProperty(idx).m_descriptor.isDataProperty());
        return m_values[idx];
    }

    ALWAYS_INLINE void uncheckedSetOwnDataProperty(size_t idx, const Value& newValue)
    {
        ASSERT(m_structure->readProperty(idx).m_descriptor.isDataProperty());
        m_values[idx] = newValue;
    }

    virtual void addFinalizer(FinalizerFunction fn, void* data);
    virtual bool removeFinalizer(FinalizerFunction fn, void* data);

    struct FastLookupSymbolResult {
        FastLookupSymbolResult()
            : m_wasSucessful(false)
            , m_propertyIndexInStructure(0)
        {
        }

        FastLookupSymbolResult(bool wasSucessful, Object* o, size_t idx)
            : m_wasSucessful(wasSucessful)
            , m_matchedObject(o)
            , m_propertyIndexInStructure(idx)
        {
        }

        bool wasSucessful() const
        {
            return m_wasSucessful;
        }

        Optional<Object*> matchedObject() const
        {
            return m_matchedObject;
        }

        size_t propertyIndexInStructure() const
        {
            return m_propertyIndexInStructure;
        }

        Value value(ExecutionState& state, Object* receiver)
        {
            ASSERT(wasSucessful());
            if (m_matchedObject) {
                return m_matchedObject->getOwnPropertyUtilForObject(state, m_propertyIndexInStructure, Value(receiver));
            } else {
                return Value();
            }
        }

    private:
        bool m_wasSucessful;
        Optional<Object*> m_matchedObject;
        size_t m_propertyIndexInStructure;
    };
    FastLookupSymbolResult fastLookupForSymbol(ExecutionState& state, Symbol* s, Optional<Object*> protochainSearchStopAt = nullptr);

    // special util function for adding property in builtins
    void directDefineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

protected:
    static inline void fillGCDescriptor(GC_word* desc)
    {
        GC_set_bit(desc, GC_WORD_OFFSET(Object, m_structure));
        GC_set_bit(desc, GC_WORD_OFFSET(Object, m_prototype));
        GC_set_bit(desc, GC_WORD_OFFSET(Object, m_values));
    }

    Object()
        : m_structure(nullptr)
        , m_prototype(nullptr)
    {
        // dummy default constructor
        // only called by VMInstance::initialize to set tag value
    }

    explicit Object(ExecutionState& state, Object* proto, size_t defaultSpace);
    // ctor for ObjectTemplate
    explicit Object(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto);

    bool defineOwnPropertyMethod(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

    inline ObjectRareData* rareData() const
    {
        ASSERT(hasRareData());
        return (ObjectRareData*)m_prototype;
    }

    inline bool hasExtendedExtraData() const
    {
        return (hasRareData() && rareData()->m_hasExtendedExtraData);
    }

    ObjectExtendedExtraData* extendedExtraData()
    {
        ASSERT(hasExtendedExtraData());
        return rareData()->m_extendedExtraData;
    }

    ObjectExtendedExtraData* ensureExtendedExtraData()
    {
        auto rd = ensureRareData();
        if (!rd->m_hasExtendedExtraData) {
            rd->m_extendedExtraData = new ObjectExtendedExtraData(rd->m_extraData);
            rd->m_hasExtendedExtraData = true;
        }
        return rd->m_extendedExtraData;
    }

    ObjectStructure* m_structure;
    Object* m_prototype;
    ObjectPropertyValueVector m_values;

    COMPILE_ASSERT(sizeof(TightVectorWithNoSize<EncodedValue, GCUtil::gc_malloc_allocator<EncodedValue>>) == sizeof(size_t) * 1, "");

    ObjectStructure* structure() const
    {
        return m_structure;
    }

    ALWAYS_INLINE Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx)
    {
        return getOwnDataPropertyUtilForObject(state, idx, this);
    }

    ALWAYS_INLINE Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& receiver)
    {
        ASSERT(m_structure->readProperty(idx).m_descriptor.isDataProperty());
        const ObjectStructureItem& item = m_structure->readProperty(idx);
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            return m_values[idx];
        } else {
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this, receiver, m_values[idx]);
        }
    }

    ALWAYS_INLINE bool setOwnDataPropertyUtilForObjectInner(ExecutionState& state, size_t idx, const ObjectStructureItem& item, const Value& newValue, const Value& receiver)
    {
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            m_values[idx] = newValue;
            return true;
        } else {
#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
            EncodedValue t = m_values[idx];
            bool ret = item.m_descriptor.nativeGetterSetterData()->m_setter(state, this, receiver, t, newValue);
            m_values[idx] = t;
            return ret;
#else
            return item.m_descriptor.nativeGetterSetterData()->m_setter(state, this, receiver, m_values[idx], newValue);
#endif
        }
    }

    ALWAYS_INLINE bool setOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(idx);
        if (LIKELY(item.m_descriptor.isWritable())) {
            return setOwnDataPropertyUtilForObjectInner(state, idx, item, newValue, receiver);
        } else {
            return false;
        }
    }

    Value getOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& receiver);
    Value getOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
                return m_values[idx];
            } else {
                return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this, receiver, m_values[idx]);
            }
        } else {
            return getOwnPropertyUtilForObjectAccCase(state, idx, receiver);
        }
    }

    NEVER_INLINE Value getOwnNonPlainDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            ASSERT(!item.m_descriptor.isPlainDataProperty());
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this, receiver, m_values[idx]);
        } else {
            return getOwnPropertyUtilForObjectAccCase(state, idx, receiver);
        }
    }

    bool setOwnPropertyUtilForObjectAccCase(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver);
    ALWAYS_INLINE bool setOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(idx);
        if (LIKELY(item.m_descriptor.isDataProperty())) {
            return setOwnDataPropertyUtilForObject(state, idx, newValue, receiver);
        } else {
            return setOwnPropertyUtilForObjectAccCase(state, idx, newValue, receiver);
        }
    }

    ALWAYS_INLINE void setOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, size_t idx, const Value& newValue, const Value& receiver)
    {
        if (UNLIKELY(!setOwnPropertyUtilForObject(state, idx, newValue, receiver) && state.inStrictMode())) {
            throwCannotWriteError(state, m_structure->readProperty(idx).m_propertyName);
        }
    }

    void setGlobalIntrinsicObject(ExecutionState& state, bool isPrototype = false);

    void deleteOwnProperty(ExecutionState& state, size_t idx);

    void markAsNonInlineCachable();

    void tryToShrinkFinalizers();

    // redefine function used only for builtin installation
    void redefineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);
};

class DerivedObject : public Object {
    // DerivedObject is a pure virtual class for sub classes of `Object`

public:
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

protected:
    DerivedObject()
        : Object()
    {
    }
    DerivedObject(ExecutionState& state)
        : Object(state)
    {
    }
    DerivedObject(ExecutionState& state, Object::PrototypeIsNullTag)
        : Object(state, Object::PrototypeIsNull)
    {
    }
    DerivedObject(ExecutionState& state, Object* proto)
        : Object(state, proto)
    {
    }
    DerivedObject(ExecutionState& state, Object* proto, size_t defaultSpace)
        : Object(state, proto, defaultSpace)
    {
    }
    DerivedObject(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto)
        : Object(structure, std::move(values), proto)
    {
    }
};
} // namespace Escargot

#endif
