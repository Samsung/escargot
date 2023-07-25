/*
 * Copyright (c) 2023-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotException__
#define __EscargotException__
namespace Escargot {

class ErrorObject;

class Exception : public PointerValue {
public:
    explicit Exception(const Value& err)
        : m_typeTag(POINTER_VALUE_EXCEPTION_TAG_IN_DATA)
        : m_error(err)
    {
    }

    Value errorValue() const
    {
        return m_error;
    }

private:
    size_t m_typeTag;
    Value m_error;
};

class ValueOrThrow {
public:
    // should be allocated only in the stack
    MAKE_STACK_ALLOCATED();

    ValueOrThrow()
        : m_value()
    {
    }

    ValueOrThrow(const Value& val)
        : m_value(val)
    {
        ASSERT(!val.isPointerValue() || !val.asPointerValue()->isException());
    }

    ValueOrThrow(Exception* ex)
        : m_value(ex)
    {
    }

    bool isThrow() const
    {
        return m_value.isException();
    }

    operator bool() const
    {
        return !isThrow();
    }

    operator Value() const
    {
        ASSERT(!isThrow());
        return m_value;
    }

    Value value() const
    {
        ASSERT(!isThrow());
        return m_value;
    }

    Value exceptionValue() const
    {
        ASSERT(isThrow());
        return m_value.asException();
    }

private:
    Value m_value;
};

} // namespace Escargot
#endif
