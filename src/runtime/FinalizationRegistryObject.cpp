/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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
#include "FinalizationRegistryObject.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/Job.h"
#include "runtime/JobQueue.h"
#include "runtime/VMInstance.h"
#include "runtime/PromiseObject.h"
#include "interpreter/ByteCode.h"
#include "parser/CodeBlock.h"
#include "util/Util.h"

namespace Escargot {

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, Object* cleanupCallback, Context* realm)
    : FinalizationRegistryObject(state, state.context()->globalObject()->finalizationRegistryPrototype(), cleanupCallback, realm)
{
}

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, Object* proto, Object* cleanupCallback, Context* realm)
    : DerivedObject(state, proto)
    , m_cleanupCallback(cleanupCallback)
    , m_realm(realm)
    , m_deletedCellCount(0)
{
    ASSERT(!!cleanupCallback && cleanupCallback->isCallable());

    addFinalizer([](PointerValue* self, void* data) {
        FinalizationRegistryObject* s = self->asFinalizationRegistryObject();
        for (size_t i = 0; i < s->m_cells.size(); i++) {
            auto cell = s->m_cells[i];
            if (cell->weakRefTarget) {
                cell->weakRefTarget->removeFinalizer(finalizer, cell);
                if (cell->unregisterToken.hasValue() && cell->unregisterToken.value() != cell->weakRefTarget) {
                    cell->unregisterToken->removeFinalizer(finalizerUnregisterToken, cell);
                }
            }
        }
        s->m_cells.clear();
        s->m_deletedCellCount = 0;
    },
                 nullptr);
}

void* FinalizationRegistryObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(FinalizationRegistryObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_cleanupCallback));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_realm));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_cells));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(FinalizationRegistryObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void FinalizationRegistryObject::setCell(PointerValue* weakRefTarget, const Value& heldValue, Optional<PointerValue*> unregisterToken)
{
    ASSERT(!!weakRefTarget);
    ASSERT(weakRefTarget->isObject() || weakRefTarget->isSymbol());
    FinalizationRegistryObjectItem* newCell = nullptr;

    if (m_deletedCellCount) {
        for (size_t i = 0; i < m_cells.size(); i++) {
            if (!m_cells[i]->weakRefTarget) {
                newCell = m_cells[i];
                m_deletedCellCount--;
                break;
            }
        }
    } else {
        newCell = new FinalizationRegistryObjectItem();
        m_cells.pushBack(newCell);
    }

    ASSERT(!!newCell);
    newCell->weakRefTarget = weakRefTarget;
    newCell->heldValue = heldValue;
    newCell->source = this;
    newCell->unregisterToken = unregisterToken;

    weakRefTarget->addFinalizer(finalizer, newCell);
    if (unregisterToken.hasValue() && unregisterToken.value() != weakRefTarget) {
        ASSERT(unregisterToken->isObject() || unregisterToken->isSymbol());
        unregisterToken->addFinalizer(finalizerUnregisterToken, newCell);
    }
}

bool FinalizationRegistryObject::deleteCell(PointerValue* unregisterToken)
{
    ASSERT(!!unregisterToken);
    ASSERT(unregisterToken->isObject() || unregisterToken->isSymbol());
    bool removed = false;
    for (size_t i = 0; i < m_cells.size(); i++) {
        if (m_cells[i]->weakRefTarget && m_cells[i]->unregisterToken.hasValue() && m_cells[i]->unregisterToken.value() == unregisterToken) {
            ASSERT(!!m_cells[i]->weakRefTarget);
            m_cells[i]->weakRefTarget->removeFinalizer(finalizer, m_cells[i]);
            if (unregisterToken != m_cells[i]->weakRefTarget) {
                unregisterToken->removeFinalizer(finalizerUnregisterToken, m_cells[i]);
            }
            m_cells[i]->reset();
            m_deletedCellCount++;

            removed = true;
        }
    }

    if (removed) {
        tryToShrinkCells();
    }

    return removed;
}

bool FinalizationRegistryObject::deleteCellOnly(FinalizationRegistryObjectItem* item)
{
    // delete cell only without removing finalizer in weakRefTarget
    // but remove finalizer of unregisterToken if it has
    ASSERT(!!item);
    for (size_t i = 0; i < m_cells.size(); i++) {
        if (m_cells[i] == item) {
            ASSERT(!!m_cells[i]->weakRefTarget);
            if (m_cells[i]->unregisterToken.hasValue() && m_cells[i]->unregisterToken.value() != m_cells[i]->weakRefTarget) {
                m_cells[i]->unregisterToken->removeFinalizer(finalizerUnregisterToken, m_cells[i]);
            }
            m_cells[i]->reset();
            m_deletedCellCount++;

            tryToShrinkCells();
            return true;
        }
    }
    return false;
}

void FinalizationRegistryObject::cleanupSome(ExecutionState& state, Optional<Object*> callback)
{
    state.context()->vmInstance()->enqueueJob(new CleanupSomeJob(state.context(), this, callback));
}

void* FinalizationRegistryObject::FinalizationRegistryObjectItem::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(FinalizationRegistryObject::FinalizationRegistryObjectItem)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject::FinalizationRegistryObjectItem, source));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject::FinalizationRegistryObjectItem, heldValue));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(FinalizationRegistryObject::FinalizationRegistryObjectItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<FinalizationRegistryObjectItem>().allocate(1);
#endif
}

