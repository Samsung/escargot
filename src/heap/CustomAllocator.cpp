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
#include "runtime/BackingStore.h"
#include "runtime/WeakRefObject.h"
#include "runtime/WeakMapObject.h"
#include "runtime/FinalizationRegistryObject.h"
#include "parser/CodeBlock.h"
#include "interpreter/ByteCode.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"

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

// NOTE
// a block that is not marked when this runs traces nothing at all.
//
// this kind is registered with mark-unconditionally (see initializeCustomAllocators),
// so the collector runs this procedure for every block that has not been reclaimed yet,
// blocks that are already garbage included -- GC_push_unconditionally pushes them
// without setting their mark bit, so that the disclaim callback of a dying block can
// still look at its referents. a block that is genuinely reachable, on the other hand,
// is always marked before it is pushed (PUSH_CONTENTS sets the mark bit and only then
// pushes, and that holds for the conservative stack scan too, which is what keeps a
// block alive during a pruning cycle). so the mark bit is exactly the "is this block
// still in use" test here, and a live block still traces all three fields.
//
// tracing anything from a dying block is what turns a dead island into a permanent
// leak, because every referent below can lead back to this very block:
//   - m_codeBlock is the owner, and the owner traces m_byteCodeBlock right back.
//   - m_stringLiteralData holds source literals, which are usually StringViews of the
//     script source; the underlying source string may be a CompressibleString or a
//     ReloadableString, and both of those keep a VMInstance* -- from there the mark
//     phase reaches every Context, Script and InterpretedCodeBlock, and from the owning
//     CodeBlock this block again.
//   - m_otherLiteralData holds BigInts, ObjectStructures and inline cache data, none of
//     which reaches a VMInstance today, but the same rule is applied: a garbage block
//     pushes nothing.
// once such a loop closes, the dying block ends up marked and survives the collection,
// and the next collection pushes it unconditionally again and repeats the whole thing.
// the block, its owner and the entire VMInstance island hanging off it are then alive
// for the rest of the process -- dropping a Context or a Script leaks its whole object
// graph instead of collecting it.
//
// as a consequence the disclaim callback must treat all of this as weak references, see
// ByteCodeBlock::clearByteCodeBlock(): it only uses the cached VMInstance back pointer
// and the non-GC buffers, and re-checks the owner before touching it.
int getValidValueInByteCodeBlock(void* ptr, GC_mark_custom_result* arr)
{
    ByteCodeBlock* current = (ByteCodeBlock*)ptr;
    arr[0].from = (GC_word*)&current->m_stringLiteralData;
    arr[1].from = (GC_word*)&current->m_otherLiteralData;
    arr[2].from = (GC_word*)&current->m_codeBlock;
    if (isMarkedHeapObject(current)) {
        arr[0].to = (GC_word*)current->m_stringLiteralData.data();
        arr[1].to = (GC_word*)current->m_otherLiteralData.data();
        arr[2].to = (GC_word*)current->m_codeBlock;
    } else {
        arr[0].to = arr[1].to = arr[2].to = nullptr;
    }
    return 0;
}

// NOTE
// the observer list holds back references: every entry points at the ArrayBuffer (or
// ArrayBufferView) that registered itself to be notified when the buffer address moves,
// and those objects point at this BackingStore again through ArrayBuffer::m_backingStore.
//
// like ByteCodeBlockKind above, the backing store kinds are registered with
// mark-unconditionally, so this procedure also runs for stores that are already garbage.
// tracing the observer list there resurrects the observing ArrayBuffer, which marks this
// store right back, and the pair -- plus the observer's prototype chain, its realm's
// GlobalObject, Context and VMInstance, i.e. the whole heap -- can never be collected.
// so the list is traced only while the store itself is reachable, and the same rule is
// applied to every other field below: a store that is only visited by the unconditional
// push is garbage and pushes nothing at all.
//
// the observer entries are already registered as disappearing links, so a live store
// whose observer died gets its entry cleared instead of dangling.
int getValidValueInNonSharedBackingStore(void* ptr, GC_mark_custom_result* arr)
{
    NonSharedBackingStore* current = (NonSharedBackingStore*)ptr;
    const bool isMarked = isMarkedHeapObject(current);
    arr[0].from = (GC_word*)&current->m_observerItems;
    arr[0].to = isMarked ? (GC_word*)current->m_observerItems.data() : nullptr;
    // m_deleterData is gated the same way, even though the disclaim callback does hand it
    // to the deleter (see clearNonSharedBackingStore()): a dying store traces nothing.
    //
    // keeping it alive from here is exactly what mark-unconditionally is for, but the cost
    // is unbounded. deleter data that leads back to this store -- the owning
    // ArrayBufferObject through m_backingStore, or a Context/VMInstance that reaches it --
    // makes the dying store mark itself, so it survives the collection, is pushed
    // unconditionally again in the next one, and neither it nor the island behind it is
    // ever collected. that is a permanent leak the engine has no way to detect, and here a
    // leak is worse than a crash: keeping GC allocated deleter data alive is the embedder's
    // job instead, see the note on BackingStoreRef::createNonSharedBackingStore().
    //
    // the common case does not change at all: deleter data that is not GC allocated (e.g.
    // the plain new/delete struct the N-API external ArrayBuffer support passes) is not a
    // heap address, so it was never retained by this push either. for a resizable store the
    // union holds m_maxByteLength and the deleter is passed nullptr, so nothing is traced.
    //
    // note that the mark-unconditionally registration still earns its keep even though
    // nothing is traced from a dead store: it also puts this kind into the eager sweep that
    // runs before marking (GC_reclaim_unconditionally_marked()), which is what gets the
    // deleter called -- and the native buffer released -- in the collection that kills the
    // store, rather than whenever the heap block is next needed for allocation.
    arr[1].from = (GC_word*)&current->m_deleterData;
    arr[1].to = (isMarked && !current->m_isResizable) ? (GC_word*)current->m_deleterData : nullptr;
    return 0;
}

