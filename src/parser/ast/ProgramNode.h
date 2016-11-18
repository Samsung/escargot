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

#ifndef ProgramNode_h
#define ProgramNode_h

#include "Node.h"
#include "StatementNode.h"

namespace Escargot {

class ProgramNode : public Node {
public:
    friend class ScriptParser;
    ProgramNode(StatementNodeVector&& body, bool isStrict)
        : Node()
    {
        m_isStrict = isStrict;
        m_body = body;
    }

    virtual ASTNodeType type() { return ASTNodeType::Program; }

    bool isStrict()
    {
        return m_isStrict;
    }

protected:
    bool m_isStrict;
    StatementNodeVector m_body; // body: [ Statement ];
};

}

#endif
