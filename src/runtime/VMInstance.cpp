#include "Escargot.h"
#include "VMInstance.h"

namespace Escargot {

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
}
}
