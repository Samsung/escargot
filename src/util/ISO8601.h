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


#ifndef __EscargotISO8601__
#define __EscargotISO8601__

#include "runtime/BigInt.h"
#include "runtime/String.h"
#include "util/Int128.h"

namespace Escargot {
class ExecutionState;
namespace ISO8601 {

class Duration {
#define FOR_EACH_DURATION(F)         \
    F(years, Years, 0)               \
    F(months, Months, 1)             \
    F(weeks, Weeks, 2)               \
    F(days, Days, 3)                 \
    F(hours, Hours, 4)               \
    F(minutes, Minutes, 5)           \
    F(seconds, Seconds, 6)           \
    F(milliseconds, Milliseconds, 7) \
    F(microseconds, Microseconds, 8) \
    F(nanoseconds, Nanoseconds, 9)

    std::array<double, 10> m_data;

public:
    enum class Type : uint8_t {
#define DEFINE_TYPE(name, Name, index) Name,
        FOR_EACH_DURATION(DEFINE_TYPE)
#undef DEFINE_TYPE
    };

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldurationstring
    static Optional<Duration> parseDurationString(String* input);

    Duration()
    {
        m_data.fill(0);
    }

    Duration(const Duration& duration)
        : m_data(duration.m_data)
    {
    }

    static String* typeName(ExecutionState& state, Type t);
    Int128 totalNanoseconds(Duration::Type type) const;

    double operator[](size_t idx) const
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    double operator[](Duration::Type idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    double& operator[](size_t idx)
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    double& operator[](Duration::Type idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    std::array<double, 10>::const_iterator begin() const { return m_data.begin(); }
    std::array<double, 10>::const_iterator end() const { return m_data.end(); }

#define DEFINE_GETTER(name, Name, index) \
    double name() const { return m_data[index]; }
    FOR_EACH_DURATION(DEFINE_GETTER)
#undef DEFINE_GETTER

#define DEFINE_SETTER(name, Name, index) \
    void set##Name(double v) { m_data[index] = v; }
    FOR_EACH_DURATION(DEFINE_SETTER)
#undef DEFINE_SETTER
};


class PartialDuration {
    std::array<Optional<double>, 10> m_data;

public:
    // https://tc39.es/proposal-temporal/#sec-temporal-totemporalpartialdurationrecord
    static PartialDuration toTemporalPartialDurationRecord(ExecutionState& state, const Value& temporalDurationLike);

    Optional<double> operator[](size_t idx) const
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    Optional<double> operator[](Duration::Type idx) const
    {
        return operator[](static_cast<size_t>(idx));
    }

    Optional<double>& operator[](size_t idx)
    {
        return m_data[static_cast<unsigned>(idx)];
    }

    Optional<double>& operator[](Duration::Type idx)
    {
        return operator[](static_cast<size_t>(idx));
    }

    std::array<Optional<double>, 10>::const_iterator begin() const { return m_data.begin(); }
    std::array<Optional<double>, 10>::const_iterator end() const { return m_data.end(); }

#define DEFINE_GETTER(name, Name, index) \
    Optional<double> name() const { return m_data[index]; }
    FOR_EACH_DURATION(DEFINE_GETTER)
#undef DEFINE_GETTER

#define DEFINE_SETTER(name, Name, index) \
    void set##Name(Optional<double> v) { m_data[index] = v; }
    FOR_EACH_DURATION(DEFINE_SETTER)
#undef DEFINE_SETTER
};

} // namespace ISO8601
} // namespace Escargot

#endif
