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

namespace Escargot {

bool Global::inited;
std::thread::id Global::MAIN_THREAD_ID;
Platform* Global::g_platform;

void Global::initialize(Platform* platform)
{
    // initialize should be invoked only once in the main thread (must be thread-safe)
    static bool called_once = true;
    RELEASE_ASSERT(called_once && !inited);

    MAIN_THREAD_ID = std::this_thread::get_id();

    ASSERT(!g_platform);
    g_platform = platform;

    called_once = false;
    inited = true;
}

void Global::finalize()
{
    // finalize should be invoked only once in the main thread (must be thread-safe)
    static bool called_once = true;
    RELEASE_ASSERT(called_once && inited);
    ASSERT(MAIN_THREAD_ID == std::this_thread::get_id());

    delete g_platform;
    g_platform = nullptr;

    called_once = false;
    inited = false;
}

bool Global::inMainThread()
{
    ASSERT(inited);
    return MAIN_THREAD_ID == std::this_thread::get_id();
}

Platform* Global::platform()
{
    ASSERT(inited && !!g_platform);
    return g_platform;
}

} // namespace Escargot
