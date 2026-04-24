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
#include <memory>
#include <string>
#include <stdexcept>

#include "Escargot.h"
#include "DebuggerTcp.h"
#include "DebuggerDevtools.h"
#include "DebuggerHttpRouter.h"
#include "DebuggerDevtoolsMessageBuilder.h"
#include "interpreter/ByteCode.h"
#include "parser/Script.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#ifdef ESCARGOT_DEBUGGER

namespace Escargot {

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
    const int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    const auto size = static_cast<size_t>(size_s);
    const std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return { buf.get(), buf.get() + size - 1 }; // We don't want the '\0' inside
}


bool DebuggerDevtools::sendMessage(const std::string& msg, const size_t length)
{
    if (UNLIKELY(!m_networkEnabled || !m_debuggerEnabled || !m_runtimeEnabled)) {
        m_pendingMessages.emplace_back(msg);
        return true;
    }

    const bool result = send(0, msg.c_str(), length == static_cast<size_t>(-1) ? msg.length() : length);
    if (result) {
        ESCARGOT_LOG_INFO("Sent message: %s\n", msg.c_str());
    } else {
        ESCARGOT_LOG_ERROR("Error sending message!");
    }
    return result;
}

bool DebuggerDevtools::sendJSONDocument(const rapidjson::Document& document)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    document.Accept(writer);
    const char* jsonReplyString = sb.GetString();

    return sendMessage(jsonReplyString);
}

void DebuggerDevtools::init(const char* options, Context* context)
{
    // ESCARGOT_LOG_INFO("Implement this: DebuggerDevtools::init\n");

    const char* msg = "{\"method\":\"Runtime.executionContextCreated\",\"params\":{"
                      "\"context\":{"
                      "\"id\":1,"
                      "\"origin\":\"\","
                      "\"name\":\"escargot\","
                      "\"auxData\":{\"isDefault\":true}"
                      "}"
                      "}}";

    sendMessage(msg);
}

bool DebuggerDevtools::skipSourceCode(String* srcName) const
{
    ESCARGOT_LOG_INFO("Implement this: DebuggerDevtools::skipSourceCode\n");
    return false;
}

uint8_t DebuggerDevtools::registerScript(String* source, String* srcName)
{
    std::string url(reinterpret_cast<const char*>(srcName->characters8()), srcName->length());

    auto it = m_scriptIdByUrl.find(url);
    if (it != m_scriptIdByUrl.end()) {
        return it->second;
    }

    uint8_t newId = m_nextScriptId++;

    m_scriptsById.emplace(newId, ScriptInfo{ newId, srcName, source });
    m_scriptIdByUrl.emplace(url, newId);

    return newId;
}

static void computeEndLocation(const LChar* src, size_t length, uint32_t& endLine, uint32_t& endColumn)
{
    for (size_t i = 0; i < length; i++) {
        if (src[i] == '\n') {
            endLine++;
            endColumn = 0;
        } else {
            endColumn++;
        }
    }
}

