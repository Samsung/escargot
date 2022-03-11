/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "Allocator.h"
#include "CustomAllocator.h"

#include "runtime/Value.h"
#include "runtime/ArrayObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/WeakRefObject.h"
#include "runtime/WeakMapObject.h"
#include "runtime/FinalizationRegistryObject.h"
#include "parser/CodeBlock.h"
#include "interpreter/ByteCode.h"

namespace Escargot {

static MAY_THREAD_LOCAL int s_gcKinds[HeapObjectKind::NumberOfKind];

template <GC_get_next_pointer_proc proc>
GC_ms_entry* markAndPushCustomIterable(GC_word* addr,
                                       struct GC_ms_entry* mark_stack_ptr,
                                       struct GC_ms_entry* mark_stack_limit,
                                       GC_word env)
{
    return GC_mark_and_push_custom_iterable(addr, mark_stack_ptr, mark_stack_limit, proc);
}

template <GC_get_sub_pointer_proc proc, const int number_of_sub_pointer>
GC_ms_entry* markAndPushCustom(GC_word* addr,
                               struct GC_ms_entry* mark_stack_ptr,
                               struct GC_ms_entry* mark_stack_limit,
                               GC_word env)
{
    GC_mark_custom_result subPtrs[number_of_sub_pointer];
    return GC_mark_and_push_custom(addr, mark_stack_ptr, mark_stack_limit, proc, subPtrs, number_of_sub_pointer);
}

void getNextValidInValueVector(GC_word* ptr, GC_word* end, GC_word** next_ptr, GC_word** from, GC_word** to)
{
    while (ptr < end) {
        Value* current = (Value*)ptr;
        if (current->isPointerValue()) {
#ifdef ESCARGOT_32
            *next_ptr = ptr + 2;
#else
            *next_ptr = ptr + 1;
#endif
            *from = ptr;
            *to = (GC_word*)current->asPointerValue();
            return;
        }
#ifdef ESCARGOT_32
        ptr = ptr + 2;
#else
        ptr = ptr + 1;
#endif
    }

    *next_ptr = end;
    *from = NULL;
    *to = NULL;
}

void getNextValidInGetObjectInlineCacheDataVector(GC_word* ptr, GC_word* end, GC_word** next_ptr, GC_word** from, GC_word** to)
{
    GetObjectInlineCacheData* current = (GetObjectInlineCacheData*)ptr;
    *next_ptr = (GC_word*)((size_t)ptr + sizeof(GetObjectInlineCacheData));
    *from = (GC_word*)&current->m_cachedhiddenClassChain;
    *to = (GC_word*)current->m_cachedhiddenClassChain;
}

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
void getNextValidInEncodedSmallValueVector(GC_word* ptr, GC_word* end, GC_word** next_ptr, GC_word** from, GC_word** to)
{
    while (ptr < end) {
        EncodedSmallValue* current = (EncodedSmallValue*)ptr;
        const auto& payload = current->payload();
        if (payload > ValueLast && ((payload & 1) == 0)) {
            *next_ptr = (GC_word*)((size_t)ptr + 4);
            *from = ptr;
            *to = reinterpret_cast<GC_word*>(payload);
            return;
        }

        ptr = (GC_word*)((size_t)ptr + 4);
    }

    *next_ptr = end;
    *from = NULL;
    *to = NULL;
}
#endif

#if !defined(NDEBUG)
int getValidValueInArrayObject(void* ptr, GC_mark_custom_result* arr)
{
    ArrayObject* current = (ArrayObject*)ptr;
    arr[0].from = (GC_word*)&current->m_structure;
    arr[0].to = (GC_word*)current->m_structure;
    arr[1].from = (GC_word*)&current->m_prototype;
    arr[1].to = (GC_word*)current->m_prototype;
    arr[2].from = (GC_word*)&current->m_values;
    arr[2].to = (GC_word*)current->m_values.data();
    arr[3].from = (GC_word*)&current->m_fastModeData;
#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
    arr[3].to = (GC_word*)current->m_fastModeData.data();
#else
    arr[3].to = (GC_word*)current->m_fastModeData;
#endif
    return 0;
}

int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr)
{
    ArrayBufferObject* current = (ArrayBufferObject*)ptr;
    arr[0].from = (GC_word*)&current->m_structure;
    arr[0].to = (GC_word*)current->m_structure;
    arr[1].from = (GC_word*)&current->m_prototype;
    arr[1].to = (GC_word*)current->m_prototype;
    arr[2].from = (GC_word*)&current->m_values;
    arr[2].to = (GC_word*)current->m_values.data();
    arr[3].from = (GC_word*)&current->m_backingStore;
    arr[3].to = (GC_word*)(current->m_backingStore.hasValue() ? current->m_backingStore.value() : nullptr);
    return 0;
}

