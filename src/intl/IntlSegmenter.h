#if defined(ENABLE_ICU) && defined(ENABLE_INTL) && defined(ENABLE_INTL_SEGMENTER)
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


#ifndef __EscargotIntlSegmenter__
#define __EscargotIntlSegmenter__

#include "runtime/Object.h"
#include "runtime/IteratorObject.h"

namespace Escargot {

class IntlSegmentsIteratorObject;

class IntlSegmenterObject : public DerivedObject {
public:
    IntlSegmenterObject(ExecutionState& state, Value locales, Value options);
    IntlSegmenterObject(ExecutionState& state, Object* proto, Value locales, Value options);

    virtual bool isIntlSegmenterObject() const override
    {
        return true;
    }

    Object* resolvedOptions(ExecutionState& state);
    IntlSegmentsObject* segment(ExecutionState& state, String* string);

protected:
    String* m_locale;
    String* m_granularity;

    UBreakIterator* m_icuSegmenter;
};

class IntlSegmentsObject : public DerivedObject {
public:
    IntlSegmentsObject(ExecutionState& state, String* string, String* u16String, String* granularity, UBreakIterator* icuSegmenter);

    virtual bool isIntlSegmentsObject() const override
    {
        return true;
    }

    Value containing(ExecutionState& state, const Value& index);
    IntlSegmentsIteratorObject* createIteratorObject(ExecutionState& state);

    String* string() const
    {
        return m_string;
    }

    String* u16String() const
    {
        return m_u16String;
    }

    String* granularity() const
    {
        return m_granularity;
    }

protected:
    String* m_string;
    String* m_u16String;
    String* m_granularity;
    UBreakIterator* m_icuSegmenter;
};

class IntlSegmentsIteratorObject : public IteratorObject {
public:
    IntlSegmentsIteratorObject(ExecutionState& state, IntlSegmentsObject* segments, UBreakIterator* icuSegmenter);

    virtual bool isIntlSegmentsIteratorObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override;

private:
    IntlSegmentsObject* m_segments;
    UBreakIterator* m_icuSegmenter;
};

} // namespace Escargot
#endif
#endif
