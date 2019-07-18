/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "parser/CodeBlock.h"

namespace Escargot {

static int s_gcKinds[HeapObjectKind::NumberOfKind];

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
    arr[3].to = (GC_word*)current->m_fastModeData.data();
    return 0;
}

GC_word* getNextValidInValueVector(GC_word* ptr, GC_word** next_ptr)
{
    Value* current = (Value*)ptr;
#ifdef ESCARGOT_32
    *next_ptr = ptr + 2;
#else
    *next_ptr = ptr + 1;
#endif
    GC_word* ret = NULL;
    if (*current && current->isPointerValue()) {
        ret = (GC_word*)current->asPointerValue();
    }
    return ret;
}

int getValidValueInCodeBlock(void* ptr, GC_mark_custom_result* arr)
{
    CodeBlock* current = (CodeBlock*)ptr;
    arr[0].from = (GC_word*)&current->m_context;
    arr[0].to = (GC_word*)current->m_context;
    arr[1].from = (GC_word*)&current->m_byteCodeBlock;
    arr[1].to = (GC_word*)current->m_byteCodeBlock;
    return 0;
}

int getValidValueInInterpretedCodeBlock(void* ptr, GC_mark_custom_result* arr)
{
    InterpretedCodeBlock* current = (InterpretedCodeBlock*)ptr;
    arr[0].from = (GC_word*)&current->m_context;
    arr[0].to = (GC_word*)current->m_context;
    arr[1].from = (GC_word*)&current->m_script;
    arr[1].to = (GC_word*)current->m_script;
    arr[2].from = (GC_word*)&current->m_identifierInfos;
    arr[2].to = (GC_word*)current->m_identifierInfos.data();
    arr[3].from = (GC_word*)&current->m_parametersInfomation;
    arr[3].to = (GC_word*)current->m_parametersInfomation.data();
    arr[4].from = (GC_word*)&current->m_parentCodeBlock;
    arr[4].to = (GC_word*)current->m_parentCodeBlock;
    arr[5].from = (GC_word*)&current->m_childBlocks;
    arr[5].to = (GC_word*)current->m_childBlocks.data();
    arr[6].from = (GC_word*)&current->m_byteCodeBlock;
    arr[6].to = (GC_word*)current->m_byteCodeBlock;
    arr[7].from = (GC_word*)&current->m_blockInfos;
    arr[7].to = (GC_word*)current->m_blockInfos.data();
#ifdef NDEBUG
    arr[8].from = (GC_word*)&current->m_context;
    arr[8].to = (GC_word*)nullptr;
#else
    arr[8].from = (GC_word*)&current->m_scopeContext;
    arr[8].to = (GC_word*)current->m_scopeContext;
#endif
    return 0;
}


void initializeCustomAllocators()
{
    s_gcKinds[HeapObjectKind::ValueVectorKind] = GC_new_kind(GC_new_free_list(),
                                                             GC_MAKE_PROC(GC_new_proc(markAndPushCustomIterable<getNextValidInValueVector>), 0),
                                                             FALSE,
                                                             TRUE);
#ifdef NDEBUG
    GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayObject)] = { 0 };
    GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayObject, m_structure));
    GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayObject, m_prototype));
    GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayObject, m_values));
    GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayObject, m_fastModeData));
    auto descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayObject));

    s_gcKinds[HeapObjectKind::ArrayObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                        descr,
                                                                        FALSE,
                                                                        TRUE);
#else
    s_gcKinds[HeapObjectKind::ArrayObjectKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                        GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInArrayObject, 4>), 0),
                                                                        FALSE,
                                                                        TRUE);
#endif
    s_gcKinds[HeapObjectKind::CodeBlockKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                      GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInCodeBlock, 2>), 0),
                                                                      FALSE,
                                                                      TRUE);

    s_gcKinds[HeapObjectKind::InterpretedCodeBlockKind] = GC_new_kind_enumerable(GC_new_free_list(),
                                                                                 GC_MAKE_PROC(GC_new_proc(markAndPushCustom<getValidValueInInterpretedCodeBlock, 9>), 0),
                                                                                 FALSE,
                                                                                 TRUE);
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
    // return (Value*)GC_MALLOC_IGNORE_OFF_PAGE(sizeof(Value) * GC_n);
    int kind = s_gcKinds[HeapObjectKind::ValueVectorKind];
    size_t size = sizeof(Value) * GC_n;

    Value* ret;
    if (size > 1024) {
        ret = (Value*)GC_GENERIC_MALLOC_IGNORE_OFF_PAGE(size, kind);
    } else {
        ret = (Value*)GC_GENERIC_MALLOC(size, kind);
    }
    return ret;
}

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
CodeBlock* CustomAllocator<CodeBlock>::allocate(size_type GC_n, const void*)
{
    // Un-comment this to use default allocator
    // return (CodeBlock*)GC_MALLOC(sizeof(CodeBlock));
    ASSERT(GC_n == 1);
    int kind = s_gcKinds[HeapObjectKind::CodeBlockKind];
    return (CodeBlock*)GC_GENERIC_MALLOC(sizeof(CodeBlock), kind);
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
} // namespace Escargot
