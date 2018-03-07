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

#include "Escargot.h"
#include "ProxyObject.h"
#include "Context.h"
#include "JobQueue.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

ProxyObject::ProxyObject(ExecutionState& state)
    : Object(state)
{
}

void* ProxyObject::operator new(size_t size)
{
    static GC_descr descr;

    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}

#endif // ESCARGOT_ENABLE_PROXY
