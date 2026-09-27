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

#include "Escargot.h"
#include "Object.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

namespace Escargot {

#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
namespace {

constexpr size_t ObjectStructureProfileKindCount = 3;
constexpr size_t ObjectStructureProfileExactSizeLimit = 128;
constexpr size_t ObjectStructureProfileSizeBucketCount = 136;
constexpr size_t ObjectStructureIndexedProfileOwnerCount = 3;
constexpr size_t ObjectStructureIndexedProfileEventCount = 7;
constexpr size_t ObjectStructureIndexedProfileIndexBucketCount = 5;

struct ObjectStructureProfileSizeCounters {
    uint64_t creations{ 0 };
    uint64_t finds{ 0 };
    uint64_t hits{ 0 };
    uint64_t misses{ 0 };
    uint64_t lastCacheHits{ 0 };
    uint64_t comparisons{ 0 };
    uint64_t adds{ 0 };
    uint64_t removes{ 0 };
    uint64_t replaces{ 0 };
};

class ObjectStructureProfile {
public:
    ObjectStructureProfile()
        : m_enabled(getenv("ESCARGOT_OBJECT_STRUCTURE_PROFILE") != nullptr)
    {
    }

    ~ObjectStructureProfile()
    {
        if (!m_enabled) {
            return;
        }

        const char* label = getenv("ESCARGOT_OBJECT_STRUCTURE_PROFILE");
        fprintf(stderr, "OSP_BEGIN label=%s\n", label && *label ? label : "unnamed");
        fprintf(stderr,
                "OSP_CONFIG access_cache_min=%d transition_max=%d transition_map_min=%d\n",
                ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE,
                ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE,
                ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE);

        for (size_t kind = 0; kind < ObjectStructureProfileKindCount; kind++) {
            uint64_t creations = 0;
            uint64_t finds = 0;
            uint64_t hits = 0;
            uint64_t misses = 0;
            uint64_t cacheHits = 0;
            uint64_t comparisons = 0;
            uint64_t adds = 0;
            uint64_t removes = 0;
            uint64_t replaces = 0;
            for (size_t bucket = 0; bucket < ObjectStructureProfileSizeBucketCount; bucket++) {
                const auto& counter = m_size[kind][bucket];
                creations += counter.creations;
                finds += counter.finds;
                hits += counter.hits;
                misses += counter.misses;
                cacheHits += counter.lastCacheHits;
                comparisons += counter.comparisons;
                adds += counter.adds;
                removes += counter.removes;
                replaces += counter.replaces;
            }
            fprintf(stderr,
                    "OSP_KIND kind=%s creations=%llu finds=%llu hits=%llu misses=%llu cache_hits=%llu comparisons=%llu adds=%llu removes=%llu replaces=%llu atomic_names=%llu non_atomic_names=%llu symbol_names=%llu\n",
                    kindName(kind),
                    static_cast<unsigned long long>(creations),
                    static_cast<unsigned long long>(finds),
                    static_cast<unsigned long long>(hits),
                    static_cast<unsigned long long>(misses),
                    static_cast<unsigned long long>(cacheHits),
                    static_cast<unsigned long long>(comparisons),
                    static_cast<unsigned long long>(adds),
                    static_cast<unsigned long long>(removes),
                    static_cast<unsigned long long>(replaces),
                    static_cast<unsigned long long>(m_nameKinds[kind][0]),
                    static_cast<unsigned long long>(m_nameKinds[kind][1]),
                    static_cast<unsigned long long>(m_nameKinds[kind][2]));

            for (size_t bucket = 0; bucket < ObjectStructureProfileSizeBucketCount; bucket++) {
                const auto& counter = m_size[kind][bucket];
                if (!(counter.creations || counter.finds || counter.adds || counter.removes || counter.replaces)) {
                    continue;
                }
                char sizeLabel[32];
                formatSizeBucket(bucket, sizeLabel, sizeof(sizeLabel));
                fprintf(stderr,
                        "OSP_SIZE kind=%s properties=%s creations=%llu finds=%llu hits=%llu misses=%llu cache_hits=%llu comparisons=%llu adds=%llu removes=%llu replaces=%llu\n",
                        kindName(kind), sizeLabel,
                        static_cast<unsigned long long>(counter.creations),
                        static_cast<unsigned long long>(counter.finds),
                        static_cast<unsigned long long>(counter.hits),
                        static_cast<unsigned long long>(counter.misses),
                        static_cast<unsigned long long>(counter.lastCacheHits),
                        static_cast<unsigned long long>(counter.comparisons),
                        static_cast<unsigned long long>(counter.adds),
                        static_cast<unsigned long long>(counter.removes),
                        static_cast<unsigned long long>(counter.replaces));
            }
        }

        fprintf(stderr,
                "OSP_TRANSITION vector_lookups=%llu vector_comparisons=%llu vector_reuses=%llu map_lookups=%llu map_reuses=%llu vector_to_map=%llu exits=%llu\n",
                static_cast<unsigned long long>(m_transitionVectorLookups),
                static_cast<unsigned long long>(m_transitionVectorComparisons),
                static_cast<unsigned long long>(m_transitionVectorReuses),
                static_cast<unsigned long long>(m_transitionMapLookups),
                static_cast<unsigned long long>(m_transitionMapReuses),
                static_cast<unsigned long long>(m_transitionVectorToMap),
                static_cast<unsigned long long>(m_transitionExits));
        fprintf(stderr,
                "OSP_MAP builds=%llu rebuilds=%llu lazy_builds=%llu inserts=%llu allocated_bytes=%llu max_allocation=%llu dense_builds=%llu non_atomic_builds=%llu hash_calls=%llu content_hash_calls=%llu index_conversions=%llu insert_probes=%llu max_insert_probe=%llu find_last_cache=%llu find_dense=%llu dense_hits=%llu dense_misses=%llu find_hash=%llu hash_hits=%llu hash_misses=%llu find_probes=%llu max_find_probe=%llu linear_fallbacks=%llu fallback_comparisons=%llu\n",
                static_cast<unsigned long long>(m_mapBuilds),
                static_cast<unsigned long long>(m_mapRebuilds),
                static_cast<unsigned long long>(m_mapLazyBuilds),
                static_cast<unsigned long long>(m_mapInserts),
                static_cast<unsigned long long>(m_mapAllocatedBytes),
                static_cast<unsigned long long>(m_mapMaxAllocation),
                static_cast<unsigned long long>(m_mapDenseBuilds),
                static_cast<unsigned long long>(m_mapNonAtomicBuilds),
                static_cast<unsigned long long>(m_mapHashCalls),
                static_cast<unsigned long long>(m_mapContentHashCalls),
                static_cast<unsigned long long>(m_mapIndexConversions),
                static_cast<unsigned long long>(m_mapInsertProbes),
                static_cast<unsigned long long>(m_mapMaxInsertProbe),
                static_cast<unsigned long long>(m_mapLastCacheHits),
                static_cast<unsigned long long>(m_mapDenseFinds),
                static_cast<unsigned long long>(m_mapDenseHits),
                static_cast<unsigned long long>(m_mapDenseMisses),
                static_cast<unsigned long long>(m_mapHashFinds),
                static_cast<unsigned long long>(m_mapHashHits),
                static_cast<unsigned long long>(m_mapHashMisses),
                static_cast<unsigned long long>(m_mapFindProbes),
                static_cast<unsigned long long>(m_mapMaxFindProbe),
                static_cast<unsigned long long>(m_mapLinearFallbacks),
                static_cast<unsigned long long>(m_mapFallbackComparisons));
        for (size_t owner = 0; owner < ObjectStructureIndexedProfileOwnerCount; owner++) {
            fprintf(stderr,
                    "OSP_INDEXED owner=%s get_hits=%llu get_misses=%llu add_default=%llu add_custom=%llu updates=%llu delete_hits=%llu delete_misses=%llu index_0_15=%llu index_16_255=%llu index_256_4095=%llu index_4096_65535=%llu index_65536_plus=%llu\n",
                    indexedOwnerName(owner),
                    static_cast<unsigned long long>(m_indexedEvents[owner][0]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][1]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][2]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][3]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][4]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][5]),
                    static_cast<unsigned long long>(m_indexedEvents[owner][6]),
                    static_cast<unsigned long long>(m_indexedIndexBuckets[owner][0]),
                    static_cast<unsigned long long>(m_indexedIndexBuckets[owner][1]),
                    static_cast<unsigned long long>(m_indexedIndexBuckets[owner][2]),
                    static_cast<unsigned long long>(m_indexedIndexBuckets[owner][3]),
                    static_cast<unsigned long long>(m_indexedIndexBuckets[owner][4]));
        }
        for (size_t bucket = 0; bucket < ObjectStructureProfileSizeBucketCount; bucket++) {
            if (!(m_mapBuildsBySize[bucket] || m_mapFindsBySize[bucket])) {
                continue;
            }
            char sizeLabel[32];
            formatSizeBucket(bucket, sizeLabel, sizeof(sizeLabel));
            fprintf(stderr,
                    "OSP_MAP_SIZE properties=%s builds=%llu finds=%llu allocated_bytes=%llu\n",
                    sizeLabel,
                    static_cast<unsigned long long>(m_mapBuildsBySize[bucket]),
                    static_cast<unsigned long long>(m_mapFindsBySize[bucket]),
                    static_cast<unsigned long long>(m_mapBytesBySize[bucket]));
        }
        fprintf(stderr, "OSP_END\n");
    }

    bool enabled() const { return m_enabled; }

    void recordCreation(ObjectStructureProfileKind kind, size_t size)
    {
        m_size[kindIndex(kind)][sizeBucket(size)].creations++;
    }

    void recordFind(ObjectStructureProfileKind kind, size_t size,
                    const ObjectStructurePropertyName& name, bool hit,
                    bool lastCacheHit, size_t comparisons)
    {
        size_t k = kindIndex(kind);
        auto& counter = m_size[k][sizeBucket(size)];
        counter.finds++;
        counter.hits += hit;
        counter.misses += !hit;
        counter.lastCacheHits += lastCacheHit;
        counter.comparisons += comparisons;
        if (name.hasAtomicString()) {
            m_nameKinds[k][0]++;
        } else if (name.isSymbol()) {
            m_nameKinds[k][2]++;
        } else {
            m_nameKinds[k][1]++;
        }
        if (kind == ObjectStructureProfileKind::WithMap) {
            m_mapFindsBySize[sizeBucket(size)]++;
        }
    }

    void recordOperation(ObjectStructureProfileKind kind, size_t size, char operation)
    {
        auto& counter = m_size[kindIndex(kind)][sizeBucket(size)];
        if (operation == 'a') {
            counter.adds++;
        } else if (operation == 'r') {
            counter.removes++;
        } else {
            counter.replaces++;
        }
    }

    void recordTransitionLookup(bool map, bool reused, size_t comparisons)
    {
        if (map) {
            m_transitionMapLookups++;
            m_transitionMapReuses += reused;
        } else {
            m_transitionVectorLookups++;
            m_transitionVectorComparisons += comparisons;
            m_transitionVectorReuses += reused;
        }
    }

    void recordTransitionVectorToMap() { m_transitionVectorToMap++; }
    void recordTransitionExit() { m_transitionExits++; }

    void recordMapBuild(size_t size, size_t bytes, bool rebuild,
                        bool dense, bool nonAtomic)
    {
        m_mapBuilds++;
        m_mapRebuilds += rebuild;
        m_mapAllocatedBytes += bytes;
        m_mapMaxAllocation = std::max(m_mapMaxAllocation, static_cast<uint64_t>(bytes));
        m_mapDenseBuilds += dense;
        m_mapNonAtomicBuilds += nonAtomic;
        size_t bucket = sizeBucket(size);
        m_mapBuildsBySize[bucket]++;
        m_mapBytesBySize[bucket] += bytes;
    }

    void recordMapLazyBuild() { m_mapLazyBuilds++; }
    void recordMapInsert() { m_mapInserts++; }
    void recordMapHash(bool content)
    {
        m_mapHashCalls++;
        m_mapContentHashCalls += content;
    }
    void recordMapIndexConversion() { m_mapIndexConversions++; }
    void recordMapInsertProbes(size_t probes)
    {
        m_mapInsertProbes += probes;
        m_mapMaxInsertProbe = std::max(m_mapMaxInsertProbe, static_cast<uint64_t>(probes));
    }
    void recordMapLastCache() { m_mapLastCacheHits++; }
    void recordMapDenseFind(bool hit)
    {
        m_mapDenseFinds++;
        m_mapDenseHits += hit;
        m_mapDenseMisses += !hit;
    }
    void recordMapHashFind(bool hit, size_t probes)
    {
        m_mapHashFinds++;
        m_mapHashHits += hit;
        m_mapHashMisses += !hit;
        m_mapFindProbes += probes;
        m_mapMaxFindProbe = std::max(m_mapMaxFindProbe, static_cast<uint64_t>(probes));
    }
    void recordMapLinearFallback(size_t comparisons)
    {
        m_mapLinearFallbacks++;
        m_mapFallbackComparisons += comparisons;
    }
    void recordIndexed(ObjectStructureIndexedProfileOwner owner, ObjectStructureIndexedProfileEvent event, uint32_t index, size_t)
    {
        size_t ownerIndex = static_cast<size_t>(owner);
        size_t eventIndex = static_cast<size_t>(event);
        m_indexedEvents[ownerIndex][eventIndex]++;

        if (index < 16) {
            m_indexedIndexBuckets[ownerIndex][0]++;
        } else if (index < 256) {
            m_indexedIndexBuckets[ownerIndex][1]++;
        } else if (index < 4096) {
            m_indexedIndexBuckets[ownerIndex][2]++;
        } else if (index < 65536) {
            m_indexedIndexBuckets[ownerIndex][3]++;
        } else {
            m_indexedIndexBuckets[ownerIndex][4]++;
        }
    }

