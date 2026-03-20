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

#ifdef ESCARGOT_DEBUGGER

namespace Escargot {

#ifdef WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#include <WS2tcpip.h>

/* On Windows the WSAEWOULDBLOCK value can be returned for non-blocking operations */
#define ESCARGOT_EWOULDBLOCK WSAEWOULDBLOCK

/* On Windows the invalid socket's value of INVALID_SOCKET */
#define ESCARGOT_INVALID_SOCKET INVALID_SOCKET

#else /* !WIN32 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>

/* On *nix the EWOULDBLOCK errno value can be returned for non-blocking operations */
#define ESCARGOT_EWOULDBLOCK EWOULDBLOCK

/* On *nix the invalid socket has a value of -1 */
#define ESCARGOT_INVALID_SOCKET (-1)

#endif /* WIN32 */

static inline int tcpGetErrno(void)
{
#ifdef WIN32
    return WSAGetLastError();
#else /* !WIN32 */
    return errno;
#endif /* WIN32 */
}

static void tcpLogError(int errorNumber)
{
    ASSERT(errorNumber != 0);

#ifdef WIN32
    char* errorMessage = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  errorNumber,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&errorMessage,
                  0,
                  NULL);
    ESCARGOT_LOG_ERROR("TCP Error: %s\n", errorMessage);
    LocalFree(errorMessage);
#else /* !WIN32 */
    ESCARGOT_LOG_ERROR("TCP Error: %s\n", strerror(errorNumber));
#endif /* WIN32 */
}

static inline void tcpCloseSocket(EscargotSocket socket)
{
#ifdef WIN32
    closesocket(socket);
#else /* !WIN32 */
    close(socket);
#endif /* WIN32 */
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

    m_pendingMessages.emplace_back(msg);
}

bool DebuggerDevtools::skipSourceCode(String* srcName) const
{
    ESCARGOT_LOG_INFO("Implement this: DebuggerDevtools::skipSourceCode\n");
    return false;
}

void computeEndLocation(const LChar* src, size_t length, uint32_t& endLine, uint32_t& endColumn)
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

    m_pendingMessages.emplace_back(
        DebuggerDevtoolsMessageBuilder::buildScriptParsedMessage(scriptId, source, srcName));
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

    m_pendingMessages.emplace_back(msg);
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

    send(0, buffer, strlen(buffer));

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
    return send(0, message.c_str(), message.size());
}

bool DebuggerDevtools::replyOK(rapidjson::Document& jsonMessage)
{
    char reply[32];
    const int jsonReplyStringLength = snprintf(reply, sizeof(reply),
                                               R"({"id": %d, "result": {}})",
                                               jsonMessage["id"].GetInt());

    send(0, reply, jsonReplyStringLength);
    return true;
}

