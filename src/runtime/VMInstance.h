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
