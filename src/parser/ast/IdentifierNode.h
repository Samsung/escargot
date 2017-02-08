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
#include "runtime/Context.h"

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

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool needToReferenceSelf = true)
    {
        if (m_name.string()->equals("arguments") && !context->isGlobalScope() && context->m_codeBlock->usesArgumentsObject()) {
            codeBlock->pushCode(StoreArgumentsObject(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex()), context, this);
            return;
        }

        if ((context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock())) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->inCatchWith() && !context->m_codeBlock->inEvalScope() && !context->m_isCatchScope && !context->m_isEvalCode && !context->m_codeBlock->hasWith()) {
                    ExecutionState state(context->m_codeBlock->context());
                    codeBlock->pushCode(SetGlobalObject(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), PropertyName(state, m_name)), context, this);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), m_name), context, this);
                }
            } else {
                if (context->m_isCatchScope && m_name == context->m_lastCatchVariableName) {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), m_name), context, this);
                    return;
                }

                if (info.m_isStackAllocated) {
                    codeBlock->pushCode(StoreByStackIndex(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), info.m_index), context, this);
                } else {
                    size_t cIdx = context->m_isCatchScope ? 1 : 0;
                    codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), info.m_upperIndex + cIdx, info.m_index), context, this);
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), context->getLastRegisterIndex(), m_name), context, this);
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_name.string()->equals("arguments") && !context->isGlobalScope() && context->m_codeBlock->usesArgumentsObject()) {
            if (UNLIKELY(context->m_isWithScope))
                codeBlock->pushCode(LoadArgumentsInWithScope(ByteCodeLOC(m_loc.index), context->getRegister()), context, this);
            else
                codeBlock->pushCode(LoadArgumentsObject(ByteCodeLOC(m_loc.index), context->getRegister()), context, this);
            return;
        }

        if ((context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock())) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->inCatchWith() && !context->m_codeBlock->inEvalScope() && !context->m_isCatchScope && !context->m_isEvalCode && !context->m_codeBlock->hasWith()) {
                    ExecutionState state(context->m_codeBlock->context());
                    size_t idx = state.context()->globalObject()->structure()->findProperty(state, m_name);
                    if (idx != SIZE_MAX) {
                        const ObjectStructureItem& item = state.context()->globalObject()->structure()->readProperty(state, idx);
                        if (item.m_descriptor.isPlainDataProperty()) {
                            codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), context->getRegister(), PropertyName(state, m_name), idx), context, this);
                            return;
                        }
                    }
                    codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), context->getRegister(), PropertyName(state, m_name)), context, this);
                } else {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), context->getRegister(), m_name), context, this);
                }
            } else {
                if (context->m_isCatchScope && m_name == context->m_lastCatchVariableName) {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), context->getRegister(), m_name), context, this);
                    return;
                }

                if (info.m_isStackAllocated) {
                    codeBlock->pushCode(LoadByStackIndex(ByteCodeLOC(m_loc.index), context->getRegister(), info.m_index), context, this);
                } else {
                    size_t cIdx = context->m_isCatchScope ? 1 : 0;
                    codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), context->getRegister(), info.m_upperIndex + cIdx, info.m_index), context, this);
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), context->getRegister(), m_name), context, this);
        }
    }


    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        generateExpressionByteCode(codeBlock, context);
    }

protected:
    AtomicString m_name;
};
}

#endif
