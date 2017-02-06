#include "Escargot.h"
#include "VMInstance.h"

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_objectRareDataTag;

VMInstance::VMInstance()
{
    std::setlocale(LC_ALL, "en_US.utf8");
    // if you don't use localtime() or timegm() or etc. you should call tzset() to use tzname[]
    tzset();

    m_timezone = nullptr;
    if (getenv("TZ")) { // Usually used by StarFish
        m_timezoneID = getenv("TZ");
    } else { // Used by escargot standalone
        icu::TimeZone* tz = icu::TimeZone::createDefault();
        ASSERT(tz != nullptr);
        tz->getID(m_timezoneID);
    }

    if (!String::emptyString) {
        String::emptyString = new (NoGC) ASCIIString("");
    }

    DoubleInSmallValue temp(0);
    g_doubleInSmallValueTag = *((size_t*)&temp);

    {
        ASCIIString str("", 0);
        g_asciiStringTag = *((size_t*)&str);
    }
    {
        Latin1String str("", 0);
        g_latin1StringTag = *((size_t*)&str);
    }
    {
        UTF16String str(u"", 0);
        g_utf16StringTag = *((size_t*)&str);
    }
    {
        RopeString str;
        g_ropeStringTag = *((size_t*)&str);
    }
    {
        StringView str;
        g_stringViewTag = *((size_t*)&str);
    }

    ObjectRareData data(nullptr);
    g_objectRareDataTag = *((size_t*)&data);
}
}
