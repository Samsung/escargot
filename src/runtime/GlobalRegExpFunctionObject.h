/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotGlobalRegExpFunctionObject__
#define __EscargotGlobalRegExpFunctionObject__

#include "runtime/FunctionObject.h"
#include "runtime/RegExpObject.h"

namespace Escargot {

struct RegExpStatus {
    String* input; // RegExp.input ($_)
    StringView lastMatch;
    StringView lastParen;
    StringView leftContext;
    StringView rightContext;
    size_t pairCount;
    StringView pairs[9];

    RegExpStatus()
    {
        input = String::emptyString;
        lastParen = lastMatch = StringView();
        leftContext = rightContext = StringView();
        pairCount = 0;
        pairs[0] = pairs[1] = pairs[2] = pairs[3] = pairs[4] = pairs[5] = pairs[6] = pairs[7] = pairs[8] = StringView();
    }
};

class GlobalRegExpFunctionObject : public FunctionObject {
    friend class RegExpObject;
    friend class GlobalRegExpFunctionObjectBuiltinFunctions;

public:
    GlobalRegExpFunctionObject(ExecutionState& state);
    virtual bool isGlobalRegExpFunctionObject()
    {
        return true;
    }

protected:
    void initInternalProperties(ExecutionState& state);
    RegExpStatus m_status;
};
}

#endif
