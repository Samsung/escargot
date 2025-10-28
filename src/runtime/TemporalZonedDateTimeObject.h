#if defined(ENABLE_TEMPORAL)
/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTemporalZonedDateTimeObject__
#define __EscargotTemporalZonedDateTimeObject__

#include "runtime/TemporalObject.h"
#include "util/ISO8601.h"

namespace Escargot {

class TemporalZonedDateTimeObject : public DerivedObject {
public:
    class ComputedTimeZone {
    public:
        ComputedTimeZone(String* timeZoneName = String::emptyString(), int64_t offset = 0)
            : m_timeZoneName(timeZoneName)
            , m_offset(offset)
        {
        }

        String* timeZoneName()
        {
            return m_timeZoneName;
        }

        int64_t offset()
        {
            return m_offset;
        }

    private:
        String* m_timeZoneName;
        int64_t m_offset;
    };

    TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, TimeZone timeZone, Calendar calendar);
    TemporalZonedDateTimeObject(ExecutionState& state, Object* proto, Int128 epochNanoseconds, ComputedTimeZone timeZone, Calendar calendar);

    virtual bool isTemporalZonedDateTimeObject() const override
    {
        return true;
    }

    ISO8601::PlainDate plainDate() const
    {
        return m_plainDateTime->plainDate();
    }

    ISO8601::PlainTime plainTime() const
    {
        return m_plainDateTime->plainTime();
    }

    ISO8601::PlainDateTime plainDateTime() const
    {
        return *m_plainDateTime;
    }

    Int128 epochNanoseconds() const
    {
        return *m_epochNanoseconds;
    }

    Calendar calendarID() const
    {
        return m_calendarID;
    }

    ComputedTimeZone timeZone() const
    {
        return m_timeZone;
    }

    // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
    String* toString(ExecutionState& state, Value options);

private:
    void init(ExecutionState& state, ComputedTimeZone timeZone);
    Int128* m_epochNanoseconds;
    ISO8601::PlainDateTime* m_plainDateTime; // stores timezone applied value
    ComputedTimeZone m_timeZone;
    Calendar m_calendarID;
    UCalendar* m_icuCalendar;
};

} // namespace Escargot

#endif
#endif
