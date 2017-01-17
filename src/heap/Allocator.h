/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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

#ifndef __EscargotAllocator__
#define __EscargotAllocator__

namespace Escargot {

class ExecutionState;

enum HeapObjectKind {
    ValueVectorKind = 0,
    ArrayObjectKind,
    CodeBlockKind,
    NumberOfKind,
};

void initializeCustomAllocators();

typedef std::function<void(ExecutionState& state, void* obj)> HeapObjectIteratorCallback;

/*
 * This Function will iterate over every objects of given kind.
 * It has to call GC_gcollect() since it needs precise mark status.
 * So don't call this in performance-critical path.
 */
void iterateSpecificKindOfObject(ExecutionState& state, HeapObjectKind kind, HeapObjectIteratorCallback callback);

template <class GC_Tp>
class CustomAllocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef GC_Tp* pointer;
    typedef const GC_Tp* const_pointer;
    typedef GC_Tp& reference;
    typedef const GC_Tp& const_reference;
    typedef GC_Tp value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef CustomAllocator<GC_Tp1> other;
    };

    CustomAllocator() throw() {}
    CustomAllocator(const CustomAllocator&) throw() {}
#if !(GC_NO_MEMBER_TEMPLATES || 0 < _MSC_VER && _MSC_VER <= 1200)
    // MSVC++ 6.0 do not support member templates
    template <class GC_Tp1>
    CustomAllocator(const CustomAllocator<GC_Tp1>&) throw() {}
#endif
    ~CustomAllocator() throw()
    {
    }

    pointer address(reference GC_x) const { return &GC_x; }
    const_pointer address(const_reference GC_x) const { return &GC_x; }
    // GC_n is permitted to be 0. The C++ standard says nothing about what
    // the return value is when GC_n == 0.
    GC_Tp* allocate(size_type GC_n, const void* = 0);

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p, size_type GC_ATTR_UNUSED GC_n)
    {
        GC_FREE(__p);
    }

    size_type max_size() const throw()
    {
        return size_t(-1) / sizeof(GC_Tp);
    }

    void construct(pointer __p, const GC_Tp& __val) { new (__p) GC_Tp(__val); }
    void destroy(pointer __p) { __p->~GC_Tp(); }
};

template <>
class CustomAllocator<void> {
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef CustomAllocator<GC_Tp1> other;
    };
};


template <class GC_T1, class GC_T2>
inline bool operator==(const CustomAllocator<GC_T1>&, const CustomAllocator<GC_T2>&)
{
    return true;
}

template <class GC_T1, class GC_T2>
inline bool operator!=(const CustomAllocator<GC_T1>&, const CustomAllocator<GC_T2>&)
{
    return false;
}

template <class GC_Tp>
class gc_malloc_allocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef GC_Tp* pointer;
    typedef const GC_Tp* const_pointer;
    typedef GC_Tp& reference;
    typedef const GC_Tp& const_reference;
    typedef GC_Tp value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_allocator<GC_Tp1> other;
    };

    gc_malloc_allocator() throw() {}
    gc_malloc_allocator(const gc_malloc_allocator&) throw() {}
#if !(GC_NO_MEMBER_TEMPLATES || 0 < _MSC_VER && _MSC_VER <= 1200)
    // MSVC++ 6.0 do not support member templates
    template <class GC_Tp1>
    gc_malloc_allocator(const gc_malloc_allocator<GC_Tp1>&) throw() {}
#endif
    ~gc_malloc_allocator() throw()
    {
    }

    pointer address(reference GC_x) const { return &GC_x; }
    const_pointer address(const_reference GC_x) const { return &GC_x; }
    // GC_n is permitted to be 0. The C++ standard says nothing about what
    // the return value is when GC_n == 0.
    GC_Tp* allocate(size_type GC_n, const void* = 0)
    {
        return (GC_Tp*)GC_MALLOC(sizeof(GC_Tp) * GC_n);
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p, size_type GC_ATTR_UNUSED GC_n)
    {
        GC_FREE(__p);
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p)
    {
        GC_FREE(__p);
    }

    size_type max_size() const throw() { return size_t(-1) / sizeof(GC_Tp); }
    void construct(pointer __p, const GC_Tp& __val) { new (__p) GC_Tp(__val); }
    void destroy(pointer __p) { __p->~GC_Tp(); }
};