private:
    static size_t kindIndex(ObjectStructureProfileKind kind)
    {
        return static_cast<size_t>(kind);
    }

    static const char* kindName(size_t kind)
    {
        static const char* names[] = { "without-transition", "with-transition", "with-map" };
        return names[kind];
    }

    static const char* indexedOwnerName(size_t owner)
    {
        static const char* names[] = { "ordinary", "array", "other" };
        return names[owner];
    }

    static size_t sizeBucket(size_t size)
    {
        if (size <= ObjectStructureProfileExactSizeLimit) {
            return size;
        }
        if (size <= 255)
            return 129;
        if (size <= 511)
            return 130;
        if (size <= 1023)
            return 131;
        if (size <= 4095)
            return 132;
        if (size <= 16383)
            return 133;
        if (size <= 65535)
            return 134;
        return 135;
    }

    static void formatSizeBucket(size_t bucket, char* output, size_t outputSize)
    {
        if (bucket <= ObjectStructureProfileExactSizeLimit) {
            snprintf(output, outputSize, "%zu", bucket);
            return;
        }
        static const char* ranges[] = {
            "129-255", "256-511", "512-1023", "1024-4095",
            "4096-16383", "16384-65535", "65536+"
        };
        snprintf(output, outputSize, "%s", ranges[bucket - 129]);
    }

    bool m_enabled;
    ObjectStructureProfileSizeCounters m_size[ObjectStructureProfileKindCount][ObjectStructureProfileSizeBucketCount]{};
    uint64_t m_nameKinds[ObjectStructureProfileKindCount][3]{};
    uint64_t m_transitionVectorLookups{ 0 };
    uint64_t m_transitionVectorComparisons{ 0 };
    uint64_t m_transitionVectorReuses{ 0 };
    uint64_t m_transitionMapLookups{ 0 };
    uint64_t m_transitionMapReuses{ 0 };
    uint64_t m_transitionVectorToMap{ 0 };
    uint64_t m_transitionExits{ 0 };
    uint64_t m_mapBuilds{ 0 };
    uint64_t m_mapRebuilds{ 0 };
    uint64_t m_mapLazyBuilds{ 0 };
    uint64_t m_mapInserts{ 0 };
    uint64_t m_mapAllocatedBytes{ 0 };
    uint64_t m_mapMaxAllocation{ 0 };
    uint64_t m_mapDenseBuilds{ 0 };
    uint64_t m_mapNonAtomicBuilds{ 0 };
    uint64_t m_mapHashCalls{ 0 };
    uint64_t m_mapContentHashCalls{ 0 };
    uint64_t m_mapIndexConversions{ 0 };
    uint64_t m_mapInsertProbes{ 0 };
    uint64_t m_mapMaxInsertProbe{ 0 };
    uint64_t m_mapLastCacheHits{ 0 };
    uint64_t m_mapDenseFinds{ 0 };
    uint64_t m_mapDenseHits{ 0 };
    uint64_t m_mapDenseMisses{ 0 };
    uint64_t m_mapHashFinds{ 0 };
    uint64_t m_mapHashHits{ 0 };
    uint64_t m_mapHashMisses{ 0 };
    uint64_t m_mapFindProbes{ 0 };
    uint64_t m_mapMaxFindProbe{ 0 };
    uint64_t m_mapLinearFallbacks{ 0 };
    uint64_t m_mapFallbackComparisons{ 0 };
    uint64_t m_mapBuildsBySize[ObjectStructureProfileSizeBucketCount]{};
    uint64_t m_mapFindsBySize[ObjectStructureProfileSizeBucketCount]{};
    uint64_t m_mapBytesBySize[ObjectStructureProfileSizeBucketCount]{};
    uint64_t m_indexedEvents[ObjectStructureIndexedProfileOwnerCount][ObjectStructureIndexedProfileEventCount]{};
    uint64_t m_indexedIndexBuckets[ObjectStructureIndexedProfileOwnerCount][ObjectStructureIndexedProfileIndexBucketCount]{};
};

ObjectStructureProfile& objectStructureProfile()
{
    static ObjectStructureProfile profile;
    return profile;
}

inline bool objectStructureProfileEnabled()
{
    return objectStructureProfile().enabled();
}

} // namespace

void recordObjectStructureProfileCreation(ObjectStructureProfileKind kind, size_t propertyCount)
{
    if (objectStructureProfileEnabled()) {
        objectStructureProfile().recordCreation(kind, propertyCount);
    }
}

void recordObjectStructureIndexedProfileEvent(ObjectStructureIndexedProfileOwner owner, ObjectStructureIndexedProfileEvent event, uint32_t index, size_t propertyCount)
{
    if (objectStructureProfileEnabled()) {
        objectStructureProfile().recordIndexed(owner, event, index, propertyCount);
    }
}

#define OBJECT_STRUCTURE_PROFILE(code)         \
    do {                                       \
        if (objectStructureProfileEnabled()) { \
            objectStructureProfile().code;     \
        }                                      \
    } while (0)
#define OBJECT_STRUCTURE_PROFILE_VALUE(code) \
    do {                                     \
        code;                                \
    } while (0)
#else
#define OBJECT_STRUCTURE_PROFILE(code) \
    do {                               \
    } while (0)
#define OBJECT_STRUCTURE_PROFILE_VALUE(code) \
    do {                                     \
    } while (0)
#endif

void* ObjectStructureItemVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureItemVector)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureItemVector, m_buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureItemVector));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureIndexPropertyVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureIndexPropertyVector)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureIndexPropertyVector, m_buffer));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureIndexPropertyVector));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureIndexDescriptorVector::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureIndexDescriptorVector)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureIndexDescriptorVector, m_buffer));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureIndexDescriptorVector));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* ObjectStructureWithoutTransition::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithoutTransition)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithoutTransition, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithoutTransition, m_lastFoundPropertyName));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithoutTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectStructure* ObjectStructure::create(Context* ctx, ObjectStructureItemTightVector&& properties, bool preferTransition)
{
    bool hasIndexStringAsPropertyName = false;
    bool hasSymbol = false;
    bool hasNonAtomicPropertyName = false;
    bool hasEnumerableProperty = true;

    for (size_t i = 0; i < properties.size(); i++) {
        const ObjectStructurePropertyName& propertyName = properties[i].m_propertyName;
#ifndef NDEBUG
        // there should be no duplicated properties
        for (size_t j = i + 1; j < properties.size(); j++) {
            ASSERT(propertyName != properties[j].m_propertyName);
        }
#endif
        if (propertyName.isSymbol()) {
            hasSymbol = true;
        }
        if (!hasIndexStringAsPropertyName) {
            hasIndexStringAsPropertyName |= propertyName.isIndexString();
        }

        hasNonAtomicPropertyName |= propertyName.hasNonAtomicString();
        hasEnumerableProperty |= properties[i].m_descriptor.isEnumerable();
    }

    // Any Symbol-bearing structure uses the partitioned representation too.
    // This keeps the physical value/key order identical to ECMAScript's
    // numeric -> string -> symbol enumeration domains even when there is no
    // numeric property yet.
    if (hasIndexStringAsPropertyName || hasSymbol) {
        return new ObjectStructureWithIndexProperties(properties);
    } else if (!isTransitionModeAvailable(properties.size())) {
        return new ObjectStructureWithMap(hasIndexStringAsPropertyName, hasSymbol, hasEnumerableProperty, std::move(properties));
    } else if (preferTransition) {
        return new ObjectStructureWithTransition(std::move(properties), hasIndexStringAsPropertyName, hasSymbol, hasNonAtomicPropertyName, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(new ObjectStructureItemVector(std::move(properties)), hasIndexStringAsPropertyName, hasSymbol, hasNonAtomicPropertyName, hasEnumerableProperty);
    }
}

ObjectStructureFindResult ObjectStructureWithoutTransition::findProperty(const ObjectStructurePropertyName& s)
{
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
    size_t profileComparisons = 0;
#endif
    size_t size = m_properties->size();
    OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
    if (m_properties->size() && m_lastFoundPropertyName == s) {
        uint16_t lastIndex = lastFoundPropertyIndex();
        if (lastIndex == std::numeric_limits<uint16_t>::max()) {
            OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, false, true, profileComparisons));
            return std::make_pair(std::numeric_limits<size_t>::max(), Optional<const ObjectStructurePropertyDescriptor*>());
        }
        OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, true, true, profileComparisons));
        return std::make_pair(lastIndex, &(*m_properties)[lastIndex].m_descriptor);
    }
    m_lastFoundPropertyName = s;
    setLastFoundPropertyIndex(std::numeric_limits<uint16_t>::max());

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if ((*m_properties)[i].m_propertyName.rawValue() == s.rawValue()) {
                    setLastFoundPropertyIndex(i);
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &(*m_properties)[i].m_descriptor);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if ((*m_properties)[i].m_propertyName == as) {
                    setLastFoundPropertyIndex(i);
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &(*m_properties)[i].m_descriptor);
                }
            }
        }
    } else if (s.isSymbol()) {
        if (m_hasSymbolPropertyName) {
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if ((*m_properties)[i].m_propertyName == s) {
                    setLastFoundPropertyIndex(i);
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &(*m_properties)[i].m_descriptor);
                }
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
            if ((*m_properties)[i].m_propertyName == s) {
                setLastFoundPropertyIndex(i);
                OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, true, false, profileComparisons));
                return std::make_pair(i, &(*m_properties)[i].m_descriptor);
            }
        }
    }

    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithoutTransition, size, s, false, false, profileComparisons));
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

ObjectStructureFindResult ObjectStructureWithoutTransition::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithoutTransition::propertyDescriptor(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_descriptor;
}

bool ObjectStructureWithoutTransition::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithoutTransition::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithoutTransition::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_propertyName;
}

const ObjectStructureItem* ObjectStructureWithoutTransition::nonIndexPropertiesData() const
{
    return m_properties->data();
}

size_t ObjectStructureWithoutTransition::propertyCount() const
{
    return m_properties->size();
}

size_t ObjectStructureWithoutTransition::namedPropertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithoutTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithoutTransition, m_properties->size(), 'a'));
    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    if (name.isSymbol()) {
        ObjectStructureItemVector properties(*m_properties, newItem);
        return new ObjectStructureWithIndexProperties(properties);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool nameIsSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructure* newStructure;
    ObjectStructureItemVector* propertiesForNewStructure;
    if (m_isReferencedByInlineCache) {
        propertiesForNewStructure = new ObjectStructureItemVector(*m_properties, newItem);
    } else {
        m_properties->push_back(newItem);
        propertiesForNewStructure = m_properties;
        m_properties = nullptr;
    }

    if (propertiesForNewStructure->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        newStructure = new ObjectStructureWithMap(propertiesForNewStructure, nullptr, nameIsIndexString, nameIsSymbol, hasEnumerableProperty);
    } else {
        newStructure = new ObjectStructureWithoutTransition(propertiesForNewStructure, nameIsIndexString, nameIsSymbol, hasNonAtomicName, hasEnumerableProperty);
    }

    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    return new ObjectStructureWithIndexProperties(*m_properties, index, desc);
}

ObjectStructure* ObjectStructureWithoutTransition::removeProperty(size_t pIndex)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithoutTransition, m_properties->size(), 'r'));
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeFitWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties)[i].m_propertyName.isSymbol();
        hasNonAtomicName = hasNonAtomicName | (*m_properties)[i].m_propertyName.hasNonAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    auto newStructure = new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    if (!m_isReferencedByInlineCache) {
        m_properties = nullptr;
    }
    return newStructure;
}

ObjectStructure* ObjectStructureWithoutTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithoutTransition, m_properties->size(), 'p'));
    ObjectStructureItemVector* newProperties = m_properties;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties);
    } else {
        m_properties = nullptr;
    }
    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, hasEnumerableProperty);
}

void* ObjectStructureWithTransition::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithTransition)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithTransition, m_transitionTableVectorBuffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithTransition));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectStructureFindResult ObjectStructureWithTransition::findProperty(const ObjectStructurePropertyName& s)
{
    size_t size = m_properties.size();
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
    size_t profileComparisons = 0;
#endif

    if (LIKELY(s.hasAtomicString())) {
        if (LIKELY(!m_hasNonAtomicPropertyName)) {
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if (m_properties[i].m_propertyName.rawValue() == s.rawValue()) {
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &m_properties[i].m_descriptor);
                }
            }
        } else {
            AtomicString as = s.asAtomicString();
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if (m_properties[i].m_propertyName == as) {
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &m_properties[i].m_descriptor);
                }
            }
        }
    } else if (s.isSymbol()) {
        if (m_hasSymbolPropertyName) {
            for (size_t i = 0; i < size; i++) {
                OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
                if (m_properties[i].m_propertyName == s) {
                    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithTransition, size, s, true, false, profileComparisons));
                    return std::make_pair(i, &m_properties[i].m_descriptor);
                }
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
            if (m_properties[i].m_propertyName == s) {
                OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithTransition, size, s, true, false, profileComparisons));
                return std::make_pair(i, &m_properties[i].m_descriptor);
            }
        }
    }

    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithTransition, size, s, false, false, profileComparisons));
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

ObjectStructureFindResult ObjectStructureWithTransition::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithTransition::propertyDescriptor(size_t valueIndex) const
{
    return m_properties[valueIndex].m_descriptor;
}

bool ObjectStructureWithTransition::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithTransition::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithTransition::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties[valueIndex].m_propertyName;
}

const ObjectStructureItem* ObjectStructureWithTransition::nonIndexPropertiesData() const
{
    return m_properties.data();
}

size_t ObjectStructureWithTransition::propertyCount() const
{
    return m_properties.size();
}

size_t ObjectStructureWithTransition::namedPropertyCount() const
{
    return m_properties.size();
}

ObjectStructure* ObjectStructureWithTransition::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithTransition, m_properties.size(), 'a'));
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
    size_t profileTransitionComparisons = 0;
