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

#include <unordered_map>
#include <iterator>

namespace WTF {

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key const, Value>>>
class HashMap : public std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator> {
public:
    struct AddResult {
        bool isNewEntry;
        typename std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::iterator iterator;
    };
    AddResult add(const Key& k, const Value& v)
    {
        AddResult r;
        auto result = std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::insert(std::make_pair(k, v));
        r.iterator = result.first;
        r.isNewEntry = result.second;
        return r;
    }

    const Value& get(const Key& k)
    {
        return std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::find(k)->second;
    }
};

} // namespace WTF

using WTF::HashMap;
