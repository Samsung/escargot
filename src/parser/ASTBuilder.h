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
    F(CatchClause)                            \
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
    F(ExportSpecifier)                        \
    F(ExpressionStatement)                    \
    F(ForStatement)                           \
    F(FunctionDeclaration)                    \
    F(FunctionExpression)                     \
    F(IfStatement)                            \
    F(ImportDefaultSpecifier)                 \
    F(ImportNamespaceSpecifier)               \
    F(ImportSpecifier)                        \
    F(InitializeParameterExpression)          \
    F(LabeledStatement)                       \
    F(Literal)                                \
    F(MemberExpression)                       \
    F(MetaProperty)                           \
    F(Property)                               \
    F(RegExpLiteral)                          \
    F(RestElement)                            \
    F(ReturnStatement)                        \
    F(SpreadElement)                          \
    F(SuperExpression)                        \
    F(SwitchCase)                             \
    F(SwitchStatement)                        \
    F(TaggedTemplateExpression)               \
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
    F(VariableDeclarator)                     \
    F(WhileStatement)                         \
    F(WithStatement)                          \
    F(YieldExpression)

class SyntaxNodeVector;

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

    SyntaxNodeVector& expressions()
    {
        // dummy function for expressions() of SequenceExpressionNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        SyntaxNodeVector* tempVector;
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
        ClassNode* tempClassNode = new ClassNode();
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
        return new IdentifierNode();
    }

    IdentifierNode* exported()
    {
        // dummy function for imported() of ExportSpecifierNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        return new IdentifierNode();
    }

    IdentifierNode* local()
    {
        // dummy function for local() of Import/ExportSpecifierNode
        // this function should never be invoked
        RELEASE_ASSERT_NOT_REACHED();
        return new IdentifierNode();
    }

    ALWAYS_INLINE operator bool()
    {
        return m_nodeType != ASTNodeTypeError;
    }

    ALWAYS_INLINE SyntaxNode* operator->() { return this; }
private:
    ASTNodeType m_nodeType;
    AtomicString m_string;
};

// Vector for SyntaxNode
// SyntaxNodeVector allows insertion(push_back) operation only
class SyntaxNodeVector {
public:
    SyntaxNodeVector()
        : m_size(0)
        , m_node()
    {
    }

    size_t size() const
    {
        return m_size;
    }

    void push_back(const SyntaxNode& val)
    {
        m_size++;
    }

    void pop_back()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    SyntaxNode& operator[](size_t idx)
    {
        return m_node;
    }

    SyntaxNode& back()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return m_node;
    }

    size_t begin()
    {
        return 0;
    }

    size_t end()
    {
        return m_size;
    }

    void insert(size_t pos, size_t first, size_t last)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    size_t m_size;
    SyntaxNode m_node;
};

typedef VectorWithInlineStorage<16, SyntaxNode, std::allocator<SyntaxNode>> ParameterNodeVector;

class SyntaxChecker {
public:
    typedef SyntaxNode ASTNode;
    typedef SyntaxNode ASTIdentifierNode;
    typedef SyntaxNode ASTStatementContainer;
    typedef SyntaxNode* ASTStatementNodePtr;
    typedef SyntaxNodeVector ASTNodeVector;

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

    ALWAYS_INLINE SyntaxNode createSequenceExpressionNode(ASTNodeVector&& expressions)
    {
        return SyntaxNode(ASTNodeType::SequenceExpression);
    }

    ALWAYS_INLINE SyntaxNode createVariableDeclarationNode(ASTNodeVector&& decl, EscargotLexer::KeywordKind kind)
    {
        return SyntaxNode(ASTNodeType::VariableDeclaration);
    }

    ALWAYS_INLINE SyntaxNode createArrayPatternNode(ASTNodeVector&& elements)
    {
        return SyntaxNode(ASTNodeType::ArrayPattern);
    }

    ALWAYS_INLINE SyntaxNode createObjectPatternNode(ASTNodeVector&& properties)
    {
        return SyntaxNode(ASTNodeType::ObjectPattern);
    }

