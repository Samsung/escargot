/*
 * Copyright (c) 2023-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotCommonASTData__
#define __EscargotCommonASTData__

namespace Escargot {

class ByteCodeBlock;

enum ASTNodeType : uint16_t {
    /* Note: These 4 types must be in this order */
    Program,
    ArrowFunctionExpression,
    FunctionExpression,
    FunctionDeclaration,
    /* End */
    /* Note: These 15 types must be in this order */
    AssignmentExpression,
    AssignmentExpressionBitwiseAnd,
    AssignmentExpressionBitwiseOr,
    AssignmentExpressionBitwiseXor,
    AssignmentExpressionDivision,
    AssignmentExpressionExponentiation,
    AssignmentExpressionLeftShift,
    AssignmentExpressionMinus,
    AssignmentExpressionMod,
    AssignmentExpressionMultiply,
    AssignmentExpressionPlus,
    AssignmentExpressionSignedRightShift,
    AssignmentExpressionUnsignedRightShift,
    AssignmentExpressionLogicalAnd,
    AssignmentExpressionLogicalOr,
    AssignmentExpressionLogicalNullish,
    AssignmentExpressionSimple,
    /* End */
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
    /* Note: These 7 types must be in this order */
    UnaryExpressionBitwiseNot,
    UnaryExpressionDelete,
    UnaryExpressionLogicalNot,
    UnaryExpressionMinus,
    UnaryExpressionPlus,
    UnaryExpressionTypeOf,
    UnaryExpressionVoid,
    /* End */
    ArrayExpression,
    ArrayPattern,
    ArrowParameterPlaceHolder,
    AssignmentPattern,
    AwaitExpression,
    BinaryExpression,
    BinaryExpressionBitwiseAnd,
    BinaryExpressionBitwiseOr,
    BinaryExpressionBitwiseXor,
    BinaryExpressionDivision,
    BinaryExpressionExponentiation,
    BinaryExpressionIn,
    BinaryExpressionPrivateIn,
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
    BinaryExpressionNullishCoalescing,
    BlockStatement,
    BreakLabelStatement,
    BreakStatement,
    CallExpression,
    CatchClause,
    Class,
    ClassBody,
    ClassDeclaration,
    ClassElement,
    ClassExpression,
    ConditionalExpression,
    ContinueLabelStatement,
    ContinueStatement,
    DebuggerStatement,
    Declaration,
    Directive,
    DoWhileStatement,
    EmptyStatement,
    ExportAllDeclaration,
    ExportDeclaration,
    ExportDefaultDeclaration,
    ExportNamedDeclaration,
    ExportStarAsNamedFromDeclaration,
    ExportSpecifier,
    Expression,
    ExpressionStatement,
    ForInStatement,
    ForOfStatement,
    ForStatement,
    Function,
    Identifier,
    IfStatement,
    ImportCall,
    ImportDeclaration,
    ImportDefaultSpecifier,
    ImportNamespaceSpecifier,
    ImportSpecifier,
    InitializeParameterExpression,
    LabelledStatement,
    Literal,
    MemberExpression,
    MetaProperty,
    NewExpression,
    ObjectExpression,
    ObjectPattern,
    Property,
    RegExpLiteral,
    RestElement,
    ReturnStatement,
    SequenceExpression,
    SpreadElement,
    Statement,
    SuperExpression,
    SwitchCase,
    SwitchStatement,
    TaggedTemplateExpression,
    TaggedTemplateQuasiExpression,
    TemplateLiteral,
    ThisExpression,
    ThrowStatement,
    TryStatement,
    UpdateExpressionDecrementPostfix,
    UpdateExpressionDecrementPrefix,
    UpdateExpressionIncrementPostfix,
    UpdateExpressionIncrementPrefix,
    VariableDeclaration,
    VariableDeclarator,
    WhileStatement,
    WithStatement,
    YieldExpression,
    ASTStatementContainer, // used only for SyntaxNode
    ASTNodeTypeError, // used only for SyntaxNode
};

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
        ByteCodeBlock* actualCodeBlock;
        size_t column;
    };
    size_t index;

    ExtendedNodeLOC()
        : ExtendedNodeLOC(0, 0, 0)
    {
    }

    ExtendedNodeLOC(size_t line, size_t column, size_t index)
        : line(line)
        , column(column)
        , index(index)
    {
    }
};