void DebuggerDevtools::parseCompleted(String* source, String* srcName, const size_t originLineOffset, String* error)
{
    if (!enabled()) {
        return;
    }

    if (!source || !srcName || !source->is8Bit() || !srcName->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return;
    }

    const uint8_t scriptId = registerScript(source, srcName);
    std::set<BreakpointByteCodeLocation, decltype(compareBreakpointLocations)*> breakpointLocationsSet(compareBreakpointLocations);
    m_breakpointInfo.emplace(scriptId, breakpointLocationsSet);

    const size_t breakpointLocationsSize = m_breakpointLocationsVector.size();

    if (originLineOffset > 0) {
        for (size_t i = 0; i < breakpointLocationsSize; i++) {
            // adjust line offset for manipulated source code
            // inserted breakpoint's line info should be bigger than `originLineOffset`
            BreakpointLocationVector& locationVector = m_breakpointLocationsVector[i]->breakpointLocations;
            for (auto& j : locationVector) {
                ASSERT(j.line > originLineOffset);
                j.line -= originLineOffset;
            }
        }
    }

    for (size_t i = 0; i < breakpointLocationsSize; i++) {
        /* function bytecode information */
        InterpretedCodeBlock* codeBlock = reinterpret_cast<InterpretedCodeBlock*>(m_breakpointLocationsVector[i]->weakCodeRef);
        uint8_t* byteCodeStart = codeBlock->byteCodeBlock()->m_code.data();

        /* save breakpoint locations. */
        BreakpointLocationVector breakpointLocations = m_breakpointLocationsVector[i]->breakpointLocations;
        for (auto& breakpointLocation : breakpointLocations) {
            auto b = BreakpointByteCodeLocation(breakpointLocation.line, reinterpret_cast<ByteCode*>(byteCodeStart + breakpointLocation.offset));
            b.byteCode->m_loc.line = breakpointLocation.line;
            m_breakpointInfo[scriptId].insert(b);
        }
    }

    rapidjson::Document reply;
    reply.SetObject();

    rapidjson::Value method(rapidjson::kStringType);
    rapidjson::Value paramsObject(rapidjson::kObjectType);
    rapidjson::Value resolvedBreakpointsArray(rapidjson::kArrayType);

    reply.AddMember("method", "Debugger.scriptParsed", reply.GetAllocator());
    reply.AddMember("params", paramsObject, reply.GetAllocator());

    rapidjson::Value scriptIdValue;
    std::string scriptIdString = string_format("%d", scriptId);
    scriptIdValue.SetString(scriptIdString.c_str(), scriptIdString.length(), reply.GetAllocator());
    reply["params"].AddMember("scriptId", scriptIdValue, reply.GetAllocator());

    rapidjson::Value urlString;
    std::string url = reinterpret_cast<const char*>(srcName->characters8());
    urlString.SetString(url.c_str(), url.length(), reply.GetAllocator());
    reply["params"].AddMember("url", urlString, reply.GetAllocator());

    reply["params"].AddMember("startLine", 0, reply.GetAllocator());
    reply["params"].AddMember("startColumn", 0, reply.GetAllocator());

    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    computeEndLocation(source->characters8(), source->length(), endLine, endColumn);
    reply["params"].AddMember("endLine", endLine, reply.GetAllocator());
    reply["params"].AddMember("endColumn", endColumn, reply.GetAllocator());

    reply["params"].AddMember("executionContextId", 1, reply.GetAllocator());

    sendJSONDocument(reply);
}

void DebuggerDevtools::sendPausedEvent(ByteCodeBlock* byteCodeBlock, const uint32_t offset, ExecutionState* state, const bool breakpoint)
{
    const auto* byteCode = reinterpret_cast<ByteCode*>(byteCodeBlock->m_code.data() + offset);
    const auto* filename = reinterpret_cast<const char*>(byteCodeBlock->codeBlock()->script()->srcName()->characters8());
    const uint8_t scripId = m_scriptIdByUrl[reinterpret_cast<const char*>(byteCodeBlock->codeBlock()->script()->srcName()->characters8())];

    const uint64_t line = byteCode->m_loc.line - 1; // chrome starts line indexes at 0
    const uint64_t column = byteCode->m_loc.column - 1; // chrome starts column indexes at 0

    // TODO: some placeholder info
    const std::string msg = string_format("{\"method\":\"Debugger.paused\","
                                          "\"params\":{"
                                          "\"callFrames\":[{"
                                          "\"callFrameId\":\"frame:0\","
                                          "\"functionName\":\"%s\","
                                          "\"location\":{"
                                          "\"scriptId\":\"%d\","
                                          "\"lineNumber\":%lu,"
                                          "\"columnNumber\":%lu"
                                          "},"
                                          "\"url\":\"%s\","
                                          "\"scopeChain\":[{"
                                          "\"type\":\"global\","
                                          "\"object\":{"
                                          "\"type\":\"object\","
                                          "\"className\":\"global\","
                                          "\"description\":\"global\","
                                          "\"objectId\":\"global:1\""
                                          "}"
                                          "}],"
                                          "\"this\":{"
                                          "\"type\":\"undefined\""
                                          "}"
                                          "}],"
                                          "\"reason\":\"%s\","
                                          "\"hitBreakpoints\":[\"%s:%lu:%lu\"]"
                                          "}"
                                          "}",
                                          reinterpret_cast<const char*>(byteCodeBlock->codeBlock()->functionName().string()->characters8()),
                                          scripId,
                                          line,
                                          column,
                                          filename,
                                          breakpoint ? "Breakpoint" : "Break on start",
                                          filename,
                                          line,
                                          column);

    sendMessage(msg, msg.length());
}

