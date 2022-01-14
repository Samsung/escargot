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

void Debugger::enableDebugger(Context* context)
{
    ASSERT(m_context == nullptr);
    m_context = context;
    m_parsingEnabled = true;
    m_context->enableDebugger(this);
}

void Debugger::disableDebugger()
{
    ASSERT(m_context != nullptr);
    m_context->disableDebugger();
    m_context = nullptr;
}

void DebuggerRemote::sendType(uint8_t type)
{
    send(type, nullptr, 0);
}

void DebuggerRemote::sendSubtype(uint8_t type, uint8_t subType)
{
    send(type, &subType, 1);
}

void DebuggerRemote::sendString(uint8_t type, String* string)
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

void DebuggerRemote::sendPointer(uint8_t type, const void* ptr)
{
    // The pointer itself is sent, not the data pointed by it
    send(type, (void*)&ptr, sizeof(void*));
}

void DebuggerRemote::parseCompleted(String* source, String* srcName, String* error)
{
    if (!enabled()) {
        return;
    }

    sendString(ESCARGOT_MESSAGE_SOURCE_8BIT, source);

    if (!enabled()) {
        return;
    }

    sendString(ESCARGOT_MESSAGE_FILE_NAME_8BIT, srcName);

    if (!enabled()) {
        return;
    }

    if (error != nullptr) {
        sendType(ESCARGOT_MESSAGE_PARSE_ERROR);

        if (enabled()) {
            sendString(ESCARGOT_MESSAGE_STRING_8BIT, error);
        }
        return;
    }

    size_t breakpointLocationsSize = m_breakpointLocationsVector.size();

    for (size_t i = 0; i < breakpointLocationsSize; i++) {
        /* Send breakpoint locations. */
        const size_t maxPacketLength = (ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1) / sizeof(BreakpointLocation);
        BreakpointLocation* ptr = m_breakpointLocationsVector[i]->breakpointLocations.data();
        size_t length = m_breakpointLocationsVector[i]->breakpointLocations.size();

        while (length > maxPacketLength) {
            if (!send(ESCARGOT_MESSAGE_BREAKPOINT_LOCATION, ptr, maxPacketLength * sizeof(BreakpointLocation))) {
                return;
            }
            ptr += maxPacketLength;
            length -= maxPacketLength;
        }

        if (!send(ESCARGOT_MESSAGE_BREAKPOINT_LOCATION, ptr, length * sizeof(BreakpointLocation))) {
            return;
        }

        InterpretedCodeBlock* codeBlock = reinterpret_cast<InterpretedCodeBlock*>(m_breakpointLocationsVector[i]->weakCodeRef);
        String* functionName = codeBlock->functionName().string();

        /* Send function name. */
        if (functionName->length() > 0) {
            StringView* functionNameView = new StringView(functionName);
            sendString(ESCARGOT_MESSAGE_FUNCTION_NAME_8BIT, functionNameView);

            if (!enabled()) {
                return;
            }
        }

        /* Send function info. */
        char* byteCodeStart = codeBlock->byteCodeBlock()->m_code.data();
        uint32_t startLine = (uint32_t)codeBlock->functionStart().line;
        uint32_t startColumn = (uint32_t)(codeBlock->functionStart().column + 1);
        FunctionInfo functionInfo;

        memcpy(&functionInfo.byteCodeStart, (void*)&byteCodeStart, sizeof(char*));
        memcpy(&functionInfo.startLine, &startLine, sizeof(uint32_t));
        memcpy(&functionInfo.startColumn, &startColumn, sizeof(uint32_t));

        if (!send(ESCARGOT_MESSAGE_FUNCTION_PTR, (void*)&functionInfo, sizeof(FunctionInfo))) {
            return;
        }
    }

    sendType(ESCARGOT_MESSAGE_PARSE_DONE);

    if (enabled() && pendingWait()) {
        waitForResolvingPendingBreakpoints();
    }
}

void DebuggerRemote::sendBacktraceInfo(uint8_t type, ByteCodeBlock* byteCodeBlock, uint32_t line, uint32_t column, uint32_t executionStateDepth)
{
    BacktraceInfo backtraceInfo;

    char* byteCode = byteCodeBlock->m_code.data();
    memcpy(&backtraceInfo.byteCode, &byteCode, sizeof(void*));
    memcpy(&backtraceInfo.line, &line, sizeof(uint32_t));
    memcpy(&backtraceInfo.column, &column, sizeof(uint32_t));
    memcpy(&backtraceInfo.executionStateDepth, &executionStateDepth, sizeof(uint32_t));

    send(type, &backtraceInfo, sizeof(BacktraceInfo));
}