COMPILE_ASSERT((int)Program == 0, "");
COMPILE_ASSERT((int)ArrowFunctionExpression == 1, "");
COMPILE_ASSERT((int)FunctionExpression == 2, "");
COMPILE_ASSERT((int)FunctionDeclaration == 3, "");

COMPILE_ASSERT(((int)AssignmentExpression + 1) == (int)AssignmentExpressionBitwiseAnd, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseAnd + 1) == (int)AssignmentExpressionBitwiseOr, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseOr + 1) == (int)AssignmentExpressionBitwiseXor, "");
COMPILE_ASSERT(((int)AssignmentExpressionBitwiseXor + 1) == (int)AssignmentExpressionDivision, "");
COMPILE_ASSERT(((int)AssignmentExpressionDivision + 1) == (int)AssignmentExpressionExponentiation, "");
COMPILE_ASSERT(((int)AssignmentExpressionExponentiation + 1) == (int)AssignmentExpressionLeftShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionLeftShift + 1) == (int)AssignmentExpressionMinus, "");
COMPILE_ASSERT(((int)AssignmentExpressionMinus + 1) == (int)AssignmentExpressionMod, "");
COMPILE_ASSERT(((int)AssignmentExpressionMod + 1) == (int)AssignmentExpressionMultiply, "");
COMPILE_ASSERT(((int)AssignmentExpressionMultiply + 1) == (int)AssignmentExpressionPlus, "");
COMPILE_ASSERT(((int)AssignmentExpressionPlus + 1) == (int)AssignmentExpressionSignedRightShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionSignedRightShift + 1) == (int)AssignmentExpressionUnsignedRightShift, "");
COMPILE_ASSERT(((int)AssignmentExpressionUnsignedRightShift + 1) == (int)AssignmentExpressionLogicalAnd, "");
COMPILE_ASSERT(((int)AssignmentExpressionLogicalAnd + 1) == (int)AssignmentExpressionLogicalOr, "");
COMPILE_ASSERT(((int)AssignmentExpressionLogicalOr + 1) == (int)AssignmentExpressionLogicalNullish, "");
COMPILE_ASSERT(((int)AssignmentExpressionLogicalNullish + 1) == (int)AssignmentExpressionSimple, "");
COMPILE_ASSERT(((int)AssignmentExpressionSimple - (int)AssignmentExpression) == 16, "");

COMPILE_ASSERT(((int)BinaryExpressionEqual + 1) == (int)BinaryExpressionNotEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionNotEqual + 1) == (int)BinaryExpressionStrictEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionStrictEqual + 1) == (int)BinaryExpressionNotStrictEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionNotStrictEqual + 1) == (int)BinaryExpressionGreaterThan, "");
COMPILE_ASSERT(((int)BinaryExpressionGreaterThan + 1) == (int)BinaryExpressionGreaterThanOrEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionGreaterThanOrEqual + 1) == (int)BinaryExpressionLessThan, "");
COMPILE_ASSERT(((int)BinaryExpressionLessThan + 1) == (int)BinaryExpressionLessThanOrEqual, "");
COMPILE_ASSERT(((int)BinaryExpressionLessThanOrEqual - (int)BinaryExpressionEqual) == 7, "");

COMPILE_ASSERT(((int)UnaryExpressionBitwiseNot + 1) == (int)UnaryExpressionDelete, "");
COMPILE_ASSERT(((int)UnaryExpressionDelete + 1) == (int)UnaryExpressionLogicalNot, "");
COMPILE_ASSERT(((int)UnaryExpressionLogicalNot + 1) == (int)UnaryExpressionMinus, "");
COMPILE_ASSERT(((int)UnaryExpressionMinus + 1) == (int)UnaryExpressionPlus, "");
COMPILE_ASSERT(((int)UnaryExpressionPlus + 1) == (int)UnaryExpressionTypeOf, "");
COMPILE_ASSERT(((int)UnaryExpressionTypeOf + 1) == (int)UnaryExpressionVoid, "");
COMPILE_ASSERT(((int)UnaryExpressionVoid - (int)UnaryExpressionBitwiseNot) == 6, "");

} // namespace Escargot

#endif
