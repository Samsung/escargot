/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#include "Escargot.h"
#include "Debugger.h"
#include "DebuggerTcp.h"
#include "interpreter/ByteCode.h"
#include "runtime/Context.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

void Debugger::sendType(uint8_t type)
{
    send(type, nullptr, 0);
}

void Debugger::sendString(uint8_t type, StringView* string)
{
    size_t length = string->length();

    if (string->has8BitContent()) {
        const LChar* chars = string->characters8();
        const size_t maxMessageLength = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1;

        while (length > maxMessageLength) {
            if (!send(type, chars, maxMessageLength)) {
                return;
            }
            length -= maxMessageLength;
            chars += maxMessageLength;
        }

        send(type + 1, chars, length);
        return;
    }

    const char16_t* chars = string->characters16();
    const size_t maxMessageLength = (ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1) / 2;

    while (length > maxMessageLength) {
        if (!send(type + 2, chars, maxMessageLength * 2)) {
            return;
        }
        length -= maxMessageLength;
        chars += maxMessageLength;
    }

    send(type + 2 + 1, chars, length * 2);
}

void Debugger::sendPointer(uint8_t type, const void* ptr)
{
    // The pointer itself is sent, not the data pointed by it
    send(type, (void*)&ptr, sizeof(void*));
}

void Debugger::sendFunctionInfo(InterpretedCodeBlock* codeBlock)
{
    char* byteCodeStart = codeBlock->byteCodeBlock()->m_code.data();
    uint32_t startLine = (uint32_t)codeBlock->functionStart().line;
    uint32_t startColumn = (uint32_t)(codeBlock->functionStart().column + 1);
    FunctionInfo functionInfo;

    memcpy(&functionInfo.byteCodeStart, (void*)&byteCodeStart, sizeof(char*));
    memcpy(&functionInfo.startLine, &startLine, sizeof(uint32_t));
    memcpy(&functionInfo.startColumn, &startColumn, sizeof(uint32_t));

    send(ESCARGOT_MESSAGE_FUNCTION_PTR, (void*)&functionInfo, sizeof(FunctionInfo));
}

void Debugger::sendBreakpointLocations(std::vector<Debugger::BreakpointLocation>& locations)
{
    const size_t maxPacketLength = (ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1) / sizeof(BreakpointLocation);
    BreakpointLocation* ptr = locations.data();
    size_t length = locations.size();

    while (length > maxPacketLength) {
        if (!send(ESCARGOT_MESSAGE_BREAKPOINT_LOCATION, ptr, maxPacketLength * sizeof(BreakpointLocation))) {
            return;
        }
        ptr += maxPacketLength;
        length -= maxPacketLength;
    }

    send(ESCARGOT_MESSAGE_BREAKPOINT_LOCATION, ptr, length * sizeof(BreakpointLocation));
}

void Debugger::stopAtBreakpoint(void* byteCodeStart, uint32_t offset, ExecutionState* state)
{
    BreakpointOffset breakpointOffset;

    memcpy(&breakpointOffset.byteCodeStart, (void*)&byteCodeStart, sizeof(void*));
    memcpy(&breakpointOffset.offset, &offset, sizeof(uint32_t));

    send(ESCARGOT_MESSAGE_BREAKPOINT_HIT, &breakpointOffset, sizeof(BreakpointOffset));

    if (!enabled()) {
        return;
    }

    m_stopState = nullptr;

    while (processIncomingMessages(state))
        ;

    m_delay = ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY;
}

void Debugger::releaseFunction(const void* ptr)
{
    // All messages which involves this pointer should be ignored until the confirmation arrives.
    m_releasedFunctions.push_back(reinterpret_cast<uintptr_t>(ptr));
    sendPointer(ESCARGOT_MESSAGE_RELEASE_FUNCTION, ptr);
}

