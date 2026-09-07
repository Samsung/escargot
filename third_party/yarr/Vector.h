/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <vector>
#include <iterator>
#include <utility>
#include <algorithm>
#include <new>

namespace WTF {

template <typename T, size_t N = 0>
class Vector;

template <typename T>
class Vector<T, 0> {
public:
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;

    Vector() {}
    Vector(const Vector& v)
    {
        append(v);
    }

    Vector(const T* v, size_t len)
    {
        impl.reserve(len);
        for (size_t i = 0; i < len; i ++) {
            impl.push_back(v[i]);
        }
    }

    Vector(std::initializer_list<T> list)
    {
        impl.reserve(list.size());
        for (auto& i : list) {
            impl.push_back(i);
        }
    }

    Vector(std::span<T> span)
    {
        impl.reserve(span.size());
        for (auto& i : span) {
            impl.push_back(i);
        }
    }

    size_t size() const
    {
        return impl.size();
    }

    T& operator[](size_t i)
    {
        return impl[i];
    }

    const T& operator[](size_t i) const
    {
        return impl[i];
    }

    T& at(size_t i)
    {
        return impl[i];
    }

    T* data()
    {
        return impl.data();
    }

    iterator begin()
    {
        return impl.begin();
    }

    iterator end()
    {
        return impl.end();
    }

    const_iterator begin() const
    {
        return impl.begin();
    }

    const_iterator end() const
    {
        return impl.end();
    }

    T& last()
    {
        return impl.back();
    }

    bool isEmpty() const
    {
        return impl.empty();
    }

    template <typename U>
    void append(const U& u)
    {
        impl.push_back(static_cast<T>(u));
    }

    void append(T&& u)
    {
        impl.push_back(std::move(u));
    }

    template <size_t M>
    void append(const Vector<T, M>& v)
    {
        impl.insert(impl.end(), v.impl.begin(), v.impl.end());
    }

    void insert(size_t i, const T& t)
    {
        impl.insert(impl.begin() + i, t);
    }

    void remove(size_t i)
    {
        impl.erase(impl.begin() + i);
    }

    void removeLast()
    {
        impl.pop_back();
    }

    void clear()
    {
        std::vector<T>().swap(impl);
    }

    void grow(size_t s)
    {
        impl.resize(s);
    }

    void shrink(size_t newLength)
    {
        ASSERT(newLength <= impl.size());
        while (impl.size() != newLength) {
            impl.pop_back();
        }
    }

    void shrinkToFit()
    {
        impl.shrink_to_fit();
    }

    size_t capacity() const
    {
        return impl.capacity();
    }

    void reserveInitialCapacity(size_t siz)
    {
        impl.reserve(siz);
    }

    void swap(Vector<T, 0>& other)
    {
        impl.swap(other.impl);
    }

    void deleteAllValues()
    {
        clear();
    }

    void reserveCapacity(size_t c)
    {
        reserve(c);
    }

    void reserve(size_t capacity)
    {
        impl.reserve(capacity);
    }

    T takeLast()
    {
        T last(*impl.rbegin());
        impl.pop_back();
        return last;
    }

    void fill(const T& val, size_t newSize)
    {
        if (size() > newSize)
            shrink(newSize);
        else if (newSize > capacity()) {
            clear();
            grow(newSize);
        }
        std::fill(begin(), end(), val);
    }

private:
    std::vector<T> impl;
};

// Small-buffer-optimized primary template, used for N>0.
// Up to N elements live inline in m_inlineStorage with zero heap allocation;
// once the element count would exceed N, storage spills (one-way) to a
// std::vector<T> and stays spilled until clear() (which has nothing left to
// preserve, so un-spilling there is free and safe).
template <typename T, size_t N>
class Vector {
public:
    typedef T* iterator;
    typedef const T* const_iterator;

    Vector() {}
    Vector(const Vector& v)
    {
        append(v);
    }

    // Copy/move-assignment are deliberately not implemented. With the old
    // single-std::vector storage, compiler-synthesized assignment happened to
    // be correct (it just forwarded to std::vector<T>::operator=). With raw
    // inline storage, a compiler-synthesized copy/move-assignment would do a
    // memberwise copy of m_inlineStorage as a plain byte array -- that
    // compiles silently (byte arrays are always memberwise-copyable) but is
    // memory-unsafe for any non-trivial T (e.g. two unique_ptrs would end up
    // pointing at the same object, causing a double free). Neither current
    // N>0 field (m_disjunctions in YarrPattern, m_parenthesesStack in
    // YarrParser) nor their containing classes are ever copy- or
    // move-assigned as a whole today, so deleting is a safe, defensive
    // default: any future code that actually needs it will get a loud
    // compile error at the call site (forcing a conscious implementation
    // decision then) instead of silent heap corruption now.
    Vector& operator=(const Vector&) = delete;
    Vector& operator=(Vector&&) = delete;