#endif
    if (m_doesTransitionTableUseMap) {
        auto iter = m_transitionTableMap->find(ObjectStructureTransitionMapItem(name, desc));
        if (iter != m_transitionTableMap->end()) {
            OBJECT_STRUCTURE_PROFILE(recordTransitionLookup(true, true, 0));
            return iter->second;
        }
        OBJECT_STRUCTURE_PROFILE(recordTransitionLookup(true, false, 0));
    } else {
        size_t len = m_transitionTableVectorBufferSize;
        for (size_t i = 0; i < len; i++) {
            OBJECT_STRUCTURE_PROFILE_VALUE(profileTransitionComparisons++);
            const auto& item = m_transitionTableVectorBuffer[i];
            if (item.m_descriptor == desc && item.m_propertyName == name) {
                OBJECT_STRUCTURE_PROFILE(recordTransitionLookup(false, true, profileTransitionComparisons));
                return item.m_structure;
            }
        }
        OBJECT_STRUCTURE_PROFILE(recordTransitionLookup(false, false, profileTransitionComparisons));
    }

    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    if (name.isSymbol()) {
        OBJECT_STRUCTURE_PROFILE(recordTransitionExit());
        ObjectStructureItemVector properties(m_properties, newItem);
        return new ObjectStructureWithIndexProperties(properties);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasNonAtomicName = m_hasNonAtomicPropertyName ? true : name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();
    ObjectStructure* newObjectStructure;

    size_t nextSize = m_properties.size() + 1;
    // ObjectStructureWithTransition cannot directly convert to ObjectStructureWithMap by just adding one property
    ASSERT(nextSize < ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE);
    if (nextSize > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE || nameIsIndexString) {
        OBJECT_STRUCTURE_PROFILE(recordTransitionExit());
        ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithoutTransition(newProperties, nameIsIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    } else {
        ObjectStructureItemTightVector newProperties(m_properties, newItem);
        newObjectStructure = new ObjectStructureWithTransition(std::move(newProperties), nameIsIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
        ObjectStructureTransitionVectorItem newTransitionItem(name, desc, newObjectStructure);

        if (m_doesTransitionTableUseMap) {
            m_transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                        newTransitionItem.m_structure));
        } else {
            if (m_transitionTableVectorBufferSize + 1 > ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MAP_MIN_SIZE) {
                OBJECT_STRUCTURE_PROFILE(recordTransitionVectorToMap());
                ObjectStructureTransitionTableMap* transitionTableMap = new (GC) ObjectStructureTransitionTableMap();
                for (size_t i = 0; i < m_transitionTableVectorBufferSize; i++) {
                    transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(m_transitionTableVectorBuffer[i].m_propertyName, m_transitionTableVectorBuffer[i].m_descriptor),
                                                              m_transitionTableVectorBuffer[i].m_structure));
                }
                transitionTableMap->insert(std::make_pair(ObjectStructureTransitionMapItem(newTransitionItem.m_propertyName, newTransitionItem.m_descriptor),
                                                          newTransitionItem.m_structure));

                GC_FREE(m_transitionTableVectorBuffer);
                m_doesTransitionTableUseMap = true;
                m_transitionTableMap = transitionTableMap;
                m_transitionTableVectorBufferCapacity = 0;
                m_transitionTableVectorBufferSize = 0;
            } else {
                if (m_transitionTableVectorBufferCapacity <= (size_t)(m_transitionTableVectorBufferSize + 1)) {
                    m_transitionTableVectorBufferCapacity = std::min(computeVectorAllocateSize(m_transitionTableVectorBufferSize + 1), (size_t)std::numeric_limits<uint8_t>::max());
                    m_transitionTableVectorBuffer = (ObjectStructureTransitionVectorItem*)GC_REALLOC_NO_SHRINK(m_transitionTableVectorBuffer, sizeof(ObjectStructureTransitionVectorItem) * m_transitionTableVectorBufferCapacity);
                }
                m_transitionTableVectorBuffer[m_transitionTableVectorBufferSize] = newTransitionItem;
                m_transitionTableVectorBufferSize++;
            }
        }
    }

    return newObjectStructure;
}

ObjectStructure* ObjectStructureWithTransition::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    OBJECT_STRUCTURE_PROFILE(recordTransitionExit());
    return new ObjectStructureWithIndexProperties(m_properties, index, desc);
}

ObjectStructure* ObjectStructureWithTransition::removeProperty(size_t pIndex)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithTransition, m_properties.size(), 'r'));
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    newProperties->resizeFitWithUninitializedValues(m_properties.size() - 1);
    size_t pc = m_properties.size();

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < pc; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | m_properties[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | m_properties[i].m_propertyName.isSymbol();
        hasNonAtomicName = hasNonAtomicName | m_properties[i].m_propertyName.hasNonAtomicString();
        hasEnumerableProperty = hasEnumerableProperty | m_properties[i].m_descriptor.isEnumerable();
        (*newProperties)[newIdx].m_propertyName = m_properties[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = m_properties[i].m_descriptor;
        newIdx++;
    }

    return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithTransition, m_properties.size(), 'p'));
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithTransition::convertToNonTransitionStructure()
{
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector(m_properties);
    return new ObjectStructureWithoutTransition(newProperties, m_hasIndexPropertyName, m_hasSymbolPropertyName, m_hasNonAtomicPropertyName, m_hasEnumerableProperty);
}

uint8_t PropertyNameMapWithCache::entryWidth(size_t count)
{
    // Zero marks an empty bucket; all live entries store propertyIndex + 1.
    if (count <= UINT8_MAX) {
        return sizeof(uint8_t);
    }
    if (count <= UINT16_MAX) {
        return sizeof(uint16_t);
    }
    ASSERT(count <= UINT32_MAX);
    return sizeof(uint32_t);
}

PropertyNameMapWithCache::PropertyNameMapWithCache(const ObjectStructureItemVector& properties)
{
    rebuild(properties);
}

size_t PropertyNameMapWithCache::hash(const ObjectStructurePropertyName& name) const
{
    // Atomic names normally use pointer identity. If a template contributed
    // non-atomic strings, hash all strings by content so equal names agree.
    size_t value;
    if (LIKELY(!m_hasNonAtomicNames)) {
        OBJECT_STRUCTURE_PROFILE(recordMapHash(false));
        // Atomic strings use exact pointers. Symbols use tagged pointers and
        // numeric names use their immediate representation, all of which are
        // stable identity values without repeating the type dispatch.
        value = name.rawValue();
    } else {
        OBJECT_STRUCTURE_PROFILE(recordMapHash(name.isPlainString()));
        value = name.hasNonAtomicString() ? name.plainString()->hashValue() : name.rawValue();
    }
    // Mix aligned pointers before masking off the low bits. Use 32-bit
    // arithmetic here to keep this inexpensive on ARM32 as well.
    uint32_t mixed = static_cast<uint32_t>(value ^ (value >> 16));
    mixed *= 0x9e3779b9U;
    return mixed ^ (mixed >> 16);
}

template <typename Entry>
void PropertyNameMapWithCache::insertEntry(const ObjectStructurePropertyName& name, size_t index)
{
    auto entries = static_cast<Entry*>(m_entries);
    if (m_denseCapacity) {
        OBJECT_STRUCTURE_PROFILE(recordMapIndexConversion());
        uint32_t numeric = name.tryToUseAsIndexProperty();
        if (numeric < m_denseCapacity) {
            entries[m_capacity + numeric] = static_cast<Entry>(index + 1);
            return;
        }
    }
    size_t slot = hash(name) & (m_capacity - 1);
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
    size_t profileProbes = 0;
#endif
    while (entries[slot]) {
        OBJECT_STRUCTURE_PROFILE_VALUE(profileProbes++);
        slot = (slot + 1) & (m_capacity - 1);
    }
    OBJECT_STRUCTURE_PROFILE(recordMapInsertProbes(profileProbes));
    entries[slot] = static_cast<Entry>(index + 1);
    m_occupied++;
}

template <typename Entry>
size_t PropertyNameMapWithCache::findEntry(const ObjectStructurePropertyName& name, const ObjectStructureItemVector& properties) const
{
    auto entries = static_cast<const Entry*>(m_entries);
    if (m_denseCapacity) {
        OBJECT_STRUCTURE_PROFILE(recordMapIndexConversion());
        uint32_t numeric = name.tryToUseAsIndexProperty();
        if (numeric < m_denseCapacity) {
            auto entry = entries[m_capacity + numeric];
            OBJECT_STRUCTURE_PROFILE(recordMapDenseFind(entry != 0));
            return entry ? static_cast<size_t>(entry) - 1 : SIZE_MAX;
        }
    }
    size_t slot = hash(name) & (m_capacity - 1);
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
    size_t profileProbes = 0;
#endif
    while (auto entry = entries[slot]) {
        OBJECT_STRUCTURE_PROFILE_VALUE(profileProbes++);
        size_t index = static_cast<size_t>(entry) - 1;
        const auto& candidate = properties[index].m_propertyName;
        if (candidate.rawValue() == name.rawValue()
            || (m_hasNonAtomicNames && name.isPlainString() && candidate == name)) {
            OBJECT_STRUCTURE_PROFILE(recordMapHashFind(true, profileProbes));
            return index;
        }
        slot = (slot + 1) & (m_capacity - 1);
    }
    OBJECT_STRUCTURE_PROFILE(recordMapHashFind(false, profileProbes));
    return SIZE_MAX;
}

void PropertyNameMapWithCache::rebuild(const ObjectStructureItemVector& properties)
{
    m_size = properties.size();
    m_entryWidth = entryWidth(m_size);
    m_hasNonAtomicNames = false;

    // A bounded low-index region, enabled only when at least half full.
    // A sparse key such as "4294967294" must never size this allocation.
    size_t denseCapacity = 16;
    while (denseCapacity < m_size) {
        denseCapacity *= 2;
    }
    size_t denseCount = 0;
    for (size_t i = 0; i < m_size; i++) {
        const auto& name = properties[i].m_propertyName;
        m_hasNonAtomicNames |= name.hasNonAtomicString();
        OBJECT_STRUCTURE_PROFILE(recordMapIndexConversion());
        denseCount += name.tryToUseAsIndexProperty() < denseCapacity;
    }
    if (denseCount < denseCapacity / 2) {
        denseCapacity = 0;
        denseCount = 0;
    }
    m_denseCapacity = denseCapacity;
    m_capacity = 4;
    while (m_size - denseCount > m_capacity - std::max(m_capacity / 4, static_cast<size_t>(1))) {
        m_capacity *= 2;
    }

    void* oldEntries = m_entries;
    size_t bytes = (m_capacity + m_denseCapacity) * m_entryWidth;
    OBJECT_STRUCTURE_PROFILE(recordMapBuild(m_size, bytes, oldEntries != nullptr, m_denseCapacity != 0, m_hasNonAtomicNames));
    m_entries = GC_MALLOC_ATOMIC(bytes);
    memset(m_entries, 0, bytes);
    m_occupied = 0;
    for (size_t i = 0; i < m_size; i++) {
        const auto& name = properties[i].m_propertyName;
        if (m_entryWidth == sizeof(uint8_t)) {
            insertEntry<uint8_t>(name, i);
        } else if (m_entryWidth == sizeof(uint16_t)) {
            insertEntry<uint16_t>(name, i);
        } else {
            insertEntry<uint32_t>(name, i);
        }
    }
    if (oldEntries) {
        GC_FREE(oldEntries);
    }
    if (m_size) {
        m_lastName = properties[m_size - 1].m_propertyName;
        m_lastIndex = m_size - 1;
    }
}

