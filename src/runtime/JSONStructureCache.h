/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifndef __EscargotJSONStructureCache__
#define __EscargotJSONStructureCache__

#include "runtime/ObjectStructure.h"

#ifndef ESCARGOT_JSON_STRUCTURE_CACHE_SIZE
#define ESCARGOT_JSON_STRUCTURE_CACHE_SIZE 16
#endif

namespace Escargot {

// Cache complete layouts instead of rebuilding a property-addition path for
// each JSON object. Entries contain keys and slots, never parsed values or
// prototypes, so each parse still creates objects in its current realm.
class JSONStructureCache : public gc {
public:
    static constexpr size_t TransitionCapacity = ESCARGOT_JSON_STRUCTURE_CACHE_SIZE;
    static constexpr size_t MaxEntryBytes = 64 * 1024;
    static constexpr size_t MaxBytes = 256 * 1024;
    static_assert(TransitionCapacity > 0, "JSON structure cache needs a transition slot");

    struct Entry : public gc {
        using KeyVector = Vector<String*, GCUtil::gc_malloc_allocator<String*>>;
        using IndexVector = Vector<size_t, GCUtil::gc_malloc_atomic_allocator<size_t>>;

        Entry(ObjectStructure* structure, KeyVector&& keys, IndexVector&& valueIndexes,
              size_t signature, size_t byteCharge)
            : m_structure(structure)
            , m_keys(std::move(keys))
            , m_valueIndexes(std::move(valueIndexes))
            , m_signature(signature)
            , m_byteCharge(byteCharge)
        {
            ASSERT(structure);
            ASSERT(m_keys.size() == m_valueIndexes.size());
#ifndef NDEBUG
            for (size_t i = 0; i < m_keys.size(); i++) {
                ASSERT(m_keys[i]);
                ASSERT(m_valueIndexes[i] < structure->propertyCount());
            }
#endif
        }

        void* operator new(size_t size)
        {
            static MAY_THREAD_LOCAL bool typeInited = false;
            static MAY_THREAD_LOCAL GC_descr descr;
            if (!typeInited) {
                GC_word bitmap[GC_BITMAP_SIZE(Entry)] = { 0 };
                GC_set_bit(bitmap, GC_WORD_OFFSET(Entry, m_structure));
                // Vector's first word is its GC-managed backing buffer.
                GC_set_bit(bitmap, GC_WORD_OFFSET(Entry, m_keys));
                GC_set_bit(bitmap, GC_WORD_OFFSET(Entry, m_valueIndexes));
                descr = GC_make_descriptor(bitmap, GC_WORD_LEN(Entry));
                typeInited = true;
            }
            return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
        }

        ObjectStructure* const m_structure;
        const KeyVector m_keys;
        const IndexVector m_valueIndexes;
        const size_t m_signature;
        const size_t m_byteCharge;
    };

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            GC_word bitmap[GC_BITMAP_SIZE(JSONStructureCache)] = { 0 };
            for (size_t i = 0; i < TransitionCapacity; i++) {
                GC_set_bit(bitmap, (offsetof(JSONStructureCache, m_transitionEntries) + i * sizeof(Slot) + offsetof(Slot, m_entry)) / sizeof(GC_word));
            }
            GC_set_bit(bitmap, GC_WORD_OFFSET(JSONStructureCache, m_nonTransitionEntry.m_entry));
            descr = GC_make_descriptor(bitmap, GC_WORD_LEN(JSONStructureCache));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }

    // The matcher verifies decoded key contents; signatures are only filters.
    // Callers keep the returned Entry alive while recursive value conversion
    // may replace or evict its cache slot.
    template <typename Matcher>
    Optional<Entry*> find(size_t signature, size_t memberCount, Matcher&& matcher)
    {
        auto matches = [&](Slot& slot) {
            return slot.m_entry && slot.m_entry->m_signature == signature
                && slot.m_entry->m_keys.size() == memberCount
                && matcher(*slot.m_entry.value());
        };
        if (m_lastSlot <= TransitionCapacity && matches(slotAt(m_lastSlot))) {
            Slot& slot = slotAt(m_lastSlot);
            slot.m_lastUsed = nextClock();
            return slot.m_entry;
        }
        for (size_t i = 0; i <= TransitionCapacity; i++) {
            if (i != m_lastSlot && matches(slotAt(i))) {
                Slot& slot = slotAt(i);
                slot.m_lastUsed = nextClock();
                m_lastSlot = i;
                return slot.m_entry;
            }
        }
        return nullptr;
    }

    bool shouldInsert(ObjectStructure* structure, size_t byteCharge)
    {
        ASSERT(structure);
        if (!structure->inTransitionMode()) {
            return true;
        }
        ASSERT(byteCharge <= MaxEntryBytes);
        if (m_transitionEntryCount < TransitionCapacity && m_bytes <= MaxBytes - byteCharge) {
            return true;
        }
        // Admit one in four misses when full. Unconditional LRU insertion
        // makes a cyclic working set larger than the cache miss every time,
        // rebuilding both layouts and entries even for recurring schemas.
        // Hits still refresh LRU order. A small deterministic PRNG avoids a
        // fixed admission phase in cyclic input, without growing the cache.
        m_admissionState ^= m_admissionState << 13;
        m_admissionState ^= m_admissionState >> 17;
        m_admissionState ^= m_admissionState << 5;
        return (m_admissionState & 3) == 0;
    }

