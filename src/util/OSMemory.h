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

#ifndef __EscargotOSMemory__
#define __EscargotOSMemory__

#include <cstddef>

namespace Escargot {

class OSMemory {
public:
    // Reservations keep their address until release(). All sizes passed to
    // commit() and decommit() must be page aligned.
    static size_t pageSize();
    static void* reserve(size_t bytes, bool writable = true, bool executable = false, bool guardPages = false, int tag = -1);
    static void* reserveUncommitted(size_t bytes, bool writable = true, bool executable = false, bool guardPages = false, int tag = -1);
    static void commit(void* address, size_t bytes, bool writable = true, bool executable = false);
    static void decommit(void* address, size_t bytes);
    static void release(void* address, size_t bytes);
};

} // namespace Escargot

#endif
