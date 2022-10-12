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

#ifndef __EscargotWASMParser__
#define __EscargotWASMParser__

#include "wasm/runtime/WASMModule.h"

namespace Escargot {

class WASMModule;
class WASMParser {
public:
    // may return null when there is error on data
    static std::unique_ptr<WASMModule> parseBinary(const uint8_t* data, size_t len);
};

} // namespace Escargot

#endif // __EscargotWASMParser__
#endif // ENABLE_WASM_INTERPRETER
