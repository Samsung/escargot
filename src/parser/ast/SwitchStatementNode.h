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

#ifndef SwitchStatementNode_h
#define SwitchStatementNode_h

#include "ExpressionNode.h"
#include "LiteralNode.h"
#include "StatementNode.h"
#include "SwitchCaseNode.h"
#include "UnaryExpressionMinusNode.h"

namespace Escargot {

class SwitchStatementNode : public StatementNode {
public:
    SwitchStatementNode(Node* discriminant, StatementContainer* casesA, Node* deflt, StatementContainer* casesB, LexicalBlockIndex lexicalBlockIndex)
        : StatementNode()
        , m_lexicalBlockIndex(lexicalBlockIndex)
        , m_discriminant(discriminant)
        , m_casesA(casesA)
        , m_default(deflt)
        , m_casesB(casesB)
    {
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        ByteCodeGenerateContext newContext(*context);
        size_t firstRegister = newContext.getRegister(); // ExeuctionResult of m_body should not be overwritten by caseNode->m_test

        bool canSkipCopyToRegister = newContext.m_canSkipCopyToRegister;
        newContext.m_canSkipCopyToRegister = false;
        size_t rIndex0 = m_discriminant->getRegister(codeBlock, &newContext);
        m_discriminant->generateExpressionByteCode(codeBlock, &newContext, rIndex0);
        newContext.m_canSkipCopyToRegister = canSkipCopyToRegister;

        // Whether the discriminant occupies its own freshly-allocated register (rather than
        // reusing an existing variable's register). Captured here, before pushLexicalBlock
        // may allocate a disposable-record register (for `using`) on top of it. - Issue #1577
        bool discriminantHasOwnRegister = (rIndex0 == newContext.getLastRegisterIndex());

        if (firstRegister == 0 && context->shouldCareScriptExecutionResult()) {
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), 0, Value()), context, this->m_loc.index);
        }

        size_t lexicalBlockIndexBefore = newContext.m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
        if (m_lexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            newContext.m_lexicalBlockIndex = m_lexicalBlockIndex;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_lexicalBlockIndex);
            blockContext = codeBlock->pushLexicalBlock(&newContext, bi, this);
        }

        // When the switch block declares a `using` variable, pushLexicalBlock allocates a
        // disposable-record register that lives until finalizeLexicalBlock. It sits on the
        // register stack above the discriminant temporaries, so those temporaries can only be
        // released (LIFO) after the disposable register is popped in finalizeLexicalBlock.
        // Releasing them earlier (the default path below) would free the disposable register
        // itself, letting a statement in a case body clobber it. - Issue #1577
        bool hasUsingBlock = (m_lexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) && (blockContext.usingBlockTryStartPosition != SIZE_MAX);

        // Case tests that are all literals have no side effect and no observable
        // evaluation order, so the whole strict-equal chain can collapse into a
        // single table lookup. - plan B
        CaseKeyVector caseKeys;
        TableKind tableKind = NoTable;
        int32_t tableMin = 0;
        uint32_t tableEntryCount = 0;
        uint32_t tableCapacity = 0;
        if (appendCaseKeys(m_casesB, caseKeys) && appendCaseKeys(m_casesA, caseKeys)) {
            tableKind = chooseTable(caseKeys, tableMin, tableEntryCount, tableCapacity);
        }

        size_t switchCodePosition = SIZE_MAX;
        std::vector<size_t> jumpCodePerCaseNodePosition;
        StatementNode* nd = nullptr;

        if (tableKind == Int32Table) {
            switchCodePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(SwitchOnInt32(ByteCodeLOC(m_loc.index), rIndex0, tableMin, tableEntryCount), &newContext, this->m_loc.index);
            codeBlock->pushTailData(tableEntryCount * sizeof(size_t));

            size_t* table = codeBlock->peekCode<SwitchOnInt32>(switchCodePosition)->table();
            for (uint32_t i = 0; i < tableEntryCount; i++) {
                table[i] = SIZE_MAX;
            }
            std::vector<bool> slotTaken(tableEntryCount, false);
            for (size_t i = 0; i < caseKeys.size(); i++) {
                size_t slot = static_cast<uint32_t>(caseKeys[i].m_value.asInt32()) - static_cast<uint32_t>(tableMin);
                // a duplicate case test is unreachable, the first one in test order wins
                if (!slotTaken[slot]) {
                    slotTaken[slot] = true;
                    caseKeys[i].m_slotIndex = slot;
                }
            }
        } else if (tableKind == ValueTable) {
            switchCodePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(SwitchOnValue(ByteCodeLOC(m_loc.index), rIndex0, tableCapacity), &newContext, this->m_loc.index);
            codeBlock->pushTailData(tableCapacity * sizeof(SwitchOnValueEntry));

            SwitchOnValue* switchCode = codeBlock->peekCode<SwitchOnValue>(switchCodePosition);
            for (size_t i = 0; i < caseKeys.size(); i++) {
                caseKeys[i].m_slotIndex = switchCode->insert(caseKeys[i].m_kind, caseKeys[i].m_value);
                if ((caseKeys[i].m_kind == SwitchOnValue::KeyKindString) && caseKeys[i].m_value.asString()->length()) {
                    // the table lives in the code stream, which the GC does not scan
                    codeBlock->m_stringLiteralData.pushBack(caseKeys[i].m_value.asString());
                }
            }
        } else {
            nd = m_casesB->firstChild();
            while (nd) {
                SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
                size_t refIndex = caseNode->test()->getRegister(codeBlock, &newContext);
                caseNode->test()->generateExpressionByteCode(codeBlock, &newContext, refIndex);
                size_t resultIndex = newContext.getRegister();
                codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this->m_loc.index);
                jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
                codeBlock->pushCode(JumpIfBoolean(ByteCodeLOC(m_loc.index), false, resultIndex), &newContext, this->m_loc.index);
                newContext.giveUpRegister();
                newContext.giveUpRegister();
                nd = nd->nextSibling();
            }
        }

        bool registerWasSame = rIndex0 == newContext.getLastRegisterIndex();
        if (tableKind == NoTable) {
            nd = m_casesA->firstChild();
            while (nd) {
                SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
                size_t refIndex = caseNode->test()->getRegister(codeBlock, &newContext);
                caseNode->test()->generateExpressionByteCode(codeBlock, &newContext, refIndex);
                size_t resultIndex = newContext.getRegister();
                codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), refIndex, rIndex0, resultIndex), &newContext, this->m_loc.index);
                jumpCodePerCaseNodePosition.push_back(codeBlock->currentCodeSize());
                codeBlock->pushCode(JumpIfBoolean(ByteCodeLOC(m_loc.index), false, resultIndex), &newContext, this->m_loc.index);
                newContext.giveUpRegister();
                newContext.giveUpRegister();
                nd = nd->nextSibling();
            }
        }

        if (!hasUsingBlock) {
            newContext.giveUpRegister();
            if (registerWasSame) {
                newContext.giveUpRegister();
            }
        }

        size_t jmpToDefault = SIZE_MAX;
        if (tableKind == NoTable) {
            jmpToDefault = codeBlock->currentCodeSize();
            codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), &newContext, this->m_loc.index);
        }
        size_t caseIdx = 0;
        nd = m_casesB->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            bindCaseBodyPosition(codeBlock, tableKind, switchCodePosition, jumpCodePerCaseNodePosition, caseKeys, caseIdx++, codeBlock->currentCodeSize());
            caseNode->generateStatementByteCode(codeBlock, &newContext);
            nd = nd->nextSibling();
        }
        if (m_default) {
            bindDefaultPosition(codeBlock, tableKind, switchCodePosition, jmpToDefault, codeBlock->currentCodeSize());
            m_default->generateStatementByteCode(codeBlock, &newContext);
        }
        nd = m_casesA->firstChild();
        while (nd) {
            SwitchCaseNode* caseNode = (SwitchCaseNode*)nd;
            bindCaseBodyPosition(codeBlock, tableKind, switchCodePosition, jumpCodePerCaseNodePosition, caseKeys, caseIdx++, codeBlock->currentCodeSize());
            caseNode->generateStatementByteCode(codeBlock, &newContext);
            nd = nd->nextSibling();
        }
        size_t breakPos = codeBlock->currentCodeSize();
        newContext.consumeBreakPositions(codeBlock, breakPos, newContext.tryCatchWithBlockStatementCount());
        newContext.m_positionToContinue = context->m_positionToContinue;

        if (!m_default) {
            bindDefaultPosition(codeBlock, tableKind, switchCodePosition, jmpToDefault, codeBlock->currentCodeSize());
        }
        if (tableKind == Int32Table) {
            // every value in the range no case claimed falls through to default
            SwitchOnInt32* switchCode = codeBlock->peekCode<SwitchOnInt32>(switchCodePosition);
            for (uint32_t i = 0; i < tableEntryCount; i++) {
                if (switchCode->table()[i] == SIZE_MAX) {
                    switchCode->table()[i] = switchCode->m_defaultPosition;
                }
            }
        }
        if (m_lexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(&newContext, blockContext);
            newContext.m_lexicalBlockIndex = lexicalBlockIndexBefore;
        }

        // Deferred release of the discriminant temporaries for the `using` case (see the
        // comment where hasUsingBlock is computed). finalizeLexicalBlock above has already
        // popped the disposable-record register, so these are now back on top. - Issue #1577
        if (hasUsingBlock) {
            newContext.giveUpRegister();
            if (discriminantHasOwnRegister) {
                newContext.giveUpRegister();
            }
        }

        newContext.propagateInformationTo(*context);
    }

    virtual ASTNodeType type() override { return ASTNodeType::SwitchStatement; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_discriminant->iterateChildren(fn);
        m_casesA->iterateChildren(fn);
        if (m_default) {
            m_default->iterateChildren(fn);
        }
        m_casesB->iterateChildren(fn);
    }

