#include "Escargot.h"
#include "DateObject.h"
#include "Context.h"

namespace Escargot {

static constexpr double hoursPerDay = 24.0;
static constexpr double minutesPerHour = 60.0;
static constexpr double secondsPerMinute = 60.0;
static constexpr double secondsPerHour = secondsPerMinute * minutesPerHour;
static constexpr double msPerSecond = 1000.0;
static constexpr double msPerMinute = msPerSecond * secondsPerMinute;
static constexpr double msPerHour = msPerSecond * secondsPerHour;
static constexpr double msPerDay = msPerHour * hoursPerDay;

DateObject::DateObject(ExecutionState& state, time64IncludingNaN val)
    : Object(state)
    , m_primitiveValue(val)
{
    setPrototype(state, state.context()->globalObject()->datePrototype());
}

void DateObject::setPrimitiveValueAsCurrentTime()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    m_primitiveValue = time.tv_sec * msPerSecond + floor(time.tv_nsec / 1000000);
}
}