void DebuggerRemote::sendVariableObjectInfo(uint8_t subType, Object* object)
{
    /* Maximum UINT32_MAX number of objects are stored. */
    uint32_t size = (uint32_t)m_activeObjects.size();
    uint32_t index;

    for (index = 0; index < size; index++) {
        if (m_activeObjects[index] == object) {
            break;
        }
    }

    if (index == size && size < UINT32_MAX) {
        m_activeObjects.pushBack(object);
    }

    VariableObjectInfo variableObjectInfo;

    variableObjectInfo.subType = subType;
    memcpy(&variableObjectInfo.index, &index, sizeof(uint32_t));
    send(ESCARGOT_MESSAGE_VARIABLE, &variableObjectInfo, sizeof(VariableObjectInfo));
}

void DebuggerRemote::stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
{
    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        m_delay--;
        if (m_delay == 0) {
            processEvents(state, byteCodeBlock);
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

    ASSERT(m_activeObjects.size() == 0);
    m_stopState = ESCARGOT_DEBUGGER_IN_WAIT_MODE;

    while (processEvents(state, byteCodeBlock))
        ;

    m_activeObjects.clear();
    m_delay = ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY;
}

void DebuggerRemote::byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock)
{
    // All messages which involves this pointer should be ignored until the confirmation arrives.
    if (enabled()) {
        m_releasedFunctions.push_back(reinterpret_cast<uintptr_t>(byteCodeBlock));
        sendPointer(ESCARGOT_MESSAGE_RELEASE_FUNCTION, byteCodeBlock);
    }
}

void DebuggerRemote::exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace)
{
    if (!enabled()) {
        return;
    }

    sendType(ESCARGOT_MESSAGE_EXCEPTION);

    if (!enabled()) {
        return;
    }

    sendString(ESCARGOT_MESSAGE_STRING_8BIT, message);

    size_t size = exceptionTrace.size();
    for (size_t i = 0; i < size && enabled(); i++) {
        sendBacktraceInfo(ESCARGOT_MESSAGE_EXCEPTION_BACKTRACE, exceptionTrace[i].byteCodeBlock, exceptionTrace[i].line, exceptionTrace[i].column, UINT32_MAX);
    }
}

void DebuggerRemote::consoleOut(String* output)
{
    if (enabled()) {
        sendType(ESCARGOT_MESSAGE_PRINT);

        if (enabled()) {
            sendString(ESCARGOT_MESSAGE_STRING_8BIT, output);
        }
    }
}

String* DebuggerRemote::getClientSource(String** sourceName)
{
    if (!enabled()) {
        return nullptr;
    }

    sendType(ESCARGOT_DEBUGGER_WAIT_FOR_SOURCE);
    while (processEvents(nullptr, nullptr))
        ;

    if (sourceName) {
        *sourceName = m_clientSourceName;
    }
    String* sourceData = m_clientSourceData;
    m_clientSourceName = nullptr;
    m_clientSourceData = nullptr;
    return sourceData;
}

bool DebuggerRemote::getWaitBeforeExitClient()
{
    sendType(ESCARGOT_DEBUGGER_WAIT_FOR_WAIT_EXIT);
    while (processEvents(nullptr, nullptr))
        ;
    return this->m_exitClient;
}

