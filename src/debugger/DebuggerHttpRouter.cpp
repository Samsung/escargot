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
#include "DebuggerHttpRouter.h"
#include "DebuggerTcp.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

static uint8_t toBase64Character(uint8_t value)
{
    if (value < 26) {
        return static_cast<uint8_t>(value + 'A');
    }

    if (value < 52) {
        return static_cast<uint8_t>(value - 26 + 'a');
    }

    if (value < 62) {
        return static_cast<uint8_t>(value - 52 + '0');
    }

    if (value == 62) {
        return '+';
    }

    return '/';
}

/**
 * Encode a byte sequence into Base64 string.
 */
static void toBase64(const uint8_t* source, uint8_t* destination, size_t length)
{
    while (length >= 3) {
        uint8_t value = (source[0] >> 2);
        destination[0] = toBase64Character(value);

        value = static_cast<uint8_t>(((source[0] << 4) | (source[1] >> 4)) & 0x3f);
        destination[1] = toBase64Character(value);

        value = static_cast<uint8_t>(((source[1] << 2) | (source[2] >> 6)) & 0x3f);
        destination[2] = toBase64Character(value);

        value = static_cast<uint8_t>(source[2] & 0x3f);
        destination[3] = toBase64Character(value);

        source += 3;
        destination += 4;
        length -= 3;
    }
}

bool DebuggerHttpRouter::requestStartsWith(const uint8_t* buffer, const size_t length, const char* prefix, const size_t prefixLength)
{
    if (length < prefixLength)
        return false;

    return memcmp(buffer, prefix, prefixLength) == 0;
}

static bool buildHttpResponse(const char* body, uint8_t* buffer, size_t bufferSize, size_t& outLen)
{
    size_t bodyLen = strlen(body);

    int len = snprintf(reinterpret_cast<char*>(buffer), bufferSize,
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json; charset=UTF-8\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       bodyLen,
                       body);

    if (len < 0 || static_cast<size_t>(len) >= bufferSize)
        return false;

    outLen = static_cast<size_t>(len);
    return true;
}

static bool buildVersionResponse(uint8_t* buffer, size_t bufferSize, size_t& outLen)
{
    static constexpr const char body[] = "{"
                                         "\"Browser\":\"Escargot/1.0\","
                                         "\"Protocol-Version\":\"1.3\""
                                         "}";

    return buildHttpResponse(body, buffer, bufferSize, outLen);
}

static bool buildListResponse(uint8_t* buffer, size_t bufferSize, const char* ip, uint16_t port, size_t& outLen)
{
    char body[512];

    int bodyLen = snprintf(body, sizeof(body),
                           "[{"
                           "\"description\":\"Escargot CDP target\","
                           "\"devtoolsFrontendUrl\":\"/devtools/inspector.html?ws=%s:%u/devtools/page/1\","
                           "\"id\":\"1\","
                           "\"title\":\"Escargot\","
                           "\"type\":\"node\","
                           "\"url\":\"file:///\","
                           "\"webSocketDebuggerUrl\":\"ws://%s:%u/devtools/page/1\""
                           "}]",
                           ip, static_cast<unsigned>(port),
                           ip, static_cast<unsigned>(port));

    if (bodyLen < 0 || static_cast<size_t>(bodyLen) >= sizeof(body)) {
        return false;
    }

    return buildHttpResponse(body, buffer, bufferSize, outLen);
}

