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
#include "wasm/interpreter/WASMInterpreter.h"
#include "wasm/interpreter/WASMByteCode.h"
#include "wasm/runtime/WASMModule.h"
#include "wasm/runtime/WASMFunction.h"
#include "wasm/runtime/WASMInstance.h"

namespace Escargot {

template <typename T>
ALWAYS_INLINE void writeValue(uint8_t*& sp, const T& v)
{
    *reinterpret_cast<T*>(sp) = v;
    sp += stackAllocatedSize<T>();
}

template <typename T>
ALWAYS_INLINE T readValue(uint8_t*& sp)
{
    T v = *reinterpret_cast<T*>(sp);
    sp -= stackAllocatedSize<T>();
    return v;
}

void WASMInterpreter::interpret(WASMInstance* instance, WASMFunction* function, size_t programCounter, uint8_t*& sp)
{
#define ADD_PROGRAM_COUNTER(codeName) programCounter += sizeof(WASM##codeName);

#define DEFINE_OPCODE(codeName) case WASM##codeName##Opcode
#define DEFINE_DEFAULT                \
    default:                          \
        RELEASE_ASSERT_NOT_REACHED(); \
        }
#define NEXT_INSTRUCTION() \
    goto NextInstruction;

NextInstruction:
    WASMOpcodeKind currentOpcode = ((WASMByteCode*)programCounter)->opcode();

    switch (currentOpcode) {
        DEFINE_OPCODE(I32Const)
            :
        {
            WASMI32Const* code = (WASMI32Const*)programCounter;
            writeValue(sp, code->value());
            ADD_PROGRAM_COUNTER(I32Const);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Call)
            :
        {
            callOperation(instance, function, programCounter, sp);
            ADD_PROGRAM_COUNTER(Call);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(End)
            :
        {
            WASMEnd* code = (WASMEnd*)programCounter;
            return;
        }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

NEVER_INLINE void WASMInterpreter::callOperation(WASMInstance* instance, WASMFunction* function, size_t programCounter, uint8_t*& sp)
{
    WASMCall* code = (WASMCall*)programCounter;

    WASMFunction* target = instance->function(code->index());
    WASMFunctionType* ft = target->functionType();
    const WASMFunctionType::WASMFunctionTypeVector& param = ft->param();
    WASMValue* paramVector = ALLOCA(sizeof(WASMValue) * param.size(), WASMValue, _);

    sp = sp - ft->paramStackSize();
    uint8_t* paramStackPointer = sp;
    for (size_t i = 0; i < param.size(); i++) {
        paramVector[i] = WASMValue(param[i], paramStackPointer);
        paramStackPointer += wasmValueSizeInStack(param[i]);
    }

    const WASMFunctionType::WASMFunctionTypeVector& result = ft->result();
    WASMValue* resultVector = ALLOCA(sizeof(WASMValue) * result.size(), WASMValue, _);
    target->call(param.size(), paramVector, resultVector);

    for (size_t i = 0; i < result.size(); i++) {
        resultVector[i].writeToStack(sp);
        sp += wasmValueSizeInStack(result[i]);
    }
}

} // namespace Escargot

#endif // ENABLE_WASM_INTERPRETER
