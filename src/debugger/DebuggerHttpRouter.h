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

#ifndef __DebuggerHttpRouter__
#define __DebuggerHttpRouter__

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

#ifdef WIN32
#include <winsock2.h>
typedef SOCKET EscargotSocket;
#else /* !WIN32 */
typedef int EscargotSocket;
#endif /* WIN32 */

enum class DebuggerClient : uint8_t {
    None,
    Escargot,
    DevTools
};

struct RequestContext {
    EscargotSocket socket;
    uint8_t* request;
    size_t requestLength;
};

using RouteHandler = bool (*)(const RequestContext&);

struct Route {
    const char* prefix;
    size_t prefixLength;
    DebuggerClient client;
    RouteHandler handler;
};

class DebuggerHttpRouter {
public:
    DebuggerHttpRouter() = default;

    static bool requestStartsWith(const uint8_t* buffer, size_t length, const char* prefix, size_t prefixLength);
    bool handleHttpRequest(EscargotSocket socket);
    bool webSocketEstablished() const;
    DebuggerClient client() const;

private:
    DebuggerClient m_client{ DebuggerClient::None };
};

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