#if defined(ENABLE_THREADING)
int getValidValueInSharedBackingStore(void* ptr, GC_mark_custom_result* arr)
{
    SharedBackingStore* current = (SharedBackingStore*)ptr;
    // same reasoning as getValidValueInNonSharedBackingStore() above
    arr[0].from = (GC_word*)&current->m_observerItems;
    arr[0].to = isMarkedHeapObject(current) ? (GC_word*)current->m_observerItems.data() : nullptr;
    return 0;
}
#endif

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

static ByteCodeBlock* byteCodeBlockToTrace(InterpretedCodeBlock* codeBlock)
{
    ByteCodeBlock* block = codeBlock->byteCodeBlock();
    if (block && codeBlock->parent() && codeBlock->context()->vmInstance()->isPruningCompiledByteCodes()) {
        // a bytecode pruning cycle is in progress: do not trace the ByteCodeBlock reference
        // so that blocks reachable only through it are collected at the end of this cycle.
        // blocks in use are kept alive by the conservative stack scan, and dying blocks
        // disconnect themselves from their CodeBlock in the disclaim callback.
        // ByteCodeBlocks of top-level CodeBlocks are preserved (managed by Script)
        return nullptr;
    }
    return block;
}

int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr)
{
    InterpretedCodeBlock* current = (InterpretedCodeBlock*)ptr;
    arr[0].from = (GC_word*)&current->m_context;
    arr[0].to = (GC_word*)current->m_context;
    arr[1].from = (GC_word*)&current->m_script;
    arr[1].to = (GC_word*)current->m_script;
    arr[2].from = (GC_word*)&current->m_byteCodeBlock;
    arr[2].to = (GC_word*)byteCodeBlockToTrace(current);
    arr[3].from = (GC_word*)&current->m_parent;
    arr[3].to = (GC_word*)current->m_parent;
    arr[4].from = (GC_word*)&current->m_children;
    arr[4].to = (GC_word*)current->m_children;
    arr[5].from = (GC_word*)&current->m_parameterNames;
    arr[5].to = (GC_word*)current->m_parameterNames.data();
    arr[6].from = (GC_word*)&current->m_identifierInfos;
    arr[6].to = (GC_word*)current->m_identifierInfos.data();
    arr[7].from = (GC_word*)&current->m_blockInfos;
    arr[7].to = (GC_word*)current->m_blockInfos;
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
    arr[2].to = (GC_word*)byteCodeBlockToTrace(current);
    arr[3].from = (GC_word*)&current->m_parent;
    arr[3].to = (GC_word*)current->m_parent;
    arr[4].from = (GC_word*)&current->m_children;
    arr[4].to = (GC_word*)current->m_children;
    arr[5].from = (GC_word*)&current->m_parameterNames;
    arr[5].to = (GC_word*)current->m_parameterNames.data();
    arr[6].from = (GC_word*)&current->m_identifierInfos;
    arr[6].to = (GC_word*)current->m_identifierInfos.data();
    arr[7].from = (GC_word*)&current->m_blockInfos;
    arr[7].to = (GC_word*)current->m_blockInfos;
    arr[8].from = (GC_word*)&current->m_rareData;
    arr[8].to = (GC_word*)current->m_rareData;
    return 0;
}

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
    arr[4].from = (GC_word*)&current->m_observerItems;
    arr[4].to = (GC_word*)current->m_observerItems.data();

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
    arr[1].from = (GC_word*)&current->source;
    arr[1].to = (GC_word*)current->source;
    return 0;
}

