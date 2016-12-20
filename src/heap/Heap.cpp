#include "Escargot.h"

#include "Heap.h"
#include "HeapProfiler.h"

namespace Escargot {

void Heap::initialize()
{
    GC_set_free_space_divisor(24);
    GC_set_force_unmap_on_gcollect(1);
    //  GC_set_full_freq(1);
    //  GC_set_time_limit(GC_TIME_UNLIMITED);

    initializeCustomAllocators();

#ifdef PROFILE_BDWGC
    Escargot::HeapUsageVisualizer::initialize();
#endif
}

void Heap::finalize()
{
    for (size_t i = 0; i < 5; i++) {
        GC_gcollect_and_unmap();
    }
}
}
