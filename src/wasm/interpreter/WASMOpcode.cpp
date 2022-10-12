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
#include "wasm/interpreter/WASMOpcode.h"

namespace Escargot {

WASMByteCodeInfo g_wasmByteCodeInfo[WASMOpcodeKind::WASMInvalidOpcode] = {
#define WABT_OPCODE(rtype, type1, type2, type3, memSize, prefix, code, name, text, decomp) \
    { WASM##name##Opcode, WASMByteCodeInfo::rtype, { WASMByteCodeInfo::type1, WASMByteCodeInfo::type2, WASMByteCodeInfo::type3 }, text },
#include "src/opcode.def"
#undef WABT_OPCODE
};

} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