void FinalizationRegistryObject::finalizer(PointerValue* self, void* data)
{
    UNUSED_PARAMETER(self);
    FinalizationRegistryObjectItem* item = (FinalizationRegistryObjectItem*)data;

    ASSERT(!!item->source && !!item->source->m_cleanupCallback);
    bool wasCallbackDeleted = false;
    auto callback = item->source->m_cleanupCallback;
    if (callback->isScriptFunctionObject()) {
        auto cb = callback->asScriptFunctionObject()->interpretedCodeBlock();
        if (cb->byteCodeBlock() && cb->byteCodeBlock()->m_code.data() == nullptr) {
            // NOTE this case means this function was deleted by gc(same time with this FinalizationRegistryObject)
            // we should check before calling because finalizer is not ordered
            wasCallbackDeleted = true;
        }
    }

    if (!wasCallbackDeleted) {
        try {
            ExecutionState tempState(item->source->m_realm);
            Value argv = item->heldValue;
            Object::call(tempState, callback, Value(), 1, &argv);
        } catch (const Value& v) {
            // do nothing
        }
    }

    // remove item from FinalizationRegistryObject
    bool deleteResult = item->source->deleteCellOnly(item);
    ASSERT(deleteResult);
}

void FinalizationRegistryObject::finalizerUnregisterToken(PointerValue* self, void* data)
{
    UNUSED_PARAMETER(self);
    FinalizationRegistryObjectItem* item = (FinalizationRegistryObjectItem*)data;

    // item should be valid and have a valid unregisterToken
    ASSERT(!!item->weakRefTarget);
    ASSERT(item->unregisterToken.hasValue() && item->unregisterToken.value() == self);
    item->unregisterToken.reset();
}

void FinalizationRegistryObject::tryToShrinkCells()
{
    size_t oldSize = m_cells.size();
    if (m_deletedCellCount > ((oldSize / 2) + 1)) {
        ASSERT(m_deletedCellCount <= oldSize);
        size_t newSize = oldSize - m_deletedCellCount;
        FinalizationRegistryObjectCells newCells;
        newCells.resizeFitWithUninitializedValues(newSize);

        size_t j = 0;
        for (size_t i = 0; i < oldSize; i++) {
            if (m_cells[i]->weakRefTarget) {
                newCells[j++] = m_cells[i];
            }
        }
        ASSERT(j == newSize);

        m_cells = std::move(newCells);
        m_deletedCellCount = 0;
    }
}

} // namespace Escargot
