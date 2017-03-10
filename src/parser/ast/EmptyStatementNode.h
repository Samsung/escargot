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

#ifndef EmptyStatmentNode_h
#define EmptyStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

// An empty statement, i.e., a solitary semicolon.
class EmptyStatementNode : public StatementNode {
public:
    EmptyStatementNode()
        : StatementNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::EmptyStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
    }

protected:
};
}

#endif
