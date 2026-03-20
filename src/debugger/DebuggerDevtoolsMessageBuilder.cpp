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
#include "interpreter/ByteCode.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace Escargot {

static std::string jsonEscape(const LChar* src, size_t len)
{
    std::string out;
    out.reserve(len + 16);

    for (size_t i = 0; i < len; ++i) {
        LChar c = src[i];
        switch (c) {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }

    return out;
}

static void computeEndLocation(const LChar* src, size_t length, uint32_t& endLine, uint32_t& endColumn)
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

std::string DebuggerDevtoolsMessageBuilder::buildScriptParsedMessage(const uint8_t scriptId, const String* source, const String* srcName)
{
    if (!source || !srcName || !source->is8Bit() || !srcName->is8Bit()) {
        return {};
    }

    const LChar* sourceName = srcName->characters8();
    const LChar* sourceCode = source->characters8();
    size_t sourceNameLength = srcName->length();
    size_t sourceCodeLength = source->length();

    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    computeEndLocation(sourceCode, sourceCodeLength, endLine, endColumn);

    char buffer[512];
    int written = snprintf(buffer, sizeof(buffer),
                           "{\"method\":\"Debugger.scriptParsed\","
                           "\"params\":{"
                           "\"scriptId\":\"%u\","
                           "\"url\":\"%.*s\","
                           "\"startLine\":0,"
                           "\"startColumn\":0,"
                           "\"endLine\":%u,"
                           "\"endColumn\":%u,"
                           "\"executionContextId\":1"
                           "}"
                           "}",
                           scriptId,
                           static_cast<int>(sourceNameLength),
                           reinterpret_cast<const char*>(sourceName),
                           endLine,
                           endColumn);

    if (written < 0 || static_cast<size_t>(written) >= sizeof(buffer)) {
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
    size_t sourceCodeLength = source->length();
    // Special characters in source code must be escaped.
    std::string escapedSource = jsonEscape(sourceCode, sourceCodeLength);

    // FIXME: buffer size depends on length of source.
    char buffer[4096];
    int written = snprintf(buffer, sizeof(buffer),
                           "{\"id\":%u,"
                           "\"result\":{"
                           "\"scriptSource\":\"%s\""
                           "}"
                           "}",
                           requestId,
                           escapedSource.c_str());

    if (written < 0 || static_cast<size_t>(written) >= sizeof(buffer)) {
        return {};
    }

    return { buffer, static_cast<size_t>(written) };
}

} // namespace Escargot

#endif /* ESCARGOT_DEBUGGER */