int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr)
{
    InterpretedCodeBlock* current = (InterpretedCodeBlock*)ptr;
    arr[0].from = (GC_word*)&current->m_context;
    arr[0].to = (GC_word*)current->m_context;
    arr[1].from = (GC_word*)&current->m_script;
    arr[1].to = (GC_word*)current->m_script;
    arr[2].from = (GC_word*)&current->m_byteCodeBlock;
    arr[2].to = (GC_word*)current->m_byteCodeBlock;
    arr[3].from = (GC_word*)&current->m_parent;
    arr[3].to = (GC_word*)current->m_parent;
    arr[4].from = (GC_word*)&current->m_children;
    arr[4].to = (GC_word*)current->m_children;
    arr[5].from = (GC_word*)&current->m_parameterNames;
    arr[5].to = (GC_word*)current->m_parameterNames.data();
    arr[6].from = (GC_word*)&current->m_identifierInfos;
    arr[6].to = (GC_word*)current->m_identifierInfos.data();
    arr[7].from = (GC_word*)&current->m_blockInfos;
    arr[7].to = (GC_word*)current->m_blockInfos.data();
    return 0;
}

int getValidValueInInterpretedCodeBlockWithRareData(void* ptr, GC_mark_custom_result* arr)
{
    InterpretedCodeBlockWithRareData* current = (InterpretedCodeBlockWithRareData*)ptr;
    arr[0].from = (GC_word*)&current->m_context;
    arr[0].to = (GC_word*)current->m_context;
    arr[1].from = (GC_word*)&current->m_script;
    arr[1].to = (GC_word*)current->m_script;
    arr[2].from = (GC_word*)&current->m_byteCodeBlock;
    arr[2].to = (GC_word*)current->m_byteCodeBlock;
    arr[3].from = (GC_word*)&current->m_parent;
    arr[3].to = (GC_word*)current->m_parent;
    arr[4].from = (GC_word*)&current->m_children;
    arr[4].to = (GC_word*)current->m_children;
    arr[5].from = (GC_word*)&current->m_parameterNames;
    arr[5].to = (GC_word*)current->m_parameterNames.data();
    arr[6].from = (GC_word*)&current->m_identifierInfos;
    arr[6].to = (GC_word*)current->m_identifierInfos.data();
    arr[7].from = (GC_word*)&current->m_blockInfos;
    arr[7].to = (GC_word*)current->m_blockInfos.data();
    arr[8].from = (GC_word*)&current->m_rareData;
    arr[8].to = (GC_word*)current->m_rareData;
    return 0;
}

int getValidValueInWeakMapObjectDataItemObject(void* ptr, GC_mark_custom_result* arr)
{
    WeakMapObject::WeakMapObjectDataItem* current = (WeakMapObject::WeakMapObjectDataItem*)ptr;
    arr[0].from = (GC_word*)&current->data;
    if (current->data.isStoredInHeap()) {
        arr[0].to = (GC_word*)current->data.payload();
    } else {
        arr[0].to = nullptr;
    }
    return 0;
}

int getValidValueInWeakRefObject(void* ptr, GC_mark_custom_result* arr)
{
    WeakRefObject* current = (WeakRefObject*)ptr;
    arr[0].from = (GC_word*)&current->m_structure;
    arr[0].to = (GC_word*)current->m_structure;
    arr[1].from = (GC_word*)&current->m_prototype;
    arr[1].to = (GC_word*)current->m_prototype;
    arr[2].from = (GC_word*)&current->m_values;
    arr[2].to = (GC_word*)current->m_values.data();
    return 0;
}

int getValidValueInFinalizationRegistryObjectItem(void* ptr, GC_mark_custom_result* arr)
{
    FinalizationRegistryObject::FinalizationRegistryObjectItem* current = (FinalizationRegistryObject::FinalizationRegistryObjectItem*)ptr;
    arr[0].from = (GC_word*)&current->heldValue;
    if (current->heldValue.isStoredInHeap()) {
        arr[0].to = (GC_word*)current->heldValue.payload();
    } else {
        arr[0].to = nullptr;
    }
    arr[1].from = (GC_word*)&current->unregisterToken;
    if (current->unregisterToken.hasValue()) {
        arr[1].to = (GC_word*)current->unregisterToken.value();
    } else {
        arr[1].to = nullptr;
    }
    arr[2].from = (GC_word*)&current->source;
    arr[2].to = (GC_word*)current->source;
    return 0;
}
#endif

