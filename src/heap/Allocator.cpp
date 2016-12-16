/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "Allocator.h"

#include "runtime/Value.h"
#include "runtime/ArrayObject.h"


#ifdef PROFILE_MASSIF
std::unordered_map<void*, void*> g_addressTable;
std::vector<void*> g_freeList;

void unregisterGCAddress(void* address)
{
    // ASSERT(g_addressTable.find(address) != g_addressTable.end());
    if (g_addressTable.find(address) != g_addressTable.end()) {
        auto iter = g_addressTable.find(address);
        free(iter->second);
        g_addressTable.erase(iter);
    }
}

void registerGCAddress(void* address, size_t siz)
{
    if (g_addressTable.find(address) != g_addressTable.end()) {
        unregisterGCAddress(address);
    }
    g_addressTable[address] = malloc(siz);
}

void* GC_malloc_hook(size_t siz)
{
    void* ptr;
#ifdef NDEBUG
    ptr = GC_malloc(siz);
#else
    ptr = GC_malloc(siz);
#endif
    registerGCAddress(ptr, siz);
    return ptr;
}
void* GC_malloc_atomic_hook(size_t siz)
{
    void* ptr;
#ifdef NDEBUG
    ptr = GC_malloc_atomic(siz);
#else
    ptr = GC_malloc_atomic(siz);
#endif
    registerGCAddress(ptr, siz);
    return ptr;
}

void* GC_generic_malloc_ignore_off_page_hook(size_t siz, int kind)
{
    void* ptr;
#ifdef NDEBUG
    ptr = GC_generic_malloc(siz, kind);
#else
    ptr = GC_generic_malloc(siz, kind);
#endif
    registerGCAddress(ptr, siz);
    return ptr;
}

void* GC_malloc_ignore_off_page_hook(size_t siz)
{
    void* ptr;
#ifdef NDEBUG
    ptr = GC_malloc_ignore_off_page(siz);
#else
    ptr = GC_malloc_ignore_off_page(siz);
#endif
    registerGCAddress(ptr, siz);
    return ptr;
}

void* GC_malloc_atomic_ignore_off_page_hook(size_t siz)
{
    void* ptr;
#ifdef NDEBUG
    ptr = GC_malloc_atomic_ignore_off_page_hook(siz);
#else
    ptr = GC_malloc_atomic_ignore_off_page_hook(siz);
#endif
    registerGCAddress(ptr, siz);
    return ptr;
}

void GC_free_hook(void* address)
{
#ifdef NDEBUG
    GC_free(address);
#else
    GC_free(address);
#endif
    unregisterGCAddress(address);
}

#endif


namespace Escargot {

static int s_gcKinds[HeapObjectKind::NumberOfKind];

template<GC_get_next_pointer_proc proc>
static struct GC_ms_entry* markAndPushCustom(GC_word* addr,
                                             struct GC_ms_entry *mark_stack_ptr,
                                             struct GC_ms_entry *mark_stack_limit,
                                             GC_word env) {
    return GC_mark_and_push_custom(addr, mark_stack_ptr, mark_stack_limit, proc);
}

static GC_word* getNextValidValue(GC_word** iterator) {
    Value* current = (*(Value**)iterator)++;
    if (*current && current->isPointerValue())
        return (GC_word*)current->asPointerValue();
    else
        return NULL;
}

void initializeCustomAllocators() {
    s_gcKinds[HeapObjectKind::ValueVectorKind] =
        GC_new_kind(GC_new_free_list(),
                    GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getNextValidValue>), 0),
                    FALSE,
                    TRUE);

    s_gcKinds[HeapObjectKind::ArrayObjectKind] =
        GC_new_kind(GC_new_free_list(),
                    0 | GC_DS_LENGTH,
                    TRUE,
                    TRUE);

#ifdef PROFILE_MASSIF
    GC_is_valid_displacement_print_proc = [](void* ptr) {
        g_freeList.push_back(ptr);
    };
    GC_set_on_collection_event([](GC_EventType evtType) {
        if (GC_EVENT_PRE_START_WORLD == evtType) {
            auto iter = g_addressTable.begin();
            while (iter != g_addressTable.end()) {
                GC_is_valid_displacement(iter->first);
                iter++;
            }

            for (unsigned i = 0; i < g_freeList.size(); i++) {
                unregisterGCAddress(g_freeList[i]);
            }

            g_freeList.clear();
        }
    });
#endif
}

void iterateSpecificKindOfObject(ExecutionState& state, HeapObjectKind kind, HeapObjectIteratorCallback callback) {

    struct HeapObjectIteratorData {
        int kind;
        ExecutionState& state;
        HeapObjectIteratorCallback callback;
    };

    HeapObjectIteratorData data { s_gcKinds[kind], state, callback };

    GC_gcollect(); // Update mark status
    GC_enumerate_reachable_objects_inner([](void* obj, size_t bytes, void* cd) {
        size_t size;
        int kind = GC_get_kind_and_size(obj, &size);
        ASSERT(size == bytes);

        HeapObjectIteratorData* data = (HeapObjectIteratorData*) cd;
        if (kind == data->kind) {
            data->callback(data->state, obj);
        }
    }, (void*)(&data));
}

template <>
Value* CustomAllocator<Value>::allocate(size_type GC_n, const void*) {
    // Un-comment this to use default allocator
    // return (Value*)GC_MALLOC_IGNORE_OFF_PAGE(sizeof(Value) * GC_n);
    int kind = s_gcKinds[HeapObjectKind::ValueVectorKind];
    return (Value*)GC_GENERIC_MALLOC_IGNORE_OFF_PAGE(sizeof(Value) * GC_n, kind);
}

template <>
ArrayObject* CustomAllocator<ArrayObject>::allocate(size_type GC_n, const void*) {
    // Un-comment this to use default allocator
    // return (ArrayObject*)GC_MALLOC(sizeof(ArrayObject));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::ArrayObjectKind];
    return (ArrayObject*)GC_GENERIC_MALLOC(sizeof(ArrayObject), kind);
}

}
