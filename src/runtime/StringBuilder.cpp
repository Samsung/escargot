#include "Escargot.h"
#include "StringBuilder.h"

namespace Escargot {

void StringBuilder::appendPiece(String* str, size_t s, size_t e)
{
    // TODO
    // if (static_cast<int64_t>(m_contentLength) > static_cast<int64_t>(ESString::maxLength() - (e - s)))
    //     ESVMInstance::currentInstance()->throwOOMError();

    StringBuilderPiece piece;
    piece.m_string = str;
    piece.m_start = s;
    piece.m_end = e;
    if (!str->hasASCIIContent()) {
        m_hasASCIIContent = false;
    }
    m_contentLength += e - s;
    if (m_piecesInlineStorageUsage < ESCARGOT_STRING_BUILDER_INLINE_STORAGE_MAX) {
        m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
    } else
        m_pieces.push_back(piece);
}

String* StringBuilder::finalize()
{
    if (m_hasASCIIContent) {
        ASCIIStringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i ++) {
            String* data = m_piecesInlineStorage[i].m_string;
            size_t s = m_piecesInlineStorage[i].m_start;
            size_t e = m_piecesInlineStorage[i].m_end;
            for (size_t j = s; j < e; j ++) {
                ret[currentLength++] = data->charAt(j);
            }
        }

        for (size_t i = 0; i < m_pieces.size(); i ++) {
            String* data = m_pieces[i].m_string;
            size_t s = m_pieces[i].m_start;
            size_t e = m_pieces[i].m_end;
            for (size_t j = s; j < e; j ++) {
                ret[currentLength++] = data->charAt(j);
            }
        }

        return new ASCIIString(std::move(ret));
    } else {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(m_contentLength);

        size_t currentLength = 0;
        for (size_t i = 0; i < m_piecesInlineStorageUsage; i ++) {
            String* data = m_piecesInlineStorage[i].m_string;
            size_t s = m_piecesInlineStorage[i].m_start;
            size_t e = m_piecesInlineStorage[i].m_end;
            for (size_t j = s; j < e; j ++) {
                ret[currentLength++] = data->charAt(j);
            }
        }

        for (size_t i = 0; i < m_pieces.size(); i ++) {
            String* data = m_pieces[i].m_string;
            size_t s = m_pieces[i].m_start;
            size_t e = m_pieces[i].m_end;
            for (size_t j = s; j < e; j ++) {
                ret[currentLength++] = data->charAt(j);
            }
        }

        return new UTF16String(std::move(ret));
    }
}

}

