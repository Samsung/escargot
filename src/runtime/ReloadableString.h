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

namespace Escargot {

class VMInstance;

class ReloadableString : public String {
    friend class VMInstance;

public:
    ReloadableString(VMInstance* instance, bool is8Bit, size_t stringLength, void* callbackData, void* (*loadCallback)(void* callbackData), void (*unloadCallback)(void* memoryPtr, void* callbackData));

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    virtual const LChar* characters8() const override
    {
        return (const LChar*)bufferAccessData().buffer;
    }

    virtual const char16_t* characters16() const override
    {
        return (const char16_t*)bufferAccessData().buffer;
    }

    virtual bool isReloadableString() override
    {
        return true;
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        if (isUnloaded()) {
            load();
        }
        return StringBufferAccessData(m_bufferData.has8BitContent, m_bufferData.length, const_cast<void*>(m_bufferData.buffer));
    }

    bool isUnloaded()
    {
        return m_isUnloaded;
    }

    void* operator new(size_t);
    void* operator new[](size_t) = delete;
    void operator delete[](void*) = delete;

    bool unload();
    void load();

private:
    void initBufferAccessData(void* data, size_t len, bool is8bit);
    ATTRIBUTE_NO_SANITIZE_ADDRESS bool unloadWorker(void* callerSP);

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
    VMInstance* m_vmInstance;
    void* m_callbackData;
    void* (*m_stringLoadCallback)(void* callbackData);
    void (*m_stringUnloadCallback)(void* memoryPtr, void* callbackData);
};
} // namespace Escargot

#endif // ENABLE_RELOADABLE_STRING

#endif
