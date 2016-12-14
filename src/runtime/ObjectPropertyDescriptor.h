#ifndef __EscargotObjectPropertyDescriptor__
#define __EscargotObjectPropertyDescriptor__

#include "runtime/ExecutionState.h"
#include "runtime/Value.h"

namespace Escargot {

typedef Value (*ObjectPropertyNativeGetter)(ExecutionState& state, Object* self);
typedef bool (*ObjectPropertyNativeSetter)(ExecutionState& state, Object* self, const Value& newData);

// NOTE
// this is static data, so we don't needed gc
struct ObjectPropertyNativeGetterSetterData {
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    ObjectPropertyNativeGetter m_getter;
    ObjectPropertyNativeSetter m_setter;

    ObjectPropertyNativeGetterSetterData(bool isWritable, bool isEnumerable, bool isConfigurable,
                                         ObjectPropertyNativeGetter getter, ObjectPropertyNativeSetter setter)
        : m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_getter(getter)
        , m_setter(setter)
    {
    }
};

class ObjectPropertyDescriptor {
    MAKE_STACK_ALLOCATED();

public:
    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1, // property can be only read, not written
        EnumerablePresent = 1 << 2, // property doesn't appear in (for .. in ..)
        ConfigurablePresent = 1 << 3, // property can't be deleted
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent
    };

    static ObjectPropertyDescriptor createDataDescriptor(PresentAttribute attribute = (PresentAttribute)(WritablePresent | EnumerablePresent | ConfigurablePresent))
    {
        return ObjectPropertyDescriptor(attribute);
    }

    static ObjectPropertyDescriptor createAccessorDescriptor(PresentAttribute attribute = (PresentAttribute)(WritablePresent | EnumerablePresent | ConfigurablePresent))
    {
        return ObjectPropertyDescriptor(attribute, HasJSGetterSetter);
    }

    static ObjectPropertyDescriptor createDataButHasNativeGetterSetterDescriptor(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
    {
        return ObjectPropertyDescriptor(nativeGetterSetterData);
    }

    bool isPlainDataWritableEnumerableConfigurable() const
    {
        return isPlainDataProperty() && m_descriptorData.presentAttributes() == AllPresent;
    }

    bool isWritable() const
    {
        return m_descriptorData.presentAttributes() & WritablePresent;
    }

    bool isEnumerable() const
    {
        return m_descriptorData.presentAttributes() & EnumerablePresent;
    }

    bool isConfigurable() const
    {
        return m_descriptorData.presentAttributes() & ConfigurablePresent;
    }

    bool isDataProperty() const
    {
        return m_descriptorData.mode() <= HasDataButHasNativeGetterSetter;
    }

    bool isPlainDataProperty() const
    {
        return m_descriptorData.mode() == PlainDataMode;
    }

    bool isNativeAccessorProperty() const
    {
        ASSERT(isDataProperty());
        return m_descriptorData.mode() == HasDataButHasNativeGetterSetter;
    }

    bool isAccessorProperty() const
    {
        return m_descriptorData.mode() == HasJSGetterSetter;
    }

    bool operator==(const ObjectPropertyDescriptor& desc) const
    {
        // TODO compare getter, setter, prop
        ASSERT(isDataProperty());
        return m_descriptorData.m_data == desc.m_descriptorData.m_data;
    }

    bool operator!=(const ObjectPropertyDescriptor& desc) const
    {
        return !operator==(desc);
    }

    ObjectPropertyNativeGetterSetterData* nativeGetterSetterData() const
    {
        ASSERT(isNativeAccessorProperty());
        return m_descriptorData.nativeGetterSetterData();
    }

protected:
    enum ObjectPropertyDescriptorMode {
        PlainDataMode,
        HasDataButHasNativeGetterSetter,
        HasJSGetterSetter,
    };

    struct ObjectPropertyDescriptorData {
        union {
            size_t m_data;
            ObjectPropertyNativeGetterSetterData* m_nativeGetterSetterData;
        };

        ObjectPropertyDescriptorData(PresentAttribute attribute, ObjectPropertyDescriptorMode mode)
        {
            m_data = 1;
            m_data |= (attribute & PresentAttribute::WritablePresent) ? 2 : 0; // 2
            m_data |= (attribute & PresentAttribute::EnumerablePresent) ? 4 : 0; // 4
            m_data |= (attribute & PresentAttribute::ConfigurablePresent) ? 8 : 0; // 8
            m_data |= (mode << 4); // after
        }

        ObjectPropertyDescriptorData(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
        {
            m_nativeGetterSetterData = nativeGetterSetterData;
        }

        ObjectPropertyNativeGetterSetterData* nativeGetterSetterData() const
        {
            ASSERT((m_data & 1) == 0);
            return m_nativeGetterSetterData;
        }

        PresentAttribute presentAttributes() const
        {
            if (m_data & 1) {
                size_t a = 0;
                if (m_data & 2) {
                    a |= PresentAttribute::WritablePresent;
                }
                if (m_data & 4) {
                    a |= PresentAttribute::EnumerablePresent;
                }
                if (m_data & 8) {
                    a |= PresentAttribute::ConfigurablePresent;
                }
                return (PresentAttribute)a;
            } else {
                size_t a = 0;
                if (m_nativeGetterSetterData->m_isWritable)
                    a |= PresentAttribute::WritablePresent;
                if (m_nativeGetterSetterData->m_isEnumerable)
                    a |= PresentAttribute::EnumerablePresent;
                if (m_nativeGetterSetterData->m_isConfigurable)
                    a |= PresentAttribute::ConfigurablePresent;
                return (PresentAttribute)a;
            }
        }

        ObjectPropertyDescriptorMode mode() const
        {
            if (m_data & 1) {
                size_t mode = m_data >> 4;
                return (ObjectPropertyDescriptorMode)mode;
            } else {
                return HasDataButHasNativeGetterSetter;
            }
        }
    };

    ObjectPropertyDescriptorData m_descriptorData;

    ObjectPropertyDescriptor(PresentAttribute attribute = (PresentAttribute)(WritablePresent | EnumerablePresent | ConfigurablePresent), ObjectPropertyDescriptorMode mode = PlainDataMode)
        : m_descriptorData(attribute, mode)
    {
    }

    ObjectPropertyDescriptor(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
        : m_descriptorData(nativeGetterSetterData)
    {
    }
};

COMPILE_ASSERT(sizeof(ObjectPropertyDescriptor) <= sizeof(size_t), "");
}

#endif
