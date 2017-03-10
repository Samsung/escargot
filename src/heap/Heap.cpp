/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"

#include "Heap.h"
#include "LeakChecker.h"

#include <malloc.h>

namespace Escargot {

void Heap::initialize()
{
    mallopt(M_MMAP_THRESHOLD, 2048);
    mallopt(M_MMAP_MAX, 1024 * 1024);

    GC_set_free_space_divisor(24);
    GC_set_force_unmap_on_gcollect(1);
    // GC_set_full_freq(1);
    // GC_set_time_limit(GC_TIME_UNLIMITED);

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
}
