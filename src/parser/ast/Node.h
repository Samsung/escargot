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

#ifndef Node_h
#define Node_h

#include "runtime/Value.h"
#include "parser/ast/CommonASTData.h"
#include "parser/ASTAllocator.h"
#include "util/Vector.h"

namespace Escargot {

class AtomicString;
struct ByteCodeGenerateContext;

class LiteralNode;
class AssignmentPatternNode;
class IdentifierNode;
class ArrayExpressionNode;
class AssignmentExpressionSimpleNode;
class PropertyNode;
class MemberExpressionNode;
class ObjectExpressionNode;
class StatementNode;
class ClassBodyNode;
class ClassElementNode;
class ClassExpressionNode;
class ClassDeclarationNode;
class SequenceExpressionNode;
class SpreadElementNode;
class VariableDeclaratorNode;
class FunctionDeclarationNode;
class FunctionExpressionNode;
class ImportSpecifierNode;
class ImportDefaultSpecifierNode;
class ImportNamespaceSpecifierNode;
class ExportSpecifierNode;
class ArrowParameterPlaceHolderNode;

class Node {
protected:
    Node()
        : m_loc(SIZE_MAX)
    {
    }

public:
    // Every Node is allocated by ASTAllocator (ASTPool memory)
    inline void* operator new(size_t size, ASTAllocator& allocator)
    {
        return allocator.allocate(size);
    }

    // For temporal Node that is allocated in the stack
    inline void* operator new(size_t size, void* p)
    {
        return p;
    }

    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
    void operator delete[](void*) = delete;

    virtual ~Node()
    {
    }

    virtual ASTNodeType type() = 0;

    bool isAssignmentOperation()
    {
        return type() >= ASTNodeType::AssignmentExpression && type() <= ASTNodeType::AssignmentExpressionSimple;
    }

    bool isLogicalOperation()
    {
        return type() >= ASTNodeType::BinaryExpressionLogicalAnd && type() <= ASTNodeType::BinaryExpressionLogicalOr;
    }

    bool isRelationOperation()
    {
        return type() >= ASTNodeType::BinaryExpressionGreaterThan && type() <= ASTNodeType::BinaryExpressionLessThanOrEqual;
    }

    bool isEqualityOperation()
    {
        return type() >= ASTNodeType::BinaryExpressionEqual && type() <= ASTNodeType::BinaryExpressionNotStrictEqual;
    }

    bool isUnaryOperation()
    {
        return type() >= ASTNodeType::UnaryExpressionBitwiseNot && type() <= ASTNodeType::UnaryExpressionVoid;
    }

    bool isIdentifier()
    {
        return type() == ASTNodeType::Identifier;
    }

    ALWAYS_INLINE IdentifierNode* asIdentifier()
    {
        ASSERT(isIdentifier());
        return (IdentifierNode*)this;
    }

    ALWAYS_INLINE ClassExpressionNode* asClassExpression()
    {
        ASSERT(type() == ASTNodeType::ClassExpression);
        return (ClassExpressionNode*)this;
    }

    bool isAssignmentPattern()
    {
        return type() == ASTNodeType::AssignmentPattern;
    }

    ALWAYS_INLINE AssignmentPatternNode* asAssignmentPattern()
    {
        ASSERT(isAssignmentPattern());
        return (AssignmentPatternNode*)this;
    }

    bool isArrayExpression()
    {
        return type() == ASTNodeType::ArrayExpression;
    }

    ALWAYS_INLINE ArrayExpressionNode* asArrayExpression()
    {
        ASSERT(isArrayExpression());
        return (ArrayExpressionNode*)this;
    }

    bool isObjectExpression()
    {
        return type() == ASTNodeType::ObjectExpression;
    }

    ALWAYS_INLINE ObjectExpressionNode* asObjectExpression()
    {
        ASSERT(isObjectExpression());
        return (ObjectExpressionNode*)this;
    }

    bool isProperty()
    {
        return type() == ASTNodeType::Property;
    }

    ALWAYS_INLINE PropertyNode* asProperty()
    {
        ASSERT(isProperty());
        return (PropertyNode*)this;
    }

    bool isAssignmentExpressionSimple()
    {
        return type() == ASTNodeType::AssignmentExpressionSimple;
    }

    ALWAYS_INLINE AssignmentExpressionSimpleNode* asAssignmentExpressionSimple()
    {
        ASSERT(isAssignmentExpressionSimple());
        return (AssignmentExpressionSimpleNode*)this;
    }

    ALWAYS_INLINE MemberExpressionNode* asMemberExpression()
    {
        ASSERT(isMemberExpression());
        return (MemberExpressionNode*)this;
    }

    ALWAYS_INLINE LiteralNode* asLiteral()
    {
        ASSERT(isLiteral());
        return (LiteralNode*)this;
    }

    ALWAYS_INLINE SequenceExpressionNode* asSequenceExpression()
    {
        ASSERT(type() == ASTNodeType::SequenceExpression);
        return (SequenceExpressionNode*)this;
    }

    ALWAYS_INLINE ClassBodyNode* asClassBody()
    {
        ASSERT(type() == ASTNodeType::ClassBody);
        return (ClassBodyNode*)this;
    }

    ALWAYS_INLINE ClassElementNode* asClassElement()
    {
        ASSERT(type() == ASTNodeType::ClassElement);
        return (ClassElementNode*)this;
    }

