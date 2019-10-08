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
    F(FunctionExpression)                     \
    F(ArrowFunctionExpression)                \
    F(ArrowParameterPlaceHolder)              \
    F(FunctionDeclaration)                    \
    F(Property)                               \
    F(Empty)                                  \
    F(EmptyStatement)                         \
    F(BlockStatement)                         \
    F(BreakStatement)                         \
    F(BreakLabelStatement)                    \
    F(ContinueStatement)                      \
    F(ContinueLabelStatement)                 \
    F(ReturnStatement)                        \
    F(IfStatement)                            \
    F(ForStatement)                           \
    F(WhileStatement)                         \
    F(DoWhileStatement)                       \
    F(SpreadElement)                          \
    F(RestElement)                            \
    F(SwitchStatement)                        \
    F(SwitchCase)                             \
    F(WithStatement)                          \
    F(ThisExpression)                         \
    F(ExpressionStatement)                    \
    F(UnaryExpressionBitwiseNot)              \
    F(UnaryExpressionDelete)                  \
    F(UnaryExpressionLogicalNot)              \
    F(UnaryExpressionMinus)                   \
    F(UnaryExpressionPlus)                    \
    F(UnaryExpressionTypeOf)                  \
    F(UnaryExpressionVoid)                    \
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
    F(AssignmentExpressionUnsignedRightShift) \
    F(AssignmentExpressionSimple)             \
    F(BinaryExpressionBitwiseAnd)             \
    F(BinaryExpressionBitwiseOr)              \
    F(BinaryExpressionBitwiseXor)             \
    F(BinaryExpressionDivision)               \
    F(BinaryExpressionEqual)                  \
    F(BinaryExpressionNotEqual)               \
    F(BinaryExpressionStrictEqual)            \
    F(BinaryExpressionNotStrictEqual)         \
    F(BinaryExpressionGreaterThan)            \
    F(BinaryExpressionGreaterThanOrEqual)     \
    F(BinaryExpressionLessThan)               \
    F(BinaryExpressionLessThanOrEqual)        \
    F(BinaryExpressionIn)                     \
    F(BinaryExpressionInstanceOf)             \
    F(BinaryExpressionLeftShift)              \
    F(BinaryExpressionLogicalAnd)             \
    F(BinaryExpressionLogicalOr)              \
    F(BinaryExpressionMinus)                  \
    F(BinaryExpressionMod)                    \
    F(BinaryExpressionMultiply)               \
    F(BinaryExpressionPlus)                   \
    F(BinaryExpressionSignedRightShift)       \
    F(BinaryExpressionUnsignedRightShift)     \
    F(UpdateExpressionDecrementPostfix)       \
    F(UpdateExpressionDecrementPrefix)        \
    F(UpdateExpressionIncrementPostfix)       \
    F(UpdateExpressionIncrementPrefix)        \
    F(MemberExpression)                       \
    F(MetaProperty)                           \
    F(YieldExpression)                        \
    F(ConditionalExpression)                  \
    F(VariableDeclarator)                     \
    F(InitializeParameterExpression)          \
    F(LabeledStatement)                       \
    F(Literal)                                \
    F(TaggedTemplateExpression)               \
    F(Directive)                              \
    F(RegExpLiteral)                          \
    F(CatchClause)                            \
    F(TryStatement)                           \
    F(ThrowStatement)                         \
    F(ClassElement)                           \
    F(SuperExpression)                        \
    F(AssignmentPattern)                      \
    F(RegisterReference)                      \
    F(ExportDefaultDeclaration)               \
    F(ExportAllDeclaration)                   \
    F(ImportNamespaceSpecifier)               \
    F(ImportDefaultSpecifier)                 \
    F(ImportSpecifier)                        \
    F(ExportSpecifier)                        \
    F(DebuggerStatement)

class SyntaxNode {
public:
    MAKE_STACK_ALLOCATED();

    SyntaxNode()
        : m_nodeType(ASTNodeTypeError)
        , m_string(AtomicString())
    {
    }

    SyntaxNode(ASTNodeType nodeType)
        : m_nodeType(nodeType)
        , m_string(AtomicString())
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
        , m_string(AtomicString())
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

