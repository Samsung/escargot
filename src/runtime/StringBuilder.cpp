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
#include "StringBuilder.h"
#include "ExecutionState.h"
#include "ErrorObject.h"

namespace Escargot {

void StringBuilderBase::throwStringLengthInvalidError(ExecutionState& state)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
}

ALWAYS_INLINE void appendPiece8(LChar* ret, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Char) {
        ret[currentLength++] = (LChar)piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        memcpy(&ret[currentLength], data, l);
        currentLength += l;
    } else {
        String* data = piece.m_string;
        size_t s = piece.m_start;
        size_t e = piece.m_length + s;
        size_t l = piece.m_length;
        const auto& accessData = data->bufferAccessData();
        if (accessData.has8BitContent) {
            memcpy(&ret[currentLength], ((LChar*)accessData.buffer) + s, l);
            currentLength += l;
        } else {
            char16_t* b = ((char16_t*)accessData.buffer);
            for (size_t k = s; k < e; k++) {
                ret[currentLength++] = b[k];
            }
        }
    }
}

ALWAYS_INLINE void appendPiece16(char16_t* ret, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Char) {
        ret[currentLength++] = piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        for (size_t j = 0; j < l; j++) {
            ret[currentLength++] = data[j];
        }
    } else {
        String* data = piece.m_string;
        size_t s = piece.m_start;
        size_t l = piece.m_length;
        if (data->has8BitContent()) {
            auto ptr = data->characters8();
            ptr += s;
            for (size_t j = 0; j < l; j++) {
                ret[currentLength++] = ptr[j];
            }
        } else {
            auto ptr = data->characters16();
            ptr += s;
            for (size_t j = 0; j < l; j++) {
                ret[currentLength++] = ptr[j];
            }
        }
    }
}


String* StringBuilderBase::finalizeBase(StringBuilderPiece* piecesInlineStorage, Optional<ExecutionState*> state)
{
    auto length = contentLength();
    if (!length) {
        clear();
        return String::emptyString();
    }

    checkStringLengthLimit(state, length);
    processInlineBufferAndMergeWithProcessedString(piecesInlineStorage);
    String* str = m_processedString;
    clear();

    return str;
}

void StringBuilderBase::processInlineBufferAndMergeWithProcessedString(StringBuilderPiece* piecesInlineStorage)
{
    String* str;
    if (m_has8BitContent) {
        Latin1StringData retString;
        LChar* ret = nullptr;
        if (m_internalStorageContentLength <= LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE) {
            ret = static_cast<LChar*>(alloca(m_internalStorageContentLength));
        } else {
            retString.resizeWithUninitializedValues(m_internalStorageContentLength);
            ret = retString.data();
        }

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            appendPiece8(ret, piece, currentLength);
        }

        clearInternal();
        if (currentLength <= LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE) {
            str = String::fromLatin1(ret, currentLength);
        } else {
            str = new Latin1String(std::move(retString));
        }
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_internalStorageContentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            appendPiece16(ret.data(), piece, currentLength);
        }

        clearInternal();
        str = new UTF16String(std::move(ret));
    }
    if (m_processedString->length()) {
        m_processedString = RopeString::createRopeString(m_processedString, str);
    } else {
        m_processedString = str;
    }
    clearInternal();
}

} // namespace Escargot
