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

#include "Escargot.h"

#include "Heap.h"
#include "LeakChecker.h"

#include <stdlib.h>

namespace Escargot {

static bool g_isInited = false;

void Heap::initialize()
{
    if (g_isInited)
        return;

    RELEASE_ASSERT(GC_get_all_interior_pointers() == 0);

    GC_set_force_unmap_on_gcollect(1);
    g_isInited = true;
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
    ESCARGOT_LOG_INFO("There are no memory usage information.\n");
    ESCARGOT_LOG_INFO("Compile Escargot with ESCARGOT_MEM_STATS option.\n");
#endif
}
}
