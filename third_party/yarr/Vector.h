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

namespace WTF {

template <typename T, size_t N = 0>
class Vector {
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

    void swap(Vector<T, N>& other)
    {
        impl.swap(other.impl);
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
    std::vector<T> impl;
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
