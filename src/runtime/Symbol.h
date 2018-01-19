/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotSymbol__
#define __EscargotSymbol__

#include "runtime/PointerValue.h"

namespace Escargot {

extern size_t g_symbolTag;

class Symbol : public PointerValue {
public:
    Symbol(String* desc = String::emptyString)
    {
        m_tag = POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA;
        m_description = desc;
    }

    virtual bool isSymbol() const
    {
        return true;
    }

    String* description() const
    {
        return m_description;
    }

    String* getSymbolDescriptiveString() const;

protected:
    size_t m_tag;
    String* m_description;
};
}

#endif