    Vector(const T* v, size_t len)
    {
        reserve(len);
        for (size_t i = 0; i < len; i ++) {
            append(v[i]);
        }
    }

    Vector(std::initializer_list<T> list)
    {
        reserve(list.size());
        for (auto& i : list) {
            append(i);
        }
    }

    Vector(std::span<T> span)
    {
        reserve(span.size());
        for (auto& i : span) {
            append(i);
        }
    }

    size_t size() const
    {
        return m_spilled ? m_heap.size() : m_size;
    }

    T& operator[](size_t i)
    {
        return m_spilled ? m_heap[i] : inlinePtr()[i];
    }

    const T& operator[](size_t i) const
    {
        return m_spilled ? m_heap[i] : inlinePtr()[i];
    }

    T& at(size_t i)
    {
        return (*this)[i];
    }

    T* data()
    {
        return m_spilled ? m_heap.data() : inlinePtr();
    }

    iterator begin()
    {
        return m_spilled ? m_heap.data() : inlinePtr();
    }

    iterator end()
    {
        return begin() + size();
    }

    const_iterator begin() const
    {
        return m_spilled ? m_heap.data() : inlinePtr();
    }

    const_iterator end() const
    {
        return begin() + size();
    }

    T& last()
    {
        return (*this)[size() - 1];
    }

    bool isEmpty() const
    {
        return size() == 0;
    }

    template <typename U>
    void append(const U& u)
    {
        if (!m_spilled && m_size == N)
            spillToHeap(N + 1);
        if (m_spilled)
            m_heap.push_back(static_cast<T>(u));
        else {
            new (inlinePtr() + m_size) T(static_cast<T>(u));
            ++m_size;
        }
    }

    void append(T&& u)
    {
        if (!m_spilled && m_size == N)
            spillToHeap(N + 1);
        if (m_spilled)
            m_heap.push_back(std::move(u));
        else {
            new (inlinePtr() + m_size) T(std::move(u));
            ++m_size;
        }
    }

    // Reaches only into v's public API (not v's private members) so this
    // works for any M/N combination, unlike the old N==0-only implementation
    // this was derived from, which relied on private access happening to be
    // per-class rather than per-instance.
    template <size_t M>
    void append(const Vector<T, M>& v)
    {
        for (typename Vector<T, M>::const_iterator it = v.begin(); it != v.end(); ++it)
            append(*it);
    }

    void insert(size_t i, const T& t)
    {
        append(t);
        for (size_t k = size() - 1; k > i; --k) {
            using std::swap;
            swap((*this)[k], (*this)[k - 1]);
        }
    }

    void remove(size_t i)
    {
        for (size_t k = i; k + 1 < size(); ++k) {
            using std::swap;
            swap((*this)[k], (*this)[k + 1]);
        }
        removeLast();
    }

    void removeLast()
    {
        if (m_spilled)
            m_heap.pop_back();
        else {
            inlinePtr()[m_size - 1].~T();
            --m_size;
        }
    }

    void clear()
    {
        if (m_spilled) {
            std::vector<T>().swap(m_heap);
            m_spilled = false;
        } else {
            for (size_t i = 0; i < m_size; ++i)
                inlinePtr()[i].~T();
        }
        m_size = 0;
    }

    void grow(size_t s)
    {
        if (s <= size()) {
            shrink(s);
            return;
        }
        while (size() < s)
            append(T());
    }

    void shrink(size_t newLength)
    {
        ASSERT(newLength <= size());
        while (size() != newLength) {
            removeLast();
        }
    }

    // Deliberately does not un-spill even if size() drops back to <= N after
    // shrinking the heap vector -- a deliberate simplification (see the
    // one-way-spill note at the class top), not a bug to "fix".
    void shrinkToFit()
    {
        if (m_spilled)
            m_heap.shrink_to_fit();
    }

    size_t capacity() const
    {
        return m_spilled ? m_heap.capacity() : N;
    }

    void reserveInitialCapacity(size_t siz)
    {
        reserve(siz);
    }

    void swap(Vector<T, N>& other)
    {
        size_t aSize = size(), bSize = other.size();
        size_t common = std::min(aSize, bSize);
        for (size_t i = 0; i < common; ++i) {
            using std::swap;
            swap((*this)[i], other[i]);
        }
        if (aSize > bSize) {
            for (size_t i = common; i < aSize; ++i)
                other.append(std::move((*this)[i]));
            shrink(common);
        } else if (bSize > aSize) {
            for (size_t i = common; i < bSize; ++i)
                append(std::move(other[i]));
            other.shrink(common);
        }
    }