bool Debugger::processIncomingMessages(ExecutionState* state)
{
    uint8_t buffer[ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH];
    size_t length;

    while (receive(buffer, length)) {
        switch (buffer[0]) {
        case ESCARGOT_MESSAGE_FUNCTION_RELEASED: {
            if (length != 1 + sizeof(uintptr_t)) {
                break;
            }

            uintptr_t ptr;
            memcpy(&ptr, buffer + 1, sizeof(uintptr_t));

            for (size_t i = 0; i < m_releasedFunctions.size(); i++) {
                if (m_releasedFunctions[i] == ptr) {
                    // Delete only the first instance.
                    m_releasedFunctions.erase(i);
                    return true;
                }
            }
            break;
        }
        case ESCARGOT_MESSAGE_UPDATE_BREAKPOINT: {
            ASSERT(length == 1 + 1 + sizeof(uintptr_t) + sizeof(uint32_t));

            uintptr_t ptr;
            uint32_t offset;
            memcpy(&ptr, buffer + 1 + 1, sizeof(uintptr_t));
            memcpy(&offset, buffer + 1 + 1 + sizeof(uintptr_t), sizeof(uint32_t));

            for (size_t i = 0; i < m_releasedFunctions.size(); i++) {
                if (m_releasedFunctions[i] == ptr) {
                    // This function has been already freed.
                    return true;
                }
            }

            ByteCode* breakpoint = (ByteCode*)(ptr + offset);

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
            if (buffer[1] != 0) {
                if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_table[BreakpointDisabledOpcode]) {
                    break;
                }
                breakpoint->m_opcodeInAddress = g_opcodeTable.m_table[BreakpointEnabledOpcode];
            } else {
                if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_table[BreakpointEnabledOpcode]) {
                    break;
                }
                breakpoint->m_opcodeInAddress = g_opcodeTable.m_table[BreakpointDisabledOpcode];
            }
#else
            if (buffer[1] != 0) {
                if (breakpoint->m_opcode != BreakpointDisabledOpcode) {
                    break;
                }
                breakpoint->m_opcode = BreakpointEnabledOpcode;
            } else {
                if (breakpoint->m_opcode != BreakpointEnabledOpcode) {
                    break;
                }
                breakpoint->m_opcode = BreakpointDisabledOpcode;
            }
#endif
            return true;
        }
        case ESCARGOT_MESSAGE_CONTINUE: {
            if (length != 1) {
                break;
            }
            m_stopState = nullptr;
            return false;
        }
        case ESCARGOT_MESSAGE_STEP: {
            if (length != 1) {
                break;
            }
            m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;
            return false;
        }
        case ESCARGOT_MESSAGE_NEXT: {
            if (length != 1) {
                break;
            }
            m_stopState = state;
            return false;
        }
        }

        ESCARGOT_LOG_ERROR("Invalid message received. Closing connection.\n");
        close();
        return false;
    }

    return enabled();
}

Debugger* createDebugger(const char* options, bool* debuggerEnabled)
{
    Debugger* debugger = new DebuggerTcp();

    if (debugger->init(options)) {
        union {
            uint16_t u16Value;
            uint8_t u8Value;
        } endian;

        endian.u16Value = 1;

        Debugger::MessageVersion version;
        version.littleEndian = (endian.u8Value == 1);

        uint32_t debuggerVersion = ESCARGOT_DEBUGGER_VERSION;
        memcpy(version.version, &debuggerVersion, sizeof(uint32_t));

        if (debugger->send(Debugger::ESCARGOT_MESSAGE_VERSION, &version, sizeof(version))) {
            Debugger::MessageConfiguration configuration;

            configuration.pointerSize = (uint8_t)sizeof(void*);

            debugger->send(Debugger::ESCARGOT_MESSAGE_CONFIGURATION, &configuration, sizeof(configuration));
        }

        if (debugger->enabled()) {
            *debuggerEnabled = true;
            debugger->m_debuggerEnabled = debuggerEnabled;
        }
    }
    return debugger;
}
}
#endif /* ESCARGOT_DEBUGGER */
