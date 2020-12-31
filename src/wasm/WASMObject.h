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
struct wasm_instance_t;
struct wasm_memory_t;
struct wasm_table_t;
struct wasm_global_t;
struct wasm_functype_t;
struct wasm_ref_t;

namespace Escargot {

struct WASMHostFunctionEnvironment : public gc {
    WASMHostFunctionEnvironment(Object* f, wasm_functype_t* ft);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    Object* func;
    wasm_functype_t* functype;
};

class WASMModuleObject : public Object {
public:
    explicit WASMModuleObject(ExecutionState& state, Object* proto, wasm_module_t* module);

    virtual bool isWASMModuleObject() const override
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

private:
    wasm_module_t* m_module;
};

class WASMInstanceObject : public Object {
public:
    explicit WASMInstanceObject(ExecutionState& state, wasm_instance_t* instance, Object* exports);
    explicit WASMInstanceObject(ExecutionState& state, Object* proto, wasm_instance_t* instance, Object* exports);

    virtual bool isWASMInstanceObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    wasm_instance_t* instance() const
    {
        ASSERT(!!m_instance);
        return m_instance;
    }

    Object* exports() const
    {
        ASSERT(!!m_exports);
        return m_exports;
    }

private:
    wasm_instance_t* m_instance;
    Object* m_exports;
};

class WASMMemoryObject : public Object {
public:
    explicit WASMMemoryObject(ExecutionState& state, wasm_memory_t* memory, ArrayBufferObject* buffer);
    explicit WASMMemoryObject(ExecutionState& state, Object* proto, wasm_memory_t* memory, ArrayBufferObject* buffer);

    virtual bool isWASMMemoryObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static WASMMemoryObject* createMemoryObject(ExecutionState& state, wasm_memory_t* memaddr);

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
    explicit WASMTableObject(ExecutionState& state, wasm_table_t* table);
    explicit WASMTableObject(ExecutionState& state, Object* proto, wasm_table_t* table);

    virtual bool isWASMTableObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static WASMTableObject* createTableObject(ExecutionState& state, wasm_table_t* tableaddr);

    wasm_table_t* table() const
    {
        ASSERT(!!m_table);
        return m_table;
    }

private:
    wasm_table_t* m_table;
};

class WASMGlobalObject : public Object {
public:
    explicit WASMGlobalObject(ExecutionState& state, wasm_global_t* global);
    explicit WASMGlobalObject(ExecutionState& state, Object* proto, wasm_global_t* global);

    virtual bool isWASMGlobalObject() const override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static WASMGlobalObject* createGlobalObject(ExecutionState& state, wasm_global_t* globaladdr);

    wasm_global_t* global() const
    {
        ASSERT(!!m_global);
        return m_global;
    }

    Value getGlobalValue(ExecutionState& state) const;

private:
    wasm_global_t* m_global;
};

// FIXME change vector structure to hash map (it requires hash value for each wasm_ref_t pointer)
typedef Vector<std::pair<wasm_ref_t*, WASMMemoryObject*>, GCUtil::gc_malloc_allocator<std::pair<wasm_ref_t*, WASMMemoryObject*>>> WASMMemoryMap;
typedef Vector<std::pair<wasm_ref_t*, WASMTableObject*>, GCUtil::gc_malloc_allocator<std::pair<wasm_ref_t*, WASMTableObject*>>> WASMTableMap;
typedef Vector<std::pair<wasm_ref_t*, WASMGlobalObject*>, GCUtil::gc_malloc_allocator<std::pair<wasm_ref_t*, WASMGlobalObject*>>> WASMGlobalMap;

class ExportedFunctionObject;
typedef Vector<std::pair<wasm_ref_t*, ExportedFunctionObject*>, GCUtil::gc_malloc_allocator<std::pair<wasm_ref_t*, ExportedFunctionObject*>>> WASMFunctionMap;

struct WASMCacheMap : public gc {
    WASMMemoryMap memoryMap;
    WASMTableMap tableMap;
    WASMGlobalMap globalMap;
    WASMFunctionMap functionMap;
};
} // namespace Escargot
#endif // __EscargotWASMObject__
#endif // ENABLE_WASM
