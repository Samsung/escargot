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

#ifndef __EscargotWASMByteCode__
#define __EscargotWASMByteCode__

#include "wasm/interpreter/WASMOpcode.h"

namespace Escargot {

class WASMByteCode {
public:
    WASMOpcodeKind opcode() const
    {
        return m_opcode;
    }

protected:
    WASMByteCode(WASMOpcodeKind opcode)
        : m_opcode(opcode)
    {
    }

    union {
        WASMOpcodeKind m_opcode;
        void* m_opcodeInAddress; // TODO
    };
};

class WASMI32Const : public WASMByteCode {
public:
    WASMI32Const(int32_t value)
        : WASMByteCode(WASMOpcodeKind::WASMI32ConstOpcode)
        , m_value(value)
    {
    }

    int32_t value() const
    {
        return m_value;
    }

protected:
    int32_t m_value;
};

class WASMCall : public WASMByteCode {
public:
    WASMCall(uint32_t index)
        : WASMByteCode(WASMOpcodeKind::WASMCallOpcode)
        , m_index(index)
    {
    }

    uint32_t index() const
    {
        return m_index;
    }

protected:
    uint32_t m_index;
};

class WASMEnd : public WASMByteCode {
public:
    WASMEnd()
        : WASMByteCode(WASMOpcodeKind::WASMEndOpcode)
    {
    }

protected:
};

} // namespace Escargot

#endif // __EscargotWASMByteCode__
#endif // ENABLE_WASM_INTERPRETER
