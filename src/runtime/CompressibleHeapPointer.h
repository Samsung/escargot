/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 or (at your option) any
 * later version.
 */

#ifndef __EscargotCompressibleHeapPointer__
#define __EscargotCompressibleHeapPointer__

#include "util/Optional.h"

namespace Escargot {

class Value;

// Copyable cage offsets for pointer slots that also travel through stack values
// and temporary vectors. Unlike heap-field pointers, decoding uses the thread's
// cage rather than the address of the slot.
// Native stack scanning recognizes cage offsets in either half of a word.
template <typename T>
class CompressiblePointer {
public:
    CompressiblePointer(Optional<T*> pointer = nullptr) { set(pointer); }

    void set(Optional<T*> pointer)
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        uintptr_t address = reinterpret_cast<uintptr_t>(pointer.unwrap());
        ASSERT(!pointer || (address - ThreadLocal::cageBase() <= UINT32_MAX && static_cast<uint32_t>(address)));
        m_pointer = static_cast<uint32_t>(address);
#else
        m_pointer = pointer;
#endif
    }

    Optional<T*> get() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_pointer ? reinterpret_cast<T*>(ThreadLocal::cageBase() + m_pointer) : nullptr;
#else
        return m_pointer;
#endif
    }

    ALWAYS_INLINE Optional<T*> getWithBase(uintptr_t base) const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        ASSERT(!(base & UINT32_MAX));
        return m_pointer ? reinterpret_cast<T*>(base + m_pointer) : nullptr;
#else
        return m_pointer;
#endif
    }

    CompressiblePointer& operator=(Optional<T*> pointer)
    {
        set(pointer);
        return *this;
    }
    CompressiblePointer& operator=(T* pointer)
    {
        set(pointer);
        return *this;
    }
    T* value() const
    {
        ASSERT(static_cast<bool>(*this));
        return get().value();
    }
    T* operator->() const { return value(); }
    template <typename Index>
    T& operator[](Index index) const { return value()[index]; }
    operator T*() const { return get().unwrap(); }
    operator bool() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_pointer != 0;
#else
        return m_pointer.hasValue();
#endif
    }
    operator Optional<T*>() const { return get(); }
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    uint32_t compressedPayload() const { return m_pointer; }
#endif

private:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    uint32_t m_pointer;
#else
    Optional<T*> m_pointer;
#endif
};

// Store a GC pointer as a native pointer or a four-byte cage offset, selected
// at compile time. In compressed builds the field's address supplies the high
// bits, so access needs no TLS load. Only use this as a field of a GC allocation.
// With supportsTag, bit zero is metadata and survives pointer assignments.
// Targets must be aligned to two bytes; the GC descriptor must mask the tag.
template <typename T, bool supportsTag = false>
class CompressibleHeapPointer {
public:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    using StorageType = uint32_t;
#else
    using StorageType = uintptr_t;
#endif

    CompressibleHeapPointer(Optional<T*> pointer = nullptr)
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        : m_low(0)
#endif
    {
        set(pointer);
    }

    CompressibleHeapPointer(const CompressibleHeapPointer&) = delete;
    CompressibleHeapPointer& operator=(const CompressibleHeapPointer&) = delete;

    void set(Optional<T*> pointer)
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        if (!pointer) {
            m_low &= supportsTag ? 1u : 0u;
            return;
        }
        uintptr_t address = reinterpret_cast<uintptr_t>(pointer.value());
        uintptr_t upper = reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX);
        ASSERT((address & ~uintptr_t(UINT32_MAX)) == upper);
        uint32_t low = static_cast<uint32_t>(address);
        ASSERT(low > (supportsTag ? 1u : 0u) && (!supportsTag || !(low & 1u)));
        m_low = low | (m_low & (supportsTag ? 1u : 0u));
#else
        m_pointer = pointer;
#endif
    }

    Optional<T*> get() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        if (!hasValue()) {
            return nullptr;
        }
        uintptr_t upper = reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX);
        return valueWithBase(upper);
#else
        return m_pointer;
#endif
    }

    CompressibleHeapPointer& operator=(Optional<T*> pointer)
    {
        set(pointer);
        return *this;
    }

    CompressibleHeapPointer& operator=(T* pointer)
    {
        set(pointer);
        return *this;
    }

    void reset() { set(nullptr); }

    T* value() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        ASSERT(hasValue());
        return valueWithBase(reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX));
#else
        ASSERT(m_pointer);
        return m_pointer.unwrap();
#endif
    }

    ALWAYS_INLINE Optional<T*> getWithBase(uintptr_t base) const
    {
        return hasValue() ? Optional<T*>(valueWithBase(base)) : Optional<T*>(nullptr);
    }

    ALWAYS_INLINE T* valueWithBase(uintptr_t base) const
    {
        ASSERT(hasValue());
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        ASSERT(base == (reinterpret_cast<uintptr_t>(this) & ~uintptr_t(UINT32_MAX)));
        return reinterpret_cast<T*>(base + (m_low & ~(supportsTag ? 1u : 0u)));
#else
        return m_pointer.unwrap();
#endif
    }

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    uint32_t compressedPayload() const { return m_low; }

    void setTag()
    {
        static_assert(supportsTag, "pointer must support a tag bit");
        m_low |= 1;
    }
    void clearTag()
    {
        static_assert(supportsTag, "pointer must support a tag bit");
        m_low &= ~uint32_t(1);
    }
    bool hasTag() const
    {
        static_assert(supportsTag, "pointer must support a tag bit");
        return m_low & 1;
    }

    // Offset one lies in the cage's reserved first page. The compressed GC
    // scanner ignores it, and it can represent a second empty state.
    void setTaggedEmpty()
    {
        static_assert(supportsTag, "pointer must support a tagged empty state");
        ASSERT(!hasValue());
        m_low = 1;
    }
    bool isTaggedEmpty() const
    {
        static_assert(supportsTag, "pointer must support a tagged empty state");
        return m_low == 1;
    }

#endif

    T* raw() const
    {
        Optional<T*> pointer = get();
        return pointer ? pointer.value() : nullptr;
    }

    T* operator->() const { return value(); }
    operator T*() const { return raw(); }
    operator Value() const;
    operator bool() const { return hasValue(); }
    bool hasValue() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_low > (supportsTag ? 1u : 0u);
#else
        return m_pointer.hasValue();
#endif
    }
    operator Optional<T*>() const { return get(); }

private:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    uint32_t m_low;
#else
    Optional<T*> m_pointer;
#endif
};

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
static_assert(sizeof(CompressibleHeapPointer<char>) == 4, "compressed pointer slot must be four bytes");
#else
static_assert(sizeof(CompressibleHeapPointer<char>) == sizeof(void*), "native pointer slot must retain its width");
#endif

} // namespace Escargot

#endif
