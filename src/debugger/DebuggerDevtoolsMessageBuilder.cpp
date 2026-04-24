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

#include "DebuggerDevtoolsMessageBuilder.h"

#ifdef ESCARGOT_DEBUGGER

#include "Escargot.h"

#include "runtime/String.h" // for split function
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace Escargot {

std::string DebuggerDevtoolsMessageBuilder::buildEmptyMessage(const uint32_t id)
{
    char buffer[64];
    const int written = snprintf(buffer, sizeof(buffer),
                                 R"({"id":%u,"result":{}})",
                                 id);

    if (written < 0) {
        return {};
    }

    return { buffer, static_cast<size_t>(written) };
}

std::string DebuggerDevtoolsMessageBuilder::buildSourceCodeMessage(const uint8_t requestId, const String* source)
{
    if (!source || !source->is8Bit()) {
        return {};
    }

    const LChar* sourceCode = source->characters8();
    const size_t sourceCodeLength = source->length();

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.String(reinterpret_cast<const rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>>::Ch*>(sourceCode), sourceCodeLength);
    const std::string escaped = sb.GetString();

    // FIXME: buffer size depends on length of source.
    char buffer[4096];
    const int written = snprintf(buffer, sizeof(buffer),
                                 "{\"id\":%u,"
                                 "\"result\":{"
                                 "\"scriptSource\":%s"
                                 "}"
                                 "}",
                                 requestId,
                                 escaped.c_str());

    if (written < 0 || static_cast<size_t>(written) >= sizeof(buffer)) {
        return {};
    }

    return { buffer, static_cast<size_t>(written) };
}

} // namespace Escargot

#endif /* ESCARGOT_DEBUGGER */