    void insert(Entry* entry)
    {
        ASSERT(entry);
        if (entry->m_byteCharge > MaxEntryBytes || entry->m_byteCharge > MaxBytes) {
            return;
        }

        // Nested objects may have installed the same schema after the caller's
        // initial lookup. Replace it instead of using two LRU slots for it.
        for (size_t i = 0; i <= TransitionCapacity; i++) {
            Slot& slot = slotAt(i);
            if (slot.m_entry && sameKeys(*slot.m_entry.value(), *entry)) {
                remove(i);
            }
        }

        size_t target = TransitionCapacity;
        if (entry->m_structure->inTransitionMode()) {
            target = 0;
            for (size_t i = 0; i < TransitionCapacity; i++) {
                if (!m_transitionEntries[i].m_entry) {
                    target = i;
                    break;
                }
                if (m_transitionEntries[i].m_lastUsed < m_transitionEntries[target].m_lastUsed) {
                    target = i;
                }
            }
        } else {
            // Non-transition layouts otherwise transfer their exclusively
            // owned property buffers when a returned object is modified.
            entry->m_structure->markReferencedByInlineCache();
        }
        remove(target);
        while (m_bytes > MaxBytes - entry->m_byteCharge) {
            size_t oldest = TransitionCapacity + 1;
            for (size_t i = 0; i <= TransitionCapacity; i++) {
                if (slotAt(i).m_entry
                    && (oldest > TransitionCapacity || slotAt(i).m_lastUsed < slotAt(oldest).m_lastUsed)) {
                    oldest = i;
                }
            }
            ASSERT(oldest <= TransitionCapacity);
            remove(oldest);
        }
        Slot& slot = slotAt(target);
        slot.m_entry = entry;
        slot.m_lastUsed = nextClock();
        m_transitionEntryCount += target < TransitionCapacity;
        m_bytes += entry->m_byteCharge;
        m_lastSlot = target;
    }

private:
    struct Slot {
        Optional<Entry*> m_entry;
        size_t m_lastUsed{ 0 };
    };
    static_assert(sizeof(Optional<Entry*>) == sizeof(Entry*), "cache GC slots contain native pointers");
    static_assert(offsetof(Slot, m_entry) == 0, "cache GC slots start with their pointer");

    Slot& slotAt(size_t index)
    {
        ASSERT(index <= TransitionCapacity);
        return index == TransitionCapacity ? m_nonTransitionEntry : m_transitionEntries[index];
    }

    static bool sameKeys(const Entry& a, const Entry& b)
    {
        if (a.m_signature != b.m_signature || a.m_keys.size() != b.m_keys.size()) {
            return false;
        }
        for (size_t i = 0; i < a.m_keys.size(); i++) {
            if (!a.m_keys[i]->equals(b.m_keys[i])) {
                return false;
            }
        }
        return true;
    }

    void remove(size_t index)
    {
        Slot& slot = slotAt(index);
        if (slot.m_entry) {
            ASSERT(m_bytes >= slot.m_entry->m_byteCharge);
            m_bytes -= slot.m_entry->m_byteCharge;
            m_transitionEntryCount -= index < TransitionCapacity;
            slot.m_entry = nullptr;
            slot.m_lastUsed = 0;
        }
        if (m_lastSlot == index) {
            m_lastSlot = TransitionCapacity + 1;
        }
    }

    size_t nextClock()
    {
        if (UNLIKELY(m_clock == std::numeric_limits<size_t>::max())) {
            // Preserve the ordering on 32-bit builds after the counter wraps.
            size_t ages[TransitionCapacity + 1];
            for (size_t i = 0; i <= TransitionCapacity; i++) {
                ages[i] = slotAt(i).m_lastUsed;
            }
            for (size_t i = 0; i <= TransitionCapacity; i++) {
                if (!slotAt(i).m_entry) {
                    continue;
                }
                size_t rank = 1;
                for (size_t j = 0; j <= TransitionCapacity; j++) {
                    rank += slotAt(j).m_entry && ages[j] < ages[i];
                }
                slotAt(i).m_lastUsed = rank;
            }
            m_clock = TransitionCapacity + 1;
        }
        return ++m_clock;
    }

    Slot m_transitionEntries[TransitionCapacity];
    Slot m_nonTransitionEntry;
    size_t m_lastSlot{ TransitionCapacity + 1 };
    size_t m_clock{ 0 };
    uint32_t m_admissionState{ 0x9e3779b9 };
    size_t m_transitionEntryCount{ 0 };
    size_t m_bytes{ 0 };
};

} // namespace Escargot

#endif
