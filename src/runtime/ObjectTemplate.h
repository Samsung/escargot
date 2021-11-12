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

#ifndef __EscargotObjectTemplate__
#define __EscargotObjectTemplate__

#include "runtime/Template.h"

namespace Escargot {

class FunctionTemplate;
class ObjectTemplatePropertyHandlerData;
struct ObjectTemplatePropertyHandlerConfiguration;

class ObjectTemplate : public Template {
public:
    ObjectTemplate(Optional<FunctionTemplate*> constructor = nullptr);

    virtual bool isObjectTemplate() const override
    {
        return true;
    }

    virtual Object* instantiate(Context* ctx) override;

    // this adds the properties only.
    // the named property handler, extraData and constructor are ignored.
    bool installTo(Context* ctx, Object* target);

    Optional<FunctionTemplate*> constructor()
    {
        return m_constructor;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    void setNamedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data);
    void setIndexedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data);
    void removeNamedPropertyHandler();
    void removeIndexedPropertyHandler();

private:
    Optional<FunctionTemplate*> m_constructor;
    ObjectTemplatePropertyHandlerData* m_namedPropertyHandler;
    ObjectTemplatePropertyHandlerData* m_indexedPropertyHandler;
};
} // namespace Escargot

#endif
