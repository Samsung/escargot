/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotReloadableString__
#define __EscargotReloadableString__

#if defined(ENABLE_RELOADABLE_STRING)

#include "runtime/String.h"
#include "runtime/CompressibleHeapPointer.h"

namespace Escargot {

class VMInstance;

class ReloadableString : public String {
    friend class VMInstance;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    friend int getValidValueInReloadableString(void*, GC_mark_pair*);
#endif

public:
    ReloadableString(VMInstance* instance, bool is8Bit, size_t stringLength, void* callbackData, void* (*loadCallback)(void* callbackData), void (*unloadCallback)(void* memoryPtr, void* callbackData));

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    virtual bool isReloadableString() override
    {
        return true;
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        if (isUnloaded()) {
            load();
        }

        // add refCount pointer to count its usage in StringBufferAccessData
        return StringBufferAccessData(m_bufferData.has8BitContent, m_bufferData.length, const_cast<void*>(m_bufferData.buffer), &m_refCount);
    }

    bool isUnloaded()
    {
        return m_isUnloaded;
    }

    size_t refCount() const
    {
        return m_refCount;
    }

    void* operator new(size_t);
    void* operator new[](size_t) = delete;
    void operator delete[](void*) = delete;

    bool unload();
    void load();

private:
    void initBufferAccessData(void* data, size_t len, bool is8bit);
    ATTRIBUTE_NO_SANITIZE_ADDRESS bool unloadWorker();

    size_t unloadedBufferSize()
    {
        if (isUnloaded()) {
            return 0;
        } else {
            return m_bufferData.length * (m_bufferData.has8BitContent ? 1 : 2);
        }
    }

    bool m_isOwnerMayFreed;
    bool m_isUnloaded;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    CompressibleHeapPointer<VMInstance> m_vmInstance;
#endif
    size_t m_refCount; // reference count representing the usage of this ReloadableString
#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
    VMInstance* m_vmInstance;
#endif
    void* m_callbackData;
    void* (*m_stringLoadCallback)(void* callbackData);
    void (*m_stringUnloadCallback)(void* memoryPtr, void* callbackData);
};
} // namespace Escargot

#endif // ENABLE_RELOADABLE_STRING

#endif
