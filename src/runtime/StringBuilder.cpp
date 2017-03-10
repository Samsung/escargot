/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
        if (!str->has8BitContent()) {
            m_has8BitContent = false;
        }
        m_contentLength += e - s;
        if (m_piecesInlineStorageUsage < STRING_BUILDER_INLINE_STORAGE_MAX) {
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        } else
            m_pieces.push_back(piece);
    }
}

String* StringBuilder::finalize(ExecutionState* state)
{
    if (!m_contentLength) {
        return String::emptyString;
    }


    if (state) {
        if (UNLIKELY(m_contentLength > STRING_MAXIMUM_LENGTH)) {
            ErrorObject::throwBuiltinError(*state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
        }
    }

    if (m_has8BitContent) {
        Latin1StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            String* data = m_piecesInlineStorage[i].m_string;
            size_t s = m_piecesInlineStorage[i].m_start;
            size_t e = m_piecesInlineStorage[i].m_end;
            size_t l = e - s;
            memcpy(&ret[currentLength], data->characters8() + s, l);
            currentLength += l;
        }

        for (size_t i = 0; i < m_pieces.size(); i++) {
            String* data = m_pieces[i].m_string;
            size_t s = m_pieces[i].m_start;
            size_t e = m_pieces[i].m_end;
            size_t l = e - s;
            memcpy(&ret[currentLength], data->characters8() + s, l);
            currentLength += l;
        }

        return new Latin1String(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i++) {
            String* data = m_piecesInlineStorage[i].m_string;
            size_t s = m_piecesInlineStorage[i].m_start;
            size_t e = m_piecesInlineStorage[i].m_end;
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

        for (size_t i = 0; i < m_pieces.size(); i++) {
            String* data = m_pieces[i].m_string;
            size_t s = m_pieces[i].m_start;
            size_t e = m_pieces[i].m_end;
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

        return new UTF16String(std::move(ret));
    }
}
}
