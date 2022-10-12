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

#include "Escargot.h"
#include "wasm/runtime/WASMModule.h"
#include "wasm/runtime/WASMInstance.h"
#include "wasm/runtime/WASMFunction.h"

#include "runtime/ErrorObject.h"

namespace Escargot {

WASMInstance* WASMModule::instantiate(ExecutionState& state, const Value& importObj)
{
    WASMInstance* instance = new WASMInstance(this);
    instance->m_function.resize(m_function.size(), nullptr);

    // If importObject is not undefined and not Object, throw a TypeError exception.
    if (!importObj.isUndefined() && !importObj.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
    }

    if (!importObj.isObject()) {
        ASSERT(importObj.isUndefined());
        // If module.imports is not empty, and importObject is not Object, throw a TypeError exception.
        if (m_import.size()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
        }
    }

    Object* importObject = importObj.asObject();
    // Let imports be << >>
    // For each (moduleName, componentName, externtype) of module_imports(module),
    for (size_t i = 0; i < m_import.size(); i++) {
        auto type = m_import[i]->type();
        String* moduleNameValue = m_import[i]->moduleName();
        String* componentName = m_import[i]->fieldName();

        // Let o be ? Get(importObject, moduleName).
        Value o = importObject->get(state, ObjectPropertyName(state, moduleNameValue)).value(state, importObject);
        // If Type(o) is not Object, throw a TypeError exception.
        if (!o.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::WASM_ReadImportsError);
        }

        // Let v be ? Get(o, componentName).
        Value v = o.asObject()->get(state, ObjectPropertyName(state, componentName)).value(state, o);

        bool throwLinkError = false;
        size_t externFuncCount = 0;
        switch (type) {
        case WASMModuleImport::Function: {
            // If externtype is of the form func functype,
            if (!v.isCallable()) {
                // If IsCallable(v) is false, throw a LinkError exception.
                throwLinkError = true;
                break;
            }

            WASMFunction* wasmFunction;
            if (v.asObject()->isExportedFunctionObject()) {
                // If v has a [[FunctionAddress]] internal slot, and therefore is an Exported Function,
                // Let funcaddr be the value of v?s [[FunctionAddress]] internal slot.
                RELEASE_ASSERT_NOT_REACHED();
            } else {
                // Create a host function from v and functype, and let funcaddr be the result.
                // TODO Let index be the number of external functions in imports. This value index is known as the index of the host function funcaddr.
                wasmFunction = new WASMImportedFunction(instance, functionType(m_import[i]->signatureIndex()), function(m_import[i]->functionIndex()), v.asObject()->getFunctionRealm(state), v.asObject());
            }

            // Let externfunc be the external value func funcaddr.
            // Append externfunc to imports.
            instance->m_function[m_import[i]->functionIndex()] = wasmFunction;
            break;
        }
        default: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        }

        if (UNLIKELY(throwLinkError)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::WASMLinkError, ErrorObject::Messages::WASM_ReadImportsError);
        }
    }

    for (size_t i = 0; i < m_function.size(); i++) {
        auto idx = m_function[i]->functionIndex();
        if (!instance->m_function[idx]) {
            instance->m_function[idx] = new WASMFunction(instance, functionType(idx), function(idx));
        }
    }

    if (m_seenStartAttribute) {
        ASSERT(instance->m_function[m_start]->functionType()->param().size() == 0);
        ASSERT(instance->m_function[m_start]->functionType()->result().size() == 0);
        instance->m_function[m_start]->call(0, nullptr, nullptr);
    }

    return instance;
}

} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
