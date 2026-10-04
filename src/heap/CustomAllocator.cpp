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
#include "runtime/EnvironmentRecord.h"

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

// Disclaim kinds trace only reachable objects. Eager sweeping is enabled separately
// through GC_new_kind_enumerable(), so native resources are still released in the
// collection that kills their owner without tracing dead object graphs.
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

int getValidValueInNonSharedBackingStore(void* ptr, GC_mark_pair* arr)
{
    NonSharedBackingStore* current = (NonSharedBackingStore*)ptr;
    const bool isMarked = isMarkedHeapObject(current);
    arr[0].from = (GC_word*)&current->m_observerItems;
    arr[0].to = isMarked ? (GC_word*)current->m_observerItems.data() : nullptr;
    // Deleter data is retained only while the store is reachable. The embedder
    // must keep GC-allocated callback data alive until the deleter runs.
    arr[1].from = (GC_word*)&current->m_deleterData;
    arr[1].to = (isMarked && !current->m_isResizable) ? (GC_word*)current->m_deleterData : nullptr;
    return 0;
}

#if defined(ENABLE_THREADING)
int getValidValueInSharedBackingStore(void* ptr, GC_mark_pair* arr)
{
    SharedBackingStore* current = (SharedBackingStore*)ptr;
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
static GC_ms_entry* markEncodedSmallValueRange(const char* start, const char* end,
                                               GC_ms_entry* mark_stack_ptr,
                                               GC_ms_entry* mark_stack_limit)
{
    constexpr size_t batchSize = 32;
    GC_mark_pair_32bit buffer[batchSize];
    int count = 0;

    char* ptr = (char*)start;
    char* limit = (char*)end;

    for (; ptr < limit; ptr += 4) {
        EncodedSmallValue* current = (EncodedSmallValue*)ptr;
        const uint32_t offset = current->compressedPayload();
        if (offset > ValueLast && ((offset & 1) == 0)) {
            buffer[count].from = reinterpret_cast<const unsigned int*>(ptr);
            buffer[count].offset = offset;
            count++;
            if (count == batchSize) {
                mark_stack_ptr = GC_mark_and_push_32bit(mark_stack_ptr, mark_stack_limit,
                                                        buffer, batchSize);
                count = 0;
            }
        }
    }

    if (count > 0) {
        mark_stack_ptr = GC_mark_and_push_32bit(mark_stack_ptr, mark_stack_limit,
                                                buffer, count);
    }

    return mark_stack_ptr;
}

GC_ms_entry* markEncodedSmallValueVector(GC_word* addr,
                                         GC_ms_entry* mark_stack_ptr,
                                         GC_ms_entry* mark_stack_limit,
                                         GC_word env)
{
#if defined(GC_DEBUG)
    const char* start = (const char*)GC_USR_PTR_FROM_BASE(addr);
#else
    const char* start = (const char*)addr;
#endif
    return markEncodedSmallValueRange(start, (const char*)addr + GC_size(addr),
                                      mark_stack_ptr, mark_stack_limit);
}

GC_ms_entry* markFunctionEnvironmentRecord(GC_word* addr,
                                           GC_ms_entry* mark_stack_ptr,
                                           GC_ms_entry* mark_stack_limit,
                                           GC_word env)
{
#if defined(GC_DEBUG)
    auto current = reinterpret_cast<FunctionEnvironmentRecord*>(GC_USR_PTR_FROM_BASE(addr));
#else
    auto current = reinterpret_cast<FunctionEnvironmentRecord*>(addr);
#endif
    // Mark procedures may also visit cleared free-list objects, whose first
    // word is a free-list link rather than a vtable. The function field is
    // cleared in that case, so do not inspect the record's layout further.
    Optional<ScriptFunctionObject*> functionOrArguments = current->m_functionObject;
    if (!functionOrArguments) {
        return mark_stack_ptr;
    }
    GC_mark_pair functionPointer = { reinterpret_cast<GC_word*>(&current->m_functionObject),
                                     reinterpret_cast<GC_word*>(functionOrArguments.value()) };
    mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit, &functionPointer, 1);

    const size_t count = current->indexedHeapStorageCount();
    const char* storage = reinterpret_cast<const char*>(current) + FunctionEnvironmentRecord::indexedHeapStorageOffset();
    const char* allocationEnd = reinterpret_cast<const char*>(addr) + GC_size(addr);
    // Debug frees poison the payload until reclamation. Do not interpret a
    // poisoned binding count as a range outside this allocation.
    if (count > static_cast<size_t>(allocationEnd - storage) / sizeof(EncodedSmallValue)) {
        return mark_stack_ptr;
    }
    const char* storageEnd = storage + count * sizeof(EncodedSmallValue);
    mark_stack_ptr = markEncodedSmallValueRange(storage, storageEnd, mark_stack_ptr, mark_stack_limit);

    // The this/new.target piece contains native-width pointer payloads and
    // starts at a pointer-aligned offset after the compressed binding array.
    const uintptr_t tail = (reinterpret_cast<uintptr_t>(storageEnd) + sizeof(GC_word) - 1) & ~(sizeof(GC_word) - 1);
    const uintptr_t end = reinterpret_cast<uintptr_t>(allocationEnd);
    for (uintptr_t ptr = tail; ptr + sizeof(GC_word) <= end; ptr += sizeof(GC_word)) {
        GC_mark_pair pointer = { reinterpret_cast<GC_word*>(ptr), reinterpret_cast<GC_word*>(*reinterpret_cast<GC_word*>(ptr)) };
        mark_stack_ptr = GC_mark_and_push_ptrs(mark_stack_ptr, mark_stack_limit, &pointer, 1);
    }
    return mark_stack_ptr;
}
#endif

