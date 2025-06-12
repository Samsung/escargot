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
#include "StringView.h"

namespace Escargot {

StringView* StringBuilderBase::initLongPiece(String* str, size_t s, size_t e)
{
    return new StringView(str, s, e);
}

void StringBuilderBase::throwStringLengthInvalidError(ExecutionState& state)
{
    ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
}

static void processPiece(LChar* buffer, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Char) {
        buffer[currentLength++] = (LChar)piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        memcpy(&buffer[currentLength], data, l);
        currentLength += l;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::String) {
        const auto& accessData = piece.m_string->bufferAccessData();
        const auto& l = accessData.length;
        if (accessData.has8BitContent) {
            memcpy(&buffer[currentLength], accessData.bufferAs8Bit, l);
            currentLength += l;
        } else {
            auto* b = accessData.bufferAs16Bit;
            for (size_t k = 0; k < l; k++) {
                buffer[currentLength++] = b[k];
            }
        }
    } else {
        String* data = piece.m_string;
        size_t s = piece.m_start;
        size_t e = piece.m_start + piece.m_length;
        size_t l = piece.m_length;
        const auto& accessData = data->bufferAccessData();
        if (accessData.has8BitContent) {
            memcpy(&buffer[currentLength], accessData.bufferAs8Bit + s, l);
            currentLength += l;
        } else {
            auto* b = accessData.bufferAs16Bit;
            for (size_t k = s; k < e; k++) {
                buffer[currentLength++] = b[k];
            }
        }
    }
}

static void processPiece(char16_t* buffer, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Char) {
        buffer[currentLength++] = piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        for (size_t j = 0; j < l; j++) {
            buffer[currentLength++] = data[j];
        }
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::String) {
        String* data = piece.m_string;
        size_t l = data->length();
        if (data->has8BitContent()) {
            auto ptr = data->characters8();
            for (size_t j = 0; j < l; j++) {
                buffer[currentLength++] = ptr[j];
            }
        } else {
            auto ptr = data->characters16();
            for (size_t j = 0; j < l; j++) {
                buffer[currentLength++] = ptr[j];
            }
        }
    } else {
        String* data = piece.m_string;
        size_t s = piece.m_start;
        size_t e = piece.m_start + piece.m_length;
        size_t l = piece.m_length;
        if (data->has8BitContent()) {
            auto ptr = data->characters8();
            ptr += s;
            for (size_t j = 0; j < l; j++) {
                buffer[currentLength++] = ptr[j];
            }
        } else {
            auto ptr = data->characters16();
            ptr += s;
            for (size_t j = 0; j < l; j++) {
                buffer[currentLength++] = ptr[j];
            }
        }
    }
}


String* StringBuilderBase::finalizeBase(StringBuilderPiece* piecesInlineStorage, Optional<ExecutionState*> state)
{
    if (!m_contentLength) {
        clear();
        return String::emptyString();
    }

    if (state && UNLIKELY(m_contentLength > STRING_MAXIMUM_LENGTH)) {
        throwStringLengthInvalidError(*state.value());
    }

    if (m_has8BitContent) {
        Latin1StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            processPiece(ret.data(), piece, currentLength);
        }

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            processPiece(ret.data(), piece, currentLength);
        }

        clear();
        return new Latin1String(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            processPiece(ret.data(), piece, currentLength);
        }

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            processPiece(ret.data(), piece, currentLength);
        }

        clear();
        return new UTF16String(std::move(ret));
    }
}

} // namespace Escargot
