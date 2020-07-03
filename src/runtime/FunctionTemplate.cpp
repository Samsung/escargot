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
#include "FunctionTemplate.h"
#include "runtime/Context.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/SandBox.h"
#include "api/internal/ValueAdapter.h"

namespace Escargot {

void* FunctionTemplate::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word objBitmap[GC_BITMAP_SIZE(FunctionTemplate)] = { 0 };
        Template::fillGCDescriptor(objBitmap);
        GC_set_bit(objBitmap, GC_WORD_OFFSET(FunctionTemplate, m_nativeFunctionData));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(FunctionTemplate, m_prototypeTemplate));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(FunctionTemplate, m_instanceTemplate));
        GC_set_bit(objBitmap, GC_WORD_OFFSET(FunctionTemplate, m_parent));
        descr = GC_make_descriptor(objBitmap, GC_WORD_LEN(FunctionTemplate));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

class CallFunctionTemplateFunctionData : public CallNativeFunctionData {
public:
    CallFunctionTemplateFunctionData(NativeFunctionPointer fn, NativeFunctionPointer callback, FunctionTemplate* functionTemplate)
        : CallNativeFunctionData(fn)
        , m_callback(callback)
        , m_functionTemplate(functionTemplate)
    {
    }

    NativeFunctionPointer m_callback;
    FunctionTemplate* m_functionTemplate;
};