template <typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    return (v < lo) ? lo : (hi < v) ? hi
                                    : v;
}

static ByteCodeBlock* byteCodeBlockToTrace(InterpretedCodeBlock* codeBlock)
{
    ByteCodeBlock* block = codeBlock->byteCodeBlock();
    if (block && codeBlock->parent() && ThreadLocal::pruningCompiledByteCodesVMCount() > 0 && codeBlock->context()->vmInstance()->isPruningCompiledByteCodes()) {
        if (!codeBlock->context()->vmInstance()->inIdleMode()) {
            size_t bytecodeSize = block->currentCodeSize();
            uint32_t pruningCount = codeBlock->pruningCount();
            uint32_t survivalCount = codeBlock->survivalCount();
            size_t currentEpoch = ThreadLocal::gcEpoch();
            size_t age = currentEpoch - block->m_lastUsedGcEpoch;
            size_t longLiveAge = 128;
            size_t allowedAge;

            if (pruningCount >= 1 || survivalCount >= 5) {
                if (bytecodeSize < 4096) {
                    allowedAge = longLiveAge;
                } else {
                    size_t largeBonus = pruningCount * 16 + survivalCount * 2;
                    size_t largePenalty = bytecodeSize / 1024;
                    allowedAge = (largeBonus > largePenalty) ? std::max((size_t)3, largeBonus - largePenalty) : 3;
                }
            } else {
                size_t baseAge = SCRIPT_FUNCTION_OBJECT_BYTECODE_PRUNING_AGE;
                size_t survivalBonus = survivalCount * 2;
                size_t sizePenalty = bytecodeSize / 2048;
                size_t totalBase = baseAge + survivalBonus;

                allowedAge = (totalBase > sizePenalty) ? (totalBase - sizePenalty) : 1;
            }

            allowedAge = clamp(allowedAge, (size_t)1, longLiveAge);

            if (age <= allowedAge) {
                if (age == 0) {
                    codeBlock->incrementSurvivalCount();
                }
                return block;
            }
        }
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
    arr[8].from = (GC_word*)&current->m_src;
    arr[8].to = (GC_word*)current->m_src.source.unwrap();
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
    arr[9].from = (GC_word*)&current->m_src;
    arr[9].to = (GC_word*)current->m_src.source.unwrap();
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

    // mark_from_all requires nonzero low bits in the first word of every live
    // object. ByteCodeBlock flags and BackingStore vtables do not satisfy that
    // contract: incremental rescanning would skip their new references. Use
    // normal marked-object rescanning and request eager sweeping independently.
    s_gcKinds[HeapObjectKind::ByteCodeBlockKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                          GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInByteCodeBlock, 3>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::ByteCodeBlockKind], ByteCodeBlock::clearByteCodeBlockFromDisclaimGC, 0);

    s_gcKinds[HeapObjectKind::NonSharedBackingStoreKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                                  GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInNonSharedBackingStore, 2>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::NonSharedBackingStoreKind], NonSharedBackingStore::clearNonSharedBackingStore, 0);

#if defined(ENABLE_THREADING)
    s_gcKinds[HeapObjectKind::SharedBackingStoreKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                               GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInSharedBackingStore, 1>), 0), FALSE, TRUE);
    GC_register_disclaim_proc(s_gcKinds[HeapObjectKind::SharedBackingStoreKind], SharedBackingStore::clearSharedBackingStore, 0);
#endif

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
    s_gcKinds[HeapObjectKind::EncodedSmallValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                                         GC_MAKE_PROC(GC_new_proc(markEncodedSmallValueVector), 0),
                                                                         FALSE,
                                                                         TRUE);
    s_gcKinds[HeapObjectKind::FunctionEnvironmentRecordKind] = GC_new_kind(GC_new_free_list(),
                                                                           GC_MAKE_PROC(GC_new_proc(markFunctionEnvironmentRecord), 0),
                                                                           FALSE,
                                                                           TRUE);
#endif

    // m_src is marked through the single bit of its leading ScriptSource pointer
    static_assert(offsetof(SourceRange, source) == 0, "");
    static_assert(sizeof(Optional<ScriptSource*>) == sizeof(size_t), "");

    s_interpreCodeBlockProcDescriptor[0] = GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlock, 9>), 0);
    {
        // add + 1 for headerwords w/debug mode
        GC_word objBitmap[GC_BITMAP_SIZE(InterpretedCodeBlock) + 1] = { 0 };
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_context));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_script));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlock, m_src));
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

    s_interpreCodeBlockProcDescriptor[1] = GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlockWithRareData, 10>), 0);
    {
        // add + 1 for headerwords w/debug mode
        GC_word objBitmap[GC_BITMAP_SIZE(InterpretedCodeBlockWithRareData) + 1] = { 0 };
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_context));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_script));
        GC_set_bit(objBitmap, headerWords + GC_WORD_OFFSET(InterpretedCodeBlockWithRareData, m_src));
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
#ifdef GC_DEBUG
            data->callback(data->state, GC_USR_PTR_FROM_BASE(obj));
#else
            data->callback(data->state, obj);
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
void* allocateFunctionEnvironmentRecord(size_t size)
{
    return GC_GENERIC_MALLOC(size, s_gcKinds[HeapObjectKind::FunctionEnvironmentRecordKind]);
}

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
