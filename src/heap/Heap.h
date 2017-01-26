#ifndef __EscargotHeap__
#define __EscargotHeap__


namespace Escargot {

class Heap {
public:
    static void initialize();
    static void finalize();
};
}

#include <gc.h>
#include <gc_mark.h>
#include <gc_typed.h>

//#define PROFILE_MASSIF
#ifdef PROFILE_MASSIF

#ifndef ESCARGOT_SHELL
#error `PROFILE_MASSIF` can only be enabled in escargot shell
#endif

void* GC_malloc_hook(size_t siz);
void* GC_malloc_atomic_hook(size_t siz);
void* GC_generic_malloc_hook(size_t siz, int kind);
void* GC_generic_malloc_ignore_off_page_hook(size_t siz, int kind);
void GC_free_hook(void* address);

#undef GC_MALLOC_EXPLICITLY_TYPED
#define GC_MALLOC_EXPLICITLY_TYPED(bytes, d) GC_MALLOC(bytes)

#undef GC_MALLOC
#define GC_MALLOC(X) GC_malloc_hook(X)

#undef GC_MALLOC_ATOMIC
#define GC_MALLOC_ATOMIC(X) GC_malloc_atomic_hook(X)

#undef GC_GENERIC_MALLOC
#define GC_GENERIC_MALLOC(siz, kind) GC_generic_malloc_hook(siz, kind)

#undef GC_GENERIC_MALLOC_IGNORE_OFF_PAGE
#define GC_GENERIC_MALLOC_IGNORE_OFF_PAGE(siz, kind) GC_generic_malloc_hook(siz, kind)

#undef GC_FREE
#define GC_FREE(X) GC_free_hook(X)

#undef GC_REGISTER_FINALIZER_NO_ORDER
#define GC_REGISTER_FINALIZER_NO_ORDER(p, f, d, of, od) GC_register_finalizer_no_order(p, f, d, of, od)

#else

/* FIXME
 * This is just a workaround to remove ignore_off_page allocator.
 * `ignore_off_page` should be removed from everywhere after stablization.
 */

#undef GC_MALLOC_IGNORE_OFF_PAGE
#define GC_MALLOC_IGNORE_OFF_PAGE GC_MALLOC

#undef GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE
#define GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE GC_MALLOC_ATOMIC

#undef GC_GENERIC_MALLOC_IGNORE_OFF_PAGE
#define GC_GENERIC_MALLOC_IGNORE_OFF_PAGE GC_GENERIC_MALLOC

#endif

#include <gc_allocator.h>
#include <gc_cpp.h>
#ifdef GC_DEBUG
#include <gc_backptr.h>
#endif

#include "Allocator.h"

#endif
