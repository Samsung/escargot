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

#ifndef __EscargotGlobalRecord__
#define __EscargotGlobalRecord__

#include <mutex>
#include <thread>

namespace Escargot {

class Platform;

// Global is a global interface used by all threads
class Global {
    static bool inited;
    static std::thread::id MAIN_THREAD_ID;
    static Platform* g_platform;

public:
    static void initialize(Platform* platform);
    static void finalize();

    static bool inMainThread();

    static Platform* platform();
};

} // namespace Escargot

#endif
