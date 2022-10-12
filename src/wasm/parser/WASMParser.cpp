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
#include "wasm/parser/WASMParser.h"
#include "wasm/runtime/WASMModule.h"
#include "wasm/interpreter/WASMOpcode.h"
#include "wasm/interpreter/WASMByteCode.h"

#include "src/escargot/binary-reader-escargot.h"

namespace wabt {

static Escargot::WASMValue::Type toWASMValueKindForFunctionType(Type type)
{
    switch (type) {
    case Type::I32:
        return Escargot::WASMValue::Type::I32;
    case Type::I64:
        return Escargot::WASMValue::Type::I32;
    case Type::F32:
        return Escargot::WASMValue::Type::F32;
    case Type::F64:
        return Escargot::WASMValue::Type::F64;
    case Type::FuncRef:
        return Escargot::WASMValue::Type::FuncRef;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

class WASMBinaryReader : public wabt::WASMBinaryReaderDelegate {
public:
    WASMBinaryReader(Escargot::WASMModule* module)
        : m_module(module)
        , m_currentFunction(nullptr)
        , m_functionStackSizeSoFar(0)
    {
    }

    virtual void BeginModule(uint32_t version) override
    {
        m_module->m_version = version;
    }

    virtual void EndModule() override
    {
    }

    virtual void OnTypeCount(Index count)
    {
        // TODO reserve vector if possible
    }

    virtual void OnFuncType(Index index, Index paramCount, Type* paramTypes, Index resultCount, Type* resultTypes) override
    {
        Escargot::WASMFunctionType::WASMFunctionTypeVector param;
        param.reserve(paramCount);
        for (size_t i = 0; i < paramCount; i++) {
            param.push_back(toWASMValueKindForFunctionType(paramTypes[i]));
        }
        Escargot::WASMFunctionType::WASMFunctionTypeVector result;
        for (size_t i = 0; i < resultCount; i++) {
            result.push_back(toWASMValueKindForFunctionType(resultTypes[i]));
        }
        m_module->m_functionType.push_back(new Escargot::WASMFunctionType(index, std::move(param), std::move(result)));
    }

    virtual void OnImportCount(Index count) override
    {
        m_module->m_import.reserve(count);
    }

    virtual void OnImportFunc(Index importIndex, std::string moduleName, std::string fieldName, Index funcIndex, Index sigIndex) override
    {
        m_module->m_function.push_back(new Escargot::WASMModuleFunction(funcIndex, sigIndex));
        m_module->m_import.push_back(new Escargot::WASMModuleImport(importIndex,
                                                                    Escargot::String::fromUTF8(moduleName.data(), moduleName.size()), Escargot::String::fromUTF8(fieldName.data(), fieldName.size()), funcIndex, sigIndex));
    }

    virtual void OnFunctionCount(Index count) override
    {
        m_module->m_function.reserve(count);
    }

    virtual void OnFunction(Index index, Index sigIndex) override
    {
        ASSERT(m_currentFunction == nullptr);
        m_module->m_function.push_back(new Escargot::WASMModuleFunction(index, sigIndex));
    }

    virtual void OnStartFunction(Index funcIndex) override
    {
        m_module->m_seenStartAttribute = true;
        m_module->m_start = funcIndex;
    }

    virtual void BeginFunctionBody(Index index, Offset size) override
    {
        ASSERT(m_currentFunction == nullptr);
        m_currentFunction = m_module->function(index);
        m_functionStackSizeSoFar = 0;
    }

    void computeStackSizePerOpcode(size_t stackGrowSize, size_t stackShrinkSize)
    {
        m_functionStackSizeSoFar += stackGrowSize;
        m_currentFunction->m_requiredStackSize = std::max(m_currentFunction->m_requiredStackSize, m_functionStackSizeSoFar);
        // TODO check underflow
        m_functionStackSizeSoFar -= stackShrinkSize;
    }

    virtual void OnOpcode(uint32_t opcode) override
    {
        size_t stackGrowSize = Escargot::g_wasmByteCodeInfo[opcode].stackGrowSize();
        size_t stackShrinkSize = Escargot::g_wasmByteCodeInfo[opcode].stackShrinkSize();
        computeStackSizePerOpcode(stackGrowSize, stackShrinkSize);
    }

    virtual void OnCallExpr(uint32_t index) override
    {
        auto functionType = m_module->functionType(m_module->function(index)->functionIndex());

        size_t stackShrinkSize = 0;
        for (size_t i = 0; i < functionType->param().size(); i++) {
            stackShrinkSize += wasmValueSizeInStack(functionType->param()[i]);
        }

        size_t stackGrowSize = 0;
        for (size_t i = 0; i < functionType->result().size(); i++) {
            stackGrowSize += wasmValueSizeInStack(functionType->result()[i]);
        }

        m_currentFunction->pushByteCode(Escargot::WASMCall(index));
        computeStackSizePerOpcode(stackGrowSize, stackShrinkSize);
    }

    virtual void OnI32ConstExpr(uint32_t value) override
    {
        m_currentFunction->pushByteCode(Escargot::WASMI32Const(value));
    }

    virtual void OnEndExpr() override
    {
        m_currentFunction->pushByteCode(Escargot::WASMEnd());
    }

    void EndFunctionBody(Index index) override
    {
        ASSERT(m_currentFunction == m_module->function(index));
        m_currentFunction = nullptr;
    }

    Escargot::WASMModule* m_module;
    Escargot::WASMModuleFunction* m_currentFunction;
    uint32_t m_functionStackSizeSoFar;
};

} // namespace wabt

namespace Escargot {

std::unique_ptr<WASMModule> WASMParser::parseBinary(const uint8_t* data, size_t len)
{
    std::unique_ptr<WASMModule> module(new WASMModule());
    wabt::WASMBinaryReader delegate(module.get());

    ReadWasmBinary(data, len, &delegate);
    return module;
}


} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
