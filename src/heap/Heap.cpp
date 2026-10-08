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
#include "runtime/Value.h"
#include <gc/gc_tiny_fl.h>
#include "runtime/Object.h"
#if defined(OS_BAREMETAL)
#include "runtime/Global.h"
#include "runtime/Platform.h"
#endif

namespace Escargot {

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
MAY_THREAD_LOCAL Optional<const GC_compressed_bitmap_descr*> Heap::s_compressedDescriptors[static_cast<size_t>(Heap::CompressedType::Count)];

void Heap::initializeCompressedType(CompressedType type, size_t size,
                                    const GC_word* bitmap, size_t slots)
{
    auto& descriptor = s_compressedDescriptors[static_cast<size_t>(type)];
    if (!descriptor) {
        size_t headerSlots = 0;
#if defined(GC_DEBUG)
        const size_t headerSize = GC_get_debug_header_size();
        headerSlots = headerSize / 4;
        std::vector<GC_word> debugBitmap((slots + headerSlots + GC_WORDSZ - 1) / GC_WORDSZ, 0);
        for (size_t slot = 0; slot < slots; ++slot) {
            if (GC_get_bit(bitmap, slot)) {
                GC_set_bit(debugBitmap.data(), slot + headerSlots);
            }
        }
        bitmap = debugBitmap.data();
        size += headerSize;
        slots += headerSlots;
#endif
        // Types before Object have their own layouts. Object and its subclasses
        // inherit the tagged property buffer.
        if (type >= CompressedType::Object) {
            descriptor = GC_make_compressed_bitmap_descriptor_with_tag(size, bitmap, slots,
                                                                       Object::compressedValuesGCSlot() + headerSlots, 1, false);
        } else {
            descriptor = GC_make_compressed_bitmap_descriptor(size, bitmap, slots);
        }
        ASSERT(descriptor);
    }
}

#endif

void Heap::initialize()
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    for (auto& descriptor : s_compressedDescriptors) {
        descriptor.reset();
    }
#endif
    COMPILE_ASSERT(GC_GRANULE_BYTES >= 8, "BDWGC allocations must be 8-byte aligned");
    COMPILE_ASSERT((GC_GRANULE_BYTES & PointerKindMask) == 0, "BDWGC granule must preserve pointer-kind bits");
    // disable data area searching in bdwgc
    GC_set_no_dls(1);
#if !defined(OS_BAREMETAL)
    // Set the default before GC_init so GC_FREE_SPACE_DIVISOR can override it.
    GC_set_free_space_divisor(12);
#endif
#if defined(OS_BAREMETAL)
    // Bare-metal/RTOS builds: tell BDWGC where the current task's stack
    // starts (cold/high end) *before* GC_init() runs, using the
    // officially-supported runtime override (see third_party/GCutil's
    // include/gc/gc.h: GC_set_stackbottom() "could be used for setting
    // GC_stackbottom value ... before the collector is initialized").
    // This is what lets gcconfig.h stay free of any per-RTOS-port
    // STACKBOTTOM logic -- see docs/porting/RTOS_PORTING_GUIDE.md.
    struct GC_stack_base sb;
    sb.mem_base = Global::platform()->stackTop();
    GC_set_stackbottom(nullptr, &sb);
#endif
    GC_init();
    GC_init_finalized_malloc();

    RELEASE_ASSERT(GC_get_all_interior_pointers() == 0);

#if defined(OS_ANDROID)
    GC_set_abort_func([](const char* msg) {
        ESCARGOT_LOG_ERROR("%s", msg);
    });
#endif

#if defined(NDEBUG)
    GC_set_warn_proc(GC_ignore_warn_proc);
#endif

    GC_set_oom_fn([](size_t sz) -> void* {
        ESCARGOT_LOG_ERROR("Out of memory allocating %zu bytes (GC heap: %zu bytes, free: %zu bytes, unmapped: %zu bytes).\n",
                           sz, GC_get_heap_size(), GC_get_free_bytes(), GC_get_unmapped_bytes());
        abort();
        return nullptr;
    });

    GC_set_force_unmap_on_gcollect(1);
#if defined(OS_BAREMETAL)
    GC_set_free_space_divisor(1);
#endif
    initializeCustomAllocators();

#ifdef PROFILE_BDWGC
    GCUtil::HeapUsageVisualizer::initialize();
#endif
}

void Heap::finalize()
{
#if defined(ESCARGOT_GOOGLE_PERF)
    // These collections exist to drain finalizers and to leave a clean heap
    // for leak checking; the process is about to exit either way. Under a
    // CPU profiler they are pure noise -- each one is a full mark of the
    // whole live heap that reclaims nothing, and they land in the profile as
    // marking cost that no part of the workload asked for.
#else
    for (size_t i = 0; i < 5; i++) {
        GC_gcollect_and_unmap();
    }
#endif
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
} // namespace Escargot
