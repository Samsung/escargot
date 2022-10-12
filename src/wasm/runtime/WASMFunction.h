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

#if defined(ENABLE_WASM_INTERPRETER)

#ifndef __EscargotWASMFunction__
#define __EscargotWASMFunction__

#include "wasm/runtime/WASMValue.h"

namespace Escargot {

class WASMInstance;
class WASMFunctionType;
class WASMModuleFunction;
class Object;
class Context;

class WASMFunction : public gc {
public:
    WASMFunction(WASMInstance* instance, WASMFunctionType* functionType, WASMModuleFunction* moduleFunction)
        : m_instance(instance)
        , m_functionType(functionType)
        , m_moduleFunction(moduleFunction)
    {
    }

    WASMInstance* instance() const
    {
        return m_instance;
    }

    WASMFunctionType* functionType() const
    {
        return m_functionType;
    }

    WASMModuleFunction* moduleFunction() const
    {
        return m_moduleFunction;
    }

    virtual void call(const uint32_t argc, WASMValue* argv, WASMValue* result);

protected:
    virtual ~WASMFunction() {}

    WASMInstance* m_instance;
    WASMFunctionType* m_functionType;
    WASMModuleFunction* m_moduleFunction;
};

class WASMImportedFunction : public WASMFunction {
public:
    WASMImportedFunction(WASMInstance* instance, WASMFunctionType* functionType, WASMModuleFunction* moduleFunction, Context* relatedContext, Object* importedFunction)
        : WASMFunction(instance, functionType, moduleFunction)
        , m_relatedContext(relatedContext)
        , m_importedFunction(importedFunction)
    {
    }

    virtual void call(const uint32_t argc, WASMValue* argv, WASMValue* result);

private:
    Context* m_relatedContext;
    Object* m_importedFunction;
};

} // namespace Escargot

#endif // __EscargotWASMFunction__
#endif // ENABLE_WASM_INTERPRETER
