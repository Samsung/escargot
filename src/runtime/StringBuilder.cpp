/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

void StringBuilder::appendPiece(String* str, size_t s, size_t e)
{
    if (e - s > 0) {
        StringBuilderPiece piece;
        piece.m_string = str;
        piece.m_start = s;
        piece.m_end = e;

        const auto& data = str->bufferAccessData();
        if (!data.has8BitContent) {
            bool has8 = true;
            for (size_t i = s; i < e; i++) {
                if (((char16_t*)data.buffer)[i] > 255) {
                    has8 = false;
                    break;
                }
            }

            if (!has8) {
                m_has8BitContent = false;
                piece.m_type = StringBuilderPiece::Type::UTF16StringStringPiece;
            } else {
                piece.m_type = StringBuilderPiece::Type::UTF16StringStringButLatin1ContentPiece;
            }

        } else {
            piece.m_type = StringBuilderPiece::Type::Latin1StringPiece;
        }

        m_contentLength += e - s;
        if (m_piecesInlineStorageUsage < STRING_BUILDER_INLINE_STORAGE_MAX) {
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        } else
            m_pieces.push_back(piece);
    }
}

void StringBuilder::appendPiece(const char* str)
{
    StringBuilderPiece piece;
    piece.m_start = 0;
    piece.m_end = strlen(str);
    piece.m_raw = str;
    piece.m_type = StringBuilderPiece::Type::ConstChar;
    if (piece.m_end) {
        m_contentLength += piece.m_end;
        if (m_piecesInlineStorageUsage < STRING_BUILDER_INLINE_STORAGE_MAX) {
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        } else
            m_pieces.push_back(piece);
    }
}

void StringBuilder::appendPiece(char16_t ch)
{
    StringBuilderPiece piece;
    piece.m_start = 0;
    piece.m_end = 1;
    piece.m_ch = ch;
    piece.m_type = StringBuilderPiece::Type::Char;

    if (ch > 255) {
        m_has8BitContent = false;
    }

    m_contentLength += 1;
    if (m_piecesInlineStorageUsage < STRING_BUILDER_INLINE_STORAGE_MAX) {
        m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
    } else
        m_pieces.push_back(piece);
}

String* StringBuilder::finalize(ExecutionState* state)
{
    if (!m_contentLength) {
        return String::emptyString;
    }


    if (state && UNLIKELY(m_contentLength > STRING_MAXIMUM_LENGTH)) {
        ErrorObject::throwBuiltinError(*state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
    }

    if (m_has8BitContent) {
        Latin1StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = m_piecesInlineStorage[i];
            if (piece.m_type == StringBuilderPiece::Char) {
                ret[currentLength++] = (LChar)piece.m_ch;
            } else if (piece.m_type == StringBuilderPiece::ConstChar) {
                const char* data = piece.m_raw;
                size_t l = piece.m_end;
                memcpy(&ret[currentLength], data, l);
                currentLength += l;
            } else {
                String* data = piece.m_string;
                size_t s = piece.m_start;
                size_t e = piece.m_end;
                size_t l = e - s;
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

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            if (piece.m_type == StringBuilderPiece::Char) {
                ret[currentLength++] = (LChar)piece.m_ch;
            } else if (piece.m_type == StringBuilderPiece::ConstChar) {
                const char* data = piece.m_raw;
                size_t l = piece.m_end;
                memcpy(&ret[currentLength], data, l);
                currentLength += l;
            } else {
                String* data = piece.m_string;
                size_t s = piece.m_start;
                size_t e = piece.m_end;
                size_t l = e - s;
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

        return new Char8String(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = m_piecesInlineStorage[i];
            if (piece.m_type == StringBuilderPiece::Char) {
                ret[currentLength++] = piece.m_ch;
            } else if (piece.m_type == StringBuilderPiece::ConstChar) {
                const char* data = piece.m_raw;
                size_t l = piece.m_end;
                for (size_t j = 0; j < l; j++) {
                    ret[currentLength++] = data[j];
                }
            } else {
                String* data = piece.m_string;
                size_t s = piece.m_start;
                size_t e = piece.m_end;
                size_t l = e - s;
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

        for (size_t i = 0; i < m_pieces.size(); i++) {
            const StringBuilderPiece& piece = m_pieces[i];
            if (piece.m_type == StringBuilderPiece::Char) {
                ret[currentLength++] = piece.m_ch;
            } else if (piece.m_type == StringBuilderPiece::ConstChar) {
                const char* data = piece.m_raw;
                size_t l = piece.m_end;
                for (size_t j = 0; j < l; j++) {
                    ret[currentLength++] = data[j];
                }
            } else {
                String* data = piece.m_string;
                size_t s = piece.m_start;
                size_t e = piece.m_end;
                size_t l = e - s;
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

        return new UTF16String(std::move(ret));
    }
}
}
