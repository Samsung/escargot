/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotValueAdapter__
#define __EscargotValueAdapter__

namespace Escargot {

class ValueRef;

inline ValueRef* toRef(const Value& v)
{
    ASSERT(!v.isEmpty());
    return reinterpret_cast<ValueRef*>(EncodedValue(v).payload());
}

inline Value toImpl(const ValueRef* v)
{
    ASSERT(v);
#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
    int64_t tmp = (int64_t)(int32_t)(size_t)v;
    return Value(EncodedValue::fromPayload((void*)tmp));
#else
    return Value(EncodedValue::fromPayload(v));
#endif
}

#define DEFINE_REF_CAST(Name)                   \
    class Name;                                 \
    inline Name* toImpl(Name##Ref* v)           \
    {                                           \
        return reinterpret_cast<Name*>(v);      \
    }                                           \
    inline Name##Ref* toRef(Name* v)            \
    {                                           \
        return reinterpret_cast<Name##Ref*>(v); \
    }

ESCARGOT_REF_LIST(DEFINE_REF_CAST);
#undef DEFINE_REF_CAST

} // namespace Escargot

#endif
