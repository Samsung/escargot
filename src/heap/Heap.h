#ifndef __Heap__
#define __Heap__


namespace Escargot {

class Heap {
public:
    static void initialize();
    static void finalize();
};
}


//#define PROFILE_MASSIF
#ifdef PROFILE_MASSIF

#ifndef ESCARGOT_SHELL
#error `PROFILE_MASSIF` can only be enabled in escargot shell
#endif

#include <gc.h>

void* GC_malloc_hook(size_t siz);
void* GC_malloc_atomic_hook(size_t siz);
void* GC_generic_malloc_ignore_off_page_hook(size_t siz, int kind);
void GC_free_hook(void* address);

#undef GC_MALLOC
#define GC_MALLOC(X) GC_malloc_hook(X)

#undef GC_MALLOC_ATOMIC
#define GC_MALLOC_ATOMIC(X) GC_malloc_atomic_hook(X)

#undef GC_GENERIC_MALLOC_IGNORE_OFF_PAGE
#define GC_GENERIC_MALLOC_IGNORE_OFF_PAGE(siz, kind) GC_generic_malloc_ignore_off_page_hook(siz, kind)

#undef GC_FREE
#define GC_FREE(X) GC_free_hook(X)

#undef GC_REGISTER_FINALIZER_NO_ORDER
#define GC_REGISTER_FINALIZER_NO_ORDER(p, f, d, of, od) GC_register_finalizer_no_order(p, f, d, of, od)

#endif

#include <gc_allocator.h>
#include <gc_cpp.h>
#include <gc_mark.h>
#ifdef GC_DEBUG
#include <gc_backptr.h>
#endif

#include "Allocator.h"

#endif
