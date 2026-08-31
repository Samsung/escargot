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

typedef int(GC_get_sub_pointer_proc)(void* ptr,
                                     struct GC_mark_pair* sub_ptrs);

namespace Escargot {

static MAY_THREAD_LOCAL int s_gcKinds[HeapObjectKind::NumberOfKind];
static MAY_THREAD_LOCAL GC_word s_interpreCodeBlockProcDescriptor[2];
static MAY_THREAD_LOCAL GC_word s_interpreCodeBlockTypedDescriptor[2];

GC_ms_entry* markValueVector(GC_word* addr,
                             struct GC_ms_entry* mark_stack_ptr,
                             struct GC_ms_entry* mark_stack_limit,
                             GC_word env)
{
#if defined(GC_DEBUG)
    const char* start = (const char*)GC_USR_PTR_FROM_BASE(addr);
#else
    const char* start = (const char*)addr;
#endif
    const char* end = ((char*)addr) + GC_size(addr);

    constexpr size_t batchSize = 32;
    GC_mark_pair buffer[batchSize];
    size_t count = 0;

    Value* ptr = (Value*)start;
    Value* limit = (Value*)end;

    for (; ptr < limit; ptr++) {
        if (ptr->isPointerValue()) {
            GC_word* to = (GC_word*)ptr->asPointerValue();
            buffer[count].from = (GC_word*)ptr;
            buffer[count].to = to;
            count++;
            if (count == batchSize) {
                mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                                       buffer, batchSize);
                count = 0;
            }
        }
    }

    if (count > 0) {
        mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                               buffer, count);
    }

    return mark_stack_ptr;
}

template <GC_get_sub_pointer_proc proc, const int number_of_sub_pointer>
GC_ms_entry* markAndPushCustom(GC_word* addr,
                               struct GC_ms_entry* mark_stack_ptr,
                               struct GC_ms_entry* mark_stack_limit,
                               GC_word env)
{
    GC_mark_pair subPtrs[number_of_sub_pointer];
#if defined(GC_DEBUG)
    const char* start = (const char*)GC_USR_PTR_FROM_BASE(addr);
#else
    const char* start = (const char*)addr;
#endif
    int i = proc((/* no const */ void*)start, subPtrs);
    return GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                 subPtrs + i,
                                 number_of_sub_pointer - i);
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
int getValidValueInByteCodeBlock(void* ptr, GC_mark_pair* arr)
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
int getValidValueInNonSharedBackingStore(void* ptr, GC_mark_pair* arr)
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
int getValidValueInSharedBackingStore(void* ptr, GC_mark_pair* arr)
{
    SharedBackingStore* current = (SharedBackingStore*)ptr;
    // same reasoning as getValidValueInNonSharedBackingStore() above
    arr[0].from = (GC_word*)&current->m_observerItems;
    arr[0].to = isMarkedHeapObject(current) ? (GC_word*)current->m_observerItems.data() : nullptr;
    return 0;
}
#endif

GC_ms_entry* markGetObjectInlineCacheDataVector(GC_word* addr,
                                                struct GC_ms_entry* mark_stack_ptr,
                                                struct GC_ms_entry* mark_stack_limit,
                                                GC_word env)
{
    const char* start = (const char*)addr;
    const char* end = ((char*)addr) + GC_size(addr);

    constexpr size_t batchSize = 32;
    GC_mark_pair buffer[batchSize];
    int count = 0;

    GetObjectInlineCacheData* ptr = (GetObjectInlineCacheData*)start;
    GetObjectInlineCacheData* limit = (GetObjectInlineCacheData*)end;

    for (; ptr < limit; ptr++) {
        GC_word* to = (GC_word*)ptr->m_cachedhiddenClassChain;
        buffer[count].from = (GC_word*)&ptr->m_cachedhiddenClassChain;
        buffer[count].to = to;
        count++;
        if (count == batchSize) {
            mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                                   buffer, batchSize);
            count = 0;
        }
    }

    if (count > 0) {
        mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                               buffer, count);
    }

    return mark_stack_ptr;
}

GC_ms_entry* markSetObjectInlineCacheDataVector(GC_word* addr,
                                                struct GC_ms_entry* mark_stack_ptr,
                                                struct GC_ms_entry* mark_stack_limit,
                                                GC_word env)
{
    const char* start = (const char*)addr;
    const char* end = ((char*)addr) + GC_size(addr);

    constexpr size_t batchSize = 32;
    GC_mark_pair buffer[batchSize];
    int count = 0;

    SetObjectInlineCacheData* ptr = (SetObjectInlineCacheData*)start;
    SetObjectInlineCacheData* limit = (SetObjectInlineCacheData*)end;

    for (; ptr < limit; ptr++) {
        GC_word* to = (GC_word*)ptr->m_cachedHiddenClassChainData;
        buffer[count].from = (GC_word*)&ptr->m_cachedHiddenClassChainData;
        buffer[count].to = to;
        count++;
        if (count == batchSize) {
            mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                                   buffer, batchSize);
            count = 0;
        }
    }

    if (count > 0) {
        mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                               buffer, count);
    }

    return mark_stack_ptr;
}

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
GC_ms_entry* markEncodedSmallValueVector(GC_word* addr,
                                         struct GC_ms_entry* mark_stack_ptr,
                                         struct GC_ms_entry* mark_stack_limit,
                                         GC_word env)
{
    const char* start = (const char*)addr;
    const char* end = ((char*)addr) + GC_size(addr);

    constexpr size_t batchSize = 32;
    GC_mark_pair buffer[batchSize];
    int count = 0;

    char* ptr = (char*)start;
    char* limit = (char*)end;

    for (; ptr < limit; ptr += 4) {
        EncodedSmallValue* current = (EncodedSmallValue*)ptr;
        const auto& payload = current->payload();
        if (payload > ValueLast && ((payload & 1) == 0)) {
            GC_word* to = reinterpret_cast<GC_word*>(payload);
            buffer[count].from = (GC_word*)ptr;
            buffer[count].to = to;
            count++;
            if (count == batchSize) {
                mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                                       buffer, BATCH_SIZE);
                count = 0;
            }
        }
    }

    if (count > 0) {
        mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit,
                                               buffer, count);
    }

    return mark_stack_ptr;
}
#endif