FunctionTemplate::FunctionTemplate(AtomicString name, size_t argumentCount, bool isStrict, bool isConstructor,
                                   NativeFunctionPointer fn, Optional<ObjectTemplate*> instanceTemplate)
    : m_name(name)
    , m_argumentCount(argumentCount)
    , m_isStrict(isStrict)
    , m_isConstructor(isConstructor)
    , m_prototypeTemplate(new ObjectTemplate())
    , m_instanceTemplate(instanceTemplate)
{
    auto fnData = new CallFunctionTemplateFunctionData([](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value {
        FunctionObject* activeFunction = state.resolveCallee();
        CallFunctionTemplateFunctionData* data = (CallFunctionTemplateFunctionData*)activeFunction->codeBlock()->asNativeCodeBlock()->nativeFunctionData();
        if (newTarget) {
            Object* ret;
            if (data->m_functionTemplate->instanceTemplate()) {
                ret = data->m_functionTemplate->instanceTemplate()->instantiate(state.context());
            } else {
                ret = new Object(state);
            }
            ret->setPrototype(state, activeFunction->getFunctionPrototype(state));
            return ret;
        } else {
            return data->m_callback(state, thisValue, argc, argv, newTarget);
        }
    },
                                                       fn, this);
    m_nativeFunctionData = fnData;
}

class CallFunctionTemplatePublicFunctionData : public CallNativeFunctionData {
public:
    CallFunctionTemplatePublicFunctionData(NativeFunctionPointer fn, FunctionObjectRef::NativeFunctionPointer callback, FunctionTemplate* functionTemplate)
        : CallNativeFunctionData(fn)
        , m_callback(callback)
        , m_functionTemplate(functionTemplate)
    {
    }

    FunctionObjectRef::NativeFunctionPointer m_callback;
    FunctionTemplate* m_functionTemplate;
};

FunctionTemplate::FunctionTemplate(AtomicString name, size_t argumentCount, bool isStrict, bool isConstructor,
                                   FunctionObjectRef::NativeFunctionPointer fn, Optional<ObjectTemplate*> instanceTemplate)
    : m_name(name)
    , m_argumentCount(argumentCount)
    , m_isStrict(isStrict)
    , m_isConstructor(isConstructor)
    , m_prototypeTemplate(new ObjectTemplate())
    , m_instanceTemplate(instanceTemplate)
{
    auto fnData = new CallFunctionTemplatePublicFunctionData([](ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value {
        FunctionObject* activeFunction = state.resolveCallee();
        CallFunctionTemplatePublicFunctionData* data = (CallFunctionTemplatePublicFunctionData*)activeFunction->codeBlock()->asNativeCodeBlock()->nativeFunctionData();
        if (newTarget) {
            Object* ret;
            if (data->m_functionTemplate->instanceTemplate()) {
                ret = data->m_functionTemplate->instanceTemplate()->instantiate(state.context());
            } else {
                ret = new Object(state);
            }
            ret->setPrototype(state, activeFunction->getFunctionPrototype(state));
            return ret;
        } else {
            ValueRef** newArgv = ALLOCA(sizeof(ValueRef*) * argc, ValueRef*, state);
            for (size_t i = 0; i < argc; i++) {
                newArgv[i] = toRef(argv[i]);
            }
            return toImpl(data->m_callback((ExecutionStateRef*)(&state), toRef(thisValue), argc, newArgv, newTarget.hasValue()));
        }
    },
                                                             fn, this);
    m_nativeFunctionData = fnData;
}

Object* FunctionTemplate::instantiate(Context* ctx)
{
    auto& instantiatedFunctionObjects = ctx->instantiatedFunctionObjects();
    for (size_t i = 0; i < instantiatedFunctionObjects.size(); i++) {
        if (instantiatedFunctionObjects[i].first == this) {
            return instantiatedFunctionObjects[i].second;
        }
    }

    const size_t functionDefaultPropertyCount = m_isConstructor ? ctx->defaultStructureForFunctionObject()->propertyCount() : ctx->defaultStructureForNotConstructorFunctionObject()->propertyCount();
    size_t propertyCount = m_properties.size() + functionDefaultPropertyCount;
    if (!m_cachedObjectStructure) {
        if (m_isConstructor) {
            // [prototype, name, length]
            ASSERT(functionDefaultPropertyCount == 3);
            ObjectStructureItem structureItemVector[3] = {
                ObjectStructureItem(ctx->staticStrings().prototype,
                                    ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::WritablePresent)),
                ObjectStructureItem(ctx->staticStrings().name,
                                    ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent)),
                ObjectStructureItem(ctx->staticStrings().length,
                                    ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent))

            };
            m_cachedObjectStructure = constructObjectStructure(ctx, structureItemVector, 3);
        } else {
            // [name, length]
            ASSERT(functionDefaultPropertyCount == 2);
            ObjectStructureItem structureItemVector[2] = {
                ObjectStructureItem(ctx->staticStrings().name,
                                    ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent)),
                ObjectStructureItem(ctx->staticStrings().length,
                                    ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent))

            };

            m_cachedObjectStructure = constructObjectStructure(ctx, structureItemVector, 2);
        }
    }

    ObjectPropertyValueVector objectPropertyValues;
    Object* functionPrototype = nullptr;
    if (m_isConstructor) {
        // [prototype, name, length]
        if (!m_prototypeTemplate->has(ctx->staticStrings().constructor)) {
            m_prototypeTemplate->set(ctx->staticStrings().constructor, Value(), true, false, true);
        }

        ObjectPropertyValue baseValues[3];
        baseValues[0] = functionPrototype = m_prototypeTemplate->instantiate(ctx);
        baseValues[1] = m_name.string();
        baseValues[2] = Value(m_argumentCount);
        constructObjectPropertyValues(ctx, baseValues, 3, objectPropertyValues);
    } else {
        // [name, length]
        ObjectPropertyValue baseValues[2];
        objectPropertyValues[0] = m_name.string();
        objectPropertyValues[1] = Value(m_argumentCount);
        constructObjectPropertyValues(ctx, baseValues, 2, objectPropertyValues);
    }

    int flags = 0;
    flags |= m_isStrict ? NativeFunctionInfo::Strict : 0;
    flags |= m_isConstructor ? NativeFunctionInfo::Constructor : 0;
    NativeFunctionObject* result = new NativeFunctionObject(ctx, m_cachedObjectStructure, std::move(objectPropertyValues), NativeFunctionInfo(m_name, nullptr, m_argumentCount, flags), m_nativeFunctionData);
    if (m_isConstructor) {
        auto idx = m_prototypeTemplate->cachedObjectStructure()->findProperty(ctx->staticStrings().constructor).first;
        result->uncheckedGetOwnDataProperty(0).asPointerValue()->asObject()->uncheckedSetOwnDataProperty(idx, result);

        struct Sender {
            FunctionTemplate* functionTemplate;
            Object* prototype;
        };

        Sender d = { this, functionPrototype };
        SandBox sb(ctx);
        sb.run([](ExecutionState& state, void* data) -> Value {
            Sender* s = (Sender*)data;
            s->prototype->markAsPrototypeObject(state);
            if (s->functionTemplate->parent()) {
                s->prototype->setPrototype(state, s->functionTemplate->parent()->instantiate(state.context())->asFunctionObject()->getFunctionPrototype(state));
            }
            return Value();
        },
               &d);
    }

    instantiatedFunctionObjects.pushBack(std::make_pair(this, result));

    return result;
}
}
