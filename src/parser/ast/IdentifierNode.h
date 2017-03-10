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

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf = true)
    {
        if (m_name.string()->equals("arguments") && !context->isGlobalScope() && context->m_codeBlock->usesArgumentsObject()) {
            codeBlock->pushCode(StoreArgumentsObject(ByteCodeLOC(m_loc.index), srcRegister), context, this);
            return;
        }

        if ((context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock())) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->inCatchWith() && !context->m_codeBlock->inEvalScope() && !context->m_catchScopeCount && !context->m_isEvalCode && !context->m_codeBlock->hasWith()) {
                    codeBlock->pushCode(SetGlobalObject(ByteCodeLOC(m_loc.index), srcRegister, PropertyName(m_name)), context, this);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                }
            } else {
                if (context->m_isWithScope || (context->m_catchScopeCount && m_name == context->m_lastCatchVariableName)) {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                    return;
                }

                if (info.m_isStackAllocated) {
                    if (srcRegister != REGULAR_REGISTER_LIMIT + info.m_index) {
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), srcRegister, REGULAR_REGISTER_LIMIT + info.m_index), context, this);
                    }
                } else {
                    size_t cIdx = context->m_catchScopeCount;
                    codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_upperIndex + cIdx, info.m_index), context, this);
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (m_name.string()->equals("arguments") && !context->isGlobalScope() && context->m_codeBlock->usesArgumentsObject()) {
            if (UNLIKELY(context->m_isWithScope))
                codeBlock->pushCode(LoadArgumentsInWithScope(ByteCodeLOC(m_loc.index), dstRegister), context, this);
            else
                codeBlock->pushCode(LoadArgumentsObject(ByteCodeLOC(m_loc.index), dstRegister), context, this);
            return;
        }

        if ((context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock())) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->inCatchWith() && !context->m_codeBlock->inEvalScope() && !context->m_catchScopeCount && !context->m_isEvalCode && !context->m_codeBlock->hasWith()) {
                    codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), dstRegister, PropertyName(m_name)), context, this);
                } else {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
                }
            } else {
                if (context->m_isWithScope || (context->m_catchScopeCount && m_name == context->m_lastCatchVariableName)) {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
                    return;
                }

                if (info.m_isStackAllocated) {
                    if (context->m_canSkipCopyToRegister) {
                        if (dstRegister != (REGULAR_REGISTER_LIMIT + info.m_index)) {
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this);
                        }
                    } else
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this);
                } else {
                    size_t cIdx = context->m_catchScopeCount;
                    codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), dstRegister, info.m_upperIndex + cIdx, info.m_index), context, this);
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
        }
    }


    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        generateExpressionByteCode(codeBlock, context, getRegister(codeBlock, context));
    }

    std::pair<bool, ByteCodeRegisterIndex> isAllocatedOnStack(ByteCodeGenerateContext* context)
    {
        if (m_name.string()->equals("arguments") && !context->isGlobalScope() && context->m_codeBlock->usesArgumentsObject()) {
            return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
        }
        if ((context->m_codeBlock->canUseIndexedVariableStorage() || context->m_codeBlock->isGlobalScopeCodeBlock())) {
            CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name);
            if (!info.m_isResultSaved) {
                return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
            } else {
                if (context->m_isWithScope || (context->m_catchScopeCount && m_name == context->m_lastCatchVariableName)) {
                    return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
                }

                if (info.m_isStackAllocated) {
                    if (context->m_canSkipCopyToRegister)
                        return std::make_pair(true, REGULAR_REGISTER_LIMIT + info.m_index);
                    else
                        return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
                } else {
                    return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
                }
            }
        } else {
            return std::make_pair(false, std::numeric_limits<ByteCodeRegisterIndex>::max());
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        auto ret = isAllocatedOnStack(context);
        if (ret.first) {
            context->pushRegister(ret.second);
            return ret.second;
        } else {
            return context->getRegister();
        }
    }

protected:
    AtomicString m_name;
};
}

#endif
