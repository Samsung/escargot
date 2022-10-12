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

#ifndef __EscargotWASMInterpreter__
#define __EscargotWASMInterpreter__

namespace Escargot {

class WASMFunction;
class WASMInstance;
class WASMInterpreter {
public:
    static void interpret(WASMInstance* instance, WASMFunction* callee, size_t programCounter, uint8_t*& sp);
    static void callOperation(WASMInstance* instance, WASMFunction* callee, size_t programCounter, uint8_t*& sp);
};

} // namespace Escargot

#endif // __EscargotWASMOpcode__
#endif // ENABLE_WASM_INTERPRETER