void PropertyNameMapWithCache::insert(const ObjectStructureItemVector& properties)
{
    OBJECT_STRUCTURE_PROFILE(recordMapInsert());
    ASSERT(properties.size() == m_size + 1);
    const auto& name = properties[properties.size() - 1].m_propertyName;
    if (m_denseCapacity) {
        OBJECT_STRUCTURE_PROFILE(recordMapIndexConversion());
    }
    bool dense = m_denseCapacity && name.tryToUseAsIndexProperty() < m_denseCapacity;
    if (entryWidth(properties.size()) != m_entryWidth
        || (!dense && m_occupied + 1 > m_capacity - std::max(m_capacity / 4, static_cast<size_t>(1)))
        || (!m_hasNonAtomicNames && name.hasNonAtomicString())) {
        rebuild(properties);
        return;
    }
    if (m_entryWidth == sizeof(uint8_t)) {
        insertEntry<uint8_t>(name, m_size);
    } else if (m_entryWidth == sizeof(uint16_t)) {
        insertEntry<uint16_t>(name, m_size);
    } else {
        insertEntry<uint32_t>(name, m_size);
    }
    m_lastName = name;
    m_lastIndex = m_size++;
}

size_t PropertyNameMapWithCache::find(const ObjectStructurePropertyName& name, const ObjectStructureItemVector& properties)
{
    if (name == m_lastName) {
        OBJECT_STRUCTURE_PROFILE(recordMapLastCache());
        return m_lastIndex;
    }
    m_lastName = name;
    // A non-atomic query can compare equal to an atomic stored name even
    // though its hash is content-based. This uncommon path must still work.
    if (UNLIKELY(!m_hasNonAtomicNames && name.hasNonAtomicString())) {
        m_lastIndex = SIZE_MAX;
#if defined(ESCARGOT_OBJECT_STRUCTURE_PROFILE)
        size_t profileComparisons = 0;
#endif
        for (size_t i = 0; i < properties.size(); i++) {
            OBJECT_STRUCTURE_PROFILE_VALUE(profileComparisons++);
            if (properties[i].m_propertyName == name) {
                m_lastIndex = i;
                break;
            }
        }
        OBJECT_STRUCTURE_PROFILE(recordMapLinearFallback(profileComparisons));
    } else if (m_entryWidth == sizeof(uint8_t)) {
        m_lastIndex = findEntry<uint8_t>(name, properties);
    } else if (m_entryWidth == sizeof(uint16_t)) {
        m_lastIndex = findEntry<uint16_t>(name, properties);
    } else {
        m_lastIndex = findEntry<uint32_t>(name, properties);
    }
    return m_lastIndex;
}

void* ObjectStructureWithMap::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ObjectStructureWithMap)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithMap, m_properties));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ObjectStructureWithMap, m_propertyNameMap));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ObjectStructureWithMap));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}


ObjectStructureFindResult ObjectStructureWithMap::findProperty(const ObjectStructurePropertyName& s)
{
    if (!m_propertyNameMap) {
        OBJECT_STRUCTURE_PROFILE(recordMapLazyBuild());
        m_propertyNameMap = createPropertyNameMap(m_properties);
    }
    auto idx = m_propertyNameMap->find(s, *m_properties);
    if (idx == SIZE_MAX) {
        OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithMap, m_properties->size(), s, false, false, 0));
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    OBJECT_STRUCTURE_PROFILE(recordFind(ObjectStructureProfileKind::WithMap, m_properties->size(), s, true, false, 0));
    return std::make_pair(idx, &(m_properties->data()[idx].m_descriptor));
}

ObjectStructureFindResult ObjectStructureWithMap::findIndexProperty(uint32_t)
{
    return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithMap::propertyDescriptor(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_descriptor;
}

bool ObjectStructureWithMap::isIndexProperty(size_t) const
{
    return false;
}

uint32_t ObjectStructureWithMap::indexPropertyName(size_t) const
{
    ASSERT_NOT_REACHED();
    return Value::InvalidIndexPropertyValue;
}

const ObjectStructurePropertyName& ObjectStructureWithMap::nonIndexPropertyName(size_t valueIndex) const
{
    return m_properties->at(valueIndex).m_propertyName;
}

const ObjectStructureItem* ObjectStructureWithMap::nonIndexPropertiesData() const
{
    return m_properties->data();
}

size_t ObjectStructureWithMap::propertyCount() const
{
    return m_properties->size();
}

size_t ObjectStructureWithMap::namedPropertyCount() const
{
    return m_properties->size();
}

ObjectStructure* ObjectStructureWithMap::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithMap, m_properties->size(), 'a'));
    ObjectStructureItem newItem(name, desc);
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    if (name.isSymbol()) {
        ObjectStructureItemVector properties(*m_properties, newItem);
        return new ObjectStructureWithIndexProperties(properties);
    }
    bool nameIsIndexString = m_hasIndexPropertyName ? true : name.isIndexString();
    bool hasSymbol = m_hasSymbolPropertyName ? true : name.isSymbol();
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : desc.isEnumerable();

    ObjectStructureItemVector* newProperties;
    Optional<PropertyNameMapWithCache*> newPropertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties, newItem);
    } else {
        newProperties = m_properties;
        newProperties->push_back(newItem);
        m_properties = nullptr;
        if (m_propertyNameMap) {
            m_propertyNameMap->insert(*newProperties);
            ASSERT(m_propertyNameMap->size() == newProperties->size());
            newPropertyNameMap = m_propertyNameMap;
            m_propertyNameMap = nullptr;
        }
    }

    ObjectStructure* newStructure = new ObjectStructureWithMap(newProperties, newPropertyNameMap, nameIsIndexString, hasSymbol, hasEnumerableProperty);
    return newStructure;
}

ObjectStructure* ObjectStructureWithMap::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    return new ObjectStructureWithIndexProperties(*m_properties, index, desc);
}

ObjectStructure* ObjectStructureWithMap::removeProperty(size_t pIndex)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithMap, m_properties->size(), 'r'));
    ObjectStructureItemVector* newProperties = new ObjectStructureItemVector();
    size_t ps = m_properties->size();
    newProperties->resizeFitWithUninitializedValues(ps - 1);

    size_t newIdx = 0;
    bool hasIndexString = false;
    bool hasSymbol = false;
    bool hasNonAtomicName = false;
    bool hasEnumerableProperty = false;
    for (size_t i = 0; i < ps; i++) {
        if (i == pIndex)
            continue;
        hasIndexString = hasIndexString | (*m_properties)[i].m_propertyName.isIndexString();
        hasSymbol = hasSymbol | (*m_properties)[i].m_propertyName.isSymbol();
        hasEnumerableProperty = hasEnumerableProperty | (*m_properties)[i].m_descriptor.isEnumerable();
        hasNonAtomicName = hasNonAtomicName | (*m_properties)[i].m_propertyName.hasNonAtomicString();
        (*newProperties)[newIdx].m_propertyName = (*m_properties)[i].m_propertyName;
        (*newProperties)[newIdx].m_descriptor = (*m_properties)[i].m_descriptor;
        newIdx++;
    }

    if (!m_isReferencedByInlineCache) {
        m_properties = nullptr;
        m_propertyNameMap = nullptr;
    }
    if (newProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        return new ObjectStructureWithMap(newProperties, nullptr, hasIndexString, hasSymbol, hasEnumerableProperty);
    } else {
        return new ObjectStructureWithoutTransition(newProperties, hasIndexString, hasSymbol, hasNonAtomicName, hasEnumerableProperty);
    }
}

ObjectStructure* ObjectStructureWithMap::replacePropertyDescriptor(size_t idx, const ObjectStructurePropertyDescriptor& newDesc)
{
    OBJECT_STRUCTURE_PROFILE(recordOperation(ObjectStructureProfileKind::WithMap, m_properties->size(), 'p'));
    ObjectStructureItemVector* newProperties = m_properties;
    auto newPropertyNameMap = m_propertyNameMap;

    if (m_isReferencedByInlineCache) {
        newProperties = new ObjectStructureItemVector(*m_properties);
        newPropertyNameMap = nullptr;
    } else {
        m_properties = nullptr;
        m_propertyNameMap = nullptr;
    }

    newProperties->at(idx).m_descriptor = newDesc;
    bool hasEnumerableProperty = m_hasEnumerableProperty ? true : newDesc.isEnumerable();
    return new ObjectStructureWithMap(newProperties, newPropertyNameMap, m_hasIndexPropertyName, m_hasSymbolPropertyName, hasEnumerableProperty);
}

constexpr size_t IndexPropertyOrderChunk::Capacity;

void* IndexPropertyOrderChunk::operator new(size_t size)
{
    return GC_MALLOC_ATOMIC(size);
}

IndexPropertyMapWithCache::IndexPropertyMapWithCache(const ObjectStructureIndexPropertyVector& properties)
{
    ASSERT(properties.size() < UINT32_MAX);
    rebuildHash(properties);
    rebuildOrder(properties);
}

void IndexPropertyMapWithCache::insert(const ObjectStructureIndexPropertyVector& properties)
{
    ASSERT(properties.size() == m_size + 1);
    ASSERT(m_size < UINT32_MAX);
    uint32_t index = properties[properties.size() - 1];
    insertOrder(properties, static_cast<uint32_t>(m_size));
    if (properties.size() > m_capacity - m_capacity / 4) {
        rebuildHash(properties);
        return;
    }
    insertHashEntry(index, m_size);
    m_size++;
}

