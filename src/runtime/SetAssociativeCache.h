/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotSetAssociativeCache__
#define __EscargotSetAssociativeCache__

#include "runtime/String.h"

namespace Escargot {

// A 2-way set-associative cache keyed by short (<= maxKeyLength byte) Latin1
// content, mapping to an already-computed String*. Shared by two call sites
// with the exact same shape/hash but different content namespaces and
// admission policy: VMInstance's small-string allocation cache (front-end
// for String::fromLatin1) and its AtomicString content-keyed lookup cache
// (front-end for AtomicStringMap probes).
//
// Backing storage (m_objects/m_metadata) is allocated externally via init()
// with GC_MALLOC/GC_MALLOC_ATOMIC rather than embedded inline, because the
// owning VMInstance uses GC_MALLOC_EXPLICITLY_TYPED with a hand-built
// pointer bitmap descriptor -- callers must still add
// GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, <cache-member>.m_objects)) and
// the equivalent for m_metadata when wiring this into such a descriptor.
template <size_t SetsT>
class SLRUSetCache {
public:
    static constexpr size_t sets = SetsT;
    static constexpr size_t ways = 2;
    static constexpr size_t size = sets * ways;
    static constexpr size_t maxKeyLength = 16;

    struct Metadata {
        size_t len = 0;
        LChar buffer[maxKeyLength] = { 0 };
    };

    void init()
    {
        m_objects = (String**)GC_MALLOC(size * sizeof(String*));
        memset(m_objects, 0, size * sizeof(String*));

        m_metadata = (Metadata*)GC_MALLOC_ATOMIC(size * sizeof(Metadata));
        memset(m_metadata, 0, size * sizeof(Metadata));
    }

    ALWAYS_INLINE Optional<String*> lookup(const LChar* src, size_t len)
    {
        ASSERT(len <= maxKeyLength);

        size_t hash = computeHash(src, len);
        size_t index = hash & (sets - 1);
        size_t baseIdx = index * 2;

        // Check Way 0
        String* str0 = m_objects[baseIdx + 0];
        Metadata& meta0 = m_metadata[baseIdx + 0];
        if (str0 && meta0.len == len && memcmp(meta0.buffer, src, len) == 0) {
            return str0;
        }

        // Check Way 1
        String* str1 = m_objects[baseIdx + 1];
        Metadata& meta1 = m_metadata[baseIdx + 1];
        if (str1 && meta1.len == len && memcmp(meta1.buffer, src, len) == 0) {
            // Swap Way 0 and Way 1 (LRU promotion) using value copies to prevent reference aliasing bugs
            String* tempStr = m_objects[baseIdx + 0];
            Metadata tempMeta = m_metadata[baseIdx + 0];

            m_objects[baseIdx + 0] = str1;
            m_metadata[baseIdx + 0] = meta1;

            m_objects[baseIdx + 1] = tempStr;
            m_metadata[baseIdx + 1] = tempMeta;
            return str1;
        }

        return nullptr;
    }

    // Unconditional MRU insertion: every miss is cached immediately, evicting
    // whatever currently sits in Way 1 (LRU).
    ALWAYS_INLINE void insertMRU(const LChar* src, size_t len, String* resultString)
    {
        ASSERT(len <= maxKeyLength);

        size_t hash = computeHash(src, len);
        size_t index = hash & (sets - 1);
        size_t baseIdx = index * 2;

        // Evict Way 1 (LRU), move Way 0 to Way 1, and insert new item into Way 0
        m_objects[baseIdx + 1] = m_objects[baseIdx + 0];
        m_metadata[baseIdx + 1] = m_metadata[baseIdx + 0];

        m_objects[baseIdx + 0] = resultString;
        Metadata& meta0 = m_metadata[baseIdx + 0];
        meta0.len = len;
        memcpy(meta0.buffer, src, len);
    }

    // SLRU admission: new content lands only in the probation slot (Way 1);
    // Way 0 (protected/MRU) is left untouched here and can only be reached
    // via the hit-promotion in lookup(). This keeps one-shot content from
    // evicting an entry another, actually-repeating, key relies on -- it
    // only earns the protected slot by being looked up a second time.
    ALWAYS_INLINE void insertProbation(const LChar* src, size_t len, String* resultString)
    {
        ASSERT(len <= maxKeyLength);

        size_t hash = computeHash(src, len);
        size_t index = hash & (sets - 1);
        size_t baseIdx = index * 2;

        m_objects[baseIdx + 1] = resultString;
        Metadata& meta1 = m_metadata[baseIdx + 1];
        meta1.len = len;
        memcpy(meta1.buffer, src, len);
    }

    // Public so GC_WORD_OFFSET(VMInstance, <member>.m_objects/.m_metadata)
    // (a plain offsetof) can reach them from an owning VMInstance descriptor.
    String** m_objects{ nullptr };
    Metadata* m_metadata{ nullptr };

private:
    static ALWAYS_INLINE size_t computeHash(const LChar* src, size_t len)
    {
        unsigned int hash = 2166136261U;
        for (size_t i = 0; i < len; ++i) {
            hash = (hash ^ src[i]) * 16777619U;
        }
        return hash;
    }
};

} // namespace Escargot

#endif
