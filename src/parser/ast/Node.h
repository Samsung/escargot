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

#ifndef Node_h
#define Node_h

#include "runtime/AtomicString.h"
#include "runtime/Value.h"

namespace Escargot {

class ByteCodeBlock;
class InterpretedCodeBlock;
struct ByteCodeGenerateContext;

enum ASTNodeType {
    /* Note: These 4 types must be in this order */
    Program,
    FunctionExpression,
    ArrowFunctionExpression,
    FunctionDeclaration,
    /* End */
    ArrowParameterPlaceHolder,
    Function,
    Property,
    Statement,
    Empty,
    EmptyStatement,
    BlockStatement,
    BreakStatement,
    BreakLabelStatement,
    ContinueStatement,
    ContinueLabelStatement,
    ReturnStatement,
    IfStatement,
    ForStatement,
    ForInStatement,
    ForOfStatement,
    WhileStatement,
    DoWhileStatement,
    SpreadElement,
    RestElement,
    SwitchStatement,
    SwitchCase,
    WithStatement,
    Declaration,
    VariableDeclaration,
    Pattern,
    Expression,
    ThisExpression,
    ExpressionStatement,
    ArrayExpression,
    UnaryExpressionBitwiseNot,
    UnaryExpressionDelete,
    UnaryExpressionLogicalNot,
    UnaryExpressionMinus,
    UnaryExpressionPlus,
    UnaryExpressionTypeOf,
    UnaryExpressionVoid,
    /* Note: These 13 types must be in this order */
    AssignmentExpression,
    AssignmentExpressionBitwiseAnd,
    AssignmentExpressionBitwiseOr,
    AssignmentExpressionBitwiseXor,
    AssignmentExpressionDivision,
    AssignmentExpressionLeftShift,
    AssignmentExpressionMinus,
    AssignmentExpressionMod,
    AssignmentExpressionMultiply,
    AssignmentExpressionPlus,
    AssignmentExpressionSignedRightShift,
    AssignmentExpressionUnsignedRightShift,
    AssignmentExpressionSimple,
    /* End */
    BinaryExpression,
    BinaryExpressionBitwiseAnd,
    BinaryExpressionBitwiseOr,
    BinaryExpressionBitwiseXor,
    BinaryExpressionDivison,
    /* Note: These 8 types must be in this order */
    BinaryExpressionEqual,
    BinaryExpressionNotEqual,
    BinaryExpressionStrictEqual,
    BinaryExpressionNotStrictEqual,
    BinaryExpressionGreaterThan,
    BinaryExpressionGreaterThanOrEqual,
    BinaryExpressionLessThan,
    BinaryExpressionLessThanOrEqual,
    /* End */
    BinaryExpressionIn,
    BinaryExpressionInstanceOf,
    BinaryExpressionLeftShift,
    BinaryExpressionLogicalAnd,
    BinaryExpressionLogicalOr,
    BinaryExpressionMinus,
    BinaryExpressionMod,
    BinaryExpressionMultiply,
    BinaryExpressionPlus,
    BinaryExpressionSignedRightShift,
    BinaryExpressionUnsignedRightShift,
    LogicalExpression,
    UpdateExpressionDecrementPostfix,
    UpdateExpressionDecrementPrefix,
    UpdateExpressionIncrementPostfix,
    UpdateExpressionIncrementPrefix,
    ObjectExpression,
    SequenceExpression,
    NewExpression,
    MemberExpression,
    MetaProperty,
    YieldExpression,
    ConditionalExpression,
    CallExpression,
    VariableDeclarator,
    Identifier,
    InitializeParameterExpression,
    LabeledStatement,
    Literal,
    TemplateLiteral,
    TaggedTemplateExpression,
    Directive,
    RegExpLiteral,
    NativeFunction,
    TryStatement,
    CatchClause,
    ThrowStatement,
    ClassDeclaration,
    ClassExpression,
    Class,
    ClassBody,
    ClassElement,
    ClassMethod,
    SuperExpression,
    AssignmentPattern,
    ArrayPattern,
    ObjectPattern,
    RegisterReference,
    ASTNodeTypeError,
};

COMPILE_ASSERT((int)Program == 0, "");
COMPILE_ASSERT((int)FunctionExpression == 1, "");
COMPILE_ASSERT((int)ArrowFunctionExpression == 2, "");
COMPILE_ASSERT((int)FunctionDeclaration == 3, "");

COMPILE_ASSERT(((int)AssignmentExpression + 1) == (int)AssignmentExpressionBitwiseAnd, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseAnd + 1) == (int)AssignmentExpressionBitwiseOr, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseOr + 1) == (int)AssignmentExpressionBitwiseXor, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseXor + 1) == (int)AssignmentExpressionDivision, "");
COMPILE_ASSERT(((int)AssignmentExpressionDivision + 1) == (int)AssignmentExpressionLeftShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionLeftShift + 1) == (int)AssignmentExpressionMinus, "");
COMPILE_ASSERT(((int)AssignmentExpressionMinus + 1) == (int)AssignmentExpressionMod, "");
COMPILE_ASSERT(((int)AssignmentExpressionMod + 1) == (int)AssignmentExpressionMultiply, "");
COMPILE_ASSERT(((int)AssignmentExpressionMultiply + 1) == (int)AssignmentExpressionPlus, "");
COMPILE_ASSERT(((int)AssignmentExpressionPlus + 1) == (int)AssignmentExpressionSignedRightShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionSignedRightShift + 1) == (int)AssignmentExpressionUnsignedRightShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionUnsignedRightShift + 1) == (int)AssignmentExpressionSimple, "");
COMPILE_ASSERT(((int)AssignmentExpressionSimple - (int)AssignmentExpression) == 12, "");

COMPILE_ASSERT(((int)BinaryExpressionEqual + 1) == (int)BinaryExpressionNotEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionNotEqual + 1) == (int)BinaryExpressionStrictEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionStrictEqual + 1) == (int)BinaryExpressionNotStrictEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionNotStrictEqual + 1) == (int)BinaryExpressionGreaterThan, "");
COMPILE_ASSERT(((int)BinaryExpressionGreaterThan + 1) == (int)BinaryExpressionGreaterThanOrEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionGreaterThanOrEqual + 1) == (int)BinaryExpressionLessThan, "");
COMPILE_ASSERT(((int)BinaryExpressionLessThan + 1) == (int)BinaryExpressionLessThanOrEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionLessThanOrEqual - (int)BinaryExpressionEqual) == 7, "");

// Location in the script source code
struct NodeLOC {
    size_t index;
    explicit NodeLOC(size_t index)
        : index(index)
    {
    }
};

// Extended location in the script source code
struct ExtendedNodeLOC {
    union {
        size_t line;
        size_t byteCodePosition;
    };
    union {
        ByteCodeBlock *actualCodeBlock;
        size_t column;
    };
    size_t index;

    ExtendedNodeLOC(size_t line, size_t column, size_t index)
        : line(line)
        , column(column)
        , index(index)
    {
    }
};

class LiteralNode;
class AssignmentPatternNode;
class IdentifierNode;
class ArrayExpressionNode;
class AssignmentExpressionSimpleNode;
class ArrayPatternNode;
class PatternNode;
class PropertyNode;
class MemberExpressionNode;
class ObjectExpressionNode;
class StatementNode;

class Node : public RefCounted<Node> {
protected:
    Node()
        : m_loc(SIZE_MAX)
    {
    }

public:
    virtual ~Node()
    {
    }

    virtual ASTNodeType type() = 0;

    virtual bool isPattern()
    {
        return false;
    }

    virtual PatternNode *asPattern(RefPtr<Node> init)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual PatternNode *asPattern(size_t initIdx)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual PatternNode *asPattern()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    inline void *operator new(size_t size)
    {
        return GC_MALLOC_UNCOLLECTABLE(size);
    }

    inline void operator delete(void *obj)
    {
        GC_FREE(obj);
    }

    inline void *operator new(size_t size, void *p)
    {
        return p;
    }

    bool isAssignmentOperation()
    {
        return type() >= ASTNodeType::AssignmentExpression && type() <= ASTNodeType::AssignmentExpressionSimple;
    }

    bool isRelationOperation()
    {
        return type() >= ASTNodeType::BinaryExpressionGreaterThan && type() <= ASTNodeType::BinaryExpressionLessThanOrEqual;
    }

    bool isEqualityOperation()
    {
        return type() >= ASTNodeType::BinaryExpressionEqual && type() <= ASTNodeType::BinaryExpressionNotStrictEqual;
    }

    bool isIdentifier()
    {
        return type() == ASTNodeType::Identifier;
    }

    IdentifierNode *asIdentifier()
    {
        ASSERT(isIdentifier());
        return (IdentifierNode *)this;
    }

    bool isAssignmentPattern()
    {
        return type() == ASTNodeType::AssignmentPattern;
    }

    AssignmentPatternNode *asAssignmentPattern()
    {
        ASSERT(isAssignmentPattern());
        return (AssignmentPatternNode *)this;
    }

    bool isArrayPattern()
    {
        return type() == ASTNodeType::ArrayPattern;
    }

    ArrayPatternNode *asArrayPattern()
    {
        ASSERT(isArrayPattern());
        return (ArrayPatternNode *)this;
    }

    bool isArrayExpression()
    {
        return type() == ASTNodeType::ArrayExpression;
    }

    ArrayExpressionNode *asArrayExpression()
    {
        ASSERT(isArrayExpression());
        return (ArrayExpressionNode *)this;
    }

    bool isObjectExpression()
    {
        return type() == ASTNodeType::ObjectExpression;
    }

    ObjectExpressionNode *asObjectExpression()
    {
        ASSERT(isObjectExpression());
        return (ObjectExpressionNode *)this;
    }

    bool isProperty()
    {
        return type() == ASTNodeType::Property;
    }

    PropertyNode *asProperty()
    {
        ASSERT(isProperty());
        return (PropertyNode *)this;
    }

    bool isAssignmentExpressionSimple()
    {
        return type() == ASTNodeType::AssignmentExpressionSimple;
    }

    AssignmentExpressionSimpleNode *asAssignmentExpressionSimple()
    {
        ASSERT(isAssignmentExpressionSimple());
        return (AssignmentExpressionSimpleNode *)this;
    }

    MemberExpressionNode *asMemberExpression()
    {
        ASSERT(isMemberExpression());
        return (MemberExpressionNode *)this;
    }

    LiteralNode *asLiteral()
    {
        ASSERT(isLiteral());
        return (LiteralNode *)this;
    }

    StatementNode *asStatementNode()
    {
        ASSERT(isStatementNode());
        return (StatementNode *)this;
    }

    bool isLiteral()
    {
        return type() == ASTNodeType::Literal;
    }

    bool isMemberExpression()
    {
        return type() == ASTNodeType::MemberExpression;
    }

    bool isSuperNode()
    {
        return type() == ASTNodeType::SuperExpression;
    }

    bool isValidAssignmentTarget()
    {
        return isIdentifier() || isMemberExpression();
    }

    virtual bool isExpressionNode()
    {
        return false;
    }

    virtual bool isStatementNode()
    {
        return false;
    }

    NodeLOC loc()
    {
        return m_loc;
    }

    virtual void generateStatementByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, ByteCodeRegisterIndex dstRegister)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void generateStoreByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf);

    virtual void generateResolveAddressByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context)
    {
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context);
    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context);

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock *codeBlock, ByteCodeGenerateContext *context);

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)> &fn)
    {
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)> &fn)
    {
    }

    NodeLOC m_loc;
};

class Pattern {
public:
};

class ASTFunctionScopeContextNameInfo {
public:
    ASTFunctionScopeContextNameInfo()
    {
        m_isExplicitlyDeclaredOrParameterName = false;
        m_isVarDeclaration = false;
        m_lexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
        m_name = AtomicString();
    }

    AtomicString name() const
    {
        return m_name;
    }

    void setName(AtomicString name)
    {
        m_name = name;
    }

    bool isExplicitlyDeclaredOrParameterName() const
    {
        return m_isExplicitlyDeclaredOrParameterName;
    }

    bool isVarDeclaration() const
    {
        return m_isVarDeclaration;
    }

    void setIsVarDeclaration(bool v)
    {
        m_isVarDeclaration = v;
    }

    void setIsExplicitlyDeclaredOrParameterName(bool v)
    {
        m_isExplicitlyDeclaredOrParameterName = v;
    }

    LexicalBlockIndex lexicalBlockIndex() const
    {
        return m_lexicalBlockIndex;
    }

    void setLexicalBlockIndex(LexicalBlockIndex v)
    {
        m_lexicalBlockIndex = v;
    }

private:
    bool m_isExplicitlyDeclaredOrParameterName : 1;
    bool m_isVarDeclaration : 1;
    LexicalBlockIndex m_lexicalBlockIndex;
    AtomicString m_name;
};

typedef TightVector<ASTFunctionScopeContextNameInfo, GCUtil::gc_malloc_atomic_allocator<ASTFunctionScopeContextNameInfo>> ASTFunctionScopeContextNameInfoVector;

// store only let, const names
class ASTBlockScopeContextNameInfo {
public:
    ASTBlockScopeContextNameInfo()
    {
        m_value = (size_t)AtomicString().string();
    }

    AtomicString name() const
    {
        return AtomicString((String *)((size_t)m_value & ~1));
    }

    void setName(AtomicString name)
    {
        m_value = (size_t)name.string() | isConstBinding();
    }

    bool isConstBinding() const
    {
        return m_value & 1;
    }

    void setIsConstBinding(bool v)
    {
        if (v)
            m_value = m_value | 1;
        else
            m_value = m_value & ~1;
    }

private:
    size_t m_value;
};

typedef TightVector<ASTBlockScopeContextNameInfo, GCUtil::gc_malloc_atomic_allocator<ASTBlockScopeContextNameInfo>> ASTBlockScopeContextNameInfoVector;
// context for block in function or program
struct ASTBlockScopeContext : public gc {
    LexicalBlockIndex m_blockIndex;
    LexicalBlockIndex m_parentBlockIndex;
    ASTBlockScopeContextNameInfoVector m_names;
    AtomicStringVector m_usingNames;
#ifndef NDEBUG
    ExtendedNodeLOC m_loc;
#endif

    void *operator new(size_t size);
    void *operator new[](size_t size) = delete;

    ASTBlockScopeContext()
        : m_blockIndex(LEXICAL_BLOCK_INDEX_MAX)
        , m_parentBlockIndex(LEXICAL_BLOCK_INDEX_MAX)
#ifndef NDEBUG
        , m_loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#endif
    {
    }
};

typedef Vector<ASTBlockScopeContext *, GCUtil::gc_malloc_allocator<ASTBlockScopeContext *>> ASTBlockScopeContextVector;

// context for function or program
struct ASTFunctionScopeContext : public gc {
    bool m_isStrict : 1;
    bool m_hasEval : 1;
    bool m_hasWith : 1;
    bool m_hasYield : 1;
    bool m_hasEvaluateBindingId : 1;
    bool m_inWith : 1;
    bool m_isArrowFunctionExpression : 1;
    bool m_isClassConstructor : 1;
    bool m_isDerivedClassConstructor : 1;
    bool m_isClassMethod : 1;
    bool m_isClassStaticMethod : 1;
    bool m_isGenerator : 1;
    bool m_hasSuperOrNewTarget : 1;
    bool m_hasManyNumeralLiteral : 1;
    bool m_hasArrowParameterPlaceHolder : 1;
    bool m_hasParameterOtherThanIdentifier : 1;
    bool m_needsToComputeLexicalBlockStuffs : 1;
    bool m_hasImplictFunctionName : 1;
    unsigned int m_nodeType : 2; // it is actually NodeType but used on FunctionExpression, ArrowFunctionExpression, FunctionDeclaration only
    LexicalBlockIndex m_lexicalBlockIndexFunctionLocatedIn : 16;
    ASTFunctionScopeContextNameInfoVector m_varNames;
    AtomicStringTightVector m_parameters;
    AtomicString m_functionName;

    // TODO save these scopes as linked-list
    Vector<ASTFunctionScopeContext *, GCUtil::gc_malloc_allocator<ASTFunctionScopeContext *>> m_childScopes;
    ASTBlockScopeContextVector m_childBlockScopes;

