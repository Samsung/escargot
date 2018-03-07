/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#if ESCARGOT_ENABLE_PROXY

#ifndef __EscargotProxyObject__
#define __EscargotProxyObject__

#include "runtime/Object.h"

namespace Escargot {

class ProxyObject : public Object {
public:
    ProxyObject(ExecutionState& state);

    virtual bool isProxyObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Proxy";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};
}

#endif // __EscargotProxyObject__
#endif // ESCARGOT_ENABLE_PROXY
