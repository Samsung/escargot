/*
  Copyright JS Foundation and other contributors, https://js.foundation/
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
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

#include "Escargot.h"
#include "esprima.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"
#include "parser/Lexer.h"

#define ALLOC_TOKEN(tokenName) Scanner::ScannerResult* tokenName = ALLOCA(sizeof(Scanner::ScannerResult), Scanner::ScannerResult, ec);

using namespace Escargot::EscargotLexer;

namespace Escargot {

namespace esprima {

namespace Messages {
const char* UnexpectedToken = "Unexpected token %s";
const char* UnexpectedNumber = "Unexpected number";
const char* UnexpectedString = "Unexpected string";
const char* UnexpectedIdentifier = "Unexpected identifier";
const char* UnexpectedReserved = "Unexpected reserved word";
const char* UnexpectedTemplate = "Unexpected quasi %s";
const char* UnexpectedEOS = "Unexpected end of input";
const char* NewlineAfterThrow = "Illegal newline after throw";
const char* InvalidRegExp = "Invalid regular expression";
const char* InvalidLHSInAssignment = "Invalid left-hand side in assignment";
const char* InvalidLHSInForIn = "Invalid left-hand side in for-in";
const char* InvalidLHSInForLoop = "Invalid left-hand side in for-loop";
const char* MultipleDefaultsInSwitch = "More than one default clause in switch statement";
const char* NoCatchOrFinally = "Missing catch or finally after try";
const char* UnknownLabel = "Undefined label \'%s\'";
const char* Redeclaration = "%s \'%s\' has already been declared";
const char* IllegalContinue = "Illegal continue statement";
const char* IllegalBreak = "Illegal break statement";
const char* IllegalReturn = "Illegal return statement";
const char* StrictModeWith = "Strict mode code may not include a with statement";
const char* StrictCatchVariable = "Catch variable may not be eval or arguments in strict mode";
const char* StrictVarName = "Variable name may not be eval or arguments in strict mode";
const char* StrictParamName = "Parameter name eval or arguments is not allowed in strict mode";
const char* StrictParamDupe = "Strict mode function may not have duplicate parameter names";
const char* StrictFunctionName = "Function name may not be eval or arguments in strict mode";
const char* StrictOctalLiteral = "Octal literals are not allowed in strict mode.";
const char* StrictLeadingZeroLiteral = "Decimals with leading zeros are not allowed in strict mode.";
const char* StrictDelete = "Delete of an unqualified identifier in strict mode.";
const char* StrictLHSAssignment = "Assignment to eval or arguments is not allowed in strict mode";
const char* StrictLHSPostfix = "Postfix increment/decrement may not have eval or arguments operand in strict mode";
const char* StrictLHSPrefix = "Prefix increment/decrement may not have eval or arguments operand in strict mode";
const char* StrictReservedWord = "Use of future reserved word in strict mode";
const char* ParameterAfterRestParameter = "Rest parameter must be last formal parameter";
const char* DefaultRestParameter = "Unexpected token =";
const char* ObjectPatternAsRestParameter = "Unexpected token {";
const char* DuplicateProtoProperty = "Duplicate __proto__ fields are not allowed in object literals";
const char* ConstructorSpecialMethod = "Class constructor may not be an accessor";
const char* DuplicateConstructor = "A class may only have one constructor";
const char* StaticPrototype = "Classes may not have static property named prototype";
const char* MissingFromClause = "Unexpected token";
const char* NoAsAfterImportNamespace = "Unexpected token";
const char* InvalidModuleSpecifier = "Unexpected token";
const char* IllegalImportDeclaration = "Unexpected token";
const char* IllegalExportDeclaration = "Unexpected token";
const char* DuplicateBinding = "Duplicate binding %s";
const char* ForInOfLoopInitializer = "%s loop variable declaration may not have an initializer";
} // namespace Messages

struct Config {
    // Config always allocated on the stack
    MAKE_STACK_ALLOCATED();

    bool range : 1;
    bool loc : 1;
    bool tokens : 1;
    bool comment : 1;
    bool parseSingleFunction : 1;
    uint32_t parseSingleFunctionChildIndex;
    CodeBlock* parseSingleFunctionTarget;
};


struct Context {
    // Escargot::esprima::Context always allocated on the stack
    MAKE_STACK_ALLOCATED();

    bool allowIn : 1;
    bool allowYield : 1;
    bool allowLexicalDeclaration : 1;
    bool isAssignmentTarget : 1;
    bool isBindingElement : 1;
    bool inFunctionBody : 1;
    bool inIteration : 1;
    bool inSwitch : 1;
    bool inArrowFunction : 1;
    bool inWith : 1;
    bool inCatchClause : 1;
    bool inLoop : 1;
    bool strict : 1;
    Scanner::ScannerResult firstCoverInitializedNameError;
    std::vector<AtomicString> catchClauseSimplyDeclaredVariableNames;
    std::vector<std::pair<AtomicString, bool>> labelSet; // <LabelString, continue accepted>
};

struct Marker {
    size_t index;
    size_t lineNumber;
    size_t lineStart;
};

struct MetaNode {
    size_t index;
    size_t line;
    size_t column;
};


/*
struct ArrowParameterPlaceHolderNode {
    String* type;
    ExpressionNodeVector params;
};
*/

struct DeclarationOptions {
    bool inFor;
    KeywordKind kind;
};

#define Parse PassNode<Node>, true
#define ParseAs(nodeType) PassNode<nodeType>, true
#define Scan ScanExpressionResult, false
#define ScanAsVoid void, false

class Parser {
public:
    // Parser always allocated on the stack
    MAKE_STACK_ALLOCATED();

    ::Escargot::Context* escargotContext;
    Config config;
    Scanner* scanner;
    Scanner scannerInstance;

    enum SourceType {
        Script,
        Module
    };

    SourceType sourceType;
    Scanner::ScannerResult lookahead;
    bool hasLineTerminator;

    Context contextInstance;
    Context* context;
    Marker baseMarker;
    Marker startMarker;
    Marker lastMarker;

    Vector<ASTFunctionScopeContext*, GCUtil::gc_malloc_allocator<ASTFunctionScopeContext*>> scopeContexts;
    ASTFunctionScopeContext* lastPoppedScopeContext;
    bool trackUsingNames;
    AtomicString lastUsingName;
    size_t stackLimit;
    size_t subCodeBlockIndex;

    LexicalBlockIndex lexicalBlockIndex;
    LexicalBlockIndex lexicalBlockCount;

    ASTFunctionScopeContext fakeContext;

    class ScanExpressionResult;

    /* Structure for conversion. Do not use it for storing a value! */
    template <typename T>
    class PassNode : public PassRefPtr<T> {
    public:
        PassNode(PassRefPtr<T> node)
            : PassRefPtr<T>(node)
        {
        }

        operator ScanExpressionResult()
        {
            return ScanExpressionResult();
        }
    };

    class ScanExpressionResult {
    public:
        ScanExpressionResult()
            : nodeType(ASTNodeTypeError)
            , str(AtomicString())
        {
        }

        ScanExpressionResult(ASTNodeType nodeType)
            : nodeType(nodeType)
            , str(AtomicString())
        {
        }

        ScanExpressionResult(ASTNodeType nodeType, AtomicString& string)
            : nodeType(nodeType)
            , str(string)
        {
        }

        template <typename T>
        ScanExpressionResult(PassRefPtr<T>)
            : nodeType(ASTNodeTypeError)
            , str(AtomicString())
        {
        }

        template <typename T>
        operator PassNode<T>()
        {
            return PassNode<T>(nullptr);
        }

        operator ASTNodeType()
        {
            return nodeType;
        }

        AtomicString& string()
        {
            return str;
        }

        void setNodeType(ASTNodeType newNodeType)
        {
            nodeType = newNodeType;
            str = AtomicString();
        }

    private:
        ASTNodeType nodeType;
        AtomicString str;
    };

    struct ParseFormalParametersResult {
        PatternNodeVector params;
        std::vector<AtomicString, gc_allocator<AtomicString>> paramSet;
        Scanner::ScannerResult stricted;
        Scanner::ScannerResult firstRestricted;
        const char* message;
        bool valid;

        ParseFormalParametersResult()
            : message(nullptr)
            , valid(false)
        {
        }
    };

    ASTFunctionScopeContext* popScopeContext(const MetaNode& node)
    {
        auto ret = scopeContexts.back();
        scopeContexts.pop_back();
        lastUsingName = AtomicString();
        lastPoppedScopeContext = ret;
        return ret;
    }

    void pushScopeContext(ASTFunctionScopeContext* ctx)
    {
        scopeContexts.push_back(ctx);
        lastUsingName = AtomicString();
    }

    ALWAYS_INLINE void insertUsingName(AtomicString name)
    {
        if (lastUsingName == name) {
            return;
        }
        scopeContexts.back()->insertUsingName(name, this->lexicalBlockIndex);
        lastUsingName = name;
    }

    void extractNamesFromFunctionParams(const ParseFormalParametersResult& paramsResult)
    {
        if (this->config.parseSingleFunction) {
            return;
        }

        const PatternNodeVector& params = paramsResult.params;
        const std::vector<AtomicString, gc_allocator<AtomicString>>& paramNames = paramsResult.paramSet;
        bool hasParameterOtherThanIdentifier = false;
#ifndef NDEBUG
        bool shouldHaveEqualNumberOfParameterListAndParameterName = true;
#endif
        scopeContexts.back()->m_parameters.resizeWithUninitializedValues(params.size());
        for (size_t i = 0; i < params.size(); i++) {
            switch (params[i]->type()) {
            case Identifier: {
                scopeContexts.back()->m_parameters[i] = params[i]->asIdentifier()->name();
                break;
            }
            case AssignmentPattern: {
                hasParameterOtherThanIdentifier = true;
                AtomicString id;
                if (params[i]->asAssignmentPattern()->left()->isIdentifier()) {
                    id = params[i]->asAssignmentPattern()->left()->asIdentifier()->name();
                }
#ifndef NDEBUG
                else {
                    shouldHaveEqualNumberOfParameterListAndParameterName = false;
                }
#endif
                scopeContexts.back()->m_parameters[i] = id;
                break;
            }
            case ArrayPattern:
            case ObjectPattern: {
                hasParameterOtherThanIdentifier = true;
#ifndef NDEBUG
                shouldHaveEqualNumberOfParameterListAndParameterName = false;
#endif
                scopeContexts.back()->m_parameters[i] = AtomicString();
                break;
            }
            case RestElement: {
                hasParameterOtherThanIdentifier = true;
#ifndef NDEBUG
                shouldHaveEqualNumberOfParameterListAndParameterName = false;
#endif
                scopeContexts.back()->m_parameters.erase(i);
                break;
            }
            default: {
                RELEASE_ASSERT_NOT_REACHED();
            }
            }
        }
#ifndef NDEBUG
        if (shouldHaveEqualNumberOfParameterListAndParameterName) {
            ASSERT(params.size() == paramNames.size());
        }
#endif
        scopeContexts.back()->m_hasParameterOtherThanIdentifier = hasParameterOtherThanIdentifier;
        for (size_t i = 0; i < paramNames.size(); i++) {
            scopeContexts.back()->insertVarName(paramNames[i], 0, true);
        }

        // Check if any identifier names are duplicated.
        if (hasParameterOtherThanIdentifier) {
            if (paramNames.size() < 2) {
                return;
            }
            for (size_t i = 0; i < paramNames.size() - 1; i++) {
                // Note: using this inner for loop because std::find didn't work for some reason.
                for (size_t j = i + 1; j < paramNames.size(); j++) {
                    if (paramNames[i] == paramNames[j]) {
                        this->throwError("duplicate argument names are not allowed in this context");
                    }
                }
            }
        }
    }

    void pushScopeContext(AtomicString functionName)
    {
        this->subCodeBlockIndex++;
        if (this->config.parseSingleFunction) {
            fakeContext = ASTFunctionScopeContext();
            pushScopeContext(&fakeContext);
            return;
        }
        auto parentContext = scopeContexts.back();
        pushScopeContext(new ASTFunctionScopeContext(this->context->strict));
        scopeContexts.back()->m_functionName = functionName;
        scopeContexts.back()->m_inWith = this->context->inWith;

        if (parentContext) {
            parentContext->m_childScopes.push_back(scopeContexts.back());
        }
    }

    Parser(::Escargot::Context* escargotContext, StringView code, size_t stackRemain, ExtendedNodeLOC startLoc = ExtendedNodeLOC(0, 0, 0))
        : scannerInstance(escargotContext, code, startLoc.line, startLoc.column)
    {
        ASSERT(escargotContext != nullptr);

        if (stackRemain >= STACK_LIMIT_FROM_BASE) {
            stackRemain = STACK_LIMIT_FROM_BASE;
        }
        volatile int sp;
        volatile size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        this->stackLimit = currentStackBase - stackRemain;
#else
        this->stackLimit = currentStackBase + stackRemain;
#endif
        this->escargotContext = escargotContext;
        this->lexicalBlockIndex = 0;
        this->lexicalBlockCount = 0;
        this->subCodeBlockIndex = 0;
        lastPoppedScopeContext = nullptr;
        trackUsingNames = true;
        config.range = false;
        config.loc = false;
        // config.source = String::emptyString;
        config.tokens = false;
        config.comment = false;
        config.parseSingleFunction = false;
        config.parseSingleFunctionTarget = nullptr;
        config.parseSingleFunctionChildIndex = 0;
        /*
        this->config = {
            range: (typeof options.range == 'boolean') && options.range,
            loc: (typeof options.loc == 'boolean') && options.loc,
            source: null,
            tokens: (typeof options.tokens == 'boolean') && options.tokens,
            comment: (typeof options.comment == 'boolean') && options.comment,
        };
        if (this->config.loc && options.source && options.source !== null) {
            this->config.source = String(options.source);
        }*/

        this->scanner = &scannerInstance;
        if (stackRemain >= STACK_LIMIT_FROM_BASE) {
            stackRemain = STACK_LIMIT_FROM_BASE;
        }

        // this->sourceType = (options && options.sourceType == 'module') ? 'module' : 'script';
        this->sourceType = Script;
        this->hasLineTerminator = false;

        this->context = &contextInstance;
        this->context->allowIn = true;
        this->context->allowYield = true;
        this->context->allowLexicalDeclaration = false;
        this->context->firstCoverInitializedNameError.reset();
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        this->context->inFunctionBody = false;
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inArrowFunction = false;
        this->context->inWith = false;
        this->context->inCatchClause = false;
        this->context->inLoop = false;
        this->context->strict = this->sourceType == Module;
        this->setMarkers(startLoc);
    }

    void setMarkers(ExtendedNodeLOC startLoc)
    {
        this->baseMarker.index = startLoc.index;
        this->baseMarker.lineNumber = this->scanner->lineNumber;
        this->baseMarker.lineStart = 0;

        this->startMarker.index = 0;
        this->startMarker.lineNumber = this->scanner->lineNumber;
        this->startMarker.lineStart = 0;

        this->lastMarker.index = 0;
        this->lastMarker.lineNumber = this->scanner->lineNumber;
        this->lastMarker.lineStart = 0;

        {
            this->lastMarker.index = this->scanner->index;
            this->lastMarker.lineNumber = this->scanner->lineNumber;
            this->lastMarker.lineStart = this->scanner->lineStart;

            this->collectComments();

            this->startMarker.index = this->scanner->index;
            this->startMarker.lineNumber = this->scanner->lineNumber;
            this->startMarker.lineStart = this->scanner->lineStart;

            Scanner::ScannerResult* next = &this->lookahead;
            this->scanner->lex(next);
            this->hasLineTerminator = false;

            if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource())) {
                next->type = Token::KeywordToken;
            }
        }
        this->lastMarker.index = this->scanner->index;
        this->lastMarker.lineNumber = this->scanner->lineNumber;
        this->lastMarker.lineStart = this->scanner->lineStart;
    }

    void throwError(const char* messageFormat, String* arg0 = String::emptyString, String* arg1 = String::emptyString, ErrorObject::Code code = ErrorObject::SyntaxError)
    {
        UTF16StringDataNonGCStd msg;
        if (arg0->length() && arg1->length()) {
            UTF8StringData d1 = arg0->toUTF8StringData();
            UTF8StringData d2 = arg1->toUTF8StringData();

            auto temp = utf8StringToUTF16String(messageFormat, strlen(messageFormat));
            msg = UTF16StringDataNonGCStd(temp.data(), temp.length());
            UTF16StringDataNonGCStd from(u"%s");
            UTF16StringDataNonGCStd arg0Data(arg0->toUTF16StringData().data());
            UTF16StringDataNonGCStd arg1Data(arg1->toUTF16StringData().data());
            size_t start_pos = msg.find(from, 0);
            RELEASE_ASSERT(start_pos != SIZE_MAX);
            msg.replace(start_pos, from.length(), arg0Data);

            start_pos = msg.find(from, start_pos + arg0Data.length());
            RELEASE_ASSERT(start_pos != SIZE_MAX);
            msg.replace(start_pos, from.length(), arg1Data);
        } else if (arg0->length()) {
            UTF8StringData d1 = arg0->toUTF8StringData();
            auto temp = utf8StringToUTF16String(messageFormat, strlen(messageFormat));
            msg = UTF16StringDataNonGCStd(temp.data(), temp.length());
            UTF16StringDataNonGCStd from(u"%s");
            UTF16StringDataNonGCStd argData(arg0->toUTF16StringData().data());
            size_t start_pos = msg.find(from, 0);
            RELEASE_ASSERT(start_pos != SIZE_MAX);
            msg.replace(start_pos, from.length(), argData);
        } else {
            msg.assign(messageFormat, &messageFormat[strlen(messageFormat)]);
        }

        size_t index = this->lastMarker.index;
        size_t line = this->lastMarker.lineNumber;
        size_t column = this->lastMarker.index - this->lastMarker.lineStart + 1;

        ErrorHandler::throwError(index, line, column, new UTF16String(msg.data(), msg.length()), code);
    }

    void replaceAll(UTF16StringDataNonGCStd& str, const UTF16StringDataNonGCStd& from, const UTF16StringDataNonGCStd& to)
    {
        if (from.empty())
            return;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    const char* checkTokenIdentifier(const unsigned char type)
    {
        switch (type) {
        case Token::EOFToken:
            return Messages::UnexpectedEOS;
        case Token::IdentifierToken:
            return Messages::UnexpectedIdentifier;
        case Token::NumericLiteralToken:
            return Messages::UnexpectedNumber;
        case Token::StringLiteralToken:
            return Messages::UnexpectedString;
        case Token::TemplateToken:
            return Messages::UnexpectedTemplate;
        default:
            return Messages::UnexpectedToken;
        }
    }

    // Throw an exception because of an unexpected token.
    void throwUnexpectedToken(Scanner::ScannerResult* token, const char* message = nullptr)
    {
        ASSERT(token != nullptr);
        const char* msg;
        if (message) {
            msg = message;
        } else {
            msg = Messages::UnexpectedToken;
        }

        String* value;
        if (token->type != InvalidToken) {
            if (!msg) {
                msg = checkTokenIdentifier(token->type);

                if (token->type == Token::KeywordToken) {
                    if (this->scanner->isFutureReservedWord(token->relatedSource())) {
                        msg = Messages::UnexpectedReserved;
                    } else if (this->context->strict && this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                        msg = Messages::StrictReservedWord;
                    }
                }
            } else if (token->type == Token::EOFToken) {
                msg = Messages::UnexpectedEOS;
            }
            value = (token->type == Token::TemplateToken) ? (String*)new UTF16String(std::move(token->valueTemplate->valueRaw)) : (String*)new StringView(token->relatedSource());
        } else {
            value = new ASCIIString("ILLEGAL");
        }

        // msg = msg.replace('%0', value);
        UTF16StringDataNonGCStd msgData;
        msgData.assign(msg, &msg[strlen(msg)]);
        UTF16StringDataNonGCStd valueData = UTF16StringDataNonGCStd(value->toUTF16StringData().data());
        replaceAll(msgData, UTF16StringDataNonGCStd(u"%s"), valueData);

        // if (token && typeof token.lineNumber == 'number') {
        if (token->type != InvalidToken) {
            const size_t index = token->start;
            const size_t line = token->lineNumber;
            const size_t column = token->start - this->lastMarker.lineStart + 1;
            ErrorHandler::throwError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        } else {
            const size_t index = this->lastMarker.index;
            const size_t line = this->lastMarker.lineNumber;
            const size_t column = index - this->lastMarker.lineStart + 1;
            ErrorHandler::throwError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        }
    }

    ALWAYS_INLINE void collectComments()
    {
        this->scanner->scanComments();
    }

    void nextToken(Scanner::ScannerResult* token = nullptr)
    {
        if (token) {
            *token = this->lookahead;
        }
        size_t tokenLineNumber = this->lookahead.lineNumber;

        this->lastMarker.index = this->scanner->index;
        this->lastMarker.lineNumber = this->scanner->lineNumber;
        this->lastMarker.lineStart = this->scanner->lineStart;

        this->collectComments();

        this->startMarker.index = this->scanner->index;
        this->startMarker.lineNumber = this->scanner->lineNumber;
        this->startMarker.lineStart = this->scanner->lineStart;

        Scanner::ScannerResult* next = &this->lookahead;
        this->scanner->lex(next);
        this->hasLineTerminator = tokenLineNumber != next->lineNumber;

        if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource())) {
            next->type = Token::KeywordToken;
        }
        //this->lookahead = *next;

        /*
        if (this->config.tokens && next.type !== Token.EOF) {
            this->tokens.push(this->convertToken(next));
        }
        */
    }

    void nextRegexToken(Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        this->collectComments();

        this->scanner->scanRegExp(token);

        // Prime the next lookahead.
        this->lookahead = *token;
        this->nextToken();
    }

    MetaNode createNode()
    {
        MetaNode n;
        n.index = this->startMarker.index + this->baseMarker.index;
        n.line = this->startMarker.lineNumber;
        n.column = this->startMarker.index - this->startMarker.lineStart;
        return n;
    }

    MetaNode startNode(Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        MetaNode n;
        n.index = token->start + this->baseMarker.index;
        n.line = token->lineNumber;
        n.column = token->start - token->lineStart;
        return n;
    }

    template <typename T>
    PassRefPtr<T> finalize(MetaNode meta, T* node)
    {
        /*
        if (this->config.range) {
            node.range = [meta.index, this->lastMarker.index];
        }

        if (this->config.loc) {
            node.loc = {
                start: {
                    line: meta.line,
                    column: meta.column
                },
                end: {
                    line: this->lastMarker.lineNumber,
                    column: this->lastMarker.index - this->lastMarker.lineStart
                }
            };
            if (this->config.source) {
                node.loc.source = this->config.source;
            }
        }*/
        auto type = node->type();
        if (type == CallExpression) {
            CallExpressionNode* c = (CallExpressionNode*)node;
            if (c->callee() && c->callee()->isIdentifier() && ((IdentifierNode*)c->callee())->name() == this->escargotContext->staticStrings().eval) {
                scopeContexts.back()->m_hasEval = true;
            }
        }

        node->m_loc = NodeLOC(meta.index);
        return adoptRef(node);
    }

    // Expect the next token to match the specified punctuator.
    // If not, an exception will be thrown.
    void expect(PunctuatorKind value)
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type != Token::PunctuatorToken || token->valuePunctuatorKind != value) {
            this->throwUnexpectedToken(token);
        }
    }

    void expectCommaSeparator()
    {
        this->expect(PunctuatorKind::Comma);
    }

    // Expect the next token to match the specified keyword.
    // If not, an exception will be thrown.

    void expectKeyword(KeywordKind keyword)
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type != Token::KeywordToken || token->valueKeywordKind != keyword) {
            this->throwUnexpectedToken(token);
        }
    }

    // Return true if the next token matches the specified punctuator.

    bool match(PunctuatorKind value)
    {
        return this->lookahead.type == Token::PunctuatorToken && this->lookahead.valuePunctuatorKind == value;
    }

    // Return true if the next token matches the specified keyword

    bool matchKeyword(KeywordKind keyword)
    {
        return this->lookahead.type == Token::KeywordToken && this->lookahead.valueKeywordKind == keyword;
    }

    // Return true if the next token matches the specified contextual keyword
    // (where an identifier is sometimes a keyword depending on the context)

    bool matchContextualKeyword(const char* keyword)
    {
        return this->lookahead.type == Token::IdentifierToken && this->lookahead.valueStringLiteralData->equals(keyword);
    }

    // Return true if the next token is an assignment operator

    bool matchAssign()
    {
        if (this->lookahead.type != Token::PunctuatorToken) {
            return false;
        }
        PunctuatorKind op = this->lookahead.valuePunctuatorKind;

        if (op >= Substitution && op < SubstitutionEnd) {
            return true;
        }
        return false;
    }

    // Cover grammar support.
    //
    // When an assignment expression position starts with an left parenthesis, the determination of the type
    // of the syntax is to be deferred arbitrarily long until the end of the parentheses pair (plus a lookahead)
    // or the first comma. This situation also defers the determination of all the expressions nested in the pair.
    //
    // There are three productions that can be parsed in a parentheses pair that needs to be determined
    // after the outermost pair is closed. They are:
    //
    //   1. AssignmentExpression
    //   2. BindingElements
    //   3. AssignmentTargets
    //
    // In order to avoid exponential backtracking, we use two flags to denote if the production can be
    // binding element or assignment target.
    //
    // The three productions have the relationship:
    //
    //   BindingElements ⊆ AssignmentTargets ⊆ AssignmentExpression
    //
    // with a single exception that CoverInitializedName when used directly in an Expression, generates
    // an early error. Therefore, we need the third state, firstCoverInitializedNameError, to track the
    // first usage of CoverInitializedName and report it when we reached the end of the parentheses pair.
    //
    // isolateCoverGrammar function runs the given parser function with a new cover grammar context, and it does not
    // effect the current flags. This means the production the parser parses is only used as an expression. Therefore
    // the CoverInitializedName check is conducted.
    //
    // inheritCoverGrammar function runs the given parse function with a new cover grammar context, and it propagates
    // the flags outside of the parser. This means the production the parser parses is used as a part of a potential
    // pattern. The CoverInitializedName check is deferred.

    typedef Node* (Parser::*ParseFunction)();

    typedef Vector<Scanner::ScannerResult, GCUtil::gc_malloc_allocator<Scanner::ScannerResult>> ScannerResultVector;

    struct IsolateCoverGrammarContext {
        bool previousIsBindingElement;
        bool previousIsAssignmentTarget;
        Scanner::ScannerResult previousFirstCoverInitializedNameError;
    };

    void checkRecursiveLimit()
    {
        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        if (UNLIKELY(currentStackBase < stackLimit)) {
#else
        if (UNLIKELY(currentStackBase > stackLimit)) {
#endif
            this->throwError("too many recursion in script", String::emptyString, String::emptyString, ErrorObject::RangeError);
        }
    }

    void startCoverGrammar(IsolateCoverGrammarContext* grammarContext)
    {
        ASSERT(grammarContext != nullptr);

        grammarContext->previousIsBindingElement = this->context->isBindingElement;
        grammarContext->previousIsAssignmentTarget = this->context->isAssignmentTarget;
        grammarContext->previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError.reset();

        checkRecursiveLimit();
    }

    void endIsolateCoverGrammar(IsolateCoverGrammarContext* grammarContext)
    {
        ASSERT(grammarContext != nullptr);

        if (UNLIKELY(this->context->firstCoverInitializedNameError)) {
            this->throwUnexpectedToken(&this->context->firstCoverInitializedNameError);
        }

        this->context->isBindingElement = grammarContext->previousIsBindingElement;
        this->context->isAssignmentTarget = grammarContext->previousIsAssignmentTarget;
        this->context->firstCoverInitializedNameError = grammarContext->previousFirstCoverInitializedNameError;
    }

    void endInheritCoverGrammar(IsolateCoverGrammarContext* grammarContext)
    {
        ASSERT(grammarContext != nullptr);

        this->context->isBindingElement = this->context->isBindingElement && grammarContext->previousIsBindingElement;
        this->context->isAssignmentTarget = this->context->isAssignmentTarget && grammarContext->previousIsAssignmentTarget;
        if (UNLIKELY(grammarContext->previousFirstCoverInitializedNameError)) {
            this->context->firstCoverInitializedNameError = grammarContext->previousFirstCoverInitializedNameError;
        }
    }

    template <typename T>
    ALWAYS_INLINE PassRefPtr<Node> isolateCoverGrammar(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        PassRefPtr<Node> result = (this->*parseFunction)();
        endIsolateCoverGrammar(&grammarContext);

        return result;
    }

    template <typename T>
    ALWAYS_INLINE ScanExpressionResult scanIsolateCoverGrammar(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        ScanExpressionResult ret = (this->*parseFunction)();
        endIsolateCoverGrammar(&grammarContext);

        return ret;
    }

    template <typename T>
    ALWAYS_INLINE PassRefPtr<Node> isolateCoverGrammarWithFunctor(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        PassRefPtr<Node> result = parseFunction();
        endIsolateCoverGrammar(&grammarContext);

        return result;
    }

    template <typename T>
    ALWAYS_INLINE void scanIsolateCoverGrammarWithFunctor(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        parseFunction();
        endIsolateCoverGrammar(&grammarContext);
    }

    template <typename T>
    ALWAYS_INLINE PassRefPtr<Node> inheritCoverGrammar(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        PassRefPtr<Node> result = (this->*parseFunction)();
        endInheritCoverGrammar(&grammarContext);

        return result;
    }

    template <typename T>
    ALWAYS_INLINE ScanExpressionResult scanInheritCoverGrammar(T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        ScanExpressionResult result = (this->*parseFunction)();
        endInheritCoverGrammar(&grammarContext);

        return result;
    }

    void consumeSemicolon()
    {
        if (this->match(PunctuatorKind::SemiColon)) {
            this->nextToken();
        } else if (!this->hasLineTerminator) {
            if (this->lookahead.type != Token::EOFToken && !this->match(PunctuatorKind::RightBrace)) {
                this->throwUnexpectedToken(&this->lookahead);
            }
            this->lastMarker.index = this->startMarker.index;
            this->lastMarker.lineNumber = this->startMarker.lineNumber;
            this->lastMarker.lineStart = this->startMarker.lineStart;
        }
    }

    IdentifierNode* finishIdentifier(Scanner::ScannerResult* token, bool isScopeVariableName)
    {
        ASSERT(token != nullptr);
        IdentifierNode* ret;
        StringView* sv = token->valueStringLiteral();
        const auto& a = sv->bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            ret = new IdentifierNode(this->escargotContext->staticStrings().asciiTable[firstCh]);
        } else {
            if (!token->plain) {
                ret = new IdentifierNode(AtomicString(this->escargotContext, sv->string()));
            } else {
                ret = new IdentifierNode(AtomicString(this->escargotContext, SourceStringView(*sv)));
            }
        }

        if (trackUsingNames) {
            insertUsingName(ret->name());
        }
        return ret;
    }

    ScanExpressionResult finishScanIdentifier(Scanner::ScannerResult* token, bool isScopeVariableName)
    {
        ASSERT(token != nullptr);
        AtomicString name;
        StringView* sv = token->valueStringLiteral();
        const auto& a = sv->bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            name = this->escargotContext->staticStrings().asciiTable[firstCh];
        } else {
            if (!token->plain) {
                name = AtomicString(this->escargotContext, sv->string());
            } else {
                name = AtomicString(this->escargotContext, SourceStringView(*sv));
            }
        }

        if (trackUsingNames) {
            insertUsingName(name);
        }

        return ScanExpressionResult(ASTNodeType::Identifier, name);
    }

