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

struct ObjectRareData {
    bool m_isExtensible;
    bool m_isPlainObject; // tells it has __proto__ at first index
    bool m_isEverSetAsPrototypeObject;
    bool m_isFastModeArrayObject;
    ObjectRareData()
    {
        m_isExtensible = true;
        m_isPlainObject = true;
        m_isEverSetAsPrototypeObject = false;
        m_isFastModeArrayObject = true;
    }
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

public:
    JSGetterSetter(FunctionObject* getter, FunctionObject* setter)
        : m_getter(getter)
        , m_setter(setter)
    {
    }

    virtual Type type()
    {
        return JSGetterSetterType;
    }

    virtual bool isJSGetterSetter()
    {
        return true;
    }

    FunctionObject* getter() const
    {
        return m_getter;
    }

    FunctionObject* setter() const
    {
        return m_setter;
    }

protected:
    FunctionObject* m_getter;
    FunctionObject* m_setter;
};

class ObjectPropertyDescriptor {
public:
    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1,
        EnumerablePresent = 1 << 2,
        ConfigurablePresent = 1 << 3,
        NonWritablePresent = 1 << 4,
        NonEnumerablePresent = 1 << 5,
        NonConfigurablePresent = 1 << 6,
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent
    };

    // for plain data property
    explicit ObjectPropertyDescriptor(const Value& value, PresentAttribute attribute = ObjectPropertyDescriptor::NotPresent)
        : m_isDataProperty(true)
        , m_property(attribute)
        , m_value(value)
    {
        checkProperty();
    }

    explicit ObjectPropertyDescriptor(const JSGetterSetter& jsGetterSetter, PresentAttribute attribute)
        : m_isDataProperty(false)
        , m_property(attribute)
        , m_getterSetter(jsGetterSetter)
    {
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

    // TODO
    // for native acc. data property

    const Value& value() const
    {
        ASSERT(isDataProperty());
        return m_value;
    }

    bool isDataProperty() const
    {
        return m_isDataProperty;
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
        return isDataProperty() && isWritable() && isEnumerable() && isConfigurable();
    }

    bool isNotPresent() const
    {
        ASSERT(isDataProperty());
        return (m_property == NotPresent);
    }

    bool hasJSGetter() const
    {
        ASSERT(!isDataProperty());
        return m_getterSetter.m_getter;
    }

    bool hasJSSetter() const
    {
        ASSERT(!isDataProperty());
        return m_getterSetter.m_setter;
    }

    const JSGetterSetter& getterSetter() const
    {
        return m_getterSetter;
    }

    void setEnumerable(bool enumerable);
    void setConfigurable(bool configurable);
    void setWritable(bool writable);

    ObjectStructurePropertyDescriptor toObjectStructurePropertyDescriptor() const;

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
            if ((m_property & WritablePresent) | (m_property & NonWritablePresent)) {
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

    ObjectGetResult(Object* self, JSGetterSetter* getterSetter, bool isEnumerable, bool isConfigurable)
        : m_hasValue(true)
        , m_isWritable(false)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isDataProperty(false)
    {
        m_accessorData.m_jsGetterSetter = getterSetter;
        m_accessorData.m_self = self;
    }

    Value value(ExecutionState& state) const
    {
        if (LIKELY(m_isDataProperty))
            return m_value;
        return valueSlowCase(state);
    }

    JSGetterSetter* jsGetterSetter()
    {
        ASSERT(!m_isDataProperty);
        return m_accessorData.m_jsGetterSetter;
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

protected:
    bool m_hasValue : 1;
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    bool m_isDataProperty : 1;
    union {
        Value m_value;
        struct {
            Object* m_self;
            JSGetterSetter* m_jsGetterSetter;
        } m_accessorData;
    };
    Value valueSlowCase(ExecutionState& state) const;
};

#define ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER 1
#define ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
class Object : public PointerValue {
    friend class Context;
    friend class GlobalObject;
    friend class ByteCodeInterpreter;
    static Object* createBuiltinObjectPrototype(ExecutionState& state);

public:
    Object(ExecutionState& state);
    static Object* createFunctionPrototypeObject(ExecutionState& state, FunctionObject* function);
    virtual Type type()
    {
        return ObjectType;
    }

    virtual bool isObject()
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

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-isextensible
    bool isExtensible()
    {
        return m_rareData == nullptr ? true : m_rareData->m_isExtensible;
    }

    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-ordinary-object-internal-methods-and-internal-slots-preventextensions
    void preventExtensions()
    {
        ensureObjectRareData();
        m_rareData->m_isExtensible = true;
    }

    Value getPrototype(ExecutionState& state)
    {
        if (LIKELY(isPlainObject())) {
            return m_values[0];
        } else {
            return getPrototypeSlowCase(state);
        }
    }

    void setPrototype(ExecutionState& state, const Value& value)
    {
        if (LIKELY(isPlainObject())) {
            m_values[0] = value;
        } else {
            setPrototypeSlowCase(state, value);
        }
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    // enumeration every property!
    // callback function should skip un-Enumerable property if needs
    virtual void enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint32_t length(ExecutionState& state);

    bool hasOwnProperty(ExecutionState& state, const ObjectPropertyName& propertyName)
    {
        return getOwnProperty(state, propertyName).hasValue();
    }

    ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, Object* receiver);
    ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P)
    {
        return get(state, P, this);
    }

    void setThrowsException(ExecutionState& state, const ObjectPropertyName& P, const Value& v, Object* receiver);
    void setThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const Value& v, Object* receiver);
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


    void markThisObjectDontNeedStructureTransitionTable(ExecutionState& state)
    {
        ASSERT(structure()->inTransitionMode());
        m_structure = m_structure->escapeTransitionMode(state);
    }

    static double nextIndexForward(ExecutionState& state, Object* obj, const double cur, const double len, const bool skipUndefined);
    static double nextIndexBackward(ExecutionState& state, Object* obj, const double cur, const double end, const bool skipUndefined);

    virtual void sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp);