template <>
class gc_malloc_allocator<void> {
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_allocator<GC_Tp1> other;
    };
};


template <class GC_T1, class GC_T2>
inline bool operator==(const gc_malloc_allocator<GC_T1>&, const gc_malloc_allocator<GC_T2>&)
{
    return true;
}

template <class GC_T1, class GC_T2>
inline bool operator!=(const gc_malloc_allocator<GC_T1>&, const gc_malloc_allocator<GC_T2>&)
{
    return false;
}

template <class GC_Tp>
class gc_malloc_pointer_free_allocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef GC_Tp* pointer;
    typedef const GC_Tp* const_pointer;
    typedef GC_Tp& reference;
    typedef const GC_Tp& const_reference;
    typedef GC_Tp value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_pointer_free_allocator<GC_Tp1> other;
    };

    gc_malloc_pointer_free_allocator() throw() {}
    gc_malloc_pointer_free_allocator(const gc_malloc_pointer_free_allocator&) throw() {}
#if !(GC_NO_MEMBER_TEMPLATES || 0 < _MSC_VER && _MSC_VER <= 1200)
    // MSVC++ 6.0 do not support member templates
    template <class GC_Tp1>
    gc_malloc_pointer_free_allocator(const gc_malloc_pointer_free_allocator<GC_Tp1>&) throw() {}
#endif
    ~gc_malloc_pointer_free_allocator() throw()
    {
    }

    pointer address(reference GC_x) const { return &GC_x; }
    const_pointer address(const_reference GC_x) const { return &GC_x; }
    // GC_n is permitted to be 0. The C++ standard says nothing about what
    // the return value is when GC_n == 0.
    GC_Tp* allocate(size_type GC_n, const void* = 0)
    {
        return (GC_Tp*)GC_MALLOC_ATOMIC(sizeof(GC_Tp) * GC_n);
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p, size_type GC_ATTR_UNUSED GC_n) { GC_FREE(__p); }
    size_type max_size() const throw() { return size_t(-1) / sizeof(GC_Tp); }
    void construct(pointer __p, const GC_Tp& __val) { new (__p) GC_Tp(__val); }
    void destroy(pointer __p) { __p->~GC_Tp(); }
};

template <>
class gc_malloc_pointer_free_allocator<void> {
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_pointer_free_allocator<GC_Tp1> other;
    };
};


template <class GC_T1, class GC_T2>
inline bool operator==(const gc_malloc_pointer_free_allocator<GC_T1>&, const gc_malloc_pointer_free_allocator<GC_T2>&)
{
    return true;
}

template <class GC_T1, class GC_T2>
inline bool operator!=(const gc_malloc_pointer_free_allocator<GC_T1>&, const gc_malloc_pointer_free_allocator<GC_T2>&)
{
    return false;
}


template <class GC_Tp>
class gc_malloc_ignore_off_page_allocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef GC_Tp* pointer;
    typedef const GC_Tp* const_pointer;
    typedef GC_Tp& reference;
    typedef const GC_Tp& const_reference;
    typedef GC_Tp value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_ignore_off_page_allocator<GC_Tp1> other;
    };

    gc_malloc_ignore_off_page_allocator() throw() {}
    gc_malloc_ignore_off_page_allocator(const gc_malloc_ignore_off_page_allocator&) throw() {}
#if !(GC_NO_MEMBER_TEMPLATES || 0 < _MSC_VER && _MSC_VER <= 1200)
    // MSVC++ 6.0 do not support member templates
    template <class GC_Tp1>
    gc_malloc_ignore_off_page_allocator(const gc_malloc_ignore_off_page_allocator<GC_Tp1>&) throw() {}
