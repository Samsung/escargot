/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

/*
 * Copyright (c) 2010 Mike Lovell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Escargot.h"
#include "Vector.h"
#include "Util.h"

#if defined(OS_ANDROID)
#include <sys/time.h>
#elif defined(OS_WINDOWS)
#include <time.h>
#else
#include <sys/timeb.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Escargot {

uint64_t fastTickCount()
{
#if defined(CLOCK_MONOTONIC_COARSE)
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    return static_cast<uint64_t>(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
#else
    return tickCount();
#endif
}

uint64_t tickCount()
{
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(d);
    return static_cast<uint64_t>(s.count()) * 1000UL + std::chrono::duration_cast<std::chrono::microseconds>(d - s).count() / 1000UL;
}

uint64_t longTickCount()
{
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(d);
    return static_cast<uint64_t>(s.count()) * 1000000UL + std::chrono::duration_cast<std::chrono::microseconds>(d - s).count();
}

uint64_t timestamp()
{
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(d);
    return static_cast<uint64_t>(s.count()) * 1000UL + std::chrono::duration_cast<std::chrono::microseconds>(d - s).count() / 1000UL;
}

ProfilerTimer::~ProfilerTimer()
{
    uint64_t end = longTickCount();
    float time = static_cast<float>((end - m_start) / 1000.f);
    ESCARGOT_LOG_INFO("did %s in %f ms\n", m_msg, time);
}

LongTaskFinder::~LongTaskFinder()
{
    uint64_t end = longTickCount();
    float time = static_cast<float>((end - m_start) / 1000.f);
    if (time >= m_loggingTime) {
        ESCARGOT_LOG_INFO("found long task %s in %f ms\n", m_msg, time);
    }
}

} // namespace Escargot