protected:
    Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea);
    void initPlainObject(ExecutionState& state);
    ObjectStructure* m_structure;
    ObjectRareData* m_rareData;
    TightVector<SmallValue, gc_malloc_ignore_off_page_allocator<SmallValue>> m_values;

    ObjectStructure* structure() const
    {
        return m_structure;
    }

    void ensureObjectRareData()
    {
        if (m_rareData == nullptr) {
            m_rareData = new ObjectRareData();
        }
    }

    Value uncheckedGetOwnDataProperty(ExecutionState& state, size_t idx)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        return m_values[idx];
    }

    void uncheckedSetOwnDataProperty(ExecutionState& state, size_t idx, const Value& newValue)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        m_values[idx] = newValue;
    }

    Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx)
    {
        return getOwnDataPropertyUtilForObject(state, idx, this);
    }

    Value getOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, Object* receiver)
    {
        ASSERT(m_structure->readProperty(state, idx).m_descriptor.isDataProperty());
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
            return m_values[idx];
        } else {
            return item.m_descriptor.nativeGetterSetterData()->m_getter(state, this);
        }
    }

    bool setOwnDataPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (LIKELY(item.m_descriptor.isWritable())) {
            if (LIKELY(item.m_descriptor.isPlainDataProperty())) {
                m_values[idx] = newValue;
                return true;
            } else {
                return item.m_descriptor.nativeGetterSetterData()->m_setter(state, this, newValue);
            }
        } else {
            return false;
        }
    }

    Value getOwnPropertyUtilForObject(ExecutionState& state, size_t idx, Object* receiver)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            return getOwnDataPropertyUtilForObject(state, idx, receiver);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    bool setOwnPropertyUtilForObject(ExecutionState& state, size_t idx, const Value& newValue)
    {
        const ObjectStructureItem& item = m_structure->readProperty(state, idx);
        if (item.m_descriptor.isDataProperty()) {
            return setOwnDataPropertyUtilForObject(state, idx, newValue);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void setOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, size_t idx, const Value& newValue)
    {
        if (UNLIKELY(!setOwnPropertyUtilForObject(state, idx, newValue))) {
            throwCannotWriteError(state, m_structure->readProperty(state, idx).m_propertyName);
        }
    }

    void throwCannotDefineError(ExecutionState& state, const PropertyName& P);
    void throwCannotWriteError(ExecutionState& state, const PropertyName& P);

    bool isPlainObject() const
    {
        if (LIKELY(m_rareData == nullptr)) {
            return true;
        } else {
            return m_rareData->m_isPlainObject;
        }
    }

    bool isEverSetAsPrototypeObject() const
    {
        if (LIKELY(m_rareData == nullptr)) {
            return false;
        } else {
            return m_rareData->m_isEverSetAsPrototypeObject;
        }
    }

    void markAsPrototypeObject()
    {
        ensureObjectRareData();
        m_rareData->m_isEverSetAsPrototypeObject = true;
    }

    void deleteOwnProperty(ExecutionState& state, size_t idx);

    Value getPrototypeSlowCase(ExecutionState& state);
    bool setPrototypeSlowCase(ExecutionState& state, const Value& value);

    bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, Object* receiver);
};
}

#endif
