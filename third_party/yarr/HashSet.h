/*
 * Copyright (c) 2024-present Samsung Electronics Co., Ltd
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

#include <unordered_set>
#include <iterator>

namespace WTF {

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

    template<typename Other>
    void formUnion(const Other& other)
    {
        for (const auto& value: other) {
            add(value);
        }
    }

    bool contains(const Key& k)
    {
        return std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::find(k) != std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::end();
    }

    bool isEmpty()
    {
        return std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, Allocator>::empty();
    }
};

} // namespace WTF

using WTF::HashSet;
