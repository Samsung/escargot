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

#if defined(ENABLE_WASM)

#ifndef __EscargotWASMObject__
#define __EscargotWASMObject__

struct wasm_module_t;
struct wasm_memory_t;
struct wasm_table_t;
struct wasm_global_t;
struct wasm_ref_t;

namespace Escargot {

class WASMModuleObject : public Object {
public:
    enum ImportExportKind {
        Function,
        Table,
        Memory,
        Global
    };

    explicit WASMModuleObject(ExecutionState& state, wasm_module_t* module);

    virtual bool isWASMModuleObject() const
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_module_t* module() const
    {
        ASSERT(!!m_module);
        return m_module;
    }

    ImportExportKind kind() const
    {
        return m_kind;
    }

private:
    wasm_module_t* m_module;
    ImportExportKind m_kind;
};

class WASMMemoryObject : public Object {
public:
    explicit WASMMemoryObject(ExecutionState& state, wasm_memory_t* memory, ArrayBufferObject* buffer);

    virtual bool isWASMMemoryObject() const
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_memory_t* memory() const
    {
        ASSERT(!!m_memory);
        return m_memory;
    }

    ArrayBufferObject* buffer() const;
    void setBuffer(ArrayBufferObject* buffer);

private:
    wasm_memory_t* m_memory;
    ArrayBufferObject* m_buffer;
};

class WASMTableObject : public Object {
public:
    explicit WASMTableObject(ExecutionState& state, wasm_table_t* table, ValueVector* values);

    virtual bool isWASMTableObject() const
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_table_t* table() const
    {
        ASSERT(!!m_table);
        return m_table;
    }

    ValueVector* values() const
    {
        ASSERT(!!m_values);
        return m_values;
    }

    Value getElement(size_t index) const;
    void setElement(size_t index, const Value& value);
    size_t length() const;

private:
    wasm_table_t* m_table;
    ValueVector* m_values;
};

class WASMGlobalObject : public Object {
public:
    explicit WASMGlobalObject(ExecutionState& state, wasm_global_t* global);

    virtual bool isWASMGlobalObject() const
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_global_t* global() const
    {
        ASSERT(!!m_global);
        return m_global;
    }

    Value getGlobalValue(ExecutionState& state) const;

private:
    wasm_global_t* m_global;
};

typedef std::unordered_map<void*, WASMMemoryObject*, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<std::pair<void* const, WASMMemoryObject*>>> WASMMemoryMap;
typedef std::unordered_map<void*, WASMTableObject*, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<std::pair<void* const, WASMTableObject*>>> WASMTableMap;
typedef std::unordered_map<void*, WASMGlobalObject*, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<std::pair<void* const, WASMGlobalObject*>>> WASMGlobalMap;

struct WASMCacheMap : public gc {
    WASMMemoryMap memoryMap;
    WASMTableMap tableMap;
    WASMGlobalMap globalMap;
};
}
#endif // __EscargotWASMObject__
#endif // ENABLE_WASM
