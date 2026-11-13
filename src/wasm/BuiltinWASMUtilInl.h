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

#ifndef ESCARGOT_BUILTINWASMUTILINL_H
#define ESCARGOT_BUILTINWASMUTILINL_H

#if defined(ENABLE_WASM)

// represent ownership of each object
// object marked with 'own' should be deleted in the current context
#define own

namespace Escargot {

static String* wasmStringValueOfExternType(ExecutionState& state, wasm_externkind_t kind)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    String* result = nullptr;
    switch (kind) {
    case WASM_EXTERN_FUNC:
        result = strings->function.string();
        break;
    case WASM_EXTERN_TABLE:
        result = strings->table.string();
        break;
    case WASM_EXTERN_MEMORY:
        result = strings->memory.string();
        break;
    case WASM_EXTERN_GLOBAL:
        result = strings->global.string();
        break;
    case WASM_EXTERN_TAG:
        result = strings->tag.string();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return result;
}

// WebAssembly.Module
static Value builtinWASMModuleExports(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->exports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

    own wasm_exporttype_vec_t export_types;
    wasm_module_exports(module, &export_types);

    // Let exports be << >>.
    // set default array length as export_types.size
    ArrayObject* exports = new ArrayObject(state, (uint64_t)export_types.size);

    for (size_t i = 0; i < export_types.size; i++) {
        // For each (name, type) of module_exports(module),
        const wasm_name_t* name = wasm_exporttype_name(export_types.data[i]);
        wasm_externkind_t type = wasm_externtype_kind(wasm_exporttype_type(export_types.data[i]));

        String* nameValue = String::fromASCII(name->data, name->size);
        // Let kind be the string value of the extern type type.
        String* kindValue = wasmStringValueOfExternType(state, type);

        // Let obj be ?[ "name" ? name, "kind" ? kind ]?.
        Object* obj = new Object(state);
        obj->defineOwnProperty(state, ObjectPropertyName(strings->name), ObjectPropertyDescriptor(nameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->kind), ObjectPropertyDescriptor(kindValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

        // Append obj to exports.
        exports->setIndexedProperty(state, Value(i), Value(obj), exports);
    }

    wasm_exporttype_vec_delete(&export_types);
    // Return exports.
    return exports;
}

static Value builtinWASMModuleImports(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    Value moduleValue = argv[0];
    if (!moduleValue.isObject() || !moduleValue.asObject()->isWASMModuleObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->WebAssemblyDotModule.string(), false, strings->imports.string(), ErrorObject::Messages::GlobalObject_IllegalFirstArgument);
    }

    // Let module be moduleObject.[[Module]].
    wasm_module_t* module = moduleValue.asObject()->asWASMModuleObject()->module();

    own wasm_importtype_vec_t import_types;
    wasm_module_imports(module, &import_types);

    // Let imports be << >>
    // set default array length as import_types.size
    ArrayObject* imports = new ArrayObject(state, (uint64_t)import_types.size);

    for (size_t i = 0; i < import_types.size; i++) {
        // For each (moduleName, name, type) of module_imports(module),
        const wasm_name_t* moduleName = wasm_importtype_module(import_types.data[i]);
        const wasm_name_t* name = wasm_importtype_name(import_types.data[i]);
        wasm_externkind_t type = wasm_externtype_kind(wasm_importtype_type(import_types.data[i]));

        String* moduleNameValue = String::fromASCII(moduleName->data, moduleName->size);
        String* nameValue = String::fromASCII(name->data, name->size);
        // Let kind be the string value of the extern type type.
        String* kindValue = wasmStringValueOfExternType(state, type);

        // Let obj be ?[ "module" ? moduleName, "name" ? name, "kind" ? kind ]?.
        Object* obj = new Object(state);
        obj->defineOwnProperty(state, ObjectPropertyName(strings->module), ObjectPropertyDescriptor(moduleNameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->name), ObjectPropertyDescriptor(nameValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
        obj->defineOwnProperty(state, ObjectPropertyName(strings->kind), ObjectPropertyDescriptor(kindValue, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

        // Append obj to imports.
        imports->setIndexedProperty(state, Value(i), Value(obj), imports);
    }

    wasm_importtype_vec_delete(&import_types);
    // Return imports.
    return imports;
}

} // namespace Escargot

#endif /* ESCARGOT_BUILTINWASMUTILINL_H */
#endif /* defined(ENABLE_WASM) */