#define DEFINE_AS_NODE(TypeName)                                 \
    RefPtr<TypeName##Node> as##TypeName##Node(RefPtr<Node> n)    \
    {                                                            \
        if (!n)                                                  \
            return RefPtr<TypeName##Node>(nullptr);              \
        ASSERT(n->is##TypeName##Node());                         \
        return RefPtr<TypeName##Node>((TypeName##Node*)n.get()); \
    }

    DEFINE_AS_NODE(Expression);
    DEFINE_AS_NODE(Statement);

    // ECMA-262 12.2 Primary Expressions

    template <typename T, bool isParse>
    T primaryExpression(void)
    {
        MetaNode node = this->createNode();

        // let value, token, raw;
        switch (this->lookahead.type) {
        case Token::IdentifierToken: {
            if (this->sourceType == SourceType::Module && this->lookahead.valueKeywordKind == AwaitKeyword) {
                this->throwUnexpectedToken(&this->lookahead);
            }
            ALLOC_TOKEN(token);
            this->nextToken(token);
            if (isParse) {
                return T(this->finalize(node, finishIdentifier(token, true)));
            }
            return finishScanIdentifier(token, true);
        }
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && this->lookahead.octal) {
                this->throwUnexpectedToken(&this->lookahead, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead.startWithZero) {
                this->throwUnexpectedToken(&this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                // raw = this->getTokenRaw(token);
                if (token->type == Token::NumericLiteralToken) {
                    if (this->context->inLoop || token->valueNumber == 0)
                        this->scopeContexts.back()->insertNumeralLiteral(Value(token->valueNumber));
                    if (isParse) {
                        return T(this->finalize(node, new LiteralNode(Value(token->valueNumber))));
                    }
                    return ScanExpressionResult(ASTNodeType::Literal);
                } else {
                    if (isParse) {
                        return T(this->finalize(node, new LiteralNode(token->valueStringLiteralForAST())));
                    }
                    return ScanExpressionResult(ASTNodeType::Literal);
                }
            }
            break;

        case Token::BooleanLiteralToken: {
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            ALLOC_TOKEN(token);
            this->nextToken(token);
            // token.value = (token.value === 'true');
            // raw = this->getTokenRaw(token);
            {
                bool value = token->relatedSource() == "true";
                this->scopeContexts.back()->insertNumeralLiteral(Value(value));
                if (isParse) {
                    return T(this->finalize(node, new LiteralNode(Value(value))));
                }
                return ScanExpressionResult(ASTNodeType::Literal);
            }
            break;
        }

        case Token::NullLiteralToken: {
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            ALLOC_TOKEN(token);
            this->nextToken(token);
            // token.value = null;
            // raw = this->getTokenRaw(token);
            this->scopeContexts.back()->insertNumeralLiteral(Value(Value::Null));
            if (isParse) {
                return T(this->finalize(node, new LiteralNode(Value(Value::Null))));
            }
            return ScanExpressionResult(ASTNodeType::Literal);
        }
        case Token::TemplateToken:
            if (isParse) {
                return T(this->templateLiteral<Parse>());
            }
            return this->templateLiteral<Scan>();

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead.valuePunctuatorKind;
            switch (value) {
            case LeftParenthesis:
                this->context->isBindingElement = false;
                if (isParse) {
                    return T(this->inheritCoverGrammar(&Parser::groupExpression<Parse>));
                }
                return this->scanInheritCoverGrammar(&Parser::groupExpression<Scan>);
            case LeftSquareBracket:
                if (isParse) {
                    return T(this->inheritCoverGrammar(&Parser::arrayInitializer<Parse>));
                }
                this->scanInheritCoverGrammar(&Parser::arrayInitializer<Scan>);
                return ScanExpressionResult(ASTNodeType::ArrayExpression);
            case LeftBrace:
                if (isParse) {
                    return T(this->inheritCoverGrammar(&Parser::objectInitializer<Parse>));
                }
                this->scanInheritCoverGrammar(&Parser::objectInitializer<Scan>);
                return ScanExpressionResult(ASTNodeType::ObjectExpression);
            case Divide:
            case DivideEqual: {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->scanner->index = this->startMarker.index;
                ALLOC_TOKEN(token);
                this->nextRegexToken(token);
                // raw = this->getTokenRaw(token);
                if (isParse) {
                    return T(this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags)));
                }
                this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags));
                return ScanExpressionResult(ASTNodeType::Literal);
            }
            default: {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                this->throwUnexpectedToken(token);
            }
            }
            break;
        }

        case Token::KeywordToken:
            if (!this->context->strict && this->context->allowYield && this->matchKeyword(YieldKeyword)) {
                if (isParse) {
                    return T(this->parseIdentifierName());
                }
                return this->scanIdentifierName();
            } else if (!this->context->strict && this->matchKeyword(LetKeyword)) {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                throwUnexpectedToken(token);
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(FunctionKeyword)) {
                    if (isParse) {
                        return T(this->parseFunctionExpression());
                    }
                    this->parseFunctionExpression();
                    return ScanExpressionResult(ASTNodeType::FunctionExpression);
                } else if (this->matchKeyword(ThisKeyword)) {
                    this->nextToken();
                    if (isParse) {
                        return T(this->finalize(node, new ThisExpressionNode()));
                    }
                    return ScanExpressionResult(ASTNodeType::ThisExpression);
                } else if (this->matchKeyword(SuperKeyword)) {
                    this->nextToken();
                    if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                        this->throwUnexpectedToken(&this->lookahead);
                    }

                    if (isParse) {
                        return T(this->finalize(node, new SuperExpressionNode(this->lookahead.valuePunctuatorKind == LeftParenthesis)));
                    }
                    return ScanExpressionResult(ASTNodeType::SuperExpression);
                } else if (this->matchKeyword(ClassKeyword)) {
                    if (isParse) {
                        return T(this->parseClassExpression());
                    }
                    this->scanClassExpression();
                    return ScanExpressionResult(ASTNodeType::ClassExpression);
                } else {
                    ALLOC_TOKEN(token);
                    this->nextToken(token);
                    this->throwUnexpectedToken(token);
                }
            }
            break;
        default: {
            ALLOC_TOKEN(token);
            this->nextToken(token);
            this->throwUnexpectedToken(token);
        }
        }

        ASSERT_NOT_REACHED();
        if (isParse) {
            return T(PassRefPtr<Node>());
        }
        return ScanExpressionResult();
    }

    void validateParam(ParseFormalParametersResult& options, const Scanner::ScannerResult* param, AtomicString name)
    {
        ASSERT(param != nullptr);
        if (this->context->strict) {
            if (this->scanner->isRestrictedWord(name)) {
                options.stricted = *param;
                options.message = Messages::StrictParamName;
            }
            if (std::find(options.paramSet.begin(), options.paramSet.end(), name) != options.paramSet.end()) {
                options.stricted = *param;
                options.message = Messages::StrictParamDupe;
            }
        } else if (!options.firstRestricted) {
            if (this->scanner->isRestrictedWord(name)) {
                options.firstRestricted = *param;
                options.message = Messages::StrictParamName;
            } else if (this->scanner->isStrictModeReservedWord(name)) {
                options.firstRestricted = *param;
                options.message = Messages::StrictReservedWord;
            } else if (std::find(options.paramSet.begin(), options.paramSet.end(), name) != options.paramSet.end()) {
                options.stricted = *param;
                options.message = Messages::StrictParamDupe;
            }
        }

        options.paramSet.push_back(name);
    }

    AtomicString createPatternName(size_t index)
    {
        ExecutionState stateForTest(this->escargotContext);
        return AtomicString(this->escargotContext, Value(index).toString(stateForTest));
    }

    PassRefPtr<RestElementNode> parseRestElement(ScannerResultVector& params)
    {
        MetaNode node = this->createNode();

        this->nextToken();
        if (this->match(LeftBrace)) {
            this->throwError(Messages::ObjectPatternAsRestParameter);
        }
        params.push_back(this->lookahead);

        RefPtr<IdentifierNode> param = this->parseVariableIdentifier();
        if (this->match(Equal)) {
            this->throwError(Messages::DefaultRestParameter);
        }
        if (!this->match(RightParenthesis)) {
            this->throwError(Messages::ParameterAfterRestParameter);
        }

        return this->finalize(node, new RestElementNode(param.get()));
    }

    ScanExpressionResult scanRestElement(ScannerResultVector& params)
    {
        this->nextToken();
        if (this->match(LeftBrace)) {
            this->throwError(Messages::ObjectPatternAsRestParameter);
        }
        params.push_back(this->lookahead);

        ScanExpressionResult param = this->scanVariableIdentifier();
        if (this->match(Equal)) {
            this->throwError(Messages::DefaultRestParameter);
        }
        if (!this->match(RightParenthesis)) {
            this->throwError(Messages::ParameterAfterRestParameter);
        }

        return ScanExpressionResult(ASTNodeType::Identifier, param.string());
    }

    PassRefPtr<RestElementNode> parseBindingRestElement(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();
        this->expect(PeriodPeriodPeriod);
        params.push_back(this->lookahead);
        RefPtr<IdentifierNode> arg = this->parseVariableIdentifier(kind);
        return this->finalize(node, new RestElementNode(arg.get()));
    }

    // ECMA-262 13.3.3 Destructuring Binding Patterns

    template <typename T, bool isParse>
    T arrayPattern(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        this->expect(LeftSquareBracket);
        ExpressionNodeVector elements;
        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
                elements.push_back(nullptr);
            } else {
                if (this->match(PeriodPeriodPeriod)) {
                    elements.push_back(this->parseBindingRestElement(params, kind, isExplicitVariableDeclaration));
                    break;
                } else {
                    elements.push_back(this->parsePatternWithDefault(params, kind, isExplicitVariableDeclaration));
                }
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        if (isParse) {
            return T(this->finalize(node, new ArrayPatternNode(std::move(elements))));
        }
        return ScanExpressionResult(ASTNodeType::ArrayPattern);
    }

    template <typename T, bool isParse>
    T propertyPattern(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        bool computed = false;
        bool shorthand = false;
        bool method = false;

        RefPtr<Node> keyNode; //'': Node.PropertyKey;
        RefPtr<Node> valueNode; //: Node.PropertyValue;
        ScanExpressionResult key;

        if (this->lookahead.type == Token::IdentifierToken) {
            ALLOC_TOKEN(keyToken);
            *keyToken = this->lookahead;

            if (isParse) {
                keyNode = this->parseVariableIdentifier();
            } else {
                key = this->scanVariableIdentifier();
            }

            if (this->match(Substitution)) {
                if (isParse) {
                    params.push_back(*keyToken);
                }
                shorthand = true;
                this->nextToken();

                if (isParse) {
                    RefPtr<Node> expr = this->assignmentExpression<Parse>();
                    valueNode = this->finalize(this->startNode(keyToken), new AssignmentPatternNode(keyNode.get(), expr.get()));
                } else {
                    this->assignmentExpression<Scan>();
                }
            } else if (!this->match(Colon)) {
                if (isParse) {
                    params.push_back(*keyToken);
                }
                shorthand = true;
                valueNode = keyNode;
                if (isExplicitVariableDeclaration) {
                    ASSERT(kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword);
                    addDeclaredNameIntoContext(valueNode->asIdentifier()->name(), this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
                }
            } else {
                this->expect(Colon);
                if (isParse) {
                    valueNode = this->parsePatternWithDefault(params, kind, isExplicitVariableDeclaration);
                } else {
                    this->parsePatternWithDefault(params, kind, isExplicitVariableDeclaration);
                }
            }
        } else {
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
            } else {
                this->scanObjectPropertyKey();
            }
            this->expect(Colon);
            if (isParse) {
                valueNode = this->parsePatternWithDefault(params, kind, isExplicitVariableDeclaration);
            } else {
                this->parsePatternWithDefault(params, kind, isExplicitVariableDeclaration);
            }
        }

        if (isParse) {
            return T(this->finalize(node, new PropertyNode(keyNode.get(), valueNode.get(), PropertyNode::Kind::Init, computed, shorthand)));
        }
        return ScanExpressionResult(ASTNodeType::ObjectPattern);
    }

    template <typename T, bool isParse>
    T objectPattern(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();
        PropertiesNodeVector properties;

        this->expect(LeftBrace);

        while (!this->match(RightBrace)) {
            if (isParse) {
                properties.push_back(this->propertyPattern<ParseAs(PropertyNode)>(params, kind, isExplicitVariableDeclaration));
            } else {
                this->propertyPattern<Scan>(params, kind, isExplicitVariableDeclaration);
            }

            if (!this->match(RightBrace)) {
                this->expect(Comma);
            }
        }

        this->expect(RightBrace);

        if (isParse) {
            return T(this->finalize(node, new ObjectPatternNode(std::move(properties))));
        }
        return ScanExpressionResult(ASTNodeType::ObjectPattern);
    }

    template <typename T, bool isParse>
    T pattern(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        if (this->match(LeftSquareBracket)) {
            if (isParse) {
                return T(this->arrayPattern<ParseAs(ArrayPatternNode)>(params, kind, isExplicitVariableDeclaration));
            } else {
                return this->arrayPattern<Scan>(params, kind, isExplicitVariableDeclaration);
            }

        } else if (this->match(LeftBrace)) {
            if (isParse) {
                return T(this->objectPattern<ParseAs(ObjectPatternNode)>(params, kind, isExplicitVariableDeclaration));
            } else {
                return this->objectPattern<Scan>(params, kind, isExplicitVariableDeclaration);
            }
        } else {
            if (this->matchKeyword(LetKeyword) && (kind == ConstKeyword || kind == LetKeyword)) {
                this->throwUnexpectedToken(&this->lookahead, Messages::UnexpectedToken);
            }
            params.push_back(this->lookahead);
            if (isParse) {
                return T(this->parseVariableIdentifier(kind, isExplicitVariableDeclaration));
            }

            return this->scanVariableIdentifier(kind, isExplicitVariableDeclaration);
        }
    }

    PassRefPtr<Node> parsePatternWithDefault(ScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        PassRefPtr<Node> pattern = this->pattern<Parse>(params, kind, isExplicitVariableDeclaration);
        if (this->match(PunctuatorKind::Substitution)) {
            this->nextToken();
            const bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = true;
            PassRefPtr<Node> right = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
            this->context->allowYield = previousAllowYield;

            return this->finalize(this->startNode(startToken), new AssignmentPatternNode(pattern.get(), right.get()));
        }

        return pattern;
    }

    bool parseFormalParameter(ParseFormalParametersResult& options)
    {
        RefPtr<Node> param;
        bool trackUsingNamesBefore = trackUsingNames;
        trackUsingNames = false;
        ScannerResultVector params;
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::PeriodPeriodPeriod) {
            param = this->parseRestElement(params);
            this->validateParam(options, &params.back(), ((RestElementNode*)param.get())->argument()->name());
            options.params.push_back(param);
            trackUsingNames = true;
            return false;
        }

        param = this->parsePatternWithDefault(params);
        for (size_t i = 0; i < params.size(); i++) {
            AtomicString as(this->escargotContext, params[i].relatedSource());
            this->validateParam(options, &params[i], as);
        }
        options.params.push_back(param);
        trackUsingNames = trackUsingNamesBefore;
        return !this->match(PunctuatorKind::RightParenthesis);
    }

    ParseFormalParametersResult parseFormalParameters(Scanner::ScannerResult* firstRestricted = nullptr)
    {
        ParseFormalParametersResult options;

        if (firstRestricted) {
            options.firstRestricted = *firstRestricted;
        }

        size_t oldSubCodeBlockIndex = this->subCodeBlockIndex;

        if (!this->match(RightParenthesis)) {
            options.paramSet.clear();
            while (this->startMarker.index < this->scanner->length) {
                if (!this->parseFormalParameter(options)) {
                    break;
                }
                this->expect(Comma);
            }
        }
        this->expect(RightParenthesis);

        this->subCodeBlockIndex = oldSubCodeBlockIndex;

        options.valid = true;
        return options;
    }

    // ECMA-262 12.2.5 Array Initializer

    template <typename T, bool isParse>
    T spreadElement()
    {
        MetaNode node = this->createNode();
        this->expect(PunctuatorKind::PeriodPeriodPeriod);

        RefPtr<Node> arg = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
        if (isParse) {
            return T(this->finalize(node, new SpreadElementNode(arg.get())));
        }
        return ScanExpressionResult(ASTNodeType::SpreadElement);
    }

    template <typename T, bool isParse>
    T arrayInitializer()
    {
        MetaNode node = this->createNode();
        ExpressionNodeVector elements;

        this->expect(LeftSquareBracket);

        bool hasSpreadElement = false;

        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
                if (isParse) {
                    elements.push_back(nullptr);
                }
            } else if (this->match(PeriodPeriodPeriod)) {
                if (isParse) {
                    elements.push_back(this->spreadElement<Parse>());
                } else {
                    this->spreadElement<Scan>();
                }

                hasSpreadElement = true;

                if (!this->match(RightSquareBracket)) {
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->expect(Comma);
                }
            } else {
                if (isParse) {
                    elements.push_back(this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>));
                } else {
                    this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        if (isParse) {
            return T(this->finalize(node, new ArrayExpressionNode(std::move(elements), AtomicString(), nullptr, hasSpreadElement)));
        }
        return ScanExpressionResult(ASTNodeType::ArrayExpression);
    }

    // ECMA-262 12.2.6 Object Initializer

    PassRefPtr<Node> parsePropertyMethod(ParseFormalParametersResult& params)
    {
        bool previousInArrowFunction = this->context->inArrowFunction;

        this->context->inArrowFunction = false;
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        const bool previousStrict = this->context->strict;
        PassRefPtr<Node> body = this->isolateCoverGrammar(&Parser::parseFunctionSourceElements);
        if (this->context->strict && params.firstRestricted) {
            this->throwUnexpectedToken(&params.firstRestricted, params.message);
        }
        if (this->context->strict && params.stricted) {
            this->throwUnexpectedToken(&params.stricted, params.message);
        }
        this->context->strict = previousStrict;
        this->context->inArrowFunction = previousInArrowFunction;

        return body;
    }

    PassRefPtr<FunctionExpressionNode> parsePropertyMethodFunction()
    {
        const bool isGenerator = false;
        const bool previousAllowYield = this->context->allowYield;
        this->context->allowYield = false;
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        pushScopeContext(AtomicString());
        ParseFormalParametersResult params = this->parseFormalParameters();
        extractNamesFromFunctionParams(params);
        RefPtr<Node> method = this->parsePropertyMethod(params);
        this->context->allowYield = previousAllowYield;

        scopeContexts.back()->m_paramsStartLOC.index = node.index;
        scopeContexts.back()->m_paramsStartLOC.column = node.column;
        scopeContexts.back()->m_paramsStartLOC.line = node.line;

        return this->finalize(node, new FunctionExpressionNode(popScopeContext(node), isGenerator, this->subCodeBlockIndex));
    }

    PassRefPtr<Node> parseObjectPropertyKey()
    {
        MetaNode node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);

        RefPtr<Node> key;
        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && token->octal) {
                this->throwUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead.startWithZero) {
                this->throwUnexpectedToken(&this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            // const raw = this->getTokenRaw(token);
            {
                Value v;
                if (token->type == Token::NumericLiteralToken) {
                    if (this->context->inLoop || token->valueNumber == 0)
                        this->scopeContexts.back()->insertNumeralLiteral(Value(token->valueNumber));
                    v = Value(token->valueNumber);
                } else {
                    v = Value(token->valueStringLiteralForAST());
                }
                key = this->finalize(node, new LiteralNode(v));
            }
            break;

        case Token::IdentifierToken:
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::KeywordToken: {
            bool trackUsingNamesBefore = this->trackUsingNames;
            this->trackUsingNames = false;
            key = this->finalize(node, finishIdentifier(token, false));
            this->trackUsingNames = trackUsingNamesBefore;
            break;
        }
        case Token::PunctuatorToken:
            if (token->valuePunctuatorKind == LeftSquareBracket) {
                key = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                this->expect(RightSquareBracket);
            } else {
                this->throwUnexpectedToken(token);
            }
            break;

        default:
            this->throwUnexpectedToken(token);
        }

        return key;
    }

    std::pair<ScanExpressionResult, String*> scanObjectPropertyKey()
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        ScanExpressionResult key;
        String* keyString = String::emptyString;

        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && token->octal) {
                this->throwUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead.startWithZero) {
                this->throwUnexpectedToken(&this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            // const raw = this->getTokenRaw(token);
            {
                if (token->type == Token::NumericLiteralToken) {
                    if (this->context->inLoop || token->valueNumber == 0)
                        this->scopeContexts.back()->insertNumeralLiteral(Value(token->valueNumber));
                } else {
                    keyString = token->valueStringLiteral();
                }
                key = ScanExpressionResult(ASTNodeType::Literal);
            }
            break;

        case Token::IdentifierToken:
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::KeywordToken: {
            bool trackUsingNamesBefore = this->trackUsingNames;
            this->trackUsingNames = false;
            key = finishScanIdentifier(token, false);
            keyString = key.string().string();
            this->trackUsingNames = trackUsingNamesBefore;
            break;
        }
        case Token::PunctuatorToken:
            if (token->valuePunctuatorKind == LeftSquareBracket) {
                key = this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                this->expect(RightSquareBracket);
            } else {
                this->throwUnexpectedToken(token);
            }
            break;

        default:
            this->throwUnexpectedToken(token);
        }

        return std::make_pair(key, keyString);
    }

    bool qualifiedPropertyName(Scanner::ScannerResult* token)
    {
        switch (token->type) {
        case Token::IdentifierToken:
        case Token::StringLiteralToken:
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::KeywordToken:
            return true;
        case Token::PunctuatorToken:
            return token->valuePunctuatorKind == LeftSquareBracket;
        default:
            return false;
        }
    }

    bool isPropertyKey(Node* key, const char* value)
    {
        if (key->type() == Identifier) {
            return ((IdentifierNode*)key)->name() == value;
        } else if (key->type() == Literal) {
            if (((LiteralNode*)key)->value().isString()) {
                return ((LiteralNode*)key)->value().asString()->equals(value);
            }
        }

        return false;
    }

    bool isPropertyKey(ScanExpressionResult result, String* name, const char* value)
    {
        if (result == Identifier) {
            return name->equals(value);
        } else if (result == Literal) {
            return name->equals(value);
        }

        return false;
    }

    template <typename T, bool isParse>
    T objectProperty(bool& hasProto, std::vector<std::pair<AtomicString, size_t>>& usedNames) //: Node.Property
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        MetaNode node = this->createNode();

        PropertyNode::Kind kind;
        RefPtr<Node> keyNode; //'': Node.PropertyKey;
        RefPtr<Node> valueNode; //: Node.PropertyValue;
        ScanExpressionResult key;

        bool computed = false;
        bool method = false;
        bool shorthand = false;
        bool isProto = false;

        if (token->type == Token::IdentifierToken) {
            this->nextToken();
            if (isParse) {
                keyNode = this->finalize(this->createNode(), finishIdentifier(token, true));
            } else {
                key = finishScanIdentifier(token, true);
            }
        } else if (this->match(PunctuatorKind::Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
            } else {
                auto keyValue = this->scanObjectPropertyKey();
                key = keyValue.first;
                isProto = keyValue.second->equals("__proto__");
            }
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(&this->lookahead);
        bool isGet = false;
        bool isSet = false;
        bool needImplictName = false;
        if (token->type == Token::IdentifierToken && lookaheadPropertyKey) {
            StringView* sv = token->valueStringLiteral();
            const auto& d = sv->bufferAccessData();
            if (d.length == 3) {
                if (d.equalsSameLength("get")) {
                    isGet = true;
                } else if (d.equalsSameLength("set")) {
                    isSet = true;
                }
            }
        }

        if (isGet) {
            kind = PropertyNode::Kind::Get;
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
                this->context->allowYield = false;
                valueNode = this->parseGetterMethod();
            } else {
                auto keyValue = this->scanObjectPropertyKey();
                key = keyValue.first;
                isProto = keyValue.second->equals("__proto__");
                this->context->allowYield = false;
                this->parseGetterMethod();
            }
        } else if (isSet) {
            kind = PropertyNode::Kind::Set;
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
                valueNode = this->parseSetterMethod();
            } else {
                auto keyValue = this->scanObjectPropertyKey();
                key = keyValue.first;
                isProto = keyValue.second->equals("__proto__");
                this->parseSetterMethod();
            }
        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::Multiply && lookaheadPropertyKey) {
            kind = PropertyNode::Kind::Init;
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
                valueNode = this->parseGeneratorMethod();
            } else {
                auto keyValue = this->scanObjectPropertyKey();
                key = keyValue.first;
                isProto = keyValue.second->equals("__proto__");
                this->parseGeneratorMethod();
            }
            method = true;
        } else {
            if (isParse) {
                if (!keyNode) {
                    this->throwUnexpectedToken(&this->lookahead);
                }
            } else {
                if (key == ASTNodeType::ASTNodeTypeError) {
                    this->throwUnexpectedToken(&this->lookahead);
                }
            }
            kind = PropertyNode::Kind::Init;
            if (this->match(PunctuatorKind::Colon)) {
                if (isParse) {
                    isProto = !this->config.parseSingleFunction && this->isPropertyKey(keyNode.get(), "__proto__");
                } else {
                    isProto |= (key == ASTNodeType::Identifier && key.string() == this->escargotContext->staticStrings().__proto__);
                }

                if (!computed && isProto) {
                    if (hasProto) {
                        this->throwError(Messages::DuplicateProtoProperty);
                    }
                    hasProto = true;
                }
                this->nextToken();
                ASTNodeType type = ASTNodeType::ASTNodeTypeError;
                if (isParse) {
                    valueNode = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
                    if (valueNode) {
                        type = valueNode->type();
                    }
                } else {
                    type = this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
                if ((type == ASTNodeType::FunctionExpression || type == ASTNodeType::ArrowFunctionExpression) && lastPoppedScopeContext->m_functionName == AtomicString()) {
                    needImplictName = true;
                }
            } else if (this->match(LeftParenthesis)) {
                if (isParse) {
                    valueNode = this->parsePropertyMethodFunction();
                } else {
                    this->parsePropertyMethodFunction();
                }
                method = true;
            } else if (token->type == Token::IdentifierToken) {
                RefPtr<Node> id;
                if (isParse) {
                    id = this->finalize(node, finishIdentifier(token, true));
                }
                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    if (isParse) {
                        RefPtr<Node> init = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                        valueNode = this->finalize(node, new AssignmentExpressionSimpleNode(id.get(), init.get()));
                    } else {
                        this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                    }
                } else {
                    shorthand = true;
                    if (isParse) {
                        valueNode = id;
                    }
                }
            } else {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                this->throwUnexpectedToken(token);
            }
        }

        if (!this->config.parseSingleFunction && (isParse ? keyNode->isIdentifier() : key == ASTNodeType::Identifier)) {
            AtomicString as = isParse ? keyNode->asIdentifier()->name() : key.string();
            bool seenInit = kind == PropertyNode::Kind::Init;
            bool seenGet = kind == PropertyNode::Kind::Get;
            bool seenSet = kind == PropertyNode::Kind::Set;
            size_t len = usedNames.size();

            for (size_t i = 0; i < len; i++) {
                const auto& n = usedNames[i];
                if (n.first == as) {
                    if (n.second == PropertyNode::Kind::Init) {
                        if (this->context->strict) {
                            if (seenInit || seenGet || seenSet) {
                                this->throwError("invalid object literal");
                            }
                        } else {
                            if (seenGet || seenSet) {
                                this->throwError("invalid object literal");
                            }
                        }
                        seenInit = true;
                    } else if (n.second == PropertyNode::Kind::Get) {
                        if (seenInit || seenGet) {
                            this->throwError("invalid object literal");
                        }
                        seenGet = true;
                    } else if (n.second == PropertyNode::Kind::Set) {
                        if (seenInit || seenSet) {
                            this->throwError("invalid object literal");
                        }
                        seenSet = true;
                    }
                }
            }
            usedNames.push_back(std::make_pair(as, kind));
        }

        if (!this->config.parseSingleFunction && (method || isGet || isSet || needImplictName)) {
            AtomicString as;
            if (isParse ? keyNode->isIdentifier() : key == ASTNodeType::Identifier) {
                as = isParse ? keyNode->asIdentifier()->name() : key.string();
            }
            lastPoppedScopeContext->m_functionName = as;
            if (needImplictName) {
                lastPoppedScopeContext->m_hasImplictFunctionName = true;
            } else {
                lastPoppedScopeContext->m_isClassMethod = true;
            }
        }
        if (isParse) {
            return T(this->finalize(node, new PropertyNode(keyNode.get(), valueNode.get(), kind, computed, shorthand)));
        }
        return ScanExpressionResult(ASTNodeType::Property);
    }

    template <typename T, bool isParse>
    T objectInitializer()
    {
        this->expect(LeftBrace);
        MetaNode node = this->createNode();
        PropertiesNodeVector properties;
        bool hasProto = false;
        std::vector<std::pair<AtomicString, size_t>> usedNames;
        while (!this->match(RightBrace)) {
            if (isParse) {
                properties.push_back(this->objectProperty<ParseAs(PropertyNode)>(hasProto, usedNames));
            } else {
                this->objectProperty<Scan>(hasProto, usedNames);
            }
            if (!this->match(RightBrace)) {
                this->expectCommaSeparator();
            }
        }
        this->expect(RightBrace);

        if (isParse) {
            return T(this->finalize(node, new ObjectExpressionNode(std::move(properties))));
        }
        return ScanExpressionResult(ASTNodeType::ObjectExpression);
    }

    // ECMA-262 12.2.9 Template Literals
    TemplateElement* parseTemplateHead()
    {
        ASSERT(this->lookahead.type == Token::TemplateToken);

        ALLOC_TOKEN(token);
        this->nextToken(token);
        MetaNode node = this->createNode();
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->valueRaw = token->valueTemplate->valueRaw;
        tm->tail = token->valueTemplate->tail;
        return tm;
    }

    bool scanTemplateHead()
    {
        ASSERT(this->lookahead.type == Token::TemplateToken);

        ALLOC_TOKEN(token);
        this->nextToken(token);

        return token->valueTemplate->tail;
    }

    TemplateElement* parseTemplateElement()
    {
        if (!this->match(PunctuatorKind::RightBrace)) {
            this->throwUnexpectedToken(&this->lookahead);
        }

        // Re-scan the current token (right brace) as a template string.
        this->scanner->scanTemplate(&this->lookahead);

        ALLOC_TOKEN(token);
        this->nextToken(token);
        MetaNode node = this->createNode();
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->valueRaw = token->valueTemplate->valueRaw;
        tm->tail = token->valueTemplate->tail;
        return tm;
    }

    bool scanTemplateElement()
    {
        if (!this->match(PunctuatorKind::RightBrace)) {
            this->throwUnexpectedToken(&this->lookahead);
        }

        // Re-scan the current token (right brace) as a template string.
        this->scanner->scanTemplate(&this->lookahead);

        ALLOC_TOKEN(token);
        this->nextToken(token);
        return token->valueTemplate->tail;
    }

    template <typename T, bool isParse>
    T templateLiteral()
    {
        MetaNode node = this->createNode();

        if (isParse) {
            ExpressionNodeVector expressions;
            TemplateElementVector* quasis = new (GC) TemplateElementVector;
            quasis->push_back(this->parseTemplateHead());
            while (!quasis->back()->tail) {
                expressions.push_back(this->expression<Parse>());
                TemplateElement* quasi = this->parseTemplateElement();
                quasis->push_back(quasi);
            }
            return T(this->finalize(node, new TemplateLiteralNode(quasis, std::move(expressions))));
        } else {
            bool isTail = this->scanTemplateHead();
            while (!isTail) {
                this->expression<Scan>();
                isTail = this->scanTemplateElement();
            }

            return ScanExpressionResult(ASTNodeType::TemplateLiteral);
        }
    }


    // FIXME
    void reinterpretExpressionAsPattern(Node* expr, ASTNodeType parent = ASTNodeTypeError)
    {
        switch (expr->type()) {
        case ArrayExpression: {
            ArrayExpressionNode* array = expr->asArrayExpression();
            ExpressionNodeVector& elements = array->elements();
            for (size_t i = 0; i < elements.size(); i++) {
                if (elements[i] != nullptr) {
                    this->reinterpretExpressionAsPattern(elements[i].get(), ArrayPattern);
                }
            }
            break;
        }
        case ObjectExpression: {
            ObjectExpressionNode* object = expr->asObjectExpression();
            PropertiesNodeVector& properties = object->properties();
            for (size_t i = 0; i < properties.size(); i++) {
                if (properties[i] != nullptr) {
                    this->reinterpretExpressionAsPattern(properties[i].get(), ObjectPattern);
                }
            }
            break;
        }
        case AssignmentExpressionSimple: {
            AssignmentExpressionSimpleNode* assign = expr->asAssignmentExpressionSimple();
            this->reinterpretExpressionAsPattern(assign->left());
            break;
        }
        default:
            if (!expr->isValidAssignmentTarget()) {
                if (parent == ObjectPattern && expr->type() != Property) {
                    this->throwError("Invalid destructuring assignment target");
                }
            }
            break;
        }
    }

    void scanReinterpretExpressionAsPattern(ScanExpressionResult expr)
    {
        switch (expr) {
        case ArrayExpression:
            expr.setNodeType(ASTNodeType::ArrayPattern);
            break;
        case ObjectExpression:
            expr.setNodeType(ASTNodeType::ObjectPattern);
            break;
        default:
            break;
        }
    }

    template <typename T, bool isParse>
    T groupExpression()
    {
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->nextToken();
            if (!this->match(Arrow)) {
                this->expect(Arrow);
            }

            if (isParse) {
                exprNode = adoptRef(new ArrowParameterPlaceHolderNode());
            } else {
                expr.setNodeType(ASTNodeType::ArrowParameterPlaceHolder);
            }
        } else {
            ALLOC_TOKEN(startToken);
            *startToken = this->lookahead;
            ScannerResultVector params;
            if (this->match(PeriodPeriodPeriod)) {
                if (isParse) {
                    exprNode = this->parseRestElement(params);
                } else {
                    expr = this->scanRestElement(params);
                }
                this->expect(RightParenthesis);
                if (!this->match(Arrow)) {
                    this->expect(Arrow);
                }
                if (isParse) {
                    exprNode = adoptRef(new ArrowParameterPlaceHolderNode(exprNode.get()));
                } else {
                    expr.setNodeType(ASTNodeType::ArrowParameterPlaceHolder);
                }
            } else {
                bool arrow = false;
                this->context->isBindingElement = true;
                if (isParse) {
                    exprNode = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
                } else {
                    expr = this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                }

                if (this->match(Comma)) {
                    ExpressionNodeVector expressions;

                    this->context->isAssignmentTarget = false;
                    if (isParse) {
                        expressions.push_back(exprNode);
                    }
                    while (this->startMarker.index < this->scanner->length) {
                        if (!this->match(Comma)) {
                            break;
                        }
                        this->nextToken();

                        if (this->match(PeriodPeriodPeriod)) {
                            if (!this->context->isBindingElement) {
                                this->throwUnexpectedToken(&this->lookahead);
                            }
                            if (isParse) {
                                expressions.push_back(this->parseRestElement(params));
                            } else {
                                this->scanRestElement(params);
                            }
                            this->expect(RightParenthesis);
                            if (!this->match(Arrow)) {
                                this->expect(Arrow);
                            }
                            this->context->isBindingElement = false;
                            for (size_t i = 0; i < expressions.size(); i++) {
                                this->reinterpretExpressionAsPattern(expressions[i].get());
                            }
                            arrow = true;
                            if (isParse) {
                                exprNode = adoptRef(new ArrowParameterPlaceHolderNode(std::move(expressions)));
                            } else {
                                expr.setNodeType(ASTNodeType::ArrowParameterPlaceHolder);
                            }
                        } else {
                            if (isParse) {
                                expressions.push_back(this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>));
                            } else {
                                this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                            }
                        }
                        if (arrow) {
                            break;
                        }
                    }
                    if (!arrow) {
                        if (isParse) {
                            exprNode = this->finalize(this->startNode(startToken), new SequenceExpressionNode(std::move(expressions)));
                        } else {
                            expr.setNodeType(ASTNodeType::SequenceExpression);
                        }
                    }
                }

                if (!arrow) {
                    this->expect(RightParenthesis);
                    if (this->match(Arrow)) {
                        if (isParse) {
                            if (exprNode->type() == Identifier && exprNode->asIdentifier()->name() == "yield") {
                                arrow = true;
                                exprNode = adoptRef(new ArrowParameterPlaceHolderNode(exprNode.get()));
                            }
                        } else {
                            if (expr == ASTNodeType::Identifier && expr.string() == "yield") {
                                arrow = true;
                                expr.setNodeType(ASTNodeType::ArrowParameterPlaceHolder);
                            }
                        }
                        if (!arrow) {
                            if (!this->context->isBindingElement) {
                                this->throwUnexpectedToken(&this->lookahead);
                            }

                            if (isParse) {
                                if (exprNode->type() == SequenceExpression) {
                                    const ExpressionNodeVector& expressions = ((SequenceExpressionNode*)exprNode.get())->expressions();
                                    for (size_t i = 0; i < expressions.size(); i++) {
                                        this->reinterpretExpressionAsPattern(expressions[i].get());
                                    }
                                } else {
                                    this->reinterpretExpressionAsPattern(exprNode.get());
                                }
                            } else {
                                if (expr != ASTNodeType::SequenceExpression) {
                                    this->scanReinterpretExpressionAsPattern(expr);
                                }
                            }

                            if (isParse) {
                                ExpressionNodeVector params;
                                if (exprNode->type() == SequenceExpression) {
                                    params = ((SequenceExpressionNode*)exprNode.get())->expressions();
                                } else {
                                    params.push_back(exprNode);
                                }

                                exprNode = adoptRef(new ArrowParameterPlaceHolderNode(std::move(params)));
                            } else {
                                expr.setNodeType(ASTNodeType::ArrowParameterPlaceHolder);
                            }
                        }
                    }
                    this->context->isBindingElement = false;
                }
            }
        }

        if (isParse) {
            return T(exprNode.release());
        }
        return expr;
    }

    // ECMA-262 12.3 Left-Hand-Side Expressions

    ArgumentVector parseArguments()
    {
        this->expect(LeftParenthesis);
        ArgumentVector args;
        if (!this->match(RightParenthesis)) {
            while (true) {
                RefPtr<Node> expr;
                if (this->match(PeriodPeriodPeriod)) {
                    expr = this->spreadElement<Parse>();
                } else {
                    expr = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                }
                args.push_back(expr);
                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expectCommaSeparator();
            }
        }
        this->expect(RightParenthesis);
        if (UNLIKELY(args.size() > 65535)) {
            this->throwError("too many arguments in call");
        }

        return args;
    }

    void scanArguments()
    {
        this->expect(LeftParenthesis);
        ArgumentVector args;
        size_t count = 0;
        if (!this->match(RightParenthesis)) {
            while (true) {
                if (this->match(PeriodPeriodPeriod)) {
                    this->spreadElement<Parse>();
                } else {
                    this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
                count++;
                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expectCommaSeparator();
            }
        }
        this->expect(RightParenthesis);
        if (UNLIKELY(count > 65535)) {
            this->throwError("too many arguments in call");
        }
    }

    bool isIdentifierName(Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        return token->type == Token::IdentifierToken || token->type == Token::KeywordToken || token->type == Token::BooleanLiteralToken || token->type == Token::NullLiteralToken;
    }

    PassRefPtr<IdentifierNode> parseIdentifierName()
    {
        MetaNode node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(token);
        }
        return this->finalize(node, finishIdentifier(token, true));
    }

    ScanExpressionResult scanIdentifierName()
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(token);
        }

        return finishScanIdentifier(token, false);
    }

    template <typename T, bool isParse>
    T newExpression()
    {
        this->nextToken();

        if (this->match(Period)) {
            this->nextToken();
            if (this->lookahead.type == Token::IdentifierToken && this->context->inFunctionBody && this->lookahead.relatedSource() == "target") {
                this->nextToken();
                scopeContexts.back()->m_hasSuperOrNewTarget = true;
                if (isParse) {
                    MetaNode node = this->createNode();
                    Node* expr = new MetaPropertyNode();
                    return T(this->finalize(node, expr));
                } else {
                    return ScanExpressionResult(ASTNodeType::MetaProperty);
                }
            } else {
                this->throwUnexpectedToken(&this->lookahead);
            }
        }

        MetaNode node = this->createNode();
        if (isParse) {
            RefPtr<Node> callee = this->isolateCoverGrammar(&Parser::leftHandSideExpression<Parse>);
            ArgumentVector args;
            if (this->match(LeftParenthesis)) {
                args = this->parseArguments();
            }
            Node* expr = new NewExpressionNode(callee.get(), std::move(args));
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            return T(this->finalize(node, expr));
        }

        ScanExpressionResult callee = this->scanIsolateCoverGrammar(&Parser::leftHandSideExpression<Scan>);
        if (this->match(LeftParenthesis)) {
            this->scanArguments();
        }
        ScanExpressionResult expr(ASTNodeType::NewExpression);
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;
        return expr;
    }

    template <typename T, bool isParse>
    T leftHandSideExpressionAllowCall()
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (this->context->inFunctionBody && this->matchKeyword(SuperKeyword)) {
            this->nextToken();
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(&this->lookahead);
            }

            scopeContexts.back()->m_hasSuperOrNewTarget = true;

            if (isParse) {
                exprNode = this->finalize(this->createNode(), new SuperExpressionNode(this->lookahead.valuePunctuatorKind == LeftParenthesis));
            } else {
                expr = ScanExpressionResult(ASTNodeType::SuperExpression);
            }
        } else if (this->matchKeyword(NewKeyword)) {
            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::newExpression<Parse>);
            } else {
                expr = this->scanInheritCoverGrammar(&Parser::newExpression<Scan>);
            }
        } else {
            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::primaryExpression<Parse>);
            } else {
                expr = this->scanInheritCoverGrammar(&Parser::primaryExpression<Scan>);
            }
        }

        while (true) {
            bool isPunctuatorTokenLookahead = this->lookahead.type == Token::PunctuatorToken;
            if (isPunctuatorTokenLookahead) {
                if (this->lookahead.valuePunctuatorKind == Period) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    bool trackUsingNamesBefore = this->trackUsingNames;
                    this->trackUsingNames = false;
                    if (isParse) {
                        PassRefPtr<IdentifierNode> property = this->parseIdentifierName();
                        exprNode = this->finalize(this->startNode(startToken), new MemberExpressionNode(exprNode.get(), property.get(), true));
                    } else {
                        this->scanIdentifierName();
                        expr.setNodeType(ASTNodeType::MemberExpression);
                    }
                    this->trackUsingNames = trackUsingNamesBefore;
                } else if (this->lookahead.valuePunctuatorKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    if (isParse) {
                        exprNode = this->finalize(this->startNode(startToken), new CallExpressionNode(exprNode.get(), this->parseArguments()));
                    } else {
                        testCalleeExpressionInScan(expr);
                        this->scanArguments();
                        expr.setNodeType(ASTNodeType::CallExpression);
                    }
                } else if (this->lookahead.valuePunctuatorKind == LeftSquareBracket) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    if (isParse) {
                        RefPtr<Node> property = this->isolateCoverGrammar(&Parser::expression<Parse>);
                        exprNode = this->finalize(this->startNode(startToken), new MemberExpressionNode(exprNode.get(), property.get(), false));
                    } else {
                        this->scanIsolateCoverGrammar(&Parser::expression<Scan>);
                        expr.setNodeType(ASTNodeType::MemberExpression);
                    }
                    this->expect(RightSquareBracket);
                } else {
                    break;
                }
            } else if (this->lookahead.type == Token::TemplateToken && this->lookahead.valueTemplate->head) {
                if (isParse) {
                    RefPtr<Node> quasi = this->templateLiteral<Parse>();
                    exprNode = this->convertTaggedTempleateExpressionToCallExpression(this->startNode(startToken), this->finalize(this->startNode(startToken), new TaggedTemplateExpressionNode(exprNode.get(), quasi.get())));
                } else {
                    expr = this->templateLiteral<Scan>();
                }
            } else {
                break;
            }
        }
        this->context->allowIn = previousAllowIn;

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    void testCalleeExpressionInScan(ScanExpressionResult callee)
    {
        if (callee == ASTNodeType::Identifier && callee.string() == escargotContext->staticStrings().eval) {
            scopeContexts.back()->m_hasEval = true;
        }
    }

    template <typename T, bool isParse>
    T super()
    {
        MetaNode node = this->createNode();

        this->expectKeyword(SuperKeyword);
        if (!this->match(LeftSquareBracket) && !this->match(Period)) {
            this->throwUnexpectedToken(&this->lookahead);
        }

        scopeContexts.back()->m_hasSuperOrNewTarget = true;

        if (isParse) {
            return T(this->finalize(node, new SuperExpressionNode(false)));
        }
        return ScanExpressionResult(ASTNodeType::SuperExpression);
    }

    template <typename T, bool isParse>
    T leftHandSideExpression()
    {
        // assert(this->context->allowIn, 'callee of new expression always allow in keyword.');
        ASSERT(this->context->allowIn);

        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (this->matchKeyword(SuperKeyword) && this->context->inFunctionBody) {
            if (isParse) {
                exprNode = this->super<Parse>();
            } else {
                this->super<Scan>();
            }
        } else if (this->matchKeyword(NewKeyword)) {
            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::newExpression<Parse>);
            } else {
                this->scanInheritCoverGrammar(&Parser::newExpression<Scan>);
            }
        } else {
            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::primaryExpression<Parse>);
            } else {
                this->scanInheritCoverGrammar(&Parser::primaryExpression<Scan>);
            }
        }

        MetaNode node = this->startNode(&this->lookahead);

        while (true) {
            if (this->match(LeftSquareBracket)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(LeftSquareBracket);
                if (isParse) {
                    RefPtr<Node> property = this->isolateCoverGrammar(&Parser::expression<Parse>);
                    exprNode = this->finalize(node, new MemberExpressionNode(exprNode.get(), property.get(), false));
                } else {
                    this->scanIsolateCoverGrammar(&Parser::expression<Scan>);
                    expr.setNodeType(ASTNodeType::MemberExpression);
                }
                this->expect(RightSquareBracket);
            } else if (this->match(Period)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(Period);
                bool trackUsingNamesBefore = this->trackUsingNames;
                this->trackUsingNames = false;
                if (isParse) {
                    RefPtr<IdentifierNode> property = this->parseIdentifierName();
                    exprNode = this->finalize(node, new MemberExpressionNode(exprNode.get(), property.get(), true));
                } else {
                    this->scanIdentifierName();
                    expr.setNodeType(ASTNodeType::MemberExpression);
                }
                this->trackUsingNames = trackUsingNamesBefore;
            } else if (this->lookahead.type == Token::TemplateToken && this->lookahead.valueTemplate->head) {
                if (isParse) {
                    RefPtr<Node> quasi = this->templateLiteral<Parse>();
                    exprNode = this->convertTaggedTempleateExpressionToCallExpression(node, this->finalize(node, new TaggedTemplateExpressionNode(exprNode.get(), quasi.get())));
                } else {
                    expr = this->templateLiteral<Scan>();
                }
            } else {
                break;
            }
        }

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    PassRefPtr<Node> convertTaggedTempleateExpressionToCallExpression(MetaNode node, RefPtr<TaggedTemplateExpressionNode> taggedTemplateExpression)
    {
        TemplateLiteralNode* templateLiteral = (TemplateLiteralNode*)taggedTemplateExpression->quasi();
        ArgumentVector args;
        ExpressionNodeVector elements;
        for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
            UTF16StringData& sd = (*templateLiteral->quasis())[i]->value;
            String* str = new UTF16String(std::move(sd));
            elements.push_back(this->finalize(node, new LiteralNode(Value(str))));
        }

        RefPtr<ArrayExpressionNode> arrayExpressionForRaw;
        {
            ExpressionNodeVector elements;
            for (size_t i = 0; i < templateLiteral->quasis()->size(); i++) {
                UTF16StringData& sd = (*templateLiteral->quasis())[i]->valueRaw;
                String* str = new UTF16String(std::move(sd));
                elements.push_back(this->finalize(node, new LiteralNode(Value(str))));
            }
            arrayExpressionForRaw = this->finalize(node, new ArrayExpressionNode(std::move(elements)));
        }

        RefPtr<ArrayExpressionNode> quasiVector = this->finalize(node, new ArrayExpressionNode(std::move(elements), this->escargotContext->staticStrings().raw, arrayExpressionForRaw.get()));
        args.push_back(quasiVector.get());
        for (size_t i = 0; i < templateLiteral->expressions().size(); i++) {
            args.push_back(templateLiteral->expressions()[i]);
        }
        return this->finalize(node, new CallExpressionNode(taggedTemplateExpression->expr(), std::move(args)));
    }


    // ECMA-262 12.4 Update Expressions

    template <typename T, bool isParse>
    T updateExpression()
    {
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            ALLOC_TOKEN(token);
            this->nextToken(token);

            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                if (exprNode->isLiteral() || exprNode->type() == ASTNodeType::ThisExpression) {
                    this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                }
                if (this->context->strict && exprNode->type() == Identifier && this->scanner->isRestrictedWord(((IdentifierNode*)exprNode.get())->name())) {
                    this->throwError(Messages::StrictLHSPrefix);
                }
            } else {
                expr = this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                if (expr == ASTNodeType::Literal || expr == ASTNodeType::ThisExpression) {
                    this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                }
                if (this->context->strict && expr == ASTNodeType::Identifier && this->scanner->isRestrictedWord(expr.string())) {
                    this->throwError(Messages::StrictLHSPrefix);
                }
            }

            if (!this->context->isAssignmentTarget && this->context->strict) {
                this->throwError(Messages::InvalidLHSInAssignment);
            }

            if (isParse) {
                MetaNode node = this->startNode(startToken);
                if (isPlus) {
                    exprNode = this->finalize(node, new UpdateExpressionIncrementPrefixNode(exprNode.get()));
                } else {
                    exprNode = this->finalize(node, new UpdateExpressionDecrementPrefixNode(exprNode.get()));
                }
            } else {
                if (isPlus) {
                    expr = ScanExpressionResult(ASTNodeType::UpdateExpressionIncrementPrefix);
                } else {
                    expr = ScanExpressionResult(ASTNodeType::UpdateExpressionDecrementPrefix);
                }
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        } else {
            if (isParse) {
                exprNode = this->inheritCoverGrammar(&Parser::leftHandSideExpressionAllowCall<Parse>);
            } else {
                expr = this->scanInheritCoverGrammar(&Parser::leftHandSideExpressionAllowCall<Scan>);
            }
            if (!this->hasLineTerminator && this->lookahead.type == Token::PunctuatorToken && (this->match(PlusPlus) || this->match(MinusMinus))) {
                bool isPlus = this->match(PlusPlus);
                if (isParse && this->context->strict && exprNode->isIdentifier() && this->scanner->isRestrictedWord(((IdentifierNode*)exprNode.get())->name())) {
                    this->throwError(Messages::StrictLHSPostfix);
                }
                if (!isParse && this->context->strict && expr == ASTNodeType::Identifier && this->scanner->isRestrictedWord(expr.string())) {
                    this->throwError(Messages::StrictLHSPostfix);
                }
                if (!this->context->isAssignmentTarget && this->context->strict) {
                    this->throwError(Messages::InvalidLHSInAssignment);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->nextToken();

                if (isParse) {
                    if (exprNode->isLiteral() || exprNode->type() == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (isPlus) {
                        exprNode = this->finalize(this->startNode(startToken), new UpdateExpressionIncrementPostfixNode(exprNode.get()));
                    } else {
                        exprNode = this->finalize(this->startNode(startToken), new UpdateExpressionDecrementPostfixNode(exprNode.get()));
                    }
                } else {
                    if (expr == ASTNodeType::Literal || expr == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (isPlus) {
                        expr = ScanExpressionResult(ASTNodeType::UpdateExpressionIncrementPostfix);
                    } else {
                        expr = ScanExpressionResult(ASTNodeType::UpdateExpressionDecrementPostfix);
                    }
                }
            }
        }

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    // ECMA-262 12.5 Unary Operators

    template <typename T, bool isParse>
    T unaryExpression()
    {
        if (this->lookahead.type == Token::PunctuatorToken) {
            auto punctuatorsKind = this->lookahead.valuePunctuatorKind;
            RefPtr<Node> exprNode;

            if (punctuatorsKind == Plus) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionPlusNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionPlus);
            } else if (punctuatorsKind == Minus) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionMinusNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionMinus);
            } else if (punctuatorsKind == Wave) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionBitwiseNotNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionBitwiseNot);
            } else if (punctuatorsKind == ExclamationMark) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionLogicalNotNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionLogicalNot);
            }
        }

        bool isKeyword = this->lookahead.type == Token::KeywordToken;

        if (isKeyword) {
            RefPtr<Node> exprNode;

            if (this->lookahead.valueKeywordKind == DeleteKeyword) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);

                    if (this->context->strict && subExpr->isIdentifier()) {
                        this->throwError(Messages::StrictDelete);
                    }
                    if (subExpr->isIdentifier()) {
                        this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                    }

                    exprNode = this->finalize(node, new UnaryExpressionDeleteNode(subExpr.get()));
                } else {
                    ScanExpressionResult subExpr = this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);

                    if (this->context->strict && subExpr == ASTNodeType::Identifier) {
                        this->throwError(Messages::StrictDelete);
                    }
                    if (subExpr == ASTNodeType::Identifier) {
                        this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                    }
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionDelete);
            } else if (this->lookahead.valueKeywordKind == VoidKeyword) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionVoidNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionVoid);
            } else if (this->lookahead.valueKeywordKind == TypeofKeyword) {
                this->nextToken();

                if (isParse) {
                    MetaNode node = this->startNode(&this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
                    exprNode = this->finalize(node, new UnaryExpressionTypeOfNode(subExpr.get()));
                } else {
                    this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
                }
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (isParse) {
                    return exprNode.release();
                }
                return ScanExpressionResult(ASTNodeType::UnaryExpressionTypeOf);
            }
        }

        return this->updateExpression<T, isParse>();
    }

    PassRefPtr<Node> parseExponentiationExpression()
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);
        // TODO
        /*
         if (expr->type != Syntax.UnaryExpression && this->match('**')) {
             this->nextToken();
             this->context->isAssignmentTarget = false;
             this->context->isBindingElement = false;
             const left = expr;
             const right = this->isolateCoverGrammar(this->parseExponentiationExpression);
             expr = this->finalize(this->startNode(startToken), new Node.BinaryExpression('**', left, right));
         }*/

        return expr;
    }

    ScanExpressionResult scanExponentiationExpression()
    {
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);
        // TODO
        /*
         if (expr->type != Syntax.UnaryExpression && this->match('**')) {
             this->nextToken();
             this->context->isAssignmentTarget = false;
             this->context->isBindingElement = false;
             const left = expr;
             const right = this->isolateCoverGrammar(this->parseExponentiationExpression);
             expr = this->finalize(this->startNode(startToken), new Node.BinaryExpression('**', left, right));
         }*/

        return expr;
    }

    // ECMA-262 12.6 Exponentiation Operators
    // ECMA-262 12.7 Multiplicative Operators
    // ECMA-262 12.8 Additive Operators
    // ECMA-262 12.9 Bitwise Shift Operators
    // ECMA-262 12.10 Relational Operators
    // ECMA-262 12.11 Equality Operators
    // ECMA-262 12.12 Binary Bitwise Operators
    // ECMA-262 12.13 Binary Logical Operators

    int binaryPrecedence(const Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        if (LIKELY(token->type == Token::PunctuatorToken)) {
            switch (token->valuePunctuatorKind) {
            case Substitution:
                return 0;
            case LogicalOr:
                return 1;
            case LogicalAnd:
                return 2;
            case BitwiseOr:
                return 3;
            case BitwiseXor:
                return 4;
            case BitwiseAnd:
                return 5;
            case Equal:
                return 6;
            case NotEqual:
                return 6;
            case StrictEqual:
                return 6;
            case NotStrictEqual:
                return 6;
            case RightInequality:
                return 7;
            case LeftInequality:
                return 7;
            case RightInequalityEqual:
                return 7;
            case LeftInequalityEqual:
                return 7;
            case LeftShift:
                return 8;
            case RightShift:
                return 8;
            case UnsignedRightShift:
                return 8;
            case Plus:
                return 9;
            case Minus:
                return 9;
            case Multiply:
                return 11;
            case Divide:
                return 11;
            case Mod:
                return 11;
            default:
                return 0;
            }
        } else if (token->type == Token::KeywordToken) {
            if (token->valueKeywordKind == InKeyword) {
                return this->context->allowIn ? 7 : 0;
            } else if (token->valueKeywordKind == InstanceofKeyword) {
                return 7;
            }
        } else {
            return 0;
        }
        return 0;
    }

    PassRefPtr<Node> parseBinaryExpression()
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::parseExponentiationExpression);

        ALLOC_TOKEN(token);
        *token = this->lookahead;
        //std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_allocator<RefPtr<Scanner::ScannerResult>>> tokenKeeper;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            Vector<Scanner::ScannerResult, GCUtil::gc_malloc_allocator<Scanner::ScannerResult>> markers;
            markers.push_back(*startToken);
            markers.push_back(this->lookahead);
            RefPtr<Node> left = expr;
            RefPtr<Node> right = this->isolateCoverGrammar(&Parser::parseExponentiationExpression);

            //std::vector<void*, GCUtil::gc_malloc_allocator<void*>> stack;
            NodeVector stack;
            Vector<Scanner::ScannerResult, GCUtil::gc_malloc_allocator<Scanner::ScannerResult>> tokenStack;

            stack.push_back(left);
            stack.push_back(right);
            tokenStack.push_back(*token);

            while (true) {
                prec = this->binaryPrecedence(&this->lookahead);
                if (prec <= 0) {
                    break;
                }

                // Reduce: make a binary expression from the three topmost entries.
                while ((stack.size() > 1) && (prec <= tokenStack.back().prec)) {
                    right = stack.back();
                    stack.pop_back();
                    Scanner::ScannerResult operator_ = tokenStack.back();
                    tokenStack.pop_back();
                    left = stack.back();
                    stack.pop_back();
                    markers.pop_back();
                    MetaNode node = this->startNode(&markers.back());
                    auto e = this->finalize(node, finishBinaryExpression(left.get(), right.get(), &operator_));
                    stack.push_back(e);
                }

                // Shift.
                this->nextToken(token);
                token->prec = prec;
                tokenStack.push_back(*token);
                markers.push_back(this->lookahead);
                auto e = this->isolateCoverGrammar(&Parser::parseExponentiationExpression);
                stack.push_back(e);
            }

            // Final reduce to clean-up the stack.
            size_t i = stack.size() - 1;
            expr = stack[i];
            markers.pop_back();
            while (i > 0) {
                MetaNode node = this->startNode(&markers.back());
                expr = this->finalize(node, finishBinaryExpression(stack[i - 1].get(), expr.get(), &tokenStack.back()));
                markers.pop_back();
                tokenStack.pop_back();
                i--;
            }
            RELEASE_ASSERT(i == 0);
        }

        return expr.release();
    }

    ScanExpressionResult scanBinaryExpressions()
    {
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::scanExponentiationExpression);

        ALLOC_TOKEN(token);
        *token = this->lookahead;
        //std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_allocator<RefPtr<Scanner::ScannerResult>>> tokenKeeper;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            ScanExpressionResult left = expr;
            ScanExpressionResult right = this->scanIsolateCoverGrammar(&Parser::scanExponentiationExpression);

            std::vector<void*, GCUtil::gc_malloc_allocator<void*>> stack;
            stack.push_back(new ScanExpressionResult(left));
            stack.push_back(token);
            stack.push_back(new ScanExpressionResult(right));

            while (true) {
                prec = this->binaryPrecedence(&this->lookahead);
                if (prec <= 0) {
                    break;
                }

                // Reduce: make a binary expression from the three topmost entries.
                while ((stack.size() > 2) && (prec <= ((Scanner::ScannerResult*)stack[stack.size() - 2])->prec)) {
                    right = *((ScanExpressionResult*)stack.back());
                    delete (ScanExpressionResult*)stack.back();
                    stack.pop_back();
                    Scanner::ScannerResult* operator_ = (Scanner::ScannerResult*)stack.back();
                    stack.pop_back();
                    left = *((ScanExpressionResult*)stack.back());
                    delete (ScanExpressionResult*)stack.back();
                    stack.pop_back();
                    auto e = scanBinaryExpression(left, right, operator_);
                    stack.push_back(new ScanExpressionResult(e));
                }

                // Shift.
                this->nextToken(token);
                token->prec = prec;
                stack.push_back(token);
                auto e = this->scanIsolateCoverGrammar(&Parser::scanExponentiationExpression);
                stack.push_back(new ScanExpressionResult(e));
            }

            // Final reduce to clean-up the stack.
            size_t i = stack.size() - 1;
            expr = *((ScanExpressionResult*)stack[i]);
            delete (ScanExpressionResult*)stack[i];
            while (i > 1) {
                expr = scanBinaryExpression(*((ScanExpressionResult*)stack[i - 2]), expr, (Scanner::ScannerResult*)stack[i - 1]);
                delete (ScanExpressionResult*)stack[i - 2];
                i -= 2;
            }
            RELEASE_ASSERT(i == 0);
        }

        return expr;
    }


    Node* finishBinaryExpression(Node* left, Node* right, Scanner::ScannerResult* token)
    {
        if (token->type == Token::PunctuatorToken) {
            PunctuatorKind oper = token->valuePunctuatorKind;
            // Additive Operators
            switch (oper) {
            case Plus:
                return new BinaryExpressionPlusNode(left, right);
            case Minus:
                return new BinaryExpressionMinusNode(left, right);
            case LeftShift:
                return new BinaryExpressionLeftShiftNode(left, right);
            case RightShift:
                return new BinaryExpressionSignedRightShiftNode(left, right);
            case UnsignedRightShift:
                return new BinaryExpressionUnsignedRightShiftNode(left, right);
            case Multiply:
                return new BinaryExpressionMultiplyNode(left, right);
            case Divide:
                return new BinaryExpressionDivisionNode(left, right);
            case Mod:
                return new BinaryExpressionModNode(left, right);
            case LeftInequality:
                return new BinaryExpressionLessThanNode(left, right);
            case RightInequality:
                return new BinaryExpressionGreaterThanNode(left, right);
            case LeftInequalityEqual:
                return new BinaryExpressionLessThanOrEqualNode(left, right);
            case RightInequalityEqual:
                return new BinaryExpressionGreaterThanOrEqualNode(left, right);
            case Equal:
                return new BinaryExpressionEqualNode(left, right);
            case NotEqual:
                return new BinaryExpressionNotEqualNode(left, right);
            case StrictEqual:
                return new BinaryExpressionStrictEqualNode(left, right);
            case NotStrictEqual:
                return new BinaryExpressionNotStrictEqualNode(left, right);
            case BitwiseAnd:
                return new BinaryExpressionBitwiseAndNode(left, right);
            case BitwiseXor:
                return new BinaryExpressionBitwiseXorNode(left, right);
            case BitwiseOr:
                return new BinaryExpressionBitwiseOrNode(left, right);
            case LogicalOr:
                return new BinaryExpressionLogicalOrNode(left, right);
            case LogicalAnd:
                return new BinaryExpressionLogicalAndNode(left, right);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ASSERT(token->type == Token::KeywordToken);
            switch (token->valueKeywordKind) {
            case InKeyword:
                return new BinaryExpressionInNode(left, right);
            case KeywordKind::InstanceofKeyword:
                return new BinaryExpressionInstanceOfNode(left, right);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }

    ScanExpressionResult scanBinaryExpression(ScanExpressionResult left, ScanExpressionResult right, Scanner::ScannerResult* token)
    {
        ScanExpressionResult nd(ASTNodeType::ASTNodeTypeError);
        if (token->type == Token::PunctuatorToken) {
            PunctuatorKind oper = token->valuePunctuatorKind;
            // Additive Operators
            switch (oper) {
            case Plus:
                nd.setNodeType(ASTNodeType::BinaryExpressionPlus);
                return nd;
            case Minus:
                nd.setNodeType(ASTNodeType::BinaryExpressionMinus);
                return nd;
            case LeftShift: //Bitse Shift Oerators
                nd.setNodeType(ASTNodeType::BinaryExpressionLeftShift);
                return nd;
            case RightShift:
                nd.setNodeType(ASTNodeType::BinaryExpressionSignedRightShift);
                return nd;
            case UnsignedRightShift:
                nd.setNodeType(ASTNodeType::BinaryExpressionUnsignedRightShift);
                return nd;
            case Multiply: // Multiplicative Operators
                nd.setNodeType(ASTNodeType::BinaryExpressionMultiply);
                return nd;
            case Divide:
                nd.setNodeType(ASTNodeType::BinaryExpressionDivison);
                return nd;
            case Mod:
                nd.setNodeType(ASTNodeType::BinaryExpressionMod);
                return nd;
            case LeftInequality: //Relative Operators
                nd.setNodeType(ASTNodeType::BinaryExpressionLessThan);
                return nd;
            case RightInequality:
                nd.setNodeType(ASTNodeType::BinaryExpressionGreaterThan);
                return nd;
            case LeftInequalityEqual:
                nd.setNodeType(ASTNodeType::BinaryExpressionLessThanOrEqual);
                return nd;
            case RightInequalityEqual:
                nd.setNodeType(ASTNodeType::BinaryExpressionGreaterThanOrEqual);
                return nd;
            case Equal: //Equality Operators
                nd.setNodeType(ASTNodeType::BinaryExpressionEqual);
                return nd;
            case NotEqual:
                nd.setNodeType(ASTNodeType::BinaryExpressionNotEqual);
                return nd;
            case StrictEqual:
                nd.setNodeType(ASTNodeType::BinaryExpressionStrictEqual);
                return nd;
            case NotStrictEqual:
                nd.setNodeType(ASTNodeType::BinaryExpressionNotStrictEqual);
                return nd;
            case BitwiseAnd: //Binary Bitwise Operator
                nd.setNodeType(ASTNodeType::BinaryExpressionBitwiseAnd);
                return nd;
            case BitwiseXor:
                nd.setNodeType(ASTNodeType::BinaryExpressionBitwiseXor);
                return nd;
            case BitwiseOr:
                nd.setNodeType(ASTNodeType::BinaryExpressionBitwiseOr);
                return nd;
            case LogicalOr:
                nd.setNodeType(ASTNodeType::BinaryExpressionLogicalOr);
                return nd;
            case LogicalAnd:
                nd.setNodeType(ASTNodeType::BinaryExpressionLogicalAnd);
                return nd;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ASSERT(token->type == Token::KeywordToken);
            switch (token->valueKeywordKind) {
            case InKeyword:
                nd.setNodeType(ASTNodeType::BinaryExpressionIn);
                return nd;
            case KeywordKind::InstanceofKeyword:
                nd.setNodeType(ASTNodeType::BinaryExpressionInstanceOf);
                return nd;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }

    // ECMA-262 12.14 Conditional Operator

    template <typename T, bool isParse>
    T conditionalExpression()
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (isParse) {
            exprNode = this->inheritCoverGrammar(&Parser::parseBinaryExpression);
        } else {
            expr = this->scanInheritCoverGrammar(&Parser::scanBinaryExpressions);
        }

        if (this->match(GuessMark)) {
            RefPtr<Node> consequent;

            this->nextToken();

            bool previousAllowIn = this->context->allowIn;
            this->context->allowIn = true;
            if (isParse) {
                consequent = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
            } else {
                this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
            }
            this->context->allowIn = previousAllowIn;

            this->expect(Colon);
            if (isParse) {
                RefPtr<Node> alternate = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                exprNode = this->finalize(this->startNode(startToken), new ConditionalExpressionNode(exprNode.get(), consequent.get(), alternate.get()));
            } else {
                this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
            }

            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        }

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    // ECMA-262 12.15 Assignment Operators

    template <typename T, bool isParse>
    T assignmentExpression()
    {
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (!this->context->allowYield && this->matchKeyword(YieldKeyword)) {
            if (isParse) {
                exprNode = this->yieldExpression<ParseAs(YieldExpressionNode)>();
            } else {
                expr = this->yieldExpression<Scan>();
            }
        } else {
            ALLOC_TOKEN(startToken);
            *startToken = this->lookahead;
            ALLOC_TOKEN(token);
            *token = *startToken;
            ASTNodeType type;
            MetaNode startNode = this->createNode();
            Marker startMarker = this->lastMarker;

            if (isParse) {
                exprNode = this->conditionalExpression<Parse>();
                type = exprNode->type();
            } else {
                expr = this->conditionalExpression<Scan>();
                type = expr;
            }

            /*
            if (token.type === Token.Identifier && (token.lineNumber === this.lookahead.lineNumber) && token.value === 'async' && (this.lookahead.type === Token.Identifier)) {
                const arg = this.primaryExpression<Parse>();
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [arg],
                    async: true
                };
            } */

            if (type == ArrowParameterPlaceHolder || this->match(Arrow)) {
                // ECMA-262 14.2 Arrow Function Definitions
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (!isParse) {
                    // rewind scanner for return to normal mode
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();

                    exprNode = this->conditionalExpression<Parse>();
                }

                pushScopeContext(AtomicString());

                ParseFormalParametersResult list;

                // FIXME reinterpretAsCoverFormalsList
                if (type == Identifier) {
                    list.params.push_back(exprNode);
                    list.paramSet.push_back(exprNode->asIdentifier()->name());
                    list.valid = true;
                } else {
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();
                    this->expect(LeftParenthesis);
                    scopeContexts.back()->m_hasArrowParameterPlaceHolder = true;

                    list = this->parseFormalParameters();
                }

                if (isParse) {
                    exprNode.release();
                }

                // FIXME remove validity check?
                if (list.valid) {
                    if (this->hasLineTerminator) {
                        this->throwUnexpectedToken(&this->lookahead);
                    }
                    this->context->firstCoverInitializedNameError.reset();

                    scopeContexts.back()->m_paramsStartLOC.index = startNode.index;
                    scopeContexts.back()->m_paramsStartLOC.column = startNode.column;
                    scopeContexts.back()->m_paramsStartLOC.line = startNode.line;

                    extractNamesFromFunctionParams(list);

                    bool previousStrict = this->context->strict;
                    bool previousAllowYield = this->context->allowYield;
                    bool previousInArrowFunction = this->context->inArrowFunction;
                    this->context->allowYield = true;
                    this->context->inArrowFunction = true;

                    this->expect(Arrow);

                    MetaNode node = this->startNode(startToken);
                    MetaNode bodyStart = this->createNode();

                    RefPtr<Node> body;
                    if (this->match(LeftBrace)) {
                        body = this->parseFunctionSourceElements();
                    } else {
                        if (this->config.parseSingleFunction) {
                            // when parsing for function call,
                            // assignmentExpression should parse(scan) only child arrow functions
                            ASSERT(this->config.parseSingleFunctionChildIndex > 0);

                            size_t realIndex = this->config.parseSingleFunctionChildIndex - 1;
                            this->config.parseSingleFunctionChildIndex++;
                            InterpretedCodeBlock* currentTarget = this->config.parseSingleFunctionTarget->asInterpretedCodeBlock();
                            size_t orgIndex = this->lookahead.start;

                            StringView src = currentTarget->childBlocks()[realIndex]->bodySrc();
                            this->scanner->index = src.length() + orgIndex;

                            this->scanner->lineNumber = currentTarget->childBlocks()[realIndex]->sourceElementStart().line;
                            this->scanner->lineStart = currentTarget->childBlocks()[realIndex]->sourceElementStart().index - currentTarget->childBlocks()[realIndex]->sourceElementStart().column;
                            this->lookahead.lineNumber = this->scanner->lineNumber;
                            this->lookahead.lineStart = this->scanner->lineStart;
                            this->nextToken();
                        } else {
                            ASSERT(!this->config.parseSingleFunction);
                            LexicalBlockIndex lexicalBlockIndexBefore = this->lexicalBlockIndex;
                            LexicalBlockIndex lexicalBlockCountBefore = this->lexicalBlockCount;
                            this->lexicalBlockIndex = 0;
                            this->lexicalBlockCount = 0;
                            size_t oldSubCodeBlockIndex = this->subCodeBlockIndex;
                            this->subCodeBlockIndex = 0;

                            if (this->config.parseSingleFunction) {
                                ASSERT(this->config.parseSingleFunctionChildIndex > 0);
                                this->config.parseSingleFunctionChildIndex++;
                            }
                            body = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);

                            this->subCodeBlockIndex = oldSubCodeBlockIndex;
                            this->lexicalBlockIndex = lexicalBlockIndexBefore;
                            this->lexicalBlockCount = lexicalBlockCountBefore;

                            scopeContexts.back()->m_bodyStartLOC.line = bodyStart.line;
                            scopeContexts.back()->m_bodyStartLOC.column = bodyStart.column;
                            scopeContexts.back()->m_bodyStartLOC.index = bodyStart.index;

                            scopeContexts.back()->m_bodyEndLOC.index = this->lastMarker.index;
#ifndef NDEBUG
                            scopeContexts.back()->m_bodyEndLOC.line = this->lastMarker.lineNumber;
                            scopeContexts.back()->m_bodyEndLOC.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif
                        }
                    }
                    this->scopeContexts.back()->m_lexicalBlockIndexFunctionLocatedIn = this->lexicalBlockIndex;

                    if (this->context->strict && list.firstRestricted) {
                        this->throwUnexpectedToken(&list.firstRestricted, list.message);
                    }
                    if (this->context->strict && list.stricted) {
                        this->throwUnexpectedToken(&list.stricted, list.message);
                    }

                    exprNode = this->finalize(node, new ArrowFunctionExpressionNode(popScopeContext(node), subCodeBlockIndex)); //TODO
                    if (!isParse) {
                        expr.setNodeType(ASTNodeType::ArrowFunctionExpression);
                    }

                    this->context->strict = previousStrict;
                    this->context->allowYield = previousAllowYield;
                    this->context->inArrowFunction = previousInArrowFunction;
                }
            } else {
                if (this->matchAssign()) {
                    if (!this->context->isAssignmentTarget && this->context->strict) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (this->context->strict && type == Identifier) {
                        AtomicString name;

                        if (isParse) {
                            IdentifierNode* id = exprNode->asIdentifier();
                            name = id->name();
                        } else {
                            name = expr.string();
                        }

                        if (this->scanner->isRestrictedWord(name)) {
                            this->throwUnexpectedToken(token, Messages::StrictLHSAssignment);
                        }
                        if (this->scanner->isStrictModeReservedWord(name)) {
                            this->throwUnexpectedToken(token, Messages::StrictReservedWord);
                        }
                    }

                    if (!this->match(Substitution)) {
                        this->context->isAssignmentTarget = false;
                        this->context->isBindingElement = false;
                    } else if (isParse) {
                        this->reinterpretExpressionAsPattern(exprNode.get());

                        if (exprNode->isLiteral() || exprNode->type() == ASTNodeType::ThisExpression) {
                            this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                        }
                    } else {
                        this->scanReinterpretExpressionAsPattern(expr);

                        if (expr == ASTNodeType::Literal || expr == ASTNodeType::ThisExpression) {
                            this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                        }
                    }

                    this->nextToken(token);
                    RefPtr<Node> rightNode;
                    Node* exprResult;

                    if (isParse) {
                        rightNode = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                    } else {
                        this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                    }

                    switch (token->valuePunctuatorKind) {
                    case Substitution:
                        if (isParse) {
                            exprResult = new AssignmentExpressionSimpleNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionSimple);
                        break;
                    case PlusEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionPlusNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionPlus);
                        break;
                    case MinusEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionMinusNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionMinus);
                        break;
                    case MultiplyEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionMultiplyNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionMultiply);
                        break;
                    case DivideEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionDivisionNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionDivision);
                        break;
                    case ModEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionModNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionMod);
                        break;
                    case LeftShiftEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionLeftShiftNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionLeftShift);
                        break;
                    case RightShiftEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionSignedRightShiftNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionSignedRightShift);
                        break;
                    case UnsignedRightShiftEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionUnsignedShiftNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionUnsignedRightShift);
                        break;
                    case BitwiseXorEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionBitwiseXorNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionBitwiseXor);
                        break;
                    case BitwiseAndEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionBitwiseAndNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionBitwiseAnd);
                        break;
                    case BitwiseOrEqual:
                        if (isParse) {
                            exprResult = new AssignmentExpressionBitwiseOrNode(exprNode.get(), rightNode.get());
                            break;
                        }
                        expr.setNodeType(ASTNodeType::AssignmentExpressionBitwiseOr);
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }

                    if (isParse) {
                        exprNode = this->finalize(this->startNode(startToken), exprResult);
                    }
                    this->context->firstCoverInitializedNameError.reset();
                }
            }
        }

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    // ECMA-262 12.16 Comma Operator

    template <typename T, bool isParse>
    T expression()
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (isParse) {
            exprNode = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
        } else {
            expr = this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
        }

        if (this->match(Comma)) {
            ExpressionNodeVector expressions;
            if (isParse) {
                expressions.push_back(exprNode);
            }
            while (this->startMarker.index < this->scanner->length) {
                if (!this->match(Comma)) {
                    break;
                }
                this->nextToken();
                if (isParse) {
                    expressions.push_back(this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>));
                } else {
                    this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
            }

            if (isParse) {
                exprNode = this->finalize(this->startNode(startToken), new SequenceExpressionNode(std::move(expressions)));
            } else {
                expr = ScanExpressionResult(ASTNodeType::SequenceExpression);
            }
        }

        if (isParse) {
            return exprNode.release();
        }
        return expr;
    }

    // ECMA-262 13.2 Block

    template <typename T, bool isParse>
    T statementListItem()
    {
        RefPtr<StatementNode> statement;
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        if (this->lookahead.type == KeywordToken) {
            switch (this->lookahead.valueKeywordKind) {
            case FunctionKeyword:
                if (isParse) {
                    statement = this->parseFunctionDeclaration();
                } else {
                    this->parseFunctionDeclaration();
                }
                break;
            case ExportKeyword:
                /*
                if (this->sourceType !== 'module') {
                    this->throwUnexpectedToken(this->lookahead, Messages.IllegalExportDeclaration);
                }
                statement = this->parseExportDeclaration();
                */
                this->throwError("export keyword is not supported yet");
                break;
            case ImportKeyword:
                /*
                if (this->sourceType !== 'module') {
                    this->throwUnexpectedToken(this->lookahead, Messages.IllegalImportDeclaration);
                }
                statement = this->parseImportDeclaration();
                */
                this->throwError("import keyword is not supported yet");
                break;
            case ConstKeyword:
                if (isParse) {
                    statement = this->parseVariableStatement(KeywordKind::ConstKeyword);
                } else {
                    this->scanVariableStatement(KeywordKind::ConstKeyword);
                }
                break;
            case LetKeyword:
                if (isParse) {
                    statement = this->isLexicalDeclaration() ? this->lexicalDeclaration<ParseAs(StatementNode)>(false) : this->parseStatement();
                } else {
                    this->isLexicalDeclaration() ? this->lexicalDeclaration<ScanAsVoid>(false) : this->scanStatement();
                }
                break;
            case ClassKeyword:
                if (isParse) {
                    statement = this->parseClassDeclaration();
                } else {
                    this->scanClassDeclaration();
                }
                break;
            default:
                if (isParse) {
                    statement = this->parseStatement();
                } else {
                    this->scanStatement();
                }
                break;
            }
        } else if (isParse) {
            statement = this->parseStatement();
        } else {
            this->scanStatement();
        }

        if (isParse) {
            return T(statement.release());
        }
        return T(nullptr);
    }

    struct ParserBlockContext {
        size_t lexicalBlockCountBefore;
        size_t lexicalBlockIndexBefore;
        size_t childLexicalBlockIndex;

        ParserBlockContext()
            : lexicalBlockCountBefore(SIZE_MAX)
            , lexicalBlockIndexBefore(SIZE_MAX)
            , childLexicalBlockIndex(SIZE_MAX)
        {
        }
    };

    ParserBlockContext openBlock()
    {
        if (UNLIKELY(this->lexicalBlockCount == LEXICAL_BLOCK_INDEX_MAX - 1)) {
            this->throwError("too many lexical blocks in script", String::emptyString, String::emptyString, ErrorObject::RangeError);
        }

        ParserBlockContext ctx;
        this->lexicalBlockCount++;
        ctx.lexicalBlockCountBefore = this->lexicalBlockCount;
        ctx.lexicalBlockIndexBefore = this->lexicalBlockIndex;
        ctx.childLexicalBlockIndex = this->lexicalBlockCount;

        this->scopeContexts.back()->insertBlockScope(ctx.childLexicalBlockIndex, this->lexicalBlockIndex,
                                                     ExtendedNodeLOC(this->lastMarker.lineNumber, this->lastMarker.index - this->lastMarker.lineStart + 1, this->lastMarker.index));
        this->lexicalBlockIndex = ctx.childLexicalBlockIndex;

        return ctx;
    }

    void closeBlock(ParserBlockContext& ctx)
    {
        if (this->config.parseSingleFunction) {
            bool finded = false;
            for (size_t i = 0; i < this->config.parseSingleFunctionTarget->asInterpretedCodeBlock()->blockInfos().size(); i++) {
                if (this->config.parseSingleFunctionTarget->asInterpretedCodeBlock()->blockInfos()[i]->m_blockIndex == ctx.childLexicalBlockIndex) {
                    finded = true;
                    break;
                }
            }
            if (!finded) {
                ctx.childLexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
            }
        } else {
            // if there is no new variable in this block, merge this block into parent block
            auto currentFunctionScope = this->scopeContexts.back();
            auto blockContext = currentFunctionScope->findBlockFromBackward(this->lexicalBlockIndex);
            if (this->lexicalBlockIndex != 0 && blockContext->m_names.size() == 0) {
                const auto currentBlockIndex = this->lexicalBlockIndex;
                LexicalBlockIndex parentBlockIndex = blockContext->m_parentBlockIndex;

                bool isThereFunctionExists = false;
                for (size_t i = 0; i < currentFunctionScope->m_childScopes.size(); i++) {
                    if (currentFunctionScope->m_childScopes[i]->m_nodeType == ASTNodeType::FunctionDeclaration
                        && currentFunctionScope->m_childScopes[i]->m_lexicalBlockIndexFunctionLocatedIn == currentBlockIndex) {
                        isThereFunctionExists = true;
                    }
                }

                if (!isThereFunctionExists) { // block collapse start
                    for (size_t i = 0; i < currentFunctionScope->m_childBlockScopes.size(); i++) {
                        if (currentFunctionScope->m_childBlockScopes[i]->m_parentBlockIndex == currentBlockIndex) {
                            currentFunctionScope->m_childBlockScopes[i]->m_parentBlockIndex = parentBlockIndex;
                        }
                    }

                    for (size_t i = 0; i < currentFunctionScope->m_childScopes.size(); i++) {
                        if (currentFunctionScope->m_childScopes[i]->m_lexicalBlockIndexFunctionLocatedIn == currentBlockIndex) {
                            currentFunctionScope->m_childScopes[i]->m_lexicalBlockIndexFunctionLocatedIn = parentBlockIndex;
                        }
                    }

                    auto parentBlockContext = currentFunctionScope->findBlockFromBackward(parentBlockIndex);
                    for (size_t i = 0; i < blockContext->m_usingNames.size(); i++) {
                        AtomicString name = blockContext->m_usingNames[i];
                        if (VectorUtil::findInVector(parentBlockContext->m_usingNames, name) == VectorUtil::invalidIndex) {
                            parentBlockContext->m_usingNames.push_back(name);
                        }
                    }

                    // remove current block context from function context
                    for (size_t i = 0; i < currentFunctionScope->m_childBlockScopes.size(); i++) {
                        if (currentFunctionScope->m_childBlockScopes[i]->m_blockIndex == currentBlockIndex) {
                            currentFunctionScope->m_childBlockScopes.erase(i);
                            break;
                        }
                    }
                    ctx.childLexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
                }
            }
        }

        this->lexicalBlockIndex = ctx.lexicalBlockIndexBefore;
    }

    template <typename T, bool isParse>
    T block()
    {
        this->expect(LeftBrace);
        RefPtr<StatementContainer> block;
        StatementNode* referNode = nullptr;

        ParserBlockContext blockContext = openBlock();

        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = true;

        if (isParse) {
            block = StatementContainer::create();
        }

        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
            if (isParse) {
                referNode = block->appendChild(this->statementListItem<ParseAs(StatementNode)>().get(), referNode);
            } else {
                this->statementListItem<ScanAsVoid>();
            }
        }
        this->expect(RightBrace);

        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        closeBlock(blockContext);

        if (isParse) {
            MetaNode node = this->createNode();
            return T(this->finalize(node, new BlockStatementNode(block.get(), blockContext.childLexicalBlockIndex)));
        }

        return T(nullptr);
    }

    // ECMA-262 13.3.1 Let and Const Declarations

    template <typename T, bool isParse>
    T lexicalBinding(KeywordKind kind, bool inFor)
    {
        auto node = this->createNode();
        ScannerResultVector params;
        RefPtr<Node> idNode;
        bool isIdentifier;
        ScanExpressionResult id;
        AtomicString name;

        if (isParse) {
            idNode = this->pattern<Parse>(params, kind, true);
            isIdentifier = (idNode->type() == Identifier);
            if (isIdentifier) {
                name = ((IdentifierNode*)idNode.get())->name();
            }
        } else {
            id = this->pattern<Scan>(params, kind, true);
            isIdentifier = (id == Identifier);
            if (isIdentifier) {
                name = id.string();
            }
        }

        // ECMA-262 12.2.1
        if (this->context->strict && isIdentifier && this->scanner->isRestrictedWord(name)) {
            this->throwError(Messages::StrictVarName);
        }

        if (isParse) {
            RefPtr<Node> init;
            if (kind == KeywordKind::ConstKeyword) {
                if (!this->matchKeyword(KeywordKind::InKeyword) && !this->matchContextualKeyword("of")) {
                    this->expect(Substitution);
                    init = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                }
            } else if ((!inFor && !isIdentifier) || this->match(Substitution)) {
                this->expect(Substitution);
                init = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
            }

            return T(this->finalize(node, new VariableDeclaratorNode(kind, idNode.get(), init.get())));
        } else {
            if (kind == KeywordKind::ConstKeyword) {
                if (!this->matchKeyword(KeywordKind::InKeyword) && !this->matchContextualKeyword("of")) {
                    this->expect(Substitution);
                    this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
            } else if ((!inFor && !isIdentifier) || this->match(Substitution)) {
                this->expect(Substitution);
                this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
            }
            return T(nullptr);
        }
    }

    template <typename T, bool isParse>
    T bindingList(KeywordKind kind, bool inFor)
    {
        if (isParse) {
            VariableDeclaratorVector list;
            list.push_back(this->lexicalBinding<ParseAs(VariableDeclaratorNode)>(kind, inFor));
            while (this->match(Comma)) {
                this->nextToken();
                list.push_back(this->lexicalBinding<ParseAs(VariableDeclaratorNode)>(kind, inFor));
            }
            return T(list);
        } else {
            this->lexicalBinding<ScanAsVoid>(kind, inFor);
            while (this->match(Comma)) {
                this->nextToken();
                this->lexicalBinding<ScanAsVoid>(kind, inFor);
            }
            return T();
        }
    }

    template <typename T, bool isParse>
    T lexicalDeclaration(bool inFor)
    {
        auto node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        auto kind = token->valueKeywordKind;

        VariableDeclaratorVector declarations;
        if (isParse) {
            declarations = this->bindingList<VariableDeclaratorVector, true>(kind, inFor);
        } else {
            this->bindingList<ScanAsVoid>(kind, inFor);
        }

        this->consumeSemicolon();

        if (isParse) {
            return T(this->finalize(node, new VariableDeclarationNode(std::move(declarations), kind)));
        }
        return T(nullptr);
    }

    bool isLexicalDeclaration()
    {
        auto previousIndex = this->scanner->index;
        auto previousLineNumber = this->scanner->lineNumber;
        auto previousLineStart = this->scanner->lineStart;
        this->collectComments();
        ALLOC_TOKEN(next);
        this->scanner->lex(next);
        this->scanner->index = previousIndex;
        this->scanner->lineNumber = previousLineNumber;
        this->scanner->lineStart = previousLineStart;

        return (next->type == Token::IdentifierToken) || (next->type == Token::PunctuatorToken && next->valuePunctuatorKind == PunctuatorKind::LeftSquareBracket) || (next->type == Token::PunctuatorToken && next->valuePunctuatorKind == PunctuatorKind::LeftBrace) || (next->type == Token::KeywordToken && next->valueKeywordKind == KeywordKind::LetKeyword) || (next->type == Token::KeywordToken && next->valueKeywordKind == KeywordKind::YieldKeyword);
    }

    // ECMA-262 13.3.2 Variable Statement
    PassRefPtr<IdentifierNode> parseVariableIdentifier(KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type == Token::KeywordToken && token->valueKeywordKind == YieldKeyword) {
            if (this->context->strict) {
                this->throwUnexpectedToken(token, Messages::StrictReservedWord);
            }
            if (!this->context->allowYield) {
                this->throwUnexpectedToken(token);
            }
        } else if (token->type != Token::IdentifierToken) {
            if (this->context->strict && token->type == Token::KeywordToken && this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                this->throwUnexpectedToken(token, Messages::StrictReservedWord);
            } else {
                if (this->context->strict || token->relatedSource() != "let" || (kind != VarKeyword)) {
                    this->throwUnexpectedToken(token);
                }
            }
        } else if (this->sourceType == Module && token->type == Token::IdentifierToken && token->relatedSource() == "await") {
            this->throwUnexpectedToken(token);
        }

        IdentifierNode* id = finishIdentifier(token, true);

        if (kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword) {
            addDeclaredNameIntoContext(id->name(), this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
        }

        return this->finalize(node, id);
    }

    ScanExpressionResult scanVariableIdentifier(KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type == Token::KeywordToken && token->valueKeywordKind == YieldKeyword) {
            if (this->context->strict) {
                this->throwUnexpectedToken(token, Messages::StrictReservedWord);
            }
            if (!this->context->allowYield) {
                this->throwUnexpectedToken(token);
            }
        } else if (token->type != Token::IdentifierToken) {
            if (this->context->strict && token->type == Token::KeywordToken && this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                this->throwUnexpectedToken(token, Messages::StrictReservedWord);
            } else {
                if (this->context->strict || token->relatedSource() != "let" || (kind != VarKeyword)) {
                    this->throwUnexpectedToken(token);
                }
            }
        } else if (this->sourceType == Module && token->type == Token::IdentifierToken && token->relatedSource() == "await") {
            this->throwUnexpectedToken(token);
        }

        ScanExpressionResult id = finishScanIdentifier(token, true);

        if (kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword) {
            addDeclaredNameIntoContext(id.string(), this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
        }

        return id;
    }

    void addDeclaredNameIntoContext(AtomicString name, LexicalBlockIndex blockIndex, KeywordKind kind, bool isExplicitVariableDeclaration = false)
    {
        ASSERT(kind == VarKeyword || kind == LetKeyword || kind == ConstKeyword);

        if (!this->config.parseSingleFunction) {
            /*
               we need this bunch of code for tolerate this error(we consider variable 'e' as lexically declared)
               try { } catch(e) { var e; }
            */
            if (this->context->inCatchClause && isExplicitVariableDeclaration && kind == KeywordKind::VarKeyword) {
                for (size_t i = 0; i < this->context->catchClauseSimplyDeclaredVariableNames.size(); i++) {
                    if (this->context->catchClauseSimplyDeclaredVariableNames[i] == name) {
                        this->scopeContexts.back()->insertVarName(name, blockIndex, true, kind == VarKeyword);
                        return;
                    }
                }
            }
            /* code end */

            if (!this->scopeContexts.back()->canDeclareName(name, blockIndex, kind == VarKeyword)) {
                this->throwError("Identifier '%s' has already been declared", name.string());
            }
            if (kind == VarKeyword) {
                this->scopeContexts.back()->insertVarName(name, blockIndex, true, true);
            } else {
                this->scopeContexts.back()->insertNameAtBlock(name, blockIndex, kind == ConstKeyword);
                this->scopeContexts.back()->m_needsToComputeLexicalBlockStuffs = true;
            }
        }
    }

    template <typename T, bool isParse>
    T variableDeclaration(DeclarationOptions& options)
    {
        ScannerResultVector params;
        RefPtr<Node> idNode;
        ScanExpressionResult id;
        bool isIdentifier;
        AtomicString name;

        if (isParse) {
            idNode = this->pattern<Parse>(params, options.kind, true);
            isIdentifier = (idNode->type() == Identifier);
            if (isIdentifier) {
                name = ((IdentifierNode*)idNode.get())->name();
            }
        } else {
            id = this->pattern<Scan>(params, options.kind, true);
            isIdentifier = (id == Identifier);
            if (isIdentifier) {
                name = id.string();
            }
        }

        // ECMA-262 12.2.1
        if (this->context->strict && isIdentifier && this->scanner->isRestrictedWord(name)) {
            this->throwError(Messages::StrictVarName);
        }

        if (options.kind != VarKeyword && !this->context->allowLexicalDeclaration) {
            this->throwError("Lexical declaration cannot appear in a single-statement context");
        }

        RefPtr<Node> initNode = nullptr;
        if (this->match(Substitution)) {
            this->nextToken();
            ASTNodeType type = ASTNodeType::ASTNodeTypeError;
            if (isParse) {
                initNode = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                if (initNode) {
                    type = initNode->type();
                }
            } else {
                type = this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
            }
            if (type == ASTNodeType::FunctionExpression || type == ASTNodeType::ArrowFunctionExpression) {
                if (lastPoppedScopeContext->m_functionName == AtomicString()) {
                    lastPoppedScopeContext->m_functionName = name;
                    lastPoppedScopeContext->m_hasImplictFunctionName = true;
                }
            }
        } else if (!isIdentifier && !options.inFor) {
            this->expect(Substitution);
        }

        if (isParse && initNode == nullptr && options.kind == ConstKeyword && !options.inFor) {
            this->throwError("Missing initializer in const identifier '%s' declaration", name.string());
        }

        if (isParse) {
            MetaNode node = this->createNode();
            return T(this->finalize(node, new VariableDeclaratorNode(options.kind, idNode.get(), initNode.get())));
        }
        return T(nullptr);
    }

    VariableDeclaratorVector parseVariableDeclarationList(DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;
        opt.kind = options.kind;

        VariableDeclaratorVector list;
        list.push_back(this->variableDeclaration<ParseAs(VariableDeclaratorNode)>(opt));
        while (this->match(Comma)) {
            this->nextToken();
            list.push_back(this->variableDeclaration<ParseAs(VariableDeclaratorNode)>(opt));
        }

        return list;
    }

    void scanVariableDeclarationList(DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;
        opt.kind = options.kind;
        size_t listSize = 0;

        this->variableDeclaration<ScanAsVoid>(opt);
        while (this->match(Comma)) {
            this->nextToken();
            this->variableDeclaration<ScanAsVoid>(opt);
        }
    }

    PassRefPtr<VariableDeclarationNode> parseVariableStatement(KeywordKind kind = VarKeyword)
    {
        this->expectKeyword(kind);
        MetaNode node = this->createNode();
        DeclarationOptions opt;
        opt.inFor = false;
        opt.kind = kind;
        VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
        this->consumeSemicolon();

        return this->finalize(node, new VariableDeclarationNode(std::move(declarations), kind));
    }

    void scanVariableStatement(KeywordKind kind = VarKeyword)
    {
        this->expectKeyword(kind);
        DeclarationOptions opt;
        opt.inFor = false;
        opt.kind = kind;
        this->scanVariableDeclarationList(opt);
        this->consumeSemicolon();
    }

    // ECMA-262 13.4 Empty Statement

    template <typename T, bool isParse>
    T emptyStatement()
    {
        this->expect(SemiColon);

        if (isParse) {
            MetaNode node = this->createNode();
            return T(this->finalize(node, new EmptyStatementNode()));
        }
        return T(nullptr);
    }

    // ECMA-262 13.5 Expression Statement

    PassRefPtr<ExpressionStatementNode> parseExpressionStatement()
    {
        MetaNode node = this->createNode();
        RefPtr<Node> expr = this->expression<Parse>();
        this->consumeSemicolon();
        return this->finalize(node, new ExpressionStatementNode(expr.get()));
    }

    void scanExpressionStatement()
    {
        this->expression<Scan>();
        this->consumeSemicolon();
    }

    // ECMA-262 13.6 If statement

    template <typename T, bool isParse>
    T ifStatement()
    {
        RefPtr<Node> test;
        RefPtr<Node> consequent;
        RefPtr<Node> alternate;
        bool allowFunctionDeclaration = !this->context->strict;

        this->expectKeyword(IfKeyword);
        this->expect(LeftParenthesis);

        if (isParse) {
            test = this->expression<Parse>();
        } else {
            this->expression<Scan>();
        }

        this->expect(RightParenthesis);

        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;

        if (isParse) {
            consequent = this->parseStatement(allowFunctionDeclaration);
        } else {
            this->scanStatement(allowFunctionDeclaration);
        }

        this->context->allowLexicalDeclaration = false;
        if (this->matchKeyword(ElseKeyword)) {
            this->nextToken();
            if (isParse) {
                alternate = this->parseStatement(allowFunctionDeclaration);
            } else {
                this->scanStatement(allowFunctionDeclaration);
            }
        }

        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        if (isParse) {
            return T(this->finalize(this->createNode(), new IfStatementNode(test.get(), consequent.get(), alternate.get())));
        }
        return T(nullptr);
    }

    // ECMA-262 13.7.2 The do-while Statement

    template <typename T, bool isParse>
    T doWhileStatement()
    {
        this->expectKeyword(DoKeyword);

        bool previousInIteration = this->context->inIteration;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inIteration = true;

        RefPtr<Node> body;
        if (isParse) {
            body = this->parseStatement(false);
        } else {
            this->scanStatement(false);
        }
        this->context->inIteration = previousInIteration;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        this->expectKeyword(WhileKeyword);
        this->expect(LeftParenthesis);
        RefPtr<Node> test;
        if (isParse) {
            test = this->expression<Parse>();
        } else {
            this->expression<Scan>();
        }

        this->expect(RightParenthesis);
        if (this->match(SemiColon)) {
            this->nextToken();
        }

        if (isParse) {
            return T(this->finalize(this->createNode(), new DoWhileStatementNode(test.get(), body.get())));
        }
        return T(nullptr);
    }

    // ECMA-262 13.7.3 The while Statement

    template <typename T, bool isParse>
    T whileStatement()
    {
        bool prevInLoop = this->context->inLoop;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inLoop = true;

        this->expectKeyword(WhileKeyword);
        this->expect(LeftParenthesis);
        RefPtr<Node> test;
        if (isParse) {
            test = this->expression<Parse>();
        } else {
            this->expression<Scan>();
        }
        this->expect(RightParenthesis);

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        RefPtr<Node> body;
        if (isParse) {
            body = this->parseStatement(false);
        } else {
            this->scanStatement(false);
        }
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        if (isParse) {
            return T(this->finalize(this->createNode(), new WhileStatementNode(test.get(), body.get())));
        }
        return T(nullptr);
    }

    // ECMA-262 13.7.4 The for Statement
    // ECMA-262 13.7.5 The for-in and for-of Statements

    enum ForStatementType {
        statementTypeFor,
        statementTypeForIn,
        statementTypeForOf
    };

    template <typename T, bool isParse>
    T forStatement()
    {
        RefPtr<Node> init;
        RefPtr<Node> test;
        RefPtr<Node> update;
        RefPtr<Node> left;
        RefPtr<Node> right;
        ForStatementType type = statementTypeFor;
        bool prevInLoop = this->context->inLoop;
        bool isLexicalDeclaration = false;

        this->expectKeyword(ForKeyword);
        this->expect(LeftParenthesis);

        ParserBlockContext headBlockContext = openBlock();

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(VarKeyword)) {
                this->nextToken();

                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                DeclarationOptions opt;
                opt.inFor = true;
                opt.kind = VarKeyword;
                VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
                this->context->allowIn = previousAllowIn;

                if (declarations.size() == 1 && this->matchKeyword(InKeyword)) {
                    RefPtr<VariableDeclaratorNode> decl = declarations[0];
                    // if (decl->init() && (decl.id.type === Syntax.ArrayPattern || decl.id.type === Syntax.ObjectPattern || this->context->strict)) {
                    if (decl->init() && (decl->id()->type() == ArrayExpression || decl->id()->type() == ObjectExpression || this->context->strict)) {
                        this->throwError(Messages::ForInOfLoopInitializer, new ASCIIString("for-in"));
                    }
                    if (isParse) {
                        left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), VarKeyword));
                    }

                    this->nextToken();
                    type = statementTypeForIn;
                } else if (declarations.size() == 1 && declarations[0]->init() == nullptr
                           && this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource() == "of") {
                    if (isParse) {
                        left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), VarKeyword));
                    }

                    this->nextToken();
                    type = statementTypeForOf;
                } else {
                    if (isParse) {
                        init = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), VarKeyword));
                    }
                    this->expect(SemiColon);
                }
            } else if (this->matchKeyword(ConstKeyword) || this->matchKeyword(LetKeyword)) {
                isLexicalDeclaration = true;
                const bool previousAllowLexicalDeclaration = this->context->allowLexicalDeclaration;
                this->context->allowLexicalDeclaration = true;

                ALLOC_TOKEN(token);
                KeywordKind kind = this->lookahead.valueKeywordKind;
                this->nextToken(token);

                if (!this->context->strict && this->matchKeyword(InKeyword)) {
                    this->nextToken();
                    left = this->finalize(this->createNode(), new IdentifierNode(AtomicString(this->escargotContext, this->lookahead.relatedSource())));
                    right = this->expression<Parse>();
                    init = nullptr;
                } else {
                    const bool previousAllowIn = this->context->allowIn;
                    this->context->allowIn = false;

                    DeclarationOptions opt;
                    opt.inFor = true;
                    opt.kind = kind;
                    VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
                    this->context->allowIn = previousAllowIn;

                    if (declarations.size() == 1 && declarations[0]->init() == nullptr && this->matchKeyword(InKeyword)) {
                        if (isParse) {
                            left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), kind));
                        }
                        this->nextToken();
                        type = statementTypeForIn;
                    } else if (declarations.size() == 1 && declarations[0]->init() == nullptr && this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource() == "of") {
                        if (isParse) {
                            left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), kind));
                        }
                        this->nextToken();
                        type = statementTypeForOf;
                    } else {
                        if (isParse) {
                            init = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations), kind));
                        }
                        this->expect(SemiColon);
                    }
                }

                this->context->allowLexicalDeclaration = previousAllowLexicalDeclaration;
            } else {
                ALLOC_TOKEN(initStartToken);
                *initStartToken = this->lookahead;
                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                init = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(InKeyword)) {
                    if (init->isLiteral() || init->isAssignmentOperation() || init->type() == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInForIn);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init.get());
                    left = init;
                    init = nullptr;
                    type = statementTypeForIn;
                } else if (this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource() == "of") {
                    if (!this->context->isAssignmentTarget || init->isAssignmentOperation()) {
                        this->throwError(Messages::InvalidLHSInForLoop);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init.get());
                    left = init;
                    init = nullptr;
                    type = statementTypeForOf;
                } else {
                    if (this->match(Comma)) {
                        ExpressionNodeVector initSeq;
                        initSeq.push_back(init);
                        while (this->match(Comma)) {
                            this->nextToken();
                            initSeq.push_back(this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>));
                        }
                        init = this->finalize(this->startNode(initStartToken), new SequenceExpressionNode(std::move(initSeq)));
                    }
                    this->expect(SemiColon);
                }
            }
        }

        if (type == statementTypeFor) {
            this->context->inLoop = true;
            if (!this->match(SemiColon)) {
                test = this->expression<Parse>();
            }
            this->expect(SemiColon);
            if (!this->match(RightParenthesis)) {
                update = this->expression<Parse>();
            }
        } else if (type == statementTypeForIn) {
            ASSERT(!isParse || left != nullptr);

            if (isParse) {
                right = this->expression<Parse>();
            } else {
                this->expression<Scan>();
            }
        } else {
            ASSERT(type == statementTypeForOf);
            ASSERT(!isParse || left != nullptr);

            if (isParse) {
                right = this->assignmentExpression<Parse>();
            } else {
                this->assignmentExpression<Scan>();
            }
        }


        this->expect(RightParenthesis);

        ParserBlockContext iterationBlockContext = openBlock();
        ASTBlockScopeContext* headBlockContextInstance = this->scopeContexts.back()->findBlockFromBackward(headBlockContext.childLexicalBlockIndex);
        if (headBlockContextInstance->m_names.size()) {
            // if there are names on headContext (let, const)
            // we should copy names into to iterationBlock
            this->scopeContexts.back()->findBlockFromBackward(iterationBlockContext.childLexicalBlockIndex)->m_names = headBlockContextInstance->m_names;
        }

        bool previousInIteration = this->context->inIteration;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inIteration = true;
        RefPtr<Node> body;
        if (isParse) {
            auto functor = std::bind(&Parser::parseStatement, std::ref(*this), false);
            body = this->isolateCoverGrammarWithFunctor(functor);
        } else {
            auto functor = std::bind(&Parser::scanStatement, std::ref(*this), false);
            this->scanIsolateCoverGrammarWithFunctor(functor);
        }
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        closeBlock(iterationBlockContext);
        closeBlock(headBlockContext);

        if (!isParse) {
            return T(nullptr);
        }

        MetaNode node = this->createNode();

        if (type == statementTypeFor) {
            ForStatementNode* forNode = new ForStatementNode(init.get(), test.get(), update.get(), body.get(), isLexicalDeclaration, headBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
            return T(this->finalize(node, forNode));
        }

        if (type == statementTypeForIn) {
            ForInOfStatementNode* forInNode = new ForInOfStatementNode(left.get(), right.get(), body.get(), true, isLexicalDeclaration, headBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
            return T(this->finalize(node, forInNode));
        }

        ASSERT(type == statementTypeForOf);
        ForInOfStatementNode* forOfNode = new ForInOfStatementNode(left.get(), right.get(), body.get(), false, isLexicalDeclaration, headBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
        return T(this->finalize(node, forOfNode));
    }

    void removeLabel(AtomicString label)
    {
        for (size_t i = 0; i < this->context->labelSet.size(); i++) {
            if (this->context->labelSet[i].first == label) {
                this->context->labelSet.erase(this->context->labelSet.begin() + i);
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool hasLabel(AtomicString label)
    {
        for (size_t i = 0; i < this->context->labelSet.size(); i++) {
            if (this->context->labelSet[i].first == label) {
                return true;
            }
        }
        return false;
    }
    // ECMA-262 13.8 The continue statement

    template <typename T, bool isParse>
    T continueStatement()
    {
        this->expectKeyword(ContinueKeyword);

        AtomicString labelString;
        if (this->lookahead.type == IdentifierToken && !this->hasLineTerminator) {
            if (isParse) {
                RefPtr<IdentifierNode> labelNode = this->parseVariableIdentifier();
                labelString = labelNode->name();
            } else {
                ScanExpressionResult label = this->scanVariableIdentifier();
                labelString = label.string();
            }

            if (!hasLabel(labelString)) {
                this->throwError(Messages::UnknownLabel, labelString.string());
            }

            for (size_t i = 0; i < this->context->labelSet.size(); i++) {
                if (this->context->labelSet[i].first == labelString && !this->context->labelSet[i].second) {
                    this->throwError(Messages::UnknownLabel, labelString.string());
                }
            }
        } else if (!this->context->inIteration) {
            this->throwError(Messages::IllegalContinue);
        }

        this->consumeSemicolon();

        if (!isParse) {
            return T(nullptr);
        }

        MetaNode node = this->createNode();
        if (labelString.string()->length() != 0) {
            return T(this->finalize(node, new ContinueLabelStatementNode(labelString.string())));
        } else {
            return T(this->finalize(node, new ContinueStatementNode()));
        }
    }

    // ECMA-262 13.9 The break statement

    template <typename T, bool isParse>
    T breakStatement()
    {
        this->expectKeyword(BreakKeyword);

        AtomicString labelString;
        if (this->lookahead.type == IdentifierToken && !this->hasLineTerminator) {
            if (isParse) {
                RefPtr<IdentifierNode> labelNode = this->parseVariableIdentifier();
                labelString = labelNode->name();
            } else {
                ScanExpressionResult label = this->scanVariableIdentifier();
                labelString = label.string();
            }

            if (!hasLabel(labelString)) {
                this->throwError(Messages::UnknownLabel, labelString.string());
            }
        } else if (!this->context->inIteration && !this->context->inSwitch) {
            this->throwError(Messages::IllegalBreak);
        }

        this->consumeSemicolon();

        if (!isParse) {
            return T(nullptr);
        }

        MetaNode node = this->createNode();
        if (labelString.string()->length() != 0) {
            return T(this->finalize(node, new BreakLabelStatementNode(labelString.string())));
        } else {
            return T(this->finalize(node, new BreakStatementNode()));
        }
    }

    // ECMA-262 13.10 The return statement

    template <typename T, bool isParse>
    T returnStatement()
    {
        if (!this->context->inFunctionBody) {
            this->throwError(Messages::IllegalReturn);
        }

        this->expectKeyword(ReturnKeyword);

        bool hasArgument = !this->match(SemiColon) && !this->match(RightBrace) && !this->hasLineTerminator && this->lookahead.type != EOFToken;
        RefPtr<Node> argument;
        if (hasArgument) {
            if (isParse) {
                argument = this->expression<Parse>();
            } else {
                this->expression<Scan>();
            }
        }
        this->consumeSemicolon();

        if (isParse) {
            return T(this->finalize(this->createNode(), new ReturnStatmentNode(argument.get())));
        }
        return T(nullptr);
    }

    // ECMA-262 13.11 The with statement

    PassRefPtr<Node> parseWithStatement()
    {
        if (this->context->strict) {
            this->throwError(Messages::StrictModeWith);
        }

        this->expectKeyword(WithKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> object = this->expression<Parse>();
        this->expect(RightParenthesis);

        scopeContexts.back()->m_hasWith = true;

        bool prevInWith = this->context->inWith;
        this->context->inWith = true;

        RefPtr<StatementNode> body = this->parseStatement(false);
        this->context->inWith = prevInWith;

        return this->finalize(node, new WithStatementNode(object, body.get()));
    }

    // ECMA-262 13.12 The switch statement

    PassRefPtr<SwitchCaseNode> parseSwitchCase()
    {
        MetaNode node = this->createNode();

        RefPtr<Node> test;
        if (this->matchKeyword(DefaultKeyword)) {
            node = this->createNode();
            this->nextToken();
            test = nullptr;
        } else {
            this->expectKeyword(CaseKeyword);
            node = this->createNode();
            test = this->expression<Parse>();
        }
        this->expect(Colon);

        RefPtr<StatementContainer> consequent = StatementContainer::create();
        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(DefaultKeyword) || this->matchKeyword(CaseKeyword)) {
                break;
            }
            consequent->appendChild(this->statementListItem<ParseAs(StatementNode)>());
        }

        return this->finalize(node, new SwitchCaseNode(test.get(), consequent.get()));
    }

    bool scanSwitchCase()
    {
        bool isDefaultNode;
        if (this->matchKeyword(DefaultKeyword)) {
            this->nextToken();
            isDefaultNode = true;
        } else {
            this->expectKeyword(CaseKeyword);
            this->expression<Scan>();
            isDefaultNode = false;
        }
        this->expect(Colon);

        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(DefaultKeyword) || this->matchKeyword(CaseKeyword)) {
                break;
            }
            this->statementListItem<ScanAsVoid>();
        }
        return isDefaultNode;
    }

    template <typename T, bool isParse>
    T switchStatement()
    {
        this->expectKeyword(SwitchKeyword);

        this->expect(LeftParenthesis);
        RefPtr<Node> discriminant;
        if (isParse) {
            discriminant = this->expression<Parse>();
        } else {
            this->expression<Scan>();
        }
        this->expect(RightParenthesis);

        bool previousInSwitch = this->context->inSwitch;
        this->context->inSwitch = true;

        RefPtr<StatementContainer> casesA, casesB;
        RefPtr<Node> deflt;
        if (isParse) {
            casesA = StatementContainer::create();
            casesB = StatementContainer::create();
        }

        bool defaultFound = false;
        this->expect(LeftBrace);

        while (true) {
            if (this->match(RightBrace)) {
                break;
            }

            if (isParse) {
                RefPtr<SwitchCaseNode> clause = this->parseSwitchCase();
                if (clause->isDefaultNode()) {
                    if (defaultFound) {
                        this->throwError(Messages::MultipleDefaultsInSwitch);
                    }
                    deflt = clause;
                    defaultFound = true;
                } else {
                    if (defaultFound) {
                        casesA->appendChild(clause);
                    } else {
                        casesB->appendChild(clause);
                    }
                }
            } else if (this->scanSwitchCase()) {
                if (defaultFound) {
                    this->throwError(Messages::MultipleDefaultsInSwitch);
                }
                defaultFound = true;
            }
        }
        this->expect(RightBrace);

        this->context->inSwitch = previousInSwitch;
        if (isParse) {
            return T(this->finalize(this->createNode(), new SwitchStatementNode(discriminant.get(), casesA.get(), deflt.get(), casesB.get())));
        }

        return T(nullptr);
    }

    // ECMA-262 13.13 Labelled Statements

    template <typename T, bool isParse>
    T labelledStatement(size_t multiLabelCount = 1)
    {
        RefPtr<Node> expr;
        AtomicString name;
        bool isIdentifier = false;

        if (isParse) {
            expr = this->expression<Parse>();
            if (expr->type() == Identifier) {
                isIdentifier = true;
                name = ((IdentifierNode*)expr.get())->name();
            }
        } else {
            ScanExpressionResult result = this->expression<Scan>();
            if (result == Identifier) {
                isIdentifier = true;
                name = result.string();
            }
        }

        StatementNode* statement;
        if (isIdentifier && this->match(Colon)) {
            this->nextToken();

            if (hasLabel(name)) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Label"), name.string());
            }

            this->context->labelSet.push_back(std::make_pair(name, false));

            if (this->lookahead.type == IdentifierToken) {
                if (isParse) {
                    RefPtr<StatementNode> labeledBody = this->labelledStatement<ParseAs(StatementNode)>(multiLabelCount + 1);
                    statement = new LabeledStatementNode(labeledBody.get(), name.string());
                } else {
                    this->labelledStatement<ScanAsVoid>(multiLabelCount + 1);
                }
            } else {
                if (this->matchKeyword(DoKeyword) || this->matchKeyword(ForKeyword) || this->matchKeyword(WhileKeyword)) {
                    // Turn labels to accept continue references.
                    size_t end = this->context->labelSet.size() - 1;

                    for (size_t i = 0; i < multiLabelCount; i++) {
                        ASSERT(end >= i);
                        this->context->labelSet[end - i].second = true;
                    }
                }

                if (isParse) {
                    RefPtr<StatementNode> labeledBody = this->parseStatement(!this->context->strict);
                    statement = new LabeledStatementNode(labeledBody.get(), name.string());
                } else {
                    this->scanStatement(!this->context->strict);
                }
            }
            removeLabel(name);
        } else {
            this->consumeSemicolon();
            if (isParse) {
                statement = new ExpressionStatementNode(expr.get());
            }
        }

        if (isParse) {
            return T(this->finalize(this->createNode(), statement));
        }
        return T(nullptr);
    }

    // ECMA-262 13.14 The throw statement

    template <typename T, bool isParse>
    T throwStatement()
    {
        auto metaNode = this->createNode();
        this->expectKeyword(ThrowKeyword);

        if (this->hasLineTerminator) {
            this->throwError(Messages::NewlineAfterThrow);
        }

        RefPtr<Node> argument;
        if (isParse) {
            argument = this->expression<Parse>();
        } else {
            this->expression<Scan>();
        }
        this->consumeSemicolon();

        if (isParse) {
            return T(this->finalize(metaNode, new ThrowStatementNode(argument.get())));
        }
        return T(nullptr);
    }

    // ECMA-262 13.15 The try statement

    template <typename T, bool isParse>
    T catchClause()
    {
        this->expectKeyword(CatchKeyword);

        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->throwUnexpectedToken(&this->lookahead);
        }

        ParserBlockContext catchBlockContext = openBlock();

        Vector<Scanner::ScannerResult, GCUtil::gc_malloc_allocator<Scanner::ScannerResult>> params;
        RefPtr<Node> param = this->pattern<Parse>(params, KeywordKind::LetKeyword);

        if (this->context->strict && param->type() == Identifier) {
            IdentifierNode* id = (IdentifierNode*)param.get();
            if (this->scanner->isRestrictedWord(id->name())) {
                this->throwError(Messages::StrictCatchVariable);
            }
        }

        this->expect(RightParenthesis);

        bool oldInCatchClause = this->context->inCatchClause;
        this->context->inCatchClause = true;

        bool gotSimplyDeclaredVariableName = param->isIdentifier();

        if (gotSimplyDeclaredVariableName) {
            this->context->catchClauseSimplyDeclaredVariableNames.push_back(param->asIdentifier()->name());
        }

        RefPtr<Node> body;
        if (isParse) {
            body = this->block<Parse>();
        } else {
            this->block<ScanAsVoid>();
        }

        if (gotSimplyDeclaredVariableName) {
            this->context->catchClauseSimplyDeclaredVariableNames.pop_back();
        }

        this->context->inCatchClause = oldInCatchClause;

        closeBlock(catchBlockContext);

        if (isParse) {
            return T(this->finalize(this->createNode(), new CatchClauseNode(param.get(), nullptr, body.get(), catchBlockContext.childLexicalBlockIndex)));
        }

        return T(nullptr);
    }

    template <typename T, bool isParse>
    T finallyClause()
    {
        this->expectKeyword(FinallyKeyword);
        if (isParse) {
            return T(this->block<ParseAs(BlockStatementNode)>());
        }
        return T(nullptr);
    }

    PassRefPtr<TryStatementNode> parseTryStatement()
    {
        this->expectKeyword(TryKeyword);

        RefPtr<BlockStatementNode> block = this->block<ParseAs(BlockStatementNode)>();
        RefPtr<CatchClauseNode> handler;
        if (this->matchKeyword(CatchKeyword)) {
            handler = this->catchClause<ParseAs(CatchClauseNode)>();
        }
        RefPtr<BlockStatementNode> finalizer;
        if (this->matchKeyword(FinallyKeyword)) {
            finalizer = this->finallyClause<ParseAs(BlockStatementNode)>();
        }

        if (!handler && !finalizer) {
            this->throwError(Messages::NoCatchOrFinally);
        }

        return this->finalize(this->createNode(), new TryStatementNode(block.get(), handler.get(), CatchClauseNodeVector(), finalizer.get()));
    }

    void scanTryStatement()
    {
        this->expectKeyword(TryKeyword);

        this->block<ScanAsVoid>();
        bool meetHandler = this->matchKeyword(CatchKeyword);
        if (meetHandler) {
            this->catchClause<ScanAsVoid>();
        }
        bool meetFinalizer = this->matchKeyword(FinallyKeyword);
        if (meetFinalizer) {
            this->finallyClause<ScanAsVoid>();
        }

        if (!meetHandler && !meetFinalizer) {
            this->throwError(Messages::NoCatchOrFinally);
        }
    }

    // ECMA-262 13.16 The debugger statement

    PassRefPtr<StatementNode> parseDebuggerStatement()
    {
        this->throwError("debugger keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        /*
        const node = this->createNode();
        this->expectKeyword('debugger');
        this->consumeSemicolon();
        return this->finalize(node, new Node.DebuggerStatement());
        */
    }

    // ECMA-262 13 Statements
    PassRefPtr<StatementNode> parseStatement(bool allowFunctionDeclaration = true)
    {
        checkRecursiveLimit();
        RefPtr<StatementNode> statement;
        switch (this->lookahead.type) {
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
        case Token::TemplateToken:
        case Token::RegularExpressionToken:
            statement = this->parseExpressionStatement();
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead.valuePunctuatorKind;
            if (value == LeftBrace) {
                statement = this->block<ParseAs(BlockStatementNode)>();
            } else if (value == LeftParenthesis) {
                statement = this->parseExpressionStatement();
            } else if (value == SemiColon) {
                statement = this->emptyStatement<ParseAs(StatementNode)>();
            } else {
                statement = this->parseExpressionStatement();
            }
            break;
        }
        case Token::IdentifierToken:
            statement = this->labelledStatement<ParseAs(StatementNode)>();
            break;

        case Token::KeywordToken:
            switch (this->lookahead.valueKeywordKind) {
            case BreakKeyword:
                statement = this->breakStatement<ParseAs(StatementNode)>();
                break;
            case ContinueKeyword:
                statement = this->continueStatement<ParseAs(StatementNode)>();
                break;
            case DebuggerKeyword:
                statement = asStatementNode(this->parseDebuggerStatement());
                break;
            case DoKeyword:
                statement = this->doWhileStatement<ParseAs(StatementNode)>();
                break;
            case ForKeyword:
                statement = this->forStatement<ParseAs(StatementNode)>();
                break;
            case FunctionKeyword: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(&this->lookahead);
                }
                statement = asStatementNode(this->parseFunctionDeclaration());
                break;
            }
            case IfKeyword:
                statement = this->ifStatement<ParseAs(StatementNode)>();
                break;
            case ReturnKeyword:
                statement = this->returnStatement<ParseAs(StatementNode)>();
                break;
            case SwitchKeyword:
                statement = this->switchStatement<ParseAs(StatementNode)>();
                break;
            case ThrowKeyword:
                statement = this->throwStatement<ParseAs(StatementNode)>();
                break;
            case TryKeyword:
                statement = asStatementNode(this->parseTryStatement());
                break;
            case VarKeyword:
            case LetKeyword:
            case ConstKeyword:
                statement = asStatementNode(this->parseVariableStatement(this->lookahead.valueKeywordKind));
                break;
            case WhileKeyword:
                statement = this->whileStatement<ParseAs(StatementNode)>();
                break;
            case WithKeyword:
                statement = asStatementNode(this->parseWithStatement());
                break;
            case ClassKeyword:
                statement = asStatementNode(this->parseClassDeclaration());
                break;
            default:
                statement = asStatementNode(this->parseExpressionStatement());
                break;
            }
            break;

        default:
            this->throwUnexpectedToken(&this->lookahead);
        }

        return statement;
    }

    void scanStatement(bool allowFunctionDeclaration = true)
    {
        checkRecursiveLimit();
        switch (this->lookahead.type) {
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
        case Token::TemplateToken:
        case Token::RegularExpressionToken:
            this->scanExpressionStatement();
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead.valuePunctuatorKind;
            if (value == LeftBrace) {
                this->block<ScanAsVoid>();
            } else if (value == LeftParenthesis) {
                this->scanExpressionStatement();
            } else if (value == SemiColon) {
                this->emptyStatement<ScanAsVoid>();
            } else {
                this->scanExpressionStatement();
            }
            break;
        }
        case Token::IdentifierToken:
            this->labelledStatement<ScanAsVoid>();
            break;

        case Token::KeywordToken:
            switch (this->lookahead.valueKeywordKind) {
            case BreakKeyword:
                this->breakStatement<ScanAsVoid>();
                break;
            case ContinueKeyword:
                this->continueStatement<ScanAsVoid>();
                break;
            case DebuggerKeyword:
                this->parseDebuggerStatement();
                break;
            case DoKeyword:
                this->doWhileStatement<ScanAsVoid>();
                break;
            case ForKeyword:
                this->forStatement<ScanAsVoid>();
                break;
            case FunctionKeyword: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(&this->lookahead);
                }
                this->parseFunctionDeclaration();
                break;
            }
            case IfKeyword:
                this->ifStatement<ScanAsVoid>();
                break;
            case ReturnKeyword:
                this->returnStatement<ScanAsVoid>();
                break;
            case SwitchKeyword:
                this->switchStatement<ScanAsVoid>();
                break;
            case ThrowKeyword:
                this->throwStatement<ScanAsVoid>();
                break;
            case TryKeyword:
                this->scanTryStatement();
                break;
            case LetKeyword:
            case ConstKeyword:
            case VarKeyword:
                this->scanVariableStatement(this->lookahead.valueKeywordKind);
                break;
            case WhileKeyword:
                this->whileStatement<ScanAsVoid>();
                break;
            case WithKeyword:
                this->parseWithStatement();
                break;
            case ClassKeyword:
                this->scanClassDeclaration();
                break;
            default:
                this->scanExpressionStatement();
                break;
            }
            break;

        default:
            this->throwUnexpectedToken(&this->lookahead);
        }
    }

    PassRefPtr<StatementContainer> parseFunctionParameters()
    {
        RefPtr<StatementContainer> container = StatementContainer::create();

        this->expect(LeftParenthesis);
        ParseFormalParametersResult options;
        size_t paramIndex = 0;
        MetaNode node = this->createNode();
        while (!this->match(RightParenthesis)) {
            bool end = !this->parseFormalParameter(options);

            RefPtr<Node> param = options.params.back();

            switch (param->type()) {
            case Identifier:
            case AssignmentPattern:
            case ArrayPattern:
            case ObjectPattern: {
                RefPtr<InitializeParameterExpressionNode> init = this->finalize(node, new InitializeParameterExpressionNode(param.get(), paramIndex));
                RefPtr<Node> statement = this->finalize(node, new ExpressionStatementNode(init.get()));
                container->appendChild(statement->asStatementNode());
                break;
            }
            case RestElement: {
                RefPtr<Node> statement = this->finalize(node, new ExpressionStatementNode(param.get()));
                container->appendChild(statement->asStatementNode());
                break;
            }
            default: {
                ASSERT_NOT_REACHED();
                break;
            }
            }

            if (end) {
                break;
            }
            paramIndex++;
            this->expect(Comma);
        }
        this->expect(RightParenthesis);

        return container;
    }


    PassRefPtr<BlockStatementNode> parseFunctionBody()
    {
        // only for parsing the body of callee function
        ASSERT(this->config.parseSingleFunction);

        this->lexicalBlockIndex = 0;
        this->lexicalBlockCount = 0;

        this->subCodeBlockIndex = 0;

        this->context->inCatchClause = false;

        MetaNode nodeStart = this->createNode();
        RefPtr<StatementContainer> body = StatementContainer::create();

        this->expect(LeftBrace);
        this->parseDirectivePrologues(body.get());

        this->context->labelSet.clear();
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inFunctionBody = true;

        StatementNode* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            if (this->match(RightBrace)) {
                break;
            }
            referNode = body->appendChild(this->statementListItem<ParseAs(StatementNode)>().get(), referNode);
        }
        this->expect(RightBrace);

        return this->finalize(nodeStart, new BlockStatementNode(body.get(), 0));
    }

    // ECMA-262 14.1 Function Definition

    PassRefPtr<BlockStatementNode> parseFunctionSourceElements()
    {
        if (this->config.parseSingleFunction) {
            // when parsing for function call,
            // parseFunctionSourceElements should parse only for child functions
            ASSERT(this->config.parseSingleFunctionChildIndex > 0);

            size_t realIndex = this->config.parseSingleFunctionChildIndex - 1;
            this->config.parseSingleFunctionChildIndex++;
            InterpretedCodeBlock* currentTarget = this->config.parseSingleFunctionTarget->asInterpretedCodeBlock();
            size_t orgIndex = this->lookahead.start;
            this->expect(LeftBrace);

            StringView src = currentTarget->childBlocks()[realIndex]->bodySrc();
            this->scanner->index = src.length() + orgIndex;

            this->scanner->lineNumber = currentTarget->childBlocks()[realIndex]->sourceElementStart().line;
            this->scanner->lineStart = currentTarget->childBlocks()[realIndex]->sourceElementStart().index - currentTarget->childBlocks()[realIndex]->sourceElementStart().column;
            this->lookahead.lineNumber = this->scanner->lineNumber;
            this->lookahead.lineStart = this->scanner->lineStart;
            this->lookahead.type = Token::PunctuatorToken;
            this->lookahead.valuePunctuatorKind = PunctuatorKind::RightBrace;
            this->expect(RightBrace);
            return this->finalize(this->createNode(), new BlockStatementNode(StatementContainer::create().get(), this->lexicalBlockIndex));
        }

        ASSERT(!this->config.parseSingleFunction);
        LexicalBlockIndex lexicalBlockIndexBefore = this->lexicalBlockIndex;
        LexicalBlockIndex lexicalBlockCountBefore = this->lexicalBlockCount;
        this->lexicalBlockIndex = 0;
        this->lexicalBlockCount = 0;

        size_t oldSubCodeBlockIndex = this->subCodeBlockIndex;
        this->subCodeBlockIndex = 0;

        bool oldInCatchClause = this->context->inCatchClause;
        this->context->inCatchClause = false;

        auto oldCatchClauseSimplyDeclaredVariableNames = std::move(this->context->catchClauseSimplyDeclaredVariableNames);

        MetaNode nodeStart = this->createNode();
        RefPtr<StatementContainer> body = StatementContainer::create();

        this->expect(LeftBrace);
        this->parseDirectivePrologues(body.get());

        auto previousLabelSet = this->context->labelSet;
        bool previousInIteration = this->context->inIteration;
        bool previousInSwitch = this->context->inSwitch;
        bool previousInFunctionBody = this->context->inFunctionBody;

        this->context->labelSet.clear();
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inFunctionBody = true;

        StatementNode* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            if (this->match(RightBrace)) {
                break;
            }
            this->statementListItem<ScanAsVoid>();
        }

        this->expect(RightBrace);

        this->context->inCatchClause = oldInCatchClause;
        this->context->catchClauseSimplyDeclaredVariableNames = std::move(oldCatchClauseSimplyDeclaredVariableNames);

        this->lexicalBlockIndex = lexicalBlockIndexBefore;
        this->lexicalBlockCount = lexicalBlockCountBefore;

        this->subCodeBlockIndex = oldSubCodeBlockIndex;

        this->context->labelSet = previousLabelSet;
        this->context->inIteration = previousInIteration;
        this->context->inSwitch = previousInSwitch;
        this->context->inFunctionBody = previousInFunctionBody;

        scopeContexts.back()->m_lexicalBlockIndexFunctionLocatedIn = lexicalBlockIndexBefore;
        scopeContexts.back()->m_bodyStartLOC.line = nodeStart.line;
        scopeContexts.back()->m_bodyStartLOC.column = nodeStart.column;
        scopeContexts.back()->m_bodyStartLOC.index = nodeStart.index;

        scopeContexts.back()->m_bodyEndLOC.index = this->lastMarker.index;