int getValidValueInWeakMapObjectDataItem(void* ptr, GC_mark_custom_result* arr)
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

    s_gcKinds[HeapObjectKind::ByteCodeBlockKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                          GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInByteCodeBlock, 3>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::ByteCodeBlockKind], ByteCodeBlock::clearByteCodeBlockFromDisclaimGC, 1);

    s_gcKinds[HeapObjectKind::NonSharedBackingStoreKind] = GC_new_kind(GC_new_free_list(),
                                                                       GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInNonSharedBackingStore, 2>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::NonSharedBackingStoreKind], NonSharedBackingStore::clearNonSharedBackingStore, 1);

#if defined(ENABLE_THREADING)
    s_gcKinds[HeapObjectKind::SharedBackingStoreKind] = GC_new_kind(GC_new_free_list(),
                                                                    GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInSharedBackingStore, 1>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::SharedBackingStoreKind], SharedBackingStore::clearSharedBackingStore, 1);
#endif

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
    s_gcKinds[HeapObjectKind::EncodedSmallValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                                         GC_MAKE_PROC(GC_new_proc(markAndPushCustomIterable<getNextValidInEncodedSmallValueVector>), 0),
                                                                         FALSE,
                                                                         TRUE);
#endif

    s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind] = GC_new_kind(GC_new_free_list(),
                                                                      GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlock, 8>), 0),
                                                                      FALSE,
                                                                      TRUE);

    s_gcKinds[HeapObjectKind::InterpretedCodeBlockWithRareDataKind] = GC_new_kind(GC_new_free_list(),
                                                                                  GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlockWithRareData, 9>), 0),
                                                                                  FALSE,
                                                                                  TRUE);

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
                                                                              GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInArrayBufferObject, 5>), 0),
                                                                              FALSE,
                                                                              TRUE);

    s_gcKinds[HeapObjectKind::WeakRefObjectKind] = GC_new_kind(GC_new_free_list(),
                                                               GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInWeakRefObject, 3>), 0),
                                                               FALSE,
                                                               TRUE);

    s_gcKinds[HeapObjectKind::FinalizationRegistryObjectItemKind] = GC_new_kind(GC_new_free_list(),
                                                                                GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInFinalizationRegistryObjectItem, 2>), 0),
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
    GC_enumerate_reachable_objects_inner([](void* obj, size_t bytes, void* cd) {
        size_t size;
        int kind = GC_get_kind_and_size(obj, &size);
        ASSERT(size == bytes);

        HeapObjectIteratorData* data = (HeapObjectIteratorData*)cd;
        if (kind == data->kind) {
#if defined(NDEBUG)
            data->callback(data->state, obj);
#else
            data->callback(data->state, GC_USR_PTR_FROM_BASE(obj));
#endif
        }
    },
                                         (void*)(&data));
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
ByteCodeBlock* CustomAllocator<ByteCodeBlock>::allocate(size_type GC_n, const void*)
{
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::ByteCodeBlockKind];
    return (ByteCodeBlock*)GC_GENERIC_MALLOC(sizeof(ByteCodeBlock), kind);
}

template <>
NonSharedBackingStore* CustomAllocator<NonSharedBackingStore>::allocate(size_type GC_n, const void*)
{
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::NonSharedBackingStoreKind];
    return (NonSharedBackingStore*)GC_GENERIC_MALLOC(sizeof(NonSharedBackingStore), kind);
}

#if defined(ENABLE_THREADING)
template <>
SharedBackingStore* CustomAllocator<SharedBackingStore>::allocate(size_type GC_n, const void*)
{
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::SharedBackingStoreKind];
    return (SharedBackingStore*)GC_GENERIC_MALLOC(sizeof(SharedBackingStore), kind);
}
#endif

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

template <>
WeakMapObject::WeakMapObjectDataItem* CustomAllocator<WeakMapObject::WeakMapObjectDataItem>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (WeakMapObject::WeakMapObjectDataItem*)GC_MALLOC(sizeof(WeakMapObject::WeakMapObjectDataItem));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::WeakMapObjectDataItemKind];
    return (WeakMapObject::WeakMapObjectDataItem*)GC_GENERIC_MALLOC(sizeof(WeakMapObject::WeakMapObjectDataItem), kind);
}
#endif

} // namespace Escargot
