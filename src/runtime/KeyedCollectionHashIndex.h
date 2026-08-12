/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotKeyedCollectionHashIndex__
#define __EscargotKeyedCollectionHashIndex__

#include "runtime/Value.h"
#include "runtime/String.h"

namespace Escargot {

// Open-addressing hash index used by Map/Set/WeakMap/WeakSet on top of their
// insertion-ordered storage vector. A bucket holds (storageIndex + 1); 0 means
// empty. Deletion in the storage (tombstoning an entry) leaves the bucket
// untouched: a stale bucket fails the key comparison, acts as a collision
// entry for probing, and is dropped at the next rebuild. The buffer contains
// no GC pointers, so it is allocated with GC_MALLOC_ATOMIC and kept alive by
// the owning object's traced member pointer.
struct KeyedCollectionHashIndex {
    // storage size at which containers switch from linear scan to this index;
    // small collections stay on the scan to avoid per-op hashing overhead
    static const size_t buildThreshold = 16;

    uint32_t capacity; // power of two
    uint32_t occupied; // buckets holding an index, including stale ones
    uint32_t buckets[1];

    static KeyedCollectionHashIndex* create(size_t liveCount)
    {
        uint32_t capacity = 32;
        while (capacity < liveCount * 2) {
            capacity <<= 1;
        }
        size_t bytes = sizeof(KeyedCollectionHashIndex) + (capacity - 1) * sizeof(uint32_t);
        auto p = reinterpret_cast<KeyedCollectionHashIndex*>(GC_MALLOC_ATOMIC(bytes));
        memset(p, 0, bytes);
        p->capacity = capacity;
        return p;
    }

    bool needsRebuild() const
    {
        return occupied * 4 >= capacity * 3;
    }

    // inserts an index for a key known to be absent; caller handles rebuild
    void insert(size_t hash, size_t storageIndex)
    {
        ASSERT(!needsRebuild());
        size_t mask = capacity - 1;
        size_t i = hash & mask;
        while (buckets[i]) {
            i = (i + 1) & mask;
        }
        buckets[i] = (uint32_t)(storageIndex + 1);
        occupied++;
    }
};

// finalizer mix shared by every hash below (avalanches a 64-bit key so
// nearby inputs land in unrelated buckets)
inline uint64_t keyedCollectionHashMix(uint64_t u)
{
    u ^= u >> 33;
    u *= 0xff51afd7ed558ccdULL;
    u ^= u >> 33;
    return u;
}

inline size_t keyedCollectionPointerHash(void* p)
{
    return (size_t)keyedCollectionHashMix((uint64_t)(uintptr_t)p);
}

// hash consistent with Value::equalsToByTheSameValueZeroAlgorithm:
// numbers by numeric value (+0/-0 collapse, NaN fixed), strings by content,
// BigInts collapsed to one bucket chain (rare as keys), others by identity
inline size_t keyedCollectionHash(const Value& v)
{
    // fast path: an int32 is always finite and non-NaN, so this skips the
    // isNumber() tag dispatch and the isnan() check below while still
    // mixing the exact same double bit pattern the general path would
    // produce for the same numeric value -- keeps int32 5 and double 5.0
    // (SameValueZero-equal) landing in the same bucket.
    if (LIKELY(v.isInt32())) {
        int32_t i32 = v.asInt32();
        if (i32 == 0) {
            return 0;
        }
        double d = (double)i32;
        uint64_t b;
        memcpy(&b, &d, sizeof(b));
        return (size_t)keyedCollectionHashMix(b);
    }
    if (v.isNumber()) {
        double d = v.asNumber();
        if (d == 0) {
            return 0;
        }
        if (UNLIKELY(std::isnan(d))) {
            return 0x7ff8000000000000ULL & 0xffffffff;
        }
        uint64_t b;
        memcpy(&b, &d, sizeof(b));
        return (size_t)keyedCollectionHashMix(b);
    }
    if (v.isPointerValue()) {
        PointerValue* p = v.asPointerValue();
        if (p->isString()) {
            String* s = p->asString();
            return s->hashValue() ^ (s->length() * 0x9e3779b9U);
        }
        if (UNLIKELY(p->isBigInt())) {
            return 0x62696e74;
        }
        return keyedCollectionPointerHash(p);
    }
    if (v.isUndefined()) {
        return 0x75646566;
    }
    if (v.isNull()) {
        return 0x6e756c6c;
    }
    ASSERT(v.isBoolean());
    return v.asBoolean() ? 0x74727565 : 0x66616c73;
}
} // namespace Escargot

#endif
