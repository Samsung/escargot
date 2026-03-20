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

#ifndef __DebuggerDevtoolsMessageBuilder__
#define __DebuggerDevtoolsMessageBuilder__

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

class String;

class DebuggerDevtoolsMessageBuilder {
public:
    static std::string buildEmptyMessage(uint32_t id);
    static std::string buildScriptParsedMessage(uint8_t scriptId, const String* source, const String* srcName);
    static std::string buildSourceCodeMessage(uint8_t requestId, const String* source);
};

} // namespace Escargot
#endif /* ESCARGOT_DEBUGGER */

#endif