void IndexPropertyMapWithCache::rebuildOrder(const ObjectStructureIndexPropertyVector& properties)
{
    m_orderChunks.clear();
    if (properties.empty()) {
        return;
    }

    uint32_t* ordinals = static_cast<uint32_t*>(GC_MALLOC_ATOMIC(sizeof(uint32_t) * properties.size()));
    for (size_t i = 0; i < properties.size(); i++) {
        ordinals[i] = static_cast<uint32_t>(i);
    }
    std::sort(ordinals, ordinals + properties.size(),
              [&properties](uint32_t a, uint32_t b) {
                  return properties[a] < properties[b];
              });
    for (size_t offset = 0; offset < properties.size(); offset += IndexPropertyOrderChunk::Capacity) {
        auto* chunk = new IndexPropertyOrderChunk();
        chunk->m_size = std::min(IndexPropertyOrderChunk::Capacity, properties.size() - offset);
        memcpy(chunk->m_ordinals, ordinals + offset, sizeof(uint32_t) * chunk->m_size);
        m_orderChunks.push_back(chunk);
    }
    GC_FREE(ordinals);
}

void IndexPropertyMapWithCache::insertOrder(const ObjectStructureIndexPropertyVector& properties, uint32_t ordinal)
{
    uint32_t index = properties[ordinal];
    size_t chunkIndex = static_cast<size_t>(std::lower_bound(m_orderChunks.begin(), m_orderChunks.end(), index,
                                                             [&properties](const IndexPropertyOrderChunk* chunk, uint32_t value) {
                                                                 uint32_t lastOrdinal = chunk->m_ordinals[chunk->m_size - 1];
                                                                 return properties[lastOrdinal] < value;
                                                             })
                                            - m_orderChunks.begin());
    if (chunkIndex == m_orderChunks.size()) {
        chunkIndex--;
    }

    IndexPropertyOrderChunk* chunk = m_orderChunks[chunkIndex];
    if (chunk->m_size == IndexPropertyOrderChunk::Capacity) {
        auto* right = new IndexPropertyOrderChunk();
        size_t leftSize = IndexPropertyOrderChunk::Capacity / 2;
        right->m_size = chunk->m_size - leftSize;
        memcpy(right->m_ordinals, chunk->m_ordinals + leftSize, sizeof(uint32_t) * right->m_size);
        chunk->m_size = leftSize;
        m_orderChunks.insert(chunkIndex + 1, right);
        if (index > properties[chunk->m_ordinals[chunk->m_size - 1]]) {
            chunk = right;
        }
    }

    size_t position = static_cast<size_t>(std::lower_bound(chunk->m_ordinals, chunk->m_ordinals + chunk->m_size, index,
                                                           [&properties](uint32_t existingOrdinal, uint32_t value) {
                                                               return properties[existingOrdinal] < value;
                                                           })
                                          - chunk->m_ordinals);
    memmove(chunk->m_ordinals + position + 1,
            chunk->m_ordinals + position,
            sizeof(uint32_t) * (chunk->m_size - position));
    chunk->m_ordinals[position] = ordinal;
    chunk->m_size++;
}

void IndexPropertyMapWithCache::insertHashEntry(uint32_t index, size_t ordinal)
{
    uint32_t mixed = index ^ (index >> 16);
    mixed *= 0x9e3779b9U;
    mixed ^= mixed >> 16;
    size_t slot = mixed & (m_capacity - 1);
    while (m_entries[slot]) {
        slot = (slot + 1) & (m_capacity - 1);
    }
    ASSERT(ordinal < UINT32_MAX);
    m_entries[slot] = static_cast<uint32_t>(ordinal + 1);
}

void IndexPropertyMapWithCache::rebuildHash(const ObjectStructureIndexPropertyVector& properties)
{
    size_t capacity = 16;
    while (properties.size() > capacity - capacity / 4) {
        capacity *= 2;
    }
    uint32_t* oldEntries = m_entries;
    m_capacity = capacity;
    m_size = properties.size();
    m_entries = static_cast<uint32_t*>(GC_MALLOC_ATOMIC(sizeof(uint32_t) * m_capacity));
    memset(m_entries, 0, sizeof(uint32_t) * m_capacity);
    for (size_t i = 0; i < properties.size(); i++) {
        insertHashEntry(properties[i], i);
    }
    if (oldEntries) {
        GC_FREE(oldEntries);
    }
}

size_t IndexPropertyMapWithCache::find(uint32_t index, const ObjectStructureIndexPropertyVector& properties) const
{
    uint32_t mixed = index ^ (index >> 16);
    mixed *= 0x9e3779b9U;
    mixed ^= mixed >> 16;
    size_t slot = mixed & (m_capacity - 1);
    while (uint32_t entry = m_entries[slot]) {
        size_t ordinal = static_cast<size_t>(entry) - 1;
        if (properties[ordinal] == index) {
            return ordinal;
        }
        slot = (slot + 1) & (m_capacity - 1);
    }
    return SIZE_MAX;
}

void IndexPropertyMapWithCache::fillOrdinalsInOrder(uint32_t* ordinals) const
{
    size_t position = 0;
    for (const auto* chunk : m_orderChunks) {
        memcpy(ordinals + position, chunk->m_ordinals, sizeof(uint32_t) * chunk->m_size);
        position += chunk->m_size;
    }
    ASSERT(position == m_size);
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithIndexProperties::defaultIndexPropertyDescriptor()
{
    static const ObjectStructurePropertyDescriptor descriptor = ObjectStructurePropertyDescriptor::createDataDescriptor();
    return descriptor;
}

bool ObjectStructureWithIndexProperties::isDefaultIndexPropertyDescriptor(const ObjectStructurePropertyDescriptor& descriptor)
{
    return descriptor == defaultIndexPropertyDescriptor();
}

size_t ObjectStructureWithIndexProperties::indexPropertyStorageSize() const
{
    return m_indexProperties ? m_indexProperties->size() : m_inlineIndexProperties.m_size;
}

uint32_t ObjectStructureWithIndexProperties::indexPropertyAt(size_t ordinal) const
{
    ASSERT(ordinal < indexPropertyStorageSize());
    return m_indexProperties ? (*m_indexProperties)[ordinal] : m_inlineIndexProperties.m_keys[ordinal];
}

void ObjectStructureWithIndexProperties::appendIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& descriptor)
{
    size_t previousCount = indexPropertyStorageSize();
    if (m_indexDescriptors) {
        m_indexDescriptors->push_back(descriptor);
    } else if (!isDefaultIndexPropertyDescriptor(descriptor)) {
        m_indexDescriptors = new ObjectStructureIndexDescriptorVector();
        m_indexDescriptors->reserve(m_indexProperties ? m_indexProperties->capacity() : InlineIndexProperties::Capacity);
        for (size_t i = 0; i < previousCount; i++) {
            m_indexDescriptors->push_back(defaultIndexPropertyDescriptor());
        }
        m_indexDescriptors->push_back(descriptor);
    }
    if (m_indexProperties) {
        m_indexProperties->push_back(index);
    } else {
        ASSERT(previousCount < InlineIndexProperties::Capacity);
        m_inlineIndexProperties.m_keys[previousCount] = index;
        m_inlineIndexProperties.m_size++;
    }
}

void ObjectStructureWithIndexProperties::sortIndexProperties()
{
    if (!m_indexProperties) {
        ASSERT(m_inlineIndexProperties.m_size <= InlineIndexProperties::Capacity);
        if (m_inlineIndexProperties.m_size == 2 && m_inlineIndexProperties.m_keys[0] > m_inlineIndexProperties.m_keys[1]) {
            std::swap(m_inlineIndexProperties.m_keys[0], m_inlineIndexProperties.m_keys[1]);
            if (m_indexDescriptors) {
                std::swap((*m_indexDescriptors)[0], (*m_indexDescriptors)[1]);
            }
        }
        return;
    }
    if (!m_indexDescriptors) {
        std::sort(m_indexProperties->begin(), m_indexProperties->end());
        return;
    }

    Vector<uint32_t, GCUtil::gc_malloc_atomic_allocator<uint32_t>> ordinals;
    ordinals.resize(m_indexProperties->size());
    for (size_t i = 0; i < ordinals.size(); i++) {
        ordinals[i] = static_cast<uint32_t>(i);
    }
    std::sort(ordinals.begin(), ordinals.end(), [this](uint32_t a, uint32_t b) {
        return (*m_indexProperties)[a] < (*m_indexProperties)[b];
    });

    auto* sortedProperties = new ObjectStructureIndexPropertyVector();
    auto* sortedDescriptors = new ObjectStructureIndexDescriptorVector();
    sortedProperties->reserve(m_indexProperties->size());
    sortedDescriptors->reserve(m_indexDescriptors->size());
    for (uint32_t ordinal : ordinals) {
        sortedProperties->push_back((*m_indexProperties)[ordinal]);
        sortedDescriptors->push_back((*m_indexDescriptors)[ordinal]);
    }
    m_indexProperties = sortedProperties;
    m_indexDescriptors = sortedDescriptors;
}

