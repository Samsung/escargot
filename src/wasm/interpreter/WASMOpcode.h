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

#ifndef __EscargotWASMOpcode__
#define __EscargotWASMOpcode__

namespace Escargot {

enum WASMOpcodeKind : size_t {
#define WABT_OPCODE(rtype, type1, type2, type3, memSize, prefix, code, name, text, decomp) \
    WASM##name##Opcode,
#include "src/opcode.def"
#undef WABT_OPCODE
    WASMInvalidOpcode,
};

struct WASMByteCodeInfo {
    enum WASMByteCodeType {
        ___,
        I32,
        I64,
        F32,
        F64,
        V128
    };
    WASMOpcodeKind m_code;
    WASMByteCodeType m_resultType;
    WASMByteCodeType m_paramTypes[3];
    const char* m_name;

    size_t stackShrinkSize() const
    {
        ASSERT(m_code != WASMOpcodeKind::WASMInvalidOpcode);
        return byteCodeTypeToMemorySize(m_paramTypes[0]) + byteCodeTypeToMemorySize(m_paramTypes[1])
            + byteCodeTypeToMemorySize(m_paramTypes[2]) + byteCodeTypeToMemorySize(m_paramTypes[3]);
    }

    size_t stackGrowSize() const
    {
        ASSERT(m_code != WASMOpcodeKind::WASMInvalidOpcode);
        return byteCodeTypeToMemorySize(m_resultType);
    }

    static size_t byteCodeTypeToMemorySize(WASMByteCodeType tp)
    {
        switch (tp) {
        case I32:
        case F32:
#if defined(ESCARGOT_32)
            return 4;
#else
            return 8; // for stack align
#endif
        case I64:
        case F64:
            return 8;
        case V128:
            return 16;
        default:
            return 0;
        }
    }
};

extern WASMByteCodeInfo g_wasmByteCodeInfo[WASMOpcodeKind::WASMInvalidOpcode];

} // namespace Escargot

#endif // __EscargotWASMOpcode__
#endif // ENABLE_WASM_INTERPRETER
