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
#include "DebuggerTcp.h"
#include "runtime/String.h" // for split function

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

static bool tcpConfigureSocket(EscargotSocket socket, uint16_t port)
{
    sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt_value = 1;

    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(int)) != 0) {
        return false;
    }

    if (bind(socket, (sockaddr*)&addr, sizeof(sockaddr_in)) != 0) {
        return false;
    }

    return listen(socket, 1) == 0;
}

static inline void tcpCloseSocket(EscargotSocket socket)
{
#ifdef WIN32
    closesocket(socket);
#else /* !WIN32 */
    close(socket);
#endif /* WIN32 */
}

static bool tcpSend(EscargotSocket socket, const uint8_t* message, size_t messageLength)
{
    do {
#ifdef OS_POSIX
        ssize_t result = recv(socket, NULL, 0, MSG_PEEK);

        if (result == 0 && (errno != ESCARGOT_EWOULDBLOCK && errno != 0)) {
            tcpLogError(tcpGetErrno());
            return false;
        }
#endif /* OS_POSIX */

        ssize_t sentBytes = send(socket, message, messageLength, 0);

        if (sentBytes < 0) {
            int errorNumber = tcpGetErrno();

            if (errorNumber == ESCARGOT_EWOULDBLOCK) {
                continue;
            }

            tcpLogError(tcpGetErrno());
            return false;
        }

        message += sentBytes;
        messageLength -= (size_t)sentBytes;
    } while (messageLength > 0);

    return true;
}

static bool tcpReceive(EscargotSocket socket, uint8_t* message, size_t maxLength, size_t* receivedLength)
{
    *receivedLength = 0;

    ssize_t length = recv(socket, message, maxLength, 0);

    if (length > 0) {
        *receivedLength = (size_t)length;
        return true;
    }

    int errorNumber = tcpGetErrno();

    if (errorNumber != ESCARGOT_EWOULDBLOCK || length == 0) {
        tcpLogError(errorNumber);
        return false;
    }
    return true;
}

static uint8_t toBase64Character(uint8_t value)
{
    if (value < 26) {
        return (uint8_t)(value + 'A');
    }

    if (value < 52) {
        return (uint8_t)(value - 26 + 'a');
    }

    if (value < 62) {
        return (uint8_t)(value - 52 + '0');
    }

    if (value == 62) {
        return (uint8_t)'+';
    }

    return (uint8_t)'/';
}

/**
 * Encode a byte sequence into Base64 string.
 */
static void toBase64(const uint8_t* source, uint8_t* destination, size_t length)
{
    while (length >= 3) {
        uint8_t value = (source[0] >> 2);
        destination[0] = toBase64Character(value);

        value = (uint8_t)(((source[0] << 4) | (source[1] >> 4)) & 0x3f);
        destination[1] = toBase64Character(value);

        value = (uint8_t)(((source[1] << 2) | (source[2] >> 6)) & 0x3f);
        destination[2] = toBase64Character(value);

        value = (uint8_t)(source[2] & 0x3f);
        destination[3] = toBase64Character(value);

        source += 3;
        destination += 4;
        length -= 3;
    }
}