bool DebuggerRemote::doEval(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, uint8_t* buffer, size_t length)
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
    if (type == ESCARGOT_MESSAGE_EVAL_8BIT || type == ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_8BIT) {
        str = new Latin1String(data, size);
    } else if (type == ESCARGOT_MESSAGE_EVAL_16BIT || type == ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_16BIT) {
        str = new UTF16String((char16_t*)data, size / 2);
    } else if (type == ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT) {
        char* sourceNameSrc = (char*)memchr(data, '\0', size);
        if (!sourceNameSrc) {
            goto error;
        }
        uint32_t sourceNameLen = sourceNameSrc - data;
        m_clientSourceName = new Latin1String(data, sourceNameLen);
        m_clientSourceData = new Latin1String(data + sourceNameLen + 1, size - sourceNameLen - 1);
        return false;
    } else if (type == ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT) {
        char16_t* str = (char16_t*)data;
        uint32_t sourceNameLen = 0;
        size = size / 2;
        for (; sourceNameLen != size; sourceNameLen++) {
            if (str[sourceNameLen] == '\0') {
                break;
            }
        }
        if (sourceNameLen == size) {
            goto error;
        }
        m_clientSourceName = new UTF16String(str, sourceNameLen);
        m_clientSourceData = new UTF16String(str + sourceNameLen + 1, size - sourceNameLen - 1);
        return false;
    }

    m_stopState = ESCARGOT_DEBUGGER_IN_EVAL_MODE;

    try {
        Value asValue(str);
        Value result(Value::ForceUninitialized);
        if (type == ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_8BIT || type == ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_16BIT) {
            result = state->context()->globalObject()->eval(*state, asValue);
        } else {
            result = state->context()->globalObject()->evalLocal(*state, asValue, state->thisValue(), byteCodeBlock->m_codeBlock, true);
        }
        type = ESCARGOT_MESSAGE_EVAL_RESULT_8BIT;
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

void DebuggerRemote::getBacktrace(ExecutionState* state, uint32_t minDepth, uint32_t maxDepth, bool getTotal)
{
    SandBox::StackTraceDataVector stackTraceData;

    bool hasSavedStackTrace = SandBox::createStackTraceData(stackTraceData, *state, true);

    uint32_t size = (uint32_t)stackTraceData.size();
    uint32_t total = 0;

    for (uint32_t i = 0; i < size; i++) {
        if ((size_t)stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
            total++;
        }
    }

    if (hasSavedStackTrace) {
        total += (uint32_t)activeSavedStackTrace()->size();
    }

    if (getTotal && !send(ESCARGOT_MESSAGE_BACKTRACE_TOTAL, &total, sizeof(uint32_t))) {
        return;
    }

    if (maxDepth == 0 || maxDepth > total) {
        maxDepth = total;
    }

    if (minDepth >= total) {
        minDepth = total;
    }

    ByteCodeLOCDataMap locMap;
    uint32_t counter = 0;

    for (uint32_t i = 0; i < size && counter < maxDepth; i++) {
        if ((size_t)stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
            if (++counter <= minDepth) {
                continue;
            }

            ByteCodeBlock* byteCodeBlock = stackTraceData[i].second.loc.actualCodeBlock;
            uint32_t line, column;

            if ((size_t)stackTraceData[i].second.loc.index == SIZE_MAX) {
                size_t byteCodePosition = stackTraceData[i].second.loc.byteCodePosition;

                ByteCodeLOCData* locData;
                auto iterMap = locMap.find(byteCodeBlock);
                if (iterMap == locMap.end()) {
                    locData = new ByteCodeLOCData();
                    locMap.insert(std::make_pair(byteCodeBlock, locData));
                } else {
                    locData = iterMap->second;
                }

                ExtendedNodeLOC loc = byteCodeBlock->computeNodeLOCFromByteCode(state->context(), byteCodePosition, byteCodeBlock->m_codeBlock, locData);
                line = (uint32_t)loc.line;
                column = (uint32_t)loc.column;
            } else {
                line = (uint32_t)stackTraceData[i].second.loc.line;
                column = (uint32_t)stackTraceData[i].second.loc.column;
            }

            sendBacktraceInfo(ESCARGOT_MESSAGE_BACKTRACE, byteCodeBlock, line, column, (uint32_t)stackTraceData[i].second.executionStateDepth);

            if (!enabled()) {
                return;
            }
        }
    }

    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }

    if (hasSavedStackTrace) {
        SavedStackTraceData* savedStackTracePtr = activeSavedStackTrace()->begin();
        SavedStackTraceData* savedStackTraceEnd = activeSavedStackTrace()->end();

        while (counter < maxDepth && savedStackTracePtr < savedStackTraceEnd) {
            if (++counter <= minDepth) {
                continue;
            }

            sendBacktraceInfo(ESCARGOT_MESSAGE_BACKTRACE, savedStackTracePtr->byteCodeBlock, savedStackTracePtr->line, savedStackTracePtr->column, UINT32_MAX);
            savedStackTracePtr++;
        }
    }

    sendType(ESCARGOT_MESSAGE_BACKTRACE_END);
}

