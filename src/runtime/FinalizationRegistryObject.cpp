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
#include "runtime/Job.h"
#include "runtime/JobQueue.h"
#include "runtime/VMInstance.h"
#include "runtime/PromiseObject.h"
#include "util/Util.h"

namespace Escargot {

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, Object* cleanupCallback, Context* realm)
    : FinalizationRegistryObject(state, state.context()->globalObject()->finalizationRegistryPrototype(), cleanupCallback, realm)
{
}

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, Object* proto, Object* cleanupCallback, Context* realm)
    : Object(state, proto)
    , m_cleanupCallback(cleanupCallback)
    , m_realm(realm)
{
    addFinalizer([](Object* self, void* data) {
        FinalizationRegistryObject* s = self->asFinalizationRegistryObject();
        for (size_t i = 0; i < s->m_cells.size(); i++) {
            s->m_cells[i]->weakRefTarget->removeFinalizer(finalizer, s->m_cells[i]);
        }
        s->m_cells.clear();
    },
                 nullptr);
}

void* FinalizationRegistryObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
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

void FinalizationRegistryObject::setCell(ExecutionState& state, Object* weakRefTarget, const Value& heldValue, Optional<Object*> unregisterToken)
{
    auto newCell = new FinalizationRegistryObjectItem();
    newCell->weakRefTarget = weakRefTarget;
    newCell->heldValue = heldValue;
    newCell->source = this;
    newCell->unregisterToken = unregisterToken;
    m_cells.pushBack(newCell);

    weakRefTarget->addFinalizer(finalizer, newCell);
}

bool FinalizationRegistryObject::deleteCell(ExecutionState& state, Object* unregisterToken)
{
    bool removed = false;
    for (size_t i = 0; i < m_cells.size(); i++) {
        if (m_cells[i]->unregisterToken.hasValue() && m_cells[i]->unregisterToken.value() == unregisterToken) {
            m_cells[i]->weakRefTarget->removeFinalizer(finalizer, m_cells[i]);
            m_cells.erase(i);
            i--;
            removed = true;
        }
    }
    return removed;
}

class CleanupSomeJob : public Job {
public:
    CleanupSomeJob(Context* relatedContext, FinalizationRegistryObject* object, Optional<Object*> callback, PromiseObject* promise)
        : Job(relatedContext)
        , m_object(object)
        , m_callback(callback)
        , m_promise(promise)
    {
    }

    SandBox::SandBoxResult run()
    {
        auto oldCallback = m_object->m_cleanupCallback;
        m_object->m_cleanupCallback = m_callback;

        clearStack<1024>();
        GC_gcollect_and_unmap();
        GC_gcollect_and_unmap();
        GC_gcollect_and_unmap();
        m_object->m_cleanupCallback = oldCallback;

        SandBox sandbox(relatedContext());
        return sandbox.run([&]() -> Value {
            ExecutionState state(relatedContext());
            m_promise->fulfill(state, Value());
            return Value();
        });
    }

private:
    FinalizationRegistryObject* m_object;
    Optional<Object*> m_callback;
    PromiseObject* m_promise;
};

void FinalizationRegistryObject::cleanupSome(ExecutionState& state, Optional<Object*> callback)
{
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    auto promise = promiseCapability.m_promise->asPromiseObject();
    state.context()->vmInstance()->enqueuePromiseJob(promise, new CleanupSomeJob(state.context(), this, callback, promise));
}

void* FinalizationRegistryObject::FinalizationRegistryObjectItem::operator new(size_t size)
{
#ifdef NDEBUG
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(FinalizationRegistryObject::FinalizationRegistryObjectItem)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject::FinalizationRegistryObjectItem, source));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject::FinalizationRegistryObjectItem, heldValue));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject::FinalizationRegistryObjectItem, unregisterToken));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(FinalizationRegistryObject::FinalizationRegistryObjectItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<FinalizationRegistryObjectItem>().allocate(1);
#endif
}

void FinalizationRegistryObject::finalizer(Object* self, void* data)
{
    FinalizationRegistryObjectItem* item = (FinalizationRegistryObjectItem*)data;
    // finalizer call should not trigger GC
    GC_disable();

    auto copyedCells = item->source->m_cells;
    for (size_t i = 0; i < item->source->m_cells.size(); i++) {
        if (self == item->source->m_cells[i]->weakRefTarget) {
            item->source->m_cells.erase(i);
            i--;
        }
    }

    if (item->source->m_cleanupCallback) {
        for (size_t i = 0; i < copyedCells.size(); i++) {
            if (self == copyedCells[i]->weakRefTarget) {
                SandBox sb(item->source->m_realm);
                struct ExecutionData {
                    FinalizationRegistryObjectItem* item;
                } ed;
                ed.item = item;
                sb.run([](ExecutionState& state, void* data) -> Value {
                    ExecutionData* ed = (ExecutionData*)data;
                    Value argv = ed->item->heldValue;
                    Object::call(state, ed->item->source->m_cleanupCallback.value(), Value(), 1, &argv);
                    return Value();
                },
                       &ed);
            }
        }
    }

    GC_enable();
}

} // namespace Escargot
