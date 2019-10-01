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
    DebuggerStatement,
    SpreadElement,
    RestElement,
    SwitchStatement,
    SwitchCase,
    WithStatement,
    Declaration,
    VariableDeclaration,
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
    ImportSpecifier,
    ImportDefaultSpecifier,
    ImportNamespaceSpecifier,
    ImportDeclaration,
    ExportSpecifier,
    ExportDeclaration,
    ExportDefaultDeclaration,
    ExportAllDeclaration,
    ExportNamedDeclaration,
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

    virtual void iterateChildren(const std::function<void(Node *node)> &fn)
    {
        fn(this);
    }

    NodeLOC m_loc;
};

typedef std::vector<RefPtr<Node>> NodeVector;
typedef std::vector<RefPtr<Node>> ArgumentVector;
typedef std::vector<RefPtr<Node>> ExpressionNodeVector;
typedef std::vector<RefPtr<Node>> PatternNodeVector;
class PropertyNode;
typedef std::vector<RefPtr<Node>> PropertiesNodeVector;
class VariableDeclaratorNode;
typedef std::vector<RefPtr<VariableDeclaratorNode>> VariableDeclaratorVector;
}

#endif