bool DebuggerDevtools::enableNetwork(rapidjson::Document& jsonMessage)
{
    this->networkEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableDebugger(rapidjson::Document& jsonMessage)
{
    this->debuggerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableRuntime(rapidjson::Document& jsonMessage)
{
    this->runtimeEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableProfiler(rapidjson::Document& jsonMessage)
{
    this->profilerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::replyMethodNotFound(rapidjson::Document& jsonMessage)
{
    char reply[256];

    const int jsonReplyStringLength = snprintf(reply, sizeof(reply),
                                               R"({"id":%d,"error":{"code":-32601,"message":"'%s' wasn't found"}})",
                                               jsonMessage["id"].GetInt(),
                                               jsonMessage["method"].GetString());

    send(0, reply, jsonReplyStringLength);

    return true;
}

const char* jsonParseErrorCodeToString(const rapidjson::ParseErrorCode errorCode)
{
    switch (errorCode) {
    case rapidjson::kParseErrorNone:
        return "kParseErrorNone: No error";
    case rapidjson::kParseErrorDocumentEmpty:
        return "kParseErrorDocumentEmpty: The document is empty";
    case rapidjson::kParseErrorDocumentRootNotSingular:
        return "kParseErrorDocumentRootNotSingular: The document root must not follow by other values";
    case rapidjson::kParseErrorValueInvalid:
        return "kParseErrorValueInvalid: Invalid value";
    case rapidjson::kParseErrorObjectMissName:
        return "kParseErrorObjectMissName: Missing a name for object member";
    case rapidjson::kParseErrorObjectMissColon:
        return "kParseErrorObjectMissColon: Missing a colon after a name of object member";
    case rapidjson::kParseErrorObjectMissCommaOrCurlyBracket:
        return "kParseErrorObjectMissCommaOrCurlyBracket: Missing a comma or '}' after an object member";
    case rapidjson::kParseErrorArrayMissCommaOrSquareBracket:
        return "kParseErrorArrayMissCommaOrSquareBracket: Missing a comma or ']' after an array element";
    case rapidjson::kParseErrorStringUnicodeEscapeInvalidHex:
        return "kParseErrorStringUnicodeEscapeInvalidHex: Incorrect hex digit after \\u escape in string";
    case rapidjson::kParseErrorStringUnicodeSurrogateInvalid:
        return "kParseErrorStringUnicodeSurrogateInvalid: The surrogate pair in string is invalid";
    case rapidjson::kParseErrorStringEscapeInvalid:
        return "kParseErrorStringEscapeInvalid: Invalid escape character in string";
    case rapidjson::kParseErrorStringMissQuotationMark:
        return "kParseErrorStringMissQuotationMark: Missing a closing quotation mark in string";
    case rapidjson::kParseErrorStringInvalidEncoding:
        return "kParseErrorStringInvalidEncoding: Invalid encoding in string";
    case rapidjson::kParseErrorNumberTooBig:
        return "kParseErrorNumberTooBig: Number too big to be stored in double";
    case rapidjson::kParseErrorNumberMissFraction:
        return "kParseErrorNumberMissFraction: Miss fraction part in number";
    case rapidjson::kParseErrorNumberMissExponent:
        return "kParseErrorNumberMissExponent: Miss exponent in number";
    case rapidjson::kParseErrorTermination:
        return "kParseErrorTermination: Parsing was terminated";
    case rapidjson::kParseErrorUnspecificSyntaxError:
        return "kParseErrorUnspecificSyntaxError: Unspecific syntax error";
    }
    return "Unknown json parsing error";
}

size_t charArrayLength(const char* buffer)
{
    size_t length = 0;
    for (; buffer[length] != '\0'; length++) {}
    return length;
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
        messageType("Debugger.setBlackboxPatterns", &DebuggerDevtools::replyOK), // set skipSourceCode, skipSourceName in DebuggerTcp
        messageType("Debugger.setPauseOnExceptions", &DebuggerDevtools::replyOK), // this should be our default?
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
        if (jsonMessage.Parse(reinterpret_cast<const char*>(buffer)).HasParseError()) {
            ESCARGOT_LOG_ERROR("Json Message parsing error: %s at offset: %d\n", jsonParseErrorCodeToString(jsonMessage.GetParseError()), static_cast<int32_t>(jsonMessage.GetErrorOffset()));
            return false;
        }
        const char* methodName = jsonMessage["method"].GetString();
        const uint32_t methodNameLength = charArrayLength(methodName);

        for (const auto& message : messageTypes) {
            // TODO: it would be better to calculate the lengths of each of the method name strings at compile time instead of using charArrayLength()
            if (!DebuggerHttpRouter::requestStartsWith(reinterpret_cast<const uint8_t*>(methodName), methodNameLength, message.methodName, charArrayLength(message.methodName))) {
                continue;
            }

            return (this->*message.handler)(jsonMessage);
        }
        ESCARGOT_LOG_ERROR("Debugger function not supported: %s\n", reinterpret_cast<const char*>(buffer));
        this->replyMethodNotFound(jsonMessage); // maybe reply with not supported instead? if there is such a reply in the spec
    }

    if (!sentPendingMessages && networkEnabled && debuggerEnabled && runtimeEnabled) {
        for (const std::string& msg : m_pendingMessages) {
            send(0, msg.data(), msg.size());
        }
        m_pendingMessages.clear();
        sentPendingMessages = true;
    }

    return enabled();
}

bool DebuggerDevtools::send(const uint8_t type, const void* buffer, const size_t length)
{
    UNUSED_PARAMETER(type);
    ASSERT(enabled());

    if (length > ESCARGOT_WS_MAX_MESSAGE_LENGTH) {
        ESCARGOT_LOG_ERROR("Cannot send WebSocket payload: 64-bit payload length is not supported.\n");
        close(CloseAbortConnection);
        return false;
    }

    size_t headerLength = 0;
    uint8_t message[ESCARGOT_WS_BUFFER_SIZE];
    message[0] = ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT | ESCARGOT_DEBUGGER_WEBSOCKET_TEXT_FRAME;

    // Server-to-client WebSocket frames are not masked,
    // therefore the masking key is not included in the header.
    if (length <= ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH) {
        headerLength = ESCARGOT_WS_HEADER_SIZE - ESCARGOT_WS_MASK_SIZE;

        message[1] = static_cast<uint8_t>(length);
    } else {
        headerLength = ESCARGOT_WS_HEADER_LEN16_SIZE - ESCARGOT_WS_MASK_SIZE;

        message[1] = ESCARGOT_WS_MESSAGE_16BIT_LENGTH_MARKER;
        message[2] = static_cast<uint8_t>((length >> 8) & 0xFF);
        message[3] = static_cast<uint8_t>(length & 0xFF);
    }

    memcpy(message + headerLength, buffer, length);

    if (!tcpSend(m_socket, message, headerLength + length)) {
        ESCARGOT_LOG_ERROR("Failed to send data via WebSocket connection.\n");
        close(CloseAbortConnection);
        return false;
    }

    ESCARGOT_LOG_INFO("Sent message: %s\n", static_cast<const char*>(buffer));
    return true;
}

bool DebuggerDevtools::receive(uint8_t* buffer, size_t& length)
{
    size_t receivedLength = 0;

    if (m_payloadLength == 0 || m_receiveBufferFill < m_headerLength + m_payloadLength) {
        /* Cannot extract a whole message from the buffer. */
        if (!tcpReceive(m_socket,
                        m_receiveBuffer + m_receiveBufferFill,
                        ESCARGOT_WS_BUFFER_SIZE - m_receiveBufferFill,
                        &receivedLength)) {
            ESCARGOT_LOG_ERROR("Failed to receive data from WebSocket connection.\n");
            close(CloseAbortConnection);
            return false;
        }

        if (receivedLength == 0 && m_receiveBufferFill < m_headerLength) {
            // ESCARGOT_LOG_INFO("Incomplete WebSocket frame header, waiting for more data.\n");
            return false;
        }

        m_receiveBufferFill = static_cast<uint32_t>(m_receiveBufferFill + receivedLength);
    }

    if (m_payloadLength == 0) {
        if (m_receiveBufferFill < ESCARGOT_WS_HEADER_BASE_SIZE) {
            return false;
        }

        if ((m_receiveBuffer[0] & ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK) != ESCARGOT_DEBUGGER_WEBSOCKET_TEXT_FRAME) {
            if ((m_receiveBuffer[0] & ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK) == ESCARGOT_DEBUGGER_WEBSOCKET_CLOSE_FRAME) {
                close(CloseEndConnection);
                return false;
            }

            ESCARGOT_LOG_ERROR("Unsupported Websocket opcode.\n");
            close(CloseProtocolUnsupported);
            return false;
        }

        if ((m_receiveBuffer[0] & ~ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK) != ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT
            || !(m_receiveBuffer[1] & ESCARGOT_DEBUGGER_WEBSOCKET_MASK_BIT)) {
            ESCARGOT_LOG_ERROR("Unsupported Websocket message.\n");
            close(CloseProtocolUnsupported);
            return false;
        }

        uint8_t payloadLengthField = m_receiveBuffer[1] & ESCARGOT_DEBUGGER_WEBSOCKET_LENGTH_MASK;

        if (payloadLengthField > ESCARGOT_WS_MESSAGE_16BIT_LENGTH_MARKER) {
            ESCARGOT_LOG_ERROR("64-bit WebSocket payload length is not supported.\n");
            close(CloseProtocolUnsupported);
            return false;
        }

        if (payloadLengthField <= ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH) {
            m_payloadLength = payloadLengthField;
        } else {
            ASSERT(payloadLengthField == ESCARGOT_WS_MESSAGE_16BIT_LENGTH_MARKER);
            // Ensure that the extended payload length field is available
            if (m_receiveBufferFill < ESCARGOT_WS_HEADER_LEN16_SIZE) {
                return false;
            }

            m_headerLength = ESCARGOT_WS_HEADER_LEN16_SIZE;
            m_payloadLength = (static_cast<uint16_t>(m_receiveBuffer[2]) << 8) | static_cast<uint16_t>(m_receiveBuffer[3]);
        }

        if (m_payloadLength == 0) {
            ESCARGOT_LOG_ERROR("Invalid WebSocket payload length: zero-length messages are not supported.\n");
            close(CloseProtocolUnsupported);
            return false;
        }
    }

    size_t totalSize = m_headerLength + m_payloadLength;

    if (m_receiveBufferFill < totalSize) {
        return false;
    }

    uint8_t* mask = m_receiveBuffer + m_headerLength - ESCARGOT_WS_MASK_SIZE;
    uint8_t* mask_end = mask + ESCARGOT_WS_MASK_SIZE;
    uint8_t* source = mask_end;
    uint8_t* buffer_end = buffer + m_payloadLength;

    while (buffer < buffer_end) {
        *buffer++ = *source++ ^ *mask++;

        if (mask >= mask_end) {
            mask -= 4;
        }
    }
    *buffer_end = 0;

    length = m_payloadLength + 1;
    m_headerLength = ESCARGOT_WS_HEADER_SIZE;
    m_payloadLength = 0;

    if (m_receiveBufferFill == totalSize) {
        m_receiveBufferFill = 0;
        return true;
    }

    m_receiveBufferFill = static_cast<uint32_t>(m_receiveBufferFill - totalSize);
    memmove(m_receiveBuffer, m_receiveBuffer + totalSize, m_receiveBufferFill);
    return true;
}

} // namespace Escargot

#endif /* ESCARGOT_DEBUGGER */
