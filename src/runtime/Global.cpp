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
#if defined(ENABLE_ATOMICS_GLOBAL_LOCK)
SpinLock Global::g_atomicsLock;
#endif
#if defined(ENABLE_THREADING)
std::mutex Global::g_waiterMutex;
std::vector<Global::Waiter*> Global::g_waiter;
#endif

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


#if defined(ENABLE_THREADING)
    for (size_t i = 0; i < g_waiter.size(); i++) {
        delete g_waiter[i];
    }
    std::vector<Waiter*>().swap(g_waiter);
#endif

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

#if defined(ENABLE_THREADING)
Global::Waiter* Global::waiter(void* blockAddress)
{
    std::lock_guard<std::mutex> guard(g_waiterMutex);
    for (size_t i = 0; i < g_waiter.size(); i++) {
        if (g_waiter[i]->m_blockAddress == blockAddress) {
            return g_waiter[i];
        }
    }

    Waiter* w = new Waiter();
    w->m_blockAddress = blockAddress;
    g_waiter.push_back(w);

    return w;
}
#endif

#ifdef ESCARGOT_USE_CUSTOM_LOGGING
void customEscargotInfoLogger(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    Global::platform()->customInfoLogger(format, arg);
    va_end(arg);
}

void customEscargotErrorLogger(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    Global::platform()->customErrorLogger(format, arg);
    va_end(arg);
}
#endif
} // namespace Escargot
