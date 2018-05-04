/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
    friend struct GlobalRegExpFunctionObjectBuiltinFunctions;

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
