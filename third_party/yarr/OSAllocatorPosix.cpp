/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "Escargot.h"
#include "OSAllocator.h"
#include "util/OSMemory.h"

#if defined(OS_POSIX)
namespace WTF {

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    return Escargot::OSMemory::reserveUncommitted(bytes, writable, executable, includesGuardPages, static_cast<int>(usage));
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    return Escargot::OSMemory::reserve(bytes, writable, executable, includesGuardPages, static_cast<int>(usage));
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
    Escargot::OSMemory::commit(address, bytes, writable, executable);
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    Escargot::OSMemory::decommit(address, bytes);
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    Escargot::OSMemory::release(address, bytes);
}

} // namespace WTF
#endif
