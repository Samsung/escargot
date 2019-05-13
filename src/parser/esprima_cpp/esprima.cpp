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

#include "wtfbridge.h"

using namespace JSC::Yarr;
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

struct ParserError : public gc {
    String* description;
    size_t index;
    size_t line;
    size_t col;

    ParserError(size_t index, size_t line, size_t col, String* description)
        : description(description)
        , index(index)
        , line(line)
        , col(col)
    {
    }

    ParserError(size_t index, size_t line, size_t col, const char* description)
        : description(new ASCIIString(description))
        , index(index)
        , line(line)
        , col(col)
    {
    }
};

struct Config : public gc {
    bool range : 1;
    bool loc : 1;
    bool tokens : 1;
    bool comment : 1;
    bool reparseArguments : 1;
    bool parseSingleFunction : 1;
    CodeBlock* parseSingleFunctionTarget;
    SmallValue parseSingleFunctionChildIndex; // use SmallValue for saving index. this reduce memory leak from stack
};


struct Context : public gc {
    bool allowIn : 1;
    bool allowYield : 1;
    bool isAssignmentTarget : 1;
    bool isBindingElement : 1;
    bool inFunctionBody : 1;
    bool inIteration : 1;
    bool inSwitch : 1;
    bool inCatch : 1;
    bool inArrowFunction : 1;
    bool inDirectCatchScope : 1;
    bool inParsingDirective : 1;
    bool inWith : 1;
    bool inLoop : 1;
    bool strict : 1;
    RefPtr<Scanner::ScannerResult> firstCoverInitializedNameError;
    std::vector<std::pair<AtomicString, size_t>> labelSet; // <LabelString, with statement count>
    std::vector<FunctionDeclarationNode*> functionDeclarationsInDirectCatchScope;
};

struct Marker : public gc {
    size_t index;
    size_t lineNumber;
    size_t lineStart;
};

struct MetaNode : public gc {
    size_t index;
    size_t line;
    size_t column;
};


/*
struct ArrowParameterPlaceHolderNode : public gc {
    String* type;
    ExpressionNodeVector params;
};
*/

struct DeclarationOptions : public gc {
    bool inFor;
};

#define Parse PassNode<Node>, true
#define ParseAs(nodeType) PassNode<nodeType>, true
#define Scan ScanExpressionResult, false
#define ScanAsVoid void, false

class Parser : public gc {
public:
    ::Escargot::Context* escargotContext;
    Config config;
    ErrorHandler errorHandlerInstance;
    ErrorHandler* errorHandler;
    Scanner* scanner;
    Scanner scannerInstance;

    /*
    std::unordered_map<IdentifierNode*, RefPtr<Scanner::ScannerResult>,
                       std::hash<IdentifierNode*>, std::equal_to<IdentifierNode*>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<IdentifierNode*, RefPtr<Scanner::ScannerResult>>>>
        nodeExtraInfo;
    */

    enum SourceType {
        Script,
        Module
    };

    SourceType sourceType;
    RefPtr<Scanner::ScannerResult> lookahead;
    bool hasLineTerminator;

    Context contextInstance;
    Context* context;
    std::vector<RefPtr<Scanner::ScannerResult>, gc_allocator_ignore_off_page<RefPtr<Scanner::ScannerResult>>> tokens;
    Marker baseMarker;
    Marker startMarker;
    Marker lastMarker;

    Vector<ASTScopeContext*, gc_allocator_ignore_off_page<ASTScopeContext*>> scopeContexts;
    bool trackUsingNames;
    AtomicString lastUsingName;
    size_t stackLimit;
    AtomicString lastScanIdentifierName;

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
        ScanExpressionResult(ASTNodeType nodeType)
            : nodeType(nodeType)
        {
        }

        ScanExpressionResult()
            : nodeType(ASTNodeTypeError)
        {
        }

        template <typename T>
        ScanExpressionResult(PassRefPtr<T>)
            : nodeType(ASTNodeTypeError)
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

        void setNodeType(ASTNodeType newNodeType)
        {
            nodeType = newNodeType;
        }