void DebuggerRemote::getScopeChain(ExecutionState* state, uint32_t stateIndex)
{
    const size_t maxMessageLength = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH - 1;
    uint8_t buffer[maxMessageLength];
    size_t nextScope = 0;

    while (stateIndex > 0) {
        state = state->parent();
        stateIndex--;

        if (!state) {
            send(ESCARGOT_MESSAGE_SCOPE_CHAIN_END, buffer, nextScope);
            return;
        }
    }

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
            } else if (record->isModuleEnvironmentRecord()) {
                type = ESCARGOT_RECORD_MODULE_ENVIRONMENT;
            } else {
                type = ESCARGOT_RECORD_DECLARATIVE_ENVIRONMENT;
            }
        } else if (record->isObjectEnvironmentRecord()) {
            type = ESCARGOT_RECORD_OBJECT_ENVIRONMENT;
        } else {
            type = ESCARGOT_RECORD_UNKNOWN_ENVIRONMENT;
        }

        buffer[nextScope++] = type;
        lexEnv = lexEnv->outerEnvironment();
    }

    send(ESCARGOT_MESSAGE_SCOPE_CHAIN_END, buffer, nextScope);
}

static void sendProperty(DebuggerRemote* debugger, ExecutionState* state, AtomicString name, Value value)
{
    uint8_t type = DebuggerRemote::ESCARGOT_VARIABLE_UNDEFINED;
    StringView* valueView = nullptr;

    if (value.isNull()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_NULL;
    } else if (value.isTrue()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_TRUE;
    } else if (value.isFalse()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_FALSE;
    } else if (value.isNumber()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_NUMBER;

        String* valueString = value.toString(*state);
        valueView = new StringView(valueString);
    } else if (value.isString() || value.isSymbol() || value.isBigInt()) {
        String* valueString;
        if (value.isString()) {
            type = DebuggerRemote::ESCARGOT_VARIABLE_STRING;
            valueString = value.asString();
        } else if (value.isBigInt()) {
            type = DebuggerRemote::ESCARGOT_VARIABLE_BIGINT;
            valueString = value.asBigInt()->toString();
        } else {
            type = DebuggerRemote::ESCARGOT_VARIABLE_SYMBOL;
            Symbol* symbol = value.asSymbol();

            valueString = String::emptyString;
            if (symbol->description().hasValue()) {
                valueString = symbol->description().value();
            }
        }
        size_t valueLength = valueString->length();

        if (valueLength >= ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH) {
            type |= DebuggerRemote::ESCARGOT_VARIABLE_LONG_VALUE;
            valueLength = ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH;
        }

        valueView = new StringView(valueString, 0, valueLength);
    } else if (value.isFunction()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_FUNCTION;
    } else if (value.isObject()) {
        type = DebuggerRemote::ESCARGOT_VARIABLE_OBJECT;

        Object* valueObject = value.asObject();

        if (valueObject->isArrayObject()) {
            type = DebuggerRemote::ESCARGOT_VARIABLE_ARRAY;
        }
    }

    size_t nameLength = name.string()->length();

    if (nameLength > ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH) {
        type |= DebuggerRemote::ESCARGOT_VARIABLE_LONG_NAME;
        nameLength = ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH;
    }

    if ((type & DebuggerRemote::ESCARGOT_VARIABLE_TYPE_MASK) < DebuggerRemote::ESCARGOT_VARIABLE_OBJECT) {
        debugger->sendSubtype(DebuggerRemote::ESCARGOT_MESSAGE_VARIABLE, type);
    } else {
        debugger->sendVariableObjectInfo(type, value.asObject());
    }

    if (debugger->connected()) {
        StringView* nameView = new StringView(name.string(), 0, nameLength);
        debugger->sendString(DebuggerRemote::ESCARGOT_MESSAGE_STRING_8BIT, nameView);
    }

    if (valueView && debugger->connected()) {
        debugger->sendString(DebuggerRemote::ESCARGOT_MESSAGE_STRING_8BIT, valueView);
    }
}

static void sendUnaccessibleProperty(DebuggerRemote* debugger, AtomicString name)
{
    uint8_t type = DebuggerRemote::ESCARGOT_VARIABLE_UNACCESSIBLE;
    size_t nameLength = name.string()->length();

    if (nameLength > ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH) {
        type |= DebuggerRemote::ESCARGOT_VARIABLE_LONG_NAME;
        nameLength = ESCARGOT_DEBUGGER_MAX_VARIABLE_LENGTH;
    }

    debugger->sendSubtype(DebuggerRemote::ESCARGOT_MESSAGE_VARIABLE, type);

    if (debugger->connected()) {
        StringView* nameView = new StringView(name.string(), 0, nameLength);
        debugger->sendString(DebuggerRemote::ESCARGOT_MESSAGE_STRING_8BIT, nameView);
    }
}

