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

#ifndef __EscargotHeap__
#define __EscargotHeap__
#include "GCUtil.h"
#include "util/Optional.h"

namespace Escargot {

class Heap {
public:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    enum class CompressedType {
        ObjectStructureWithMap,
        ObjectStructureWithIndexProperties,
        Symbol,
        IteratorRecord,
        ObjectStructureWithTransition,
        ObjectStructureWithTransitionAndSymbols,
        ObjectStructureWithTransitionWithMap,
        ObjectStructureTransitionStorage,
        ObjectStructureRetiredBuffer,
        GetObjectInlineCacheSimpleCaseData,
        Object,
        SymbolObject,
        BigIntObject,
        BooleanObject,
        NumberObject,
        StringObject,
        StringIteratorObject,
        ArrayIteratorObject,
        MapIteratorObject,
        SetIteratorObject,
        ProxyObject,
        RegExpStringIteratorObject,
        WeakRefObject,
        DateObject,
        ArrayBufferView,
        ArrayBufferObject,
        SharedArrayBufferObject,
        IteratorHelperObject,
        MapObject,
        SetObject,
        WeakMapObject,
        WeakSetObject,
        FinalizationRegistryObject,
        GeneratorObject,
        AsyncGeneratorObject,
        PromiseObject,
        ModulePromiseObject,
        GlobalObject,
        FunctionObject,
        NativeFunctionObject,
        ScriptFunctionObject,
        ExtendedNativeFunctionObject,
        ScriptArrowFunctionObject,
        ScriptAsyncFunctionObject,
        ScriptAsyncGeneratorFunctionObject,
        ScriptGeneratorFunctionObject,
        ScriptClassMethodFunctionObject,
        ErrorObject,
        BoundFunctionObject,
        ArgumentsObject,
        TemporalDurationObject,
        TemporalInstantObject,
        TemporalPlainTimeObject,
        AsyncFromSyncIteratorObject,
        GlobalObjectProxyObject,
        ShadowRealmObject,
        WrapForValidIteratorObject,
        WrappedFunctionObject,
        ScriptClassConstructorPrototypeObject,
        DisposableStackObject,
        AsyncDisposableStackObject,
        ModuleNamespaceObject,
        ScriptClassConstructorFunctionObject,
        RegExpObject,
        TemporalPlainDateObject,
        TemporalPlainDateTimeObject,
        TemporalZonedDateTimeObject,
        ScriptAsyncFunctionHelperFunctionObject,
        ScriptAsyncFromSyncIteratorHelperFunctionObject,
        ScriptAsyncFromSyncIteratorCloseOnRejectFunctionObject,
        ObjectWithPropertyHandler,
        ExposableObject,
        GenericIteratorObject,
        IntlLocaleObject,
        IntlPluralRulesObject,
        IntlDisplayNamesObject,
        IntlRelativeTimeFormatObject,
        IntlListFormatObject,
        IntlSegmenterObject,
        IntlSegmentsObject,
        IntlSegmentsIteratorObject,
        IntlDateTimeFormatObject,
        IntlDurationFormatObject,
        WASMModuleObject,
        WASMInstanceObject,
        WASMMemoryObject,
        WASMTableObject,
        WASMGlobalObject,
        ExportedFunctionObject,
        Count
    };
    static bool isCompressedTypeInitialized(CompressedType type)
    {
        return !!s_compressedDescriptors[static_cast<size_t>(type)];
    }
    static void initializeCompressedType(CompressedType type, size_t size,
                                         const GC_word* bitmap, size_t slots);
    static void* mallocCompressed(CompressedType type, size_t size)
    {
        auto& descriptor = s_compressedDescriptors[static_cast<size_t>(type)];
        ASSERT(descriptor);
#if defined(GC_DEBUG)
        Optional<void*> object = GC_GENERIC_MALLOC(size, GC_compressed_bitmap_descriptor_kind(descriptor.value()));
#else
        Optional<void*> object = GC_malloc_explicitly_typed_compressed(size, descriptor.value());
#endif
        ASSERT(object);
        return object.value();
    }
    static void* mallocCompressedFinalized(CompressedType type, size_t size,
                                           GC_finalization_proc finalizer)
    {
        void* object = mallocCompressed(type, size);
        GC_REGISTER_FINALIZER_NO_ORDER(object, finalizer, nullptr, nullptr, nullptr);
        return object;
    }

private:
    static MAY_THREAD_LOCAL Optional<const GC_compressed_bitmap_descr*> s_compressedDescriptors[static_cast<size_t>(CompressedType::Count)];

public:
#endif
    static void initialize();
    static void finalize();
    static void printGCHeapUsage();
};
} // namespace Escargot

#include "CustomAllocator.h"

#endif
