/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotPrototypeObject__
#define __EscargotPrototypeObject__

#include "runtime/Object.h"

namespace Escargot {

class PrototypeObject : public Object {
    friend class Global;
    explicit PrototypeObject()
        : Object()
    {
        // ctor for reading tag
    }

public:
    explicit PrototypeObject(ExecutionState& state);
    explicit PrototypeObject(ExecutionState& state, Object* proto, size_t defaultSpace = ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER);
    enum ForGlobalBuiltin { __ForGlobalBuiltin__ };
    explicit PrototypeObject(ExecutionState& state, ForGlobalBuiltin);

    virtual void markAsPrototypeObject(ExecutionState& state) override;
    virtual bool isEverSetAsPrototypeObject() const override
    {
        ASSERT(!hasRareData() || !rareData()->m_isEverSetAsPrototypeObject);
        return true;
    }
};

} // namespace Escargot

#endif