void ObjectStructureWithIndexProperties::finishConstruction()
{
    m_hasIndexPropertyName = indexPropertyStorageSize() != 0;
    m_hasSymbolPropertyName = !m_symbolProperties->empty();
    m_hasNonAtomicPropertyName = false;
    m_hasEnumerableProperty = false;
    for (const auto& item : *m_namedProperties) {
        ASSERT(!item.m_propertyName.isSymbol());
        m_hasNonAtomicPropertyName |= item.m_propertyName.hasNonAtomicString();
        m_hasEnumerableProperty |= item.m_descriptor.isEnumerable();
    }
    for (const auto& item : *m_symbolProperties) {
        ASSERT(item.m_propertyName.isSymbol());
        m_hasEnumerableProperty |= item.m_descriptor.isEnumerable();
    }
    if (indexPropertyStorageSize()) {
        if (!m_indexDescriptors) {
            m_hasEnumerableProperty = true;
        } else {
            for (const auto& descriptor : *m_indexDescriptors) {
                m_hasEnumerableProperty |= descriptor.isEnumerable();
            }
        }
    }
    if (indexPropertyStorageSize() > 8 && !m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        m_indexPropertyMap = new IndexPropertyMapWithCache(*m_indexProperties);
    }
}

ObjectStructureWithIndexProperties::ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                                                       ObjectStructureItemVector* symbolProperties,
                                                                       ObjectStructureIndexPropertyVector* indexProperties,
                                                                       const InlineIndexProperties& inlineIndexProperties,
                                                                       ObjectStructureIndexDescriptorVector* indexDescriptors,
                                                                       Optional<PropertyNameMapWithCache*> namedMap,
                                                                       Optional<IndexPropertyMapWithCache*> indexMap)
    : ObjectStructure(indexProperties ? !indexProperties->empty() : inlineIndexProperties.m_size != 0, false, false, false)
    , m_namedProperties(namedProperties)
    , m_symbolProperties(symbolProperties)
    , m_inlineIndexProperties(inlineIndexProperties)
    , m_indexProperties(indexProperties)
    , m_indexDescriptors(indexDescriptors)
    , m_namedPropertyMap(namedMap)
    , m_indexPropertyMap(indexMap)
{
    finishConstruction();
}

ObjectStructureWithIndexProperties::ObjectStructureWithIndexProperties(ObjectStructureItemVector* namedProperties,
                                                                       ObjectStructureItemVector* symbolProperties,
                                                                       ObjectStructureIndexPropertyVector* indexProperties,
                                                                       const InlineIndexProperties& inlineIndexProperties,
                                                                       ObjectStructureIndexDescriptorVector* indexDescriptors,
                                                                       Optional<PropertyNameMapWithCache*> namedMap,
                                                                       Optional<IndexPropertyMapWithCache*> indexMap,
                                                                       bool hasNonAtomicPropertyName,
                                                                       bool hasEnumerableProperty)
    : ObjectStructure(indexProperties ? !indexProperties->empty() : inlineIndexProperties.m_size != 0,
                      !symbolProperties->empty(), hasNonAtomicPropertyName, hasEnumerableProperty)
    , m_namedProperties(namedProperties)
    , m_symbolProperties(symbolProperties)
    , m_inlineIndexProperties(inlineIndexProperties)
    , m_indexProperties(indexProperties)
    , m_indexDescriptors(indexDescriptors)
    , m_namedPropertyMap(namedMap)
    , m_indexPropertyMap(indexMap)
{
    if (indexPropertyStorageSize() > 8 && !m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        m_indexPropertyMap = new IndexPropertyMapWithCache(*m_indexProperties);
    }
}

void* ObjectStructureWithIndexProperties::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectStructureWithIndexProperties)] = { 0 };
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_namedProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_symbolProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexProperties));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexDescriptors));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_namedPropertyMap));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectStructureWithIndexProperties, m_indexPropertyMap));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectStructureWithIndexProperties));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findProperty(const ObjectStructurePropertyName& name)
{
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return findIndexProperty(index);
    }
    return findNonIndexProperty(name);
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findNonIndexProperty(const ObjectStructurePropertyName& name)
{
    if (name.isSymbol()) {
        for (size_t i = 0; i < m_symbolProperties->size(); i++) {
            if ((*m_symbolProperties)[i].m_propertyName == name) {
                return std::make_pair(m_namedProperties->size() + i, &(*m_symbolProperties)[i].m_descriptor);
            }
        }
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }

    size_t namedIndex = SIZE_MAX;
    if (m_namedProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
        if (!m_namedPropertyMap) {
            m_namedPropertyMap = new PropertyNameMapWithCache(*m_namedProperties);
        }
        namedIndex = m_namedPropertyMap->find(name, *m_namedProperties);
    } else {
        for (size_t i = 0; i < m_namedProperties->size(); i++) {
            if ((*m_namedProperties)[i].m_propertyName == name) {
                namedIndex = i;
                break;
            }
        }
    }
    if (namedIndex == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(namedIndex, &(*m_namedProperties)[namedIndex].m_descriptor);
}

ObjectStructureFindResult ObjectStructureWithIndexProperties::findIndexProperty(uint32_t index)
{
    size_t ordinal = SIZE_MAX;
    if (m_indexPropertyMap) {
        ASSERT(m_indexProperties);
        ordinal = m_indexPropertyMap->find(index, *m_indexProperties);
    } else if (m_indexProperties) {
        auto it = std::lower_bound(m_indexProperties->begin(), m_indexProperties->end(), index);
        if (it != m_indexProperties->end() && *it == index) {
            ordinal = static_cast<size_t>(it - m_indexProperties->begin());
        }
    } else {
        auto* begin = m_inlineIndexProperties.m_keys;
        auto* end = begin + m_inlineIndexProperties.m_size;
        auto* it = std::lower_bound(begin, end, index);
        if (it != end && *it == index) {
            ordinal = static_cast<size_t>(it - begin);
        }
    }
    if (ordinal == SIZE_MAX) {
        return std::make_pair(SIZE_MAX, Optional<const ObjectStructurePropertyDescriptor*>());
    }
    return std::make_pair(namedPropertyCount() + ordinal,
                          m_indexDescriptors ? &(*m_indexDescriptors)[ordinal] : &defaultIndexPropertyDescriptor());
}

const ObjectStructurePropertyDescriptor& ObjectStructureWithIndexProperties::propertyDescriptor(size_t valueIndex) const
{
    if (valueIndex < m_namedProperties->size()) {
        return (*m_namedProperties)[valueIndex].m_descriptor;
    }
    valueIndex -= m_namedProperties->size();
    if (valueIndex < m_symbolProperties->size()) {
        return (*m_symbolProperties)[valueIndex].m_descriptor;
    }
    size_t indexOrdinal = valueIndex - m_symbolProperties->size();
    return m_indexDescriptors ? (*m_indexDescriptors)[indexOrdinal] : defaultIndexPropertyDescriptor();
}

bool ObjectStructureWithIndexProperties::isIndexProperty(size_t valueIndex) const
{
    return valueIndex >= namedPropertyCount();
}

uint32_t ObjectStructureWithIndexProperties::indexPropertyName(size_t valueIndex) const
{
    ASSERT(isIndexProperty(valueIndex));
    return indexPropertyAt(valueIndex - namedPropertyCount());
}

const ObjectStructurePropertyName& ObjectStructureWithIndexProperties::nonIndexPropertyName(size_t valueIndex) const
{
    ASSERT(!isIndexProperty(valueIndex));
    if (valueIndex < m_namedProperties->size()) {
        return (*m_namedProperties)[valueIndex].m_propertyName;
    }
    return (*m_symbolProperties)[valueIndex - m_namedProperties->size()].m_propertyName;
}

size_t ObjectStructureWithIndexProperties::propertyCount() const
{
    return namedPropertyCount() + indexPropertyStorageSize();
}

size_t ObjectStructureWithIndexProperties::namedPropertyCount() const
{
    return m_namedProperties->size() + m_symbolProperties->size();
}

void ObjectStructureWithIndexProperties::fillIndexPropertyOrdinalsInOrder(uint32_t* ordinals) const
{
    if (m_indexPropertyMap) {
        m_indexPropertyMap->fillOrdinalsInOrder(ordinals);
        return;
    }
    ObjectStructure::fillIndexPropertyOrdinalsInOrder(ordinals);
}

ObjectStructure* ObjectStructureWithIndexProperties::addProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    uint32_t index = name.tryToUseAsIndexProperty();
    if (index != Value::InvalidIndexPropertyValue) {
        return addIndexProperty(index, desc);
    }
    return addNonIndexProperty(name, desc);
}

ObjectStructure* ObjectStructureWithIndexProperties::addNonIndexProperty(const ObjectStructurePropertyName& name, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    ObjectStructureIndexPropertyVector* indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    ObjectStructureIndexDescriptorVector* indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;

    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties);
        symbolProperties = copyProperties(m_symbolProperties);
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors) : nullptr;
    } else {
        namedProperties = m_namedProperties;
        symbolProperties = m_symbolProperties;
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }

    if (name.isSymbol()) {
        if (symbolProperties == emptyProperties()) {
            symbolProperties = new ObjectStructureItemVector();
        }
        symbolProperties->push_back(ObjectStructureItem(name, desc));
    } else {
        if (namedProperties == emptyProperties()) {
            namedProperties = new ObjectStructureItemVector();
        }
        namedProperties->push_back(ObjectStructureItem(name, desc));
        if (namedMap) {
            namedMap->insert(*namedProperties);
        }
    }
    bool hasNonAtomicPropertyName = m_hasNonAtomicPropertyName || name.hasNonAtomicString();
    bool hasEnumerableProperty = m_hasEnumerableProperty || desc.isEnumerable();
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  hasNonAtomicPropertyName, hasEnumerableProperty);
}

