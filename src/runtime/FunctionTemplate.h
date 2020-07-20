/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotFunctionTemplate__
#define __EscargotFunctionTemplate__

#include "runtime/Template.h"
#include "runtime/ObjectTemplate.h"
#include "parser/CodeBlock.h"
#include "api/EscargotPublic.h"

namespace Escargot {

class CallTemplateFunctionData;

class FunctionTemplate : public Template {
public:
    enum BuiltinFunctionSlot : size_t {
        CallTemplateFunctionDataIndex = 0,
    };

    FunctionTemplate(AtomicString name, size_t argumentCount, bool isStrict, bool isConstructor,
                     FunctionTemplateRef::NativeFunctionPointer fn);

    void updateCallbackFunction(FunctionTemplateRef::NativeFunctionPointer fn);

    // returns the unique function instance in context.
    virtual Object* instantiate(Context* ctx) override;

    ObjectTemplate* prototypeTemplate() const
    {
        return m_prototypeTemplate;
    }

    // ObjectTemplate for new'ed instance of this functionTemplate
    ObjectTemplate* instanceTemplate() const
    {
        return m_instanceTemplate;
    }

    void inherit(Optional<FunctionTemplate*> parent)
    {
        ASSERT(!m_cachedObjectStructure);
        ASSERT(parent->m_isConstructor);
        m_parent = parent;
    }

    Optional<FunctionTemplate*> parent() const
    {
        return m_parent;
    }

    virtual bool isFunctionTemplate() const override
    {
        return true;
    }

    bool isConstructor() const
    {
        return m_isConstructor;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    AtomicString m_name;
    size_t m_argumentCount;
    bool m_isStrict;
    bool m_isConstructor;
    CallTemplateFunctionData* m_nativeFunctionData;
    ObjectTemplate* m_prototypeTemplate;
    ObjectTemplate* m_instanceTemplate;
    Optional<FunctionTemplate*> m_parent;
};
}

#endif