    ALWAYS_INLINE SyntaxNode createCallExpressionNode(SyntaxNode callee, ASTNodeVector&& arguments)
    {
        return SyntaxNode(ASTNodeType::CallExpression);
    }

    ALWAYS_INLINE SyntaxNode createClassBodyNode(ASTNodeVector&& elementList, ASTNode constructor)
    {
        return SyntaxNode(ASTNodeType::ClassBody);
    }

    ALWAYS_INLINE SyntaxNode createNewExpressionNode(SyntaxNode callee, ASTNodeVector&& arguments)
    {
        return SyntaxNode(ASTNodeType::NewExpression);
    }

    ALWAYS_INLINE SyntaxNode createTemplateLiteralNode(TemplateElementVector* vector, ASTNodeVector&& expressions)
    {
        return SyntaxNode(ASTNodeType::TemplateLiteral);
    }

    ALWAYS_INLINE SyntaxNode createArrowParameterPlaceHolderNode(ASTNodeVector&& params)
    {
        return SyntaxNode(ASTNodeType::ArrowParameterPlaceHolder);
    }

    ALWAYS_INLINE SyntaxNode createArrayExpressionNode(ASTNodeVector&& elements, AtomicString additionalPropertyName, SyntaxNode additionalPropertyExpression, bool hasSpreadElement, bool isTaggedTemplateExpression)
    {
        return SyntaxNode(ASTNodeType::ArrayExpression);
    }

    ALWAYS_INLINE SyntaxNode createObjectExpressionNode(ASTNodeVector&& properties)
    {
        return SyntaxNode(ASTNodeType::ObjectExpression);
    }

    ALWAYS_INLINE SyntaxNode createExportNamedDeclarationNode(ASTNode declaration, ASTNodeVector&& specifiers, ASTNode source)
    {
        return SyntaxNode(ASTNodeType::ExportNamedDeclaration);
    }