    void setNodeType(ASTNodeType type)
    {
        m_nodeType = type;
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

    ALWAYS_INLINE SyntaxNode* asExportSpecifier()
    {
        ASSERT(m_nodeType == ExportSpecifier);
        return this;
    }

    ALWAYS_INLINE SyntaxNode* appendChild(SyntaxNode* c)
    {
        ASSERT(m_nodeType == ASTStatementContainer);
        return nullptr;
    }

    ALWAYS_INLINE SyntaxNode* appendChild(SyntaxNode* c, SyntaxNode* referNode)
    {
        ASSERT(m_nodeType == ASTStatementContainer);
        return nullptr;
    }

    void tryToSetImplicitName(AtomicString s)
    {
        // dummy function for tryToSetImplicitName() of ClassExpressionNode
        ASSERT(m_nodeType == ClassExpression);
        return;
    }

    std::vector<SyntaxNode>& expressions()
    {
        // dummy function for expressions() of SequenceExpressionNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        std::vector<SyntaxNode>* tempVector = new std::vector<SyntaxNode>();
        return *tempVector;
    }

    ASTFunctionScopeContext* scopeContext()
    {
        // dummy function for scopeContext() of FunctionDeclarationNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        return new ASTFunctionScopeContext();
    }

    ClassNode& classNode()
    {
        // dummy function for classNode() of ClassDeclarationNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        ClassNode* tempClassNode = new ClassNode();
        return *tempClassNode;
    }

    Value value()
    {
        // dummy function for value() of LiteralNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        return Value();
    }

    IdentifierNode* imported()
    {
        // dummy function for imported() of ImportSpecifierNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        return new IdentifierNode();
    }

    IdentifierNode* exported()
    {
        // dummy function for imported() of ExportSpecifierNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        return new IdentifierNode();
    }

    IdentifierNode* local()
    {
        // dummy function for local() of Import/ExportSpecifierNode
        // this function should never be invocked
        RELEASE_ASSERT_NOT_REACHED();
        return new IdentifierNode();
    }

    ALWAYS_INLINE operator bool()
    {
        return m_nodeType != ASTNodeTypeError;
    }

    ALWAYS_INLINE SyntaxNode* operator->() { return this; }
    ALWAYS_INLINE SyntaxNode* get() { return this; }
    ALWAYS_INLINE SyntaxNode release() { return *this; }
private:
    ASTNodeType m_nodeType;
    AtomicString m_string;
};

class SyntaxChecker {
public:
    MAKE_STACK_ALLOCATED();

    SyntaxChecker() {}
    typedef SyntaxNode ASTNode;
    typedef SyntaxNode ASTPassNode;
    typedef SyntaxNode ASTNodePtr;
    typedef SyntaxNode ASTIdentifierNode;
    typedef SyntaxNode ASTStatementContainer;
    typedef SyntaxNode* ASTStatementNodePtr;
    typedef std::vector<SyntaxNode> ASTNodeVector;

    bool isNodeGenerator() { return false; }
    SyntaxNode createIdentifierNode(const AtomicString& name)
    {
        return SyntaxNode(ASTNodeType::Identifier, name);
    }

