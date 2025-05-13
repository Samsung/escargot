/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "Symbol.h"
#include "Value.h"
#include "VMInstance.h"

namespace Escargot {

String* Symbol::descriptionString() const
{
    return m_description ? const_cast<String*>(m_description.value()) : String::emptyString();
}

Value Symbol::descriptionValue() const
{
    return m_description ? m_description.value() : Value();
}

Symbol* Symbol::fromGlobalSymbolRegistry(VMInstance* vm, String* stringKey)
{
    ASSERT(!!stringKey);
    // Let stringKey be ? ToString(key).
    // For each element e of the GlobalSymbolRegistry List,
    auto& list = vm->globalSymbolRegistry();
    for (size_t i = 0; i < list.size(); i++) {
        // If SameValue(e.[[Key]], stringKey) is true, return e.[[Symbol]].
        if (list[i].key->equals(stringKey)) {
            return list[i].symbol;
        }
    }
    // Assert: GlobalSymbolRegistry does not currently contain an entry for stringKey.
    // Let newSymbol be a new unique Symbol value whose [[Description]] value is stringKey.
    Symbol* newSymbol = new Symbol(stringKey);
    // Append the Record { [[Key]]: stringKey, [[Symbol]]: newSymbol } to the GlobalSymbolRegistry List.
    GlobalSymbolRegistryItem item;
    item.key = stringKey;
    item.symbol = newSymbol;
    list.pushBack(item);
    // Return newSymbol.
    return newSymbol;
}

Value Symbol::keyForSymbol(VMInstance* vm, Symbol* sym)
{
    ASSERT(!!sym);
    auto& list = vm->globalSymbolRegistry();

    for (size_t i = 0; i < list.size(); i++) {
        // If SameValue(e.[[Symbol]], sym) is true, return e.[[Key]].
        if (list[i].symbol == sym) {
            ASSERT(!!list[i].key);
            return list[i].key;
        }
    }
    return Value();
}

String* Symbol::symbolDescriptiveString() const
{
    StringBuilder sb;
    sb.appendString("Symbol(");
    sb.appendString(descriptionString());
    sb.appendString(")");
    return sb.finalize();
}

SymbolFinalizerData* Symbol::ensureFinalizerData()
{
    if (!m_finalizerData) {
        m_finalizerData = new SymbolFinalizerData();

        // register finalizers
#define FINALIZER_CALLBACK()                                         \
    Symbol* self = (Symbol*)sym;                                     \
    auto d = self->finalizerData();                                  \
    for (size_t i = 0; i < d->m_finalizer.size(); i++) {             \
        if (LIKELY(!!d->m_finalizer[i].first)) {                     \
            d->m_finalizer[i].first(self, d->m_finalizer[i].second); \
        }                                                            \
    }                                                                \
    d->m_finalizer.clear();

#ifndef NDEBUG
        GC_finalization_proc of = nullptr;
        void* od = nullptr;
        GC_REGISTER_FINALIZER_NO_ORDER(
            this, [](void* sym, void*) {
                FINALIZER_CALLBACK()
            },
            nullptr, &of, &od);
        ASSERT(!of);
        ASSERT(!od);
#else
        GC_REGISTER_FINALIZER_NO_ORDER(
            this, [](void* sym, void*) {
                FINALIZER_CALLBACK()
            },
            nullptr, nullptr, nullptr);
#endif
    }

    return m_finalizerData;
}

void Symbol::addFinalizer(FinalizerFunction fn, void* data)
{
    auto finData = ensureFinalizerData();
    if (UNLIKELY(finData->m_removedFinalizerCount)) {
        for (size_t i = 0; i < finData->m_finalizer.size(); i++) {
            if (finData->m_finalizer[i].first == nullptr) {
                ASSERT(finData->m_finalizer[i].second == nullptr);
                finData->m_finalizer[i].first = fn;
                finData->m_finalizer[i].second = data;
                finData->m_removedFinalizerCount--;
                return;
            }
        }
        ASSERT_NOT_REACHED();
    }

    finData->m_finalizer.pushBack(std::make_pair(fn, data));
}

bool Symbol::removeFinalizer(FinalizerFunction fn, void* data)
{
    auto finData = finalizerData();
    for (size_t i = 0; i < finData->m_finalizer.size(); i++) {
        if (finData->m_finalizer[i].first == fn && finData->m_finalizer[i].second == data) {
            finData->m_finalizer[i].first = nullptr;
            finData->m_finalizer[i].second = nullptr;
            finData->m_removedFinalizerCount++;

            tryToShrinkFinalizers();
            return true;
        }
    }
    return false;
}

void Symbol::tryToShrinkFinalizers()
{
    auto finData = finalizerData();
    auto& oldFinalizer = finData->m_finalizer;
    size_t oldSize = oldFinalizer.size();
    if (finData->m_removedFinalizerCount > ((oldSize / 2) + 1)) {
        ASSERT(finData->m_removedFinalizerCount <= oldSize);
        size_t newSize = oldSize - finData->m_removedFinalizerCount;
        TightVector<std::pair<FinalizerFunction, void*>, GCUtil::gc_malloc_atomic_allocator<std::pair<FinalizerFunction, void*>>> newFinalizer;
        newFinalizer.resizeWithUninitializedValues(newSize);

        size_t j = 0;
        for (size_t i = 0; i < oldSize; i++) {
            if (oldFinalizer[i].first) {
                newFinalizer[j].first = oldFinalizer[i].first;
                newFinalizer[j].second = oldFinalizer[i].second;
                j++;
            }
        }
        ASSERT(j == newSize);

        finData->m_finalizer = std::move(newFinalizer);
        finData->m_removedFinalizerCount = 0;
    }
}

} // namespace Escargot