    ALWAYS_INLINE SyntaxNode createImportDeclarationNode(ASTNodeVector&& specifiers, ASTNode src)
    {
        return SyntaxNode(ASTNodeType::ImportDeclaration);
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
    typedef IdentifierNode* ASTIdentifierNode;
    typedef StatementContainer* ASTStatementContainer;
    typedef StatementNode* ASTStatementNodePtr;
    typedef std::vector<Node*> ASTNodeVector;

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

    SequenceExpressionNode* createSequenceExpressionNode(ASTNodeVector&& expressions)
    {
        return new (m_allocator) SequenceExpressionNode(std::forward<ASTNodeVector>(expressions));
    }

    VariableDeclarationNode* createVariableDeclarationNode(ASTNodeVector&& decl, EscargotLexer::KeywordKind kind)
    {
        return new (m_allocator) VariableDeclarationNode(std::forward<ASTNodeVector>(decl), kind);
    }

    ArrayPatternNode* createArrayPatternNode(ASTNodeVector&& elements)
    {
        return new (m_allocator) ArrayPatternNode(std::forward<ASTNodeVector>(elements));
    }

    ObjectPatternNode* createObjectPatternNode(ASTNodeVector&& properties)
    {
        return new (m_allocator) ObjectPatternNode(std::forward<ASTNodeVector>(properties));
    }

    CallExpressionNode* createCallExpressionNode(Node* callee, ASTNodeVector&& arguments)
    {
        return new (m_allocator) CallExpressionNode(callee, std::forward<ASTNodeVector>(arguments));
    }

    ClassBodyNode* createClassBodyNode(ASTNodeVector&& elementList, ASTNode constructor)
    {
        return new (m_allocator) ClassBodyNode(std::forward<ASTNodeVector>(elementList), constructor);
    }

    NewExpressionNode* createNewExpressionNode(Node* callee, ASTNodeVector&& arguments)
    {
        return new (m_allocator) NewExpressionNode(callee, std::forward<ASTNodeVector>(arguments));
    }

    TemplateLiteralNode* createTemplateLiteralNode(TemplateElementVector* vector, ASTNodeVector&& expressions)
    {
        return new (m_allocator) TemplateLiteralNode(vector, std::forward<ASTNodeVector>(expressions));
    }

    ArrowParameterPlaceHolderNode* createArrowParameterPlaceHolderNode(ASTNodeVector&& params)
    {
        return new (m_allocator) ArrowParameterPlaceHolderNode(std::forward<ASTNodeVector>(params));
    }

    ArrayExpressionNode* createArrayExpressionNode(ASTNodeVector&& elements, AtomicString additionalPropertyName, Node* additionalPropertyExpression, bool hasSpreadElement, bool isTaggedTemplateExpression)
    {
        return new (m_allocator) ArrayExpressionNode(std::forward<ASTNodeVector>(elements), additionalPropertyName, additionalPropertyExpression, hasSpreadElement, isTaggedTemplateExpression);
    }

    ObjectExpressionNode* createObjectExpressionNode(ASTNodeVector&& properties)
    {
        return new (m_allocator) ObjectExpressionNode(std::forward<ASTNodeVector>(properties));
    }

    ExportNamedDeclarationNode* createExportNamedDeclarationNode(Node* declaration, ASTNodeVector&& specifiers, Node* source)
    {
        return new (m_allocator) ExportNamedDeclarationNode(declaration, std::forward<ASTNodeVector>(specifiers), source);
    }

    ImportDeclarationNode* createImportDeclarationNode(ASTNodeVector&& specifiers, Node* src)
    {
        return new (m_allocator) ImportDeclarationNode(std::forward<ASTNodeVector>(specifiers), src);
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
            NodeVector& elements = array->elements();
            for (size_t i = 0; i < elements.size(); i++) {
                if (elements[i] != nullptr) {
                    elements[i] = this->reinterpretExpressionAsPattern(elements[i]);
                }
            }
            result = new (m_allocator) ArrayPatternNode(std::move(elements), array->m_loc);
            break;
        }
        case ObjectExpression: {
            ObjectExpressionNode* object = expr->asObjectExpression();
            NodeVector& properties = object->properties();
            for (size_t i = 0; i < properties.size(); i++) {
                ASSERT(properties[i]->type() == Property);
                Node* value = this->reinterpretExpressionAsPattern(properties[i]->asProperty()->value());
                properties[i]->asProperty()->setValue(value);
            }
            result = new (m_allocator) ObjectPatternNode(std::move(properties), object->m_loc);
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
        NodeVector args;
        NodeVector elements;
        for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
            UTF16StringData& sd = (*templateLiteral->quasis())[i]->value;
            String* str = new UTF16String(std::move(sd));
            elements.push_back(new (m_allocator) LiteralNode(Value(str)));
        }

        ArrayExpressionNode* arrayExpressionForRaw = nullptr;
        {
            NodeVector elements;
            for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
                UTF16StringData& sd = (*templateLiteral->quasis())[i]->valueRaw;
                String* str = new UTF16String(std::move(sd));
                elements.push_back(new (m_allocator) LiteralNode(Value(str)));
            }
            arrayExpressionForRaw = new (m_allocator) ArrayExpressionNode(std::move(elements));
        }

        ArrayExpressionNode* quasiVector = new (m_allocator) ArrayExpressionNode(std::move(elements), raw, arrayExpressionForRaw, false, true);
        args.push_back(quasiVector);
        for (size_t i = 0; i < templateLiteral->expressions().size(); i++) {
            args.push_back(templateLiteral->expressions()[i]);
        }

        if (taggedTemplateExpression->expr()->isIdentifier() && taggedTemplateExpression->expr()->asIdentifier()->name() == "eval") {
            scopeContext->m_hasEval = true;
        }
        return new (m_allocator) CallExpressionNode(taggedTemplateExpression->expr(), std::move(args));
    }

private:
    ASTAllocator& m_allocator;
};

} // Escargot

#endif
