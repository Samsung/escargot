/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"

#include "Heap.h"
#include "LeakChecker.h"

#include <stdlib.h>

namespace Escargot {

static bool g_isInited = false;

void Heap::initialize(bool applyMallOpt, bool applyGcOpt)
{
    if (g_isInited)
        return;

    g_isInited = true;
    if (applyMallOpt) {
#ifdef M_MMAP_THRESHOLD
        mallopt(M_MMAP_THRESHOLD, 2048);
#endif
#ifdef M_MMAP_MAX
        mallopt(M_MMAP_MAX, 1024 * 1024);
#endif
    }

    if (applyGcOpt) {
        GC_set_free_space_divisor(24);
    }
    GC_set_force_unmap_on_gcollect(1);

    initializeCustomAllocators();

#ifdef PROFILE_BDWGC
    GCUtil::HeapUsageVisualizer::initialize();
#endif
}

void Heap::finalize()
{
    for (size_t i = 0; i < 5; i++) {
        GC_gcollect_and_unmap();
    }
}

void Heap::printGCHeapUsage()
{
#ifdef ESCARGOT_MEM_STATS
    GC_print_heap_usage();
#else
    printf("There are no memory usage information.\n");
    printf("Compile Escargot with ESCARGOT_MEM_STATS option.\n");
#endif
}
}
