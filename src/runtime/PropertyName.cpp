#include "Escargot.h"
#include "PropertyName.h"
#include "Context.h"

namespace Escargot {

PropertyName::PropertyName(ExecutionState& state, String* string)
{
    StringBufferAccessData data = string->bufferAccessData();
    if (data.length == 0) {
        m_data = ((size_t)AtomicString().string()) | 1;
        return;
    }
    bool needsRemainNormalString = false;
    char16_t c = data.has8BitContent ? ((LChar*)data.buffer)[0] : ((char16_t*)data.buffer)[0];
    if ((c == '.' || (c >= '0' && c <= '9')) && data.length > 16) {
        needsRemainNormalString = true;
    }

    if (UNLIKELY(needsRemainNormalString)) {
        m_data = (size_t)string;
    } else {
        if (c < ESCARGOT_ASCII_TABLE_MAX && (data.length == 1)) {
            m_data = ((size_t)state.context()->staticStrings().asciiTable[c].string()) | 1;
        } else {
            m_data = ((size_t)AtomicString(state, string).string()) | 1;
        }
    }
}
}