    template <class ClassType>
    SyntaxNode createClass(SyntaxNode id, SyntaxNode superClass, SyntaxNode classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
    {
        ClassType* tempClass = new ClassType();
        tempClass->relaxAdoptionRequirement();
        ASTNodeType type = tempClass->type();
        tempClass->deref();
        return SyntaxNode(type);
    }

    SyntaxNode createForInOfStatementNode(SyntaxNode* left, SyntaxNode* right, SyntaxNode* body, bool forIn, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
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

    ALWAYS_INLINE SyntaxNode createCallExpressionNode(SyntaxNode* callee, ASTNodeVector&& arguments)
    {
        return SyntaxNode(ASTNodeType::CallExpression);
    }

    ALWAYS_INLINE SyntaxNode createClassBodyNode(ASTNodeVector&& elementList, ASTNode constructor)
    {
        return SyntaxNode(ASTNodeType::ClassBody);
    }

    ALWAYS_INLINE SyntaxNode createNewExpressionNode(SyntaxNode* callee, ASTNodeVector&& arguments)
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

    ALWAYS_INLINE SyntaxNode createArrayExpressionNode(ASTNodeVector&& elements, AtomicString additionalPropertyName, SyntaxNode* additionalPropertyExpression, bool hasSpreadElement, bool isTaggedTemplateExpression)
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

    SyntaxNode reinterpretExpressionAsPattern(SyntaxNode* expr)
    {
        SyntaxNode result = *expr;
        switch (expr->type()) {
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

    SyntaxNode convertTaggedTemplateExpressionToCallExpression(SyntaxNode taggedTemplateExpression, ASTFunctionScopeContext* scopeContext, AtomicString raw)
    {
        return SyntaxNode(CallExpression);
    }
};

class NodeGenerator {
public:
    MAKE_STACK_ALLOCATED();

    NodeGenerator() {}
    typedef RefPtr<Node> ASTNode;
    typedef PassRefPtr<Node> ASTPassNode;
    typedef Node* ASTNodePtr;
    typedef IdentifierNode* ASTIdentifierNode;
    typedef RefPtr<StatementContainer> ASTStatementContainer;
    typedef StatementNode* ASTStatementNodePtr;
    typedef std::vector<RefPtr<Node>> ASTNodeVector;

    bool isNodeGenerator() { return true; }
    IdentifierNode* createIdentifierNode(const AtomicString& name)
    {
        return new IdentifierNode(name);
    }

    template <class ClassType>
    ClassType* createClass(RefPtr<Node> id, RefPtr<Node> superClass, RefPtr<Node> classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
    {
        return new ClassType(id, superClass, classBody, classBodyLexicalBlockIndex, classSrc);
    }

    ForInOfStatementNode* createForInOfStatementNode(Node* left, Node* right, Node* body, bool forIn, bool hasLexicalDeclarationOnInit, LexicalBlockIndex headLexicalBlockIndex, LexicalBlockIndex iterationLexicalBlockIndex)
    {
        return new ForInOfStatementNode(left, right, body, forIn, hasLexicalDeclarationOnInit, headLexicalBlockIndex, iterationLexicalBlockIndex);
    }

    SequenceExpressionNode* createSequenceExpressionNode(ASTNodeVector&& expressions)
    {
        return new SequenceExpressionNode(std::forward<ASTNodeVector>(expressions));
    }

    VariableDeclarationNode* createVariableDeclarationNode(ASTNodeVector&& decl, EscargotLexer::KeywordKind kind)
    {
        return new VariableDeclarationNode(std::forward<ASTNodeVector>(decl), kind);
    }

    ArrayPatternNode* createArrayPatternNode(ASTNodeVector&& elements)
    {
        return new ArrayPatternNode(std::forward<ASTNodeVector>(elements));
    }

    ObjectPatternNode* createObjectPatternNode(ASTNodeVector&& properties)
    {
        return new ObjectPatternNode(std::forward<ASTNodeVector>(properties));
    }

    CallExpressionNode* createCallExpressionNode(Node* callee, ASTNodeVector&& arguments)
    {
        return new CallExpressionNode(callee, std::forward<ASTNodeVector>(arguments));
    }

    ClassBodyNode* createClassBodyNode(ASTNodeVector&& elementList, ASTNode constructor)
    {
        return new ClassBodyNode(std::forward<ASTNodeVector>(elementList), constructor);
    }

    NewExpressionNode* createNewExpressionNode(Node* callee, ASTNodeVector&& arguments)
    {
        return new NewExpressionNode(callee, std::forward<ASTNodeVector>(arguments));
    }

    TemplateLiteralNode* createTemplateLiteralNode(TemplateElementVector* vector, ASTNodeVector&& expressions)
    {
        return new TemplateLiteralNode(vector, std::forward<ASTNodeVector>(expressions));
    }

    ArrowParameterPlaceHolderNode* createArrowParameterPlaceHolderNode(ASTNodeVector&& params)
    {
        return new ArrowParameterPlaceHolderNode(std::forward<ASTNodeVector>(params));
    }

    ArrayExpressionNode* createArrayExpressionNode(ASTNodeVector&& elements, AtomicString additionalPropertyName, Node* additionalPropertyExpression, bool hasSpreadElement, bool isTaggedTemplateExpression)
    {
        return new ArrayExpressionNode(std::forward<ASTNodeVector>(elements), additionalPropertyName, additionalPropertyExpression, hasSpreadElement, isTaggedTemplateExpression);
    }

    ObjectExpressionNode* createObjectExpressionNode(ASTNodeVector&& properties)
    {
        return new ObjectExpressionNode(std::forward<ASTNodeVector>(properties));
    }

    ExportNamedDeclarationNode* createExportNamedDeclarationNode(RefPtr<Node> declaration, ASTNodeVector&& specifiers, RefPtr<Node> source)
    {
        return new ExportNamedDeclarationNode(declaration, std::forward<ASTNodeVector>(specifiers), source);
    }

    ImportDeclarationNode* createImportDeclarationNode(ASTNodeVector&& specifiers, RefPtr<Node> src)
    {
        return new ImportDeclarationNode(std::forward<ASTNodeVector>(specifiers), src);
    }

    RefPtr<StatementContainer> createStatementContainer()
    {
        return StatementContainer::create();
    }

#define DECLARE_CREATE_FUNCTION(name)                          \
    template <typename... Args>                                \
    ALWAYS_INLINE name##Node* create##name##Node(Args... args) \
    {                                                          \
        return new name##Node(args...);                        \
    }
    FOR_EACH_TARGET_NODE(DECLARE_CREATE_FUNCTION)
#undef DECLARE_CREATE_FUNCTION

    RefPtr<Node> reinterpretExpressionAsPattern(Node* expr)
    {
        RefPtr<Node> result = expr;
        switch (expr->type()) {
        case Identifier:
        case MemberExpression:
        case RestElement:
        case AssignmentPattern:
            break;
        case SpreadElement: {
            SpreadElementNode* spread = (SpreadElementNode*)expr;
            RefPtr<Node> arg = this->reinterpretExpressionAsPattern(spread->argument());
            result = adoptRef(new RestElementNode(arg.get(), spread->m_loc));
            break;
        }
        case ArrayExpression: {
            ArrayExpressionNode* array = expr->asArrayExpression();
            ExpressionNodeVector& elements = array->elements();
            for (size_t i = 0; i < elements.size(); i++) {
                if (elements[i] != nullptr) {
                    elements[i] = this->reinterpretExpressionAsPattern(elements[i].get());
                }
            }
            result = adoptRef(new ArrayPatternNode(std::move(elements), array->m_loc));
            break;
        }
        case ObjectExpression: {
            ObjectExpressionNode* object = expr->asObjectExpression();
            PropertiesNodeVector& properties = object->properties();
            for (size_t i = 0; i < properties.size(); i++) {
                ASSERT(properties[i]->type() == Property);
                RefPtr<Node> value = this->reinterpretExpressionAsPattern(properties[i]->asProperty()->value());
                properties[i]->asProperty()->setValue(value.get());
            }
            result = adoptRef(new ObjectPatternNode(std::move(properties), object->m_loc));
            break;
        }
        case AssignmentExpressionSimple: {
            AssignmentExpressionSimpleNode* assign = expr->asAssignmentExpressionSimple();
            RefPtr<Node> left = this->reinterpretExpressionAsPattern(assign->left());
            result = adoptRef(new AssignmentPatternNode(left.get(), assign->right(), assign->m_loc));
            break;
        }
        default:
            break;
        }
        return result;
    }

    // FIXME MetaNode
    PassRefPtr<Node> convertTaggedTemplateExpressionToCallExpression(RefPtr<TaggedTemplateExpressionNode> taggedTemplateExpression, ASTFunctionScopeContext* scopeContext, AtomicString raw)
    {
        TemplateLiteralNode* templateLiteral = (TemplateLiteralNode*)taggedTemplateExpression->quasi();
        ArgumentVector args;
        ExpressionNodeVector elements;
        for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
            UTF16StringData& sd = (*templateLiteral->quasis())[i]->value;
            String* str = new UTF16String(std::move(sd));
            elements.push_back(adoptRef(new LiteralNode(Value(str))));
        }

        RefPtr<ArrayExpressionNode> arrayExpressionForRaw;
        {
            ExpressionNodeVector elements;
            for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
                UTF16StringData& sd = (*templateLiteral->quasis())[i]->valueRaw;
                String* str = new UTF16String(std::move(sd));
                elements.push_back(adoptRef(new LiteralNode(Value(str))));
            }
            arrayExpressionForRaw = adoptRef(new ArrayExpressionNode(std::move(elements)));
        }

        RefPtr<ArrayExpressionNode> quasiVector = adoptRef(new ArrayExpressionNode(std::move(elements), raw, arrayExpressionForRaw.get(), false, true));
        args.push_back(quasiVector.get());
        for (size_t i = 0; i < templateLiteral->expressions().size(); i++) {
            args.push_back(templateLiteral->expressions()[i]);
        }

        if (taggedTemplateExpression->expr()->isIdentifier() && taggedTemplateExpression->expr()->asIdentifier()->name() == "eval") {
            scopeContext->m_hasEval = true;
        }
        return adoptRef(new CallExpressionNode(taggedTemplateExpression->expr(), std::move(args)));
    }
};
} // Escargot

#endif