void DebuggerDevtools::stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
{
    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        m_delay--;
        if (m_delay == 0) {
            processEvents(state, byteCodeBlock);
        }
        return;
    }

    sendPausedEvent(byteCodeBlock, offset, state, !m_startBreakpoint);

    if (m_startBreakpoint) {
        m_startBreakpoint = false;
    }

    if (!enabled()) {
        return;
    }

    ASSERT(m_activeObjects.empty());
    m_stopState = ESCARGOT_DEBUGGER_IN_WAIT_MODE;

    while (processEvents(state, byteCodeBlock))
        ;

    m_activeObjects.clear();
    m_delay = ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY;
}

void DebuggerDevtools::byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock)
{
}

void DebuggerDevtools::exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace)
{
}

void DebuggerDevtools::consoleOut(String* output)
{
}

String* DebuggerDevtools::getClientSource(String** sourceName)
{
    return nullptr;
}

bool DebuggerDevtools::getWaitBeforeExitClient()
{
    while (processEvents(nullptr, nullptr))
        ;
    return true;
}

template <size_t N>
constexpr MessageType messageType(const char (&methodName)[N], const MessageHandler handler)
{
    return MessageType{ methodName, handler };
}

bool DebuggerDevtools::resume(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    replyOK(jsonMessage);

    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }
    m_stopState = nullptr;
    return false;
}

bool DebuggerDevtools::stepInto(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    replyOK(jsonMessage);

    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        return true;
    }
    m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;
    return false;
}

bool DebuggerDevtools::stepOver(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    replyOK(jsonMessage);

    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }
    m_stopState = state;
    return false;
}

bool DebuggerDevtools::stepOut(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    replyOK(jsonMessage);

    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }

    const LexicalEnvironment* lexEnv = getFunctionLexEnv(state);

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

bool DebuggerDevtools::sendProperties(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    // TODO: placeholder info
    const std::string msg = string_format("{\"id\":%u,\"result\":{"
                                          "\"result\":["
                                          "{"
                                          "\"name\":\"print\","
                                          "\"configurable\":true,"
                                          "\"enumerable\":true,"
                                          "\"value\":{"
                                          "\"type\":\"function\","
                                          "\"description\":\"function print()\""
                                          "}"
                                          "},"
                                          "{"
                                          "\"name\":\"c\","
                                          "\"configurable\":true,"
                                          "\"enumerable\":true,"
                                          "\"value\":{"
                                          "\"type\":\"number\","
                                          "\"value\":8,"
                                          "\"description\":\"8\""
                                          "}"
                                          "},"
                                          "{"
                                          "\"name\":\"globalVar\","
                                          "\"configurable\":true,"
                                          "\"enumerable\":true,"
                                          "\"value\":{"
                                          "\"type\":\"string\","
                                          "\"value\":\"escargot\","
                                          "\"description\":\"escargot\""
                                          "}"
                                          "}"
                                          "]"
                                          "}}",
                                          jsonMessage["id"].GetUint());

    sendMessage(msg, msg.length());

    return true;
}

bool DebuggerDevtools::sendSourceCode(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    const uint32_t requestId = jsonMessage["id"].GetUint();
    const uint32_t scriptId = std::stoi(jsonMessage["params"]["scriptId"].GetString());

    const auto it = m_scriptsById.find(scriptId);
    if (it == m_scriptsById.end()) {
        return false;
    }

    const String* source = it->second.source;

    if (!source->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return false;
    }

    const std::string message = DebuggerDevtoolsMessageBuilder::buildSourceCodeMessage(requestId, source);
    return sendMessage(message);
}

bool DebuggerDevtools::replyOK(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    const std::string jsonReplyString = string_format(R"({"id": %d, "result": {}})",
                                                      jsonMessage["id"].GetInt());

    sendMessage(jsonReplyString, jsonReplyString.length());
    return true;
}

