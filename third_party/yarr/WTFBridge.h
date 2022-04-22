/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef yarr_wtfbridge_h
#define yarr_wtfbridge_h

#include "Escargot.h"
#include "runtime/String.h"
#include <stdio.h>
#include <stdarg.h>

typedef unsigned char LChar;

enum TextCaseSensitivity {
    TextCaseSensitive,
    TextCaseInsensitive
};


namespace JSC {
namespace Yarr {
template <typename T, size_t N = 0>
class Vector {
public:
    typedef typename std::vector<T>::iterator iterator;
    std::vector<T> impl;

public:
    Vector() {}
    Vector(const Vector& v)
    {
        append(v);
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

    void clear()
    {
        std::vector<T>().swap(impl);
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

    void swap(Vector& other)
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
};


template <typename T, size_t N>
inline void
deleteAllValues(Vector<T, N>& v)
{
    v.deleteAllValues();
}

static inline void
dataLog(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/*
  * Do-nothing version of a macro used by WTF to avoid unused
  * parameter warnings.
  */
#define UNUSED_PARAM(e)
#define ASSERT_UNUSED(variable, assertion) ASSERT(assertion)

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key const, Value>>>
class HashMap : public std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator> {
public:
    void add(const Key& k, const Value& v)
    {
        std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::insert(std::make_pair(k, v));
    }

    const Value& get(const Key& k)
    {
        return std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::find(k)->second;
    }
};

template <typename Key, typename Allocator = std::allocator<Key>>
class HashSet : public std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator> {
public:
    struct AddResult {
        bool isNewEntry;
    };
    AddResult add(const Key& k)
    {
        AddResult r;
        r.isNewEntry = std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::insert(k).second;
        return r;
    }

    bool contains(const Key& k)
    {
        return std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::find(k) != std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::end();
    }
};

template <typename T>
class Optional {
public:
    Optional()
        : m_hasValue(false)
        , m_value()
    {
    }

    Optional(T value)
        : m_hasValue(true)
        , m_value(value)
    {
    }

    Optional(std::nullptr_t value)
        : m_hasValue(false)
        , m_value()
    {
    }

    T value()
    {
        ASSERT(m_hasValue);
        return m_value;
    }

    const T value() const
    {
        ASSERT(m_hasValue);
        return m_value;
    }

    bool hasValue() const
    {
        return m_hasValue;
    }

    operator bool() const
    {
        return hasValue();
    }

    bool operator==(const Optional<T>& other) const
    {
        if (m_hasValue != other.hasValue()) {
            return false;
        }
        return m_hasValue ? m_value == other.m_value : true;
    }

    bool operator!=(const Optional<T>& other) const
    {
        return !this->operator==(other);
    }

    bool operator==(const T& other) const
    {
        if (m_hasValue) {
            return value() == other;
        }
        return false;
    }

    bool operator!=(const T& other) const
    {
        return !operator==(other);
    }

protected:
    bool m_hasValue;
    T m_value;
};

class String {
public:
    String()
        : m_impl(::Escargot::String::emptyString)
    {
    }

    String(::Escargot::String* impl)
        : m_impl(impl)
    {
    }

    ALWAYS_INLINE char16_t operator[](const size_t idx) const
    {
        return m_impl->charAt(idx);
    }

    ALWAYS_INLINE size_t length() const
    {
        return m_impl->length();
    }

    bool equals(const String& src) const
    {
        return m_impl->equals(src.m_impl);
    }

    ALWAYS_INLINE size_t hashValue() const
    {
        return m_impl->hashValue();
    }

    bool contains(char c) const
    {
        char b[2] = { c, 0x0 };
        return m_impl->contains(b);
    }

    ALWAYS_INLINE bool is8Bit() const
    {
        return m_impl->is8Bit();
    }

    template <typename Any>
    const Any* characters() const
    {
        if (is8Bit()) {
            return (Any*)m_impl->characters8();
        } else {
            return (Any*)m_impl->characters16();
        }
    }

    ALWAYS_INLINE const LChar* characters8() const
    {
        return m_impl->characters8();
    }

    ALWAYS_INLINE const char16_t* characters16() const
    {
        return m_impl->characters16();
    }

    ALWAYS_INLINE::Escargot::String* impl()
    {
        return m_impl;
    }

    template <const size_t srcLen>
    ALWAYS_INLINE bool operator==(const char (&src)[srcLen]) const
    {
        return m_impl->equals(src);
    }

private:
    ::Escargot::String* m_impl;
};

class StringBuilder {
public:
    void append(int c)
    {
        char16_t buf[2];
        auto cnt = ::Escargot::utf32ToUtf16((char32_t)c, buf);
        m_impl.appendChar(buf[0]);
        if (cnt == 2) {
            m_impl.appendChar(buf[1]);
        }
    }

    String toString()
    {
        return m_impl.finalize();
    }

    void clear()
    {
        m_impl.clear();
    }

private:
    ::Escargot::StringBuilder m_impl;
};

} /* namespace Yarr */

} /* namespace JSC */

namespace std {
template <>
struct hash<::JSC::Yarr::String> {
    size_t operator()(::JSC::Yarr::String const& x) const
    {
        return x.hashValue();
    }
};

template <>
struct equal_to<::JSC::Yarr::String> {
    bool operator()(::JSC::Yarr::String const& a, ::JSC::Yarr::String const& b) const
    {
        return a.equals(b);
    }
};
}

namespace WTF {
const size_t notFound = static_cast<size_t>(-1);
using String = ::JSC::Yarr::String;
}

#define JS_EXPORT_PRIVATE
#define WTF_EXPORT_PRIVATE
#define WTF_MAKE_FAST_ALLOCATED
#define NO_RETURN_DUE_TO_ASSERT

#define PLATFORM(WTF_FEATURE) (defined WTF_PLATFORM_##WTF_FEATURE && WTF_PLATFORM_##WTF_FEATURE)
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE && WTF_CPU_##WTF_FEATURE)
#define HAVE(WTF_FEATURE) (defined HAVE_##WTF_FEATURE && HAVE_##WTF_FEATURE)
#define OS(NAME) (defined OS_##NAME && OS_##NAME)
#define USE(WTF_FEATURE) (defined WTF_USE_##WTF_FEATURE && WTF_USE_##WTF_FEATURE)
#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE && ENABLE_##WTF_FEATURE)

#if ESCARGOT_64
#define WTF_CPU_X86_64 1
#endif

#if defined(OS_WINDOWS)
#define WTF_OS_WINDOWS 1
#else
#define WTF_OS_LINUX 1
#define WTF_OS_UNIX 1
#define HAVE_ERRNO_H 1
#define HAVE_MMAP 1
#endif

#define WTFMove std::move

// NOTE there is no make_unique in c++11
#if (__cplusplus < 201402L)
namespace std {
template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}
}
#endif

#include "ASCIICType.h"

#undef RELEASE_ASSERT
#define RELEASE_ASSERT ASSERT

#endif /* yarr_wtfbridge_h */