static void sendObjectProperties(DebuggerRemote* debugger, ExecutionState* state, Object* object)
{
    Object::OwnPropertyKeyVector keys = object->ownPropertyKeys(*state);
    size_t size = keys.size();

    for (size_t i = 0; i < size; i++) {
        ObjectPropertyName propertyName(*state, keys[i]);
        AtomicString name(*state, keys[i].toStringWithoutException(*state));

        try {
            ObjectGetResult result = object->getOwnProperty(*state, propertyName);

            sendProperty(debugger, state, name, result.value(*state, Value(object)));
        } catch (const Value& val) {
            sendUnaccessibleProperty(debugger, name);
        }
    }
}

static void sendRecordProperties(DebuggerRemote* debugger, ExecutionState* state, IdentifierRecordVector& identifiers, EnvironmentRecord* record)
{
    size_t size = identifiers.size();

    for (size_t i = 0; i < size; i++) {
        AtomicString name = identifiers[i].m_name;

        try {
            EnvironmentRecord::GetBindingValueResult result = record->getBindingValue(*state, name);
            ASSERT(result.m_hasBindingValue);
            sendProperty(debugger, state, name, result.m_value);
        } catch (const Value& val) {
            sendUnaccessibleProperty(debugger, name);
        }
    }
}

static void sendRecordProperties(DebuggerRemote* debugger, ExecutionState* state, const ModuleEnvironmentRecord::ModuleBindingRecordVector& bindings, ModuleEnvironmentRecord* record)
{
    size_t size = bindings.size();

    for (size_t i = 0; i < size; i++) {
        AtomicString name = bindings[i].m_localName;

        try {
            EnvironmentRecord::GetBindingValueResult result = record->getBindingValue(*state, name);
            ASSERT(result.m_hasBindingValue);
            sendProperty(debugger, state, name, result.m_value);
        } catch (const Value& val) {
            sendUnaccessibleProperty(debugger, name);
        }
    }
}

void DebuggerRemote::getScopeVariables(ExecutionState* state, uint32_t stateIndex, uint32_t index)
{
    while (stateIndex > 0) {
        state = state->parent();
        stateIndex--;

        if (!state) {
            sendSubtype(ESCARGOT_MESSAGE_VARIABLE, ESCARGOT_VARIABLE_END);
            return;
        }
    }

    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv && index > 0) {
        lexEnv = lexEnv->outerEnvironment();
        index--;
    }

    if (!lexEnv) {
        sendSubtype(ESCARGOT_MESSAGE_VARIABLE, ESCARGOT_VARIABLE_END);
        return;
    }

    EnvironmentRecord* record = lexEnv->record();
    if (record->isGlobalEnvironmentRecord()) {
        GlobalEnvironmentRecord* global = record->asGlobalEnvironmentRecord();

        sendRecordProperties(this, state, *global->m_globalDeclarativeRecord, record);
        sendObjectProperties(this, state, global->m_globalObject);
    } else if (record->isDeclarativeEnvironmentRecord()) {
        DeclarativeEnvironmentRecord* declarativeRecord = record->asDeclarativeEnvironmentRecord();
        if (declarativeRecord->isFunctionEnvironmentRecord()) {
            IdentifierRecordVector* identifierRecordVector = declarativeRecord->asFunctionEnvironmentRecord()->getRecordVector();

            if (identifierRecordVector != NULL) {
                sendRecordProperties(this, state, *identifierRecordVector, record);
            }
        } else if (record->isModuleEnvironmentRecord()) {
            sendRecordProperties(this, state, record->asModuleEnvironmentRecord()->moduleBindings(), record->asModuleEnvironmentRecord());
        } else if (declarativeRecord->isDeclarativeEnvironmentRecordNotIndexed()) {
            sendRecordProperties(this, state, declarativeRecord->asDeclarativeEnvironmentRecordNotIndexed()->m_recordVector, record);
        }
    } else if (record->isObjectEnvironmentRecord()) {
        sendObjectProperties(this, state, record->asObjectEnvironmentRecord()->bindingObject());
    }

    sendSubtype(ESCARGOT_MESSAGE_VARIABLE, ESCARGOT_VARIABLE_END);
}

