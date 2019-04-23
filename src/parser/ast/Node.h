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
    ASTNodeTypeError,
    ArrowFunctionExpression,
    ArrowParameterPlaceHolder,
    Program,
    Function,
    FunctionExpression,
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
    FunctionDeclaration,
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
    BinaryExpression,
    BinaryExpressionBitwiseAnd,
    BinaryExpressionBitwiseOr,
    BinaryExpressionBitwiseXor,
    BinaryExpressionDivison,
    BinaryExpressionEqual,
    BinaryExpressionGreaterThan,
    BinaryExpressionGreaterThanOrEqual,
    BinaryExpressionIn,
    BinaryExpressionInstanceOf,
    BinaryExpressionLeftShift,
    BinaryExpressionLessThan,
    BinaryExpressionLessThanOrEqual,
    BinaryExpressionLogicalAnd,
    BinaryExpressionLogicalOr,
    BinaryExpressionMinus,
    BinaryExpressionMod,
    BinaryExpressionMultiply,
    BinaryExpressionNotEqual,
    BinaryExpressionNotStrictEqual,
    BinaryExpressionPlus,
    BinaryExpressionSignedRightShift,
    BinaryExpressionStrictEqual,
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
    YieldExpression,
    ConditionalExpression,
    CallExpression,
    VariableDeclarator,
    Identifier,
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
};

struct NodeLOC {
    size_t index;
    explicit NodeLOC(size_t index)
        : index(index)
    {
    }
};

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
class IdentifierNode;
class MemberExpressionNode;
class StatementNode;

class Node : public RefCounted<Node> {
    friend class ScriptParser;

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

    bool isIdentifier()
    {
        return type() == ASTNodeType::Identifier;
    }

    IdentifierNode *asIdentifier()
    {
        ASSERT(isIdentifier());
        return (IdentifierNode *)this;
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

class ASTScopeContextNameInfo {
public:
    ASTScopeContextNameInfo()
    {
        m_value = (size_t)AtomicString().string();
    }

    AtomicString name() const
    {
        return AtomicString((String *)((size_t)m_value & ~1));
    }

    void setName(AtomicString name)
    {
        m_value = (size_t)name.string() | isExplicitlyDeclaredOrParameterName();
    }

    bool isExplicitlyDeclaredOrParameterName() const
    {
        return m_value & 1;
    }

    void setIsExplicitlyDeclaredOrParameterName(bool v)
    {
        if (v)
            m_value = m_value | 1;
        else
            m_value = m_value & ~1;
    }

private:
    size_t m_value;
};

typedef TightVector<ASTScopeContextNameInfo, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<ASTScopeContextNameInfo>> ASTScopeContextNameInfoVector;

struct ASTScopeContext : public gc {
    bool m_isStrict : 1;
    bool m_hasEval : 1;
    bool m_hasWith : 1;
    bool m_hasCatch : 1;
    bool m_hasYield : 1;
    bool m_hasEvaluateBindingId : 1;
    bool m_inCatch : 1;
    bool m_inWith : 1;
    bool m_isArrowFunctionExpression : 1;
    bool m_isClassConstructor : 1;
    bool m_hasManyNumeralLiteral : 1;
    bool m_needsSpecialInitialize : 1; // flag for fd in catch
    bool m_hasRestElement : 1;
    ASTNodeType m_nodeType : 12;
    ASTScopeContextNameInfoVector m_names;
    AtomicStringVector m_usingNames;
    AtomicStringTightVector m_parameters;
    AtomicString m_functionName;
    Vector<ASTScopeContext *, GCUtil::gc_malloc_ignore_off_page_allocator<ASTScopeContext *>> m_childScopes;
    Vector<Value, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<Value>> m_numeralLiteralData;
    ExtendedNodeLOC m_locStart;
#ifndef NDEBUG
    ExtendedNodeLOC m_locEnd;
#else
    NodeLOC m_locEnd;
#endif

    void *operator new(size_t size);
    void *operator new[](size_t size) = delete;

    bool hasName(AtomicString name)
    {
        for (size_t i = 0; i < m_names.size(); i++) {
            if (m_names[i].name() == name) {
                return true;
            }
        }

        return false;
    }

    void insertName(AtomicString name, bool isExplicitlyDeclaredOrParameterName)
    {
        for (size_t i = 0; i < m_names.size(); i++) {
            if (m_names[i].name() == name) {
                if (isExplicitlyDeclaredOrParameterName) {
                    m_names[i].setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
                }
                return;
            }
        }

        ASTScopeContextNameInfo info;
        info.setName(name);
        info.setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
        m_names.push_back(info);
    }

    void insertUsingName(AtomicString name)
    {
        if (VectorUtil::findInVector(m_usingNames, name) == VectorUtil::invalidIndex) {
            m_usingNames.push_back(name);
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

    explicit ASTScopeContext(bool isStrict = false)
        : m_isStrict(isStrict)
        , m_hasEval(false)
        , m_hasWith(false)
        , m_hasCatch(false)
        , m_hasYield(false)
        , m_hasEvaluateBindingId(false)
        , m_inCatch(false)
        , m_inWith(false)
        , m_isArrowFunctionExpression(false)
        , m_isClassConstructor(false)
        , m_hasManyNumeralLiteral(false)
        , m_needsSpecialInitialize(false)
        , m_hasRestElement(false)
        , m_locStart(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#ifndef NDEBUG
        , m_locEnd(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#else
        , m_locEnd(SIZE_MAX)
#endif
    {
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
