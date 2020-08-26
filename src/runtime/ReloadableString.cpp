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

#if defined(ENABLE_RELOADABLE_STRING)
#include "Escargot.h"
#include "ReloadableString.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

void* ReloadableString::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ReloadableString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ReloadableString, m_vmInstance));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ReloadableString, m_callbackData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ReloadableString, m_bufferData.buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ReloadableString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ReloadableString::ReloadableString(VMInstance* instance, bool is8Bit, size_t stringLength, void* callbackData, void* (*loadCallback)(void* callbackData), void (*unloadCallback)(void* memoryPtr, void* callbackData))
    : String()
    , m_isOwnerMayFreed(false)
    , m_isUnloaded(true)
    , m_vmInstance(instance)
    , m_callbackData(callbackData)
    , m_stringLoadCallback(loadCallback)
    , m_stringUnloadCallback(unloadCallback)
{
    m_bufferData.hasSpecialImpl = true;
    m_bufferData.has8BitContent = is8Bit;
    m_bufferData.length = stringLength;
    m_bufferData.buffer = nullptr;

    auto& v = instance->reloadableStrings();
    v.push_back(this);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        ReloadableString* self = (ReloadableString*)obj;
        if (!self->m_isUnloaded) {
            self->m_stringUnloadCallback(const_cast<void*>(self->m_bufferData.buffer), self->m_callbackData);
        }
        if (!self->m_isOwnerMayFreed) {
            auto& v = self->m_vmInstance->reloadableStrings();
            v.erase(std::find(v.begin(), v.end(), self));
        }
    },
                                   nullptr, nullptr, nullptr);
}

UTF8StringDataNonGCStd ReloadableString::toNonGCUTF8StringData(int options) const
{
    return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
}

UTF8StringData ReloadableString::toUTF8StringData() const
{
    return bufferAccessData().toUTF8String<UTF8StringData>();
}

UTF16StringData ReloadableString::toUTF16StringData() const
{
    auto data = bufferAccessData();
    if (data.has8BitContent) {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(data.length);
        for (size_t i = 0; i < data.length; i++) {
            ret[i] = data.uncheckedCharAtFor8Bit(i);
        }
        return ret;
    } else {
        return UTF16StringData(data.bufferAs16Bit, data.length);
    }
}

bool ReloadableString::unload()
{
    ASSERT(!m_isUnloaded);
    if (UNLIKELY(!m_bufferData.length)) {
        return false;
    }

    return unloadWorker(currentStackPointer());
}

bool ReloadableString::unloadWorker(void* callerSP)
{
#if defined(STACK_GROWS_DOWN)
    size_t* start = (size_t*)((size_t)callerSP & ~(sizeof(size_t) - 1));
    size_t* end = (size_t*)m_vmInstance->stackStartAddress();
#else
    size_t* start = (size_t*)m_vmInstance->stackStartAddress();
    size_t* end = (size_t*)((size_t)callerSP & ~(sizeof(size_t) - 1));
#endif

    while (start != end) {
        if (UNLIKELY(*start == (size_t)m_bufferData.buffer)) {
            // if there is reference on stack, we cannot unload string.
            return false;
        }
        start++;
    }

    m_stringUnloadCallback(const_cast<void*>(m_bufferData.buffer), m_callbackData);
    m_bufferData.buffer = nullptr;
    m_isUnloaded = true;

    return true;
}

void ReloadableString::load()
{
    ASSERT(m_isUnloaded);
    ASSERT(m_bufferData.length);

    m_isUnloaded = false;
    m_bufferData.buffer = m_stringLoadCallback(m_callbackData);
    if (UNLIKELY(m_bufferData.buffer == nullptr)) {
        ESCARGOT_LOG_ERROR("failed to load string(ReloadableString::load) %p\n", this);
        RELEASE_ASSERT_NOT_REACHED();
    }
}
}

#endif // ENABLE_RELOADABLE_STRING