static LexicalEnvironment* getFunctionLexEnv(ExecutionState* state)
{
    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv) {
        EnvironmentRecord* record = lexEnv->record();

        if (record->isDeclarativeEnvironmentRecord()
            && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            return lexEnv;
        }

        lexEnv = lexEnv->outerEnvironment();
    }
    return nullptr;
}

bool DebuggerRemote::processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest)
{
    uint8_t buffer[ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH];
    size_t length;

    while (true) {
        if (isBlockingRequest) {
            if (!receive(buffer, length)) {
                break;
            }
        } else {
            if (isThereAnyEvent()) {
                if (!receive(buffer, length)) {
                    break;
                }
            } else {
                return false;
            }
        }
        switch (buffer[0]) {
        case ESCARGOT_DEBUGGER_CLIENT_SOURCE_8BIT_START:
        case ESCARGOT_DEBUGGER_CLIENT_SOURCE_16BIT_START: {
            if ((length <= 1 + sizeof(uint32_t) || state != nullptr || byteCodeBlock)) {
                break;
            }

            return doEval(state, byteCodeBlock, buffer, length);
        }
        case ESCARGOT_DEBUGGER_THERE_WAS_NO_SOURCE: {
            if (length != 1 || state != nullptr || byteCodeBlock) {
                break;
            }
            return false;
        }
        case ESCARGOT_DEBUGGER_WAIT_BEFORE_EXIT: {
            m_exitClient = buffer[1];
            return false;
        }

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
                if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointDisabledOpcode]) {
                    break;
                }
                breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointEnabledOpcode];
            } else {
                if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointEnabledOpcode]) {
                    break;
                }
                breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointDisabledOpcode];
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
        case ESCARGOT_MESSAGE_FINISH: {
            if (length != 1 || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            LexicalEnvironment* lexEnv = getFunctionLexEnv(state);

            if (!lexEnv) {
                m_stopState = nullptr;
                return false;
            }

            ExecutionState* stopState = state->parent();

            while (stopState && getFunctionLexEnv(stopState) == lexEnv) {
                stopState = stopState->parent();
            }

            m_stopState = stopState;
            return false;
        }
        case ESCARGOT_MESSAGE_EVAL_8BIT_START:
        case ESCARGOT_MESSAGE_EVAL_16BIT_START: {
            if ((length <= 1 + sizeof(uint32_t)) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }
            ASSERT(byteCodeBlock.hasValue());
            return doEval(state, byteCodeBlock, buffer, length);
        }
        case ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_8BIT_START:
        case ESCARGOT_MESSAGE_EVAL_WITHOUT_STOP_16BIT_START: {
            if ((length <= 1 + sizeof(uint32_t)) || (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE && m_stopState != ESCARGOT_DEBUGGER_ALWAYS_STOP)) {
                break;
            }
            ASSERT(!byteCodeBlock.hasValue());
            return doEval(state, nullptr, buffer, length);
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
            if ((length != 1 + sizeof(uint32_t)) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            uint32_t stateIndex;
            memcpy(&stateIndex, buffer + 1, sizeof(uint32_t));

            getScopeChain(state, stateIndex);
            return true;
        }
        case ESCARGOT_MESSAGE_GET_SCOPE_VARIABLES: {
            if ((length != 1 + sizeof(uint32_t) + sizeof(uint32_t)) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            uint32_t stateIndex;
            uint32_t index;
            memcpy(&stateIndex, buffer + 1, sizeof(uint32_t));
            memcpy(&index, buffer + 1 + sizeof(uint32_t), sizeof(uint32_t));

            getScopeVariables(state, stateIndex, index);
            return true;
        }
        case ESCARGOT_MESSAGE_GET_OBJECT: {
            if (length != 1 + sizeof(uint32_t) || m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
                break;
            }

            uint32_t index;
            memcpy(&index, buffer + 1, sizeof(uint32_t));

            if (index < m_activeObjects.size()) {
                sendObjectProperties(this, state, m_activeObjects[index]);
            }

            sendSubtype(ESCARGOT_MESSAGE_VARIABLE, ESCARGOT_VARIABLE_END);
            return true;
        }
        case ESCARGOT_DEBUGGER_PENDING_CONFIG: {
            if (length != 2) {
                break;
            }
            m_pendingWait = buffer[1];
            return true;
        }
        case ESCARGOT_DEBUGGER_PENDING_RESUME: {
            if (!m_waitForResume || length != 1) {
                break;
            }
            m_waitForResume = false;
            return true;
        }
        }

        ESCARGOT_LOG_ERROR("Invalid message received. Closing connection.\n");
        close();
        return false;
    }
    return enabled();
}

