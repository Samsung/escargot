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

#ifndef __EscargotTemporalDurationObject__
#define __EscargotTemporalDurationObject__

#include "runtime/TemporalObject.h"
#include "util/ISO8601.h"

namespace Escargot {

class TemporalDurationObject : public DerivedObject {
public:
    TemporalDurationObject(ExecutionState& state, Object* proto,
                           const Value& years, const Value& months, const Value& weeks, const Value& days,
                           const Value& hours, const Value& minutes, const Value& seconds, const Value& milliseconds,
                           const Value& microseconds, const Value& nanoseconds);

    TemporalDurationObject(ExecutionState& state, const ISO8601::Duration& duration);

    virtual bool isTemporalDurationObject() const override
    {
        return true;
    }

    const ISO8601::Duration& duration() const
    {
        return m_duration;
    }

    double operator[](size_t idx) const
    {
        return m_duration[static_cast<unsigned>(idx)];
    }

    double operator[](ISO8601::Duration::Type idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    double& operator[](size_t idx)
    {
        return m_duration[static_cast<unsigned>(idx)];
    }

    double& operator[](ISO8601::Duration::Type idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    // https://tc39.es/proposal-temporal/#sec-durationsign
    int sign() const
    {
        for (const auto& v : m_duration) {
            if (v < 0) {
                return -1;
            }
            if (v > 0) {
                return 1;
            }
        }
        return 0;
    }

private:
    ISO8601::Duration m_duration;
};

} // namespace Escargot

#endif
#endif
