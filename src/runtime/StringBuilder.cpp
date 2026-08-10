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

// writes value's decimal digits (and a leading '-' if negative) into
// buffer[pos..pos+len), where len must be exactly decimalDigitLengthOf(value)
// (the caller already sized the destination from that, at append time)
template <typename CharType>
static void writeInt32Digits(CharType* buffer, size_t pos, size_t len, int32_t value)
{
    bool neg = value < 0;
    uint32_t v = neg ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    size_t end = pos + len;
    do {
        buffer[--end] = (CharType)('0' + (v % 10));
        v /= 10;
    } while (v);
    if (neg) {
        buffer[--end] = (CharType)'-';
    }
    ASSERT(end == pos);
}

static void processPiece(LChar* buffer, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength, const char* numberScratch)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Char) {
        buffer[currentLength++] = (LChar)piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        memcpy(&buffer[currentLength], data, l);
        currentLength += l;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Int32Digits) {
        writeInt32Digits(buffer, currentLength, piece.m_length, piece.m_int32Value);
        currentLength += piece.m_length;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Digits) {
        memcpy(&buffer[currentLength], numberScratch + piece.m_digitsOffset, piece.m_length);
        currentLength += piece.m_length;
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

static void processPiece(char16_t* buffer, const StringBuilderBase::StringBuilderPiece& piece, size_t& currentLength, const char* numberScratch)
{
    if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Char) {
        buffer[currentLength++] = piece.m_ch;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::ConstChar) {
        const char* data = piece.m_raw;
        size_t l = piece.m_length;
        for (size_t j = 0; j < l; j++) {
            buffer[currentLength++] = data[j];
        }
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Int32Digits) {
        writeInt32Digits(buffer, currentLength, piece.m_length, piece.m_int32Value);
        currentLength += piece.m_length;
    } else if (piece.m_type == StringBuilderBase::StringBuilderPiece::Type::Digits) {
        const char* data = numberScratch + piece.m_digitsOffset;
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

    const char* numberScratch = m_numberScratch ? m_numberScratch.value()->data() : nullptr;
    if (m_has8BitContent) {
        Latin1StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            processPiece(ret.data(), piece, currentLength, numberScratch);
        }

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            processPiece(ret.data(), piece, currentLength, numberScratch);
        }

        clear();
        return new Latin1String(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
            processPiece(ret.data(), piece, currentLength, numberScratch);
        }

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            processPiece(ret.data(), piece, currentLength, numberScratch);
        }

        clear();
        return new UTF16String(std::move(ret));
    }
}

} // namespace Escargot