#endif
    ~gc_malloc_ignore_off_page_allocator() throw()
    {
    }

    pointer address(reference GC_x) const { return &GC_x; }
    const_pointer address(const_reference GC_x) const { return &GC_x; }
    // GC_n is permitted to be 0. The C++ standard says nothing about what
    // the return value is when GC_n == 0.
    GC_Tp* allocate(size_type GC_n, const void* = 0)
    {
        if (sizeof(GC_Tp) * GC_n > 1024) {
            return (GC_Tp*)GC_MALLOC_IGNORE_OFF_PAGE(sizeof(GC_Tp) * GC_n);
        } else {
            return (GC_Tp*)GC_MALLOC(sizeof(GC_Tp) * GC_n);
        }
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p, size_type GC_ATTR_UNUSED GC_n)
    {
        GC_FREE(__p);
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p)
    {
        GC_FREE(__p);
    }

    size_type max_size() const throw() { return size_t(-1) / sizeof(GC_Tp); }
    void construct(pointer __p, const GC_Tp& __val) { new (__p) GC_Tp(__val); }
    void destroy(pointer __p) { __p->~GC_Tp(); }
};

template <>
class gc_malloc_ignore_off_page_allocator<void> {
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_ignore_off_page_allocator<GC_Tp1> other;
    };
};


template <class GC_T1, class GC_T2>
inline bool operator==(const gc_malloc_ignore_off_page_allocator<GC_T1>&, const gc_malloc_ignore_off_page_allocator<GC_T2>&)
{
    return true;
}

template <class GC_T1, class GC_T2>
inline bool operator!=(const gc_malloc_ignore_off_page_allocator<GC_T1>&, const gc_malloc_ignore_off_page_allocator<GC_T2>&)
{
    return false;
}

template <class GC_Tp>
class gc_malloc_atomic_ignore_off_page_allocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef GC_Tp* pointer;
    typedef const GC_Tp* const_pointer;
    typedef GC_Tp& reference;
    typedef const GC_Tp& const_reference;
    typedef GC_Tp value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_atomic_ignore_off_page_allocator<GC_Tp1> other;
    };

    gc_malloc_atomic_ignore_off_page_allocator() throw() {}
    gc_malloc_atomic_ignore_off_page_allocator(const gc_malloc_atomic_ignore_off_page_allocator&) throw() {}
#if !(GC_NO_MEMBER_TEMPLATES || 0 < _MSC_VER && _MSC_VER <= 1200)
    // MSVC++ 6.0 do not support member templates
    template <class GC_Tp1>
    gc_malloc_atomic_ignore_off_page_allocator(const gc_malloc_atomic_ignore_off_page_allocator<GC_Tp1>&) throw() {}
#endif
    ~gc_malloc_atomic_ignore_off_page_allocator() throw()
    {
    }

    pointer address(reference GC_x) const { return &GC_x; }
    const_pointer address(const_reference GC_x) const { return &GC_x; }
    // GC_n is permitted to be 0. The C++ standard says nothing about what
    // the return value is when GC_n == 0.
    GC_Tp* allocate(size_type GC_n, const void* = 0)
    {
        if (sizeof(GC_Tp) * GC_n > 1024) {
            return (GC_Tp*)GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE(sizeof(GC_Tp) * GC_n);
        } else {
            return (GC_Tp*)GC_MALLOC_ATOMIC(sizeof(GC_Tp) * GC_n);
        }
    }

    // __p is not permitted to be a null pointer.
    void deallocate(pointer __p, size_type GC_ATTR_UNUSED GC_n) { GC_FREE(__p); }
    size_type max_size() const throw() { return size_t(-1) / sizeof(GC_Tp); }
    void construct(pointer __p, const GC_Tp& __val) { new (__p) GC_Tp(__val); }
    void destroy(pointer __p) { __p->~GC_Tp(); }
};

template <>
class gc_malloc_atomic_ignore_off_page_allocator<void> {
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;

    template <class GC_Tp1>
    struct rebind {
        typedef gc_malloc_atomic_ignore_off_page_allocator<GC_Tp1> other;
    };
};


template <class GC_T1, class GC_T2>
inline bool operator==(const gc_malloc_atomic_ignore_off_page_allocator<GC_T1>&, const gc_malloc_atomic_ignore_off_page_allocator<GC_T2>&)
{
    return true;
}

template <class GC_T1, class GC_T2>
inline bool operator!=(const gc_malloc_atomic_ignore_off_page_allocator<GC_T1>&, const gc_malloc_atomic_ignore_off_page_allocator<GC_T2>&)
{
    return false;
}
}

#endif
