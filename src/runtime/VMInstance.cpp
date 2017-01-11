#include "Escargot.h"
#include "VMInstance.h"

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_stringTag;

VMInstance::VMInstance()
{
    std::setlocale(LC_ALL, "en_US.utf8");
    // if you don't use localtime() or timegm() or etc. you should call tzset() to use tzname[]
    tzset();

    m_timezone = nullptr;
    if (getenv("TZ")) {
        m_timezoneID = getenv("TZ");
    }
    m_timezoneID = "US/Pacific";

    auto temp = new DoubleInSmallValue(0);
    g_doubleInSmallValueTag = *((size_t*)temp);
    g_stringTag = *((size_t*)String::emptyString);
}
}
