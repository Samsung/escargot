/*
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

#include "Escargot.h"
#include "Debugger.h"
#include "DebuggerTcp.h"
#include "interpreter/ByteCode.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/GlobalObject.h"
#include "runtime/SandBox.h"
#include "parser/Script.h"

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

void Debugger::stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
{
    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        m_delay = (uint8_t)(m_delay - 1);
        if (m_delay == 0) {
            processIncomingMessages(state, byteCodeBlock);
        }
        return;
    }

    BreakpointOffset breakpointOffset;
    void* byteCodeStart = byteCodeBlock->m_code.data();

    memcpy(&breakpointOffset.byteCodeStart, (void*)&byteCodeStart, sizeof(void*));
    memcpy(&breakpointOffset.offset, &offset, sizeof(uint32_t));

    send(ESCARGOT_MESSAGE_BREAKPOINT_HIT, &breakpointOffset, sizeof(BreakpointOffset));

    if (!enabled()) {
        return;
    }

    m_stopState = nullptr;

    while (processIncomingMessages(state, byteCodeBlock))
        ;

    m_delay = ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY;
}

void Debugger::releaseFunction(const void* ptr)
{
    // All messages which involves this pointer should be ignored until the confirmation arrives.
    m_releasedFunctions.push_back(reinterpret_cast<uintptr_t>(ptr));
    sendPointer(ESCARGOT_MESSAGE_RELEASE_FUNCTION, ptr);
}

bool Debugger::processIncomingMessages(ExecutionState* state, ByteCodeBlock* byteCodeBlock)
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
            if (length != 1 + 1 + sizeof(uintptr_t) + sizeof(uint32_t)) {
                break;
            }

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
            if (length != 1 || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }
            m_stopState = nullptr;
            return false;
        }
        case ESCARGOT_MESSAGE_STEP: {
            if (length != 1 || m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
                break;
            }
            m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;
            return false;
        }
        case ESCARGOT_MESSAGE_NEXT: {
            if (length != 1 || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }
            m_stopState = state;
            return false;
        }
        case ESCARGOT_MESSAGE_EVAL_8BIT_START:
        case ESCARGOT_MESSAGE_EVAL_16BIT_START: {
            if ((length <= 1 + sizeof(uint32_t)) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            return doEval(state, byteCodeBlock, buffer, length);
        }
        case ESCARGOT_MESSAGE_GET_BACKTRACE: {
            if ((length != 1 + sizeof(uint32_t) + sizeof(uint32_t) + 1) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            uint32_t minDepth;
            uint32_t maxDepth;
            memcpy(&minDepth, buffer + 1, sizeof(uint32_t));
            memcpy(&maxDepth, buffer + 1 + sizeof(uint32_t), sizeof(uint32_t));

            getBacktrace(state, minDepth, maxDepth, buffer[1 + sizeof(uint32_t) + sizeof(uint32_t)] != 0);
            return true;
        }
        case ESCARGOT_MESSAGE_GET_SCOPE_CHAIN: {
            if (length != 1 || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }
            getScopeChain(state);
            return true;
        }
        }

        ESCARGOT_LOG_ERROR("Invalid message received. Closing connection.\n");
        close();
        return false;
    }

    return enabled();
}

bool Debugger::doEval(ExecutionState* state, ByteCodeBlock* byteCodeBlock, uint8_t* buffer, size_t length)
{
    uint8_t type = (uint8_t)(buffer[0] + 1);
    uint32_t size;
    char* data;

    memcpy(&size, buffer + 1, sizeof(uint32_t));

    if (size > ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1 - sizeof(uint32_t)) {
        if (length != ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH) {
            goto error;
        }

        data = (char*)GC_MALLOC_ATOMIC(size);
        char* ptr = data;
        char* end = data + size;

        length = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1 - sizeof(uint32_t);
        memcpy(ptr, buffer + 1 + sizeof(uint32_t), length);
        ptr += length;

        do {
            if (receive(buffer, length)) {
                if (buffer[0] != type || (length > (size_t)(end - ptr + 1))) {
                    goto error;
                }

                memcpy(ptr, buffer + 1, length - 1);
                ptr += length - 1;
            } else if (!enabled()) {
                return false;
            }
        } while (ptr < end);
    } else {
        if (size + 1 + sizeof(uint32_t) != length) {
            goto error;
        }

        data = (char*)buffer + 1 + sizeof(uint32_t);
    }

    String* str;

    if (type == ESCARGOT_MESSAGE_EVAL_8BIT) {
        str = new Latin1String(data, size);
    } else {
        str = new UTF16String((char16_t*)data, size / 2);
    }

    type = ESCARGOT_MESSAGE_EVAL_RESULT_8BIT;
    m_stopState = ESCARGOT_DEBUGGER_IN_EVAL_MODE;

    try {
        Value asValue(str);
        Value result(state->context()->globalObject()->evalLocal(*state, asValue, Value(Value::Undefined), byteCodeBlock->m_codeBlock->asInterpretedCodeBlock(), true));
        str = result.toStringWithoutException(*state);
    } catch (const Value& val) {
        type = ESCARGOT_MESSAGE_EVAL_FAILED_8BIT;
        str = val.toStringWithoutException(*state);
    }

    m_stopState = ESCARGOT_DEBUGGER_IN_WAIT_MODE;

    if (enabled()) {
        StringView* strView = new StringView(str);
        sendString(type, strView);
    }

    return true;

error:
    ESCARGOT_LOG_ERROR("Invalid eval message received. Closing connection.\n");
    close();
    return false;
}

void Debugger::getBacktrace(ExecutionState* state, uint32_t minDepth, uint32_t maxDepth, bool getTotal)
{
    StringBuilder builder;
    SandBox::StackTraceDataVector stackTraceData;

    SandBox::createStackTraceData(stackTraceData, *state);

    uint32_t total = (uint32_t)stackTraceData.size();

    if (maxDepth == 0 || maxDepth > total) {
        maxDepth = total;
    }

    if (minDepth >= total) {
        minDepth = total;
    }

    for (uint32_t i = minDepth; i < maxDepth; i++) {
        if ((size_t)stackTraceData[i].second.loc.index == SIZE_MAX && (size_t)stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
            ByteCodeBlock* byteCodeBlock = stackTraceData[i].second.loc.actualCodeBlock;
            size_t byteCodePosition = stackTraceData[i].second.loc.byteCodePosition;

            ExtendedNodeLOC loc = byteCodeBlock->computeNodeLOCFromByteCode(state->context(), byteCodePosition, byteCodeBlock->m_codeBlock);
            builder.appendString(byteCodeBlock->m_codeBlock->script()->src());
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.line));
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.column));
        } else {
            builder.appendString(stackTraceData[i].second.src);
        }
        builder.appendChar('\0');
    }

    if (enabled() && getTotal) {
        send(ESCARGOT_MESSAGE_BACKTRACE_TOTAL, &total, sizeof(uint32_t));
    }

    StringView* backtrace = new StringView(builder.finalize());
    if (enabled()) {
        sendString(ESCARGOT_MESSAGE_BACKTRACE_8BIT, backtrace);
    }
}

void Debugger::getScopeChain(ExecutionState* state)
{
    const size_t maxMessageLength = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1;
    uint8_t buffer[maxMessageLength];
    size_t nextScope = 0;

    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv) {
        EnvironmentRecord* record = lexEnv->record();
        uint8_t type;

        if (nextScope >= maxMessageLength) {
            if (!send(ESCARGOT_MESSAGE_SCOPE_CHAIN, buffer, maxMessageLength)) {
                return;
            }
            nextScope = 0;
        }


        if (record->isGlobalEnvironmentRecord()) {
            type = ESCARGOT_RECORD_GLOBAL_ENVIRONMENT;
        } else if (record->isDeclarativeEnvironmentRecord()) {
            DeclarativeEnvironmentRecord* declarativeRecord = record->asDeclarativeEnvironmentRecord();
            if (declarativeRecord->isFunctionEnvironmentRecord()) {
                type = ESCARGOT_RECORD_FUNCTION_ENVIRONMENT;
            } else {
                type = ESCARGOT_RECORD_DECLARATIVE_ENVIRONMENT;
            }
        } else if (record->isObjectEnvironmentRecord()) {
            type = ESCARGOT_RECORD_OBJECT_ENVIRONMENT;
        } else if (record->isModuleEnvironmentRecord()) {
            type = ESCARGOT_RECORD_MODULE_ENVIRONMENT;
        } else {
            type = ESCARGOT_RECORD_UNKNOWN_ENVIRONMENT;
        }

        buffer[nextScope++] = type;
        lexEnv = lexEnv->outerEnvironment();
    }

    send(ESCARGOT_MESSAGE_SCOPE_CHAIN_END, buffer, nextScope);
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

            configuration.maxMessageSize = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH;
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
