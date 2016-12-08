/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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

#ifndef RestElementNode_h
#define RestElementNode_h

#include "IdentifierNode.h"
#include "Node.h"

namespace Escargot {

class RestElementNode : public Node {
public:
    RestElementNode(IdentifierNode* argument)
        : Node()
    {
        m_argument = argument;
    }

    virtual ASTNodeType type() { return ASTNodeType::RestElement; }
    IdentifierNode* argument()
    {
        return m_argument;
    }

protected:
    IdentifierNode* m_argument;
};
}

#endif
