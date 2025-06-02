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

#ifndef __EscargotSymbol__
#define __EscargotSymbol__

#include "runtime/PointerValue.h"
#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

class VMInstance;

struct SymbolFinalizerData : public gc {
    SymbolFinalizerData()
        : m_removedFinalizerCount(0)
    {
    }

    size_t m_removedFinalizerCount;
    TightVector<std::pair<FinalizerFunction, void*>, GCUtil::gc_malloc_allocator<std::pair<FinalizerFunction, void*>>> m_finalizer;
};

class Symbol : public PointerValue {
public:
    explicit Symbol(Optional<String*> desc = nullptr)
        : m_typeTag(POINTER_VALUE_SYMBOL_TAG_IN_DATA)
        , m_description(desc)
        , m_finalizerData()
    {
    }

    String* descriptionString() const;
    Value descriptionValue() const;

    bool hasFinalizerData() const
    {
        return !!m_finalizerData;
    }

    SymbolFinalizerData* finalizerData()
    {
        ASSERT(m_finalizerData.hasValue());
        return m_finalizerData.value();
    }

    String* symbolDescriptiveString() const;

    static Symbol* fromGlobalSymbolRegistry(VMInstance* vm, String* stringKey);
    static Value keyForSymbol(VMInstance* vm, Symbol* sym);

    virtual void addFinalizer(FinalizerFunction fn, void* data) override;
    virtual bool removeFinalizer(FinalizerFunction fn, void* data) override;

private:
    SymbolFinalizerData* ensureFinalizerData();
    void tryToShrinkFinalizers();

    size_t m_typeTag;
    Optional<String*> m_description; // nullptr of desc represents `undefined`
    Optional<SymbolFinalizerData*> m_finalizerData; // handle finalizer data of Symbol
};
} // namespace Escargot

#endif
