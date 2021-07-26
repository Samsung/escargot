/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/PointerValue.h"
#include "runtime/ArrayObject.h"

namespace Escargot {

bool Global::inited;
Platform* Global::g_platform;

void Global::initialize(Platform* platform)
{
    // initialize should be invoked only once in the program
    static bool called_once = true;
    RELEASE_ASSERT(called_once && !inited);

    ASSERT(!g_platform);
    g_platform = platform;

    // initialize PointerValue tag values
    // tag values should be initialized once and not changed
    PointerValue::g_arrayObjectTag = ArrayObject().getTag();
    PointerValue::g_arrayPrototypeObjectTag = ArrayPrototypeObject().getTag();
    PointerValue::g_objectRareDataTag = ObjectRareData(nullptr).getTag();
    PointerValue::g_doubleInEncodedValueTag = DoubleInEncodedValue(0).getTag();

    called_once = false;
    inited = true;
}

void Global::finalize()
{
    // finalize should be invoked only once in the program
    static bool called_once = true;
    RELEASE_ASSERT(called_once && inited);

    delete g_platform;
    g_platform = nullptr;

    called_once = false;
    inited = false;
}

Platform* Global::platform()
{
    ASSERT(inited && !!g_platform);
    return g_platform;
}

} // namespace Escargot
