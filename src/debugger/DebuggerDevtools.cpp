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

#include "Escargot.h"
#include "DebuggerTcp.h"
#include "DebuggerDevtools.h"
#include "DebuggerHttpRouter.h"
#include "DebuggerDevtoolsMessageBuilder.h"

#include "interpreter/ByteCode.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#ifdef ESCARGOT_DEBUGGER

namespace Escargot {

bool DebuggerDevtools::sendMessage(const std::string& msg, const int length)
{
    if (UNLIKELY(!m_networkEnabled || !m_debuggerEnabled || !m_runtimeEnabled)) {
        m_pendingMessages.emplace_back(msg);
        return true;
    }

    ESCARGOT_LOG_INFO("Sending message: %s\n", msg.c_str());
    return send(0, msg.c_str(), length == -1 ? msg.length() : length);
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

void DebuggerDevtools::parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error)
{
    if (!enabled()) {
        return;
    }

    if (!source || !srcName || !source->is8Bit() || !srcName->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return;
    }

    uint8_t scriptId = registerScript(source, srcName);

    sendMessage(DebuggerDevtoolsMessageBuilder::buildScriptParsedMessage(scriptId, source, srcName));
}

void DebuggerDevtools::sendPausedEvent(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
{
    // TODO: Placeholder info
    const char* msg = "{\"method\":\"Debugger.paused\","
                      "\"params\":{"
                      "\"callFrames\":[{"
                      "\"callFrameId\":\"frame:0\","
                      "\"functionName\":\"\","
                      "\"location\":{"
                      "\"scriptId\":\"1\","
                      "\"lineNumber\":0,"
                      "\"columnNumber\":0"
                      "},"
                      "\"url\":\"hello.js\","
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
                      "\"reason\":\"breakpoint\","
                      "\"hitBreakpoints\":[\"breakpoint:1\"]"
                      "}"
                      "}";

    sendMessage(msg);
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

    uint8_t* byteCodeStart = byteCodeBlock->m_code.data();
    sendPausedEvent(byteCodeBlock, offset, state);

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

bool DebuggerDevtools::resume(rapidjson::Document& jsonMessage)
{
    replyOK(jsonMessage);

    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }
    m_stopState = nullptr;
    return false;
}

bool DebuggerDevtools::sendProperties(rapidjson::Document& jsonMessage)
{
    uint32_t requestId = jsonMessage["id"].GetUint();

    char buffer[4096];

    // TODO: placeholder info
    snprintf(buffer, sizeof(buffer),
             "{\"id\":%u,\"result\":{"
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
             requestId);

    sendMessage(buffer);

    return true;
}

bool DebuggerDevtools::sendSourceCode(rapidjson::Document& jsonMessage)
{
    const uint32_t requestId = jsonMessage["id"].GetUint();
    const uint32_t scriptId = std::stoi(jsonMessage["params"]["scriptId"].GetString());

    auto it = m_scriptsById.find(scriptId);
    if (it == m_scriptsById.end()) {
        return false;
    }

    String* source = it->second.source;

    if (!source->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return false;
    }

    std::string message = DebuggerDevtoolsMessageBuilder::buildSourceCodeMessage(requestId, source);
    return sendMessage(message);
}

bool DebuggerDevtools::replyOK(rapidjson::Document& jsonMessage)
{
    char reply[32];
    const int jsonReplyStringLength = snprintf(reply, sizeof(reply),
                                               R"({"id": %d, "result": {}})",
                                               jsonMessage["id"].GetInt());

    sendMessage(reply, jsonReplyStringLength);
    return true;
}

bool DebuggerDevtools::enableNetwork(rapidjson::Document& jsonMessage)
{
    this->m_networkEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableDebugger(rapidjson::Document& jsonMessage)
{
    this->m_debuggerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableRuntime(rapidjson::Document& jsonMessage)
{
    this->m_runtimeEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableProfiler(rapidjson::Document& jsonMessage)
{
    this->m_profilerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::setPauseOnExceptions(rapidjson::Document& jsonMessage)
{
    this->m_pauseOnExceptions = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::replyMethodNotFound(rapidjson::Document& jsonMessage)
{
    char reply[256];

    const int jsonReplyStringLength = snprintf(reply, sizeof(reply),
                                               R"({"id":%d,"error":{"code":-32601,"message":"'%s' wasn't found"}})",
                                               jsonMessage["id"].GetInt(),
                                               jsonMessage["method"].GetString());

    sendMessage(reply, jsonReplyStringLength);

    return true;
}

bool DebuggerDevtools::processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest)
{
    uint8_t buffer[ESCARGOT_WS_MAX_MESSAGE_LENGTH];
    size_t length;

    // NOTE: keep sorted
    static constexpr MessageType messageTypes[] = {
        messageType("Debugger.enable", &DebuggerDevtools::enableDebugger),
        messageType("Debugger.getScriptSource", &DebuggerDevtools::sendSourceCode),
        messageType("Debugger.resume", &DebuggerDevtools::resume),
        messageType("Debugger.setAsyncCallStackDepth", &DebuggerDevtools::replyOK), // we may be able to set something for this one
        messageType("Debugger.setBlackboxPatterns", &DebuggerDevtools::replyOK), // we ignore this for now, but if needed set skipSourceName in DebuggerTcp
        messageType("Debugger.setPauseOnExceptions", &DebuggerDevtools::setPauseOnExceptions),
        messageType("Network.clearAcceptedEncodingsOverride", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.emulateNetworkConditionsByRule", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.enable", &DebuggerDevtools::enableNetwork),
        messageType("Network.overrideNetworkState", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.setAttachDebugStack", &DebuggerDevtools::replyMethodNotFound),
        messageType("Network.setBlockedURLs", &DebuggerDevtools::replyMethodNotFound),
        messageType("Profiler.enable", &DebuggerDevtools::enableProfiler),
        messageType("Runtime.enable", &DebuggerDevtools::enableRuntime),
        messageType("Runtime.getProperties", &DebuggerDevtools::sendProperties),
        messageType("Runtime.runIfWaitingForDebugger", &DebuggerDevtools::resume),
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

            return (this->*message.handler)(jsonMessage);
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