    ALWAYS_INLINE ClassDeclarationNode* asClassDeclaration()
    {
        ASSERT(type() == ASTNodeType::ClassDeclaration);
        return (ClassDeclarationNode*)this;
    }

    ALWAYS_INLINE VariableDeclaratorNode* asVariableDeclarator()
    {
        ASSERT(type() == ASTNodeType::VariableDeclarator);
        return (VariableDeclaratorNode*)this;
    }

    ALWAYS_INLINE FunctionDeclarationNode* asFunctionDeclaration()
    {
        ASSERT(type() == ASTNodeType::FunctionDeclaration);
        return (FunctionDeclarationNode*)this;
    }

    ALWAYS_INLINE FunctionExpressionNode* asFunctionExpression()
    {
        ASSERT(type() == ASTNodeType::FunctionExpression);
        return (FunctionExpressionNode*)this;
    }

    ALWAYS_INLINE SpreadElementNode* asSpreadElement()
    {
        ASSERT(type() == ASTNodeType::SpreadElement);
        return (SpreadElementNode*)this;
    }

    ALWAYS_INLINE ImportSpecifierNode* asImportSpecifier()
    {
        ASSERT(type() == ASTNodeType::ImportSpecifier);
        return (ImportSpecifierNode*)this;
    }

    ALWAYS_INLINE ImportDefaultSpecifierNode* asImportDefaultSpecifier()
    {
        ASSERT(type() == ASTNodeType::ImportDefaultSpecifier);
        return (ImportDefaultSpecifierNode*)this;
    }

    ALWAYS_INLINE ImportNamespaceSpecifierNode* asImportNamespaceSpecifier()
    {
        ASSERT(type() == ASTNodeType::ImportNamespaceSpecifier);
        return (ImportNamespaceSpecifierNode*)this;
    }

    ALWAYS_INLINE ExportSpecifierNode* asExportSpecifier()
    {
        ASSERT(type() == ASTNodeType::ExportSpecifier);
        return (ExportSpecifierNode*)this;
    }

    ALWAYS_INLINE ArrowParameterPlaceHolderNode* asArrowParameterPlaceHolder()
    {
        ASSERT(type() == ArrowParameterPlaceHolder);
        return (ArrowParameterPlaceHolderNode*)this;
    }

    bool isLiteral()
    {
        return type() == ASTNodeType::Literal;
    }

    bool isMemberExpression()
    {
        return type() == ASTNodeType::MemberExpression;
    }

    bool isSuperExpression()
    {
        return type() == ASTNodeType::SuperExpression;
    }

    virtual bool isExpression()
    {
        return false;
    }

    virtual bool isStatement()
    {
        return false;
    }

    NodeLOC loc()
    {
        return m_loc;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf);

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context);
    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context);

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context);

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn)
    {
        fn(this);
    }

    NodeLOC m_loc;
};

class SentinelNode {
public:
    SentinelNode()
        : m_astNode(nullptr)
        , m_next(nullptr)
    {
    }

    SentinelNode(Node* node)
        : m_astNode(node)
        , m_next(nullptr)
    {
    }

    Node* astNode() const
    {
        return m_astNode;
    }

    SentinelNode* next() const
    {
        return m_next;
    }

    void setASTNode(Node* node)
    {
        m_astNode = node;
    }

    void setNext(SentinelNode* next)
    {
        m_next = next;
    }

    ALWAYS_INLINE void* operator new(size_t size, ASTAllocator& allocator)
    {
        return allocator.allocate(size);
    }

private:
    Node* m_astNode;
    SentinelNode* m_next;
};

class NodeList {
public:
    NodeList()
        : m_size(0)
        , m_headNode()
        , m_lastNode(&m_headNode)
    {
        ASSERT(!m_headNode.next() && !m_lastNode->next());
    }

    NodeList(const NodeList& other)
        : m_size(other.m_size)
        , m_headNode(other.m_size > 0 ? other.m_headNode : nullptr)
        , m_lastNode(other.m_size > 0 ? other.m_lastNode : &m_headNode)
    {
        ASSERT(!m_lastNode->next());
    }

    size_t size() const
    {
        return m_size;
    }

    void append(ASTAllocator& allocator, Node* node)
    {
        SentinelNode* newNode = new (allocator) SentinelNode(node);
        m_lastNode->setNext(newNode);
        m_lastNode = newNode;
        m_size++;
    }

    SentinelNode* begin() const
    {
        return m_headNode.next();
    }

    SentinelNode* end() const
    {
        return nullptr;
    }

    SentinelNode* back()
    {
        return m_lastNode;
    }

    const NodeList& operator=(const NodeList& other)
    {
        if (&other == this) {
            return *this;
        }

        m_size = other.size();
        m_headNode = other.m_headNode;
        if (other.size()) {
            m_lastNode = other.m_lastNode;
        } else {
            m_lastNode = &m_headNode;
        }
        return *this;
    }

private:
    size_t m_size;
    SentinelNode m_headNode;
    SentinelNode* m_lastNode;
};

// we can use atomic allocator here because there is no pointer value on Vector
typedef VectorWithInlineStorage<KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT, Value, GCUtil::gc_malloc_atomic_allocator<Value>> NumeralLiteralVector;
} // namespace Escargot

#endif
