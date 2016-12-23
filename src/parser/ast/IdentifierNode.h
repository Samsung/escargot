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

#ifndef IdentifierNode_h
#define IdentifierNode_h

#include "ExpressionNode.h"
#include "Node.h"
#include "PatternNode.h"

namespace Escargot {

// interface Identifier <: Node, Expression, Pattern {
class IdentifierNode : public Node {
public:
    friend class ScriptParser;
    IdentifierNode(const AtomicString& name)
        : Node()
    {
        m_name = name;
    }

    virtual ASTNodeType type() { return ASTNodeType::Identifier; }
    AtomicString name()
    {
        return m_name;
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_name.string()->equals("arguments") && !context->m_codeBlock->isGlobalScopeCodeBlock() && context->m_codeBlock->usesArgumentsObject()) {
            codeBlock->pushCode(StoreArgumentsObject(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex()), context, this);
            return;
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock()) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                /*if (!context->m_codeBlock->inEvalWithCatchYieldScope() && context->m_codeBlock->hasNonConfiguableNameOnGlobal(m_name)) {
                    ExecutionState state(context->m_codeBlock->context());
                    codeBlock->pushCode(StoreByGlobalName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), PropertyName(state, m_name)), context, this);
                } else */ if (!context->m_codeBlock->inEvalWithCatchYieldScope()) {
                    ExecutionState state(context->m_codeBlock->context());
                    codeBlock->pushCode(SetGlobalObject(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), PropertyName(state, m_name)), context, this);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), m_name), context, this);
                }
            } else {
                if (info.m_isStackAllocated) {
                    codeBlock->pushCode(StoreByStackIndex(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), info.m_index), context, this);
                } else {
                    codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), info.m_upperIndex, info.m_index), context, this);
                }
            }
        } else {
            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), m_name), context, this);
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_name.string()->equals("arguments") && !context->m_codeBlock->isGlobalScopeCodeBlock() && context->m_codeBlock->usesArgumentsObject()) {
            codeBlock->pushCode(LoadArgumentsObject(ByteCodeLOC(m_loc.index), context->getRegister()), context, this);
            return;
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock()) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                /*if (!context->m_codeBlock->inEvalWithCatchYieldScope() && context->m_codeBlock->hasNonConfiguableNameOnGlobal(m_name)) {
                    ExecutionState state(context->m_codeBlock->context());
                    codeBlock->pushCode(LoadByGlobalName(ByteCodeLOC(m_loc.index), context->getRegister(), PropertyName(state, m_name)), context, this);
                } else */ if (!context->m_codeBlock->inEvalWithCatchYieldScope()) {
                    ExecutionState state(context->m_codeBlock->context());
                    codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), context->getRegister(), PropertyName(state, m_name)), context, this);
                } else {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), context->getRegister(), m_name), context, this);
                }
            } else {
                if (info.m_isStackAllocated) {
                    codeBlock->pushCode(LoadByStackIndex(ByteCodeLOC(m_loc.index), context->getRegister(), info.m_index), context, this);
                } else {
                    codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), context->getRegister(), info.m_upperIndex, info.m_index), context, this);
                }
            }
        } else {
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), context->getRegister(), m_name), context, this);
        }
    }

protected:
    AtomicString m_name;
};
}

#endif
