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

#ifndef __DebuggerTcp__
#define __DebuggerTcp__

#include "Debugger.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

#ifdef WIN32
#include <winsock2.h>
typedef SOCKET EscargotSocket;
#else /* !WIN32 */
typedef int EscargotSocket;
#endif /* WIN32 */
#define ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH 125

#define ESCARGOT_DEBUGGER_WEBSOCKET_FIN_BIT 0x80
#define ESCARGOT_DEBUGGER_WEBSOCKET_TEXT_FRAME 1
#define ESCARGOT_DEBUGGER_WEBSOCKET_BINARY_FRAME 2
#define ESCARGOT_DEBUGGER_WEBSOCKET_CLOSE_FRAME 8
#define ESCARGOT_DEBUGGER_WEBSOCKET_OPCODE_MASK 0x0f
#define ESCARGOT_DEBUGGER_WEBSOCKET_LENGTH_MASK 0x7f
#define ESCARGOT_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX 125
#define ESCARGOT_DEBUGGER_WEBSOCKET_MASK_BIT 0x80

#define ESCARGOT_WS_HEADER_BASE_SIZE 2
#define ESCARGOT_WS_EXT_LEN16_SIZE 2
#define ESCARGOT_WS_MASK_SIZE 4

#define ESCARGOT_WS_MESSAGE_16BIT_LENGTH_MARKER 126
#define ESCARGOT_WS_MAX_MESSAGE_LENGTH 65535

#define ESCARGOT_WS_HEADER_SIZE (ESCARGOT_WS_HEADER_BASE_SIZE + ESCARGOT_WS_MASK_SIZE)
#define ESCARGOT_WS_HEADER_LEN16_SIZE (ESCARGOT_WS_HEADER_SIZE + ESCARGOT_WS_EXT_LEN16_SIZE)
#define ESCARGOT_WS_BUFFER_SIZE (ESCARGOT_WS_HEADER_LEN16_SIZE + ESCARGOT_WS_MAX_MESSAGE_LENGTH)

class DebuggerTcp : public Debugger {
public:
    DebuggerTcp(const EscargotSocket socket, String* skipSource, const uint32_t bufferSize, const uint8_t type)
        : m_socket(socket)
        , m_receiveBuffer(new uint8_t[bufferSize])
        , m_bufferSize(bufferSize)
        , m_receiveBufferFill(0)
        , m_payloadLength(0)
        , m_headerLength(0)
        , m_websocketMessageType(type)
        , m_skipSourceName(skipSource)
    {
    }

    static Debugger* createDebugger(const char* options, Context* context);
    void init(const char* options, Context* context) override {}

    bool skipSourceCode(String* srcName) const override;

    static void computeSha1(const uint8_t* source1, size_t source1Length,
                            const uint8_t* source2, size_t source2Length,
                            uint8_t destination[20]);

    static bool tcpReceive(EscargotSocket socket, uint8_t* message, size_t maxLength, size_t* receivedLength);
    static bool tcpSend(EscargotSocket socket, const uint8_t* message, size_t messageLength);

protected:
    enum CloseReason {
        CloseEndConnection,
        CloseAbortConnection,
        CloseProtocolUnsupported,
        CloseProtocolError,
    };

    bool send(uint8_t type, const void* buffer, size_t length);
    bool receive(uint8_t* buffer, size_t& length);
    bool isThereAnyEvent();
    void close(CloseReason reason);

    EscargotSocket m_socket;
    uint8_t* m_receiveBuffer;
    uint32_t m_bufferSize;
    uint32_t m_receiveBufferFill;
    uint16_t m_payloadLength;
    uint8_t m_headerLength;
    uint8_t m_websocketMessageType;


    // skip generating debugging bytecode for source code whose name contains m_skipSourceName
    String* m_skipSourceName;
};
} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