static bool webSocketHandshake(EscargotSocket socket)
{
    uint8_t buffer[1024];
    size_t remainingLength = sizeof(buffer);
    uint8_t* message = buffer;

    while (true) {
        size_t receivedLength;
        if (!tcpReceive(socket, message, remainingLength, &receivedLength)) {
            return false;
        }

        message += receivedLength;
        remainingLength -= receivedLength;

        if (message > (buffer + 4) && memcmp(message - 4, "\r\n\r\n", 4) == 0) {
            break;
        }

        if (remainingLength == 0) {
            ESCARGOT_LOG_ERROR("WebSocket Error: Request too long\n");
            return false;
        }
    }

    /* Check protocol. */
    const char expectedProtocol[] = "GET /escargot-debugger";
    const size_t expectedProtocolLength = sizeof(expectedProtocol) - 1;

    if ((size_t)(message - buffer) < expectedProtocolLength
        || memcmp(buffer, expectedProtocol, expectedProtocolLength) != 0) {
        ESCARGOT_LOG_ERROR("WebSocket Error: Invalid handshake format.\n");
        return false;
    }

    uint8_t* websocketKey = buffer + expectedProtocolLength;

    const char expectedWebsocketKey[] = "Sec-WebSocket-Key:";
    const size_t expectedWebsocketKeyLength = sizeof(expectedWebsocketKey) - 1;

    while (true) {
        if ((size_t)(message - websocketKey) < expectedWebsocketKeyLength) {
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
    const size_t sha1Length = 20;

    DebuggerTcp::computeSha1(websocketKey,
                             (size_t)(websocketKeyEnd - websocketKey),
                             (const uint8_t*)"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                             36,
                             buffer);

    /* The SHA-1 key is 20 bytes long but toBase64 expects a length
     * divisible by 3 so an extra 0 is appended at the end. */
    buffer[sha1Length] = 0;

    toBase64(buffer, buffer + sha1Length + 1, sha1Length + 1);

    /* Last value must be replaced by equal sign. */
    const uint8_t responsePrefix[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";

    if (!tcpSend(socket, responsePrefix, sizeof(responsePrefix) - 1)
        || !tcpSend(socket, buffer + sha1Length + 1, 27)) {
        return false;
    }

    const uint8_t responseSuffix[] = "=\r\n\r\n";
    return tcpSend(socket, responseSuffix, sizeof(responseSuffix) - 1);
}

void DebuggerTcp::init(const char* options, Context* context)
{
    uint16_t port = 6501;
    int timeout = -1;

    if (options) {
        auto v = split(options, ';');
        const char portOption[] = "--port=";
        const char acceptTimeoutOption[] = "--accept-timeout=";
        const char skipOption[] = "--skip=";
        for (size_t i = 0; i < v.size(); i++) {
            const std::string& s = v[i];
            if (s.find(portOption) == 0) {
                int i = std::atoi(s.data() + sizeof(portOption) - 1);
                if (i > 0 && i <= 65535) {
                    port = i;
                }
            } else if (s.find(acceptTimeoutOption) == 0) {
                timeout = std::atoi(s.data() + sizeof(acceptTimeoutOption) - 1);
            } else if (s.find(skipOption) == 0) {
                const char* skipStr = const_cast<const char*>(s.data() + sizeof(skipOption) - 1);
                size_t skipLen = strlen(skipStr);
                m_skipSourceName = String::fromASCII(skipStr, skipLen);
            }
        }
    }

    ASSERT(enabled() == false);

#ifdef WIN32
    WSADATA wsaData;
    int wsa_init_status = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_init_status != NO_ERROR) {
        return;
    }
#endif /* WIN32*/

    EscargotSocket serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == ESCARGOT_INVALID_SOCKET) {
        return;
    }

    if (!tcpConfigureSocket(serverSocket, port)) {
        int error = tcpGetErrno();
        tcpCloseSocket(serverSocket);
        tcpLogError(error);
        return;
    }

    ESCARGOT_LOG_INFO("Waiting for client connection 0.0.0.0:%hd\n", port);

    struct pollfd fd[1];
    fd[0].fd = serverSocket;
    fd[0].events = POLLIN;
    while (true) {
        int rc = poll(fd, 1, 0);

        if (rc != 0) {
            break;
        }

        usleep(10 * 1000); // 10ms
        if (timeout == -1) {
            continue;
        }
        timeout -= 10;
        if (timeout < 0) {
            ESCARGOT_LOG_ERROR("Waiting for client connection error: timeout reached\n");
            tcpCloseSocket(serverSocket);
            return;
        }
    }

    sockaddr_in addr;
    socklen_t sinSize = sizeof(sockaddr_in);

    m_socket = accept(serverSocket, (sockaddr*)&addr, &sinSize);

    tcpCloseSocket(serverSocket);

    if (m_socket == ESCARGOT_INVALID_SOCKET) {
        tcpLogError(tcpGetErrno());
        return;
    }

#ifdef WIN32
    u_long nonblockingEnabled = 1;

    /* Set non-blocking mode. */
    if (ioctlsocket(m_socket, FIONBIO, &nonblockingEnabled) != NO_ERROR) {
        tcpCloseSocket(m_socket);
        return;
    }
#else /* !WIN32 */
    int socketFlags = fcntl(m_socket, F_GETFL, 0);

    if (socketFlags < 0) {
        tcpCloseSocket(m_socket);
        return;
    }

    /* Set non-blocking mode. */
    if (fcntl(m_socket, F_SETFL, socketFlags | O_NONBLOCK) == -1) {
        tcpCloseSocket(m_socket);
        return;
    }
#endif /* WIN32 */

    ESCARGOT_LOG_INFO("Connected from: %s\n", inet_ntoa(addr.sin_addr));

    if (!webSocketHandshake(m_socket)) {
        tcpCloseSocket(m_socket);
        return;
    }

    m_receiveBufferFill = 0;
    m_messageLength = 0;

    enable(context);

    return DebuggerRemote::init(nullptr, context);
}

bool DebuggerTcp::skipSourceCode(String* srcName) const
{
    ASSERT(!!srcName);
    if (!m_skipSourceName || m_skipSourceName->length() == 0 || srcName->length() == 0) {
        return false;
    }

    return srcName->contains(m_skipSourceName);
}

#define ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT 0x80
#define ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME 2
#define ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK 0x0f
#define ESCARGOT_DEBUGGER_WEBSOCKET_LENGTH_MASK 0x7f
#define ESCARGOT_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX 125
#define ESCARGOT_DEBUGGER_WEBSOCKET_MASK_BIT 0x80

bool DebuggerTcp::send(uint8_t type, const void* buffer, size_t length)
{
    ASSERT(enabled());
    ASSERT(length < ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH);

    uint8_t message[ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH + 2];

    message[0] = ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT | ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME;
    message[1] = (uint8_t)(length + 1);
    message[2] = type;
    memcpy(message + 3, buffer, length);

    if (tcpSend(m_socket, message, length + 3)) {
        return true;
    }

    close();
    return false;
}

bool DebuggerTcp::isThereAnyEvent()
{
    // if there is remained receive buffer data,
    // user should call receive function again
    if (m_receiveBufferFill) {
        return true;
    }

    struct pollfd fd[1];
    fd[0].fd = m_socket;
    fd[0].events = POLLIN;
    int rc = poll(fd, 1, 0);

    if (rc == 0) {
        return false;
    }

    return true;
}

bool DebuggerTcp::receive(uint8_t* buffer, size_t& length)
{
    size_t receivedLength;

    if (m_messageLength == 0 || m_receiveBufferFill < 2 + sizeof(uint32_t) + m_messageLength) {
        /* Cannot extract a whole message from the buffer. */
        if (!tcpReceive(m_socket,
                        m_receiveBuffer + m_receiveBufferFill,
                        ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH + 2 + sizeof(uint32_t) - m_receiveBufferFill,
                        &receivedLength)) {
            close();
            return false;
        }

        if (receivedLength == 0 && m_receiveBufferFill < (2 + sizeof(uint32_t))) {
            return false;
        }

        m_receiveBufferFill = (uint8_t)(m_receiveBufferFill + receivedLength);
    }

    if (m_messageLength == 0) {
        if (m_receiveBufferFill < 3) {
            return false;
        }

        if ((m_receiveBuffer[0] & ~ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK) != ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT
            || !(m_receiveBuffer[1] & ESCARGOT_DEBUGGER_WEBSOCKET_MASK_BIT)) {
            ESCARGOT_LOG_ERROR("Unsupported Websocket message.\n");
            close();
            return false;
        }

        if ((m_receiveBuffer[0] & ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK) != ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME) {
            ESCARGOT_LOG_ERROR("Unsupported Websocket opcode.\n");
            close();
            return false;
        }

        m_messageLength = (uint8_t)(m_receiveBuffer[1] & ESCARGOT_DEBUGGER_WEBSOCKET_LENGTH_MASK);

        if (m_messageLength == 0 || m_messageLength > ESCARGOT_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX) {
            ESCARGOT_LOG_ERROR("Unsupported Websocket message size.\n");
            close();
            return false;
        }
    }

    size_t totalSize = 2 + sizeof(uint32_t) + m_messageLength;

    if (m_receiveBufferFill < totalSize) {
        return false;
    }

    uint8_t* mask = m_receiveBuffer + 2;
    uint8_t* mask_end = mask + sizeof(uint32_t);
    uint8_t* source = mask_end;
    uint8_t* buffer_end = buffer + m_messageLength;

    while (buffer < buffer_end) {
        *buffer++ = *source++ ^ *mask++;

        if (mask >= mask_end) {
            mask -= 4;
        }
    }

    length = m_messageLength;
    m_messageLength = 0;

    if (m_receiveBufferFill == totalSize) {
        m_receiveBufferFill = 0;
        return true;
    }

    m_receiveBufferFill = (uint8_t)(m_receiveBufferFill - totalSize);
    memmove(m_receiveBuffer, m_receiveBuffer + totalSize, m_receiveBufferFill);
    return true;
}

void DebuggerTcp::close(void)
{
    if (enabled()) {
        tcpCloseSocket(m_socket);
        disable();
    }
}
} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */
