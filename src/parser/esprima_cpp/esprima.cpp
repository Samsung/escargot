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
    {
        this->index = index;
        this->line = line;
        this->col = col;
        this->description = description;
    }

    ParserError(size_t index, size_t line, size_t col, const char* description)
    {
        this->index = index;
        this->line = line;
        this->col = col;
        this->description = new ASCIIString(description);
    }
};

struct Config : public gc {
    bool range : 1;
    bool loc : 1;
    bool tokens : 1;
    bool comment : 1;
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

    typedef std::pair<ASTNodeType, AtomicString> ScanExpressionResult;

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
        for (size_t i = 0; i < vector.size(); i++) {
            ASSERT(vector[i]->isIdentifier());
            IdentifierNode* id = (IdentifierNode*)vector[i].get();
            scopeContexts.back()->insertName(id->name(), true);
        }
    }

    void pushScopeContext(const PatternNodeVector& params, AtomicString functionName)
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
        scopeContexts.back()->m_parameters.resizeWithUninitializedValues(params.size());
        for (size_t i = 0; i < params.size(); i++) {
            ASSERT(params[i]->isIdentifier());
            IdentifierNode* id = (IdentifierNode*)params[i].get();
            scopeContexts.back()->m_parameters[i] = id->name();
        }
        if (parentContext) {
            parentContext->m_childScopes.push_back(scopeContexts.back());
        }
    }

    Parser(::Escargot::Context* escargotContext, StringView code, size_t stackRemain, size_t startLine = 0, size_t startColumn = 0, size_t startIndex = 0)
        : errorHandler(&errorHandlerInstance)
        , scannerInstance(escargotContext, code, this->errorHandler, startLine, startColumn)
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

        this->baseMarker.index = startIndex;
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

            if (this->context->strict && next->type == Token::IdentifierToken) {
                if (this->scanner->isStrictModeReservedWord(next->relatedSource())) {
                    next->type = Token::KeywordToken;
                }
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
        throw this->errorHandler->createError(index, line, column, new UTF16String(msg.data(), msg.length()), code);
    }

    void tolerateError(const char* messageFormat, String* arg0 = String::emptyString, String* arg1 = String::emptyString, ErrorObject::Code code = ErrorObject::SyntaxError)
    {
        throwError(messageFormat, arg0, arg1, code);
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

    // Throw an exception because of the token.
    Error unexpectedTokenError(RefPtr<Scanner::ScannerResult> token = nullptr, const char* message = nullptr)
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
                msg = (token->type == Token::EOFToken) ? Messages::UnexpectedEOS : (token->type == Token::IdentifierToken) ? Messages::UnexpectedIdentifier : (token->type == Token::NumericLiteralToken) ? Messages::UnexpectedNumber : (token->type == Token::StringLiteralToken) ? Messages::UnexpectedString : (token->type == Token::TemplateToken) ? Messages::UnexpectedTemplate : Messages::UnexpectedToken;

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
            return this->errorHandler->createError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        } else {
            const size_t index = this->lastMarker.index;
            const size_t line = this->lastMarker.lineNumber;
            const size_t column = index - this->lastMarker.lineStart + 1;
            return this->errorHandler->createError(index, line, column, new UTF16String(msgData.data(), msgData.length()), ErrorObject::SyntaxError);
        }
    }

    void throwUnexpectedToken(RefPtr<Scanner::ScannerResult> token, const char* message = nullptr)
    {
        throw this->unexpectedTokenError(token, message);
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

        if (this->context->strict && next->type == Token::IdentifierToken) {
            if (this->scanner->isStrictModeReservedWord(next->relatedSource())) {
                next->type = Token::KeywordToken;
            }
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
            if (c->callee()->isIdentifier()) {
                if (((IdentifierNode*)c->callee())->name() == this->escargotContext->staticStrings().eval) {
                    scopeContexts.back()->m_hasEval = true;
                    if (this->context->inArrowFunction) {
                        insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
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
        AtomicString name;
        StringView sv = token->valueStringLiteral();
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            name = this->escargotContext->staticStrings().asciiTable[firstCh];
        } else {
            if (!token->plain) {
                name = AtomicString(this->escargotContext, sv.string());
            } else {
                name = AtomicString(this->escargotContext, SourceStringView(sv));
            }
        }

        if (trackUsingNames) {
            insertUsingName(name);
        }

        return std::make_pair(ASTNodeType::Identifier, name);
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

    PassRefPtr<Node> parsePrimaryExpression()
    {
        MetaNode node = this->createNode();

        // let value, token, raw;
        switch (this->lookahead->type) {
        case Token::IdentifierToken:
            if (this->sourceType == SourceType::Module && this->lookahead->valueKeywordKind == AwaitKeyword) {
                this->throwUnexpectedToken(this->lookahead);
            }
            return this->finalize(node, finishIdentifier(this->nextToken(), true));
            break;
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
                    return this->finalize(node, new LiteralNode(Value(token->valueNumber)));
                } else {
                    if (shouldCreateAST()) {
                        return this->finalize(node, new LiteralNode(token->valueStringLiteralForAST()));
                    } else {
                        return this->finalize(node, new LiteralNode(Value(String::emptyString)));
                    }
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
                return this->finalize(node, new LiteralNode(Value(value)));
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
            return this->finalize(node, new LiteralNode(Value(Value::Null)));
            break;
        }
        case Token::TemplateToken:
            return this->parseTemplateLiteral();
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead->valuePunctuatorKind;
            switch (value) {
            case LeftParenthesis:
                this->context->isBindingElement = false;
                return this->inheritCoverGrammar(&Parser::parseGroupExpression);
                break;
            case LeftSquareBracket:
                return this->inheritCoverGrammar(&Parser::parseArrayInitializer);
                break;
            case LeftBrace:
                return this->inheritCoverGrammar(&Parser::parseObjectInitializer);
                break;
            case Divide:
            case DivideEqual: {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->scanner->index = this->startMarker.index;
                PassRefPtr<Scanner::ScannerResult> token = this->nextRegexToken();
                // raw = this->getTokenRaw(token);
                return this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags));
                break;
            }
            default:
                this->throwUnexpectedToken(this->nextToken());
            }
            break;
        }

        case Token::KeywordToken:
            if (!this->context->strict && this->context->allowYield && this->matchKeyword(YieldKeyword)) {
                return this->parseIdentifierName();
            } else if (!this->context->strict && this->matchKeyword(LetKeyword)) {
                throwUnexpectedToken(this->nextToken());
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(FunctionKeyword)) {
                    return this->parseFunctionExpression();
                } else if (this->matchKeyword(ThisKeyword)) {
                    if (this->context->inArrowFunction) {
                        insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
                    this->nextToken();
                    return this->finalize(node, new ThisExpressionNode());
                } else if (this->matchKeyword(ClassKeyword)) {
                    return this->parseClassExpression();
                } else {
                    this->throwUnexpectedToken(this->nextToken());
                }
            }
            break;
        default:
            this->throwUnexpectedToken(this->nextToken());
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ScanExpressionResult scanPrimaryExpression()
    {
        MetaNode node = this->createNode();

        // let value, token, raw;
        switch (this->lookahead->type) {
        case Token::IdentifierToken:
            if (this->sourceType == SourceType::Module && this->lookahead->valueKeywordKind == AwaitKeyword) {
                this->throwUnexpectedToken(this->lookahead);
            }
            return finishScanIdentifier(this->nextToken(), true);
            break;
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
                    return std::make_pair(ASTNodeType::Literal, AtomicString());
                } else {
                    return std::make_pair(ASTNodeType::Literal, AtomicString());
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
                return std::make_pair(ASTNodeType::Literal, AtomicString());
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
            return std::make_pair(ASTNodeType::Literal, AtomicString());
            break;
        }
        case Token::TemplateToken:
            this->parseTemplateLiteral();
            return std::make_pair(ASTNodeType::TemplateLiteral, AtomicString());
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead->valuePunctuatorKind;
            switch (value) {
            case LeftParenthesis:
                this->context->isBindingElement = false;
                return this->scanInheritCoverGrammar(&Parser::scanGroupExpression);
                break;
            case LeftSquareBracket:
                this->scanInheritCoverGrammar(&Parser::scanArrayInitializer);
                return std::make_pair(ASTNodeType::ArrayExpression, AtomicString());
                break;
            case LeftBrace:
                this->scanInheritCoverGrammar(&Parser::scanObjectInitializer);
                return std::make_pair(ASTNodeType::ObjectExpression, AtomicString());
                break;
            case Divide:
            case DivideEqual: {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->scanner->index = this->startMarker.index;
                PassRefPtr<Scanner::ScannerResult> token = this->nextRegexToken();
                // raw = this->getTokenRaw(token);
                this->finalize(node, new RegExpLiteralNode(token->valueRegexp.body, token->valueRegexp.flags));
                return std::make_pair(ASTNodeType::Literal, AtomicString());
                break;
            }
            default:
                this->throwUnexpectedToken(this->nextToken());
            }
            break;
        }

        case Token::KeywordToken:
            if (!this->context->strict && this->context->allowYield && this->matchKeyword(YieldKeyword)) {
                return this->scanIdentifierName();
            } else if (!this->context->strict && this->matchKeyword(LetKeyword)) {
                throwUnexpectedToken(this->nextToken());
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(FunctionKeyword)) {
                    this->parseFunctionExpression();
                    return std::make_pair(ASTNodeType::FunctionExpression, AtomicString());
                } else if (this->matchKeyword(ThisKeyword)) {
                    if (this->context->inArrowFunction) {
                        insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
                    this->nextToken();
                    return std::make_pair(ASTNodeType::ThisExpression, AtomicString());
                } else if (this->matchKeyword(ClassKeyword)) {
                    this->parseClassExpression();
                    return std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString());
                } else {
                    this->throwUnexpectedToken(this->nextToken());
                }
            }
            break;
        default:
            this->throwUnexpectedToken(this->nextToken());
        }

        ASSERT_NOT_REACHED();
        return std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString());
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
    }

    PassRefPtr<RestElementNode> parseRestElement(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params)
    {
        this->throwError("Rest element is not supported yet");

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

        return this->finalize(node, new RestElementNode(param.get()));
    }

    PassRefPtr<Node> parsePattern(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
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
            return this->parseVariableIdentifier(kind);
        }
    }

    std::pair<ASTNodeType, AtomicString> scanPattern(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
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
            return std::make_pair(ASTNodeType::Identifier, this->scanVariableIdentifier(kind));
        }
    }

    PassRefPtr<Node> parsePatternWithDefault(std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        PassRefPtr<Node> pattern = this->parsePattern(params, kind);
        if (this->match(PunctuatorKind::Substitution)) {
            this->throwError("Assignment in parameter is not supported yet");

            this->nextToken();
            const bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = true;
            PassRefPtr<Node> right = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
            this->context->allowYield = previousAllowYield;
            return this->finalize(this->startNode(startToken), new AssignmentExpressionSimpleNode(pattern.get(), right.get()));
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
            RefPtr<RestElementNode> param = this->parseRestElement(params);
            this->validateParam(options, params.back(), param->argument()->name());
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
        {
            this->params = std::move(params);
            this->stricted = stricted;
            this->firstRestricted = firstRestricted;
            this->message = nullptr;
            this->valid = true;
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

    PassRefPtr<SpreadElementNode> parseSpreadElement()
    {
        this->expect(PunctuatorKind::PeriodPeriodPeriod);
        MetaNode node = this->createNode();

        RefPtr<Node> arg = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
        this->throwError("Spread element is not supported yet");
        return this->finalize(node, new SpreadElementNode(arg.get()));
    }

    ScanExpressionResult scanSpreadElement()
    {
        this->expect(PunctuatorKind::PeriodPeriodPeriod);
        MetaNode node = this->createNode();

        this->scanInheritCoverGrammar(&Parser::scanAssignmentExpression);
        this->throwError("Spread element is not supported yet");

        return std::make_pair(ASTNodeType::SpreadElement, AtomicString());
    }

    PassRefPtr<Node> parseArrayInitializer()
    {
        // const elements: Node.ArrayExpressionElement[] = [];
        ExpressionNodeVector elements;

        this->expect(LeftSquareBracket);
        MetaNode node = this->createNode();

        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
                elements.push_back(nullptr);
            } else if (this->match(PeriodPeriodPeriod)) {
                PassRefPtr<SpreadElementNode> element = this->parseSpreadElement();
                if (!this->match(RightSquareBracket)) {
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->expect(Comma);
                }
                elements.push_back(element);
            } else {
                elements.push_back(this->inheritCoverGrammar(&Parser::parseAssignmentExpression));
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        return this->finalize(node, new ArrayExpressionNode(std::move(elements)));
    }

    ScanExpressionResult scanArrayInitializer()
    {
        this->expect(LeftSquareBracket);

        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
            } else if (this->match(PeriodPeriodPeriod)) {
                this->scanSpreadElement();
                if (!this->match(RightSquareBracket)) {
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->expect(Comma);
                }
            } else {
                this->scanInheritCoverGrammar(&Parser::scanAssignmentExpression);
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        return std::make_pair(ASTNodeType::ArrayExpression, AtomicString());
    }

    // ECMA-262 12.2.6 Object Initializer

    PassRefPtr<Node> parsePropertyMethod(ParseFormalParametersResult& params)
    {
        bool previousInArrowFunction = this->context->inArrowFunction;

        this->context->inArrowFunction = false;
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        pushScopeContext(params.params, AtomicString());
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
        this->expect(LeftParenthesis);
        MetaNode node = this->createNode();
        ParseFormalParametersResult params = this->parseFormalParameters();
        RefPtr<Node> method = this->parsePropertyMethod(params);
        this->context->allowYield = previousAllowYield;

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
                key = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
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
        RefPtr<Scanner::ScannerResult> token = this->nextToken();
        ScanExpressionResult key(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        String* keyString = String::emptyString;
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
                if (token->type == Token::NumericLiteralToken) {
                    if (this->context->inLoop || token->valueNumber == 0)
                        this->scopeContexts.back()->insertNumeralLiteral(Value(token->valueNumber));
                } else {
                    keyString = token->valueStringLiteralForAST().asString();
                }
                key.first = Literal;
            }
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
                key = this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
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

    PassRefPtr<PropertyNode> parseObjectProperty(bool& hasProto, std::vector<std::pair<AtomicString, size_t>>& usedNames) //: Node.Property
    {
        MetaNode node = this->createNode();
        RefPtr<Scanner::ScannerResult> token = this->lookahead;

        PropertyNode::Kind kind;
        RefPtr<Node> key; //'': Node.PropertyKey;
        RefPtr<Node> value; //: Node.PropertyValue;

        bool computed = false;
        bool method = false;
        bool shorthand = false;

        if (token->type == Token::IdentifierToken) {
            this->nextToken();
            MetaNode node = this->createNode();
            key = this->finalize(node, finishIdentifier(token, true));
        } else if (this->match(PunctuatorKind::Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            key = this->parseObjectPropertyKey();
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(this->lookahead);
        bool isGet = false;
        bool isSet = false;
        bool isGenerator = false;

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
            key = this->parseObjectPropertyKey();
            this->context->allowYield = false;
            value = this->parseGetterMethod();
        } else if (isSet) {
            kind = PropertyNode::Kind::Set;
            computed = this->match(LeftSquareBracket);
            key = this->parseObjectPropertyKey();
            value = this->parseSetterMethod();
        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::Multiply && lookaheadPropertyKey) {
            kind = PropertyNode::Kind::Init;
            computed = this->match(LeftSquareBracket);
            key = this->parseObjectPropertyKey();
            value = this->parseGeneratorMethod();
            method = true;
        } else {
            if (!key) {
                this->throwUnexpectedToken(this->lookahead);
            }
            kind = PropertyNode::Kind::Init;
            if (this->match(PunctuatorKind::Colon)) {
                if (!this->config.parseSingleFunction && !computed && this->isPropertyKey(key.get(), "__proto__")) {
                    if (hasProto) {
                        this->tolerateError(Messages::DuplicateProtoProperty);
                    }
                    hasProto = true;
                }
                this->nextToken();
                value = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
            } else if (this->match(LeftParenthesis)) {
                value = this->parsePropertyMethodFunction();
                method = true;
            } else if (token->type == Token::IdentifierToken) {
                this->throwError("Property shorthand is not supported yet");
                RefPtr<Node> id = this->finalize(node, finishIdentifier(token, true));
                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    RefPtr<Node> init = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    //value = this->finalize(node, new AssignmentPatternNode(id, init));
                } else {
                    shorthand = true;
                    value = id;
                }
            } else {
                this->throwUnexpectedToken(this->nextToken());
            }
        }

        if (!this->config.parseSingleFunction) {
            if (key->isIdentifier()) {
                AtomicString as = key->asIdentifier()->name();
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
        }

        // return this->finalize(node, new PropertyNode(kind, key, computed, value, method, shorthand));
        return this->finalize(node, new PropertyNode(key.get(), value.get(), kind, computed));
    }

    void scanObjectProperty(bool& hasProto, std::vector<std::pair<AtomicString, size_t>>& usedNames) //: Node.Property
    {
        RefPtr<Scanner::ScannerResult> token = this->lookahead;

        PropertyNode::Kind kind;
        ScanExpressionResult key(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        String* keyString = String::emptyString;

        bool computed = false;
        bool method = false;
        bool shorthand = false;

        if (token->type == Token::IdentifierToken) {
            this->nextToken();
            key = finishScanIdentifier(token, true);
        } else if (this->match(PunctuatorKind::Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            auto keyValue = this->scanObjectPropertyKey();
            key = keyValue.first;
            keyString = keyValue.second;
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(this->lookahead);
        bool isGet = false;
        bool isSet = false;
        bool isGenerator = false;

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
            auto keyValue = this->scanObjectPropertyKey();
            key = keyValue.first;
            keyString = keyValue.second;
            this->context->allowYield = false;
            this->parseGetterMethod();
        } else if (isSet) {
            kind = PropertyNode::Kind::Set;
            computed = this->match(LeftSquareBracket);
            auto keyValue = this->scanObjectPropertyKey();
            key = keyValue.first;
            keyString = keyValue.second;
            this->parseSetterMethod();
        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::Multiply && lookaheadPropertyKey) {
            kind = PropertyNode::Kind::Init;
            computed = this->match(LeftSquareBracket);
            auto keyValue = this->scanObjectPropertyKey();
            key = keyValue.first;
            keyString = keyValue.second;
            this->parseGeneratorMethod();
            method = true;
        } else {
            if (key.first == ASTNodeType::ASTNodeTypeError) {
                this->throwUnexpectedToken(this->lookahead);
            }
            kind = PropertyNode::Kind::Init;
            if (this->match(PunctuatorKind::Colon)) {
                bool isProto = (key.first == ASTNodeType::Identifier && key.second == this->escargotContext->staticStrings().__proto__)
                    || (key.first == ASTNodeType::Literal && keyString->equals("__proto__"));
                if (!computed && isProto) {
                    if (hasProto) {
                        this->tolerateError(Messages::DuplicateProtoProperty);
                    }
                    hasProto = true;
                }
                this->nextToken();
                this->scanInheritCoverGrammar(&Parser::scanAssignmentExpression);
            } else if (this->match(LeftParenthesis)) {
                this->parsePropertyMethodFunction();
                method = true;
            } else if (token->type == Token::IdentifierToken) {
                this->throwError("Property shorthand is not supported yet");
                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
                } else {
                    shorthand = true;
                }
            } else {
                this->throwUnexpectedToken(this->nextToken());
            }
        }

        if (key.first == ASTNodeType::Identifier) {
            AtomicString as = key.second;
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
    }

    PassRefPtr<Node> parseObjectInitializer()
    {
        this->expect(LeftBrace);
        MetaNode node = this->createNode();
        PropertiesNodeVector properties;
        bool hasProto = false;
        std::vector<std::pair<AtomicString, size_t>> usedNames;
        while (!this->match(RightBrace)) {
            properties.push_back(this->parseObjectProperty(hasProto, usedNames));
            if (!this->match(RightBrace)) {
                this->expectCommaSeparator();
            }
        }
        this->expect(RightBrace);

        return this->finalize(node, new ObjectExpressionNode(std::move(properties)));
    }

    ScanExpressionResult scanObjectInitializer()
    {
        this->expect(LeftBrace);
        bool hasProto = false;
        std::vector<std::pair<AtomicString, size_t>> usedNames;
        while (!this->match(RightBrace)) {
            this->scanObjectProperty(hasProto, usedNames);
            if (!this->match(RightBrace)) {
                this->expectCommaSeparator();
            }
        }
        this->expect(RightBrace);

        return std::make_pair(ASTNodeType::ObjectExpression, AtomicString());
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
            expressions.push_back(this->parseExpression());
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
                expr = this->inheritCoverGrammar(this->parseAssignmentExpression);

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
                            expressions.push(this->inheritCoverGrammar(this->parseAssignmentExpression));
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
        switch (expr.first) {
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

    PassRefPtr<Node> parseGroupExpression()
    {
        RefPtr<Node> expr;

        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->nextToken();
            if (!this->match(Arrow)) {
                this->expect(Arrow);
            }

            //TODO
            expr = this->finalize(this->startNode(this->lookahead), new ArrowParameterPlaceHolderNode());
        } else {
            RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

            bool arrow = false;
            this->context->isBindingElement = true;
            expr = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);

            if (this->match(Comma)) {
                ExpressionNodeVector expressions;

                this->context->isAssignmentTarget = false;
                expressions.push_back(expr);
                while (this->startMarker.index < this->scanner->length) {
                    if (!this->match(Comma)) {
                        break;
                    }
                    this->nextToken();

                    expressions.push_back(this->inheritCoverGrammar(&Parser::parseAssignmentExpression));
                }
                if (!arrow) {
                    expr = this->finalize(this->startNode(startToken), new SequenceExpressionNode(std::move(expressions)));
                }
            }

            if (!arrow) {
                this->expect(RightParenthesis);
                if (this->match(Arrow)) {
                    if (expr->type() == Identifier && expr->asIdentifier()->name() == "yield") {
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
                        if (expr->type() == SequenceExpression) {
                            const ExpressionNodeVector& expressions = ((SequenceExpressionNode*)expr.get())->expressions();
                            for (size_t i = 0; i < expressions.size(); i++) {
                                this->reinterpretExpressionAsPattern(expressions[i].get());
                            }
                        } else {
                            this->reinterpretExpressionAsPattern(expr.get());
                        }

                        //TODO
                        ExpressionNodeVector params;
                        if (expr->type() == SequenceExpression) {
                            params = ((SequenceExpressionNode*)expr.get())->expressions();
                        } else {
                            params.push_back(expr);
                        }

                        expr = this->finalize(this->startNode(this->lookahead), new ArrowParameterPlaceHolderNode(std::move(params)));
                    }
                }
                this->context->isBindingElement = false;
            }
        }

        return expr.release();
    }

    ScanExpressionResult scanGroupExpression()
    {
        ScanExpressionResult expr(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));

        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->nextToken();
            if (!this->match(Arrow)) {
                this->expect(Arrow);
            }

            //TODO
            expr.first = ASTNodeType::ArrowParameterPlaceHolder;
        } else {
            RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

            bool arrow = false;
            this->context->isBindingElement = true;
            expr = this->scanInheritCoverGrammar(&Parser::scanAssignmentExpression);

            if (this->match(Comma)) {
                this->context->isAssignmentTarget = false;
                while (this->startMarker.index < this->scanner->length) {
                    if (!this->match(Comma)) {
                        break;
                    }
                    this->nextToken();

                    this->scanInheritCoverGrammar(&Parser::scanAssignmentExpression);
                }
                if (!arrow) {
                    expr.first = ASTNodeType::SequenceExpression;
                }
            }

            if (!arrow) {
                this->expect(RightParenthesis);
                if (this->match(Arrow)) {
                    if (expr.first == Identifier && expr.second.string()->equals("yield")) {
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
                        /*
                        // TODO
                        if (expr->type() == SequenceExpression) {
                            const ExpressionNodeVector& expressions = ((SequenceExpressionNode*)expr.get())->expressions();
                            for (size_t i = 0; i < expressions.size(); i++) {
                                this->reinterpretExpressionAsPattern(expressions[i].get());
                            }
                        } else {
                            this->reinterpretExpressionAsPattern(expr.get());
                        }
                        ExpressionNodeVector params;
                        if (expr->type() == SequenceExpression) {
                            params =((SequenceExpressionNode*)expr.get())->expressions();
                        } else {
                            params.push_back(expr);
                        }
                        */

                        expr.first = ASTNodeType::SequenceExpression;
                    }
                }
                this->context->isBindingElement = false;
            }
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
                    expr = this->parseSpreadElement();
                } else {
                    expr = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
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
                    this->parseSpreadElement();
                } else {
                    this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
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

    PassRefPtr<Node> parseNewExpression()
    {
        MetaNode node = this->createNode();

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

        Node* expr;
        RefPtr<Node> callee = this->isolateCoverGrammar(&Parser::parseLeftHandSideExpression);
        ArgumentVector args;
        if (this->match(LeftParenthesis)) {
            args = this->parseArguments();
        }
        expr = new NewExpressionNode(callee.get(), std::move(args));
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;
        return this->finalize(node, expr);
    }


    ScanExpressionResult scanNewExpression()
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

        ScanExpressionResult callee = this->scanIsolateCoverGrammar(&Parser::scanLeftHandSideExpression);
        if (this->match(LeftParenthesis)) {
            this->scanArguments();
        }
        ScanExpressionResult expr(ASTNodeType::NewExpression, AtomicString());
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;
        return expr;
    }

    PassRefPtr<Node> parseLeftHandSideExpressionAllowCall()
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        RefPtr<Node> expr;
        if (this->context->inFunctionBody && this->matchKeyword(SuperKeyword)) {
            MetaNode node = this->createNode();
            this->nextToken();
            this->throwError("super keyword is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // expr = this->finalize(node, new Node.Super());
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(this->lookahead);
            }
        } else {
            expr = this->inheritCoverGrammar(this->matchKeyword(NewKeyword) ? &Parser::parseNewExpression : &Parser::parsePrimaryExpression);
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
                    PassRefPtr<IdentifierNode> property = this->parseIdentifierName();
                    this->trackUsingNames = trackUsingNamesBefore;
                    expr = this->finalize(this->startNode(startToken), new MemberExpressionNode(expr.get(), property.get(), true));
                } else if (this->lookahead->valuePunctuatorKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    expr = this->finalize(this->startNode(startToken), new CallExpressionNode(expr.get(), this->parseArguments()));
                } else if (this->lookahead->valuePunctuatorKind == LeftSquareBracket) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    RefPtr<Node> property = this->isolateCoverGrammar(&Parser::parseExpression);
                    this->expect(RightSquareBracket);
                    expr = this->finalize(this->startNode(startToken), new MemberExpressionNode(expr.get(), property.get(), false));
                } else {
                    break;
                }
            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                expr = this->convertTaggedTempleateExpressionToCallExpression(this->startNode(startToken), this->finalize(this->startNode(startToken), new TaggedTemplateExpressionNode(expr.get(), quasi.get())));
            } else {
                break;
            }
        }
        this->context->allowIn = previousAllowIn;

        return expr;
    }

    ScanExpressionResult scanLeftHandSideExpressionAllowCall()
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        ScanExpressionResult expr(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        if (this->context->inFunctionBody && this->matchKeyword(SuperKeyword)) {
            this->nextToken();
            this->throwError("super keyword is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // expr = this->finalize(node, new Node.Super());
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(this->lookahead);
            }
        } else {
            expr = this->scanInheritCoverGrammar(this->matchKeyword(NewKeyword) ? &Parser::scanNewExpression : &Parser::scanPrimaryExpression);
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
                    ScanExpressionResult property = this->scanIdentifierName();
                    this->trackUsingNames = trackUsingNamesBefore;
                    expr.first = ASTNodeType::MemberExpression;
                } else if (this->lookahead->valuePunctuatorKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    testCalleeExpressionInScan(expr);
                    this->scanArguments();
                    expr.first = ASTNodeType::CallExpression;
                } else if (this->lookahead->valuePunctuatorKind == LeftSquareBracket) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    ScanExpressionResult property = this->scanIsolateCoverGrammar(&Parser::scanExpression);
                    this->expect(RightSquareBracket);
                    expr.first = ASTNodeType::MemberExpression;
                } else {
                    break;
                }
            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                auto exprNode = this->convertTaggedTempleateExpressionToCallExpression(this->startNode(startToken), this->finalize(this->startNode(startToken), new TaggedTemplateExpressionNode(nullptr, quasi.get())));
                expr.first = exprNode->type();
            } else {
                break;
            }
        }
        this->context->allowIn = previousAllowIn;

        return expr;
    }

    void testCalleeExpressionInScan(ScanExpressionResult callee)
    {
        if (callee.first == ASTNodeType::Identifier && callee.second == escargotContext->staticStrings().eval) {
            scopeContexts.back()->m_hasEval = true;
            if (this->context->inArrowFunction) {
                insertUsingName(this->escargotContext->staticStrings().stringThis);
            }
        }
    }

    PassRefPtr<Node> parseSuper()
    {
        MetaNode node = this->createNode();

        this->expectKeyword(SuperKeyword);
        if (!this->match(LeftSquareBracket) && !this->match(Period)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        this->throwError("super keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        // return this->finalize(node, new Node.Super());
        return nullptr;
    }

    ScanExpressionResult scanSuper()
    {
        MetaNode node = this->createNode();

        this->expectKeyword(SuperKeyword);
        if (!this->match(LeftSquareBracket) && !this->match(Period)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        this->throwError("super keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
        // return this->finalize(node, new Node.Super());
        ScanExpressionResult expr(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        return expr;
    }

    PassRefPtr<Node> parseLeftHandSideExpression()
    {
        // assert(this->context->allowIn, 'callee of new expression always allow in keyword.');
        ASSERT(this->context->allowIn);

        MetaNode node = this->startNode(this->lookahead);
        RefPtr<Node> expr = (this->matchKeyword(SuperKeyword) && this->context->inFunctionBody) ? this->parseSuper() : this->inheritCoverGrammar(this->matchKeyword(NewKeyword) ? &Parser::parseNewExpression : &Parser::parsePrimaryExpression);

        while (true) {
            if (this->match(LeftSquareBracket)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(LeftSquareBracket);
                RefPtr<Node> property = this->isolateCoverGrammar(&Parser::parseExpression);
                this->expect(RightSquareBracket);
                expr = this->finalize(node, new MemberExpressionNode(expr.get(), property.get(), false));

            } else if (this->match(Period)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(Period);
                bool trackUsingNamesBefore = this->trackUsingNames;
                this->trackUsingNames = false;
                RefPtr<IdentifierNode> property = this->parseIdentifierName();
                this->trackUsingNames = trackUsingNamesBefore;
                expr = this->finalize(node, new MemberExpressionNode(expr.get(), property.get(), true));

            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                expr = this->convertTaggedTempleateExpressionToCallExpression(node, this->finalize(node, new TaggedTemplateExpressionNode(expr.get(), quasi.get())).get());
            } else {
                break;
            }
        }

        return expr;
    }

    ScanExpressionResult scanLeftHandSideExpression()
    {
        // assert(this->context->allowIn, 'callee of new expression always allow in keyword.');
        ASSERT(this->context->allowIn);

        MetaNode node = this->startNode(this->lookahead);
        ScanExpressionResult expr = (this->matchKeyword(SuperKeyword) && this->context->inFunctionBody) ? this->scanSuper() : this->scanInheritCoverGrammar(this->matchKeyword(NewKeyword) ? &Parser::scanNewExpression : &Parser::scanPrimaryExpression);

        while (true) {
            if (this->match(LeftSquareBracket)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(LeftSquareBracket);
                ScanExpressionResult property = this->scanIsolateCoverGrammar(&Parser::scanExpression);
                this->expect(RightSquareBracket);
                expr.first = ASTNodeType::MemberExpression;
            } else if (this->match(Period)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->expect(Period);
                bool trackUsingNamesBefore = this->trackUsingNames;
                this->trackUsingNames = false;
                this->scanIdentifierName();
                this->trackUsingNames = trackUsingNamesBefore;
                expr.first = ASTNodeType::MemberExpression;
            } else if (this->lookahead->type == Token::TemplateToken && this->lookahead->valueTemplate->head) {
                RefPtr<Node> quasi = this->parseTemplateLiteral();
                expr.first = this->convertTaggedTempleateExpressionToCallExpression(node, this->finalize(node, new TaggedTemplateExpressionNode(nullptr, quasi.get())).get())->type();
            } else {
                break;
            }
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

    PassRefPtr<Node> parseUpdateExpression()
    {
        RefPtr<Node> expr;
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            MetaNode node = this->startNode(startToken);
            RefPtr<Scanner::ScannerResult> token = this->nextToken();
            expr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
            if (expr->isLiteral() || expr->type() == ASTNodeType::ThisExpression) {
                this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
            }
            if (this->context->strict && expr->type() == Identifier && this->scanner->isRestrictedWord(((IdentifierNode*)expr.get())->name())) {
                this->tolerateError(Messages::StrictLHSPrefix);
            }
            if (!this->context->isAssignmentTarget && this->context->strict) {
                this->tolerateError(Messages::InvalidLHSInAssignment);
            }
            bool prefix = true;

            if (isPlus) {
                expr = this->finalize(node, new UpdateExpressionIncrementPrefixNode(expr.get()));
            } else {
                expr = this->finalize(node, new UpdateExpressionDecrementPrefixNode(expr.get()));
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        } else {
            expr = this->inheritCoverGrammar(&Parser::parseLeftHandSideExpressionAllowCall);
            if (!this->hasLineTerminator && this->lookahead->type == Token::PunctuatorToken) {
                if (this->match(PlusPlus) || this->match(MinusMinus)) {
                    bool isPlus = this->match(PlusPlus);
                    if (this->context->strict && expr->isIdentifier() && this->scanner->isRestrictedWord(((IdentifierNode*)expr.get())->name())) {
                        this->tolerateError(Messages::StrictLHSPostfix);
                    }
                    if (!this->context->isAssignmentTarget && this->context->strict) {
                        this->tolerateError(Messages::InvalidLHSInAssignment);
                    }
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->nextToken();

                    if (expr->isLiteral() || expr->type() == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (isPlus) {
                        expr = this->finalize(this->startNode(startToken), new UpdateExpressionIncrementPostfixNode(expr.get()));
                    } else {
                        expr = this->finalize(this->startNode(startToken), new UpdateExpressionDecrementPostfixNode(expr.get()));
                    }
                }
            }
        }

        return expr;
    }

    ScanExpressionResult scanUpdateExpression()
    {
        ScanExpressionResult expr(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            MetaNode node = this->startNode(startToken);
            RefPtr<Scanner::ScannerResult> token = this->nextToken();
            expr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
            if (expr.first == ASTNodeType::Literal || expr.first == ASTNodeType::ThisExpression) {
                this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
            }
            if (this->context->strict && expr.first == ASTNodeType::Identifier && this->scanner->isRestrictedWord(expr.second)) {
                this->tolerateError(Messages::StrictLHSPrefix);
            }
            if (!this->context->isAssignmentTarget && this->context->strict) {
                this->tolerateError(Messages::InvalidLHSInAssignment);
            }
            bool prefix = true;

            if (isPlus) {
                expr = std::make_pair(ASTNodeType::UpdateExpressionIncrementPrefix, AtomicString());
            } else {
                expr = std::make_pair(ASTNodeType::UpdateExpressionDecrementPrefix, AtomicString());
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        } else {
            expr = this->scanInheritCoverGrammar(&Parser::scanLeftHandSideExpressionAllowCall);
            if (!this->hasLineTerminator && this->lookahead->type == Token::PunctuatorToken) {
                if (this->match(PlusPlus) || this->match(MinusMinus)) {
                    bool isPlus = this->match(PlusPlus);
                    if (this->context->strict && expr.first == ASTNodeType::Identifier && this->scanner->isRestrictedWord(expr.second)) {
                        this->tolerateError(Messages::StrictLHSPostfix);
                    }
                    if (!this->context->isAssignmentTarget && this->context->strict) {
                        this->tolerateError(Messages::InvalidLHSInAssignment);
                    }
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->nextToken();

                    if (expr.first == ASTNodeType::Literal || expr.first == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (isPlus) {
                        expr = std::make_pair(ASTNodeType::UpdateExpressionIncrementPostfix, AtomicString());
                    } else {
                        expr = std::make_pair(ASTNodeType::UpdateExpressionDecrementPostfix, AtomicString());
                    }
                }
            }
        }

        return expr;
    }

    // ECMA-262 12.5 Unary Operators

    PassRefPtr<Node> parseUnaryExpression()
    {
        bool matchPun = this->lookahead->type == Token::PunctuatorToken;
        if (matchPun) {
            auto punctuatorsKind = this->lookahead->valuePunctuatorKind;
            if (punctuatorsKind == Plus) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionPlusNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == Minus) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionMinusNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == Wave) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionBitwiseNotNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == ExclamationMark) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionLogicalNotNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            }
        }

        bool isKeyword = this->lookahead->type == Token::KeywordToken;

        if (isKeyword) {
            if (this->lookahead->valueKeywordKind == DeleteKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionDeleteNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (this->context->strict && subExpr->isIdentifier()) {
                    this->tolerateError(Messages::StrictDelete);
                }
                if (subExpr->isIdentifier()) {
                    this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                }
                return expr;
            } else if (this->lookahead->valueKeywordKind == VoidKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionVoidNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (this->lookahead->valueKeywordKind == TypeofKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionTypeOfNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (subExpr->isIdentifier()) {
                    AtomicString s = subExpr->asIdentifier()->name();
                    if (!this->scopeContexts.back()->hasName(s)) {
                        this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                    }
                }
                return expr;
            }
        }

        return this->parseUpdateExpression();
    }

    ScanExpressionResult scanUnaryExpression()
    {
        bool matchPun = this->lookahead->type == Token::PunctuatorToken;
        if (matchPun) {
            auto punctuatorsKind = this->lookahead->valuePunctuatorKind;
            if (punctuatorsKind == Plus) {
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionPlus, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == Minus) {
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionMinus, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == Wave) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionBitwiseNot, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (punctuatorsKind == ExclamationMark) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionLogicalNot, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            }
        }

        bool isKeyword = this->lookahead->type == Token::KeywordToken;

        if (isKeyword) {
            if (this->lookahead->valueKeywordKind == DeleteKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionDelete, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (this->context->strict && subExpr.first == ASTNodeType::Identifier) {
                    this->tolerateError(Messages::StrictDelete);
                }
                if (subExpr.first == ASTNodeType::Identifier) {
                    this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                }
                return expr;
            } else if (this->lookahead->valueKeywordKind == VoidKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionVoid, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (this->lookahead->valueKeywordKind == TypeofKeyword) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionTypeOf, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                if (subExpr.first == ASTNodeType::Identifier) {
                    AtomicString s = subExpr.second;
                    if (!this->scopeContexts.back()->hasName(s)) {
                        this->scopeContexts.back()->m_hasEvaluateBindingId = true;
                    }
                }
                return expr;
            }
        }

        return this->scanUpdateExpression();
    }

    PassRefPtr<Node> parseExponentiationExpression()
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
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
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
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
        Node* nd;
        if (token->type == Token::PunctuatorToken) {
            PunctuatorKind oper = token->valuePunctuatorKind;
            // Additive Operators
            if (oper == Plus) {
                nd = new BinaryExpressionPlusNode(left, right);
            } else if (oper == Minus) {
                nd = new BinaryExpressionMinusNode(left, right);
            } else if (oper == LeftShift) { // Bitwise Shift Operators
                nd = new BinaryExpressionLeftShiftNode(left, right);
            } else if (oper == RightShift) {
                nd = new BinaryExpressionSignedRightShiftNode(left, right);
            } else if (oper == UnsignedRightShift) {
                nd = new BinaryExpressionUnsignedRightShiftNode(left, right);
            } else if (oper == Multiply) { // Multiplicative Operators
                nd = new BinaryExpressionMultiplyNode(left, right);
            } else if (oper == Divide) {
                nd = new BinaryExpressionDivisionNode(left, right);
            } else if (oper == Mod) {
                nd = new BinaryExpressionModNode(left, right);
            } else if (oper == LeftInequality) { // Relational Operators
                nd = new BinaryExpressionLessThanNode(left, right);
            } else if (oper == RightInequality) {
                nd = new BinaryExpressionGreaterThanNode(left, right);
            } else if (oper == LeftInequalityEqual) {
                nd = new BinaryExpressionLessThanOrEqualNode(left, right);
            } else if (oper == RightInequalityEqual) {
                nd = new BinaryExpressionGreaterThanOrEqualNode(left, right);
            } else if (oper == Equal) { // Equality Operators
                nd = new BinaryExpressionEqualNode(left, right);
            } else if (oper == NotEqual) {
                nd = new BinaryExpressionNotEqualNode(left, right);
            } else if (oper == StrictEqual) {
                nd = new BinaryExpressionStrictEqualNode(left, right);
            } else if (oper == NotStrictEqual) {
                nd = new BinaryExpressionNotStrictEqualNode(left, right);
            } else if (oper == BitwiseAnd) { // Binary Bitwise Operator
                nd = new BinaryExpressionBitwiseAndNode(left, right);
            } else if (oper == BitwiseXor) {
                nd = new BinaryExpressionBitwiseXorNode(left, right);
            } else if (oper == BitwiseOr) {
                nd = new BinaryExpressionBitwiseOrNode(left, right);
            } else if (oper == LogicalOr) {
                nd = new BinaryExpressionLogicalOrNode(left, right);
            } else if (oper == LogicalAnd) {
                nd = new BinaryExpressionLogicalAndNode(left, right);
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ASSERT(token->type == Token::KeywordToken);
            if (token->valueKeywordKind == InKeyword) {
                nd = new BinaryExpressionInNode(left, right);
            } else if (token->valueKeywordKind == KeywordKind::InstanceofKeyword) {
                nd = new BinaryExpressionInstanceOfNode(left, right);
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        return nd;
    }

    ScanExpressionResult scanBinaryExpression(ScanExpressionResult left, ScanExpressionResult right, Scanner::ScannerResult* token)
    {
        ScanExpressionResult nd(ASTNodeType::ASTNodeTypeError, AtomicString());
        if (token->type == Token::PunctuatorToken) {
            PunctuatorKind oper = token->valuePunctuatorKind;
            // Additive Operators
            if (oper == Plus) {
                nd.first = ASTNodeType::BinaryExpressionPlus;
            } else if (oper == Minus) {
                nd.first = ASTNodeType::BinaryExpressionMinus;
            } else if (oper == LeftShift) { // Bitwise Shift Operators
                nd.first = ASTNodeType::BinaryExpressionLeftShift;
            } else if (oper == RightShift) {
                nd.first = ASTNodeType::BinaryExpressionSignedRightShift;
            } else if (oper == UnsignedRightShift) {
                nd.first = ASTNodeType::BinaryExpressionUnsignedRightShift;
            } else if (oper == Multiply) { // Multiplicative Operators
                nd.first = ASTNodeType::BinaryExpressionMultiply;
            } else if (oper == Divide) {
                nd.first = ASTNodeType::BinaryExpressionDivison;
            } else if (oper == Mod) {
                nd.first = ASTNodeType::BinaryExpressionMod;
            } else if (oper == LeftInequality) { // Relational Operators
                nd.first = ASTNodeType::BinaryExpressionLessThan;
            } else if (oper == RightInequality) {
                nd.first = ASTNodeType::BinaryExpressionGreaterThan;
            } else if (oper == LeftInequalityEqual) {
                nd.first = ASTNodeType::BinaryExpressionLessThanOrEqual;
            } else if (oper == RightInequalityEqual) {
                nd.first = ASTNodeType::BinaryExpressionGreaterThanOrEqual;
            } else if (oper == Equal) { // Equality Operators
                nd.first = ASTNodeType::BinaryExpressionEqual;
            } else if (oper == NotEqual) {
                nd.first = ASTNodeType::BinaryExpressionNotEqual;
            } else if (oper == StrictEqual) {
                nd.first = ASTNodeType::BinaryExpressionStrictEqual;
            } else if (oper == NotStrictEqual) {
                nd.first = ASTNodeType::BinaryExpressionNotStrictEqual;
            } else if (oper == BitwiseAnd) { // Binary Bitwise Operator
                nd.first = ASTNodeType::BinaryExpressionBitwiseAnd;
            } else if (oper == BitwiseXor) {
                nd.first = ASTNodeType::BinaryExpressionBitwiseXor;
            } else if (oper == BitwiseOr) {
                nd.first = ASTNodeType::BinaryExpressionBitwiseOr;
            } else if (oper == LogicalOr) {
                nd.first = ASTNodeType::BinaryExpressionLogicalOr;
            } else if (oper == LogicalAnd) {
                nd.first = ASTNodeType::BinaryExpressionLogicalAnd;
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ASSERT(token->type == Token::KeywordToken);
            if (token->valueKeywordKind == InKeyword) {
                nd.first = ASTNodeType::BinaryExpressionIn;
            } else if (token->valueKeywordKind == KeywordKind::InstanceofKeyword) {
                nd.first = ASTNodeType::BinaryExpressionInstanceOf;
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        return nd;
    }

    // ECMA-262 12.14 Conditional Operator

    PassRefPtr<Node> parseConditionalExpression()
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;

        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::parseBinaryExpression);
        if (this->match(GuessMark)) {
            this->nextToken();

            bool previousAllowIn = this->context->allowIn;
            this->context->allowIn = true;
            RefPtr<Node> consequent = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
            this->context->allowIn = previousAllowIn;

            this->expect(Colon);
            RefPtr<Node> alternate = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);

            expr = this->finalize(this->startNode(startToken), new ConditionalExpressionNode(expr.get(), consequent.get(), alternate.get()));
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        }

        return expr.release();
    }

    ScanExpressionResult scanConditionalExpression()
    {
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::scanBinaryExpressions);
        if (this->match(GuessMark)) {
            this->nextToken();

            bool previousAllowIn = this->context->allowIn;
            this->context->allowIn = true;
            ScanExpressionResult consequent = this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
            this->context->allowIn = previousAllowIn;

            this->expect(Colon);
            ScanExpressionResult alternate = this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);

            expr = std::make_pair(ASTNodeType::ConditionalExpression, AtomicString());
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        }

        return expr;
    }

    // ECMA-262 12.15 Assignment Operators
    void checkPatternParam(ParseParameterOptions& options, Node* param)
    {
        switch (param->type()) {
        case Identifier:
            this->validateParam(options, nullptr, ((IdentifierNode*)param)->name());
            break;
        case RestElement:
            this->checkPatternParam(options, ((RestElementNode*)param)->argument());
            break;
        /*
        // case AssignmentPattern:
        case AssignmentExpression:
        case AssignmentExpressionBitwiseAnd:
        case AssignmentExpressionBitwiseOr:
        case AssignmentExpressionBitwiseXor:
        case AssignmentExpressionDivision:
        case AssignmentExpressionLeftShift:
        case AssignmentExpressionMinus:
        case AssignmentExpressionMod:
        case AssignmentExpressionMultiply:
        case AssignmentExpressionPlus:
        case AssignmentExpressionSignedRightShift:
        case AssignmentExpressionUnsignedRightShift:
        case AssignmentExpressionSimple:
            this->checkPatternParam(options, ((AssignmentExpressionSimpleNode*)param)->left());
            break;
            */
        default:
            RELEASE_ASSERT_NOT_REACHED();
            /*
             case Syntax.ArrayPattern:
                 for (let i = 0; i < param.elements.length; i++) {
                     if (param.elements[i] !== null) {
                         this->checkPatternParam(options, param.elements[i]);
                     }
                 }
                 break;
             case Syntax.YieldExpression:
                 break;
             default:
                 RELEASE_ASSERT(param->type() == Object)
                 assert(param.type === Syntax.ObjectPattern, 'Invalid type');
                 for (let i = 0; i < param.properties.length; i++) {
                     this->checkPatternParam(options, param.properties[i].value);
                 }
                 break;
                 */
        }
    }

    ParseFormalParametersResult reinterpretAsCoverFormalsList(Node* expr)
    {
        PatternNodeVector params;
        params.push_back(expr);
        ParseParameterOptions options;

        switch (expr->type()) {
        case Identifier:
            break;
        case ArrowParameterPlaceHolder:
            params = ((ArrowParameterPlaceHolderNode*)expr)->params();
            break;
        default:
            return ParseFormalParametersResult();
        }

        for (size_t i = 0; i < params.size(); ++i) {
            RefPtr<Node> param = params[i];
            /*
             if (param->type() == AssignmentPattern) {
                 if (param->right()->type() == YieldExpression) {
                    if (param.right.argument) {
                        this.throwUnexpectedToken(this.lookahead);
                    }
                    param.right.type = Syntax.Identifier;
                    param.right.name = 'yield';
                    delete param.right.argument;
                    delete param.right.delegate;
                 }
             }
             */
            this->checkPatternParam(options, param.get());
            //params[i] = param;
        }

        if (this->context->strict || !this->context->allowYield) {
            for (size_t i = 0; i < params.size(); ++i) {
                RefPtr<Node> param = params[i];
                if (param->type() == YieldExpression) {
                    this->throwUnexpectedToken(this->lookahead);
                }
            }
        }

        if (options.message == Messages::StrictParamDupe) {
            RefPtr<Scanner::ScannerResult> token = this->context->strict ? options.stricted : options.firstRestricted;
            this->throwUnexpectedToken(token, options.message);
        }

        return ParseFormalParametersResult(params, options.stricted, options.firstRestricted, options.message);
    }

    PassRefPtr<Node> parseAssignmentExpression()
    {
        RefPtr<Node> expr;

        if (!this->context->allowYield && this->matchKeyword(YieldKeyword)) {
            expr = this->parseYieldExpression();
        } else {
            RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
            RefPtr<Scanner::ScannerResult> token = startToken;
            expr = this->parseConditionalExpression();

            /*
            if (token.type === Token.Identifier && (token.lineNumber === this.lookahead.lineNumber) && token.value === 'async' && (this.lookahead.type === Token.Identifier)) {
                const arg = this.parsePrimaryExpression();
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [arg],
                    async: true
                };
            } */

            if (expr->type() == ArrowParameterPlaceHolder || this->match(Arrow)) {
                // ECMA-262 14.2 Arrow Function Definitions
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                ParseFormalParametersResult list = this->reinterpretAsCoverFormalsList(expr.get()); //TODO

                if (list.valid) {
                    if (this->hasLineTerminator) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    this->context->firstCoverInitializedNameError = nullptr;

                    pushScopeContext(list.params, AtomicString());

                    extractNamesFromFunctionParams(list.params);
                    bool previousStrict = this->context->strict;
                    bool previousAllowYield = this->context->allowYield;
                    bool previousInArrowFunction = this->context->inArrowFunction;
                    this->context->allowYield = true;
                    this->context->inArrowFunction = true;

                    MetaNode node = this->startNode(startToken);
                    MetaNode nodeStart = this->createNode();

                    this->expect(Arrow);
                    RefPtr<Node> body = this->match(LeftBrace) ? this->parseFunctionSourceElements() : this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    bool expression = body->type() != BlockStatement;
                    if (expression) {
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
                    expr = this->finalize(node, new ArrowFunctionExpressionNode(std::move(list.params), body.get(), popScopeContext(node), expression)); //TODO

                    this->context->strict = previousStrict;
                    this->context->allowYield = previousAllowYield;
                    this->context->inArrowFunction = previousInArrowFunction;
                }
            } else {
                if (this->matchAssign()) {
                    if (!this->context->isAssignmentTarget && this->context->strict) {
                        this->tolerateError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (this->context->strict && expr->type() == Identifier) {
                        IdentifierNode* id = expr->asIdentifier();
                        if (this->scanner->isRestrictedWord(id->name())) {
                            this->throwUnexpectedToken(token, Messages::StrictLHSAssignment);
                        }
                        if (this->scanner->isStrictModeReservedWord(id->name())) {
                            this->throwUnexpectedToken(token, Messages::StrictReservedWord);
                        }
                    }

                    if (!this->match(Substitution)) {
                        this->context->isAssignmentTarget = false;
                        this->context->isBindingElement = false;
                    } else {
                        this->reinterpretExpressionAsPattern(expr.get());
                    }

                    if (expr->isLiteral() || expr->type() == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    Node* exprResult;
                    token = this->nextToken();
                    RefPtr<Node> right = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    if (token->valuePunctuatorKind == Substitution) {
                        exprResult = new AssignmentExpressionSimpleNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == PlusEqual) {
                        exprResult = new AssignmentExpressionPlusNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == MinusEqual) {
                        exprResult = new AssignmentExpressionMinusNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == MultiplyEqual) {
                        exprResult = new AssignmentExpressionMultiplyNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == DivideEqual) {
                        exprResult = new AssignmentExpressionDivisionNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == ModEqual) {
                        exprResult = new AssignmentExpressionModNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == LeftShiftEqual) {
                        exprResult = new AssignmentExpressionLeftShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == RightShiftEqual) {
                        exprResult = new AssignmentExpressionSignedRightShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == UnsignedRightShiftEqual) {
                        exprResult = new AssignmentExpressionUnsignedShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == BitwiseXorEqual) {
                        exprResult = new AssignmentExpressionBitwiseXorNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == BitwiseAndEqual) {
                        exprResult = new AssignmentExpressionBitwiseAndNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorKind == BitwiseOrEqual) {
                        exprResult = new AssignmentExpressionBitwiseOrNode(expr.get(), right.get());
                    } else {
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    expr = this->finalize(this->startNode(startToken), exprResult);
                    this->context->firstCoverInitializedNameError = nullptr;
                }
            }
        }
        return expr.release();
    }

    ScanExpressionResult scanAssignmentExpression()
    {
        ScanExpressionResult expr;

        if (!this->context->allowYield && this->matchKeyword(YieldKeyword)) {
            expr = this->scanYieldExpression();
        } else {
            RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
            RefPtr<Scanner::ScannerResult> token = startToken;
            auto lastMarker = this->lastMarker;
            expr = this->scanConditionalExpression();

            /*
            if (token.type === Token.Identifier && (token.lineNumber === this.lookahead.lineNumber) && token.value === 'async' && (this.lookahead.type === Token.Identifier)) {
                const arg = this.parsePrimaryExpression();
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [arg],
                    async: true
                };
            } */

            if (expr.first == ArrowParameterPlaceHolder || this->match(Arrow)) {
                // ECMA-262 14.2 Arrow Function Definitions
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                // rewind scanner for return to normal mode
                this->scanner->index = lastMarker.index;
                this->scanner->lineNumber = lastMarker.lineNumber;
                this->scanner->lineStart = lastMarker.lineStart;
                this->nextToken();

                RefPtr<Node> exprNode = this->parseConditionalExpression();

                ParseFormalParametersResult list = this->reinterpretAsCoverFormalsList(exprNode.get()); //TODO

                if (list.valid) {
                    if (this->hasLineTerminator) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    this->context->firstCoverInitializedNameError = nullptr;

                    pushScopeContext(list.params, AtomicString());

                    extractNamesFromFunctionParams(list.params);
                    bool previousStrict = this->context->strict;
                    bool previousAllowYield = this->context->allowYield;
                    bool previousInArrowFunction = this->context->inArrowFunction;
                    this->context->allowYield = true;
                    this->context->inArrowFunction = true;

                    MetaNode node = this->startNode(startToken);
                    MetaNode nodeStart = this->createNode();

                    this->expect(Arrow);
                    RefPtr<Node> body = this->match(LeftBrace) ? this->parseFunctionSourceElements() : this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    bool expression = body->type() != BlockStatement;
                    if (expression) {
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
                    exprNode = this->finalize(node, new ArrowFunctionExpressionNode(std::move(list.params), body.get(), popScopeContext(node), expression)); //TODO

                    this->context->strict = previousStrict;
                    this->context->allowYield = previousAllowYield;
                    this->context->inArrowFunction = previousInArrowFunction;
                }

                expr.first = ASTNodeType::ArrowFunctionExpression;
            } else {
                if (this->matchAssign()) {
                    if (!this->context->isAssignmentTarget && this->context->strict) {
                        this->tolerateError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    if (this->context->strict && expr.first == ASTNodeType::Identifier) {
                        if (this->scanner->isRestrictedWord(expr.second)) {
                            this->throwUnexpectedToken(token, Messages::StrictLHSAssignment);
                        }
                        if (this->scanner->isStrictModeReservedWord(expr.second)) {
                            this->throwUnexpectedToken(token, Messages::StrictReservedWord);
                        }
                    }

                    if (!this->match(Substitution)) {
                        this->context->isAssignmentTarget = false;
                        this->context->isBindingElement = false;
                    } else {
                        this->scanReinterpretExpressionAsPattern(expr);
                    }

                    if (expr.first == ASTNodeType::Literal || expr.first == ASTNodeType::ThisExpression) {
                        this->throwError(Messages::InvalidLHSInAssignment, String::emptyString, String::emptyString, ErrorObject::ReferenceError);
                    }

                    ScanExpressionResult exprResult(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
                    token = this->nextToken();
                    ScanExpressionResult right = this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
                    if (token->valuePunctuatorKind == Substitution) {
                        exprResult.first = ASTNodeType::AssignmentExpressionSimple;
                    } else if (token->valuePunctuatorKind == PlusEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionPlus;
                    } else if (token->valuePunctuatorKind == MinusEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMinus;
                    } else if (token->valuePunctuatorKind == MultiplyEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMultiply;
                    } else if (token->valuePunctuatorKind == DivideEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionDivision;
                    } else if (token->valuePunctuatorKind == ModEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMod;
                    } else if (token->valuePunctuatorKind == LeftShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionLeftShift;
                    } else if (token->valuePunctuatorKind == RightShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionSignedRightShift;
                    } else if (token->valuePunctuatorKind == UnsignedRightShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionUnsignedRightShift;
                    } else if (token->valuePunctuatorKind == BitwiseXorEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionBitwiseXor;
                    } else if (token->valuePunctuatorKind == BitwiseAndEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionBitwiseAnd;
                    } else if (token->valuePunctuatorKind == BitwiseOrEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionBitwiseOr;
                    } else {
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    expr = exprResult;
                    this->context->firstCoverInitializedNameError = nullptr;
                }
            }
        }
        return expr;
    }

    // ECMA-262 12.16 Comma Operator

    PassRefPtr<Node> parseExpression()
    {
        RefPtr<Scanner::ScannerResult> startToken = this->lookahead;
        RefPtr<Node> expr = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);

        if (this->match(Comma)) {
            ExpressionNodeVector expressions;
            expressions.push_back(expr);
            while (this->startMarker.index < this->scanner->length) {
                if (!this->match(Comma)) {
                    break;
                }
                this->nextToken();
                expressions.push_back(this->isolateCoverGrammar(&Parser::parseAssignmentExpression));
            }

            expr = this->finalize(this->startNode(startToken), new SequenceExpressionNode(std::move(expressions)));
        }

        return expr.release();
    }

    ScanExpressionResult scanExpression()
    {
        ScanExpressionResult expr = this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);

        if (this->match(Comma)) {
            while (this->startMarker.index < this->scanner->length) {
                if (!this->match(Comma)) {
                    break;
                }
                this->nextToken();
                this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
            }

            expr = std::make_pair(ASTNodeType::SequenceExpression, AtomicString());
        }

        return expr;
    }

    // ECMA-262 13.2 Block

    PassRefPtr<StatementNode> parseStatementListItem()
    {
        RefPtr<StatementNode> statement;
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        if (this->lookahead->type == KeywordToken) {
            switch (this->lookahead->valueKeywordKind) {
            case FunctionKeyword:
                statement = this->parseFunctionDeclaration();
                break;
            default:
                statement = this->parseStatement();
                break;
            }
        } else {
            statement = this->parseStatement();
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

        return statement.release();
    }

    void scanStatementListItem()
    {
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        if (this->lookahead->type == KeywordToken) {
            switch (this->lookahead->valueKeywordKind) {
            case FunctionKeyword:
                this->parseFunctionDeclaration();
                break;
            default:
                this->scanStatement();
                break;
            }
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
    }

    PassRefPtr<BlockStatementNode> parseBlock()
    {
        this->expect(LeftBrace);
        MetaNode node = this->createNode();
        RefPtr<StatementContainer> block = StatementContainer::create();
        StatementNode* referNode = nullptr;
        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
            referNode = block->appendChild(this->parseStatementListItem().get(), referNode);
        }
        this->expect(RightBrace);

        return this->finalize(node, new BlockStatementNode(block.get()));
    }

    void scanBlock()
    {
        this->expect(LeftBrace);
        MetaNode node = this->createNode();
        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
            this->scanStatementListItem();
        }
        this->expect(RightBrace);
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
                this->tolerateError(Messages.StrictVarName);
            }
        }

        let init = null;
        if (kind === 'const') {
            if (!this->matchKeyword('in') && !this->matchContextualKeyword('of')) {
                this->expect('=');
                init = this->isolateCoverGrammar(this->parseAssignmentExpression);
            }
        } else if ((!options.inFor && id.type !== Syntax.Identifier) || this->match('=')) {
            this->expect('=');
            init = this->isolateCoverGrammar(this->parseAssignmentExpression);
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
                const expr = this->parseAssignmentExpression();
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
            const right = this->isolateCoverGrammar(this->parseAssignmentExpression);
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

    AtomicString scanVariableIdentifier(KeywordKind kind = KeywordKindEnd)
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

        return finishScanIdentifier(token, true).second;
    }

    struct DeclarationOptions {
        bool inFor;
    };

    PassRefPtr<VariableDeclaratorNode> parseVariableDeclaration(DeclarationOptions& options)
    {
        MetaNode node = this->createNode();

        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> params;
        RefPtr<Node> id = this->parsePattern(params, VarKeyword);

        // ECMA-262 12.2.1
        if (this->context->strict && id->type() == Identifier) {
            if (this->scanner->isRestrictedWord(((IdentifierNode*)id.get())->name())) {
                this->tolerateError(Messages::StrictVarName);
            }
        }

        if (id->type() == Identifier && !this->config.parseSingleFunction) {
            this->scopeContexts.back()->insertName(((IdentifierNode*)id.get())->name(), true);
        }

        RefPtr<Node> init;
        if (this->match(Substitution)) {
            this->nextToken();
            init = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
        } else if (id->type() != Identifier && !options.inFor) {
            this->expect(Substitution);
        }

        return this->finalize(node, new VariableDeclaratorNode(id.get(), init.get()));
    }

    void scanVariableDeclaration(DeclarationOptions& options)
    {
        std::vector<RefPtr<Scanner::ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<Scanner::ScannerResult>>> params;
        std::pair<ASTNodeType, AtomicString> id = this->scanPattern(params, VarKeyword);

        // ECMA-262 12.2.1
        if (this->context->strict && id.first == Identifier) {
            if (this->scanner->isRestrictedWord(id.second)) {
                this->tolerateError(Messages::StrictVarName);
            }
        }

        if (id.first == Identifier && !this->config.parseSingleFunction) {
            this->scopeContexts.back()->insertName(id.second, true);
        }

        if (this->match(Substitution)) {
            this->nextToken();
            this->scanIsolateCoverGrammar(&Parser::scanAssignmentExpression);
        } else if (id.first != Identifier && !options.inFor) {
            this->expect(Substitution);
        }
    }

    VariableDeclaratorVector parseVariableDeclarationList(DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;

        VariableDeclaratorVector list;
        list.push_back(this->parseVariableDeclaration(opt));
        while (this->match(Comma)) {
            this->nextToken();
            list.push_back(this->parseVariableDeclaration(opt));
        }

        return list;
    }

    size_t scanVariableDeclarationList(DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;
        size_t listSize = 0;

        this->scanVariableDeclaration(opt);
        listSize++;
        while (this->match(Comma)) {
            this->nextToken();
            this->scanVariableDeclaration(opt);
            listSize++;
        }
        return listSize;
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

    PassRefPtr<EmptyStatementNode> parseEmptyStatement()
    {
        this->expect(SemiColon);
        MetaNode node = this->createNode();
        return this->finalize(node, new EmptyStatementNode());
    }

    void scanEmptyStatement()
    {
        this->expect(SemiColon);
    }

    // ECMA-262 13.5 Expression Statement

    PassRefPtr<ExpressionStatementNode> parseExpressionStatement()
    {
        MetaNode node = this->createNode();
        RefPtr<Node> expr = this->parseExpression();
        this->consumeSemicolon();
        return this->finalize(node, new ExpressionStatementNode(expr.get()));
    }

    void scanExpressionStatement()
    {
        this->scanExpression();
        this->consumeSemicolon();
    }

    // ECMA-262 13.6 If statement

    PassRefPtr<IfStatementNode> parseIfStatement()
    {
        RefPtr<Node> consequent;
        RefPtr<Node> alternate;

        this->expectKeyword(IfKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> test = this->parseExpression();

        this->expect(RightParenthesis);
        consequent = this->parseStatement(this->context->strict ? false : true);
        if (this->matchKeyword(ElseKeyword)) {
            this->nextToken();
            alternate = this->parseStatement();
        }

        return this->finalize(node, new IfStatementNode(test.get(), consequent.get(), alternate.get()));
    }

    void scanIfStatement()
    {
        this->expectKeyword(IfKeyword);
        this->expect(LeftParenthesis);
        this->scanExpression();

        this->expect(RightParenthesis);
        this->scanStatement(this->context->strict ? false : true);
        if (this->matchKeyword(ElseKeyword)) {
            this->nextToken();
            this->scanStatement();
        }
    }

    // ECMA-262 13.7.2 The do-while Statement

    PassRefPtr<DoWhileStatementNode> parseDoWhileStatement()
    {
        this->expectKeyword(DoKeyword);
        MetaNode node = this->createNode();

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        RefPtr<Node> body = this->parseStatement(false);
        this->context->inIteration = previousInIteration;

        this->expectKeyword(WhileKeyword);
        this->expect(LeftParenthesis);
        RefPtr<Node> test = this->parseExpression();
        this->expect(RightParenthesis);
        if (this->match(SemiColon)) {
            this->nextToken();
        }

        return this->finalize(node, new DoWhileStatementNode(test.get(), body.get()));
    }

    void scanDoWhileStatement()
    {
        this->expectKeyword(DoKeyword);
        MetaNode node = this->createNode();

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        this->scanStatement(false);
        this->context->inIteration = previousInIteration;

        this->expectKeyword(WhileKeyword);
        this->expect(LeftParenthesis);
        this->scanExpression();
        this->expect(RightParenthesis);
        if (this->match(SemiColon)) {
            this->nextToken();
        }
    }

    // ECMA-262 13.7.3 The while Statement

    PassRefPtr<WhileStatementNode> parseWhileStatement()
    {
        RefPtr<Node> body;

        bool prevInLoop = this->context->inLoop;
        this->context->inLoop = true;

        this->expectKeyword(WhileKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> test = this->parseExpression();

        this->expect(RightParenthesis);

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        body = this->parseStatement(false);
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;

        return this->finalize(node, new WhileStatementNode(test.get(), body.get()));
    }

    void scanWhileStatement()
    {
        bool prevInLoop = this->context->inLoop;
        this->context->inLoop = true;

        this->expectKeyword(WhileKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        this->scanExpression();

        this->expect(RightParenthesis);
        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        this->scanStatement(false);
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;
    }

    // ECMA-262 13.7.4 The for Statement
    // ECMA-262 13.7.5 The for-in and for-of Statements

    PassRefPtr<StatementNode> parseForStatement()
    {
        RefPtr<Node> init;
        RefPtr<Node> test;
        RefPtr<Node> update;
        bool forIn = true;
        RefPtr<Node> left;
        RefPtr<Node> right;

        bool prevInLoop = this->context->inLoop;

        this->expectKeyword(ForKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(VarKeyword)) {
                MetaNode metaInit = this->createNode();
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
                        this->tolerateError(Messages::ForInOfLoopInitializer, new ASCIIString("for-in"));
                    }
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    this->nextToken();
                    left = init;
                    right = this->parseExpression();
                    init = nullptr;
                } else if (declarations.size() == 1 && declarations[0]->init() == nullptr
                           && this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    this->nextToken();
                    left = init;
                    right = this->parseAssignmentExpression();
                    init = nullptr;
                    forIn = false;
                } else {
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
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
                    right = this->parseExpression();
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
                        right = this->parseExpression();
                        init = null;
                    } else if (declarations.length === 1 && declarations[0].init === null && this->matchContextualKeyword('of')) {
                        init = this->finalize(init, new Node.VariableDeclaration(declarations, kind));
                        this->nextToken();
                        left = init;
                        right = this->parseAssignmentExpression();
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
                init = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(InKeyword)) {
                    if (init->isLiteral() || init->type() == ASTNodeType::AssignmentExpression || init->type() == ASTNodeType::ThisExpression) {
                        this->tolerateError(Messages::InvalidLHSInForIn);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init.get());
                    left = init;
                    right = this->parseExpression();
                    init = nullptr;
                } else if (this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    this->throwError("for of is not supported yet");
                    RELEASE_ASSERT_NOT_REACHED();
                    /*
                    if (!this->context->isAssignmentTarget || init.type === Syntax.AssignmentExpression) {
                        this->tolerateError(Messages.InvalidLHSInForLoop);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init);
                    left = init;
                    right = this->parseAssignmentExpression();
                    init = null;
                    forIn = false;
                    */
                } else {
                    if (this->match(Comma)) {
                        ExpressionNodeVector initSeq;
                        initSeq.push_back(init);
                        while (this->match(Comma)) {
                            this->nextToken();
                            initSeq.push_back(this->isolateCoverGrammar(&Parser::parseAssignmentExpression));
                        }
                        init = this->finalize(this->startNode(initStartToken), new SequenceExpressionNode(std::move(initSeq)));
                    }
                    this->expect(SemiColon);
                }
            }
        }

        if (left == nullptr) {
            this->context->inLoop = true;
            if (!this->match(SemiColon)) {
                test = this->parseExpression();
            }
            this->expect(SemiColon);
            if (!this->match(RightParenthesis)) {
                update = this->parseExpression();
            }
        }

        RefPtr<Node> body = nullptr;

        this->expect(RightParenthesis);

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        auto functor = std::bind(&Parser::parseStatement, std::ref(*this), false);
        body = this->isolateCoverGrammarWithFunctor(functor);
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;

        if (left == nullptr) {
            return this->finalize(node, new ForStatementNode(init.get(), test.get(), update.get(), body.get()));
        } else {
            if (forIn) {
                return this->finalize(node, new ForInStatementNode(left.get(), right.get(), body.get(), false));
            } else {
                this->throwError("For of is not supported yet");
                RELEASE_ASSERT_NOT_REACHED();
                // return this->finalize(node, new Node.ForOfStatement(left, right, body));
            }
        }
        /*
        return (typeof left === 'undefined') ?
            this->finalize(node, new Node.ForStatement(init, test, update, body)) :
            forIn ? this->finalize(node, new Node.ForInStatement(left, right, body)) :
                this->finalize(node, new Node.ForOfStatement(left, right, body));

        */
    }

    void scanForStatement()
    {
        RefPtr<Node> init;
        RefPtr<Node> test;
        RefPtr<Node> update;
        bool forIn = true;
        RefPtr<Node> left;
        RefPtr<Node> right;

        bool prevInLoop = this->context->inLoop;

        this->expectKeyword(ForKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(VarKeyword)) {
                MetaNode metaInit = this->createNode();
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
                        this->tolerateError(Messages::ForInOfLoopInitializer, new ASCIIString("for-in"));
                    }
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    this->nextToken();
                    left = init;
                    right = this->parseExpression();
                    init = nullptr;
                } else if (declarations.size() == 1 && declarations[0]->init() == nullptr
                           && this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
                    this->nextToken();
                    left = init;
                    right = this->parseAssignmentExpression();
                    init = nullptr;
                    forIn = false;
                } else {
                    init = this->finalize(metaInit, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
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
                    right = this->parseExpression();
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
                        right = this->parseExpression();
                        init = null;
                    } else if (declarations.length === 1 && declarations[0].init === null && this->matchContextualKeyword('of')) {
                        init = this->finalize(init, new Node.VariableDeclaration(declarations, kind));
                        this->nextToken();
                        left = init;
                        right = this->parseAssignmentExpression();
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
                init = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(InKeyword)) {
                    if (init->isLiteral() || init->type() == ASTNodeType::AssignmentExpression || init->type() == ASTNodeType::ThisExpression) {
                        this->tolerateError(Messages::InvalidLHSInForIn);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init.get());
                    left = init;
                    right = this->parseExpression();
                    init = nullptr;
                } else if (this->lookahead->type == Token::IdentifierToken && this->lookahead->relatedSource() == "of") {
                    this->throwError("for of is not supported yet");
                    RELEASE_ASSERT_NOT_REACHED();
                    /*
                    if (!this->context->isAssignmentTarget || init.type === Syntax.AssignmentExpression) {
                        this->tolerateError(Messages.InvalidLHSInForLoop);
                    }

                    this->nextToken();
                    this->reinterpretExpressionAsPattern(init);
                    left = init;
                    right = this->parseAssignmentExpression();
                    init = null;
                    forIn = false;
                    */
                } else {
                    if (this->match(Comma)) {
                        ExpressionNodeVector initSeq;
                        initSeq.push_back(init);
                        while (this->match(Comma)) {
                            this->nextToken();
                            initSeq.push_back(this->isolateCoverGrammar(&Parser::parseAssignmentExpression));
                        }
                        init = this->finalize(this->startNode(initStartToken), new SequenceExpressionNode(std::move(initSeq)));
                    }
                    this->expect(SemiColon);
                }
            }
        }

        if (left == nullptr) {
            this->context->inLoop = true;
            if (!this->match(SemiColon)) {
                this->scanExpression();
            }
            this->expect(SemiColon);
            if (!this->match(RightParenthesis)) {
                this->scanExpression();
            }
        }

        this->expect(RightParenthesis);

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        auto functor = std::bind(&Parser::scanStatement, std::ref(*this), false);
        this->scanIsolateCoverGrammarWithFunctor(functor);
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;

        if (left == nullptr) {
        } else {
            if (forIn) {
            } else {
                this->throwError("For of is not supported yet");
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
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

    PassRefPtr<Node> parseContinueStatement()
    {
        this->expectKeyword(ContinueKeyword);
        MetaNode node = this->createNode();

        RefPtr<IdentifierNode> label = nullptr;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            label = this->parseVariableIdentifier();

            if (!hasLabel(label->name())) {
                this->throwError(Messages::UnknownLabel, label->name().string());
            }
        }

        if (label) {
            for (size_t i = 0; i < this->context->labelSet.size(); i++) {
                if (this->context->labelSet[i].first == label->name() && this->context->labelSet[i].second == 1) {
                    this->throwError(Messages::UnknownLabel, label->name().string());
                }
            }
        }

        this->consumeSemicolon();
        if (label == nullptr && !this->context->inIteration) {
            this->throwError(Messages::IllegalContinue);
        }

        if (label) {
            auto string = label->name().string();
            return this->finalize(node, new ContinueLabelStatementNode(string));
        } else {
            return this->finalize(node, new ContinueStatementNode());
        }
    }

    void scanContinueStatement()
    {
        this->expectKeyword(ContinueKeyword);

        AtomicString label;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            label = this->scanVariableIdentifier();

            if (!hasLabel(label)) {
                this->throwError(Messages::UnknownLabel, label.string());
            }
        }

        if (label.string()->length()) {
            for (size_t i = 0; i < this->context->labelSet.size(); i++) {
                if (this->context->labelSet[i].first == label && this->context->labelSet[i].second == 1) {
                    this->throwError(Messages::UnknownLabel, label.string());
                }
            }
        }

        this->consumeSemicolon();
        if (label.string()->length() == 0 && !this->context->inIteration) {
            this->throwError(Messages::IllegalContinue);
        }
    }

    // ECMA-262 13.9 The break statement

    PassRefPtr<Node> parseBreakStatement()
    {
        this->expectKeyword(BreakKeyword);
        MetaNode node = this->createNode();

        RefPtr<IdentifierNode> label = nullptr;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            label = this->parseVariableIdentifier();

            if (!hasLabel(label->name())) {
                this->throwError(Messages::UnknownLabel, label->name().string());
            }
        }

        this->consumeSemicolon();
        if (label == nullptr && !this->context->inIteration && !this->context->inSwitch) {
            this->throwError(Messages::IllegalBreak);
        }

        if (label) {
            auto string = label->name().string();
            return this->finalize(node, new BreakLabelStatementNode(string));
        } else {
            return this->finalize(node, new BreakStatementNode());
        }
    }

    void scanBreakStatement()
    {
        this->expectKeyword(BreakKeyword);

        AtomicString label;
        if (this->lookahead->type == IdentifierToken && !this->hasLineTerminator) {
            label = this->scanVariableIdentifier();

            if (!hasLabel(label)) {
                this->throwError(Messages::UnknownLabel, label.string());
            }
        }

        this->consumeSemicolon();
        if (label == AtomicString() && !this->context->inIteration && !this->context->inSwitch) {
            this->throwError(Messages::IllegalBreak);
        }
    }

    // ECMA-262 13.10 The return statement

    PassRefPtr<Node> parseReturnStatement()
    {
        if (!this->context->inFunctionBody) {
            this->tolerateError(Messages::IllegalReturn);
        }

        this->expectKeyword(ReturnKeyword);
        MetaNode node = this->createNode();

        bool hasArgument = !this->match(SemiColon) && !this->match(RightBrace) && !this->hasLineTerminator && this->lookahead->type != EOFToken;
        RefPtr<Node> argument = nullptr;
        if (hasArgument) {
            argument = this->parseExpression();
        }
        this->consumeSemicolon();

        return this->finalize(node, new ReturnStatmentNode(argument.get()));
    }

    void scanReturnStatement()
    {
        if (!this->context->inFunctionBody) {
            this->tolerateError(Messages::IllegalReturn);
        }

        this->expectKeyword(ReturnKeyword);

        bool hasArgument = !this->match(SemiColon) && !this->match(RightBrace) && !this->hasLineTerminator && this->lookahead->type != EOFToken;
        if (hasArgument) {
            this->scanExpression();
        }
        this->consumeSemicolon();
    }

    // ECMA-262 13.11 The with statement

    PassRefPtr<Node> parseWithStatement()
    {
        if (this->context->strict) {
            this->tolerateError(Messages::StrictModeWith);
        }

        this->expectKeyword(WithKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> object = this->parseExpression();
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
            test = this->parseExpression();
        }
        this->expect(Colon);

        RefPtr<StatementContainer> consequent = StatementContainer::create();
        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(DefaultKeyword) || this->matchKeyword(CaseKeyword)) {
                break;
            }
            consequent->appendChild(this->parseStatementListItem());
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
            this->scanExpression();
            isDefaultNode = false;
        }
        this->expect(Colon);

        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(DefaultKeyword) || this->matchKeyword(CaseKeyword)) {
                break;
            }
            this->scanStatementListItem();
        }
        return isDefaultNode;
    }

    PassRefPtr<SwitchStatementNode> parseSwitchStatement()
    {
        this->expectKeyword(SwitchKeyword);
        MetaNode node = this->createNode();

        this->expect(LeftParenthesis);
        RefPtr<Node> discriminant = this->parseExpression();
        this->expect(RightParenthesis);

        bool previousInSwitch = this->context->inSwitch;
        this->context->inSwitch = true;

        RefPtr<StatementContainer> casesA = StatementContainer::create(), casesB = StatementContainer::create();
        RefPtr<Node> deflt;
        bool defaultFound = false;
        this->expect(LeftBrace);
        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
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
        }
        this->expect(RightBrace);

        this->context->inSwitch = previousInSwitch;

        return this->finalize(node, new SwitchStatementNode(discriminant.get(), casesA.get(), deflt.get(), casesB.get(), false));
    }

    void scanSwitchStatement()
    {
        this->expectKeyword(SwitchKeyword);

        this->expect(LeftParenthesis);
        this->scanExpression();
        this->expect(RightParenthesis);

        bool previousInSwitch = this->context->inSwitch;
        this->context->inSwitch = true;

        bool defaultFound = false;
        this->expect(LeftBrace);
        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
            bool isDefaultNode = this->scanSwitchCase();
            if (isDefaultNode) {
                if (defaultFound) {
                    this->throwError(Messages::MultipleDefaultsInSwitch);
                }
                defaultFound = true;
            } else {
            }
        }
        this->expect(RightBrace);

        this->context->inSwitch = previousInSwitch;
    }

    // ECMA-262 13.13 Labelled Statements

    PassRefPtr<Node> parseLabelledStatement()
    {
        RefPtr<Node> expr = this->parseExpression();
        MetaNode node = this->createNode();

        StatementNode* statement;
        if ((expr->type() == Identifier) && this->match(Colon)) {
            this->nextToken();

            RefPtr<IdentifierNode> id = (IdentifierNode*)expr.get();
            if (hasLabel(id->name())) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Label"), id->name().string());
            }

            this->context->labelSet.push_back(std::make_pair(id->name(), 0));
            RefPtr<StatementNode> labeledBody = this->parseStatement(this->context->strict ? false : true);
            removeLabel(id->name());

            statement = new LabeledStatementNode(labeledBody.get(), id->name().string());
        } else {
            this->consumeSemicolon();
            statement = new ExpressionStatementNode(expr.get());
        }
        return this->finalize(node, statement);
    }

    void scanLabelledStatement()
    {
        ScanExpressionResult expr = this->scanExpression();

        StatementNode* statement;
        if ((expr.first == Identifier) && this->match(Colon)) {
            this->nextToken();

            if (hasLabel(expr.second)) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Label"), expr.second.string());
            }

            this->context->labelSet.push_back(std::make_pair(expr.second, 0));
            this->scanStatement(this->context->strict ? false : true);
            removeLabel(expr.second);
        } else {
            this->consumeSemicolon();
        }
    }

    // ECMA-262 13.14 The throw statement

    PassRefPtr<Node> parseThrowStatement()
    {
        this->expectKeyword(ThrowKeyword);

        if (this->hasLineTerminator) {
            this->throwError(Messages::NewlineAfterThrow);
        }

        MetaNode node = this->createNode();
        RefPtr<Node> argument = this->parseExpression();
        this->consumeSemicolon();

        return this->finalize(node, new ThrowStatementNode(argument.get()));
    }

    void scanThrowStatement()
    {
        this->expectKeyword(ThrowKeyword);

        if (this->hasLineTerminator) {
            this->throwError(Messages::NewlineAfterThrow);
        }

        this->scanExpression();
        this->consumeSemicolon();
    }

    // ECMA-262 13.15 The try statement

    PassRefPtr<CatchClauseNode> parseCatchClause()
    {
        this->expectKeyword(CatchKeyword);

        MetaNode node = this->createNode();

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
        RefPtr<Node> param = this->parsePattern(params);

        std::vector<String*, GCUtil::gc_malloc_ignore_off_page_allocator<String*>> paramMap;
        for (size_t i = 0; i < params.size(); i++) {
            bool has = false;
            for (size_t j = 0; j < paramMap.size(); j++) {
                StringView sv = params[i]->valueStringLiteral();
                if (paramMap[j]->equals(&sv)) {
                    has = true;
                }
            }
            if (has) {
                this->tolerateError(Messages::DuplicateBinding, new StringView(params[i]->relatedSource()));
            } else {
                paramMap.push_back(new StringView(params[i]->relatedSource()));
            }
        }

        if (this->context->strict && param->type() == Identifier) {
            IdentifierNode* id = (IdentifierNode*)param.get();
            if (this->scanner->isRestrictedWord(id->name())) {
                this->tolerateError(Messages::StrictCatchVariable);
            }
        }

        this->expect(RightParenthesis);
        RefPtr<Node> body = this->parseBlock();

        this->context->inCatch = prevInCatch;

        this->context->inDirectCatchScope = prevInDirectCatchScope;

        std::vector<FunctionDeclarationNode*> vec = std::move(this->context->functionDeclarationsInDirectCatchScope);

        this->context->functionDeclarationsInDirectCatchScope = std::move(vecBefore);

        return this->finalize(node, new CatchClauseNode(param.get(), nullptr, body.get(), vec));
    }

    void scanCatchClause()
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
        RefPtr<Node> param = this->parsePattern(params);

        std::vector<String*, GCUtil::gc_malloc_ignore_off_page_allocator<String*>> paramMap;
        for (size_t i = 0; i < params.size(); i++) {
            bool has = false;
            for (size_t j = 0; j < paramMap.size(); j++) {
                StringView sv = params[i]->valueStringLiteral();
                if (paramMap[j]->equals(&sv)) {
                    has = true;
                }
            }
            if (has) {
                this->tolerateError(Messages::DuplicateBinding, new StringView(params[i]->relatedSource()));
            } else {
                paramMap.push_back(new StringView(params[i]->relatedSource()));
            }
        }

        if (this->context->strict && param->type() == Identifier) {
            IdentifierNode* id = (IdentifierNode*)param.get();
            if (this->scanner->isRestrictedWord(id->name())) {
                this->tolerateError(Messages::StrictCatchVariable);
            }
        }

        this->expect(RightParenthesis);
        this->scanBlock();

        this->context->inCatch = prevInCatch;

        this->context->inDirectCatchScope = prevInDirectCatchScope;

        std::vector<FunctionDeclarationNode*> vec = std::move(this->context->functionDeclarationsInDirectCatchScope);

        this->context->functionDeclarationsInDirectCatchScope = std::move(vecBefore);

        scopeContexts.back()->m_hasCatch = true;
    }

    PassRefPtr<BlockStatementNode> parseFinallyClause()
    {
        this->expectKeyword(FinallyKeyword);
        return this->parseBlock();
    }

    void scanFinallyClause()
    {
        this->expectKeyword(FinallyKeyword);
        this->scanBlock();
    }

    PassRefPtr<TryStatementNode> parseTryStatement()
    {
        this->expectKeyword(TryKeyword);
        MetaNode node = this->createNode();

        RefPtr<BlockStatementNode> block = this->parseBlock();
        RefPtr<CatchClauseNode> handler = this->matchKeyword(CatchKeyword) ? this->parseCatchClause() : nullptr;
        RefPtr<BlockStatementNode> finalizer = this->matchKeyword(FinallyKeyword) ? this->parseFinallyClause() : nullptr;

        if (!handler && !finalizer) {
            this->throwError(Messages::NoCatchOrFinally);
        }

        return this->finalize(node, new TryStatementNode(block.get(), handler.get(), CatchClauseNodeVector(), finalizer.get()));
    }

    void scanTryStatement()
    {
        this->expectKeyword(TryKeyword);

        this->scanBlock();
        bool meetHandler = this->matchKeyword(CatchKeyword) ? (this->scanCatchClause(), true) : false;
        bool meetFinalizer = this->matchKeyword(FinallyKeyword) ? (this->scanFinallyClause(), true) : false;

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
                statement = this->parseBlock();
            } else if (value == LeftParenthesis) {
                statement = this->parseExpressionStatement();
            } else if (value == SemiColon) {
                statement = this->parseEmptyStatement();
            } else {
                statement = this->parseExpressionStatement();
            }
            break;
        }
        case Token::IdentifierToken:
            statement = asStatementNode(this->parseLabelledStatement());
            break;

        case Token::KeywordToken:
            switch (this->lookahead->valueKeywordKind) {
            case BreakKeyword:
                statement = asStatementNode(this->parseBreakStatement());
                break;
            case ContinueKeyword:
                statement = asStatementNode(this->parseContinueStatement());
                break;
            case DebuggerKeyword:
                statement = asStatementNode(this->parseDebuggerStatement());
                break;
            case DoKeyword:
                statement = asStatementNode(this->parseDoWhileStatement());
                break;
            case ForKeyword:
                statement = asStatementNode(this->parseForStatement());
                break;
            case FunctionKeyword: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                statement = asStatementNode(this->parseFunctionDeclaration());
                break;
            }
            case IfKeyword:
                statement = asStatementNode(this->parseIfStatement());
                break;
            case ReturnKeyword:
                statement = asStatementNode(this->parseReturnStatement());
                break;
            case SwitchKeyword:
                statement = asStatementNode(this->parseSwitchStatement());
                break;
            case ThrowKeyword:
                statement = asStatementNode(this->parseThrowStatement());
                break;
            case TryKeyword:
                statement = asStatementNode(this->parseTryStatement());
                break;
            case VarKeyword:
                statement = asStatementNode(this->parseVariableStatement());
                break;
            case WhileKeyword:
                statement = asStatementNode(this->parseWhileStatement());
                break;
            case WithKeyword:
                statement = asStatementNode(this->parseWithStatement());
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
                this->scanBlock();
            } else if (value == LeftParenthesis) {
                this->scanExpressionStatement();
            } else if (value == SemiColon) {
                this->scanEmptyStatement();
            } else {
                this->scanExpressionStatement();
            }
            break;
        }
        case Token::IdentifierToken:
            this->scanLabelledStatement();
            break;

        case Token::KeywordToken:
            switch (this->lookahead->valueKeywordKind) {
            case BreakKeyword:
                this->scanBreakStatement();
                break;
            case ContinueKeyword:
                this->scanContinueStatement();
                break;
            case DebuggerKeyword:
                this->parseDebuggerStatement();
                break;
            case DoKeyword:
                this->scanDoWhileStatement();
                break;
            case ForKeyword:
                this->scanForStatement();
                break;
            case FunctionKeyword: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                this->parseFunctionDeclaration();
                break;
            }
            case IfKeyword:
                this->scanIfStatement();
                break;
            case ReturnKeyword:
                this->scanReturnStatement();
                break;
            case SwitchKeyword:
                this->scanSwitchStatement();
                break;
            case ThrowKeyword:
                this->scanThrowStatement();
                break;
            case TryKeyword:
                this->scanTryStatement();
                break;
            case VarKeyword:
                this->scanVariableStatement();
                break;
            case WhileKeyword:
                this->scanWhileStatement();
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

        RefPtr<Node> expr = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);

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

        return this->finalize(nodeStart, new BlockStatementNode(body.get()));
    }

    // ECMA-262 14.1 Function Definition

    PassRefPtr<Node> parseFunctionSourceElements()
    {
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
                referNode = body->appendChild(this->parseStatementListItem().get(), referNode);
            }
        } else {
            StatementNode* referNode = nullptr;
            while (this->startMarker.index < this->scanner->length) {
                if (this->match(RightBrace)) {
                    break;
                }
                this->scanStatementListItem();
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
            return this->finalize(nodeStart, new BlockStatementNode(body.get()));
        } else {
            return this->finalize(nodeStart, new BlockStatementNode(StatementContainer::create().get()));
        }
    }

    PassRefPtr<FunctionDeclarationNode> parseFunctionDeclaration(bool identifierIsOptional = false)
    {
        this->expectKeyword(FunctionKeyword);
        MetaNode node = this->createNode();

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        const char* message = nullptr;
        RefPtr<IdentifierNode> id;
        RefPtr<Scanner::ScannerResult> firstRestricted = nullptr;

        if (!identifierIsOptional || !this->match(LeftParenthesis)) {
            RefPtr<Scanner::ScannerResult> token = this->lookahead;
            id = this->parseVariableIdentifier();
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

        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->allowYield = !isGenerator;
        this->context->inArrowFunction = false;

        this->expect(LeftParenthesis);
        ParseFormalParametersResult formalParameters = this->parseFormalParameters(firstRestricted);
        PatternNodeVector params = std::move(formalParameters.params);
        RefPtr<Scanner::ScannerResult> stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        scopeContexts.back()->insertName(id->name(), true);
        insertUsingName(id->name());
        pushScopeContext(params, id->name());
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

        if (this->context->inDirectCatchScope) {
            scopeContexts.back()->m_needsSpecialInitialize = true;
        }

        RefPtr<FunctionDeclarationNode> fd = this->finalize(node, new FunctionDeclarationNode(id->name(), std::move(params), body.get(), popScopeContext(node), isGenerator));

        if (this->context->inDirectCatchScope) {
            this->context->functionDeclarationsInDirectCatchScope.push_back(fd.get());
        }

        return fd;
    }

    PassRefPtr<FunctionExpressionNode> parseFunctionExpression()
    {
        this->expectKeyword(FunctionKeyword);
        MetaNode node = this->createNode();

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

        if (!this->match(LeftParenthesis)) {
            RefPtr<Scanner::ScannerResult> token = this->lookahead;
            id = (!this->context->strict && !isGenerator && this->matchKeyword(YieldKeyword)) ? this->parseIdentifierName() : this->parseVariableIdentifier();
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

        this->expect(LeftParenthesis);
        ParseFormalParametersResult formalParameters = this->parseFormalParameters(firstRestricted);
        PatternNodeVector params = std::move(formalParameters.params);
        RefPtr<Scanner::ScannerResult> stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        AtomicString fnName;
        if (id) {
            fnName = id->name();
        }

        pushScopeContext(params, fnName);

        if (id) {
            scopeContexts.back()->insertName(fnName, false);
            insertUsingName(fnName);
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

        return this->finalize(node, new FunctionExpressionNode(fnName, std::move(params), body.get(), popScopeContext(node), isGenerator));
    }

    // ECMA-262 14.1.1 Directive Prologues

    PassRefPtr<Node> parseDirective()
    {
        RefPtr<Scanner::ScannerResult> token = this->lookahead;
        StringView directiveValue;
        bool isLiteral = false;

        MetaNode node = this->createNode();
        RefPtr<Node> expr = this->parseExpression();
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
        if (this->match(RightParenthesis)) {
            this->throwUnexpectedToken(this->lookahead);
        } else {
            this->parseFormalParameter(options);
        }
        this->expect(RightParenthesis);

        ParseFormalParametersResult options2(std::move(options.params), options.stricted, options.firstRestricted, options.message);
        RefPtr<Node> method = this->parsePropertyMethod(options2);
        this->context->allowYield = previousAllowYield;

        extractNamesFromFunctionParams(options.params);
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
                argument = this->parseAssignmentExpression();
            } else {
                if (!this->match(';') && !this->match('}') && !this->match(')') && this->lookahead.type !== Token.EOF) {
                    argument = this->parseAssignmentExpression();
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
                argument = this->parseAssignmentExpression();
            } else {
                if (!this->match(';') && !this->match('}') && !this->match(')') && this->lookahead.type !== Token.EOF) {
                    argument = this->parseAssignmentExpression();
                }
            }
            this->context.allowYield = previousAllowYield;
        }

        return this->finalize(node, new Node.YieldExpression(argument, delegate));
        */
    }

    // ECMA-262 14.5 Class Definitions
    /*
    parseClassElement(hasConstructor): Node.Property {
        let token = this.lookahead;
        let node = this.createNode();

        let kind: string;
        let key: Node.PropertyKey;
        let value: Node.FunctionExpression;
        let computed = false;
        let method = false;
        let isStatic = false;

        if (this.match('*')) {
            this.nextToken();
        } else {
            computed = this.match('[');
            key = this.parseObjectPropertyKey();
            const id = <Node.Identifier>key;
            if (id.name === 'static' && (this.qualifiedPropertyName(this.lookahead) || this.match('*'))) {
                token = this.lookahead;
                isStatic = true;
                computed = this.match('[');
                if (this.match('*')) {
                    this.nextToken();
                } else {
                    key = this.parseObjectPropertyKey();
                }
            }
        }

        const lookaheadPropertyKey = this.qualifiedPropertyName(this.lookahead);
        if (token.type === Token.Identifier) {
            if (token.value === 'get' && lookaheadPropertyKey) {
                kind = 'get';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                this.context.allowYield = false;
                value = this.parseGetterMethod();
            } else if (token.value === 'set' && lookaheadPropertyKey) {
                kind = 'set';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                value = this.parseSetterMethod();
            }
        } else if (token.type === Token.Punctuator && token.value === '*' && lookaheadPropertyKey) {
            kind = 'init';
            computed = this.match('[');
            key = this.parseObjectPropertyKey();
            value = this.parseGeneratorMethod();
            method = true;
        }

        if (!kind && key && this.match('(')) {
            kind = 'init';
            value = this.parsePropertyMethodFunction();
            method = true;
        }

        if (!kind) {
            this.throwUnexpectedToken(this.lookahead);
        }

        if (kind === 'init') {
            kind = 'method';
        }

        if (!computed) {
            if (isStatic && this.isPropertyKey(key, 'prototype')) {
                this.throwUnexpectedToken(token, Messages.StaticPrototype);
            }
            if (!isStatic && this.isPropertyKey(key, 'constructor')) {
                if (kind !== 'method' || !method || value.generator) {
                    this.throwUnexpectedToken(token, Messages.ConstructorSpecialMethod);
                }
                if (hasConstructor.value) {
                    this.throwUnexpectedToken(token, Messages.DuplicateConstructor);
                } else {
                    hasConstructor.value = true;
                }
                kind = 'constructor';
            }
        }


        return this.finalize(node, new Node.MethodDefinition(key, computed, value, kind, isStatic));
    }

    parseClassElementList(): Node.Property[] {
        const body = [];
        let hasConstructor = { value: false };

        this.expect('{');
        while (!this.match('}')) {
            if (this.match(';')) {
                this.nextToken();
            } else {
                body.push(this.parseClassElement(hasConstructor));
            }
        }
        this.expect('}');

        return body;
    }

    parseClassBody(): Node.ClassBody {
        const node = this.createNode();
        const elementList = this.parseClassElementList();

        return this.finalize(node, new Node.ClassBody(elementList));
    }

    parseClassDeclaration(identifierIsOptional?: boolean): Node.ClassDeclaration {
        const node = this.createNode();

        const previousStrict = this.context.strict;
        this.context.strict = true;
        this.expectKeyword('class');

        const id = (identifierIsOptional && (this.lookahead.type !== Token.Identifier)) ? null : this.parseVariableIdentifier();
        let superClass = null;
        if (this.matchKeyword('extends')) {
            this.nextToken();
            superClass = this.isolateCoverGrammar(this.parseLeftHandSideExpressionAllowCall);
        }
        const classBody = this.parseClassBody();
        this.context.strict = previousStrict;

        return this.finalize(node, new Node.ClassDeclaration(id, superClass, classBody));
    }

    parseClassExpression(): Node.ClassExpression {
        const node = this.createNode();

        const previousStrict = this.context.strict;
        this.context.strict = true;
        this.expectKeyword('class');
        const id = (this.lookahead.type === Token.Identifier) ? this.parseVariableIdentifier() : null;
        let superClass = null;
        if (this.matchKeyword('extends')) {
            this.nextToken();
            superClass = this.isolateCoverGrammar(this.parseLeftHandSideExpressionAllowCall);
        }
        const classBody = this.parseClassBody();
        this.context.strict = previousStrict;

        return this.finalize(node, new Node.ClassExpression(id, superClass, classBody));
    }
    */
    Node* parseClassExpression()
    {
        this->throwError("class keyword is not supported yet");
        RELEASE_ASSERT_NOT_REACHED();
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
            referNode = body->appendChild(this->parseStatementListItem().get(), referNode);
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
                    this.match('[') ? this.parseArrayInitializer() : this.parseAssignmentExpression();
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
                    declaration = this.parseStatementListItem();
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
    Parser parser(ctx, codeBlock->src(), stackRemain, codeBlock->sourceElementStart().line, codeBlock->sourceElementStart().column, codeBlock->sourceElementStart().index);
    parser.trackUsingNames = false;
    parser.config.parseSingleFunction = true;
    parser.config.parseSingleFunctionTarget = codeBlock;
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