#ifndef NDEBUG
        scopeContexts.back()->m_bodyEndLOC.line = this->lastMarker.lineNumber;
        scopeContexts.back()->m_bodyEndLOC.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif

        return this->finalize(nodeStart, new BlockStatementNode(StatementContainer::create().get(), this->lexicalBlockIndex));
    }

    template <class FunctionType, bool isFunctionDeclaration>
    PassRefPtr<FunctionType> parseFunction(MetaNode node)
    {
        this->expectKeyword(FunctionKeyword);

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        const char* message = nullptr;
        RefPtr<IdentifierNode> id;
        Scanner::ScannerResult firstRestricted;

        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->allowYield = !isGenerator;
        this->context->inArrowFunction = false;

        if (isFunctionDeclaration || !this->match(LeftParenthesis)) {
            ALLOC_TOKEN(token);
            *token = this->lookahead;
            id = (!isFunctionDeclaration && !this->context->strict && !isGenerator && this->matchKeyword(YieldKeyword)) ? this->parseIdentifierName() : this->parseVariableIdentifier();

            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    this->throwUnexpectedToken(token, Messages::StrictFunctionName);
                }
            } else {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    firstRestricted = *token;
                    message = Messages::StrictFunctionName;
                } else if (this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                    firstRestricted = *token;
                    message = Messages::StrictReservedWord;
                }
            }
        }

        MetaNode paramsStart = this->createNode();
        this->expect(LeftParenthesis);
        AtomicString fnName = id ? id->name() : AtomicString();

        if (isFunctionDeclaration) {
            ASSERT(id);
            addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::VarKeyword);
            insertUsingName(fnName);
            pushScopeContext(fnName);
        } else {
            pushScopeContext(fnName);
            if (id) {
                scopeContexts.back()->insertVarName(fnName, 0, false);
                scopeContexts.back()->insertUsingName(fnName, 0);
            }
        }

        scopeContexts.back()->m_paramsStartLOC.index = paramsStart.index;
        scopeContexts.back()->m_paramsStartLOC.column = paramsStart.column;
        scopeContexts.back()->m_paramsStartLOC.line = paramsStart.line;

        ParseFormalParametersResult formalParameters = this->parseFormalParameters(&firstRestricted);
        Scanner::ScannerResult stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        extractNamesFromFunctionParams(formalParameters);
        bool previousStrict = this->context->strict;
        RefPtr<Node> body = this->parseFunctionSourceElements();
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(&firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->throwUnexpectedToken(&stricted, message);
        }
        this->context->strict = previousStrict;
        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;

        return this->finalize(node, new FunctionType(popScopeContext(node), isGenerator, subCodeBlockIndex));
    }

    PassRefPtr<FunctionDeclarationNode> parseFunctionDeclaration()
    {
        MetaNode node = this->createNode();
        return parseFunction<FunctionDeclarationNode, true>(node);
    }

    PassRefPtr<FunctionExpressionNode> parseFunctionExpression()
    {
        MetaNode node = this->createNode();
        return parseFunction<FunctionExpressionNode, false>(node);
    }

    // ECMA-262 14.1.1 Directive Prologues

    PassRefPtr<Node> parseDirective()
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        StringView* directiveValue;
        bool isLiteral = false;

        MetaNode node = this->createNode();
        RefPtr<Node> expr = this->expression<Parse>();
        if (expr->type() == Literal) {
            isLiteral = true;
            directiveValue = token->valueStringLiteral();
        }
        this->consumeSemicolon();

        if (isLiteral) {
            return this->finalize(node, new DirectiveNode(asExpressionNode(expr).get(), *directiveValue));
        } else {
            return this->finalize(node, new ExpressionStatementNode(expr.get()));
        }
    }

    void parseDirectivePrologues(StatementContainer* container)
    {
        ASSERT(container != nullptr);

        Scanner::ScannerResult firstRestricted;

        ALLOC_TOKEN(token);
        while (true) {
            *token = this->lookahead;
            if (token->type != StringLiteralToken) {
                break;
            }

            RefPtr<Node> statement = this->parseDirective();
            container->appendChild(statement->asStatementNode());

            if (statement->type() != Directive) {
                break;
            }

            DirectiveNode* directive = (DirectiveNode*)statement.get();
            if (token->plain && directive->value().equals("use strict")) {
                this->scopeContexts.back()->m_isStrict = this->context->strict = true;
                if (firstRestricted) {
                    this->throwUnexpectedToken(&firstRestricted, Messages::StrictOctalLiteral);
                }
            } else {
                if (!firstRestricted && token->octal) {
                    firstRestricted = *token;
                }
            }
        }
    }

    // ECMA-262 14.3 Method Definitions

    PassRefPtr<FunctionExpressionNode> parseGetterMethod()
    {
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        this->expect(RightParenthesis);

        bool isGenerator = false;
        ParseFormalParametersResult params;
        bool previousAllowYield = this->context->allowYield;
        this->context->allowYield = false;
        pushScopeContext(AtomicString());
        extractNamesFromFunctionParams(params);
        RefPtr<Node> method = this->parsePropertyMethod(params);
        this->context->allowYield = previousAllowYield;

        scopeContexts.back()->m_paramsStartLOC.index = node.index;
        scopeContexts.back()->m_paramsStartLOC.column = node.column;
        scopeContexts.back()->m_paramsStartLOC.line = node.line;
        return this->finalize(node, new FunctionExpressionNode(popScopeContext(node), isGenerator, this->subCodeBlockIndex));
    }

    PassRefPtr<FunctionExpressionNode> parseSetterMethod()
    {
        MetaNode node = this->createNode();

        ParseFormalParametersResult options;

        bool isGenerator = false;
        bool previousAllowYield = this->context->allowYield;
        this->context->allowYield = false;

        this->expect(LeftParenthesis);

        pushScopeContext(AtomicString());

        if (this->match(RightParenthesis)) {
            this->throwUnexpectedToken(&this->lookahead);
        } else {
            this->parseFormalParameter(options);
        }
        this->expect(RightParenthesis);

        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->inArrowFunction = false;
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        extractNamesFromFunctionParams(options);
        scopeContexts.back()->m_paramsStartLOC.index = node.index;
        scopeContexts.back()->m_paramsStartLOC.column = node.column;
        scopeContexts.back()->m_paramsStartLOC.line = node.line;

        const bool previousStrict = this->context->strict;
        PassRefPtr<Node> method = this->isolateCoverGrammar(&Parser::parseFunctionSourceElements);
        if (this->context->strict && options.firstRestricted) {
            this->throwUnexpectedToken(&options.firstRestricted, options.message);
        }
        if (this->context->strict && options.stricted) {
            this->throwUnexpectedToken(&options.stricted, options.message);
        }
        this->context->strict = previousStrict;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowYield = previousAllowYield;

        return this->finalize(node, new FunctionExpressionNode(popScopeContext(node), isGenerator, this->subCodeBlockIndex));
    }

    PassRefPtr<FunctionExpressionNode> parseGeneratorMethod()
    {
        MetaNode node = this->createNode();
        const bool previousAllowYield = this->context->allowYield;
        this->expect(LeftParenthesis);

        pushScopeContext(AtomicString());
        this->context->allowYield = true;
        ParseFormalParametersResult formalParameters = this->parseFormalParameters();
        this->context->allowYield = false;
        extractNamesFromFunctionParams(formalParameters);
        // Note: Change it to parsePropertyMethod, if possible
        RefPtr<Node> method = this->isolateCoverGrammar(&Parser::parseFunctionSourceElements);
        this->context->allowYield = previousAllowYield;

        scopeContexts.back()->m_paramsStartLOC.index = node.index;
        scopeContexts.back()->m_paramsStartLOC.column = node.column;
        scopeContexts.back()->m_paramsStartLOC.line = node.line;
        return this->finalize(node, new FunctionExpressionNode(popScopeContext(node), true, this->subCodeBlockIndex));
    }

    // ECMA-262 14.4 Generator Function Definitions

    bool isStartOfExpression()
    {
        bool start = true;

        if (this->lookahead.type == Token::PunctuatorToken) {
            switch (this->lookahead.valuePunctuatorKind) {
            case PunctuatorKind::LeftSquareBracket:
            case PunctuatorKind::LeftParenthesis:
            case PunctuatorKind::LeftBrace:
            case PunctuatorKind::Plus:
            case PunctuatorKind::Minus:
            case PunctuatorKind::ExclamationMark:
            case PunctuatorKind::Wave:
            case PunctuatorKind::PlusPlus:
            case PunctuatorKind::MinusMinus:
            case PunctuatorKind::Divide:
            case PunctuatorKind::DivideEqual:
                start = true;
                break;
            default:
                start = false;
                break;
            }
        } else if (this->lookahead.type == Token::KeywordToken) {
            switch (this->lookahead.valueKeywordKind) {
            case KeywordKind::ClassKeyword:
            case KeywordKind::DeleteKeyword:
            case KeywordKind::FunctionKeyword:
            case KeywordKind::LetKeyword:
            case KeywordKind::NewKeyword:
            case KeywordKind::SuperKeyword:
            case KeywordKind::ThisKeyword:
            case KeywordKind::TypeofKeyword:
            case KeywordKind::VoidKeyword:
            case KeywordKind::YieldKeyword:
                start = true;
                break;
            default:
                start = false;
            }
        }

        return start;
    }


    template <typename T, bool isParse>
    T yieldExpression()
    {
        MetaNode node = this->createNode();
        this->expectKeyword(YieldKeyword);

        RefPtr<Node> exprNode;
        bool delegate = false;

        if (!this->hasLineTerminator) {
            const bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = false;
            delegate = this->match(Multiply);

            if (delegate) {
                this->nextToken();
                if (isParse) {
                    exprNode = this->assignmentExpression<Parse>();
                } else {
                    this->assignmentExpression<Scan>();
                }
            } else if (isStartOfExpression()) {
                if (isParse) {
                    exprNode = this->assignmentExpression<Parse>();
                } else {
                    this->assignmentExpression<Scan>();
                }
            }
            this->context->allowYield = previousAllowYield;
        }

        scopeContexts.back()->m_hasYield = true;

        if (isParse) {
            return this->finalize(node, new YieldExpressionNode(exprNode, delegate));
        }

        return ScanExpressionResult(ASTNodeType::YieldExpression);
    }

    // ECMA-262 14.5 Class Definitions

    template <typename T, bool isParse>
    T classElement(RefPtr<FunctionExpressionNode>* constructor, bool hasSuperClass)
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        MetaNode node = this->createNode();

        ClassElementNode::Kind kind = ClassElementNode::Kind::None;

        std::pair<ScanExpressionResult, String*> keyScanResult;
        RefPtr<Node> keyNode;

        RefPtr<FunctionExpressionNode> value;
        bool computed = false;
        bool isStatic = false;

        if (this->match(Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            if (isParse) {
                keyNode = this->parseObjectPropertyKey();
            } else {
                keyScanResult = this->scanObjectPropertyKey();
            }

            if (token->type == Token::KeywordToken && (this->qualifiedPropertyName(&this->lookahead) || this->match(Multiply)) && *token->valueStringLiteral() == "static") {
                *token = this->lookahead;
                isStatic = true;
                computed = this->match(LeftSquareBracket);
                if (this->match(Multiply)) {
                    this->nextToken();
                } else {
                    if (isParse) {
                        keyNode = this->parseObjectPropertyKey();
                    } else {
                        keyScanResult = this->scanObjectPropertyKey();
                    }
                }
            }
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(&this->lookahead);
        if (isParse) {
            if (token->type == Token::IdentifierToken) {
                if (*token->valueStringLiteral() == "get" && lookaheadPropertyKey) {
                    kind = ClassElementNode::Kind::Get;
                    computed = this->match(LeftSquareBracket);
                    keyNode = this->parseObjectPropertyKey();
                    this->context->allowYield = false;
                    value = this->parseGetterMethod();
                } else if (*token->valueStringLiteral() == "set" && lookaheadPropertyKey) {
                    kind = ClassElementNode::Kind::Set;
                    computed = this->match(LeftSquareBracket);
                    keyNode = this->parseObjectPropertyKey();
                    value = this->parseSetterMethod();
                }
            } else if (lookaheadPropertyKey && token->type == Token::PunctuatorToken && token->valuePunctuatorKind == Multiply) {
                kind = ClassElementNode::Kind::Method;
                computed = this->match(LeftSquareBracket);
                keyNode = this->parseObjectPropertyKey();
                value = this->parseGeneratorMethod();
            }
        } else {
            if (token->type == Token::IdentifierToken) {
                if (*token->valueStringLiteral() == "get" && lookaheadPropertyKey) {
                    kind = ClassElementNode::Kind::Get;
                    computed = this->match(LeftSquareBracket);
                    keyScanResult = this->scanObjectPropertyKey();
                    this->context->allowYield = false;
                    value = this->parseGetterMethod();
                } else if (*token->valueStringLiteral() == "set" && lookaheadPropertyKey) {
                    kind = ClassElementNode::Kind::Set;
                    computed = this->match(LeftSquareBracket);
                    keyScanResult = this->scanObjectPropertyKey();
                    value = this->parseSetterMethod();
                }
            } else if (lookaheadPropertyKey && token->type == Token::PunctuatorToken && token->valuePunctuatorKind == Multiply) {
                kind = ClassElementNode::Kind::Method;
                computed = this->match(LeftSquareBracket);
                keyScanResult = this->scanObjectPropertyKey();
                value = this->parseGeneratorMethod();
            }
        }


        if (kind == ClassElementNode::Kind::None && (keyNode || keyScanResult.first != ASTNodeTypeError) && this->match(LeftParenthesis)) {
            kind = ClassElementNode::Kind::Method;
            value = this->parsePropertyMethodFunction();
        }

        if (kind == ClassElementNode::Kind::None) {
            this->throwUnexpectedToken(&this->lookahead);
        }

        if (isParse) {
            if (!computed) {
                if (isStatic && this->isPropertyKey(keyNode.get(), "prototype")) {
                    this->throwUnexpectedToken(token, Messages::StaticPrototype);
                }
                if (!isStatic && this->isPropertyKey(keyNode.get(), "constructor")) {
                    if (kind != ClassElementNode::Kind::Method || value->isGenerator()) {
                        this->throwUnexpectedToken(token, Messages::ConstructorSpecialMethod);
                    }
                    if (*constructor != nullptr) {
                        this->throwUnexpectedToken(token, Messages::DuplicateConstructor);
                    } else {
                        if (!this->config.parseSingleFunction) {
                            lastPoppedScopeContext->m_functionName = escargotContext->staticStrings().constructor;
                            lastPoppedScopeContext->m_isClassConstructor = true;
                            lastPoppedScopeContext->m_isDerivedClassConstructor = hasSuperClass;
                        }
                        *constructor = value;
                        return T(nullptr);
                    }
                }
            }
        } else {
            if (!computed) {
                if (isStatic && this->isPropertyKey(keyScanResult.first, keyScanResult.second, "prototype")) {
                    this->throwUnexpectedToken(token, Messages::StaticPrototype);
                }
                if (!isStatic && this->isPropertyKey(keyScanResult.first, keyScanResult.second, "constructor")) {
                    if (kind != ClassElementNode::Kind::Method || value->isGenerator()) {
                        this->throwUnexpectedToken(token, Messages::ConstructorSpecialMethod);
                    }
                    if (*constructor != nullptr) {
                        this->throwUnexpectedToken(token, Messages::DuplicateConstructor);
                    } else {
                        if (!this->config.parseSingleFunction) {
                            lastPoppedScopeContext->m_functionName = escargotContext->staticStrings().constructor;
                            lastPoppedScopeContext->m_isClassConstructor = true;
                            lastPoppedScopeContext->m_isDerivedClassConstructor = hasSuperClass;
                        }
                        *constructor = value;
                        return T(nullptr);
                    }
                }
            }
        }

        if (!this->config.parseSingleFunction) {
            if (isParse) {
                if (keyNode->type() == Identifier) {
                    lastPoppedScopeContext->m_functionName = keyNode->asIdentifier()->name();
                }
            } else {
                if (keyScanResult.first == Identifier) {
                    lastPoppedScopeContext->m_functionName = keyScanResult.first.string();
                }
            }
            if (isStatic) {
                lastPoppedScopeContext->m_isClassStaticMethod = true;
            } else {
                lastPoppedScopeContext->m_isClassMethod = true;
            }
        }

        if (isParse) {
            return T(this->finalize(node, new ClassElementNode(keyNode.get(), value.get(), kind, computed, isStatic)));
        }
        return T(nullptr);
    }

    template <typename T, bool isParse>
    T classBody(bool hasSuperClass)
    {
        MetaNode node = this->createNode();

        ClassElementNodeVector body;
        RefPtr<FunctionExpressionNode> constructor = nullptr;

        this->expect(LeftBrace);
        while (!this->match(RightBrace)) {
            if (this->match(SemiColon)) {
                this->nextToken();
            } else {
                if (isParse) {
                    PassRefPtr<ClassElementNode> classElement = this->classElement<ParseAs(ClassElementNode)>(&constructor, hasSuperClass);
                    if (classElement != nullptr) {
                        body.push_back(classElement);
                    }
                } else {
                    this->classElement<ScanAsVoid>(&constructor, hasSuperClass);
                }
            }
        }
        this->expect(RightBrace);

        if (isParse) {
            return T(this->finalize(node, new ClassBodyNode(std::move(body), constructor)));
        }
        return T(nullptr);
    }

    template <typename T, bool isParse, class ClassType>
    T classDeclaration(bool identifierIsOptional)
    {
        bool previousStrict = this->context->strict;
        this->context->strict = true;
        this->expectKeyword(ClassKeyword);
        MetaNode node = this->createNode();

        RefPtr<IdentifierNode> idNode;
        AtomicString id;

        if (!identifierIsOptional || this->lookahead.type == Token::IdentifierToken) {
            if (isParse) {
                idNode = this->parseVariableIdentifier();
                id = idNode->name();
            } else {
                id = this->scanVariableIdentifier().string();
            }
        }

        if (!identifierIsOptional && id.string()->length()) {
            addDeclaredNameIntoContext(id, this->lexicalBlockIndex, KeywordKind::LetKeyword);
        }

        RefPtr<Node> superClass;
        if (this->matchKeyword(ExtendsKeyword)) {
            this->nextToken();
            if (isParse) {
                superClass = this->isolateCoverGrammar(&Parser::leftHandSideExpressionAllowCall<Parse>);
            } else {
                this->scanIsolateCoverGrammar(&Parser::leftHandSideExpressionAllowCall<Scan>);
            }
        }

        ParserBlockContext classBlockContext = openBlock();
        if (id.string()->length()) {
            addDeclaredNameIntoContext(id, this->lexicalBlockIndex, KeywordKind::ConstKeyword);
        }

        RefPtr<ClassBodyNode> classBody;
        if (isParse) {
            classBody = this->classBody<ParseAs(ClassBodyNode)>(superClass);
        } else {
            this->classBody<ScanAsVoid>(superClass);
        }

        this->context->strict = previousStrict;
        closeBlock(classBlockContext);

        if (isParse) {
            return T(this->finalize(node, new ClassType(idNode, superClass, classBody.get(), classBlockContext.childLexicalBlockIndex)));
        }
        return T(nullptr);
    }

    PassRefPtr<ClassDeclarationNode> parseClassDeclaration()
    {
        return classDeclaration<ParseAs(ClassDeclarationNode), ClassDeclarationNode>(false);
    }

    void scanClassDeclaration()
    {
        classDeclaration<ScanAsVoid, ClassDeclarationNode>(false);
    }

    PassRefPtr<ClassExpressionNode> parseClassExpression()
    {
        return classDeclaration<ParseAs(ClassExpressionNode), ClassExpressionNode>(true);
    }

    void scanClassExpression()
    {
        classDeclaration<ScanAsVoid, ClassExpressionNode>(true);
    }

    // ECMA-262 15.1 Scripts
    // ECMA-262 15.2 Modules

    PassRefPtr<ProgramNode> parseProgram()
    {
        MetaNode startNode = this->createNode();
        pushScopeContext(new ASTFunctionScopeContext(this->context->strict));
        this->context->allowLexicalDeclaration = true;
        RefPtr<StatementContainer> container = StatementContainer::create();
        this->parseDirectivePrologues(container.get());
        StatementNode* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            referNode = container->appendChild(this->statementListItem<ParseAs(StatementNode)>().get(), referNode);
        }
        scopeContexts.back()->m_bodyStartLOC.line = startNode.line;
        scopeContexts.back()->m_bodyStartLOC.column = startNode.column;
        scopeContexts.back()->m_bodyStartLOC.index = startNode.index;

        MetaNode endNode = this->createNode();
#ifndef NDEBUG
        scopeContexts.back()->m_bodyEndLOC.line = endNode.line;
        scopeContexts.back()->m_bodyEndLOC.column = endNode.column;
#endif
        scopeContexts.back()->m_bodyEndLOC.index = endNode.index;
        return this->finalize(startNode, new ProgramNode(container.get(), scopeContexts.back() /*, this->sourceType*/));
    }

    PassRefPtr<FunctionNode> parseScriptFunction()
    {
        ASSERT(this->config.parseSingleFunction);

        MetaNode node = this->createNode();
        RefPtr<StatementContainer> params = this->parseFunctionParameters();
        RefPtr<BlockStatementNode> body = this->parseFunctionBody();

        return this->finalize(node, new FunctionNode(params.get(), body.get()));
    }

    PassRefPtr<FunctionNode> parseScriptArrowFunction(bool hasArrowParameterPlaceHolder)
    {
        ASSERT(this->config.parseSingleFunction);

        MetaNode node = this->createNode();
        RefPtr<StatementContainer> params;
        RefPtr<BlockStatementNode> body;

        // generate parameter statements
        if (hasArrowParameterPlaceHolder) {
            params = parseFunctionParameters();
        } else {
            RefPtr<Node> param = this->conditionalExpression<Parse>();
            ASSERT(param->type() == Identifier);
            params = StatementContainer::create();
            RefPtr<InitializeParameterExpressionNode> init = this->finalize(node, new InitializeParameterExpressionNode(param.get(), 0));
            RefPtr<Node> statement = this->finalize(node, new ExpressionStatementNode(init.get()));
            params->appendChild(statement->asStatementNode());
        }
        this->expect(Arrow);
        this->context->inArrowFunction = true;

        // generate body statements
        if (this->match(LeftBrace)) {
            body = parseFunctionBody();
        } else {
            MetaNode nodeStart = this->createNode();
            RefPtr<StatementContainer> container = StatementContainer::create();

            auto previousLabelSet = this->context->labelSet;
            bool previousInIteration = this->context->inIteration;
            bool previousInSwitch = this->context->inSwitch;
            bool previousInFunctionBody = this->context->inFunctionBody;

            this->context->labelSet.clear();
            this->context->inIteration = false;
            this->context->inSwitch = false;
            this->context->inFunctionBody = true;

            bool previousStrict = this->context->strict;
            bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = true;

            RefPtr<Node> expr = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);

            container->appendChild(this->finalize(nodeStart, new ReturnStatmentNode(expr.get())), nullptr);

            /*
               if (this->context->strict && list.firstRestricted) {
               this->throwUnexpectedToken(list.firstRestricted, list.message);
               }
               if (this->context->strict && list.stricted) {
               this->throwUnexpectedToken(list.stricted, list.message);
               }
             */

            this->context->strict = previousStrict;
            this->context->allowYield = previousAllowYield;

            this->context->labelSet = previousLabelSet;
            this->context->inIteration = previousInIteration;
            this->context->inSwitch = previousInSwitch;
            this->context->inFunctionBody = previousInFunctionBody;

            body = this->finalize(nodeStart, new BlockStatementNode(container.get()));
        }

        return this->finalize(node, new FunctionNode(params.get(), body.get()));
    }

    // ECMA-262 15.2.2 Imports
    /*
    parseModuleSpecifier(): Node.Literal {
        const node = this.createNode();

        if (this.lookahead.type !== Token.StringLiteral) {
            this.throwError(Messages.InvalidModuleSpecifier);
        }

        const token = this.nextToken();
        const raw = this.getTokenRaw(token);
        return this.finalize(node, new Node.Literal(token.value, raw));
    }

    // import {<foo as bar>} ...;
    parseImportSpecifier(): Node.ImportSpecifier {
        const node = this.createNode();

        let local;
        const imported = this.parseIdentifierName();
        if (this.matchContextualKeyword('as')) {
            this.nextToken();
            local = this.parseVariableIdentifier();
        } else {
            local = imported;
        }

        return this.finalize(node, new Node.ImportSpecifier(local, imported));
    }

    // {foo, bar as bas}
    parseNamedImports(): Node.ImportSpecifier[] {
        this.expect('{');
        const specifiers: Node.ImportSpecifier[] = [];
        while (!this.match('}')) {
            specifiers.push(this.parseImportSpecifier());
            if (!this.match('}')) {
                this.expect(',');
            }
        }
        this.expect('}');

        return specifiers;
    }

    // import <foo> ...;
    parseImportDefaultSpecifier(): Node.ImportDefaultSpecifier {
        const node = this.createNode();
        const local = this.parseIdentifierName();
        return this.finalize(node, new Node.ImportDefaultSpecifier(local));
    }

    // import <* as foo> ...;
    parseImportNamespaceSpecifier(): Node.ImportNamespaceSpecifier {
        const node = this.createNode();

        this.expect('*');
        if (!this.matchContextualKeyword('as')) {
            this.throwError(Messages.NoAsAfterImportNamespace);
        }
        this.nextToken();
        const local = this.parseIdentifierName();

        return this.finalize(node, new Node.ImportNamespaceSpecifier(local));
    }

    parseImportDeclaration(): Node.ImportDeclaration {
        if (this.context.inFunctionBody) {
            this.throwError(Messages.IllegalImportDeclaration);
        }

        const node = this.createNode();
        this.expectKeyword('import');

        let src: Node.Literal;
        let specifiers: Node.ImportDeclarationSpecifier[] = [];
        if (this.lookahead.type === Token.StringLiteral) {
            // import 'foo';
            src = this.parseModuleSpecifier();
        } else {
            if (this.match('{')) {
                // import {bar}
                specifiers = specifiers.concat(this.parseNamedImports());
            } else if (this.match('*')) {
                // import * as foo
                specifiers.push(this.parseImportNamespaceSpecifier());
            } else if (this.isIdentifierName(this.lookahead) && !this.matchKeyword('default')) {
                // import foo
                specifiers.push(this.parseImportDefaultSpecifier());
                if (this.match(',')) {
                    this.nextToken();
                    if (this.match('*')) {
                        // import foo, * as foo
                        specifiers.push(this.parseImportNamespaceSpecifier());
                    } else if (this.match('{')) {
                        // import foo, {bar}
                        specifiers = specifiers.concat(this.parseNamedImports());
                    } else {
                        this.throwUnexpectedToken(this.lookahead);
                    }
                }
            } else {
                this.throwUnexpectedToken(this.nextToken());
            }

            if (!this.matchContextualKeyword('from')) {
                const message = this.lookahead.value ? Messages.UnexpectedToken : Messages.MissingFromClause;
                this.throwError(message, this.lookahead.value);
            }
            this.nextToken();
            src = this.parseModuleSpecifier();
        }
        this.consumeSemicolon();

        return this.finalize(node, new Node.ImportDeclaration(specifiers, src));
    }

    // ECMA-262 15.2.3 Exports

    parseExportSpecifier(): Node.ExportSpecifier {
        const node = this.createNode();

        const local = this.parseIdentifierName();
        let exported = local;
        if (this.matchContextualKeyword('as')) {
            this.nextToken();
            exported = this.parseIdentifierName();
        }

        return this.finalize(node, new Node.ExportSpecifier(local, exported));
    }

    parseExportDeclaration(): Node.ExportDeclaration {
        if (this.context.inFunctionBody) {
            this.throwError(Messages.IllegalExportDeclaration);
        }

        const node = this.createNode();
        this.expectKeyword('export');

        let exportDeclaration;
        if (this.matchKeyword('default')) {
            // export default ...
            this.nextToken();
            if (this.matchKeyword('function')) {
                // export default function foo () {}
                // export default function () {}
                const declaration = this.parseFunctionDeclaration(true);
                exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
            } else if (this.matchKeyword('class')) {
                // export default class foo {}
                const declaration = this.parseClassDeclaration(true);
                exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
            } else {
                if (this.matchContextualKeyword('from')) {
                    this.throwError(Messages.UnexpectedToken, this.lookahead.value);
                }
                // export default {};
                // export default [];
                // export default (1 + 2);
                const declaration = this.match('{') ? this.parseObjectInitializer() :
                    this.match('[') ? this.parseArrayInitializer() : this.assignmentExpression<Parse>();
                this.consumeSemicolon();
                exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
            }

        } else if (this.match('*')) {
            // export * from 'foo';
            this.nextToken();
            if (!this.matchContextualKeyword('from')) {
                const message = this.lookahead.value ? Messages.UnexpectedToken : Messages.MissingFromClause;
                this.throwError(message, this.lookahead.value);
            }
            this.nextToken();
            const src = this.parseModuleSpecifier();
            this.consumeSemicolon();
            exportDeclaration = this.finalize(node, new Node.ExportAllDeclaration(src));

        } else if (this.lookahead.type === Token.Keyword) {
            // export var f = 1;
            let declaration;
            switch (this.lookahead.value) {
                case 'let':
                case 'const':
                    declaration = this.parseLexicalDeclaration({ inFor: false });
                    break;
                case 'var':
                case 'class':
                case 'function':
                    declaration = this.statementListItem<ParseAs(StatementNode)>();
                    break;
                default:
                    this.throwUnexpectedToken(this.lookahead);
            }
            exportDeclaration = this.finalize(node, new Node.ExportNamedDeclaration(declaration, [], null));

        } else {
            const specifiers = [];
            let source = null;
            let isExportFromIdentifier = false;

            this.expect('{');
            while (!this.match('}')) {
                isExportFromIdentifier = isExportFromIdentifier || this.matchKeyword('default');
                specifiers.push(this.parseExportSpecifier());
                if (!this.match('}')) {
                    this.expect(',');
                }
            }
            this.expect('}');

            if (this.matchContextualKeyword('from')) {
                // export {default} from 'foo';
                // export {foo} from 'foo';
                this.nextToken();
                source = this.parseModuleSpecifier();
                this.consumeSemicolon();
            } else if (isExportFromIdentifier) {
                // export {default}; // missing fromClause
                const message = this.lookahead.value ? Messages.UnexpectedToken : Messages.MissingFromClause;
                this.throwError(message, this.lookahead.value);
            } else {
                // export {foo};
                this.consumeSemicolon();
            }
            exportDeclaration = this.finalize(node, new Node.ExportNamedDeclaration(null, specifiers, source));
        }

        return exportDeclaration;
    }
    */
};

