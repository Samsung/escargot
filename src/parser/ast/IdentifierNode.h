/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

namespace Escargot {

// interface Identifier <: Node, Expression, Pattern {
class IdentifierNode : public Node {
public:
    IdentifierNode()
        : Node()
        , m_name()
    {
    }

    explicit IdentifierNode(const AtomicString& name)
        : Node()
        , m_name(name)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::Identifier; }

    AtomicString name()
    {
        return m_name;
    }

    bool isPointsArgumentsObject(ByteCodeGenerateContext* context)
    {
        if (context->m_codeBlock->context()->staticStrings().arguments == m_name && context->m_codeBlock->usesArgumentsObject() && !context->m_codeBlock->isArrowFunctionExpression()) {
            return true;
        }
        return false;
    }

    void addLexicalVariableErrorsIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, InterpretedCodeBlock::IndexedIdentifierInfo info, bool isLexicallyDeclaredBindingInitialization, bool isVariableChainging = false)
    {
        // <temporal dead zone error>
        // only stack allocated lexical variables needs check (heap variables are checked on runtime)
        if (!isLexicallyDeclaredBindingInitialization && info.m_isResultSaved && info.m_isStackAllocated && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            bool find = false;
            auto iter = context->m_lexicallyDeclaredNames->begin();
            while (iter != context->m_lexicallyDeclaredNames->end()) {
                if (iter->first == info.m_blockIndex && iter->second == m_name) {
                    find = true;
                    break;
                }
                iter++;
            }

            if (!find) {
                codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), (uint8_t)ErrorCode::ReferenceError, ErrorObject::Messages::IsNotInitialized, m_name), context, this->m_loc.index);
            }
        }

        // <const variable check>
        // every indexed variables are checked on bytecode generation time
        if (!isLexicallyDeclaredBindingInitialization && isVariableChainging && info.m_isResultSaved && !info.m_isMutable && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), (uint8_t)ErrorCode::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, m_name), context, this->m_loc.index);
        }
    }

    void initUsingVariable(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister,
                           bool isLexicallyDeclaredBindingInitialization, bool isUsingBindingInitialization)
    {
        if (isLexicallyDeclaredBindingInitialization && isUsingBindingInitialization) {
            auto* bi = context->m_codeBlock->blockInfo(context->m_lexicalBlockIndex);
            size_t i;
            for (i = 0; i < bi->identifiers().size(); i++) {
                if (bi->identifiers()[i].m_name == m_name) {
                    ASSERT(bi->identifiers()[i].m_isUsing);
                    break;
                }
            }
            ASSERT(i != bi->identifiers().size());
            codeBlock->pushCode(InitializeDisposable(ByteCodeLOC(m_loc.index), context->m_isAwaitUsingBindingInitialization,
                                                     srcRegister, context->m_disposableRecordRegisterStack->back()),
                                context, this->m_loc.index);
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        bool isUsingBindingInitialization = context->m_isUsingBindingInitialization;
        bool isFunctionDeclarationBindingInitialization = context->m_isFunctionDeclarationBindingInitialization;
        bool isVarDeclaredBindingInitialization = context->m_isVarDeclaredBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;
        context->m_isUsingBindingInitialization = false;
        context->m_isFunctionDeclarationBindingInitialization = false;
        context->m_isVarDeclaredBindingInitialization = false;

        if (isLexicallyDeclaredBindingInitialization) {
            context->addLexicallyDeclaredNames(m_name);
        }

        if (context->m_inParameterInitialization && codeBlock->m_codeBlock->hasParameterName(m_name)) {
            context->addInitializedParameterNames(m_name);
        }

        if (isPointsArgumentsObject(context)) {
            codeBlock->pushCode(EnsureArgumentsObject(ByteCodeLOC(m_loc.index)), context, this->m_loc.index);
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name, context);
            addLexicalVariableErrorsIfNeeds(codeBlock, context, info, isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization, true);

            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->hasAncestorUsesNonIndexedVariableStorage()) {
                    size_t addressRegisterIndex = SIZE_MAX;
                    if (mayNeedsResolveAddress(codeBlock, context) && !needToReferenceSelf) {
                        addressRegisterIndex = context->getLastRegisterIndex();
                        context->giveUpRegister();
                    }
                    if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization || isVarDeclaredBindingInitialization) {
                        codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this->m_loc.index);
                    } else {
                        if (addressRegisterIndex != SIZE_MAX) {
                            codeBlock->pushCode(StoreByNameWithAddress(ByteCodeLOC(m_loc.index), addressRegisterIndex, srcRegister, m_name), context, this->m_loc.index);
                        } else {
                            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this->m_loc.index);
                        }
                    }
                } else {
                    if (isLexicallyDeclaredBindingInitialization) {
                        codeBlock->pushCode(InitializeGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this->m_loc.index);
                    } else {
                        codeBlock->pushCode(SetGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this->m_loc.index);
                    }
                }
            } else {
                if (info.m_type != InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared && !isVarDeclaredBindingInitialization) {
                    if (!info.m_isMutable) {
                        if (codeBlock->m_codeBlock->isStrict())
                            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), (uint8_t)ErrorCode::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, m_name), context, this->m_loc.index);
                        return;
                    }
                }

                if (info.m_isStackAllocated) {
                    if (srcRegister != REGULAR_REGISTER_LIMIT + info.m_index) {
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), srcRegister, REGULAR_REGISTER_LIMIT + info.m_index), context, this->m_loc.index);
                    }
                } else {
                    if (info.m_isGlobalLexicalVariable) {
                        if (isLexicallyDeclaredBindingInitialization) {
                            codeBlock->pushCode(InitializeGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this->m_loc.index);
                        } else {
                            codeBlock->pushCode(SetGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this->m_loc.index);
                        }
                    } else {
                        if (isLexicallyDeclaredBindingInitialization || isVarDeclaredBindingInitialization) {
                            if (LIKELY(info.m_upperIndex == 0)) {
                                codeBlock->pushCode(InitializeByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_index), context, this->m_loc.index);
                                initUsingVariable(codeBlock, context, srcRegister, isLexicallyDeclaredBindingInitialization, isUsingBindingInitialization);
                                return;
                            }

                            // it is for the case that function name of function expression should be initialized on the heap with other lexical variables
                            ASSERT(m_name == codeBlock->codeBlock()->functionName() && codeBlock->codeBlock()->isFunctionExpression() && codeBlock->codeBlock()->isFunctionNameSaveOnHeap());
                        }

                        codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_upperIndex, info.m_index), context, this->m_loc.index);
                    }
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            size_t addressRegisterIndex = SIZE_MAX;
            if (mayNeedsResolveAddress(codeBlock, context) && !needToReferenceSelf) {
                addressRegisterIndex = context->getLastRegisterIndex();
                context->giveUpRegister();
            }
            if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization || isVarDeclaredBindingInitialization) {
                codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this->m_loc.index);
            } else {
                if (addressRegisterIndex != SIZE_MAX) {
                    codeBlock->pushCode(StoreByNameWithAddress(ByteCodeLOC(m_loc.index), addressRegisterIndex, srcRegister, m_name), context, this->m_loc.index);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this->m_loc.index);
                }
            }
        }

        initUsingVariable(codeBlock, context, srcRegister, isLexicallyDeclaredBindingInitialization, isUsingBindingInitialization);
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        if (isPointsArgumentsObject(context)) {
            codeBlock->pushCode(EnsureArgumentsObject(ByteCodeLOC(m_loc.index)), context, this->m_loc.index);
        }

        if (context->m_inParameterInitialization && codeBlock->m_codeBlock->hasParameterName(m_name)) {
            addParameterReferenceErrorIfNeeds(codeBlock, context);
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name, context);
            addLexicalVariableErrorsIfNeeds(codeBlock, context, info, false);

            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->hasAncestorUsesNonIndexedVariableStorage()) {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this->m_loc.index);
                } else {
                    if (context->m_codeBlock->context()->staticStrings().undefined == m_name) {
                        // getting global undefined value
                        // convert to LoadLiteral of undefined value
                        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value()), context, this->m_loc.index);
                    } else {
                        codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), dstRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this->m_loc.index);
                    }
                }
            } else {
                if (info.m_isStackAllocated) {
                    if (context->m_canSkipCopyToRegister) {
                        if (dstRegister != (REGULAR_REGISTER_LIMIT + info.m_index)) {
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this->m_loc.index);
                        }
                    } else
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this->m_loc.index);
                } else {
                    if (info.m_isGlobalLexicalVariable) {
                        // m_isResultSaved should be false for global undefined
                        ASSERT(context->m_codeBlock->context()->staticStrings().undefined != m_name);
                        codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), dstRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this->m_loc.index);
                    } else {
                        codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), dstRegister, info.m_upperIndex, info.m_index), context, this->m_loc.index);
                    }
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this->m_loc.index);
        }
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, getRegister(codeBlock, context));
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (mayNeedsResolveAddress(codeBlock, context)) {
            auto r = context->getRegister();
            codeBlock->pushCode(ResolveNameAddress(ByteCodeLOC(m_loc.index), m_name, r), context, this->m_loc.index);
        }
    }

    std::tuple<bool, ByteCodeRegisterIndex, InterpretedCodeBlock::IndexedIdentifierInfo> isAllocatedOnStack(ByteCodeGenerateContext* context)
    {
        if (isPointsArgumentsObject(context)) {
            return std::make_tuple(false, REGISTER_LIMIT, InterpretedCodeBlock::IndexedIdentifierInfo());
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name, context);
            if (!info.m_isResultSaved) {
                return std::make_tuple(false, REGISTER_LIMIT, info);
            } else {
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

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        auto ret = isAllocatedOnStack(context);
        if (std::get<0>(ret)) {
            context->pushRegister(std::get<1>(ret));
            return std::get<1>(ret);
        } else {
            return context->getRegister();
        }
    }


    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        fn(m_name, false);
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        fn(m_name, true);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

private:
    void addParameterReferenceErrorIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        // check if parameter value is referenced before initialized
        bool find = false;
        for (size_t i = 0; i < context->m_initializedParameterNames.size(); i++) {
            if (context->m_initializedParameterNames[i] == m_name) {
                find = true;
                break;
            }
        }

        if (!find) {
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), (uint8_t)ErrorCode::ReferenceError, ErrorObject::Messages::IsNotInitialized, m_name), context, this->m_loc.index);
        }
    }

    bool mayNeedsResolveAddress(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(m_name, context);
            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->hasAncestorUsesNonIndexedVariableStorage()) {
                    if (context->m_isWithScope) {
                        return true;
                    }

                    if (context->m_isLeftBindingAffectedByRightExpression) {
                        return true;
                    }
                }
            }
        } else {
            if (context->m_isWithScope) {
                return true;
            }
            if (context->m_isLeftBindingAffectedByRightExpression) {
                return true;
            }
        }
        return false;
    }

    AtomicString m_name;
};
} // namespace Escargot

#endif
