#ifndef __EscargotObject__
#define __EscargotObject__

#include "runtime/ObjectStructure.h"
#include "runtime/PointerValue.h"
#include "runtime/SmallValue.h"
#include "util/Vector.h"

namespace Escargot {

class FunctionObject;
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
    MAKE_STACK_ALLOCATED()
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

    virtual bool isErrorObject()
    {
        return false;
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

    class ObjectPropertyDescriptorForDefineOwnProperty {
    public:
        // for plain data property
        ObjectPropertyDescriptorForDefineOwnProperty(const Value& value, ObjectPropertyDescriptor::PresentAttribute attribute = ObjectPropertyDescriptor::AllPresent)
            : m_descriptor(ObjectPropertyDescriptor::createDataDescriptor(attribute))
            , m_value(value)
        {
        }

        // TODO
        // for native acc. data property
        // for js acc. property

        const Value& value() const
        {
            ASSERT(m_descriptor.isDataProperty());
            return m_value;
        }

        const ObjectPropertyDescriptor& descriptor() const
        {
            return m_descriptor;
        }

    protected:
        MAKE_STACK_ALLOCATED();
        ObjectPropertyDescriptor m_descriptor;
        Value m_value;
    };

    class ObjectGetResult {
    public:
        // TODO implement js getter case
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

        Value value() const
        {
            if (LIKELY(m_isDataProperty))
                return m_value;
            RELEASE_ASSERT_NOT_REACHED();
        }

        bool hasValue() const
        {
            return m_hasValue;
        }

        bool isWritable() const
        {
            ASSERT(hasValue());
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
        Value m_value;
    };

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    // enumeration every property!
    // callback function should skip un-Enumerable property if needs
    virtual void enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectPropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
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
    void defineOwnPropertyThrowsException(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc)
    {
        if (!defineOwnProperty(state, P, desc)) {
            throwCannotDefineError(state, P.toPropertyName(state));
        }
    }

    void defineOwnPropertyThrowsExceptionWhenStrictMode(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptorForDefineOwnProperty& desc)
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

protected:
    Object(ExecutionState& state, size_t defaultSpace, bool initPlainArea);
    void initPlainObject(ExecutionState& state);
    ObjectStructure* m_structure;
    ObjectRareData* m_rareData;
    Vector<SmallValue, gc_malloc_ignore_off_page_allocator<SmallValue>> m_values;

    ObjectStructure* structure() const
    {
        return m_structure;
    }
    bool checkPropertyAlreadyDefinedWithNonWritableInPrototype(ExecutionState& state, const ObjectPropertyName& P);
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