void DebuggerRemote::waitForResolvingPendingBreakpoints()
{
    m_waitForResume = true;
    sendType(ESCARGOT_DEBUGGER_WAITING_AFTER_PENDING);
    while (m_waitForResume) {
        processEvents(nullptr, nullptr);

        if (!enabled()) {
            break;
        }
    }
}

DebuggerRemote::SavedStackTraceDataVector* Debugger::saveStackTrace(ExecutionState& state)
{
    SavedStackTraceDataVector* savedStackTrace = new SavedStackTraceDataVector();
    SandBox::StackTraceDataVector stackTraceData;
    ByteCodeLOCDataMap locMap;
    uint32_t counter = 0;

    bool hasSavedStackTrace = SandBox::createStackTraceData(stackTraceData, state, true);
    uint32_t total = (uint32_t)stackTraceData.size();

    for (uint32_t i = 0; i < total && counter < ESCARGOT_DEBUGGER_MAX_STACK_TRACE_LENGTH; i++) {
        if ((size_t)stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
            ByteCodeBlock* byteCodeBlock = stackTraceData[i].second.loc.actualCodeBlock;
            uint32_t line, column;

            counter++;

            if ((size_t)stackTraceData[i].second.loc.index == SIZE_MAX) {
                size_t byteCodePosition = stackTraceData[i].second.loc.byteCodePosition;

                ByteCodeLOCData* locData;
                auto iterMap = locMap.find(byteCodeBlock);
                if (iterMap == locMap.end()) {
                    locData = new ByteCodeLOCData();
                    locMap.insert(std::make_pair(byteCodeBlock, locData));
                } else {
                    locData = iterMap->second;
                }

                ExtendedNodeLOC loc = byteCodeBlock->computeNodeLOCFromByteCode(state.context(), byteCodePosition, byteCodeBlock->m_codeBlock, locData);

                line = (uint32_t)loc.line;
                column = (uint32_t)loc.column;
            } else {
                line = (uint32_t)stackTraceData[i].second.loc.line;
                column = (uint32_t)stackTraceData[i].second.loc.column;
            }

            savedStackTrace->push_back(SavedStackTraceData(byteCodeBlock, line, column));
        }
    }

    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }

    if (hasSavedStackTrace) {
        Debugger* debugger = state.context()->debugger();
        SavedStackTraceData* savedStackTracePtr = debugger->activeSavedStackTrace()->begin();
        SavedStackTraceData* savedStackTraceEnd = debugger->activeSavedStackTrace()->end();

        while (counter < ESCARGOT_DEBUGGER_MAX_STACK_TRACE_LENGTH && savedStackTracePtr < savedStackTraceEnd) {
            savedStackTrace->push_back(SavedStackTraceData(savedStackTracePtr->byteCodeBlock, savedStackTracePtr->line, savedStackTracePtr->column));
            savedStackTracePtr++;
            counter++;
        }
    }

    return savedStackTrace;
}

void Debugger::pumpDebuggerEvents(ExecutionState* state)
{
    while (processEvents(state, nullptr, false))
        ;
}

void DebuggerRemote::init(const char*, Context*)
{
    union {
        uint16_t u16Value;
        uint8_t u8Value;
    } endian;

    endian.u16Value = 1;

    MessageVersion version;
    version.littleEndian = (endian.u8Value == 1);

    uint32_t debuggerVersion = ESCARGOT_DEBUGGER_VERSION;
    memcpy(version.version, &debuggerVersion, sizeof(uint32_t));

    if (!send(ESCARGOT_MESSAGE_VERSION, &version, sizeof(version))) {
        return;
    }

    MessageConfiguration configuration;

    configuration.maxMessageSize = ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH;
    configuration.pointerSize = (uint8_t)sizeof(void*);

    send(ESCARGOT_MESSAGE_CONFIGURATION, &configuration, sizeof(configuration));
}

void Debugger::createDebuggerRemote(const char* options, Context* context)
{
    DebuggerRemote* debugger = new DebuggerTcp();

    debugger->init(options, context);
}

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */
