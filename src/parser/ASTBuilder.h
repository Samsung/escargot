/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotASTBuilder__
#define __EscargotASTBuilder__

namespace Escargot {

#define FOR_EACH_TARGET_NODE(F)               \
    F(ArrayExpression)                        \
    F(ArrayPattern)                           \
    F(ArrowFunctionExpression)                \
    F(ArrowParameterPlaceHolder)              \
    F(AssignmentExpressionBitwiseAnd)         \
    F(AssignmentExpressionBitwiseOr)          \
    F(AssignmentExpressionBitwiseXor)         \
    F(AssignmentExpressionDivision)           \
    F(AssignmentExpressionLeftShift)          \
    F(AssignmentExpressionMinus)              \
    F(AssignmentExpressionMod)                \
    F(AssignmentExpressionMultiply)           \
    F(AssignmentExpressionPlus)               \
    F(AssignmentExpressionSignedRightShift)   \
    F(AssignmentExpressionSimple)             \
    F(AssignmentExpressionUnsignedRightShift) \
    F(AwaitExpression)                        \
    F(BinaryExpressionBitwiseAnd)             \
    F(BinaryExpressionBitwiseOr)              \
    F(BinaryExpressionBitwiseXor)             \
    F(BinaryExpressionDivision)               \
    F(BinaryExpressionEqual)                  \
    F(BinaryExpressionGreaterThan)            \
    F(BinaryExpressionGreaterThanOrEqual)     \
    F(BinaryExpressionIn)                     \
    F(BinaryExpressionInstanceOf)             \
    F(BinaryExpressionLeftShift)              \
    F(BinaryExpressionLessThan)               \
    F(BinaryExpressionLessThanOrEqual)        \
    F(BinaryExpressionLogicalAnd)             \
    F(BinaryExpressionLogicalOr)              \
    F(BinaryExpressionMinus)                  \
    F(BinaryExpressionMod)                    \
    F(BinaryExpressionMultiply)               \
    F(BinaryExpressionNotEqual)               \
    F(BinaryExpressionNotStrictEqual)         \
    F(BinaryExpressionPlus)                   \
    F(BinaryExpressionSignedRightShift)       \
    F(BinaryExpressionStrictEqual)            \
    F(BinaryExpressionUnsignedRightShift)     \
    F(BlockStatement)                         \
    F(BreakLabelStatement)                    \
    F(BreakStatement)                         \
    F(CallExpression)                         \
    F(CatchClause)                            \
    F(ClassBody)                              \
    F(ClassElement)                           \
    F(ConditionalExpression)                  \
    F(ContinueLabelStatement)                 \
    F(ContinueStatement)                      \
    F(DebuggerStatement)                      \
    F(Directive)                              \
    F(DoWhileStatement)                       \
    F(EmptyStatement)                         \
    F(ExportAllDeclaration)                   \
    F(ExportDefaultDeclaration)               \
    F(ExportNamedDeclaration)                 \
    F(ExportSpecifier)                        \
    F(ExpressionStatement)                    \
    F(ForStatement)                           \
    F(FunctionDeclaration)                    \
    F(FunctionExpression)                     \
    F(IfStatement)                            \
    F(ImportDeclaration)                      \
    F(ImportDefaultSpecifier)                 \
    F(ImportNamespaceSpecifier)               \
    F(ImportSpecifier)                        \
    F(InitializeParameterExpression)          \
    F(LabeledStatement)                       \
    F(Literal)                                \
    F(MemberExpression)                       \
    F(MetaProperty)                           \
    F(NewExpression)                          \
    F(ObjectExpression)                       \
    F(ObjectPattern)                          \
    F(Property)                               \
    F(RegExpLiteral)                          \
    F(RestElement)                            \
    F(ReturnStatement)                        \
    F(SequenceExpression)                     \
    F(SpreadElement)                          \
    F(SuperExpression)                        \
    F(SwitchCase)                             \
    F(SwitchStatement)                        \
    F(TaggedTemplateExpression)               \
    F(TemplateLiteral)                        \
    F(ThisExpression)                         \
    F(ThrowStatement)                         \
    F(TryStatement)                           \
    F(UnaryExpressionBitwiseNot)              \
    F(UnaryExpressionDelete)                  \
    F(UnaryExpressionLogicalNot)              \
    F(UnaryExpressionMinus)                   \
    F(UnaryExpressionPlus)                    \
    F(UnaryExpressionTypeOf)                  \
    F(UnaryExpressionVoid)                    \
    F(UpdateExpressionDecrementPostfix)       \
    F(UpdateExpressionDecrementPrefix)        \
    F(UpdateExpressionIncrementPostfix)       \
    F(UpdateExpressionIncrementPrefix)        \
    F(VariableDeclaration)                    \
    F(VariableDeclarator)                     \
    F(WhileStatement)                         \
    F(WithStatement)                          \
    F(YieldExpression)

class SyntaxNodeList;

class SyntaxNode {
public:
    MAKE_STACK_ALLOCATED();

    SyntaxNode()
        : m_nodeType(ASTNodeTypeError)
        , m_string()
    {
    }

    SyntaxNode(ASTNodeType nodeType)
        : m_nodeType(nodeType)
        , m_string()
    {
    }

    SyntaxNode(ASTNodeType nodeType, const AtomicString& string)
        : m_nodeType(nodeType)
        , m_string(string)
    {
        ASSERT(m_nodeType == Identifier);
    }

    SyntaxNode(std::nullptr_t ptr)
        : m_nodeType(ASTNodeTypeError)
        , m_string()
    {
    }

    SyntaxNode(SyntaxNode* node)
        : m_nodeType(node->m_nodeType)
        , m_string(node->m_string)
    {
        ASSERT(node != nullptr);
    }

    ASTNodeType type() { return m_nodeType; }
    AtomicString& name()
    {
        ASSERT(m_nodeType == Identifier);
        return m_string;
    }

    AtomicString& functionName()
    {
        ASSERT(m_nodeType == FunctionDeclaration || m_nodeType == FunctionExpression);
        return m_string;
    }

    AtomicString& assignmentPatternName()
    {
        ASSERT(m_nodeType == AssignmentPattern);
        return m_string;
    }

    void setNodeType(ASTNodeType type)
    {
        m_nodeType = type;
    }

    void setString(const AtomicString& string)
    {
        m_string = string;
    }

    ALWAYS_INLINE bool isAssignmentOperation()
    {
        return m_nodeType >= ASTNodeType::AssignmentExpression && m_nodeType <= ASTNodeType::AssignmentExpressionSimple;
    }

    ALWAYS_INLINE bool isIdentifier()
    {
        return m_nodeType == ASTNodeType::Identifier;
    }

    ALWAYS_INLINE bool isLiteral()
    {
        return m_nodeType == ASTNodeType::Literal;
    }

    ALWAYS_INLINE SyntaxNode* asIdentifier()
    {
        ASSERT(m_nodeType == Identifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asClassExpression()
    {
        ASSERT(m_nodeType == ClassExpression);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asClassDeclaration()
    {
        ASSERT(m_nodeType == ClassDeclaration);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asSequenceExpression()
    {
        ASSERT(m_nodeType == SequenceExpression);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asFunctionDeclaration()
    {
        ASSERT(m_nodeType == FunctionDeclaration);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asFunctionExpression()
    {
        ASSERT(m_nodeType == FunctionExpression);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asLiteral()
    {
        ASSERT(m_nodeType == Literal);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asImportSpecifier()
    {
        ASSERT(m_nodeType == ImportSpecifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asImportDefaultSpecifier()
    {
        ASSERT(m_nodeType == ImportDefaultSpecifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asImportNamespaceSpecifier()
    {
        ASSERT(m_nodeType == ImportNamespaceSpecifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* asExportSpecifier()
    {
        ASSERT(m_nodeType == ExportSpecifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode appendChild(SyntaxNode c)
    {
        ASSERT(m_nodeType == ASTStatementContainer);
        return SyntaxNode();
    }

    ALWAYS_INLINE SyntaxNode appendChild(SyntaxNode c, SyntaxNode referNode)
    {
        ASSERT(m_nodeType == ASTStatementContainer);
        return SyntaxNode();
    }

    void tryToSetImplicitName(AtomicString s)
    {
        // dummy function for tryToSetImplicitName() of ClassExpressionNode
        ASSERT(m_nodeType == ClassExpression);
        return;
    }

    SyntaxNodeList& expressions()
    {
        // dummy function for expressions() of SequenceExpressionNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        SyntaxNodeList* tempVector;
        return *tempVector;
    }

    ASTFunctionScopeContext* scopeContext()
    {
        // dummy function for scopeContext() of FunctionDeclarationNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        ASTFunctionScopeContext* scopeContext;
        return scopeContext;
    }

    ClassNode& classNode()
    {
        // dummy function for classNode() of ClassDeclarationNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        ClassNode* tempClassNode;
        return *tempClassNode;
    }

    Value value()
    {
        // dummy function for value() of LiteralNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    }

    IdentifierNode* imported()
    {
        // dummy function for imported() of ImportSpecifierNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        IdentifierNode* identifier;
        return identifier;
    }

    IdentifierNode* exported()
    {
        // dummy function for imported() of ExportSpecifierNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        IdentifierNode* identifier;
        return identifier;
    }

    IdentifierNode* local()
    {
        // dummy function for local() of Import/ExportSpecifierNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        IdentifierNode* identifier;
        return identifier;
    }

    SyntaxNode& astNode()
    {
        // dummy function for node() of ASTSentinelNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        return *this;
    }

    SyntaxNode* next()
    {
        // dummy function for next() of ASTSentinelNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        return this;
    }

    void setASTNode(const SyntaxNode& node)
    {
        // dummy function for setASTNode() of ASTSentinelNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE operator bool() const
    {
        return m_nodeType != ASTNodeTypeError;
    }

    ALWAYS_INLINE SyntaxNode* operator->() { return this; }
private:
    ASTNodeType m_nodeType;
    AtomicString m_string;
};

// List for SyntaxNode
// SyntaxNodeList allows insertion(append) operation only
class SyntaxNodeList {
public:
    SyntaxNodeList()
        : m_size(0)
        , m_node()
    {
    }

    size_t size() const
    {
        return m_size;
    }

    void append(ASTAllocator& allocator, const SyntaxNode& val)
    {
        m_size++;
    }

    void append(ASTAllocator& allocator, void* node)
    {
        ASSERT(node == nullptr);
        m_size++;
    }

    /*
    SyntaxNode& operator[](size_t idx)
    {
        return m_node;
    }
    */

    SyntaxNode* begin()
    {
        return &m_node;
    }

    SyntaxNode* end()
    {
        return &m_node;
    }

    SyntaxNode* back()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return &m_node;
    }

private:
    size_t m_size;
    SyntaxNode m_node;
};

class SyntaxChecker {
public:
    typedef SyntaxNode ASTNode;
    typedef SyntaxNode ASTStatementContainer;
    typedef SyntaxNode* ASTSentinelNode;
    typedef SyntaxNodeList ASTNodeList;

    MAKE_STACK_ALLOCATED();

    SyntaxChecker() {}
    bool isNodeGenerator() { return false; }
    SyntaxNode createIdentifierNode(const AtomicString& name)
    {
        return SyntaxNode(ASTNodeType::Identifier, name);
    }

    template <class ClassType>
    SyntaxNode createClass(SyntaxNode id, SyntaxNode superClass, SyntaxNode classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
    {
        // temporally generated Class to identify the type
        ClassType tempClass;
        ASTNodeType type = tempClass.type();
        return SyntaxNode(type);
    }

    SyntaxNode createForInOfStatementNode(SyntaxNode left, SyntaxNode right, SyntaxNode body, bool forIn, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
    {
        if (forIn) {
            return SyntaxNode(ASTNodeType::ForInStatement);
        } else {
            return SyntaxNode(ASTNodeType::ForOfStatement);
        }
    }

    ALWAYS_INLINE SyntaxNode createFunctionNode(StatementContainer* params, BlockStatementNode* body, NumeralLiteralVector&& numeralLiteralVector)
    {
        return SyntaxNode(ASTNodeType::Function);
    }

    ALWAYS_INLINE SyntaxNode createProgramNode(StatementContainer* body, ASTFunctionScopeContext* scopeContext, Script::ModuleData* moduleData, NumeralLiteralVector&& numeralLiteralVector)
    {
        return SyntaxNode(ASTNodeType::Program);
    }

    ALWAYS_INLINE SyntaxNode createAssignmentPatternNode(SyntaxNode left, SyntaxNode right)
    {
        SyntaxNode assign(ASTNodeType::AssignmentPattern);
        if (left.isIdentifier()) {
            assign.setString(left->name());
        }

        return assign;
    }

    ALWAYS_INLINE SyntaxNode createStatementContainer()
    {
        return SyntaxNode(ASTNodeType::ASTStatementContainer);
    }

#define DECLARE_CREATE_FUNCTION(name)                         \
    template <typename... Args>                               \
    ALWAYS_INLINE SyntaxNode create##name##Node(Args... args) \
    {                                                         \
        return SyntaxNode(name);                              \
    }
    FOR_EACH_TARGET_NODE(DECLARE_CREATE_FUNCTION)
#undef DECLARE_CREATE_FUNCTION

    SyntaxNode reinterpretExpressionAsPattern(SyntaxNode& expr)
    {
        SyntaxNode result = expr;
        switch (expr.type()) {
        case Identifier:
        case MemberExpression:
        case RestElement:
        case AssignmentPattern:
            break;
        case SpreadElement:
            result.setNodeType(ASTNodeType::RestElement);
            break;
        case ArrayExpression:
            result.setNodeType(ASTNodeType::ArrayPattern);
            break;
        case ObjectExpression:
            result.setNodeType(ASTNodeType::ObjectPattern);
            break;
        case AssignmentExpressionSimple:
            result.setNodeType(ASTNodeType::AssignmentPattern);
            break;
        default:
            break;
        }

        return result;
    }

    SyntaxNode& convertToParameterSyntaxNode(SyntaxNode& node)
    {
        ASSERT(node->type() == Identifier || node->type() == AssignmentPattern || node->type() == ArrayPattern || node->type() == ObjectPattern || node->type() == RestElement);
        return node;
    }

    SyntaxNode convertTaggedTemplateExpressionToCallExpression(SyntaxNode taggedTemplateExpression, ASTFunctionScopeContext* scopeContext, AtomicString raw)
    {
        return SyntaxNode(CallExpression);
    }
};

class NodeGenerator {
public:
    typedef Node* ASTNode;
    typedef StatementContainer* ASTStatementContainer;
    typedef SentinelNode* ASTSentinelNode;
    typedef NodeList ASTNodeList;

    MAKE_STACK_ALLOCATED();

    NodeGenerator(ASTAllocator& allocator)
        : m_allocator(allocator)
    {
    }

    bool isNodeGenerator() { return true; }
    IdentifierNode* createIdentifierNode(const AtomicString& name)
    {
        return new (m_allocator) IdentifierNode(name);
    }

    template <class ClassType>
    ClassType* createClass(Node* id, Node* superClass, Node* classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
    {
        return new (m_allocator) ClassType(id, superClass, classBody, classBodyLexicalBlockIndex, classSrc);
    }

    ForInOfStatementNode* createForInOfStatementNode(Node* left, Node* right, Node* body, bool forIn, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
    {
        return new (m_allocator) ForInOfStatementNode(left, right, body, forIn, hasLexicalDeclarationOnInit, headLexicalBlockIndex, iterationLexicalBlockIndex);
    }

    FunctionNode* createFunctionNode(StatementContainer* params, BlockStatementNode* body, NumeralLiteralVector&& numeralLiteralVector)
    {
        return new (m_allocator) FunctionNode(params, body, std::forward<NumeralLiteralVector>(numeralLiteralVector));
    }

    ProgramNode* createProgramNode(StatementContainer* body, ASTFunctionScopeContext* scopeContext, Script::ModuleData* moduleData, NumeralLiteralVector&& numeralLiteralVector)
    {
        return new (m_allocator) ProgramNode(body, scopeContext, moduleData, std::forward<NumeralLiteralVector>(numeralLiteralVector));
    }

    AssignmentPatternNode* createAssignmentPatternNode(Node* left, Node* right)
    {
        return new (m_allocator) AssignmentPatternNode(left, right);
    }

    StatementContainer* createStatementContainer()
    {
        return StatementContainer::create(m_allocator);
    }

#define DECLARE_CREATE_FUNCTION(name)                          \
    template <typename... Args>                                \
    ALWAYS_INLINE name##Node* create##name##Node(Args... args) \
    {                                                          \
        return new (m_allocator) name##Node(args...);          \
    }
    FOR_EACH_TARGET_NODE(DECLARE_CREATE_FUNCTION)
#undef DECLARE_CREATE_FUNCTION

    Node* reinterpretExpressionAsPattern(Node* expr)
    {
        Node* result = expr;
        switch (expr->type()) {
        case Identifier:
        case MemberExpression:
        case RestElement:
        case AssignmentPattern:
            break;
        case SpreadElement: {
            SpreadElementNode* spread = (SpreadElementNode*)expr;
            Node* arg = this->reinterpretExpressionAsPattern(spread->argument());
            result = new (m_allocator) RestElementNode(arg, spread->m_loc);
            break;
        }
        case ArrayExpression: {
            ArrayExpressionNode* array = expr->asArrayExpression();
            NodeList& elements = array->elements();
            for (SentinelNode* element = elements.begin(); element != elements.end(); element = element->next()) {
                if (element->astNode() != nullptr) {
                    element->setASTNode(this->reinterpretExpressionAsPattern(element->astNode()));
                }
            }
            result = new (m_allocator) ArrayPatternNode(elements, array->m_loc);
            break;
        }
        case ObjectExpression: {
            ObjectExpressionNode* object = expr->asObjectExpression();
            NodeList& properties = object->properties();
            for (SentinelNode* property = properties.begin(); property != properties.end(); property = property->next()) {
                ASSERT(property->astNode()->type() == Property);
                Node* value = this->reinterpretExpressionAsPattern(property->astNode()->asProperty()->value());
                property->astNode()->asProperty()->setValue(value);
            }
            result = new (m_allocator) ObjectPatternNode(properties, object->m_loc);
            break;
        }
        case AssignmentExpressionSimple: {
            AssignmentExpressionSimpleNode* assign = expr->asAssignmentExpressionSimple();
            Node* left = this->reinterpretExpressionAsPattern(assign->left());
            result = new (m_allocator) AssignmentPatternNode(left, assign->right(), assign->m_loc);
            break;
        }
        default:
            break;
        }
        return result;
    }

    SyntaxNode convertToParameterSyntaxNode(Node* node)
    {
        SyntaxNode paramNode(node->type());
        if (node->isIdentifier()) {
            paramNode.setString(node->asIdentifier()->name());
        } else if (node->isAssignmentPattern()) {
            if (node->asAssignmentPattern()->left()->isIdentifier()) {
                paramNode.setString(node->asAssignmentPattern()->left()->asIdentifier()->name());
            }
        }

        return paramNode;
    }

    // FIXME MetaNode
    Node* convertTaggedTemplateExpressionToCallExpression(TaggedTemplateExpressionNode* taggedTemplateExpression, ASTFunctionScopeContext* scopeContext, AtomicString raw)
    {
        TemplateLiteralNode* templateLiteral = (TemplateLiteralNode*)taggedTemplateExpression->quasi();
        NodeList args;
        NodeList elements;
        for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
            UTF16StringData& sd = (*templateLiteral->quasis())[i]->value;
            String* str = new UTF16String(std::move(sd));
            elements.append(m_allocator, new (m_allocator) LiteralNode(Value(str)));
        }

        ArrayExpressionNode* arrayExpressionForRaw = nullptr;
        {
            NodeList elements;
            for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
                UTF16StringData& sd = (*templateLiteral->quasis())[i]->valueRaw;
                String* str = new UTF16String(std::move(sd));
                elements.append(m_allocator, new (m_allocator) LiteralNode(Value(str)));
            }
            arrayExpressionForRaw = new (m_allocator) ArrayExpressionNode(elements);
        }

        ArrayExpressionNode* quasiVector = new (m_allocator) ArrayExpressionNode(elements, raw, arrayExpressionForRaw, false, true);
        args.append(m_allocator, quasiVector);
        for (SentinelNode* expression = templateLiteral->expressions().begin(); expression != templateLiteral->expressions().end(); expression = expression->next()) {
            args.append(m_allocator, expression->astNode());
        }

        if (taggedTemplateExpression->expr()->isIdentifier() && taggedTemplateExpression->expr()->asIdentifier()->name() == "eval") {
            scopeContext->m_hasEval = true;
        }
        return new (m_allocator) CallExpressionNode(taggedTemplateExpression->expr(), args);
    }

private:
    ASTAllocator& m_allocator;
};

} // Escargot

#endif
