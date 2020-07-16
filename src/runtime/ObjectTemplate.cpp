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

#include "Escargot.h"
#include "ObjectTemplate.h"
#include "runtime/Value.h"
#include "runtime/Object.h"
#include "runtime/Context.h"
#include "runtime/FunctionTemplate.h"

namespace Escargot {

void* ObjectTemplate::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(ObjectTemplate)] = { 0 };
        ObjectTemplate::fillGCDescriptor(objBitmap);
        GC_set_bit(objBitmap, GC_WORD_OFFSET(ObjectTemplate, m_constructor));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(ObjectTemplate));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectTemplate::ObjectTemplate(Optional<FunctionTemplate*> constructor)
    : m_constructor(constructor)
{
}

Object* ObjectTemplate::instantiate(Context* ctx)
{
    size_t propertyCount = m_properties.size();
    if (!m_cachedObjectStructure) {
        m_cachedObjectStructure = constructObjectStructure(ctx, nullptr, 0);
    }
    ObjectPropertyValueVector objectPropertyValues;
    constructObjectPropertyValues(ctx, nullptr, 0, objectPropertyValues);

    Object* result;
    if (m_constructor && m_constructor->isConstructor()) {
        FunctionObject* fn = m_constructor->instantiate(ctx)->asFunctionObject();
        result = new Object(m_cachedObjectStructure, std::move(objectPropertyValues), fn->uncheckedGetOwnDataProperty(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER).asPointerValue()->asObject());
    } else {
        result = new Object(m_cachedObjectStructure, std::move(objectPropertyValues), ctx->globalObject()->objectPrototype());
    }
    postProcessing(result);
    return result;
}
}
