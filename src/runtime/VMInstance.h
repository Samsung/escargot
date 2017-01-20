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

    void setTimezone(icu::TimeZone* tz)
    {
        m_timezone = tz->clone();
    }

    icu::UnicodeString timezoneID() const
    {
        return m_timezoneID;
    }

    void setTimezoneID(icu::UnicodeString id)
    {
        m_timezoneID = id;
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }
    void* operator new[](size_t size) = delete;

protected:
    icu::Locale m_locale;
    icu::TimeZone* m_timezone;
    icu::UnicodeString m_timezoneID;
};
}

#endif
