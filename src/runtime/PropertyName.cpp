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