static ByteCodeBlock* byteCodeBlockToTrace(InterpretedCodeBlock* codeBlock)
{
    ByteCodeBlock* block = codeBlock->byteCodeBlock();
    if (block && codeBlock->parent() && ThreadLocal::pruningCompiledByteCodesVMCount() > 0 && codeBlock->context()->vmInstance()->isPruningCompiledByteCodes()) {
        // a bytecode pruning cycle is in progress: do not trace the ByteCodeBlock reference
        // so that blocks reachable only through it are collected at the end of this cycle.
        // blocks in use are kept alive by the conservative stack scan, and dying blocks
        // disconnect themselves from their CodeBlock in the disclaim callback.
        // ByteCodeBlocks of top-level CodeBlocks are preserved (managed by Script)
        return nullptr;
    }
    return block;
}

int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_pair* arr)
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

int getValidValueInInterpretedCodeBlockWithRareData(void* ptr, GC_mark_pair* arr)
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

void initializeCustomAllocators()
{
    if (s_gcKinds[HeapObjectKind::ValueVectorKind]) {
        return;
    }

#ifdef GC_DEBUG
    const size_t headerWords = GC_get_debug_header_size() / sizeof(GC_word);
#else
    const size_t headerWords = 0;
#endif
    s_gcKinds[HeapObjectKind::ValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                             GC_MAKE_PROC(GC_new_proc(markValueVector), 0),
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
                                                                         GC_MAKE_PROC(GC_new_proc(markEncodedSmallValueVector), 0),
                                                                         FALSE,
                                                                         TRUE);
#endif

    s_interpreCodeBlockProcDescriptor[0] = GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlock, 8>), 0);
    {
        // add + 1 for headerwords w/debug mode
        GC_word objBitmap[GC_BITMAP_SIZE(InterpretedCodeBlock) + 1] = { 0 };
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_context));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_script));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_byteCodeBlock));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_parent));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_children));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_parameterNames));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_identifierInfos));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_blockInfos));
        s_interpreCodeBlockTypedDescriptor[0] = GC_make_descriptor(objBitmap, headerWords + GC_WORD_LEN(InterpretedCodeBlock));
    }
    s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind] = GC_new_kind(GC_new_free_list(),
                                                                      s_interpreCodeBlockTypedDescriptor[0],
                                                                      FALSE,
                                                                      TRUE);

    s_interpreCodeBlockProcDescriptor[1] = GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlockWithRareData, 9>), 0);
    {
        // add + 1 for headerwords w/debug mode
        GC_word objBitmap[GC_BITMAP_SIZE(InterpretedCodeBlockWithRareData) + 1] = { 0 };
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_context));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_script));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_byteCodeBlock));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_parent));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_children));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_parameterNames));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_identifierInfos));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_blockInfos));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_rareData));
        s_interpreCodeBlockTypedDescriptor[1] = GC_make_descriptor(objBitmap, headerWords + GC_WORD_LEN(InterpretedCodeBlockWithRareData));
    }
    s_gcKinds[HeapObjectKind::InterpretedCodeBlockWithRareDataKind] = GC_new_kind(GC_new_free_list(),
                                                                                  s_interpreCodeBlockTypedDescriptor[1],
                                                                                  FALSE,
                                                                                  TRUE);
    {
        // add + 1 for headerwords w/debug mode
        GC_word objBitmap[GC_BITMAP_SIZE(ArrayObject) + 1] = { 0 };
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(ArrayObject, m_structure));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(ArrayObject, m_prototype));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(ArrayObject, m_values));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(ArrayObject, m_fastModeData));
        auto descr = GC_make_descriptor(objBitmap, headerWords + GC_WORD_LEN(ArrayObject));
        s_gcKinds[HeapObjectKind::ArrayObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                            descr,
                                                                            FALSE,
                                                                            TRUE);
    }
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

void setInterpretedCodeBlockDescriptorToProc()
{
    GC_change_kind_descriptor_inner(&s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind], &s_interpreCodeBlockProcDescriptor[0], 2);
}

void setInterpretedCodeBlockDescriptorToTyped()
{
    GC_change_kind_descriptor_inner(&s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind], &s_interpreCodeBlockTypedDescriptor[0], 2);
}

} // namespace Escargot
