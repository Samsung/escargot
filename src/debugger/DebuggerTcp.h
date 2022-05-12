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

class DebuggerTcp : public DebuggerRemote {
public:
    DebuggerTcp()
        : m_socket(0)
        , m_receiveBuffer{}
        , m_receiveBufferFill(0)
        , m_messageLength(0)
        , m_skipSourceName(nullptr)
    {
    }

    virtual void init(const char* options, Context* context) override;

    virtual bool skipSourceCode(String* srcName) const override;

    static void computeSha1(const uint8_t* source1, size_t source1Length,
                            const uint8_t* source2, size_t source2Length,
                            uint8_t destination[20]);

protected:
    virtual bool send(uint8_t type, const void* buffer, size_t length) override;
    virtual bool receive(uint8_t* buffer, size_t& length) override;
    virtual bool isThereAnyEvent() override;
    virtual void close(CloseReason reason) override;

private:
    void receiveData();

    EscargotSocket m_socket;
    uint8_t m_receiveBuffer[2 + sizeof(uint32_t) + ESCARGOT_DEBUGGER_MAX_MESSAGE_LENGTH];
    uint8_t m_receiveBufferFill;
    uint8_t m_messageLength;

    // skip generating debugging bytecode for source code whose name contains m_skipSourceName
    String* m_skipSourceName;
};
} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