ObjectStructure* ObjectStructureWithIndexProperties::addIndexProperty(uint32_t index, const ObjectStructurePropertyDescriptor& desc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    ObjectStructureIndexPropertyVector* indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    ObjectStructureIndexDescriptorVector* indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;

    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties);
        symbolProperties = copyProperties(m_symbolProperties);
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors) : nullptr;
    } else {
        namedProperties = m_namedProperties;
        symbolProperties = m_symbolProperties;
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }

    size_t previousIndexCount = indexProperties ? indexProperties->size() : inlineIndexProperties.m_size;
    if (!indexDescriptors && !isDefaultIndexPropertyDescriptor(desc)) {
        indexDescriptors = new ObjectStructureIndexDescriptorVector();
        indexDescriptors->reserve(indexProperties ? indexProperties->capacity() : InlineIndexProperties::Capacity);
        for (size_t i = 0; i < previousIndexCount; i++) {
            indexDescriptors->push_back(defaultIndexPropertyDescriptor());
        }
    }
    if (!indexProperties && previousIndexCount < InlineIndexProperties::Capacity) {
        uint32_t* begin = inlineIndexProperties.m_keys;
        size_t insertionIndex = static_cast<size_t>(std::lower_bound(begin, begin + previousIndexCount, index) - begin);
        for (size_t i = previousIndexCount; i > insertionIndex; i--) {
            inlineIndexProperties.m_keys[i] = inlineIndexProperties.m_keys[i - 1];
        }
        inlineIndexProperties.m_keys[insertionIndex] = index;
        inlineIndexProperties.m_size++;
        if (indexDescriptors) {
            indexDescriptors->insert(insertionIndex, desc);
        }
    } else {
        if (!indexProperties) {
            ASSERT(previousIndexCount == InlineIndexProperties::Capacity);
            indexProperties = new ObjectStructureIndexPropertyVector();
            indexProperties->reserve(8);
            indexProperties->push_back(inlineIndexProperties.m_keys[0]);
            indexProperties->push_back(inlineIndexProperties.m_keys[1]);
            inlineIndexProperties.m_size = 0;
        }
        if (indexMap || previousIndexCount >= 8) {
            // Once an order index exists, value slots and descriptor metadata
            // stay in append order even if deletes later take the live count
            // below the hash threshold. Only compact ordinals are reordered.
            indexProperties->push_back(index);
            if (indexDescriptors) {
                indexDescriptors->push_back(desc);
            }
            if (indexMap) {
                indexMap->insert(*indexProperties);
            }
        } else {
            size_t insertionIndex = static_cast<size_t>(std::lower_bound(indexProperties->begin(), indexProperties->end(), index)
                                                        - indexProperties->begin());
            indexProperties->insert(insertionIndex, index);
            if (indexDescriptors) {
                indexDescriptors->insert(insertionIndex, desc);
            }
        }
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  m_hasNonAtomicPropertyName, m_hasEnumerableProperty || desc.isEnumerable());
}

ObjectStructure* ObjectStructureWithIndexProperties::removeProperty(size_t valueIndex)
{
    auto* namedProperties = copyProperties(m_namedProperties);
    auto* symbolProperties = copyProperties(m_symbolProperties);
    auto* indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties) : nullptr;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    auto* indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors) : nullptr;
    if (valueIndex < m_namedProperties->size()) {
        auto* filtered = new ObjectStructureItemVector();
        filtered->reserve(m_namedProperties->size() - 1);
        for (size_t i = 0; i < m_namedProperties->size(); i++) {
            if (i != valueIndex) {
                filtered->push_back((*m_namedProperties)[i]);
            }
        }
        namedProperties = filtered;
    } else if (valueIndex < namedPropertyCount()) {
        size_t removedSymbolOrdinal = valueIndex - m_namedProperties->size();
        auto* filtered = new ObjectStructureItemVector();
        filtered->reserve(m_symbolProperties->size() - 1);
        for (size_t i = 0; i < m_symbolProperties->size(); i++) {
            if (i != removedSymbolOrdinal) {
                filtered->push_back((*m_symbolProperties)[i]);
            }
        }
        symbolProperties = filtered;
    } else {
        size_t removedIndexOrdinal = valueIndex - namedPropertyCount();
        size_t oldIndexCount = indexPropertyStorageSize();
        size_t newIndexCount = oldIndexCount - 1;
        if (!m_indexPropertyMap && newIndexCount <= InlineIndexProperties::Capacity) {
            indexProperties = nullptr;
            inlineIndexProperties.m_size = 0;
            for (size_t i = 0; i < oldIndexCount; i++) {
                if (i != removedIndexOrdinal) {
                    inlineIndexProperties.m_keys[inlineIndexProperties.m_size++] = indexPropertyAt(i);
                }
            }
        } else {
            auto* filtered = new ObjectStructureIndexPropertyVector();
            filtered->reserve(newIndexCount);
            for (size_t i = 0; i < oldIndexCount; i++) {
                if (i != removedIndexOrdinal) {
                    filtered->push_back(indexPropertyAt(i));
                }
            }
            indexProperties = filtered;
            inlineIndexProperties.m_size = 0;
        }
        if (m_indexDescriptors) {
            auto* filteredDescriptors = new ObjectStructureIndexDescriptorVector();
            filteredDescriptors->reserve(m_indexDescriptors->size() - 1);
            bool allDefault = true;
            for (size_t i = 0; i < m_indexDescriptors->size(); i++) {
                if (i != removedIndexOrdinal) {
                    const auto& descriptor = (*m_indexDescriptors)[i];
                    filteredDescriptors->push_back(descriptor);
                    allDefault &= isDefaultIndexPropertyDescriptor(descriptor);
                }
            }
            indexDescriptors = allDefault ? nullptr : filteredDescriptors;
        }
    }

    size_t remainingIndexCount = indexProperties ? indexProperties->size() : inlineIndexProperties.m_size;
    if (!remainingIndexCount && symbolProperties->empty()) {
        if (namedProperties == emptyProperties()) {
            namedProperties = new ObjectStructureItemVector();
        }
        namedProperties->reserve(namedProperties->size() + symbolProperties->size());
        for (const auto& item : *symbolProperties) {
            namedProperties->push_back(item);
        }
        bool hasSymbol = false;
        bool hasNonAtomic = false;
        bool hasEnumerable = false;
        for (const auto& item : *namedProperties) {
            hasSymbol |= item.m_propertyName.isSymbol();
            hasNonAtomic |= item.m_propertyName.hasNonAtomicString();
            hasEnumerable |= item.m_descriptor.isEnumerable();
        }
        if (namedProperties->size() > ESCARGOT_OBJECT_STRUCTURE_ACCESS_CACHE_BUILD_MIN_SIZE) {
            return new ObjectStructureWithMap(namedProperties, nullptr, false, hasSymbol, hasEnumerable);
        }
        return new ObjectStructureWithoutTransition(namedProperties, false, hasSymbol, hasNonAtomic, hasEnumerable);
    }
    Optional<IndexPropertyMapWithCache*> indexMap;
    if (m_indexPropertyMap && remainingIndexCount) {
        ASSERT(indexProperties);
        indexMap = new IndexPropertyMapWithCache(*indexProperties);
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, nullptr, indexMap);
}

ObjectStructure* ObjectStructureWithIndexProperties::replacePropertyDescriptor(size_t valueIndex, const ObjectStructurePropertyDescriptor& newDesc)
{
    ObjectStructureItemVector* namedProperties;
    ObjectStructureItemVector* symbolProperties;
    ObjectStructureIndexPropertyVector* indexProperties;
    InlineIndexProperties inlineIndexProperties = m_inlineIndexProperties;
    ObjectStructureIndexDescriptorVector* indexDescriptors;
    Optional<PropertyNameMapWithCache*> namedMap;
    Optional<IndexPropertyMapWithCache*> indexMap;
    if (m_isReferencedByInlineCache) {
        namedProperties = copyProperties(m_namedProperties);
        symbolProperties = copyProperties(m_symbolProperties);
        indexProperties = m_indexProperties ? new ObjectStructureIndexPropertyVector(*m_indexProperties) : nullptr;
        indexDescriptors = m_indexDescriptors ? new ObjectStructureIndexDescriptorVector(*m_indexDescriptors) : nullptr;
    } else {
        namedProperties = m_namedProperties;
        symbolProperties = m_symbolProperties;
        indexProperties = m_indexProperties;
        indexDescriptors = m_indexDescriptors;
        namedMap = m_namedPropertyMap;
        indexMap = m_indexPropertyMap;
        m_namedProperties = nullptr;
        m_symbolProperties = nullptr;
        m_indexProperties = nullptr;
        m_indexDescriptors = nullptr;
        m_namedPropertyMap = nullptr;
        m_indexPropertyMap = nullptr;
    }
    if (valueIndex < namedProperties->size()) {
        (*namedProperties)[valueIndex].m_descriptor = newDesc;
    } else if (valueIndex < namedProperties->size() + symbolProperties->size()) {
        (*symbolProperties)[valueIndex - namedProperties->size()].m_descriptor = newDesc;
    } else {
        size_t indexOrdinal = valueIndex - namedProperties->size() - symbolProperties->size();
        if (!indexDescriptors && !isDefaultIndexPropertyDescriptor(newDesc)) {
            indexDescriptors = new ObjectStructureIndexDescriptorVector();
            indexDescriptors->resize(indexProperties ? indexProperties->size() : inlineIndexProperties.m_size, defaultIndexPropertyDescriptor());
        }
        if (indexDescriptors) {
            (*indexDescriptors)[indexOrdinal] = newDesc;
            if (isDefaultIndexPropertyDescriptor(newDesc)) {
                bool allDefault = true;
                for (const auto& descriptor : *indexDescriptors) {
                    allDefault &= isDefaultIndexPropertyDescriptor(descriptor);
                }
                if (allDefault) {
                    indexDescriptors = nullptr;
                }
            }
        }
    }
    return new ObjectStructureWithIndexProperties(namedProperties, symbolProperties, indexProperties, inlineIndexProperties, indexDescriptors, namedMap, indexMap,
                                                  m_hasNonAtomicPropertyName, m_hasEnumerableProperty || newDesc.isEnumerable());
}
} // namespace Escargot
