#include "Escargot.h"

#include "Heap.h"
#include "LeakChecker.h"

#include <malloc.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace Escargot {

void Heap::initialize()
{
    mallopt(M_MMAP_THRESHOLD, 2048);
    mallopt(M_MMAP_MAX, 1024 * 1024);

    GC_set_free_space_divisor(24);
    GC_set_force_unmap_on_gcollect(1);
    // GC_set_full_freq(1);
    // GC_set_time_limit(GC_TIME_UNLIMITED);

    GC_register_mark_stack_func([]() {
        unw_cursor_t cursor;
        unw_context_t uc;
        unw_word_t bp;
        unw_word_t sp;

        unw_getcontext(&uc);
        unw_init_local(&cursor, &uc);

        unw_step(&cursor);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);

        while (unw_step(&cursor) > 0) {
            unw_get_reg(&cursor, UNW_REG_SP, &bp);
            // printf("sp %p bp %p\n", (void*)sp, (void*)bp);
            RELEASE_ASSERT(bp > sp);
            GC_push_all_eager((char*)sp, (char*)bp);
            sp = bp;
        }
    });

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
