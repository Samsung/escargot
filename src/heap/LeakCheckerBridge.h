/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotLeakCheckerBridge__
#define __EscargotLeakCheckerBridge__

#ifdef PROFILE_BDWGC

#include "runtime/Value.h"

namespace Escargot {

class ExecutionState;
Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget);
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget);
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Value newTarget);
}

#endif // PROFILE_BDWGC

#endif // __EscargotLeakCheckerBridge__
