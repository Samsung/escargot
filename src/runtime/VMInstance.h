/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotVMInstance__
#define __EscargotVMInstance__

#include "runtime/Context.h"

namespace Escargot {

class VMInstance : public gc {
public:
    VMInstance();

    icu::TimeZone* timezone() const
    {
        return m_timezone;
    }

    void setTimezone()
    {
        if (m_timezoneID == "") {
            icu::TimeZone* tz = icu::TimeZone::createDefault();
            ASSERT(tz != nullptr);
            tz->getID(m_timezoneID);
            delete tz;
        }
        m_timezone = (icu::TimeZone::createTimeZone(m_timezoneID))->clone();
    }

    void setTimezoneID(icu::UnicodeString id)
    {
        m_timezoneID = id;
    }

    DateObject* cachedUTC() const
    {
        return m_cachedUTC;
    }

    void setCachedUTC(DateObject* d)
    {
        m_cachedUTC = d;
    }

    void* operator new(size_t size)
    {
        static bool typeInited = false;
        static GC_descr descr;
        if (!typeInited) {
            GC_word obj_bitmap[GC_BITMAP_SIZE(VMInstance)] = { 0 };
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(VMInstance, m_cachedUTC));
            descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(VMInstance));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

protected:
    icu::Locale m_locale;
    icu::TimeZone* m_timezone;
    icu::UnicodeString m_timezoneID;
    DateObject* m_cachedUTC;
};
}

#endif