RefPtr<ProgramNode> parseProgram(::Escargot::Context* ctx, StringView source, bool strictFromOutside, bool inWith, size_t stackRemain)
{
    Parser parser(ctx, source, stackRemain);
    parser.context->strict = strictFromOutside;
    parser.context->inWith = inWith;
    RefPtr<ProgramNode> nd = parser.parseProgram();
    return nd;
}

RefPtr<FunctionNode> parseSingleFunction(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock, ASTFunctionScopeContext*& scopeContext, size_t stackRemain)
{
    Parser parser(ctx, codeBlock->src(), stackRemain, codeBlock->sourceElementStart());
    parser.trackUsingNames = false;
    parser.context->allowLexicalDeclaration = true;
    parser.config.parseSingleFunction = true;
    parser.config.parseSingleFunctionTarget = codeBlock;
    // when parsing for function call, childindex should be set to index 1
    parser.config.parseSingleFunctionChildIndex = 1;

    scopeContext = new ASTFunctionScopeContext(codeBlock->isStrict());
    parser.pushScopeContext(scopeContext);
    parser.context->allowYield = !codeBlock->isGenerator();

    if (codeBlock->isArrowFunctionExpression()) {
        return parser.parseScriptArrowFunction(codeBlock->hasArrowParameterPlaceHolder());
    }
    return parser.parseScriptFunction();
}

} // namespace esprima
} // namespace Escargot
