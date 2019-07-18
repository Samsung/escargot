/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
    explicit IdentifierNode(const AtomicString& name)
        : Node()
        , m_name(name)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Identifier; }
    AtomicString name()
    {
        return m_name;
    }

    bool shouldUseNamedOperationOnGlobal(ByteCodeGenerateContext* context)
    {
        return context->m_codeBlock->asInterpretedCodeBlock()->inNotIndexedCodeBlockScope() || context->m_codeBlock->asInterpretedCodeBlock()->inCatchWith() || context->m_codeBlock->asInterpretedCodeBlock()->inEvalWithScope() || context->m_catchScopeCount || context->m_isEvalCode || context->m_codeBlock->asInterpretedCodeBlock()->hasWith();
    }

    void addLexcialVariableErrorsIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, InterpretedCodeBlock::IndexedIdentifierInfo info, bool isLexicallyDeclaredBindingInitialization, bool isVariableChainging = false)
    {
        // <temporal dead zone error>
        // only stack allocated lexical variables needs check (heap variables are checked on runtime)
        if (!isLexicallyDeclaredBindingInitialization && info.m_isResultSaved && info.m_isStackAllocated && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            bool finded = false;
            auto iter = context->m_lexicallyDeclaredNames->rbegin();
            while (iter != context->m_lexicallyDeclaredNames->rend()) {
                if (*iter == m_name) {
                    finded = true;
                    break;
                }
                iter++;
            }
            if (!finded) {
                codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::ReferenceError, errorMessage_IsNotInitialized, m_name), context, this);
            }
        }

        // <const variable check>
        // every indexed variables are checked on bytecode generation time
        if (!isLexicallyDeclaredBindingInitialization && isVariableChainging && info.m_isResultSaved && !info.m_isMutable && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable, m_name), context, this);
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        bool isFunctionDeclarationBindingInitialization = context->m_isFunctionDeclarationBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;
        context->m_isFunctionDeclarationBindingInitialization = false;

        if (context->m_codeBlock->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
            if (shouldUseNamedOperationOnGlobal(context)) {
                if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization) {
                    codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                }
            } else {
                InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
                addLexcialVariableErrorsIfNeeds(codeBlock, context, info, isLexicallyDeclaredBindingInitialization, true);

                if (!info.m_isResultSaved) {
                    codeBlock->pushCode(SetGlobalObject(ByteCodeLOC(m_loc.index), srcRegister, PropertyName(m_name)), context, this);
                } else {
                    ASSERT(!info.m_isStackAllocated);
                    size_t cIdx = context->m_catchScopeCount;
                    if (isLexicallyDeclaredBindingInitialization) {
                        ASSERT(info.m_upperIndex + cIdx == 0);
                        codeBlock->pushCode(InitializeByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_index), context, this);
                    } else {
                        codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_upperIndex + cIdx, info.m_index), context, this);
                    }
                }
            }
        } else if (context->m_codeBlock->asInterpretedCodeBlock()->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            addLexcialVariableErrorsIfNeeds(codeBlock, context, info, isLexicallyDeclaredBindingInitialization, true);

            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->asInterpretedCodeBlock()->inNotIndexedCodeBlockScope() && !context->m_codeBlock->asInterpretedCodeBlock()->inCatchWith() && !context->m_codeBlock->asInterpretedCodeBlock()->inEvalWithScope() && !context->m_catchScopeCount && !context->m_isEvalCode && !context->m_codeBlock->asInterpretedCodeBlock()->hasWith()) {
                    codeBlock->pushCode(SetGlobalObject(ByteCodeLOC(m_loc.index), srcRegister, PropertyName(m_name)), context, this);
                } else {
                    if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization) {
                        codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this);
                    } else {
                        codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                    }
                }
            } else {
                if (context->m_isWithScope || (context->m_catchScopeCount && m_name == context->m_lastCatchVariableName)) {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                    return;
                }

                if (info.m_type != InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
                    if (!info.m_isMutable) {
                        if (codeBlock->m_codeBlock->isStrict())
                            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable, m_name), context, this);
                        return;
                    }
                }

                if (info.m_isStackAllocated) {
                    if (srcRegister != REGULAR_REGISTER_LIMIT + info.m_index) {
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), srcRegister, REGULAR_REGISTER_LIMIT + info.m_index), context, this);
                    }
                } else {
                    size_t cIdx = context->m_catchScopeCount;
                    if (isLexicallyDeclaredBindingInitialization) {
                        ASSERT(info.m_upperIndex + cIdx == 0);
                        codeBlock->pushCode(InitializeByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_index), context, this);
                    } else {
                        codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_upperIndex + cIdx, info.m_index), context, this);
                    }
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->asInterpretedCodeBlock()->canAllocateEnvironmentOnStack());
            if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization) {
                codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this);
            } else {
                codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
            }
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (context->m_codeBlock->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
            if (shouldUseNamedOperationOnGlobal(context)) {
                codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
            } else {
                InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
                addLexcialVariableErrorsIfNeeds(codeBlock, context, info, false);

                if (!info.m_isResultSaved) {
                    codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), dstRegister, PropertyName(m_name)), context, this);
                } else {
                    ASSERT(!info.m_isStackAllocated);
                    size_t cIdx = context->m_catchScopeCount;
                    codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), dstRegister, info.m_upperIndex + cIdx, info.m_index), context, this);
                }
            }
        } else if (context->m_codeBlock->asInterpretedCodeBlock()->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            addLexcialVariableErrorsIfNeeds(codeBlock, context, info, false);

            if (!info.m_isResultSaved) {
                if (!context->m_codeBlock->asInterpretedCodeBlock()->inNotIndexedCodeBlockScope() && !context->m_codeBlock->asInterpretedCodeBlock()->inCatchWith() && !context->m_codeBlock->asInterpretedCodeBlock()->inEvalWithScope() && !context->m_catchScopeCount && !context->m_isEvalCode && !context->m_codeBlock->asInterpretedCodeBlock()->hasWith()) {
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
            ASSERT(!context->m_codeBlock->asInterpretedCodeBlock()->canAllocateEnvironmentOnStack());
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

    std::tuple<bool, ByteCodeRegisterIndex, InterpretedCodeBlock::IndexedIdentifierInfo> isAllocatedOnStack(ByteCodeGenerateContext* context)
    {
        if (context->m_codeBlock->asInterpretedCodeBlock()->canUseIndexedVariableStorage() || context->m_codeBlock->asInterpretedCodeBlock()->isGlobalScopeCodeBlock()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            if (!info.m_isResultSaved) {
                return std::make_tuple(false, REGISTER_LIMIT, info);
            } else {
                if (context->m_isWithScope || (context->m_catchScopeCount && m_name == context->m_lastCatchVariableName)) {
                    return std::make_tuple(false, REGISTER_LIMIT, info);
                }

                if (info.m_isStackAllocated && info.m_isMutable) {
                    if (context->m_canSkipCopyToRegister)
                        return std::make_tuple(true, REGULAR_REGISTER_LIMIT + info.m_index, info);
                    else
                        return std::make_tuple(false, REGISTER_LIMIT, info);
                } else {
                    return std::make_tuple(false, REGISTER_LIMIT, info);
                }
            }
        } else {
            return std::make_tuple(false, REGISTER_LIMIT, InterpretedCodeBlock::IndexedIdentifierInfo());
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        auto ret = isAllocatedOnStack(context);
        if (std::get<0>(ret)) {
            context->pushRegister(std::get<1>(ret));
            return std::get<1>(ret);
        } else {
            return context->getRegister();
        }
    }


    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        fn(m_name, false);
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        fn(m_name, true);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

private:
    AtomicString m_name;
};
}

#endif