    void deleteAllValues()
    {
        clear();
    }

    void reserveCapacity(size_t c)
    {
        reserve(c);
    }

    void reserve(size_t capacity)
    {
        if (capacity <= N && !m_spilled)
            return;
        if (!m_spilled)
            spillToHeap(capacity);
        else
            m_heap.reserve(capacity);
    }

    T takeLast()
    {
        if (m_spilled) {
            T last(std::move(m_heap.back()));
            m_heap.pop_back();
            return last;
        }
        T last(std::move(inlinePtr()[m_size - 1]));
        inlinePtr()[m_size - 1].~T();
        --m_size;
        return last;
    }

    void fill(const T& val, size_t newSize)
    {
        if (size() > newSize)
            shrink(newSize);
        else if (newSize > capacity()) {
            clear();
            grow(newSize);
        }
        std::fill(begin(), end(), val);
    }

    ~Vector()
    {
        clear();
    }

private:
    T* inlinePtr()
    {
        return reinterpret_cast<T*>(m_inlineStorage);
    }

    const T* inlinePtr() const
    {
        return reinterpret_cast<const T*>(m_inlineStorage);
    }

    void spillToHeap(size_t capacityHint)
    {
        // precondition: !m_spilled
        m_heap.reserve(capacityHint);
        for (size_t i = 0; i < m_size; ++i) {
            m_heap.push_back(std::move(inlinePtr()[i]));
            inlinePtr()[i].~T();
        }
        m_size = 0;
        m_spilled = true;
    }

    alignas(alignof(T)) unsigned char m_inlineStorage[sizeof(T) * N];
    size_t m_size { 0 }; // valid only while !m_spilled
    bool m_spilled { false };
    std::vector<T> m_heap; // valid only once m_spilled; empty otherwise
};

template <typename T>
class GCVector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    GCVector() {}
    GCVector(const GCVector& v)
    {
        append(v);
    }

    GCVector(const T* v, size_t len)
    {
        impl.reserve(len);
        for (size_t i = 0; i < len; i ++) {
            impl.push_back(v[i]);
        }
    }

    GCVector(std::initializer_list<T> list)
    {
        impl.reserve(list.size());
        for (auto& i : list) {
            impl.push_back(i);
        }
    }

    size_t size() const
    {
        return impl.size();
    }

    T& operator[](size_t i)
    {
        return impl[i];
    }

    const T& operator[](size_t i) const
    {
        return impl[i];
    }

    T& at(size_t i)
    {
        return impl[i];
    }

    T* data()
    {
        return impl.data();
    }

    iterator begin()
    {
        return impl.begin();
    }

    iterator end()
    {
        return impl.end();
    }

    const_iterator begin() const
    {
        return impl.begin();
    }

    const_iterator end() const
    {
        return impl.end();
    }

    T& last()
    {
        return impl.back();
    }

    bool isEmpty() const
    {
        return impl.empty();
    }

    template <typename U>
    void append(const U& u)
    {
        impl.push_back(static_cast<T>(u));
    }

    void append(T&& u)
    {
        impl.push_back(std::move(u));
    }

    template <size_t M>
    void append(const Vector<T, M>& v)
    {
        impl.insert(impl.end(), v.impl.begin(), v.impl.end());
    }

    void insert(size_t i, const T& t)
    {
        impl.insert(impl.begin() + i, t);
    }

    void remove(size_t i)
    {
        impl.erase(impl.begin() + i);
    }

    void removeLast()
    {
        impl.pop_back();
    }

    void clear()
    {
        impl.clear();
    }

    void grow(size_t s)
    {
        impl.resize(s);
    }

    void shrink(size_t newLength)
    {
        ASSERT(newLength <= impl.size());
        while (impl.size() != newLength) {
            impl.pop_back();
        }
    }

    void shrinkToFit()
    {
        impl.shrink_to_fit();
    }

    size_t capacity() const
    {
        return impl.capacity();
    }

    void reserveInitialCapacity(size_t siz)
    {
        impl.reserve(siz);
    }

    void deleteAllValues()
    {
        clear();
    }

    void reserve(size_t capacity)
    {
        impl.reserve(capacity);
    }

    T takeLast()
    {
        T last(*impl.rbegin());
        impl.pop_back();
        return last;
    }

    void fill(const T& val, size_t newSize)
    {
        if (size() > newSize)
            shrink(newSize);
        else if (newSize > capacity()) {
            clear();
            grow(newSize);
        }
        std::fill(begin(), end(), val);
    }

private:
    Escargot::Vector<T, GCUtil::gc_malloc_allocator<T>> impl;
};

} // namespace WTF

using WTF::Vector;
using WTF::GCVector;