void initializeCustomAllocators()
{
    if (s_gcKinds[HeapObjectKind::ValueVectorKind]) {
        return;
    }

    s_gcKinds[HeapObjectKind::ValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                             GC_MAKE_PROC(GC_new_proc(markAndPushCustomIterable<getNextValidInValueVector>), 0),
                                                             FALSE,
                                                             TRUE);

    s_gcKinds[HeapObjectKind::GetObjectInlineCacheDataVectorKind] = GC_new_kind(GC_new_free_list(),
                                                                                GC_MAKE_PROC(GC_new_proc(markAndPushCustomIterable<getNextValidInGetObjectInlineCacheDataVector>), 0),
                                                                                FALSE,
                                                                                TRUE);

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
    s_gcKinds[HeapObjectKind::EncodedSmallValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                                         GC_MAKE_PROC(GC_new_proc(markAndPushCustomIterable<getNextValidInEncodedSmallValueVector>), 0),
                                                                         FALSE,
                                                                         TRUE);
#endif

#ifdef NDEBUG
    GC_word objBitmap[GC_BITMAP_SIZE(ArrayObject)] = { 0 };
    GC_set_bit(objBitmap, GC_WORD_OFFSET(ArrayObject, m_structure));
    GC_set_bit(objBitmap, GC_WORD_OFFSET(ArrayObject, m_prototype));
    GC_set_bit(objBitmap, GC_WORD_OFFSET(ArrayObject, m_values));
    GC_set_bit(objBitmap, GC_WORD_OFFSET(ArrayObject, m_fastModeData));
    auto descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ArrayObject));

    s_gcKinds[HeapObjectKind::ArrayObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                        descr,
                                                                        FALSE,
                                                                        TRUE);
#else
    s_gcKinds[HeapObjectKind::ArrayObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                        GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInArrayObject, 4>), 0),
                                                                        FALSE,
                                                                        TRUE);

    s_gcKinds[HeapObjectKind::ArrayBufferObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                              GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInArrayBufferObject, 4>), 0),
                                                                              FALSE,
                                                                              TRUE);

    s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind] = GC_new_kind(GC_new_free_list(),
                                                                      GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlock, 8>), 0),
                                                                      FALSE,
                                                                      TRUE);

    s_gcKinds[HeapObjectKind::InterpretedCodeBlockWithRareDataKind] = GC_new_kind(GC_new_free_list(),
                                                                                  GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlockWithRareData, 9>), 0),
                                                                                  FALSE,
                                                                                  TRUE);

    s_gcKinds[HeapObjectKind::WeakMapObjectDataItemKind] = GC_new_kind(GC_new_free_list(),
                                                                       GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInWeakMapObjectDataItemObject, 1>), 0),
                                                                       FALSE,
                                                                       TRUE);

    s_gcKinds[HeapObjectKind::WeakRefObjectKind] = GC_new_kind(GC_new_free_list(),
                                                               GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInWeakRefObject, 3>), 0),
                                                               FALSE,
                                                               TRUE);

    s_gcKinds[HeapObjectKind::FinalizationRegistryObjectItemKind] = GC_new_kind(GC_new_free_list(),
                                                                                GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInFinalizationRegistryObjectItem, 3>), 0),
                                                                                FALSE,
                                                                                TRUE);
#endif
}

void iterateSpecificKindOfObject(ExecutionState& state, HeapObjectKind kind, HeapObjectIteratorCallback callback)
{
    struct HeapObjectIteratorData {
        int kind;
        ExecutionState& state;
        HeapObjectIteratorCallback callback;
    };

    HeapObjectIteratorData data{ s_gcKinds[kind], state, callback };

    ASSERT(!GC_is_disabled());
    GC_gcollect(); // Update mark status. See comments of iterateSpecificKindOfObject in src/heap/Allocator.h
    GC_disable();
    GC_enumerate_reachable_objects_inner([](void* obj, size_t bytes, void* cd) {
        size_t size;
        int kind = GC_get_kind_and_size(obj, &size);
        ASSERT(size == bytes);

        HeapObjectIteratorData* data = (HeapObjectIteratorData*)cd;
        if (kind == data->kind) {
            data->callback(data->state, GC_USR_PTR_FROM_BASE(obj));
        }
    },
                                         (void*)(&data));
    GC_enable();
}

