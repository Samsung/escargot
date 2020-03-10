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

#ifndef __EscargotUtil__
#define __EscargotUtil__

namespace Escargot {

#if defined(COMPILER_GCC)
template <const int siz>
inline void __attribute__((optimize("O0"))) clearStack()
{
    volatile char a[siz] = { 0 };
}
#elif defined(COMPILER_CLANG)
template <const int siz>
[[clang::optnone]] inline void clearStack()
{
    volatile char a[siz] = { 0 };
}
#elif defined(COMPILER_MSVC)
#pragma optimize("", off)
template <const int siz>
inline void clearStack()
{
    volatile char a[siz] = { 0 };
}
#pragma optimize("", on)
#else
#error
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
inline void* currentStackPointer()
{
    return __builtin_frame_address(0);
}
#elif defined(COMPILER_MSVC)
inline void* currentStackPointer()
{
    volatile int temp;
    return &temp;
}
#else
#error
#endif

class StorePositiveIntergerAsOdd {
public:
    StorePositiveIntergerAsOdd(const size_t& src = 0)
    {
        ASSERT(src < std::numeric_limits<size_t>::max() / 2);
        m_data = (src << 1) | 0x1;
    }

    operator size_t() const
    {
        return m_data >> 1;
    }

private:
    size_t m_data;
};

uint64_t fastTickCount(); // increase 1000 by 1 second(fast version. not super accurate)
uint64_t tickCount(); // increase 1000 by 1 second
uint64_t longTickCount(); // increase 1000000 by 1 second
uint64_t timestamp(); // increase 1000 by 1 second

class ProfilerTimer {
public:
    explicit ProfilerTimer(const char* msg)
        : m_start(longTickCount())
        , m_msg(msg)
    {
    }
    ~ProfilerTimer();

private:
    uint64_t m_start;
    const char* m_msg;
};

class LongTaskFinder {
public:
    LongTaskFinder(const char* msg, size_t loggingTimeInMS)
        : m_loggingTime(loggingTimeInMS)
        , m_start(longTickCount())
        , m_msg(msg)
    {
    }
    ~LongTaskFinder();

private:
    size_t m_loggingTime;
    uint64_t m_start;
    const char* m_msg;
};
} // namespace Escargot

namespace Escargot {

namespace detail {

template <typename T, typename Comparator>
ALWAYS_INLINE bool
merge(T* dst, const T* src, size_t srcIndex, size_t srcEnd, size_t width, Comparator c)
{
    size_t left = srcIndex;
    size_t leftEnd = std::min(left + width, srcEnd);
    size_t right = leftEnd;
    size_t rightEnd = std::min(right + width, srcEnd);
    bool lessOrEqual;

    for (size_t dstIndex = left; dstIndex < rightEnd; ++dstIndex) {
        if (right < rightEnd) {
            if (left >= leftEnd) {
                dst[dstIndex] = src[right++];
                continue;
            }

            bool result = c(src[right], src[left], &lessOrEqual);
            if (!result) {
                return false;
            }
            if (lessOrEqual) {
                dst[dstIndex] = src[right++];
                continue;
            }
        }

        dst[dstIndex] = src[left++];
    }
    return true;
}

} /* namespace detail */

/*
 * Sort the array using the merge sort algorithm. The scratch should point to
 * a temporary storage that can hold nelems elements.
 *
 * The comparator must provide the () operator with the following signature:
 *
 *     bool operator()(const T& a, const T& a, bool *lessOrEqualp);
 *
 * It should return true on success and set *lessOrEqualp to the result of
 * a <= b operation. If it returns false, the sort terminates immediately with
 * the false result. In this case the content of the array and scratch is
 * arbitrary.
 */
template <typename T, typename Comparator>
bool mergeSort(T* array, size_t nelems, T* scratch, Comparator c)
{
    T* dst = scratch;
    T* src = array;

    for (size_t width = 1; width < nelems; width *= 2) {
        for (size_t srcIndex = 0; srcIndex < nelems; srcIndex += 2 * width)
            if (!detail::merge(dst, src, srcIndex, nelems, width, c)) {
                return false;
            }
        T* tmp = src;
        src = dst;
        dst = tmp;
    }

    if (src != array) {
        for (size_t i = 0; i < nelems; i++)
            array[i] = src[i];
    }
    return true;
}
} // namespace Escargot
#endif
