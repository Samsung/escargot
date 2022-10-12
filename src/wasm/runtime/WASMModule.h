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

#ifndef __EscargotWASMModule__
#define __EscargotWASMModule__

#include <numeric>
#include "wasm/runtime/WASMValue.h"
#include "runtime/Value.h"

namespace wabt {
class WASMBinaryReader;
}

namespace Escargot {

class WASMModule;
class WASMInstance;

class WASMFunctionType : public gc {
public:
    typedef Vector<WASMValue::Type, GCUtil::gc_malloc_atomic_allocator<WASMValue::Type>> WASMFunctionTypeVector;
    WASMFunctionType(uint32_t index,
                     WASMFunctionTypeVector&& param,
                     WASMFunctionTypeVector&& result)
        : m_index(index)
        , m_param(std::move(param))
        , m_result(std::move(result))
        , m_paramStackSize(computeStackSize(m_param))
        , m_resultStackSize(computeStackSize(m_result))
    {
    }

    uint32_t index() const
    {
        return m_index;
    }

    const WASMFunctionTypeVector& param() const
    {
        return m_param;
    }

    const WASMFunctionTypeVector& result() const
    {
        return m_result;
    }

    size_t paramStackSize() const
    {
        return m_paramStackSize;
    }

    size_t resultStackSize() const
    {
        return m_resultStackSize;
    }

private:
    uint32_t m_index;
    WASMFunctionTypeVector m_param;
    WASMFunctionTypeVector m_result;
    size_t m_paramStackSize;
    size_t m_resultStackSize;

    static size_t computeStackSize(const WASMFunctionTypeVector& v)
    {
        size_t s = 0;
        for (size_t i = 0; i < v.size(); i++) {
            s += wasmValueSizeInStack(v[i]);
        }
        return s;
    }
};

// https://webassembly.github.io/spec/core/syntax/modules.html#syntax-import
class WASMModuleImport : public gc {
public:
    enum Type {
        Function,
        Table,
        Memory,
        Global
    };

    WASMModuleImport(uint32_t importIndex,
                     String* moduleName, String* fieldName,
                     uint32_t functionIndex, uint32_t signatureIndex)
        : m_type(Type::Function)
        , m_importIndex(importIndex)
        , m_moduleName(std::move(moduleName))
        , m_fieldName(std::move(fieldName))
        , m_functionIndex(functionIndex)
        , m_signatureIndex(signatureIndex)
    {
    }

    Type type() const
    {
        return m_type;
    }

    uint32_t importIndex() const
    {
        return m_importIndex;
    }

    String* moduleName() const
    {
        return m_moduleName;
    }

    String* fieldName() const
    {
        return m_fieldName;
    }

    uint32_t functionIndex() const
    {
        ASSERT(type() == Type::Function);
        return m_functionIndex;
    }

    uint32_t signatureIndex() const
    {
        ASSERT(type() == Type::Function);
        return m_signatureIndex;
    }

private:
    Type m_type;
    uint32_t m_importIndex;
    String* m_moduleName;
    String* m_fieldName;

    union {
        struct {
            uint32_t m_functionIndex;
            uint32_t m_signatureIndex;
        };
    };
};

class WASMModuleFunction : public gc {
    friend class wabt::WASMBinaryReader;

public:
    WASMModuleFunction(uint32_t functionIndex, uint32_t signatureIndex)
        : m_functionIndex(functionIndex)
        , m_signatureIndex(signatureIndex)
        , m_requiredStackSize(0)
    {
    }

    uint32_t functionIndex() const
    {
        return m_functionIndex;
    }

    uint32_t signatureIndex() const
    {
        return m_signatureIndex;
    }

    uint32_t requiredStackSize() const
    {
        return m_requiredStackSize;
    }

    template <typename CodeType>
    void pushByteCode(const CodeType& code)
    {
        char* first = (char*)&code;
        size_t start = m_byteCode.size();

        m_byteCode.resizeWithUninitializedValues(m_byteCode.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_byteCode[start++] = *first;
            first++;
        }
    }

    uint8_t* byteCode()
    {
        return m_byteCode.data();
    }

private:
    uint32_t m_functionIndex;
    uint32_t m_signatureIndex;
    uint32_t m_requiredStackSize;
    Vector<uint8_t, GCUtil::gc_malloc_atomic_allocator<uint8_t>> m_byteCode;
};

class WASMModule {
    friend class wabt::WASMBinaryReader;

public:
    WASMModule()
        : m_seenStartAttribute(false)
        , m_version(0)
        , m_start(0)
    {
    }

    WASMModuleFunction* function(uint32_t index)
    {
        for (size_t i = 0; i < m_function.size(); i++) {
            if (m_function[i]->functionIndex() == index) {
                return m_function[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    WASMFunctionType* functionType(uint32_t index)
    {
        for (size_t i = 0; i < m_functionType.size(); i++) {
            if (m_functionType[i]->index() == index) {
                return m_functionType[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    WASMInstance* instantiate(ExecutionState& state, const Value& importObject);

private:
    bool m_seenStartAttribute;
    uint32_t m_version;
    uint32_t m_start;
    Vector<WASMModuleImport*, GCUtil::gc_malloc_allocator<WASMModuleImport*>> m_import;
    Vector<WASMFunctionType*, GCUtil::gc_malloc_allocator<WASMFunctionType*>> m_functionType;
    Vector<WASMModuleFunction*, GCUtil::gc_malloc_allocator<WASMModuleFunction*>> m_function;
};

} // namespace Escargot

#endif // __EscargotWASMModule__
#endif // ENABLE_WASM_INTERPRETER
