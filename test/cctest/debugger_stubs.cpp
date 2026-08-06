/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

// testapi.cpp references DebuggerOperationsRef methods whose real definitions
// (in src/api/EscargotPublic.cpp) are compiled only when ESCARGOT_DEBUGGER is
// enabled. The N-API cctest build dir is configured without the debugger, so
// those symbols are absent and testapi.cpp fails to link. These stubs satisfy
// the linker so the rest of the test binary (including the Napi.* suite) can be
// built; the debugger tests themselves are not exercised in this configuration.
// Guarded off when ESCARGOT_DEBUGGER is set, so a debugger-enabled build uses
// the real definitions and these do not clash.

#if !defined(ESCARGOT_DEBUGGER)

#include "api/EscargotPublic.h"

namespace Escargot {

StringRef* DebuggerOperationsRef::BreakpointOperations::eval(StringRef* sourceCode, bool& isError, size_t& objectIndex)
{
    isError = false;
    objectIndex = 0;
    return nullptr;
}

void DebuggerOperationsRef::BreakpointOperations::getStackTrace(DebuggerStackTraceDataVector& outStackTrace)
{
}

void DebuggerOperationsRef::BreakpointOperations::getLexicalScopeChain(uint32_t stateIndex, LexicalScopeChainVector& outLexicalScopeChain)
{
}

DebuggerOperationsRef::PropertyKeyValueVector DebuggerOperationsRef::BreakpointOperations::getLexicalScopeChainProperties(uint32_t stateIndex, uint32_t scopeIndex)
{
    return PropertyKeyValueVector();
}

StringRef* DebuggerOperationsRef::getFunctionName(WeakCodeRef* weakCodeRef)
{
    return nullptr;
}

bool DebuggerOperationsRef::updateBreakpoint(WeakCodeRef* weakCodeRef, uint32_t offset, bool enable)
{
    return false;
}

} // namespace Escargot

#endif // !ESCARGOT_DEBUGGER
