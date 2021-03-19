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

String* StringBuilderBase::finalizeBase(StringBuilderPiece* piecesInlineStorage, ExecutionState* state)
{
    if (!m_contentLength) {
        clear();
        return String::emptyString;
    }


    if (state && UNLIKELY(m_contentLength > STRING_MAXIMUM_LENGTH)) {
        ErrorObject::throwBuiltinError(*state, ErrorObject::RangeError, ErrorObject::Messages::String_InvalidStringLength);
    }

    if (m_has8BitContent) {
        Latin1StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
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

        clear();
        return new Latin1String(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            const StringBuilderPiece& piece = piecesInlineStorage[i];
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

        clear();
        return new UTF16String(std::move(ret));
    }
}

} // namespace Escargot