    // we can use atomic allocator here because there is no pointer value on Vector<m_numeralLiteralData>
    Vector<Value, GCUtil::gc_malloc_atomic_allocator<Value>> m_numeralLiteralData;

    ExtendedNodeLOC m_paramsStartLOC;
    ExtendedNodeLOC m_bodyStartLOC;
#ifndef NDEBUG
    ExtendedNodeLOC m_bodyEndLOC;
#else
    NodeLOC m_bodyEndLOC;
#endif

    void *operator new(size_t size);
    void *operator new[](size_t size) = delete;

    bool hasVarName(AtomicString name)
    {
        for (size_t i = 0; i < m_varNames.size(); i++) {
            if (m_varNames[i].name() == name) {
                return true;
            }
        }

        return false;
    }

    ASTBlockScopeContext *findBlock(LexicalBlockIndex blockIndex)
    {
        size_t b = m_childBlockScopes.size();
        for (size_t i = 0; i < b; i++) {
            if (m_childBlockScopes[i]->m_blockIndex == blockIndex) {
                return m_childBlockScopes[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ASTBlockScopeContext *findBlockFromBackward(LexicalBlockIndex blockIndex)
    {
        int32_t b = (int32_t)m_childBlockScopes.size() - 1;
        for (int32_t i = b; i >= 0; i--) {
            if (m_childBlockScopes[(size_t)i]->m_blockIndex == blockIndex) {
                return m_childBlockScopes[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool hasName(AtomicString name, LexicalBlockIndex blockIndex)
    {
        return hasNameAtBlock(name, blockIndex) || hasVarName(name);
    }

    bool hasNameAtBlock(AtomicString name, LexicalBlockIndex blockIndex)
    {
        while (blockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            ASTBlockScopeContext *blockContext = findBlock(blockIndex);
            for (size_t i = 0; i < blockContext->m_names.size(); i++) {
                if (blockContext->m_names[i].name() == name) {
                    return true;
                }
            }

            blockIndex = blockContext->m_parentBlockIndex;
        }
        return false;
    }

    void insertVarName(AtomicString name, LexicalBlockIndex blockIndex, bool isExplicitlyDeclaredOrParameterName, bool isVarDeclaration = true)
    {
        for (size_t i = 0; i < m_varNames.size(); i++) {
            if (m_varNames[i].name() == name) {
                if (isExplicitlyDeclaredOrParameterName) {
                    m_varNames[i].setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
                }
                return;
            }
        }

        ASTFunctionScopeContextNameInfo info;
        info.setName(name);
        info.setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
        info.setIsVarDeclaration(isVarDeclaration);
        info.setLexicalBlockIndex(blockIndex);
        m_varNames.push_back(info);
    }

    void insertUsingName(AtomicString name, LexicalBlockIndex blockIndex)
    {
        ASTBlockScopeContext *blockContext = findBlockFromBackward(blockIndex);
        if (VectorUtil::findInVector(blockContext->m_usingNames, name) == VectorUtil::invalidIndex) {
            blockContext->m_usingNames.push_back(name);
        }
    }

    void insertNumeralLiteral(Value v)
    {
        ASSERT(!v.isPointerValue());
        if (m_hasManyNumeralLiteral) {
            return;
        }
        if (VectorUtil::findInVector(m_numeralLiteralData, v) == VectorUtil::invalidIndex) {
            if (m_numeralLiteralData.size() < KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT)
                m_numeralLiteralData.push_back(v);
            else {
                m_numeralLiteralData.clear();
                m_hasManyNumeralLiteral = true;
            }
        }
    }

    void insertBlockScope(LexicalBlockIndex blockIndex, LexicalBlockIndex parentBlockIndex, ExtendedNodeLOC loc)
    {
#ifndef NDEBUG
        size_t b = m_childBlockScopes.size();
        for (size_t i = 0; i < b; i++) {
            if (m_childBlockScopes[i]->m_blockIndex == blockIndex) {
                ASSERT_NOT_REACHED();
            }
        }
#endif

        ASTBlockScopeContext *newContext = new ASTBlockScopeContext();
        newContext->m_blockIndex = blockIndex;
        newContext->m_parentBlockIndex = parentBlockIndex;
#ifndef NDEBUG
        newContext->m_loc = loc;
#endif
        m_childBlockScopes.push_back(newContext);
    }

    bool insertNameAtBlock(AtomicString name, LexicalBlockIndex blockIndex, bool isConstBinding)
    {
        ASTBlockScopeContext *blockContext = findBlockFromBackward(blockIndex);
        for (size_t i = 0; i < blockContext->m_names.size(); i++) {
            if (blockContext->m_names[i].name() == name) {
                return false;
            }
        }

        ASTBlockScopeContextNameInfo info;
        info.setName(name);
        info.setIsConstBinding(isConstBinding);
        blockContext->m_names.push_back(info);
        return true;
    }

    bool blockHasName(AtomicString name, size_t blockIndex)
    {
        ASTBlockScopeContext *blockContext = findBlockFromBackward(blockIndex);
        for (size_t i = 0; i < blockContext->m_names.size(); i++) {
            if (blockContext->m_names[i].name() == name) {
                return true;
            }
        }
        return false;
    }

    struct FindNameResult {
        bool isFinded;
        bool isVarName;
        LexicalBlockIndex lexicalBlockIndexIfExists;

        FindNameResult()
            : isFinded(false)
            , isVarName(false)
            , lexicalBlockIndexIfExists(LEXICAL_BLOCK_INDEX_MAX)
        {
        }
    };

    bool canDeclareName(AtomicString name, LexicalBlockIndex blockIndex, bool isVarName)
    {
        if (isVarName) {
            for (size_t i = 0; i < m_childBlockScopes.size(); i++) {
                auto blockScopeContext = m_childBlockScopes[i];

                // this means block was already closed
                // { <block 1> { <block 2> } <block 1> }
                if (blockIndex < blockScopeContext->m_blockIndex) {
                    continue;
                }

                for (size_t j = 0; j < blockScopeContext->m_names.size(); j++) {
                    if (name == blockScopeContext->m_names[j].name()) {
                        return false;
                    }
                }
            }
        } else {
            if (blockHasName(name, blockIndex)) {
                return false;
            }

            for (size_t i = 0; i < m_varNames.size(); i++) {
                if (m_varNames[i].name() == name) {
                    if (m_varNames[i].lexicalBlockIndex() >= blockIndex) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    explicit ASTFunctionScopeContext(bool isStrict = false)
        : m_isStrict(isStrict)
        , m_hasEval(false)
        , m_hasWith(false)
        , m_hasYield(false)
        , m_hasEvaluateBindingId(false)
        , m_inWith(false)
        , m_isArrowFunctionExpression(false)
        , m_isClassConstructor(false)
        , m_isDerivedClassConstructor(false)
        , m_isClassMethod(false)
        , m_isClassStaticMethod(false)
        , m_isGenerator(false)
        , m_hasSuperOrNewTarget(false)
        , m_hasManyNumeralLiteral(false)
        , m_hasArrowParameterPlaceHolder(false)
        , m_hasParameterOtherThanIdentifier(false)
        , m_needsToComputeLexicalBlockStuffs(false)
        , m_hasImplictFunctionName(false)
        , m_nodeType(ASTNodeType::Program)
        , m_lexicalBlockIndexFunctionLocatedIn(LEXICAL_BLOCK_INDEX_MAX)
        , m_paramsStartLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
        , m_bodyStartLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#ifndef NDEBUG
        , m_bodyEndLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#else
        , m_bodyEndLOC(SIZE_MAX)
#endif
    {
        // function is first block context
        insertBlockScope(0, LEXICAL_BLOCK_INDEX_MAX, m_bodyStartLOC);
    }
};

typedef std::vector<RefPtr<Node>> NodeVector;
typedef std::vector<RefPtr<Node>> ArgumentVector;
typedef std::vector<RefPtr<Node>> ExpressionNodeVector;
typedef std::vector<RefPtr<Node>> PatternNodeVector;
class PropertyNode;
typedef std::vector<RefPtr<PropertyNode>> PropertiesNodeVector;
class VariableDeclaratorNode;
typedef std::vector<RefPtr<VariableDeclaratorNode>> VariableDeclaratorVector;
}

#endif