private:
    enum TableKind {
        NoTable,
        Int32Table,
        ValueTable,
    };

    struct CaseKey {
        Value m_value;
        SwitchOnValue::KeyKind m_kind;
        // slot the case body address goes into; SIZE_MAX for a duplicate case
        // test, which can never be reached
        size_t m_slotIndex;
    };

    typedef std::vector<CaseKey> CaseKeyVector;

    // Thresholds are tuned empirically, re-measure if a better shape shows up.
    static const size_t s_minimumTableCaseCount = 6;
    static const size_t s_maximumTableCaseCount = 65536;
    static const size_t s_maximumInt32TableRange = 512;
    static const size_t s_maximumInt32TableSpread = 3;

    // false as soon as one case test is not a literal this node knows how to
    // turn into a key, which leaves the whole switch on the strict-equal chain
    static bool appendCaseKeys(StatementContainer* cases, CaseKeyVector& keys)
    {
        StatementNode* nd = cases->firstChild();
        while (nd) {
            Node* test = ((SwitchCaseNode*)nd)->test();
            Value literal;
            if (test->isLiteral()) {
                literal = ((LiteralNode*)test)->value();
            } else if (test->type() == ASTNodeType::UnaryExpressionMinus && ((UnaryExpressionMinusNode*)test)->argument()->isLiteral()) {
                // the parser does not fold `case -1:` into a literal
                const Value& operand = ((LiteralNode*)((UnaryExpressionMinusNode*)test)->argument())->value();
                if (!operand.isNumber()) {
                    return false;
                }
                literal = Value(Value::DoubleToIntConvertibleTestNeeds, -operand.asNumber());
            } else {
                return false;
            }

            CaseKey key;
            key.m_value = literal;
            key.m_kind = SwitchOnValue::classifyKey(key.m_value);
            if (key.m_kind == SwitchOnValue::KeyKindUnsupported) {
                // a BigInt case test
                return false;
            }
            if ((key.m_kind == SwitchOnValue::KeyKindString) && !key.m_value.asString()->length()) {
                key.m_value = Value(String::emptyString());
            }
            key.m_slotIndex = SIZE_MAX;
            keys.push_back(key);
            nd = nd->nextSibling();
        }
        return true;
    }

    static TableKind chooseTable(const CaseKeyVector& keys, int32_t& tableMin, uint32_t& tableEntryCount, uint32_t& tableCapacity)
    {
        if ((keys.size() < s_minimumTableCaseCount) || (keys.size() > s_maximumTableCaseCount)) {
            return NoTable;
        }

        bool allInt32 = true;
        int32_t minimum = std::numeric_limits<int32_t>::max();
        int32_t maximum = std::numeric_limits<int32_t>::min();
        for (size_t i = 0; i < keys.size(); i++) {
            if (keys[i].m_kind != SwitchOnValue::KeyKindInt32) {
                allInt32 = false;
                break;
            }
            minimum = std::min(minimum, keys[i].m_value.asInt32());
            maximum = std::max(maximum, keys[i].m_value.asInt32());
        }

        if (allInt32) {
            // computed unsigned so that a range spanning the whole int32 domain
            // cannot overflow
            const uint64_t range = static_cast<uint64_t>(static_cast<uint32_t>(maximum) - static_cast<uint32_t>(minimum)) + 1;
            if ((range <= s_maximumInt32TableRange) && (range <= keys.size() * s_maximumInt32TableSpread)) {
                tableMin = minimum;
                tableEntryCount = static_cast<uint32_t>(range);
                return Int32Table;
            }
        }

        // load factor of at most one half, which also keeps a free slot around
        // so that probing always ends
        tableCapacity = 8;
        while (tableCapacity < keys.size() * 2) {
            tableCapacity *= 2;
        }
        return ValueTable;
    }

    static void bindCaseBodyPosition(ByteCodeBlock* codeBlock, TableKind tableKind, size_t switchCodePosition,
                                     const std::vector<size_t>& jumpCodePerCaseNodePosition, const CaseKeyVector& caseKeys,
                                     size_t caseIndex, size_t position)
    {
        if (tableKind == NoTable) {
            codeBlock->peekCode<JumpIfBoolean>(jumpCodePerCaseNodePosition[caseIndex])->m_jumpPosition = position;
        } else if (caseKeys[caseIndex].m_slotIndex != SIZE_MAX) {
            if (tableKind == Int32Table) {
                codeBlock->peekCode<SwitchOnInt32>(switchCodePosition)->table()[caseKeys[caseIndex].m_slotIndex] = position;
            } else {
                codeBlock->peekCode<SwitchOnValue>(switchCodePosition)->table()[caseKeys[caseIndex].m_slotIndex].m_position = position;
            }
        }
    }

    static void bindDefaultPosition(ByteCodeBlock* codeBlock, TableKind tableKind, size_t switchCodePosition, size_t jmpToDefault, size_t position)
    {
        if (tableKind == NoTable) {
            codeBlock->peekCode<Jump>(jmpToDefault)->m_jumpPosition = position;
        } else if (tableKind == Int32Table) {
            codeBlock->peekCode<SwitchOnInt32>(switchCodePosition)->m_defaultPosition = position;
        } else {
            codeBlock->peekCode<SwitchOnValue>(switchCodePosition)->m_defaultPosition = position;
        }
    }

    LexicalBlockIndex m_lexicalBlockIndex;
    Node* m_discriminant;
    StatementContainer* m_casesA;
    Node* m_default;
    StatementContainer* m_casesB;
};
} // namespace Escargot

#endif
