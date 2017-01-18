#ifndef __EscargotObjectStructurePropertyDescriptor__
#define __EscargotObjectStructurePropertyDescriptor__

#include "runtime/ExecutionState.h"
#include "runtime/Value.h"

namespace Escargot {

typedef Value (*ObjectPropertyNativeGetter)(ExecutionState& state, Object* self, const Value& data);
typedef bool (*ObjectPropertyNativeSetter)(ExecutionState& state, Object* self, const Value& setterInputData, Value& objectInternalData);

struct ObjectPropertyNativeGetterSetterData {
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    bool m_isRemovedOnRedefineProperty : 1; // special flag for arguments object
    ObjectPropertyNativeGetter m_getter;
    ObjectPropertyNativeSetter m_setter;

    ObjectPropertyNativeGetterSetterData(bool isWritable, bool isEnumerable, bool isConfigurable,
                                         ObjectPropertyNativeGetter getter, ObjectPropertyNativeSetter setter, bool isRemovedOnRedefineProperty = false)
        : m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_isRemovedOnRedefineProperty(isRemovedOnRedefineProperty)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }
    void* operator new[](size_t size) = delete;
};

class ObjectStructurePropertyDescriptor {
    MAKE_STACK_ALLOCATED();

public:
    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1, // property can be only read, not written
        EnumerablePresent = 1 << 2, // property doesn't appear in (for .. in ..)
        ConfigurablePresent = 1 << 3, // property can't be deleted
        HasJSGetter = 1 << 4,
        HasJSSetter = 1 << 5,
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent,
        AccessorAllPresent = EnumerablePresent | ConfigurablePresent | HasJSGetter | HasJSSetter
    };

    static ObjectStructurePropertyDescriptor createDataDescriptor(PresentAttribute attribute = (PresentAttribute)(WritablePresent | EnumerablePresent | ConfigurablePresent))
    {
        return ObjectStructurePropertyDescriptor(attribute);
    }

    static ObjectStructurePropertyDescriptor createAccessorDescriptor(PresentAttribute attribute)
    {
        return ObjectStructurePropertyDescriptor(attribute, HasJSGetterSetter);
    }

    static ObjectStructurePropertyDescriptor createDataButHasNativeGetterSetterDescriptor(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
    {
        return ObjectStructurePropertyDescriptor(nativeGetterSetterData);
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

    bool isAccessorProperty() const
    {
        return !isDataProperty();
    }

    bool hasJSGetter() const
    {
        ASSERT(isAccessorProperty());
        return m_descriptorData.presentAttributes() & HasJSGetter;
    }

    bool hasJSSetter() const
    {
        ASSERT(isAccessorProperty());
        return m_descriptorData.presentAttributes() & HasJSSetter;
    }

    bool isPlainDataProperty() const
    {
        return m_descriptorData.mode() == PlainDataMode;
    }

    bool isNativeAccessorProperty() const
    {
        //ASSERT(isDataProperty());
        return m_descriptorData.mode() == HasDataButHasNativeGetterSetter;
    }

    bool operator==(const ObjectStructurePropertyDescriptor& desc) const
    {
        return m_descriptorData.m_data == desc.m_descriptorData.m_data;
    }

    bool operator!=(const ObjectStructurePropertyDescriptor& desc) const
    {
        return !operator==(desc);
    }

    ObjectPropertyNativeGetterSetterData* nativeGetterSetterData() const
    {
        ASSERT(isNativeAccessorProperty());
        return m_descriptorData.nativeGetterSetterData();
    }

protected:
    enum ObjectStructurePropertyDescriptorMode {
        PlainDataMode,
        HasJSGetterSetter,
        HasDataButHasNativeGetterSetter,
    };

    struct ObjectStructurePropertyDescriptorData {
        union {
            size_t m_data;
            ObjectPropertyNativeGetterSetterData* m_nativeGetterSetterData;
        };

        ObjectStructurePropertyDescriptorData(PresentAttribute attribute, ObjectStructurePropertyDescriptorMode mode)
        {
            m_data = 1;
            m_data |= (attribute & PresentAttribute::WritablePresent) ? 2 : 0; // 2
            m_data |= (attribute & PresentAttribute::EnumerablePresent) ? 4 : 0; // 4
            m_data |= (attribute & PresentAttribute::ConfigurablePresent) ? 8 : 0; // 8
            m_data |= (attribute & PresentAttribute::HasJSGetter) ? 16 : 0; // 16
            m_data |= (attribute & PresentAttribute::HasJSSetter) ? 32 : 0; // 32
            m_data |= (mode) ? 64 : 0; // 64
            ASSERT(mode < 2);
        }

        ObjectStructurePropertyDescriptorData(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
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
            if (LIKELY(m_data & 1)) {
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
                if (m_data & 16) {
                    a |= PresentAttribute::HasJSGetter;
                }
                if (m_data & 32) {
                    a |= PresentAttribute::HasJSSetter;
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

        ObjectStructurePropertyDescriptorMode mode() const
        {
            if (LIKELY(m_data & 1)) {
                return (ObjectStructurePropertyDescriptorMode)(m_data & 64);
            } else {
                return HasDataButHasNativeGetterSetter;
            }
        }
    };

    ObjectStructurePropertyDescriptorData m_descriptorData;

    ObjectStructurePropertyDescriptor(PresentAttribute attribute = (PresentAttribute)(WritablePresent | EnumerablePresent | ConfigurablePresent), ObjectStructurePropertyDescriptorMode mode = PlainDataMode)
        : m_descriptorData(attribute, mode)
    {
        if (m_descriptorData.mode() == HasJSGetterSetter) {
            ASSERT(!(attribute & WritablePresent));
        }

        if (m_descriptorData.mode() == HasJSGetterSetter) {
            if (attribute & PresentAttribute::HasJSSetter) {
                // set writable flag for checking fast
                m_descriptorData = ObjectStructurePropertyDescriptorData((PresentAttribute)(WritablePresent | attribute), mode);
            }
        }
    }

    ObjectStructurePropertyDescriptor(ObjectPropertyNativeGetterSetterData* nativeGetterSetterData)
        : m_descriptorData(nativeGetterSetterData)
    {
    }
};

COMPILE_ASSERT(sizeof(ObjectStructurePropertyDescriptor) <= sizeof(size_t), "");
}

#endif
