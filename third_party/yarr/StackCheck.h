/*
 * Copyright (c) 2024-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

namespace WTF {

class StackCheck {
public:
    StackCheck()
    {
        m_stackLimit = Escargot::ThreadLocal::extendedStackLimit();
    }
    bool isSafeToRecurse()
    {
#ifdef STACK_GROWS_DOWN
        return m_stackLimit < (size_t)Escargot::currentStackPointer();
#else
        return m_stackLimit > (size_t)Escargot::currentStackPointer();
#endif
    }

private:
    size_t m_stackLimit;
};

} // namespace WTF

using WTF::StackCheck;
