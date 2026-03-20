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

#ifndef __DebuggerDevtools__
#define __DebuggerDevtools__

#include "Debugger.h"
#include "DebuggerTcp.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

#ifdef WIN32
#include <winsock2.h>
typedef SOCKET EscargotSocket;
#else /* !WIN32 */
typedef int EscargotSocket;
#endif /* WIN32 */

struct ScriptInfo {
    uint8_t scriptId;
    String* url;
    String* source;
};

class DebuggerDevtools : public DebuggerTcp {
public:
    DebuggerDevtools(EscargotSocket socket, String* skipSource)
        : DebuggerTcp(socket, skipSource, ESCARGOT_WS_BUFFER_SIZE, ESCARGOT_DEBUGGER_WEBSOCKET_TEXT_FRAME)
    {
    }

    bool sendMessage(const std::string& msg, const int length = -1);
    void init(const char* options, Context* context) override;
    bool skipSourceCode(String* srcName) const override;

    void parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error = nullptr) override;
    void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) override;
    void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) override;
    void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) override;
    void consoleOut(String* output) override;
    String* getClientSource(String** sourceName) override;
    bool getWaitBeforeExitClient() override;


    void sendPausedEvent(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state);

protected:
    bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) override;

private:
    bool sendProperties(rapidjson::Document& jsonMessage);
    bool resume(rapidjson::Document& jsonMessage);
    bool sendSourceCode(rapidjson::Document& jsonMessage);
    bool replyOK(rapidjson::Document& jsonMessage);
    bool enableNetwork(rapidjson::Document& jsonMessage);
    bool enableDebugger(rapidjson::Document& jsonMessage);
    bool enableRuntime(rapidjson::Document& jsonMessage);
    bool enableProfiler(rapidjson::Document& jsonMessage);
    bool setPauseOnExceptions(rapidjson::Document& jsonMessage);
    bool replyMethodNotFound(rapidjson::Document& jsonMessage);

    uint8_t registerScript(String* url, String* source);

    bool m_networkEnabled = false;
    bool m_debuggerEnabled = false;
    bool m_runtimeEnabled = false;
    bool m_profilerEnabled = false;
    bool m_pauseOnExceptions = false;

    std::unordered_map<uint8_t, ScriptInfo> m_scriptsById;
    std::unordered_map<std::string, uint8_t> m_scriptIdByUrl;
    uint8_t m_nextScriptId = 1;

    std::vector<std::string> m_pendingMessages;
};

using MessageHandler = bool (DebuggerDevtools::*)(rapidjson::Document&);

struct MessageType {
    const char* methodName;
    MessageHandler handler;
};
} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