template <>
Value* CustomAllocator<Value>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (Value*)GC_MALLOC(sizeof(Value) * GC_n);
    int kind = s_gcKinds[HeapObjectKind::ValueVectorKind];
    size_t size = sizeof(Value) * GC_n;

    Value* ret;
    ret = (Value*)GC_GENERIC_MALLOC(size, kind);
    return ret;
}

template <>
GetObjectInlineCacheData* CustomAllocator<GetObjectInlineCacheData>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (Value*)GC_MALLOC(sizeof(GetObjectInlineCacheData) * GC_n);
    // typed calloc test
    /*
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GetObjectInlineCacheData)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GetObjectInlineCacheData, m_cachedhiddenClassChain));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GetObjectInlineCacheData));
        typeInited = true;
    }
    return (GetObjectInlineCacheData*)GC_CALLOC_EXPLICITLY_TYPED(GC_n, sizeof(GetObjectInlineCacheData), descr);
    */
    int kind = s_gcKinds[HeapObjectKind::GetObjectInlineCacheDataVectorKind];
    size_t size = sizeof(GetObjectInlineCacheData) * GC_n;

    GetObjectInlineCacheData* ret;
    ret = (GetObjectInlineCacheData*)GC_GENERIC_MALLOC(size, kind);
    return ret;
}

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
template <>
EncodedSmallValue* CustomAllocator<EncodedSmallValue>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (Value*)GC_MALLOC(sizeof(Value) * GC_n);
    int kind = s_gcKinds[HeapObjectKind::EncodedSmallValueVectorKind];
    size_t size = sizeof(EncodedSmallValue) * GC_n;

    EncodedSmallValue* ret;
    ret = (EncodedSmallValue*)GC_GENERIC_MALLOC(size, kind);
    return ret;
}
#endif

template <>
ArrayObject* CustomAllocator<ArrayObject>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (ArrayObject*)GC_MALLOC(sizeof(ArrayObject));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::ArrayObjectKind];
    return (ArrayObject*)GC_GENERIC_MALLOC(sizeof(ArrayObject), kind);
}

#if !defined(NDEBUG)
template <>
ArrayBufferObject* CustomAllocator<ArrayBufferObject>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (ArrayBufferObject*)GC_MALLOC(sizeof(ArrayBufferObject));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::ArrayBufferObjectKind];
    return (ArrayBufferObject*)GC_GENERIC_MALLOC(sizeof(ArrayBufferObject), kind);
}

template <>
InterpretedCodeBlock* CustomAllocator<InterpretedCodeBlock>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (InterpretedCodeBlock*)GC_MALLOC(sizeof(InterpretedCodeBlock));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind];
    return (InterpretedCodeBlock*)GC_GENERIC_MALLOC(sizeof(InterpretedCodeBlock), kind);
}

template <>
InterpretedCodeBlockWithRareData* CustomAllocator<InterpretedCodeBlockWithRareData>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (InterpretedCodeBlockWithRareData*)GC_MALLOC(sizeof(InterpretedCodeBlockWithRareData));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::InterpretedCodeBlockWithRareDataKind];
    return (InterpretedCodeBlockWithRareData*)GC_GENERIC_MALLOC(sizeof(InterpretedCodeBlockWithRareData), kind);
}

template <>
WeakMapObject::WeakMapObjectDataItem* CustomAllocator<WeakMapObject::WeakMapObjectDataItem>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (WeakRefObject*)GC_MALLOC(sizeof(WeakMapObject::WeakMapObjectDataItem));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::WeakMapObjectDataItemKind];
    return (WeakMapObject::WeakMapObjectDataItem*)GC_GENERIC_MALLOC(sizeof(WeakMapObject::WeakMapObjectDataItem), kind);
}

template <>
WeakRefObject* CustomAllocator<WeakRefObject>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (WeakRefObject*)GC_MALLOC(sizeof(WeakRefObject));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::WeakRefObjectKind];
    return (WeakRefObject*)GC_GENERIC_MALLOC(sizeof(WeakRefObject), kind);
}

template <>
FinalizationRegistryObject::FinalizationRegistryObjectItem* CustomAllocator<FinalizationRegistryObject::FinalizationRegistryObjectItem>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (FinalizationRegistryObject::FinalizationRegistryObjectItem*)GC_MALLOC(sizeof(FinalizationRegistryObject::FinalizationRegistryObjectItem));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::FinalizationRegistryObjectItemKind];
    return (FinalizationRegistryObject::FinalizationRegistryObjectItem*)GC_GENERIC_MALLOC(sizeof(FinalizationRegistryObject::FinalizationRegistryObjectItem), kind);
}
#endif

} // namespace Escargot
