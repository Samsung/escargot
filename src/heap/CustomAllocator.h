/*
 * Copyright (c) 1996-1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * Copyright (c) 2002
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */
/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotCustomAllocator__
#define __EscargotCustomAllocator__

namespace Escargot {

class ExecutionState;

enum HeapObjectKind {
    ValueVectorKind = 0,
    ArrayObjectKind,
    CodeBlockKind,
    InterpretedCodeBlockKind,
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
        // memset(__p, 0, GC_n * sizeof(GC_Tp));
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
}

#endif