    private:
        ASTNodeType nodeType;
    };


    ASTScopeContext fakeContext;

    ASTScopeContext* popScopeContext(const MetaNode& node)
    {
        auto ret = scopeContexts.back();
        scopeContexts.pop_back();
        lastUsingName = AtomicString();
        return ret;
    }

    void pushScopeContext(ASTScopeContext* ctx)
    {
        scopeContexts.push_back(ctx);
        lastUsingName = AtomicString();
    }

    ALWAYS_INLINE void insertUsingName(AtomicString name)
    {
        if (lastUsingName == name) {
            return;
        }
        scopeContexts.back()->insertUsingName(name);
        lastUsingName = name;
    }

    void extractNamesFromFunctionParams(const PatternNodeVector& vector)
    {
        if (this->config.parseSingleFunction)
            return;

        scopeContexts.back()->m_parameters.resizeWithUninitializedValues(vector.size());
        for (size_t i = 0; i < vector.size(); i++) {
            AtomicString id;
            if (vector[i]->isIdentifier()) {
                id = vector[i]->asIdentifier()->name();
            } else if (vector[i]->isDefaultArgument()) {
                id = vector[i]->asDefaultArgument()->left()->asIdentifier()->name();
            } else {
                ASSERT(vector[i]->type() == ASTNodeType::ArrowParameterPlaceHolder);
                scopeContexts.back()->m_parameters.resize(scopeContexts.back()->m_parameters.size() - 1);
                continue;
            }
            scopeContexts.back()->m_parameters[i] = id;
            scopeContexts.back()->insertName(id, true);
        }
    }

    void pushScopeContext(AtomicString functionName)
    {
        if (this->config.parseSingleFunction) {
            fakeContext = ASTScopeContext();
            pushScopeContext(&fakeContext);
            return;
        }
        auto parentContext = scopeContexts.back();
        pushScopeContext(new ASTScopeContext(this->context->strict));
        scopeContexts.back()->m_functionName = functionName;
        scopeContexts.back()->m_inCatch = this->context->inCatch;
        scopeContexts.back()->m_inWith = this->context->inWith;

        if (parentContext) {
            parentContext->m_childScopes.push_back(scopeContexts.back());
        }
    }

    Parser(::Escargot::Context* escargotContext, StringView code, size_t stackRemain, ExtendedNodeLOC startLoc = ExtendedNodeLOC(0, 0, 0))
        : errorHandler(&errorHandlerInstance)
        , scannerInstance(escargotContext, code, this->errorHandler, startLoc.line, startLoc.column)
    {
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
        trackUsingNames = true;
        config.range = false;
        config.loc = false;
        // config.source = String::emptyString;
        config.tokens = false;
        config.comment = false;
        config.parseSingleFunction = false;
        config.parseSingleFunctionTarget = nullptr;
        config.parseSingleFunctionChildIndex = SmallValue((uint32_t)0);
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
        this->lookahead = nullptr;
        this->hasLineTerminator = false;

        this->context = &contextInstance;
        this->context->allowIn = true;
        this->context->allowYield = true;
        this->context->firstCoverInitializedNameError = nullptr;
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        this->context->inFunctionBody = false;
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inCatch = false;
        this->context->inArrowFunction = false;
        this->context->inDirectCatchScope = false;
        this->context->inParsingDirective = false;
        this->context->inWith = false;
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

            PassRefPtr<Scanner::ScannerResult> next = this->scanner->lex();
            this->hasLineTerminator = false;

            if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource())) {
                next->type = Token::KeywordToken;
            }
            this->lookahead = next;
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

        this->errorHandler->throwError(index, line, column, new UTF16String(msg.data(), msg.length()), code);
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
    void throwUnexpectedToken(RefPtr<Scanner::ScannerResult> token, const char* message = nullptr)
    {
        const char* msg;
        if (message) {
            msg = message;
        } else {
            msg = Messages::UnexpectedToken;
        }

        String* value;
        if (token) {
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
            value = new StringView((token->type == Token::TemplateToken) ? token->valueTemplate->raw : token->relatedSource());
        } else {
            value = new ASCIIString("ILLEGAL");
        }

        // msg = msg.replace('%0', value);
        UTF16StringDataNonGCStd msgData;
        msgData.assign(msg, &msg[strlen(msg)]);
        UTF16StringDataNonGCStd valueData = UTF16StringDataNonGCStd(value->toUTF16StringData().data());
        replaceAll(msgData, UTF16StringDataNonGCStd(u"%s"), valueData);

        // if (token && typeof token.lineNumber == 'number') {
        if (token) {
            const size_t index = token->start;
            const size_t line = token->lineNumber;
            const size_t column = token->start - this->lastMarker.lineStart + 1;
            this->errorHandler->throwError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        } else {
            const size_t index = this->lastMarker.index;
            const size_t line = this->lastMarker.lineNumber;
            const size_t column = index - this->lastMarker.lineStart + 1;
            this->errorHandler->throwError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        }
    }

    ALWAYS_INLINE void collectComments()
    {
        this->scanner->scanComments();
    }

    PassRefPtr<Scanner::ScannerResult> nextToken()
    {
        PassRefPtr<Scanner::ScannerResult> token(this->lookahead.release());

        this->lastMarker.index = this->scanner->index;
        this->lastMarker.lineNumber = this->scanner->lineNumber;
        this->lastMarker.lineStart = this->scanner->lineStart;

        this->collectComments();

        this->startMarker.index = this->scanner->index;
        this->startMarker.lineNumber = this->scanner->lineNumber;
        this->startMarker.lineStart = this->scanner->lineStart;

        PassRefPtr<Scanner::ScannerResult> next = this->scanner->lex();
        this->hasLineTerminator = token->lineNumber != next->lineNumber;

        if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource())) {
            next->type = Token::KeywordToken;
        }
        this->lookahead = next;

        /*
        if (this->config.tokens && next.type !== Token.EOF) {
            this->tokens.push(this->convertToken(next));
        }
        */

        return token;
    }

    PassRefPtr<Scanner::ScannerResult> nextRegexToken()
    {
        this->collectComments();

        RefPtr<Scanner::ScannerResult> token = this->scanner->scanRegExp();

        // Prime the next lookahead.
        this->lookahead = token;
        this->nextToken();

        return token.release();
    }

    bool shouldCreateAST()
    {
        return !inProgramParsingAndInFunctionSourceNode() || this->context->inParsingDirective;
    }

    bool inProgramParsingAndInFunctionSourceNode()
    {
        return !this->config.parseSingleFunction && this->scopeContexts.size() > 1;
    }

    MetaNode createNode()
    {
        MetaNode n;
        n.index = this->startMarker.index + this->baseMarker.index;
        n.line = this->startMarker.lineNumber;
        n.column = this->startMarker.index - this->startMarker.lineStart;
        return n;
    }

    MetaNode startNode(const RefPtr<Scanner::ScannerResult>& token)
    {
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
                if (this->context->inArrowFunction) {
                    insertUsingName(this->escargotContext->staticStrings().stringThis);
                }
            }
        } else if (type == WithStatement) {
            scopeContexts.back()->m_hasWith = true;
        } else if (type == YieldExpression) {
            scopeContexts.back()->m_hasYield = true;
        } else if (type == CatchClause) {
            scopeContexts.back()->m_hasCatch = true;
        }

        node->m_loc = NodeLOC(meta.index);
        return adoptRef(node);
    }

    // Expect the next token to match the specified punctuator.
    // If not, an exception will be thrown.
    void expect(PunctuatorKind value)
    {
        PassRefPtr<Scanner::ScannerResult> token = this->nextToken();
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
        PassRefPtr<Scanner::ScannerResult> token = this->nextToken();
        if (token->type != Token::KeywordToken || token->valueKeywordKind != keyword) {
            this->throwUnexpectedToken(token);
        }
    }

    // Return true if the next token matches the specified punctuator.

    bool match(PunctuatorKind value)
    {
        return this->lookahead->type == Token::PunctuatorToken && this->lookahead->valuePunctuatorKind == value;
    }

    // Return true if the next token matches the specified keyword

    bool matchKeyword(KeywordKind keyword)
    {
        return this->lookahead->type == Token::KeywordToken && this->lookahead->valueKeywordKind == keyword;
    }

    // Return true if the next token matches the specified contextual keyword
    // (where an identifier is sometimes a keyword depending on the context)

    bool matchContextualKeyword(KeywordKind keyword)
    {
        return this->lookahead->type == Token::IdentifierToken && this->lookahead->valueKeywordKind == keyword;
    }

    // Return true if the next token is an assignment operator

    bool matchAssign()
    {
        if (this->lookahead->type != Token::PunctuatorToken) {
            return false;
        }
        PunctuatorKind op = this->lookahead->valuePunctuatorKind;

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

    template <typename T>
    PassRefPtr<Node> isolateCoverGrammar(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        PassRefPtr<Node> result = (this->*parseFunction)();
        if (this->context->firstCoverInitializedNameError != nullptr) {
            this->throwUnexpectedToken(this->context->firstCoverInitializedNameError);
        }

        this->context->isBindingElement = previousIsBindingElement;
        this->context->isAssignmentTarget = previousIsAssignmentTarget;
        this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;

        return result;
    }

    template <typename T>
    ScanExpressionResult scanIsolateCoverGrammar(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        ScanExpressionResult ret = (this->*parseFunction)();
        if (this->context->firstCoverInitializedNameError != nullptr) {
            this->throwUnexpectedToken(this->context->firstCoverInitializedNameError);
        }

        this->context->isBindingElement = previousIsBindingElement;
        this->context->isAssignmentTarget = previousIsAssignmentTarget;
        this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;

        return ret;
    }

    template <typename T>
    PassRefPtr<Node> isolateCoverGrammarWithFunctor(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        PassRefPtr<Node> result = parseFunction();
        if (this->context->firstCoverInitializedNameError != nullptr) {
            this->throwUnexpectedToken(this->context->firstCoverInitializedNameError);
        }

        this->context->isBindingElement = previousIsBindingElement;
        this->context->isAssignmentTarget = previousIsAssignmentTarget;
        this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;

        return result;
    }

    template <typename T>
    void scanIsolateCoverGrammarWithFunctor(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        parseFunction();
        if (this->context->firstCoverInitializedNameError != nullptr) {
            this->throwUnexpectedToken(this->context->firstCoverInitializedNameError);
        }

        this->context->isBindingElement = previousIsBindingElement;
        this->context->isAssignmentTarget = previousIsAssignmentTarget;
        this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;
    }

    template <typename T>
    PassRefPtr<Node> inheritCoverGrammar(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        PassRefPtr<Node> result = (this->*parseFunction)();

        this->context->isBindingElement = this->context->isBindingElement && previousIsBindingElement;
        this->context->isAssignmentTarget = this->context->isAssignmentTarget && previousIsAssignmentTarget;
        if (previousFirstCoverInitializedNameError)
            this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;

        return result;
    }

    template <typename T>
    ScanExpressionResult scanInheritCoverGrammar(T parseFunction)
    {
        const bool previousIsBindingElement = this->context->isBindingElement;
        const bool previousIsAssignmentTarget = this->context->isAssignmentTarget;
        RefPtr<Scanner::ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

        this->context->isBindingElement = true;
        this->context->isAssignmentTarget = true;
        this->context->firstCoverInitializedNameError = nullptr;

        this->checkRecursiveLimit();
        ScanExpressionResult result = (this->*parseFunction)();

        this->context->isBindingElement = this->context->isBindingElement && previousIsBindingElement;
        this->context->isAssignmentTarget = this->context->isAssignmentTarget && previousIsAssignmentTarget;
        if (previousFirstCoverInitializedNameError)
            this->context->firstCoverInitializedNameError = previousFirstCoverInitializedNameError;

        return result;
    }

    void consumeSemicolon()
    {
        if (this->match(PunctuatorKind::SemiColon)) {
            this->nextToken();
        } else if (!this->hasLineTerminator) {
            if (this->lookahead->type != Token::EOFToken && !this->match(PunctuatorKind::RightBrace)) {
                this->throwUnexpectedToken(this->lookahead);
            }
            this->lastMarker.index = this->startMarker.index;
            this->lastMarker.lineNumber = this->startMarker.lineNumber;
            this->lastMarker.lineStart = this->startMarker.lineStart;
        }
    }

    IdentifierNode* finishIdentifier(PassRefPtr<Scanner::ScannerResult> token, bool isScopeVariableName)
    {
        IdentifierNode* ret;
        StringView sv = token->valueStringLiteral();
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            ret = new IdentifierNode(this->escargotContext->staticStrings().asciiTable[firstCh]);
        } else {
            if (!token->plain) {
                ret = new IdentifierNode(AtomicString(this->escargotContext, sv.string()));
            } else {
                ret = new IdentifierNode(AtomicString(this->escargotContext, SourceStringView(sv)));
            }
        }

        if (trackUsingNames) {
            insertUsingName(ret->name());
        }
        return ret;
    }

    ScanExpressionResult finishScanIdentifier(PassRefPtr<Scanner::ScannerResult> token, bool isScopeVariableName)
    {
        StringView sv = token->valueStringLiteral();
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            lastScanIdentifierName = this->escargotContext->staticStrings().asciiTable[firstCh];
        } else {
            if (!token->plain) {
                lastScanIdentifierName = AtomicString(this->escargotContext, sv.string());
            } else {
                lastScanIdentifierName = AtomicString(this->escargotContext, SourceStringView(sv));
            }
        }

        if (trackUsingNames) {
            insertUsingName(lastScanIdentifierName);
        }

        return ScanExpressionResult(ASTNodeType::Identifier);
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
        switch (this->lookahead->type) {
        case Token::IdentifierToken:
            if (this->sourceType == SourceType::Module && this->lookahead->valueKeywordKind == AwaitKeyword) {
                this->throwUnexpectedToken(this->lookahead);
            }
            if (isParse) {
                return T(this->finalize(node, finishIdentifier(this->nextToken(), true)));
            }
            return finishScanIdentifier(this->nextToken(), true);
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && this->lookahead->octal) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            {
                PassRefPtr<Scanner::ScannerResult> token = this->nextToken();
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
                        if (shouldCreateAST()) {
                            return T(this->finalize(node, new LiteralNode(token->valueStringLiteralForAST())));
                        }
                        return T(this->finalize(node, new LiteralNode(Value(String::emptyString))));
                    }
                    return ScanExpressionResult(ASTNodeType::Literal);
                }
            }
            break;

        case Token::BooleanLiteralToken: {
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            PassRefPtr<Scanner::ScannerResult> token = this->nextToken();
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
            PassRefPtr<Scanner::ScannerResult> token = this->nextToken();
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
                return T(this->parseTemplateLiteral());
            }
            this->parseTemplateLiteral();
            return ScanExpressionResult(ASTNodeType::TemplateLiteral);

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead->valuePunctuatorKind;
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
                PassRefPtr<Scanner::ScannerResult> token = this->nextRegexToken();
                // raw = this->getTokenRaw(token);
                if (isParse) {
                    return T(this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags)));
                }
                this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags));
                return ScanExpressionResult(ASTNodeType::Literal);
            }
            default:
                this->throwUnexpectedToken(this->nextToken());
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
                throwUnexpectedToken(this->nextToken());
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
                    if (this->context->inArrowFunction) {
                        insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
                    this->nextToken();
                    if (isParse) {
                        return T(this->finalize(node, new ThisExpressionNode()));
                    }
                    return ScanExpressionResult(ASTNodeType::ThisExpression);
                } else if (this->matchKeyword(ClassKeyword)) {
                    if (isParse) {
                        return T(this->parseClassExpression());
                    }
                    this->parseClassExpression();
                    return ScanExpressionResult(ASTNodeType::ASTNodeTypeError);
                } else {
                    this->throwUnexpectedToken(this->nextToken());
                }
            }
            break;
        default:
            this->throwUnexpectedToken(this->nextToken());
        }

        ASSERT_NOT_REACHED();
        if (isParse) {
            return T(PassRefPtr<Node>());
        }
        return ScanExpressionResult();
    }

    struct ParseParameterOptions {
        PatternNodeVector params;
        std::vector<AtomicString, gc_allocator<AtomicString>> paramSet;
        RefPtr<Scanner::ScannerResult> stricted;
        const char* message;
        RefPtr<Scanner::ScannerResult> firstRestricted;
        ParseParameterOptions()
        {
            firstRestricted = nullptr;
            stricted = nullptr;
            message = nullptr;
        }
    };

    void validateParam(ParseParameterOptions& options, const RefPtr<Scanner::ScannerResult>& param, AtomicString name)
    {
        if (this->context->strict) {
            if (this->scanner->isRestrictedWord(name)) {
                options.stricted = param;
                options.message = Messages::StrictParamName;
            }
            if (std::find(options.paramSet.begin(), options.paramSet.end(), name) != options.paramSet.end()) {
                options.stricted = param;
                options.message = Messages::StrictParamDupe;
            }
        } else if (!options.firstRestricted) {
            if (this->scanner->isRestrictedWord(name)) {
                options.firstRestricted = param;
                options.message = Messages::StrictParamName;
            } else if (this->scanner->isStrictModeReservedWord(name)) {
                options.firstRestricted = param;
                options.message = Messages::StrictReservedWord;
            } else if (std::find(options.paramSet.begin(), options.paramSet.end(), name) != options.paramSet.end()) {
                options.stricted = param;
                options.message = Messages::StrictParamDupe;
            }
        }

        options.paramSet.push_back(name);

        if (scopeContexts.back()->m_hasRestElement || scopeContexts.back()->m_hasNonIdentArgument) {
            for (size_t i = 0; i < options.paramSet.size() - 1; i++) {
                // Check if any identifier names are duplicated.
                // Note: using this inner for loop because std::find didn't work for some reason.
                for (size_t j = i + 1; j < options.paramSet.size(); j++) {
                    if (options.paramSet[i] == options.paramSet[j]) {
                        this->throwError("duplicate argument names are not allowed in this context");
                    }
                }
            }
        }
    }

    PassRefPtr<IdentifierNode> parseRestElement(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params)
    {
        this->nextToken();
        if (this->match(LeftBrace)) {
            this->throwError(Messages::ObjectPatternAsRestParameter);
        }
        params.push_back(this->lookahead);

        MetaNode node = this->createNode();

        RefPtr<IdentifierNode> param = this->parseVariableIdentifier();
        if (this->match(Equal)) {
            this->throwError(Messages::DefaultRestParameter);
        }
        if (!this->match(RightParenthesis)) {
            this->throwError(Messages::ParameterAfterRestParameter);
        }

        return this->finalize(node, new IdentifierNode(param.get()->name()));
    }

    template <typename T, bool isParse>
    T pattern(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
    {
        if (this->match(LeftSquareBracket)) {
            this->throwError("Array pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // pattern = this->parseArrayPattern(params, kind);
        } else if (this->match(LeftBrace)) {
            this->throwError("Object pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // pattern = this->parseObjectPattern(params, kind);
        } else {
            if (this->matchKeyword(LetKeyword) && (kind == ConstKeyword || kind == LetKeyword)) {
                this->throwUnexpectedToken(this->lookahead, Messages::UnexpectedToken);
            }
            params.push_back(this->lookahead);
            if (isParse) {
                return T(this->parseVariableIdentifier(kind));
            }

            this->scanVariableIdentifier(kind);
            return ScanExpressionResult(ASTNodeType::Identifier);
        }
    }

    PassRefPtr<Node> parsePatternWithDefault(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        PassRefPtr<Node> pattern = this->pattern<Parse>(params, kind);
        if (this->match(PunctuatorKind::Substitution)) {
            scopeContexts.back()->m_hasNonIdentArgument = true;
            this->nextToken();
            const bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = true;
            PassRefPtr<Node> right = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
            this->context->allowYield = previousAllowYield;
            return this->finalize(this->startNode(startToken), new DefaultArgumentNode(pattern.get(), right.get()));
        }

        return pattern;
    }

    bool parseFormalParameter(ParseParameterOptions& options)
    {
        RefPtr<Node> param;
        bool trackUsingNamesBefore = trackUsingNames;
        trackUsingNames = false;
        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> params;
        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::PeriodPeriodPeriod) {
            RefPtr<IdentifierNode> param = this->parseRestElement(params);
            scopeContexts.back()->m_hasRestElement = true;
            this->validateParam(options, params.back(), param.get()->name());
            options.params.push_back(param);
            trackUsingNames = true;
            return false;
        }

        param = this->parsePatternWithDefault(params);
        for (size_t i = 0; i < params.size(); i++) {
            AtomicString as(this->escargotContext, params[i]->relatedSource());
            this->validateParam(options, params[i], as);
        }
        options.params.push_back(param);
        trackUsingNames = trackUsingNamesBefore;
        return !this->match(PunctuatorKind::RightParenthesis);
    }

    struct ParseFormalParametersResult {
        PatternNodeVector params;
        RefPtr<Scanner::ScannerResult> stricted;
        RefPtr<Scanner::ScannerResult> firstRestricted;
        const char* message;
        bool valid;

        ParseFormalParametersResult()
            : message(nullptr)
            , valid(false)
        {
        }
        ParseFormalParametersResult(PatternNodeVector params, RefPtr<Scanner::ScannerResult> stricted, RefPtr<Scanner::ScannerResult> firstRestricted, const char* message)
            : params(std::move(params))
            , stricted(stricted)
            , firstRestricted(firstRestricted)
            , message(nullptr)
            , valid(true)
        {
        }
    };

    ParseFormalParametersResult parseFormalParameters(RefPtr<Scanner::ScannerResult> firstRestricted = nullptr)
    {
        ParseParameterOptions options;

        options.firstRestricted = firstRestricted;

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

        return ParseFormalParametersResult(options.params, options.stricted, options.firstRestricted, options.message);
    }

    // ECMA-262 12.2.5 Array Initializer

    template <typename T, bool isParse>
    T spreadElement()
    {
        this->expect(PunctuatorKind::PeriodPeriodPeriod);
        MetaNode node = this->createNode();

        RefPtr<Node> arg = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
        if (isParse) {
            return T(this->finalize(node, new SpreadElementNode(arg.get())));
        }
        return ScanExpressionResult(ASTNodeType::SpreadElement);
    }

    template <typename T, bool isParse>
    T arrayInitializer()
    {
        // const elements: Node.ArrayExpressionElement[] = [];
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
            MetaNode node = this->createNode();
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

        pushScopeContext(AtomicString());
        extractNamesFromFunctionParams(params.params);
        const bool previousStrict = this->context->strict;
        PassRefPtr<Node> body = this->isolateCoverGrammar(&Parser::parseFunctionSourceElements);
        if (this->context->strict && params.firstRestricted) {
            this->throwUnexpectedToken(params.firstRestricted, params.message);
        }
        if (this->context->strict && params.stricted) {
            this->throwUnexpectedToken(params.stricted, params.message);
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
        ParseFormalParametersResult params = this->parseFormalParameters();
        RefPtr<Node> method = this->parsePropertyMethod(params);
        this->context->allowYield = previousAllowYield;

        scopeContexts.back()->m_paramsStart.index = node.index;

        return this->finalize(node, new FunctionExpressionNode(AtomicString(), std::move(params.params), method.get(), popScopeContext(node), isGenerator));
    }

    PassRefPtr<Node> parseObjectPropertyKey()
    {
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        MetaNode node = this->createNode();

        RefPtr<Node> key;
        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && token->octal) {
                this->throwUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
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

    std::pair<ScanExpressionResult, bool> scanObjectPropertyKey()
    {
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        ScanExpressionResult key(ASTNodeType::ASTNodeTypeError);
        bool isProto = false;

        switch (token->type) {
        case Token::NumericLiteralToken:
            if (this->context->strict && token->octal) {
                this->throwUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            // const raw = this->getTokenRaw(token);
            if (this->context->inLoop || token->valueNumber == 0)
                this->scopeContexts.back()->insertNumeralLiteral(Value(token->valueNumber));
            key.setNodeType(Literal);
            break;
        case Token::StringLiteralToken:
            isProto = (token->valueStringLiteral() == "__proto__");
            key.setNodeType(Literal);
            break;

        case Token::IdentifierToken:
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::KeywordToken: {
            bool trackUsingNamesBefore = this->trackUsingNames;
            this->trackUsingNames = false;
            key = finishScanIdentifier(token, false);
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

        return std::make_pair(key, isProto);
    }

    bool qualifiedPropertyName(RefPtr<Scanner::ScannerResult> token)
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

    template <typename T, bool isParse>
    T objectProperty(bool& hasProto, std::vector<std::pair<AtomicString, size_t>>& usedNames) //: Node.Property
    {
        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        MetaNode node = this->createNode();

        PropertyNode::Kind kind;
        RefPtr<Node> keyNode; //'': Node.PropertyKey;
        RefPtr<Node> valueNode; //: Node.PropertyValue;
        ScanExpressionResult key;
        AtomicString scanIdentifierName;

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
                scanIdentifierName = lastScanIdentifierName;
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
                isProto = keyValue.second;
                scanIdentifierName = lastScanIdentifierName;
            }
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(this->lookahead);
        bool isGet = false;
        bool isSet = false;

        if (token->type == Token::IdentifierToken && lookaheadPropertyKey) {
            StringView sv = token->valueStringLiteral();
            const auto& d = sv.bufferAccessData();
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
                isProto = keyValue.second;
                scanIdentifierName = lastScanIdentifierName;
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
                isProto = keyValue.second;
                scanIdentifierName = lastScanIdentifierName;
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
                scanIdentifierName = lastScanIdentifierName;
                this->parseGeneratorMethod();
            }
            method = true;
        } else {
            if (isParse) {
                if (!keyNode) {
                    this->throwUnexpectedToken(this->lookahead);
                }
            } else {
                if (key == ASTNodeType::ASTNodeTypeError) {
                    this->throwUnexpectedToken(this->lookahead);
                }
            }
            kind = PropertyNode::Kind::Init;
            if (this->match(PunctuatorKind::Colon)) {
                if (isParse) {
                    isProto = !this->config.parseSingleFunction && this->isPropertyKey(keyNode.get(), "__proto__");
                } else {
                    isProto |= (key == ASTNodeType::Identifier && scanIdentifierName == this->escargotContext->staticStrings().__proto__);
                }

                if (!computed && isProto) {
                    if (hasProto) {
                        this->throwError(Messages::DuplicateProtoProperty);
                    }
                    hasProto = true;
                }
                this->nextToken();

                if (isParse) {
                    valueNode = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
                } else {
                    this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
            } else if (this->match(LeftParenthesis)) {
                if (isParse) {
                    valueNode = this->parsePropertyMethodFunction();
                } else {
                    this->parsePropertyMethodFunction();
                }
                method = true;
            } else if (token->type == Token::IdentifierToken) {
                this->throwError("Property shorthand is not supported yet");
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
                        //value = this->finalize(node, new AssignmentPatternNode(id, init));
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
                this->throwUnexpectedToken(this->nextToken());
            }
        }

        if (!this->config.parseSingleFunction && (isParse ? keyNode->isIdentifier() : key == ASTNodeType::Identifier)) {
            AtomicString as = isParse ? keyNode->asIdentifier()->name() : scanIdentifierName;
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

        if (isParse) {
            // return this->finalize(node, new PropertyNode(kind, key, computed, value, method, shorthand));
            return T(this->finalize(node, new PropertyNode(keyNode.get(), valueNode.get(), kind, computed)));
        }
        return ScanExpressionResult();
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
        ASSERT(this->lookahead->type == Token::TemplateToken);

        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        MetaNode node = this->createNode();
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->raw = token->valueTemplate->raw;
        tm->tail = token->valueTemplate->tail;
        return tm;
    }

    TemplateElement* parseTemplateElement()
    {
        if (!this->match(PunctuatorKind::RightBrace)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        // Re-scan the current token (right brace) as a template string.
        this->lookahead = this->scanner->scanTemplate();

        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        MetaNode node = this->createNode();
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->raw = token->valueTemplate->raw;
        tm->tail = token->valueTemplate->tail;
        return tm;
    }

    PassRefPtr<Node> parseTemplateLiteral()
    {
        MetaNode node = this->createNode();

        ExpressionNodeVector expressions;
        TemplateElementVector* quasis = new (GC) TemplateElementVector;
        quasis->push_back(this->parseTemplateHead());
        while (!quasis->back()->tail) {
            expressions.push_back(this->expression<Parse>());
            TemplateElement* quasi = this->parseTemplateElement();
            quasis->push_back(quasi);
        }

        return this->finalize(node, new TemplateLiteralNode(quasis, std::move(expressions)));
    }
    /*
     // ECMA-262 12.2.10 The Grouping Operator

    reinterpretExpressionAsPattern(expr) {
        switch (expr.type) {
            case Syntax.Identifier:
            case Syntax.MemberExpression:
            case Syntax.RestElement:
            case Syntax.AssignmentPattern:
                break;
            case Syntax.SpreadElement:
                expr.type = Syntax.RestElement;
                this->reinterpretExpressionAsPattern(expr.argument);
                break;
            case Syntax.ArrayExpression:
                expr.type = Syntax.ArrayPattern;
                for (let i = 0; i < expr.elements.length; i++) {
                    if (expr.elements[i] !== null) {
                        this->reinterpretExpressionAsPattern(expr.elements[i]);
                    }
                }
                break;
            case Syntax.ObjectExpression:
                expr.type = Syntax.ObjectPattern;
                for (let i = 0; i < expr.properties.length; i++) {
                    this->reinterpretExpressionAsPattern(expr.properties[i].value);
                }
                break;
            case Syntax.AssignmentExpression:
                expr.type = Syntax.AssignmentPattern;
                delete expr.operator;
                this->reinterpretExpressionAsPattern(expr.left);
                break;
            default:
                // Allow other node type for tolerant parsing.
                break;
        }
    }

    parseGroupExpression(): ArrowParameterPlaceHolderNode | Node.Expression {
        let expr;

        this->expect('(');
        if (this->match(')')) {
            this->nextToken();
            if (!this->match('=>')) {
                this->expect('=>');
            }
            expr = {
                type: ArrowParameterPlaceHolder,
                params: []
            };
        } else {
            const startToken = this->lookahead;
            let params = [];
            if (this->match('...')) {
                expr = this->parseRestElement(params);
                this->expect(')');
                if (!this->match('=>')) {
                    this->expect('=>');
                }
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [expr]
                };
            } else {
                let arrow = false;
                this->context.isBindingElement = true;
                expr = this->inheritCoverGrammar(this->assignmentExpression<Parse>);

                if (this->match(',')) {
                    const expressions = [];

                    this->context.isAssignmentTarget = false;
                    expressions.push(expr);
                    while (this->startMarker.index < this->scanner.length) {
                        if (!this->match(',')) {
                            break;
                        }
                        this->nextToken();

                        if (this->match('...')) {
                            if (!this->context.isBindingElement) {
                                this->throwUnexpectedToken(this->lookahead);
                            }
                            expressions.push(this->parseRestElement(params));
                            this->expect(')');
                            if (!this->match('=>')) {
                                this->expect('=>');
                            }
                            this->context.isBindingElement = false;
                            for (let i = 0; i < expressions.length; i++) {
                                this->reinterpretExpressionAsPattern(expressions[i]);
                            }
                            arrow = true;
                            expr = {
                                type: ArrowParameterPlaceHolder,
                                params: expressions
                            };
                        } else {
                            expressions.push(this->inheritCoverGrammar(this->assignmentExpression<Parse>));
                        }
                        if (arrow) {
                            break;
                        }
                    }
                    if (!arrow) {
                        expr = this->finalize(this->startNode(startToken), new Node.SequenceExpression(expressions));
                    }
                }

                if (!arrow) {
                    this->expect(')');
                    if (this->match('=>')) {
                        if (expr.type === Syntax.Identifier && expr.name === 'yield') {
                            arrow = true;
                            expr = {
                                type: ArrowParameterPlaceHolder,
                                params: [expr]
                            };
                        }
                        if (!arrow) {
                            if (!this->context.isBindingElement) {
                                this->throwUnexpectedToken(this->lookahead);
                            }

                            if (expr.type === Syntax.SequenceExpression) {
                                for (let i = 0; i < expr.expressions.length; i++) {
                                    this->reinterpretExpressionAsPattern(expr.expressions[i]);
                                }
                            } else {
                                this->reinterpretExpressionAsPattern(expr);
                            }

                            const params = (expr.type === Syntax.SequenceExpression ? expr.expressions : [expr]);
                            expr = {
                                type: ArrowParameterPlaceHolder,
                                params: params
                            };
                        }
                    }
                    this->context.isBindingElement = false;
                }
            }
        }

        return expr;
    }
    */
    void reinterpretExpressionAsPattern(Node* expr)
    {
        switch (expr->type()) {
        case ArrayExpression:
            this->throwError("Array pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            /* TODO(ES6) this part is only for es6
            expr.type = Syntax.ArrayPattern;
            for (let i = 0; i < expr.elements.length; i++) {
                if (expr.elements[i] !== null) {
                    this->reinterpretExpressionAsPattern(expr.elements[i]);
                }
            }
            */
            break;
        case ObjectExpression:
            this->throwError("Object pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            /* TODO(ES6) this part is only for es6
            expr.type = Syntax.ObjectPattern;
            for (let i = 0; i < expr.properties.length; i++) {
                this->reinterpretExpressionAsPattern(expr.properties[i].value);
            }
            */
            break;
        default:
            break;
        }
    }

    void scanReinterpretExpressionAsPattern(ScanExpressionResult expr)
    {
        switch (expr) {
        case ArrayExpression:
            this->throwError("Array pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            /* TODO(ES6) this part is only for es6
            expr.type = Syntax.ArrayPattern;
            for (let i = 0; i < expr.elements.length; i++) {
                if (expr.elements[i] !== null) {
                    this->reinterpretExpressionAsPattern(expr.elements[i]);
                }
            }
            */
            break;
        case ObjectExpression:
            this->throwError("Object pattern is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            /* TODO(ES6) this part is only for es6
            expr.type = Syntax.ObjectPattern;
            for (let i = 0; i < expr.properties.length; i++) {
                this->reinterpretExpressionAsPattern(expr.properties[i].value);
            }
            */
            break;
        default:
            break;
        }
    }

    template <typename T, bool isParse>
    T groupExpression()
    {
        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->nextToken();
            if (!this->match(Arrow)) {
                this->expect(Arrow);
            }

            //TODO
            if (isParse) {
                return T(this->finalize(this->startNode(this->lookahead), new ArrowParameterPlaceHolderNode()));
            }
            return ScanExpressionResult(ASTNodeType::ArrowParameterPlaceHolder);
        }

        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        this->context->isBindingElement = true;
        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

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

                if (isParse) {
                    expressions.push_back(this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>));
                } else {
                    this->scanInheritCoverGrammar(&Parser::assignmentExpression<Scan>);
                }
            }

            if (isParse) {
                exprNode = this->finalize(this->startNode(startToken), new SequenceExpressionNode(std::move(expressions)));
            } else {
                expr.setNodeType(ASTNodeType::SequenceExpression);
            }
        }

        this->expect(RightParenthesis);
        //TODO Scanning has not been implemented yet
        if (this->match(Arrow) && isParse) {
            bool arrow = false;

            if (exprNode->type() == Identifier && exprNode->asIdentifier()->name() == "yield") {
                this->throwError("Yield property is not supported yet");
                RELEASE_ASSERT_NOT_REACHED();
                /*
                arrow = true;
                expr = this->finalize(this->startNode(startToken), new ArrowParameterPlaceHolderNode());
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [expr],
                };
                */
            }

            if (!arrow) {
                if (!this->context->isBindingElement) {
                    this->throwUnexpectedToken(this->lookahead);
                }

                if (exprNode->type() == SequenceExpression) {
                    const ExpressionNodeVector& expressions = ((SequenceExpressionNode*)exprNode.get())->expressions();
                    for (size_t i = 0; i < expressions.size(); i++) {
                        this->reinterpretExpressionAsPattern(expressions[i].get());
                    }
                } else {
                    this->reinterpretExpressionAsPattern(exprNode.get());
                }

                //TODO
                ExpressionNodeVector params;
                if (exprNode->type() == SequenceExpression) {
                    params = ((SequenceExpressionNode*)exprNode.get())->expressions();
                } else {
                    params.push_back(exprNode);
                }

                exprNode = this->finalize(this->startNode(this->lookahead), new ArrowParameterPlaceHolderNode(std::move(params)));
            }
            this->context->isBindingElement = false;
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

    bool isIdentifierName(RefPtr<Scanner::ScannerResult> token)
    {
        return token->type == Token::IdentifierToken || token->type == Token::KeywordToken || token->type == Token::BooleanLiteralToken || token->type == Token::NullLiteralToken;
    }

    PassRefPtr<IdentifierNode> parseIdentifierName()
    {
        MetaNode node = this->createNode();
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(token);
        }
        return this->finalize(node, finishIdentifier(token, true));
    }

    ScanExpressionResult scanIdentifierName()
    {
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
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
            if (this->lookahead->type == Token::IdentifierToken && this->context->inFunctionBody && this->lookahead->relatedSource() == "target") {
                // TODO
                RefPtr<IdentifierNode> property = this->parseIdentifierName();
                this->throwError("Meta property is not supported yet");
                RELEASE_ASSERT_NOT_REACHED();
                // expr = new Node.MetaProperty(id, property);
            } else {
                this->throwUnexpectedToken(this->lookahead);
            }
        }

        if (isParse) {
            RefPtr<Node> callee = this->isolateCoverGrammar(&Parser::leftHandSideExpression<Parse>);
            ArgumentVector args;
            if (this->match(LeftParenthesis)) {
                args = this->parseArguments();
            }
            Node* expr = new NewExpressionNode(callee.get(), std::move(args));
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            MetaNode node = this->createNode();
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        RefPtr<Node> exprNode;
        ScanExpressionResult expr;

        if (this->context->inFunctionBody && this->matchKeyword(SuperKeyword)) {
            this->nextToken();
            this->throwError("super keyword is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // MetaNode node = this->createNode();
            // expr = this->finalize(node, new Node.Super());
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(this->lookahead);
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
            bool isPunctuatorTokenLookahead = this->lookahead->type == Token::PunctuatorToken;
            if (isPunctuatorTokenLookahead) {
                if (this->lookahead->valuePunctuatorKind == Period) {
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
                } else if (this->lookahead->valuePunctuatorKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    if (isParse) {
                        exprNode = this->finalize(this->startNode(startToken), new CallExpressionNode(exprNode.get(), this->parseArguments()));
                    } else {
                        testCalleeExpressionInScan(expr);
                        this->scanArguments();
                        expr.setNodeType(ASTNodeType::CallExpression);
                    }
                } else if (this->lookahead->valuePunctuatorKind == LeftSquareBracket) {
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
            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                // Note: exprNode is nullptr for Scan
                exprNode = this->convertTaggedTempleateExpressionToCallExpression(this->startNode(startToken), this->finalize(this->startNode(startToken), new TaggedTemplateExpressionNode(exprNode.get(), quasi.get())));
                if (!isParse) {
                    expr.setNodeType(exprNode->type());
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
        if (callee == ASTNodeType::Identifier && lastScanIdentifierName == escargotContext->staticStrings().eval) {
            scopeContexts.back()->m_hasEval = true;
            if (this->context->inArrowFunction) {
                insertUsingName(this->escargotContext->staticStrings().stringThis);
            }
        }
    }

    template <typename T, bool isParse>
    T super()
    {
        MetaNode node = this->createNode();

        this->expectKeyword(SuperKeyword);
        if (!this->match(LeftSquareBracket) && !this->match(Period)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        this->throwError("super keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        if (isParse) {
            // return this->finalize(node, new Node.Super());
            return PassNode<Node>(nullptr);
        }
        return ScanExpressionResult(ASTNodeType::ASTNodeTypeError);
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

        MetaNode node = this->startNode(this->lookahead);

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
            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                // Note: exprNode is nullptr for Scan
                exprNode = this->convertTaggedTempleateExpressionToCallExpression(node, this->finalize(node, new TaggedTemplateExpressionNode(exprNode.get(), quasi.get())).get());
                if (!isParse) {
                    expr.setNodeType(exprNode->type());
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
                String* str = new StringView((*templateLiteral->quasis())[i]->raw);
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            RefPtr<Scanner::ScannerResult> token = this->nextToken();

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
                if (this->context->strict && expr == ASTNodeType::Identifier && this->scanner->isRestrictedWord(lastScanIdentifierName)) {
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
            if (!this->hasLineTerminator && this->lookahead->type == Token::PunctuatorToken && (this->match(PlusPlus) || this->match(MinusMinus))) {
                bool isPlus = this->match(PlusPlus);
                if (isParse && this->context->strict && exprNode->isIdentifier() && this->scanner->isRestrictedWord(((IdentifierNode*)exprNode.get())->name())) {
                    this->throwError(Messages::StrictLHSPostfix);
                }
                if (!isParse && this->context->strict && expr == ASTNodeType::Identifier && this->scanner->isRestrictedWord(lastScanIdentifierName)) {
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
        if (this->lookahead->type == Token::PunctuatorToken) {
            auto punctuatorsKind = this->lookahead->valuePunctuatorKind;
            RefPtr<Node> exprNode;

            if (punctuatorsKind == Plus) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(this->lookahead);
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
                    MetaNode node = this->startNode(this->lookahead);
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
                    MetaNode node = this->startNode(this->lookahead);
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
                    MetaNode node = this->startNode(this->lookahead);
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
                return ScanExpressionResult(ASTNodeType::UnaryExpressionBitwiseNot);
            }
        }

        bool isKeyword = this->lookahead->type == Token::KeywordToken;

        if (isKeyword) {
            RefPtr<Node> exprNode;

            if (this->lookahead->valueKeywordKind == DeleteKeyword) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(this->lookahead);
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
            } else if (this->lookahead->valueKeywordKind == VoidKeyword) {
                this->nextToken();
                if (isParse) {
                    MetaNode node = this->startNode(this->lookahead);
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
            } else if (this->lookahead->valueKeywordKind == TypeofKeyword) {
                this->nextToken();

                if (isParse) {
                    MetaNode node = this->startNode(this->lookahead);
                    RefPtr<Node> subExpr = this->inheritCoverGrammar(&Parser::unaryExpression<Parse>);

                    if (subExpr->isIdentifier()) {
                        AtomicString s = subExpr->asIdentifier()->name();
                        if (!this->scopeContexts.back()->hasName(s)) {
                            this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                        }
                    }
                    exprNode = this->finalize(node, new UnaryExpressionTypeOfNode(subExpr.get()));
                } else {
                    ScanExpressionResult subExpr = this->scanInheritCoverGrammar(&Parser::unaryExpression<Scan>);

                    if (subExpr == ASTNodeType::Identifier) {
                        if (!this->scopeContexts.back()->hasName(lastScanIdentifierName)) {
                            this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                        }
                    }
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
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

    int binaryPrecedence(const RefPtr<Scanner::ScannerResult>& token)
    {
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::parseExponentiationExpression);

        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> tokenKeeper;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> markers;
            markers.push_back(startToken);
            markers.push_back(this->lookahead);
            RefPtr<Node> left = expr;
            RefPtr<Node> right = this->isolateCoverGrammar(&Parser::parseExponentiationExpression);

            std::vector<void*, GCUtil::gc_malloc_ignore_off_page_allocator<void*>> stack;
            left->ref();
            stack.push_back(left.get());
            token->ref();
            stack.push_back(token.get());
            right->ref();
            stack.push_back(right.get());

            while (true) {
                prec = this->binaryPrecedence(this->lookahead);
                if (prec <= 0) {
                    break;
                }

                // Reduce: make a binary expression from the three topmost entries.
                while ((stack.size() > 2) && (prec <= ((Scanner::ScannerResult*)stack[stack.size() - 2])->prec)) {
                    right = (Node*)stack.back();
                    right->deref();
                    stack.pop_back();
                    RefPtr<Scanner::ScannerResult> operator_ = (Scanner::ScannerResult*)stack.back();
                    operator_->deref();
                    stack.pop_back();
                    left = (Node*)stack.back();
                    left->deref();
                    stack.pop_back();
                    markers.pop_back();
                    MetaNode node = this->startNode(markers.back());
                    auto e = this->finalize(node, finishBinaryExpression(left.get(), right.get(), operator_.get()));
                    e->ref();
                    stack.push_back(e.get());
                }

                // Shift.
                token = this->nextToken();
                token->prec = prec;
                token->ref();
                stack.push_back(token.get());
                markers.push_back(this->lookahead);
                auto e = this->isolateCoverGrammar(&Parser::parseExponentiationExpression);
                e->ref();
                stack.push_back(e.get());
            }

            // Final reduce to clean-up the stack.
            size_t i = stack.size() - 1;
            expr = (Node*)stack[i];
            expr->deref();
            markers.pop_back();
            while (i > 1) {
                MetaNode node = this->startNode(markers.back());
                expr = this->finalize(node, finishBinaryExpression((Node*)stack[i - 2], expr.get(), (Scanner::ScannerResult*)stack[i - 1]));
                ((Node*)stack[i - 2])->deref();
                ((Scanner::ScannerResult*)stack[i - 1])->deref();
                i -= 2;
            }
            RELEASE_ASSERT(i == 0);
        }

        return expr.release();
    }

    ScanExpressionResult scanBinaryExpressions()
    {
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::scanExponentiationExpression);

        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> tokenKeeper;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            ScanExpressionResult left = expr;
            ScanExpressionResult right = this->scanIsolateCoverGrammar(&Parser::scanExponentiationExpression);

            std::vector<void*, GCUtil::gc_malloc_ignore_off_page_allocator<void*>> stack;
            stack.push_back(new ScanExpressionResult(left));
            token->ref();
            stack.push_back(token.get());
            stack.push_back(new ScanExpressionResult(right));

            while (true) {
                prec = this->binaryPrecedence(this->lookahead);
                if (prec <= 0) {
                    break;
                }

                // Reduce: make a binary expression from the three topmost entries.
                while ((stack.size() > 2) && (prec <= ((Scanner::ScannerResult*)stack[stack.size() - 2])->prec)) {
                    right = *((ScanExpressionResult*)stack.back());
                    delete (ScanExpressionResult*)stack.back();
                    stack.pop_back();
                    RefPtr<Scanner::ScannerResult> operator_ = (Scanner::ScannerResult*)stack.back();
                    operator_->deref();
                    stack.pop_back();
                    left = *((ScanExpressionResult*)stack.back());
                    delete (ScanExpressionResult*)stack.back();
                    stack.pop_back();
                    auto e = scanBinaryExpression(left, right, operator_.get());
                    stack.push_back(new ScanExpressionResult(e));
                }

                // Shift.
                token = this->nextToken();
                token->prec = prec;
                token->ref();
                stack.push_back(token.get());
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
                ((Scanner::ScannerResult*)stack[i - 1])->deref();
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
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
                exprNode = this->parseYieldExpression();
            } else {
                expr = this->scanYieldExpression();
            }
        } else {
            RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
            RefPtr<Scanner::ScannerResult> token = startToken;
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

                ParseFormalParametersResult list;
                pushScopeContext(AtomicString());

                if (type == Identifier) {
                    list = ParseFormalParametersResult();
                    list.params.push_back(exprNode);
                    list.valid = true;
                } else {
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();
                    this->expect(LeftParenthesis);

                    list = this->parseFormalParameters();
                    scopeContexts.back()->m_paramsStart.index = startNode.index;
                }

                if (isParse) {
                    exprNode.release();
                }

                if (list.valid) {
                    if (this->hasLineTerminator) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    this->context->firstCoverInitializedNameError = nullptr;

                    extractNamesFromFunctionParams(list.params);
                    bool previousStrict = this->context->strict;
                    bool previousAllowYield = this->context->allowYield;
                    bool previousInArrowFunction = this->context->inArrowFunction;
                    this->context->allowYield = true;
                    this->context->inArrowFunction = true;

                    MetaNode node = this->startNode(startToken);
                    MetaNode nodeStart = this->createNode();

                    this->expect(Arrow);
                    RefPtr<Node> body = this->match(LeftBrace) ? this->parseFunctionSourceElements() : this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
                    bool isExpression = body->type() != BlockStatement;
                    if (isExpression) {
                        if (this->config.parseSingleFunction) {
                            ASSERT(this->config.parseSingleFunctionChildIndex.asUint32());
                            this->config.parseSingleFunctionChildIndex = SmallValue(this->config.parseSingleFunctionChildIndex.asUint32() + 1);
                        }
                        scopeContexts.back()->m_locStart.line = nodeStart.line;
                        scopeContexts.back()->m_locStart.column = nodeStart.column;
                        scopeContexts.back()->m_locStart.index = nodeStart.index;

                        scopeContexts.back()->m_locEnd.index = this->lastMarker.index;
#ifndef NDEBUG
                        scopeContexts.back()->m_locEnd.line = this->lastMarker.lineNumber;
                        scopeContexts.back()->m_locEnd.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif
                    }

                    if (this->context->strict && list.firstRestricted) {
                        this->throwUnexpectedToken(list.firstRestricted, list.message);
                    }
                    if (this->context->strict && list.stricted) {
                        this->throwUnexpectedToken(list.stricted, list.message);
                    }

                    exprNode = this->finalize(node, new ArrowFunctionExpressionNode(std::move(list.params), body.get(), popScopeContext(node), isExpression)); //TODO
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
                            name = lastScanIdentifierName;
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

                    token = this->nextToken();
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
                    this->context->firstCoverInitializedNameError = nullptr;
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
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
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
        if (this->lookahead->type == KeywordToken) {
            switch (this->lookahead->valueKeywordKind) {
            case FunctionKeyword:
                if (isParse) {
                    statement = this->parseFunctionDeclaration();
                } else {
                    this->parseFunctionDeclaration();
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

        /*
         if (this->lookahead.type === Token.Keyword) {
             switch (this->lookahead.value) {
                 case 'export':
                     if (this->sourceType !== 'module') {
                         this->throwUnexpectedToken(this->lookahead, Messages.IllegalExportDeclaration);
                     }
                     statement = this->parseExportDeclaration();
                     break;
                 case 'import':
                     if (this->sourceType !== 'module') {
                         this->throwUnexpectedToken(this->lookahead, Messages.IllegalImportDeclaration);
                     }
                     statement = this->parseImportDeclaration();
                     break;
                 case 'const':
                     statement = this->parseLexicalDeclaration({ inFor: false });
                     break;
                 case 'function':
                     statement = this->parseFunctionDeclaration();
                     break;
                 case 'class':
                     statement = this->parseClassDeclaration();
                     break;
                 case 'let':
                     statement = this->isLexicalDeclaration() ? this->parseLexicalDeclaration({ inFor: false }) : this->parseStatement();
                     break;
                 default:
                     statement = this->parseStatement();
                     break;
             }
         } else {
             statement = this->parseStatement();
         }*/

        if (isParse) {
            return T(statement.release());
        }
        return T(nullptr);
    }

    template <typename T, bool isParse>
    T block()
    {
        this->expect(LeftBrace);
        RefPtr<StatementContainer> block;
        StatementNode* referNode = nullptr;

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

        if (isParse) {
            MetaNode node = this->createNode();
            return T(this->finalize(node, new BlockStatementNode(block.get())));
        }
        return T(nullptr);
    }

    /*
         // ECMA-262 13.3.1 Let and Const Declarations

    parseLexicalBinding(kind: string, options): Node.VariableDeclarator {
        const node = this->createNode();
        let params = [];
        const id = this->parsePattern(params, kind);

        // ECMA-262 12.2.1
        if (this->context.strict && id.type === Syntax.Identifier) {
            if (this->scanner.isRestrictedWord((<Node.Identifier>(id)).name)) {
                this->throwError(Messages.StrictVarName);
            }
        }

        let init = null;
        if (kind === 'const') {
            if (!this->matchKeyword('in') && !this->matchContextualKeyword('of')) {
                this->expect('=');
                init = this->isolateCoverGrammar(this->assignmentExpression<Parse>);
            }
        } else if ((!options.inFor && id.type !== Syntax.Identifier) || this->match('=')) {
            this->expect('=');
            init = this->isolateCoverGrammar(this->assignmentExpression<Parse>);
        }

        return this->finalize(node, new Node.VariableDeclarator(id, init));
    }

    parseBindingList(kind: string, options): Node.VariableDeclarator[] {
        let list = [this->parseLexicalBinding(kind, options)];

        while (this->match(',')) {
            this->nextToken();
            list.push(this->parseLexicalBinding(kind, options));
        }

        return list;
    }

    isLexicalDeclaration(): boolean {
        const previousIndex = this->scanner.index;
        const previousLineNumber = this->scanner.lineNumber;
        const previousLineStart = this->scanner.lineStart;
        this->collectComments();
        const next = <any>this->scanner.lex();
        this->scanner.index = previousIndex;
        this->scanner.lineNumber = previousLineNumber;
        this->scanner.lineStart = previousLineStart;

        return (next.type === Token.Identifier) ||
            (next.type === Token.Punctuator && next.value === '[') ||
            (next.type === Token.Punctuator && next.value === '{') ||
            (next.type === Token.Keyword && next.value === 'let') ||
            (next.type === Token.Keyword && next.value === 'yield');
    }

    parseLexicalDeclaration(options): Node.VariableDeclaration {
        const node = this->createNode();
        const kind = this->nextToken().value;
        assert(kind === 'let' || kind === 'const', 'Lexical declaration must be either let or const');

        const declarations = this->parseBindingList(kind, options);
        this->consumeSemicolon();

        return this->finalize(node, new Node.VariableDeclaration(declarations, kind));
    }

    // ECMA-262 13.3.3 Destructuring Binding Patterns

    parseBindingRestElement(params, kind: string): Node.RestElement {
        const node = this->createNode();
        this->expect('...');
        params.push(this->lookahead);
        const arg = this->parseVariableIdentifier(kind);
        return this->finalize(node, new Node.RestElement(arg));
    }


    parseArrayPattern(params, kind: string): Node.ArrayPattern {
        const node = this->createNode();

        this->expect('[');
        const elements: Node.ArrayPatternElement[] = [];
        while (!this->match(']')) {
            if (this->match(',')) {
                this->nextToken();
                elements.push(null);
            } else {
                if (this->match('...')) {
                    elements.push(this->parseBindingRestElement(params, kind));
                    break;
                } else {
                    elements.push(this->parsePatternWithDefault(params, kind));
                }
                if (!this->match(']')) {
                    this->expect(',');
                }
            }

        }
        this->expect(']');

        return this->finalize(node, new Node.ArrayPattern(elements));
    }

    parsePropertyPattern(params, kind: string): Node.Property {
        const node = this->createNode();

        let computed = false;
        let shorthand = false;
        const method = false;

        let key: Node.PropertyKey;
        let value: Node.PropertyValue;

        if (this->lookahead.type === Token.Identifier) {
            const keyToken = this->lookahead;
            key = this->parseVariableIdentifier();
            const init = this->finalize(node, new Node.Identifier(keyToken.value));
            if (this->match('=')) {
                params.push(keyToken);
                shorthand = true;
                this->nextToken();
                const expr = this->assignmentExpression<Parse>();
                value = this->finalize(this->startNode(keyToken), new Node.AssignmentPattern(init, expr));
            } else if (!this->match(':')) {
                params.push(keyToken);
                shorthand = true;
                value = init;
            } else {
                this->expect(':');
                value = this->parsePatternWithDefault(params, kind);
            }
        } else {
            computed = this->match('[');
            key = this->parseObjectPropertyKey();
            this->expect(':');
            value = this->parsePatternWithDefault(params, kind);
        }

        return this->finalize(node, new Node.Property('init', key, computed, value, method, shorthand));
    }

    parseObjectPattern(params, kind: string): Node.ObjectPattern {
        const node = this->createNode();
        const properties: Node.Property[] = [];

        this->expect('{');
        while (!this->match('}')) {
            properties.push(this->parsePropertyPattern(params, kind));
            if (!this->match('}')) {
                this->expect(',');
            }
        }
        this->expect('}');

        return this->finalize(node, new Node.ObjectPattern(properties));
    }

    parsePattern(params, kind?: string): Node.BindingIdentifier | Node.BindingPattern {
        let pattern;

        if (this->match('[')) {
            pattern = this->parseArrayPattern(params, kind);
        } else if (this->match('{')) {
            pattern = this->parseObjectPattern(params, kind);
        } else {
            if (this->matchKeyword('let') && (kind === 'const' || kind === 'let')) {
                this->throwUnexpectedToken(this->lookahead, Messages.UnexpectedToken);
            }
            params.push(this->lookahead);
            pattern = this->parseVariableIdentifier(kind);
        }

        return pattern;
    }

    parsePatternWithDefault(params, kind?: string): Node.AssignmentPattern | Node.BindingIdentifier | Node.BindingPattern {
        const startToken = this->lookahead;

        let pattern = this->parsePattern(params, kind);
        if (this->match('=')) {
            this->nextToken();
            const previousAllowYield = this->context.allowYield;
            this->context.allowYield = true;
            const right = this->isolateCoverGrammar(this->assignmentExpression<Parse>);
            this->context.allowYield = previousAllowYield;
            pattern = this->finalize(this->startNode(startToken), new Node.AssignmentPattern(pattern, right));
        }

        return pattern;
    }
    */

    // ECMA-262 13.3.2 Variable Statement
    PassRefPtr<IdentifierNode> parseVariableIdentifier(KeywordKind kind = KeywordKindEnd)
    {
        MetaNode node = this->createNode();

        RefPtr<Scanner::ScannerResult> token = this->nextToken();
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

        return this->finalize(node, finishIdentifier(token, true));
    }

    void scanVariableIdentifier(KeywordKind kind = KeywordKindEnd)
    {
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
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

        finishScanIdentifier(token, true);
    }

    struct DeclarationOptions {
        bool inFor;
    };

    template <typename T, bool isParse>
    T variableDeclaration(DeclarationOptions& options)
    {
        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> params;
        RefPtr<Node> idNode;
        ScanExpressionResult id;
        bool isIdentifier;
        AtomicString name;

        if (isParse) {
            idNode = this->pattern<Parse>(params, VarKeyword);
            isIdentifier = (idNode->type() == Identifier);
            if (isIdentifier) {
                name = ((IdentifierNode*)idNode.get())->name();
            }
        } else {
            id = this->pattern<Scan>(params, VarKeyword);
            isIdentifier = (id == Identifier);
            if (isIdentifier) {
                name = lastScanIdentifierName;
            }
        }

        // ECMA-262 12.2.1
        if (this->context->strict && isIdentifier && this->scanner->isRestrictedWord(name)) {
            this->throwError(Messages::StrictVarName);
        }

        if (isIdentifier && !this->config.parseSingleFunction) {
            this->scopeContexts.back()->insertName(name, true);
        }

        RefPtr<Node> initNode;
        if (this->match(Substitution)) {
            this->nextToken();
            if (isParse) {
                initNode = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);
            } else {
                this->scanIsolateCoverGrammar(&Parser::assignmentExpression<Scan>);
            }
        } else if (!isIdentifier && !options.inFor) {
            this->expect(Substitution);
        }

        if (isParse) {
            MetaNode node = this->createNode();
            return T(this->finalize(node, new VariableDeclaratorNode(idNode.get(), initNode.get())));
        }
        return T(nullptr);
    }

    VariableDeclaratorVector parseVariableDeclarationList(DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;

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
        size_t listSize = 0;

        this->variableDeclaration<ScanAsVoid>(opt);
        while (this->match(Comma)) {
            this->nextToken();
            this->variableDeclaration<ScanAsVoid>(opt);
        }
    }

    PassRefPtr<VariableDeclarationNode> parseVariableStatement()
    {
        this->expectKeyword(VarKeyword);
        MetaNode node = this->createNode();
        DeclarationOptions opt;
        opt.inFor = false;
        VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
        this->consumeSemicolon();

        return this->finalize(node, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
    }

    void scanVariableStatement()
    {
        this->expectKeyword(VarKeyword);
        DeclarationOptions opt;
        opt.inFor = false;
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

        if (isParse) {
            consequent = this->parseStatement(allowFunctionDeclaration);
        } else {
            this->scanStatement(allowFunctionDeclaration);
        }

        if (this->matchKeyword(ElseKeyword)) {
            this->nextToken();
            if (isParse) {
                alternate = this->parseStatement(allowFunctionDeclaration);
            } else {
                this->scanStatement(allowFunctionDeclaration);
            }
        }

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
        this->context->inIteration = true;
        RefPtr<Node> body;
        if (isParse) {
            body = this->parseStatement(false);
        } else {
            this->scanStatement(false);
        }
        this->context->inIteration = previousInIteration;

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

        this->expectKeyword(ForKeyword);
        this->expect(LeftParenthesis);

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(VarKeyword)) {
                this->nextToken();

                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                DeclarationOptions opt;
                opt.inFor = true;
                VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
                this->context->allowIn = previousAllowIn;

                if (declarations.size() == 1 && this->matchKeyword(InKeyword)) {
                    RefPtr<VariableDeclaratorNode> decl = declarations[0];
                    // if (decl->init() && (decl.id.type === Syntax.ArrayPattern || decl.id.type === Syntax.ObjectPattern || this->context->strict)) {
                    if (decl->init() && (decl->id()->type() == ArrayExpression || decl->id()->type() == ObjectExpression || this->context->strict)) {
                        this->throwError(Messages::ForInOfLoopInitializer, new ASCIIString("for-in"));
                    }
                    if (isParse) {
                        left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    }

                    this->nextToken();
                    type = statementTypeForIn;
                } else if (declarations.size() == 1 && declarations[0]->init() == nullptr
                           && this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    if (isParse) {
                        left = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    }

                    this->nextToken();
                    type = statementTypeForOf;
                } else {
                    if (isParse) {
                        init = this->finalize(this->createNode(), new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    }
                    this->expect(SemiColon);
                }
            } else if (this->matchKeyword(ConstKeyword) || this->matchKeyword(LetKeyword)) {
                // TODO
                this->throwUnexpectedToken(this->nextToken());
                /*
                init = this->createNode();
                const kind = this->nextToken().value;

                if (!this->context->strict && this->lookahead.value === 'in') {
                    init = this->finalize(init, new Node.Identifier(kind));
                    this->nextToken();
                    left = init;
                    right = this->expression<Parse>();
                    init = null;
                } else {
                    const previousAllowIn = this->context->allowIn;
                    this->context->allowIn = false;
                    const declarations = this->parseBindingList(kind, { inFor: true });
                    this->context->allowIn = previousAllowIn;

                    if (declarations.length === 1 && declarations[0].init === null && this->matchKeyword('in')) {
                        init = this->finalize(init, new Node.VariableDeclaration(declarations, kind));
                        this->nextToken();
                        left = init;
                        right = this->expression<Parse>();
                        init = null;
                    } else if (declarations.length === 1 && declarations[0].init === null && this->matchContextualKeyword('of')) {
                        init = this->finalize(init, new Node.VariableDeclaration(declarations, kind));
                        this->nextToken();
                        left = init;
                        right = this->assignmentExpression<Parse>();
                        init = null;
                        forIn = false;
                    } else {
                        this->consumeSemicolon();
                        init = this->finalize(init, new Node.VariableDeclaration(declarations, kind));
                    }
                }
                */
            } else {
                RefPtr<Scanner::ScannerResult> initStartToken = this->lookahead;
                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                init = this->inheritCoverGrammar(&Parser::assignmentExpression<Parse>);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(InKeyword)) {
                    if (init->isLiteral() || init->type() == ASTNodeType::AssignmentExpression || init->type() == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInForIn);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init.get());
                    left = init;
                    init = nullptr;
                    type = statementTypeForIn;
                } else if (this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    if (!this->context->isAssignmentTarget || init->type() == ASTNodeType::AssignmentExpression) {
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

        bool previousInIteration = this->context->inIteration;
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

        if (!isParse) {
            return T(nullptr);
        }

        MetaNode node = this->createNode();

        if (type == statementTypeFor) {
            return T(this->finalize(node, new ForStatementNode(init.get(), test.get(), update.get(), body.get())));
        }

        if (type == statementTypeForIn) {
            return T(this->finalize(node, new ForInOfStatementNode(left.get(), right.get(), body.get(), true)));
        }

        ASSERT(type == statementTypeForOf);
        return T(this->finalize(node, new ForInOfStatementNode(left.get(), right.get(), body.get(), false)));
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

        AtomicString label;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            if (isParse) {
                RefPtr<IdentifierNode> labelNode = this->parseVariableIdentifier();
                label = labelNode->name();
            } else {
                this->scanVariableIdentifier();
                label = lastScanIdentifierName;
            }

            if (!hasLabel(label)) {
                this->throwError(Messages::UnknownLabel, label.string());
            }

            for (size_t i = 0; i < this->context->labelSet.size(); i++) {
                if (this->context->labelSet[i].first == label && this->context->labelSet[i].second == 1) {
                    this->throwError(Messages::UnknownLabel, label.string());
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
        if (label.string()->length() != 0) {
            return T(this->finalize(node, new ContinueLabelStatementNode(label.string())));
        } else {
            return T(this->finalize(node, new ContinueStatementNode()));
        }
    }

    // ECMA-262 13.9 The break statement

    template <typename T, bool isParse>
    T breakStatement()
    {
        this->expectKeyword(BreakKeyword);

        AtomicString label;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            if (isParse) {
                RefPtr<IdentifierNode> labelNode = this->parseVariableIdentifier();
                label = labelNode->name();
            } else {
                this->scanVariableIdentifier();
                label = lastScanIdentifierName;
            }

            if (!hasLabel(label)) {
                this->throwError(Messages::UnknownLabel, label.string());
            }
        } else if (!this->context->inIteration && !this->context->inSwitch) {
            this->throwError(Messages::IllegalBreak);
        }

        this->consumeSemicolon();

        if (!isParse) {
            return T(nullptr);
        }

        MetaNode node = this->createNode();
        if (label.string()->length() != 0) {
            return T(this->finalize(node, new BreakLabelStatementNode(label.string())));
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

        bool hasArgument = !this->match(SemiColon) && !this->match(RightBrace) && !this->hasLineTerminator && this->lookahead->type != EOFToken;
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

        bool prevInWith = this->context->inWith;
        this->context->inWith = true;

        for (size_t i = 0; i < this->context->labelSet.size(); i++) {
            this->context->labelSet[i].second++;
        }

        RefPtr<StatementNode> body = this->parseStatement(false);
        this->context->inWith = prevInWith;

        for (size_t i = 0; i < this->context->labelSet.size(); i++) {
            this->context->labelSet[i].second--;
        }

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
            return T(this->finalize(this->createNode(), new SwitchStatementNode(discriminant.get(), casesA.get(), deflt.get(), casesB.get(), false)));
        }
        return T(nullptr);
    }

    // ECMA-262 13.13 Labelled Statements

    template <typename T, bool isParse>
    T labelledStatement()
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
                name = lastScanIdentifierName;
            }
        }

        StatementNode* statement;
        if (isIdentifier && this->match(Colon)) {
            this->nextToken();

            if (hasLabel(name)) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Label"), name.string());
            }

            this->context->labelSet.push_back(std::make_pair(name, 0));
            if (isParse) {
                RefPtr<StatementNode> labeledBody = this->parseStatement(!this->context->strict);
                statement = new LabeledStatementNode(labeledBody.get(), name.string());
            } else {
                this->scanStatement(!this->context->strict);
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
            return T(this->finalize(this->createNode(), new ThrowStatementNode(argument.get())));
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
            this->throwUnexpectedToken(this->lookahead);
        }

        bool prevInCatch = this->context->inCatch;
        this->context->inCatch = true;

        std::vector<FunctionDeclarationNode*> vecBefore = std::move(this->context->functionDeclarationsInDirectCatchScope);
        bool prevInDirectCatchScope = this->context->inDirectCatchScope;
        this->context->inDirectCatchScope = true;

        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> params;
        RefPtr<Node> param = this->pattern<Parse>(params);

        std::vector<String*, GCUtil::gc_malloc_ignore_off_page_allocator<String*>> paramMap;
        for (size_t i = 0; i < params.size(); i++) {
            for (size_t j = 0; j < paramMap.size(); j++) {
                StringView sv = params[i]->valueStringLiteral();
                if (paramMap[j]->equals(&sv)) {
                    this->throwError(Messages::DuplicateBinding, new StringView(params[i]->relatedSource()));
                }
            }

            paramMap.push_back(new StringView(params[i]->relatedSource()));
        }

        if (this->context->strict && param->type() == Identifier) {
            IdentifierNode* id = (IdentifierNode*)param.get();
            if (this->scanner->isRestrictedWord(id->name())) {
                this->throwError(Messages::StrictCatchVariable);
            }
        }

        this->expect(RightParenthesis);
        RefPtr<Node> body;
        if (isParse) {
            body = this->block<Parse>();
        } else {
            this->block<ScanAsVoid>();
        }

        this->context->inCatch = prevInCatch;

        this->context->inDirectCatchScope = prevInDirectCatchScope;

        std::vector<FunctionDeclarationNode*> vec = std::move(this->context->functionDeclarationsInDirectCatchScope);

        this->context->functionDeclarationsInDirectCatchScope = std::move(vecBefore);

        if (isParse) {
            return T(this->finalize(this->createNode(), new CatchClauseNode(param.get(), nullptr, body.get(), vec)));
        }

        scopeContexts.back()->m_hasCatch = true;
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
        switch (this->lookahead->type) {
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
        case Token::TemplateToken:
        case Token::RegularExpressionToken:
            statement = this->parseExpressionStatement();
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead->valuePunctuatorKind;
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
            switch (this->lookahead->valueKeywordKind) {
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
                    this->throwUnexpectedToken(this->lookahead);
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
                statement = asStatementNode(this->parseVariableStatement());
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
            this->throwUnexpectedToken(this->lookahead);
        }

        return statement;
    }

    void scanStatement(bool allowFunctionDeclaration = true)
    {
        checkRecursiveLimit();
        switch (this->lookahead->type) {
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
        case Token::TemplateToken:
        case Token::RegularExpressionToken:
            this->scanExpressionStatement();
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead->valuePunctuatorKind;
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
            switch (this->lookahead->valueKeywordKind) {
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
                    this->throwUnexpectedToken(this->lookahead);
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
            case VarKeyword:
                this->scanVariableStatement();
                break;
            case WhileKeyword:
                this->whileStatement<ScanAsVoid>();
                break;
            case WithKeyword:
                this->parseWithStatement();
                break;
            default:
                this->scanExpressionStatement();
                break;
            }
            break;

        default:
            this->throwUnexpectedToken(this->lookahead);
        }
    }

    PassRefPtr<Node> parseArrowFunctionSourceElements()
    {
        ASSERT(this->config.parseSingleFunction);

        RefPtr<StatementContainer> argumentInitializers = nullptr;

        if (this->config.reparseArguments) {
            this->reparseFunctionArguments(argumentInitializers);
        }

        this->context->inArrowFunction = true;
        if (this->match(LeftBrace)) {
            return parseFunctionSourceElements();
        }

        bool prevInDirectCatchScope = this->context->inDirectCatchScope;
        this->context->inDirectCatchScope = false;

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

        this->expect(Arrow);
        MetaNode nodeStart = this->createNode();

        RefPtr<Node> expr = this->isolateCoverGrammar(&Parser::assignmentExpression<Parse>);

        RefPtr<StatementContainer> body = StatementContainer::create();
        body->appendChild(this->finalize(nodeStart, new ReturnStatmentNode(expr.get())), nullptr);

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

        this->context->inDirectCatchScope = prevInDirectCatchScope;

        return this->finalize(nodeStart, new BlockStatementNode(body.get(), argumentInitializers.get()));
    }

    // ECMA-262 14.1 Function Definition

    void reparseFunctionArguments(RefPtr<StatementContainer>& argumentInitializers)
    {
        InterpretedCodeBlock* currentTarget = this->config.parseSingleFunctionTarget->asInterpretedCodeBlock();
        Scanner ParamsScannerInstance(this->escargotContext, currentTarget->paramsSrc(), this->errorHandler, 0, 0);
        this->scanner = &ParamsScannerInstance;
        this->setMarkers(ExtendedNodeLOC(0, 0, 0));

        argumentInitializers = StatementContainer::create();
        this->expect(LeftParenthesis);
        ParseParameterOptions options;
        while (!this->match(RightParenthesis)) {
            bool end = !this->parseFormalParameter(options);

            RefPtr<Node> param = options.params.back();

            if (!param->isIdentifier()) {
                MetaNode node = this->createNode();

                RefPtr<Node> statement = this->finalize(node, new ExpressionStatementNode(param.get()));
                argumentInitializers->appendChild(statement->asStatementNode());
            }

            if (end) {
                break;
            }
            this->expect(Comma);
        }
        this->expect(RightParenthesis);

        this->scanner = &this->scannerInstance;
        this->scanner->index = 0;
        this->scanner->lineNumber = currentTarget->sourceElementStart().line;
        this->scanner->lineStart = currentTarget->sourceElementStart().column;
        this->setMarkers(currentTarget->sourceElementStart());
    }

    PassRefPtr<Node> parseFunctionSourceElements()
    {
        RefPtr<StatementContainer> argumentInitializers = nullptr;

        if (this->config.parseSingleFunction) {
            if (this->config.parseSingleFunctionChildIndex.asUint32()) {
                size_t realIndex = this->config.parseSingleFunctionChildIndex.asUint32() - 1;
                this->config.parseSingleFunctionChildIndex = SmallValue(this->config.parseSingleFunctionChildIndex.asUint32() + 1);
                InterpretedCodeBlock* currentTarget = this->config.parseSingleFunctionTarget->asInterpretedCodeBlock();
                size_t orgIndex = this->lookahead->end;
                this->expect(LeftBrace);

                StringView src = currentTarget->childBlocks()[realIndex]->src();
                this->scanner->index = src.length() + orgIndex;

                this->scanner->lineNumber = currentTarget->childBlocks()[realIndex]->sourceElementStart().line;
                this->scanner->lineStart = currentTarget->childBlocks()[realIndex]->sourceElementStart().index - currentTarget->childBlocks()[realIndex]->sourceElementStart().column;
                this->lookahead->lineNumber = this->scanner->lineNumber;
                this->lookahead->lineNumber = this->scanner->lineStart;
                this->lookahead->type = Token::PunctuatorToken;
                this->lookahead->valuePunctuatorKind = PunctuatorKind::RightBrace;
                this->expect(RightBrace);
                return this->finalize(this->createNode(), new BlockStatementNode(StatementContainer::create().get()));
            }
            this->config.parseSingleFunctionChildIndex = SmallValue(this->config.parseSingleFunctionChildIndex.asUint32() + 1);
            if (this->config.reparseArguments) {
                this->reparseFunctionArguments(argumentInitializers);
            }
        }
        bool prevInDirectCatchScope = this->context->inDirectCatchScope;
        this->context->inDirectCatchScope = false;

        MetaNode nodeStart = this->createNode();

        this->expect(LeftBrace);
        RefPtr<StatementContainer> body = this->parseDirectivePrologues();

        auto previousLabelSet = this->context->labelSet;
        bool previousInIteration = this->context->inIteration;
        bool previousInSwitch = this->context->inSwitch;
        bool previousInFunctionBody = this->context->inFunctionBody;

        this->context->labelSet.clear();
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inFunctionBody = true;

        if (shouldCreateAST()) {
            StatementNode* referNode = nullptr;
            while (this->startMarker.index < this->scanner->length) {
                if (this->match(RightBrace)) {
                    break;
                }
                referNode = body->appendChild(this->statementListItem<ParseAs(StatementNode)>().get(), referNode);
            }
        } else {
            StatementNode* referNode = nullptr;
            while (this->startMarker.index < this->scanner->length) {
                if (this->match(RightBrace)) {
                    break;
                }
                this->statementListItem<ScanAsVoid>();
            }
        }

        this->expect(RightBrace);

        this->context->labelSet = previousLabelSet;
        this->context->inIteration = previousInIteration;
        this->context->inSwitch = previousInSwitch;
        this->context->inFunctionBody = previousInFunctionBody;

        scopeContexts.back()->m_locStart.line = nodeStart.line;
        scopeContexts.back()->m_locStart.column = nodeStart.column;
        scopeContexts.back()->m_locStart.index = nodeStart.index;

        scopeContexts.back()->m_locEnd.index = this->lastMarker.index;
#ifndef NDEBUG
        scopeContexts.back()->m_locEnd.line = this->lastMarker.lineNumber;
        scopeContexts.back()->m_locEnd.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif

        this->context->inDirectCatchScope = prevInDirectCatchScope;

        if (this->config.parseSingleFunction) {
            return this->finalize(nodeStart, new BlockStatementNode(body.get(), argumentInitializers.get()));
        } else {
            return this->finalize(nodeStart, new BlockStatementNode(StatementContainer::create().get()));
        }
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
        RefPtr<Scanner::ScannerResult> firstRestricted = nullptr;

        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->allowYield = !isGenerator;
        this->context->inArrowFunction = false;

        if (isFunctionDeclaration || !this->match(LeftParenthesis)) {
            RefPtr<Scanner::ScannerResult> token = this->lookahead;
            id = (!isFunctionDeclaration && !this->context->strict && !isGenerator && this->matchKeyword(YieldKeyword)) ? this->parseIdentifierName() : this->parseVariableIdentifier();
            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    this->throwUnexpectedToken(token, Messages::StrictFunctionName);
                }
            } else {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    firstRestricted = token;
                    message = Messages::StrictFunctionName;
                } else if (this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                    firstRestricted = token;
                    message = Messages::StrictReservedWord;
                }
            }
        }

        MetaNode paramsStart = this->createNode();
        this->expect(LeftParenthesis);
        AtomicString fnName = id ? id->name() : AtomicString();

        if (!isFunctionDeclaration) {
            pushScopeContext(fnName);
        }

        if (id) {
            scopeContexts.back()->insertName(fnName, isFunctionDeclaration);
            insertUsingName(fnName);
        }

        if (isFunctionDeclaration) {
            pushScopeContext(fnName);
        }

        scopeContexts.back()->m_paramsStart.index = paramsStart.index;

        ParseFormalParametersResult formalParameters = this->parseFormalParameters(firstRestricted);
        PatternNodeVector params = std::move(formalParameters.params);
        RefPtr<Scanner::ScannerResult> stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        extractNamesFromFunctionParams(params);
        bool previousStrict = this->context->strict;
        RefPtr<Node> body = this->parseFunctionSourceElements();
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->throwUnexpectedToken(stricted, message);
        }
        this->context->strict = previousStrict;
        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;

        if (isFunctionDeclaration && this->context->inDirectCatchScope) {
            scopeContexts.back()->m_needsSpecialInitialize = true;
        }

        return this->finalize(node, new FunctionType(fnName, std::move(params), body.get(), popScopeContext(node), isGenerator));
    }

    PassRefPtr<FunctionDeclarationNode> parseFunctionDeclaration()
    {
        MetaNode node = this->createNode();

        RefPtr<FunctionDeclarationNode> fd = parseFunction<FunctionDeclarationNode, true>(node);

        if (this->context->inDirectCatchScope) {
            this->context->functionDeclarationsInDirectCatchScope.push_back(fd.get());
        }

        return fd;
    }

    PassRefPtr<FunctionExpressionNode> parseFunctionExpression()
    {
        MetaNode node = this->createNode();
        return parseFunction<FunctionExpressionNode, false>(node);
    }

    // ECMA-262 14.1.1 Directive Prologues

    PassRefPtr<Node> parseDirective()
    {
        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        StringView directiveValue;
        bool isLiteral = false;

        MetaNode node = this->createNode();
        RefPtr<Node> expr = this->expression<Parse>();
        if (expr->type() == Literal) {
            isLiteral = true;
            directiveValue = token->valueStringLiteral();
        }
        this->consumeSemicolon();

        if (isLiteral) {
            return this->finalize(node, new DirectiveNode(asExpressionNode(expr).get(), directiveValue));
        } else {
            return this->finalize(node, new ExpressionStatementNode(expr.get()));
        }
    }

    PassRefPtr<StatementContainer> parseDirectivePrologues()
    {
        RefPtr<Scanner::ScannerResult> firstRestricted = nullptr;

        this->context->inParsingDirective = true;
        RefPtr<StatementContainer> body = StatementContainer::create();
        while (true) {
            RefPtr<Scanner::ScannerResult> token = this->lookahead;
            if (token->type != StringLiteralToken) {
                break;
            }

            RefPtr<Node> statement = this->parseDirective();
            body->appendChild(statement->asStatementNode());

            if (statement->type() != Directive) {
                break;
            }

            DirectiveNode* directive = (DirectiveNode*)statement.get();
            if (token->plain && directive->value().equals("use strict")) {
                this->scopeContexts.back()->m_isStrict = this->context->strict = true;
                if (firstRestricted) {
                    this->throwUnexpectedToken(firstRestricted, Messages::StrictOctalLiteral);
                }
            } else {
                if (!firstRestricted && token->octal) {
                    firstRestricted = token;
                }
            }
        }

        this->context->inParsingDirective = false;

        return body;
    }

    // ECMA-262 14.3 Method Definitions

    PassRefPtr<FunctionExpressionNode> parseGetterMethod()
    {
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        this->expect(RightParenthesis);

        bool isGenerator = false;
        ParseFormalParametersResult params(PatternNodeVector(), nullptr, nullptr, nullptr);
        bool previousAllowYield = this->context->allowYield;
        this->context->allowYield = false;
        RefPtr<Node> method = this->parsePropertyMethod(params);
        this->context->allowYield = previousAllowYield;

        extractNamesFromFunctionParams(params.params);
        scopeContexts.back()->m_paramsStart.index = node.index;
        return this->finalize(node, new FunctionExpressionNode(AtomicString(), std::move(params.params), method.get(), popScopeContext(node), isGenerator));
    }

    PassRefPtr<FunctionExpressionNode> parseSetterMethod()
    {
        MetaNode node = this->createNode();

        ParseParameterOptions options;

        bool isGenerator = false;
        bool previousAllowYield = this->context->allowYield;
        this->context->allowYield = false;

        this->expect(LeftParenthesis);

        pushScopeContext(AtomicString());

        if (this->match(RightParenthesis)) {
            this->throwUnexpectedToken(this->lookahead);
        } else {
            this->parseFormalParameter(options);
        }
        this->expect(RightParenthesis);

        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->inArrowFunction = false;
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        extractNamesFromFunctionParams(options.params);
        scopeContexts.back()->m_paramsStart.index = node.index;

        const bool previousStrict = this->context->strict;
        PassRefPtr<Node> method = this->isolateCoverGrammar(&Parser::parseFunctionSourceElements);
        if (this->context->strict && options.firstRestricted) {
            this->throwUnexpectedToken(options.firstRestricted, options.message);
        }
        if (this->context->strict && options.stricted) {
            this->throwUnexpectedToken(options.stricted, options.message);
        }
        this->context->strict = previousStrict;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowYield = previousAllowYield;

        return this->finalize(node, new FunctionExpressionNode(AtomicString(), std::move(options.params), method.get(), popScopeContext(node), isGenerator));
    }

    FunctionExpressionNode* parseGeneratorMethod()
    {
        // TODO
        this->throwUnexpectedToken(this->nextToken());
        RELEASE_ASSERT_NOT_REACHED();
        /*
        MetaNode node = this->createNode();
        const isGenerator = true;
        const previousAllowYield = this->context.allowYield;

        this->context.allowYield = true;
        const params = this->parseFormalParameters();
        this->context.allowYield = false;
        const method = this->parsePropertyMethod(params);
        this->context.allowYield = previousAllowYield;

        return this->finalize(node, new Node.FunctionExpression(null, params.params, method, isGenerator));
        */
    }

    // ECMA-262 14.4 Generator Function Definitions

    Node* parseYieldExpression()
    {
        this->throwError("yield keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        /*
        const node = this->createNode();
        this->expectKeyword('yield');

        let argument = null;
        let delegate = false;
        if (!this->hasLineTerminator) {
            const previousAllowYield = this->context.allowYield;
            this->context.allowYield = false;
            delegate = this->match('*');
            if (delegate) {
                this->nextToken();
                argument = this->assignmentExpression<Parse>();
            } else {
                if (!this->match(';') && !this->match('}') && !this->match(')') && this->lookahead.type !== Token.EOF) {
                    argument = this->assignmentExpression<Parse>();
                }
            }
            this->context.allowYield = previousAllowYield;
        }

        return this->finalize(node, new Node.YieldExpression(argument, delegate));
        */
    }

    ScanExpressionResult scanYieldExpression()
    {
        this->throwError("yield keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        /*
        const node = this->createNode();
        this->expectKeyword('yield');

        let argument = null;
        let delegate = false;
        if (!this->hasLineTerminator) {
            const previousAllowYield = this->context.allowYield;
            this->context.allowYield = false;
            delegate = this->match('*');
            if (delegate) {
                this->nextToken();
                argument = this->assignmentExpression<Parse>();
            } else {
                if (!this->match(';') && !this->match('}') && !this->match(')') && this->lookahead.type !== Token.EOF) {
                    argument = this->assignmentExpression<Parse>();
                }
            }
            this->context.allowYield = previousAllowYield;
        }

        return this->finalize(node, new Node.YieldExpression(argument, delegate));
        */
    }

    // ECMA-262 14.5 Class Definitions

    PassRefPtr<ClassElementNode> parseClassElement(RefPtr<FunctionExpressionNode>* constructor)
    {
        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        MetaNode node = this->createNode();

        ClassElementNode::Kind kind = ClassElementNode::Kind::None;
        RefPtr<Node> key;
        RefPtr<FunctionExpressionNode> value;
        bool computed = false;
        bool isStatic = false;

        if (this->match(Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            key = this->parseObjectPropertyKey();

            if (token->type == Token::KeywordToken && (this->qualifiedPropertyName(this->lookahead) || this->match(Multiply)) && token->valueStringLiteral() == "static") {
                token = this->lookahead;
                isStatic = true;
                computed = this->match(LeftSquareBracket);
                if (this->match(Multiply)) {
                    this->nextToken();
                } else {
                    key = this->parseObjectPropertyKey();
                }
            }
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(this->lookahead);
        if (token->type == Token::IdentifierToken) {
            if (token->valueStringLiteral() == "get" && lookaheadPropertyKey) {
                kind = ClassElementNode::Kind::Get;
                computed = this->match(LeftSquareBracket);
                key = this->parseObjectPropertyKey();
                this->context->allowYield = false;
                value = this->parseGetterMethod();
            } else if (token->valueStringLiteral() == "set" && lookaheadPropertyKey) {
                kind = ClassElementNode::Kind::Set;
                computed = this->match(LeftSquareBracket);
                key = this->parseObjectPropertyKey();
                value = this->parseSetterMethod();
            }
        } else if (token->type == Token::PunctuatorToken && token->valueStringLiteral() == "*" && lookaheadPropertyKey) {
            kind = ClassElementNode::Kind::Method;
            computed = this->match(LeftSquareBracket);
            key = this->parseObjectPropertyKey();
            value = this->parseGeneratorMethod();
        }

        if (kind == ClassElementNode::Kind::None && key && this->match(LeftParenthesis)) {
            kind = ClassElementNode::Kind::Method;
            value = this->parsePropertyMethodFunction();
        }

        if (kind == ClassElementNode::Kind::None) {
            this->throwUnexpectedToken(this->lookahead);
        }

        if (!computed) {
            if (isStatic && this->isPropertyKey(key.get(), "prototype")) {
                this->throwUnexpectedToken(token, Messages::StaticPrototype);
            }
            if (!isStatic && this->isPropertyKey(key.get(), "constructor")) {
                if (kind != ClassElementNode::Kind::Method || value->function().isGenerator()) {
                    this->throwUnexpectedToken(token, Messages::ConstructorSpecialMethod);
                }
                if (*constructor != nullptr) {
                    this->throwUnexpectedToken(token, Messages::DuplicateConstructor);
                } else {
                    *constructor = value;
                    return nullptr;
                }
            }
        }

        return this->finalize(node, new ClassElementNode(key.get(), value.get(), kind, computed, isStatic));
    }

    PassRefPtr<ClassBodyNode> parseClassBody()
    {
        MetaNode node = this->createNode();

        ClassElementNodeVector body;
        RefPtr<FunctionExpressionNode> constructor = nullptr;

        this->expect(LeftBrace);
        while (!this->match(RightBrace)) {
            if (this->match(SemiColon)) {
                this->nextToken();
            } else {
                PassRefPtr<ClassElementNode> classElement = this->parseClassElement(&constructor);
                if (classElement != nullptr) {
                    body.push_back(classElement);
                }
            }
        }
        this->expect(RightBrace);

        return this->finalize(node, new ClassBodyNode(std::move(body), constructor));
    }

    struct ClassProperty {
        RefPtr<IdentifierNode> id;
        RefPtr<ClassBodyNode> classBody;
        RefPtr<Node> superClass;
    };

    ClassProperty parseClassProperties(bool identifierIsOptional)
    {
        bool previousStrict = this->context->strict;
        this->context->strict = true;
        this->expectKeyword(ClassKeyword);

        RefPtr<IdentifierNode> id = (identifierIsOptional && this->lookahead->type != Token::IdentifierToken) ? nullptr : this->parseVariableIdentifier();

        if (id) {
            scopeContexts.back()->insertName(id->name(), true);
            insertUsingName(id->name());
        }

        RefPtr<Node> superClass = nullptr;
        if (this->matchKeyword(ExtendsKeyword)) {
            this->throwError("extends keyword is not supported yet");
            this->nextToken();
            superClass = this->isolateCoverGrammar(&Parser::leftHandSideExpressionAllowCall<Parse>);
        }

        RefPtr<ClassBodyNode> classBody = this->parseClassBody();
        this->context->strict = previousStrict;

        return ClassProperty{ id, classBody, superClass };
    }

    PassRefPtr<ClassDeclarationNode> parseClassDeclaration()
    {
        ClassProperty classProperties = parseClassProperties(false);

        return this->finalize(this->createNode(), new ClassDeclarationNode(classProperties.id, classProperties.superClass, classProperties.classBody.get(), this->escargotContext));
    }

    PassRefPtr<ClassExpressionNode> parseClassExpression()
    {
        ClassProperty classProperties = parseClassProperties(true);

        return this->finalize(this->createNode(), new ClassExpressionNode(classProperties.id, classProperties.superClass, classProperties.classBody.get(), this->escargotContext));
    }

    // ECMA-262 15.1 Scripts
    // ECMA-262 15.2 Modules

    PassRefPtr<ProgramNode> parseProgram()
    {
        MetaNode node = this->createNode();
        pushScopeContext(new ASTScopeContext(this->context->strict));
        RefPtr<StatementContainer> body = this->parseDirectivePrologues();
        StatementNode* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            referNode = body->appendChild(this->statementListItem<ParseAs(StatementNode)>().get(), referNode);
        }
        scopeContexts.back()->m_locStart.line = node.line;
        scopeContexts.back()->m_locStart.column = node.column;
        scopeContexts.back()->m_locStart.index = node.index;

        MetaNode endNode = this->createNode();
#ifndef NDEBUG
        scopeContexts.back()->m_locEnd.line = endNode.line;
        scopeContexts.back()->m_locEnd.column = endNode.column;
#endif
        scopeContexts.back()->m_locEnd.index = endNode.index;
        return this->finalize(node, new ProgramNode(body.get(), scopeContexts.back() /*, this->sourceType*/));
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

RefPtr<ProgramNode> parseProgram(::Escargot::Context* ctx, StringView source, bool strictFromOutside, size_t stackRemain)
{
    Parser parser(ctx, source, stackRemain);
    parser.context->strict = strictFromOutside;
    RefPtr<ProgramNode> nd = parser.parseProgram();
    return nd;
}

std::tuple<RefPtr<Node>, ASTScopeContext*> parseSingleFunction(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock, size_t stackRemain)
{
    Parser parser(ctx, codeBlock->src(), stackRemain, codeBlock->sourceElementStart());
    parser.trackUsingNames = false;
    parser.config.parseSingleFunction = true;
    parser.config.parseSingleFunctionTarget = codeBlock;
    parser.config.reparseArguments = codeBlock->shouldReparseArguments();
    auto sc = new ASTScopeContext(codeBlock->isStrict());
    parser.pushScopeContext(sc);
    RefPtr<Node> nd;
    if (codeBlock->isArrowFunctionExpression()) {
        nd = parser.parseArrowFunctionSourceElements();
    } else {
        nd = parser.parseFunctionSourceElements();
    }
    return std::make_tuple(nd, sc);
}

} // namespace esprima
} // namespace Escargot
