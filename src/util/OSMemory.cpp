/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#include "Escargot.h"
#include "OSMemory.h"
#include <cstdint>
#include <cstdlib>

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(OS_WINDOWS)
#include <windows.h>
#endif

namespace Escargot {

size_t OSMemory::pageSize()
{
#if defined(OS_POSIX)
    static const size_t size = static_cast<size_t>(getpagesize());
#elif defined(OS_WINDOWS)
    static const size_t size = []() {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return static_cast<size_t>(info.dwPageSize);
    }();
#else
    static const size_t size = 4096;
#endif
    return size;
}

#if defined(OS_POSIX)
static int protection(bool writable, bool executable)
{
    return PROT_READ | (writable ? PROT_WRITE : 0) | (executable ? PROT_EXEC : 0);
}

static int mappingFlags(bool executable)
{
    int flags = MAP_PRIVATE | MAP_ANON;
#if defined(OS_DARWIN) && defined(MAP_JIT) && TARGET_OS_IPHONE
    if (executable) {
        flags |= MAP_JIT;
    }
#endif
    return flags;
}
#elif defined(OS_WINDOWS)
static DWORD protection(bool writable, bool executable)
{
    return executable ? (writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
                      : (writable ? PAGE_READWRITE : PAGE_READONLY);
}
#endif

void* OSMemory::reserve(size_t bytes, bool writable, bool executable, bool guardPages, int tag)
{
    RELEASE_ASSERT(bytes);
#if defined(OS_POSIX)
#if defined(OS_DARWIN)
    int descriptor = tag;
#else
    int descriptor = -1;
    (void)tag;
#endif
    void* result = mmap(nullptr, bytes, protection(writable, executable), mappingFlags(executable), descriptor, 0);
    RELEASE_ASSERT(result != MAP_FAILED);
    if (guardPages) {
        RELEASE_ASSERT(bytes >= 2 * pageSize());
        RELEASE_ASSERT(bytes % pageSize() == 0);
        RELEASE_ASSERT(mmap(result, pageSize(), PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == result);
        void* lastPage = static_cast<char*>(result) + bytes - pageSize();
        RELEASE_ASSERT(mmap(lastPage, pageSize(), PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == lastPage);
    }
    return result;
#elif defined(OS_WINDOWS)
    (void)guardPages;
    (void)tag;
    void* result = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, protection(writable, executable));
    RELEASE_ASSERT(result);
    return result;
#else
    (void)writable;
    (void)executable;
    (void)guardPages;
    (void)tag;
    void* result = malloc(bytes);
    RELEASE_ASSERT(result);
    return result;
#endif
}

void* OSMemory::reserveUncommitted(size_t bytes, bool writable, bool executable, bool guardPages, int tag)
{
    RELEASE_ASSERT(bytes);
#if defined(OS_POSIX)
    (void)writable;
    (void)executable;
    (void)guardPages;
#if defined(OS_DARWIN)
    int descriptor = tag;
#else
    int descriptor = -1;
    (void)tag;
#endif
    int flags = MAP_PRIVATE | MAP_ANON;
#if defined(OS_QNX)
    flags |= MAP_LAZY;
#endif
    void* result = mmap(nullptr, bytes, PROT_NONE, flags, descriptor, 0);
    RELEASE_ASSERT(result != MAP_FAILED);
    return result;
#elif defined(OS_WINDOWS)
    (void)guardPages;
    (void)tag;
    void* result = VirtualAlloc(nullptr, bytes, MEM_RESERVE, protection(writable, executable));
    RELEASE_ASSERT(result);
    return result;
#else
    return reserve(bytes, writable, executable, guardPages, tag);
#endif
}

void OSMemory::commit(void* address, size_t bytes, bool writable, bool executable)
{
    RELEASE_ASSERT(address && bytes);
#if defined(OS_POSIX)
    RELEASE_ASSERT(reinterpret_cast<uintptr_t>(address) % pageSize() == 0);
    RELEASE_ASSERT(mmap(address, bytes, protection(writable, executable), mappingFlags(executable) | MAP_FIXED, -1, 0) == address);
#elif defined(OS_WINDOWS)
    RELEASE_ASSERT(VirtualAlloc(address, bytes, MEM_COMMIT, protection(writable, executable)) == address);
#else
    (void)writable;
    (void)executable;
#endif
}

void OSMemory::decommit(void* address, size_t bytes)
{
    RELEASE_ASSERT(address && bytes);
#if defined(OS_POSIX)
    RELEASE_ASSERT(reinterpret_cast<uintptr_t>(address) % pageSize() == 0);
    RELEASE_ASSERT(mmap(address, bytes, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == address);
#elif defined(OS_WINDOWS)
    RELEASE_ASSERT(VirtualFree(address, bytes, MEM_DECOMMIT));
#endif
}

bool OSMemory::discard(void* address, size_t bytes)
{
    RELEASE_ASSERT(address && bytes);
    RELEASE_ASSERT(reinterpret_cast<uintptr_t>(address) % pageSize() == 0);
    RELEASE_ASSERT(bytes % pageSize() == 0);
#if defined(OS_POSIX) && defined(MADV_DONTNEED)
    return madvise(address, bytes, MADV_DONTNEED) == 0;
#elif defined(OS_WINDOWS)
    return VirtualAlloc(address, bytes, MEM_RESET, PAGE_READWRITE) != nullptr;
#else
    return false;
#endif
}

void OSMemory::release(void* address, size_t bytes)
{
    RELEASE_ASSERT(address && bytes);
#if defined(OS_POSIX)
    RELEASE_ASSERT(munmap(address, bytes) == 0);
#elif defined(OS_WINDOWS)
    RELEASE_ASSERT(VirtualFree(address, 0, MEM_RELEASE));
#else
    free(address);
#endif
}

} // namespace Escargot
