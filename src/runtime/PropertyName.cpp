#include "Escargot.h"
#include "PropertyName.h"
#include "Context.h"

namespace Escargot {

PropertyName::PropertyName(ExecutionState& state, String* string)
{
    if (string->length() == 0) {
        m_data = ((size_t)AtomicString().string()) | 1;
        return;
    }
    bool needsRemainNormalString = false;
    char16_t c = string->charAt(0);
    if (c == '.' || (c >= '0' && c <= '9')) {
        needsRemainNormalString = true;
    }

    if (UNLIKELY(needsRemainNormalString)) {
        m_data = (size_t)string;
    } else {
        m_data = ((size_t)AtomicString(state, string).string()) | 1;
    }
    ASSERT(m_data);
}

}