static bool webSocketHandshake(EscargotSocket socket, uint8_t* buffer, size_t length)
{
    uint8_t* websocketKey = buffer;

    constexpr char expectedWebsocketKey[] = "Sec-WebSocket-Key:";
    constexpr size_t expectedWebsocketKeyLength = sizeof(expectedWebsocketKey) - 1;

    while (true) {
        if (length < expectedWebsocketKeyLength) {
            ESCARGOT_LOG_ERROR("Sec-WebSocket-Key not found.\n");
            return false;
        }

        if (websocketKey[0] == 'S'
            && websocketKey[-1] == '\n'
            && websocketKey[-2] == '\r'
            && memcmp(websocketKey, expectedWebsocketKey, expectedWebsocketKeyLength) == 0) {
            websocketKey += expectedWebsocketKeyLength;
            break;
        }

        websocketKey++;
    }

    /* String terminated by double newlines. */
    while (*websocketKey == ' ') {
        websocketKey++;
    }

    uint8_t* websocketKeyEnd = websocketKey;

    while (*websocketKeyEnd > ' ') {
        websocketKeyEnd++;
    }

    /* Since the buffer is not needed anymore it can
     * be reused for storing the SHA-1 key and Base64 string. */
    constexpr size_t sha1Length = 20;

    DebuggerTcp::computeSha1(websocketKey,
                             static_cast<size_t>(websocketKeyEnd - websocketKey),
                             reinterpret_cast<const uint8_t*>("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"),
                             36,
                             buffer);

    /* The SHA-1 key is 20 bytes long but toBase64 expects a length
     * divisible by 3 so an extra 0 is appended at the end. */
    buffer[sha1Length] = 0;

    toBase64(buffer, buffer + sha1Length + 1, sha1Length + 1);

    /* Last value must be replaced by equal sign. */
    constexpr uint8_t responsePrefix[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";

    if (!DebuggerTcp::tcpSend(socket, responsePrefix, sizeof(responsePrefix) - 1)
        || !DebuggerTcp::tcpSend(socket, buffer + sha1Length + 1, 27)) {
        return false;
    }

    constexpr uint8_t responseSuffix[] = "=\r\n\r\n";
    return DebuggerTcp::tcpSend(socket, responseSuffix, sizeof(responseSuffix) - 1);
}

bool DebuggerHttpRouter::webSocketEstablished() const
{
    return m_client != DebuggerClient::None;
}

DebuggerClient DebuggerHttpRouter::client() const
{
    return m_client;
}

bool handleWebSocketRequest(const RequestContext& ctx)
{
    return webSocketHandshake(ctx.socket, ctx.request, ctx.requestLength);
}

bool handleJsonVersion(const RequestContext& ctx)
{
    uint8_t buffer[1024];
    size_t responseLength = 0;

    if (!buildVersionResponse(buffer, sizeof(buffer), responseLength)) {
        return false;
    }

    return DebuggerTcp::tcpSend(ctx.socket, buffer, responseLength);
}

bool handleJsonList(const RequestContext& ctx)
{
    struct sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    getsockname(ctx.socket, reinterpret_cast<struct sockaddr*>(&addr), &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    const uint16_t port = ntohs(addr.sin_port);

    uint8_t buffer[1024];
    size_t responseLength = 0;

    if (!buildListResponse(buffer, sizeof(buffer), ip, port, responseLength)) {
        return false;
    }

    return DebuggerTcp::tcpSend(ctx.socket, buffer, responseLength);
}

static bool readHttpMessage(EscargotSocket socket, uint8_t* buffer, size_t capacity, size_t& messageLength)
{
    size_t remainingLength = capacity;
    uint8_t* message = buffer;

    while (true) {
        size_t receivedLength = 0;
        if (!DebuggerTcp::tcpReceive(socket, message, remainingLength, &receivedLength)) {
            return false;
        }

        message += receivedLength;
        remainingLength -= receivedLength;

        if (message > (buffer + 4) && memcmp(message - 4, "\r\n\r\n", 4) == 0) {
            messageLength = static_cast<size_t>(message - buffer);
            return true;
        }

        if (remainingLength == 0) {
            ESCARGOT_LOG_ERROR("WebSocket Error: Request too long\n");
            return false;
        }
    }
}

template <size_t N>
constexpr Route route(const char (&prefix)[N], DebuggerClient client, RouteHandler handler)
{
    return Route{ prefix, N - 1, client, handler };
}

bool DebuggerHttpRouter::handleHttpRequest(EscargotSocket socket)
{
    uint8_t buffer[1024];
    size_t messageLength = 0;

    if (!readHttpMessage(socket, buffer, sizeof(buffer), messageLength)) {
        return false;
    }

    static constexpr Route routes[] = {
        route("GET /escargot-debugger", DebuggerClient::Escargot, handleWebSocketRequest),
        route("GET /devtools/page/1", DebuggerClient::DevTools, handleWebSocketRequest),
        route("GET /json/version", DebuggerClient::None, handleJsonVersion),
        route("GET /json/list", DebuggerClient::None, handleJsonList),
        route("GET /json", DebuggerClient::None, handleJsonList)
    };

    for (const auto& route : routes) {
        if (!requestStartsWith(buffer, messageLength, route.prefix, route.prefixLength)) {
            continue;
        }

        m_client = route.client;
        // Skip the matched route prefix
        uint8_t* remainder = buffer + route.prefixLength;
        size_t remainderLength = messageLength - route.prefixLength;

        return route.handler(RequestContext{ socket, remainder, remainderLength });
    }

    ESCARGOT_LOG_ERROR("Unsupported http request\n");
    return false;
}

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */
