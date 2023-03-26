/*
    static void setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block);
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotByteCodeInterpreter__
#define __EscargotByteCodeInterpreter__

namespace Escargot {

class ExecutionState;
class ByteCodeBlock;

class Interpreter {
public:
    enum BitwiseOperationKind : unsigned {
        And,
        Or,
        Xor,
    };

    enum ShiftOperationKind : unsigned {
        Left,
        SignedRight,
        UnsignedRight,
    };

    static Value interpret(ExecutionState* state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile);
};
} // namespace Escargot

#endif
