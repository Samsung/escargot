#include "Escargot.h"
#include "VMInstance.h"

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_objectRareDataTag;
extern size_t g_stringTag;

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
    g_stringTag = *((size_t*)String::emptyString);
    ObjectRareData data(nullptr);
    g_objectRareDataTag = *((size_t*)&data);
}
}