bool DebuggerDevtools::enableNetwork(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    this->m_networkEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableDebugger(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    this->m_debuggerEnabled = true;

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"], reply.GetAllocator());

    rapidjson::Value result(rapidjson::kObjectType);
    reply.AddMember("result", result, reply.GetAllocator());

    rapidjson::Value debuggerIdValue;
    std::string debuggerIdString = string_format("escargot-%d", rand() % 1000000);
    debuggerIdValue.SetString(debuggerIdString.c_str(), debuggerIdString.length(), reply.GetAllocator());

    reply["result"].AddMember("debuggerId", debuggerIdValue, reply.GetAllocator());

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::enableRuntime(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    this->m_runtimeEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableProfiler(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    this->m_profilerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::setPauseOnExceptions(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    // TODO: handdle other parameters: state: none (unset), uncaught, caught, all
    this->m_pauseOnExceptions = true;
    return replyOK(jsonMessage);
}

void setBreakpointState(ByteCode* breakpoint, const bool enabled)
{
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
    if (enabled) {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointDisabledOpcode]) {
            return;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointEnabledOpcode];
    } else {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointEnabledOpcode]) {
            return;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointDisabledOpcode];
    }
#else
    if (enabled) {
        if (breakpoint->m_opcode != BreakpointDisabledOpcode) {
            return;
        }
        breakpoint->m_opcode = BreakpointEnabledOpcode;
    } else {
        if (breakpoint->m_opcode != BreakpointEnabledOpcode) {
            return;
        }
        breakpoint->m_opcode = BreakpointDisabledOpcode;
    }
#endif
}

bool DebuggerDevtools::setBreakpointsActive(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    this->m_breakpointsActive = jsonMessage["params"]["active"].GetBool();

    for (ByteCode* breakpointBytecode : m_setBreakPoints) {
        setBreakpointState(breakpointBytecode, m_breakpointsActive);
    }

    return replyOK(jsonMessage);
}

bool DebuggerDevtools::setBreakpointByUrl(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    const std::string breakpointCondition = jsonMessage["params"]["condition"].GetString();
    if (!breakpointCondition.empty()) {
        ESCARGOT_LOG_ERROR("Warning: Breakpoint conditions are not supported!");
    }

    std::string breakpointFile = jsonMessage["params"]["url"].GetString();
    if (breakpointFile.find("file://") == 0) {
        breakpointFile.erase(0, 7);
    }
    const uint8_t scriptId = this->m_scriptIdByUrl[breakpointFile];
    std::string scriptIdString = string_format("%d", scriptId);

    const uint32_t lineNumber = jsonMessage["params"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t columnNumber = jsonMessage["params"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0

    rapidjson::Document reply;
    reply.SetObject();

    rapidjson::Value resultObject(rapidjson::kObjectType);
    rapidjson::Value resultArray(rapidjson::kArrayType);
    rapidjson::Value breakpointID(rapidjson::kStringType);

    reply.AddMember("id", jsonMessage["id"].GetInt(), reply.GetAllocator());
    reply.AddMember("result", resultObject, reply.GetAllocator());
    reply["result"].AddMember("locations", resultArray, reply.GetAllocator());

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if ((lineNumber == breakpointInfo.line && (columnNumber == breakpointInfo.byteCode->m_loc.column || columnNumber == 1))
            || (reply["result"]["locations"].Empty() && breakpointInfo.line > lineNumber)) {
            rapidjson::Value locationObject(rapidjson::kObjectType);
            rapidjson::Value scriptIdValue;

            scriptIdValue.SetString(scriptIdString.c_str(), scriptIdString.length(), reply.GetAllocator());

            locationObject.AddMember("scriptId", scriptIdValue, reply.GetAllocator());
            locationObject.AddMember("lineNumber", breakpointInfo.line - 1, reply.GetAllocator());
            locationObject.AddMember("columnNumber", breakpointInfo.byteCode->m_loc.column - 1, reply.GetAllocator());

            reply["result"]["locations"].PushBack(locationObject, reply.GetAllocator());

            std::string breakpointIdString = string_format("%s:%d:%lu", breakpointFile.c_str(), breakpointInfo.line, breakpointInfo.byteCode->m_loc.column);
            breakpointID.SetString(breakpointIdString.c_str(), breakpointIdString.length(), reply.GetAllocator());
            reply["result"].AddMember("breakpointId", breakpointID, reply.GetAllocator());

            /* Enable breakpoint */
            setBreakpointState(breakpointInfo.byteCode, true);
            m_setBreakPoints.emplace(breakpointInfo.byteCode);
            break;
        }
    }

    const auto ret = sendJSONDocument(reply);
    return ret;
}

bool DebuggerDevtools::removeBreakpoint(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    const std::string breakpointId = jsonMessage["params"]["breakpointId"].GetString();

    std::stringstream ss(breakpointId);
    std::string breakpointIdInfo[3];
    constexpr char delim = ':';
    int i = 0;

    while (std::getline(ss, breakpointIdInfo[i], delim)) {
        ++i;
    }
    ASSERT(i == 3);

    const std::string breakpointFile = breakpointIdInfo[0];
    const uint32_t lineNumber = stoi(breakpointIdInfo[1]);
    const uint32_t columnNumber = stoi(breakpointIdInfo[2]);
    const uint8_t scriptId = this->m_scriptIdByUrl[breakpointFile];

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if (lineNumber == breakpointInfo.line && columnNumber == breakpointInfo.byteCode->m_loc.column) {
            /* Disable breakpoint */
            setBreakpointState(breakpointInfo.byteCode, false);
            m_setBreakPoints.erase(breakpointInfo.byteCode);
            break;
        }
    }

    return replyOK(jsonMessage);
}

bool DebuggerDevtools::sendPossibleBreakpoints(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    if (jsonMessage["params"]["restrictToFunction"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: restrictToFunction is not supported\n");
    }

    std::string scriptIdString = jsonMessage["params"]["start"]["scriptId"].GetString();
    const uint8_t scriptId = std::stoi(scriptIdString);

    if (m_breakpointInfo.find(scriptId) == m_breakpointInfo.end()) {
        ESCARGOT_LOG_ERROR("Script Id not found: %d", scriptId);
        return replyOK(jsonMessage);
    }

    if (scriptId != std::stoi(jsonMessage["params"]["end"]["scriptId"].GetString())) {
        ESCARGOT_LOG_ERROR("Error: Script ranges across multiple scripts not supported!\n");
        return replyMethodNotFound(jsonMessage);
    }

    const uint32_t startLine = jsonMessage["params"]["start"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t startColumn = jsonMessage["params"]["start"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0
    const uint32_t endLine = jsonMessage["params"]["end"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t endColumn = jsonMessage["params"]["end"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0

    rapidjson::Document reply;
    reply.SetObject();

    rapidjson::Value resultObject(rapidjson::kObjectType);
    rapidjson::Value resultArray(rapidjson::kArrayType);

    reply.AddMember("id", jsonMessage["id"].GetInt(), reply.GetAllocator());
    reply.AddMember("result", resultObject, reply.GetAllocator());
    reply["result"].AddMember("locations", resultArray, reply.GetAllocator());

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if (((startLine <= breakpointInfo.line && breakpointInfo.line <= endLine)
             && startColumn <= breakpointInfo.byteCode->m_loc.column && breakpointInfo.byteCode->m_loc.column <= endColumn)
            || (reply["result"]["locations"].Empty() && breakpointInfo.line > startLine && breakpointInfo.line > endLine)) {
            rapidjson::Value locationObject(rapidjson::kObjectType);
            rapidjson::Value scriptIdValue;

            scriptIdValue.SetString(scriptIdString.c_str(), scriptIdString.length(), reply.GetAllocator());

            locationObject.AddMember("scriptId", scriptIdValue, reply.GetAllocator());
            locationObject.AddMember("lineNumber", breakpointInfo.line - 1, reply.GetAllocator());
            locationObject.AddMember("columnNumber", breakpointInfo.byteCode->m_loc.column - 1, reply.GetAllocator());

            reply["result"]["locations"].PushBack(locationObject, reply.GetAllocator());
        }
    }

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::replyMethodNotFound(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    const std::string jsonReplyString = string_format(R"({"id":%d,"error":{"code":-32601,"message":"'%s' wasn't found"}})",
                                                      jsonMessage["id"].GetInt(), jsonMessage["method"].GetString());

    sendMessage(jsonReplyString, jsonReplyString.length());

    return true;
}

bool DebuggerDevtools::processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest)
{
    uint8_t buffer[ESCARGOT_WS_MAX_MESSAGE_LENGTH];
    size_t length;

    // NOTE: keep sorted
    static constexpr MessageType messageTypes[] = {
        messageType("Debugger.enable", &DebuggerDevtools::enableDebugger),
        messageType("Debugger.getPossibleBreakpoints", &DebuggerDevtools::sendPossibleBreakpoints),
        messageType("Debugger.getScriptSource", &DebuggerDevtools::sendSourceCode),
        messageType("Debugger.removeBreakpoint", &DebuggerDevtools::removeBreakpoint),
        messageType("Debugger.resume", &DebuggerDevtools::resume),
        messageType("Debugger.setAsyncCallStackDepth", &DebuggerDevtools::replyOK), // we may be able to set something for this one
        messageType("Debugger.setBlackboxPatterns", &DebuggerDevtools::replyOK), // we ignore this for now, but if needed set skipSourceName in DebuggerTcp
        messageType("Debugger.setBreakpointByUrl", &DebuggerDevtools::setBreakpointByUrl),
        messageType("Debugger.setBreakpointsActive", &DebuggerDevtools::setBreakpointsActive),
        messageType("Debugger.setPauseOnExceptions", &DebuggerDevtools::setPauseOnExceptions),
        messageType("Debugger.stepInto", &DebuggerDevtools::stepInto),
        messageType("Debugger.stepOut", &DebuggerDevtools::stepOut),
        messageType("Debugger.stepOver", &DebuggerDevtools::stepOver),
        messageType("Network.clearAcceptedEncodingsOverride", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.emulateNetworkConditionsByRule", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.enable", &DebuggerDevtools::enableNetwork),
        messageType("Network.overrideNetworkState", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.setAttachDebugStack", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.setBlockedURLs", &DebuggerDevtools::replyMethodNotFound),
        messageType("Profiler.enable", &DebuggerDevtools::enableProfiler),
        messageType("Runtime.enable", &DebuggerDevtools::enableRuntime),
        messageType("Runtime.getProperties", &DebuggerDevtools::sendProperties),
        messageType("Runtime.runIfWaitingForDebugger", &DebuggerDevtools::replyOK),
    };

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

        printf("MESSAGE: %.*s\n", static_cast<int>(length), reinterpret_cast<const char*>(buffer));

        rapidjson::Document document;
        document.Parse(reinterpret_cast<const rapidjson::GenericDocument<rapidjson::UTF8<>>::Ch*>(buffer));

        rapidjson::Document jsonMessage;
        if (UNLIKELY(jsonMessage.Parse(reinterpret_cast<const char*>(buffer)).HasParseError())) {
            ESCARGOT_LOG_ERROR("Json Message parsing error: %s at offset: %d\n", GetParseError_En(jsonMessage.GetParseError()), static_cast<int32_t>(jsonMessage.GetErrorOffset()));
            return false;
        }
        const char* methodName = jsonMessage["method"].GetString();
        if (UNLIKELY(methodName == nullptr)) {
            ESCARGOT_LOG_ERROR("Debugger method not provided: %s\n", reinterpret_cast<const char*>(buffer));
            return false;
        }
        const uint32_t methodNameLength = strlen(methodName);

        for (const auto& message : messageTypes) {
            // TODO: it would be better to calculate the lengths of each of the method name strings at compile time instead of using strlen()
            if (!DebuggerHttpRouter::requestStartsWith(reinterpret_cast<const uint8_t*>(methodName), methodNameLength, message.methodName, strlen(message.methodName))) {
                continue;
            }

            return (this->*message.handler)(jsonMessage, state);
        }
        ESCARGOT_LOG_ERROR("Debugger function not supported: %s\n", reinterpret_cast<const char*>(buffer));
        this->replyMethodNotFound(jsonMessage); // maybe reply with not supported instead? if there is such a reply in the spec
    }

    if (!m_pendingMessages.empty() && m_networkEnabled && m_debuggerEnabled && m_runtimeEnabled) {
        for (const std::string& msg : m_pendingMessages) {
            sendMessage(msg);
        }
        m_pendingMessages.clear();
    }

    return enabled();
}

} // namespace Escargot

#endif /* ESCARGOT_DEBUGGER */
