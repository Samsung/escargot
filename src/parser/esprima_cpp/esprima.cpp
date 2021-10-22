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

#include "Escargot.h"
#include "esprima.h"

#include "Messages.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/AST.h"
#include "parser/CodeBlock.h"
#include "parser/Lexer.h"
#include "parser/Script.h"
#include "parser/ASTBuilder.h"
#include "parser/ASTAllocator.h"
#include "parser/esprima_cpp/ParserContext.h"

#define ALLOC_TOKEN(tokenName) Scanner::ScannerResult* tokenName = ALLOCA(sizeof(Scanner::ScannerResult), Scanner::ScannerResult, ec);

#define ASTNode typename ASTBuilder::ASTNode
#define ASTStatementContainer typename ASTBuilder::ASTStatementContainer
#define ASTSentinelNode typename ASTBuilder::ASTSentinelNode
#define ASTNodeList typename ASTBuilder::ASTNodeList

#if defined(ESCARGOT_SMALL_CONFIG)
#define NEW_FUNCTION_AST_BUILDER() NodeGenerator newBuilder(this->allocator, false);
#else
#define NEW_FUNCTION_AST_BUILDER() SyntaxChecker newBuilder;
#endif

#define BEGIN_FUNCTION_SCANNING(name)                                                              \
    NEW_FUNCTION_AST_BUILDER()                                                                     \
    auto oldSubCodeBlockIndex = ++this->subCodeBlockIndex;                                         \
    auto oldScopeContext = pushScopeContext(name);                                                 \
    if (UNLIKELY(this->currentClassInfo != nullptr)) {                                             \
        if (this->currentClassInfo->m_firstMethod                                                  \
            && this->currentScopeContext == this->currentClassInfo->m_lastMethod->m_nextSibling) { \
            this->currentClassInfo->m_lastMethod = this->currentScopeContext;                      \
        } else if (!this->currentClassInfo->m_firstMethod) {                                       \
            this->currentClassInfo->m_lastMethod = this->currentClassInfo->m_firstMethod           \
                = this->currentScopeContext;                                                       \
        }                                                                                          \
    }                                                                                              \
    LexicalBlockIndex lexicalBlockIndexBefore = this->lexicalBlockIndex;                           \
    LexicalBlockIndex lexicalBlockCountBefore = this->lexicalBlockCount;                           \
    this->lexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;                                             \
    this->lexicalBlockCount = LEXICAL_BLOCK_INDEX_MAX;                                             \
    ParserBlockContext blockContext;                                                               \
    openBlock(blockContext);

#define END_FUNCTION_SCANNING()                                                                \
    closeBlock(blockContext);                                                                  \
    this->lexicalBlockIndex = lexicalBlockIndexBefore;                                         \
    this->lexicalBlockCount = lexicalBlockCountBefore;                                         \
    this->currentScopeContext->m_lexicalBlockIndexFunctionLocatedIn = lexicalBlockIndexBefore; \
    popScopeContext(oldScopeContext);                                                          \
    this->subCodeBlockIndex = oldSubCodeBlockIndex;

using namespace Escargot::EscargotLexer;

namespace Escargot {
namespace esprima {

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
    NodeVector params;
};
*/

struct DeclarationOptions {
    bool inFor;
    KeywordKind kind;
};

class Parser {
public:
    // Parser always allocated on the stack
    MAKE_STACK_ALLOCATED();

    ::Escargot::Context* escargotContext;
    Scanner* scanner;
    Scanner scannerInstance;

    ASTAllocator& allocator;

    enum SourceType {
        Script,
        Module
    };

    SourceType sourceType;
    Scanner::ScannerResult lookahead;

    Script::ModuleData* moduleData;

    ParserContext contextInstance;
    ParserContext* context;

    InterpretedCodeBlock* codeBlock;
    Marker baseMarker;
    Marker startMarker;
    Marker lastMarker;

    ASTScopeContext* currentScopeContext;
    ASTScopeContext* lastPoppedScopeContext;
    ASTClassInfo* outerClassInfo;
    ASTClassInfo* currentClassInfo;

    AtomicString lastUsingName;
    std::function<void(AtomicString, LexicalBlockIndex, KeywordKind, bool)> nameDeclaredCallback;

    bool trackUsingNames;
    bool hasLineTerminator;
    bool isParsingSingleFunction;
    size_t stackLimit;
    size_t subCodeBlockIndex;
    size_t taggedTemplateExpressionIndex;

    ASTBlockContext* currentBlockContext;
    LexicalBlockIndex lexicalBlockIndex;
    LexicalBlockIndex lexicalBlockCount;

    NumeralLiteralVector numeralLiteralVector;

    ASTScopeContext fakeContext;

    AtomicString stringArguments;

    struct ParseFormalParametersResult {
        VectorWithInlineStorage<8, SyntaxNode, std::allocator<SyntaxNode>> params;
        VectorWithInlineStorage<8, AtomicString, std::allocator<AtomicString>> paramSet;
        Scanner::SmallScannerResult stricted;
        Scanner::SmallScannerResult firstRestricted;
        const char* message;

        ParseFormalParametersResult()
            : message(nullptr)
        {
        }
    };

    Parser(::Escargot::Context* escargotContext, StringView code, ASTClassInfo* outerClassInfo, bool isModule, size_t stackRemain, ExtendedNodeLOC startLoc = ExtendedNodeLOC(1, 0, 0))
        : scannerInstance(escargotContext, &contextInstance, code, isModule, startLoc.line, startLoc.column)
        , allocator(escargotContext->astAllocator())
        , fakeContext(escargotContext->astAllocator())
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
        this->stringArguments = escargotContext->staticStrings().arguments;
        this->currentBlockContext = nullptr;
        this->lexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
        this->lexicalBlockCount = LEXICAL_BLOCK_INDEX_MAX;
        this->subCodeBlockIndex = 0;
        this->taggedTemplateExpressionIndex = 0;
        this->lastPoppedScopeContext = &fakeContext;
        this->currentScopeContext = nullptr;
        this->outerClassInfo = outerClassInfo;
        this->currentClassInfo = outerClassInfo;
        this->trackUsingNames = true;
        this->isParsingSingleFunction = false;
        this->codeBlock = nullptr;

        this->scanner = &scannerInstance;
        this->sourceType = isModule ? Module : Script;
        this->moduleData = isModule ? new Script::ModuleData() : nullptr;
        this->hasLineTerminator = false;

        this->context = &contextInstance;
        this->context->allowIn = true;
        this->context->allowYield = false;
        this->context->allowLexicalDeclaration = false;
        this->context->allowSuperCall = false;
        this->context->allowSuperProperty = false;
        this->context->allowNewTarget = false;
        this->context->allowStrictDirective = true;
        this->context->allowArguments = true;
        this->context->await = false;
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        this->context->inFunctionBody = false;
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inArrowFunction = false;
        this->context->inWith = false;
        this->context->inCatchClause = false;
        this->context->inLoop = false;
        this->context->inParameterParsing = false;
        this->context->inParameterNameParsing = false;
        this->context->hasRestrictedWordInArrayOrObjectInitializer = false;
        this->context->strict = this->sourceType == Module;
        this->setMarkers(startLoc);
    }

    bool inGlobalSourceCodeParsing()
    {
        return !this->isParsingSingleFunction && this->currentScopeContext->m_nodeType == ASTNodeType::Program;
    }

    void setMarkers(ExtendedNodeLOC startLoc)
    {
        this->baseMarker.index = startLoc.index;
        this->baseMarker.lineNumber = this->scanner->lineNumber;
        this->baseMarker.lineStart = 0;

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

        if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource(this->scanner->source))) {
            this->scanner->convertToKeywordInStrictMode(next);
        }
    }

    void insertNumeralLiteral(Value v)
    {
        ASSERT(!v.isPointerValue());

        if (VectorUtil::findInVector(numeralLiteralVector, v) == VectorUtil::invalidIndex) {
            if (numeralLiteralVector.size() < KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT) {
                numeralLiteralVector.push_back(v);
            }
        }
    }

    ASTScopeContext* popScopeContext(ASTScopeContext* lastPushedScopeContext)
    {
        auto ret = this->currentScopeContext;
        this->lastUsingName = AtomicString();
        this->lastPoppedScopeContext = ret;
        this->currentScopeContext = lastPushedScopeContext;
        return ret;
    }

    void pushScopeContext(ASTScopeContext* ctx)
    {
        this->currentScopeContext = ctx;
        this->lastUsingName = AtomicString();
#ifdef ESCARGOT_DEBUGGER
        if (this->escargotContext->debugger() && this->escargotContext->debugger()->enabled()) {
            ctx->m_hasEval = true;
        }
#endif /* ESCARGOT_DEBUGGER */
    }

    ASTScopeContext* pushScopeContext(AtomicString functionName)
    {
        auto parentContext = this->currentScopeContext;
        // initialize subCodeBlockIndex before parsing of an internal function
        this->subCodeBlockIndex = 0;
        if (this->isParsingSingleFunction) {
            fakeContext = ASTScopeContext(this->allocator);
            pushScopeContext(&fakeContext);
            return parentContext;
        }
        pushScopeContext(new (this->allocator) ASTScopeContext(this->allocator, this->context->strict));
        this->currentScopeContext->m_functionName = functionName;
        this->currentScopeContext->m_inWith = this->context->inWith;

        if (parentContext) {
            parentContext->appendChild(this->currentScopeContext);
        }

        return parentContext;
    }

    class TrackUsingNameBlocker {
    public:
        TrackUsingNameBlocker(Parser* parser)
            : m_parser(parser)
            , m_oldValue(parser->trackUsingNames)
        {
            parser->trackUsingNames = false;
        }

        ~TrackUsingNameBlocker()
        {
            m_parser->trackUsingNames = m_oldValue;
        }

    private:
        Parser* m_parser;
        bool m_oldValue;
    };

    ALWAYS_INLINE void insertUsingName(AtomicString name)
    {
        if (this->isParsingSingleFunction) {
            return;
        }

        if (this->lastUsingName == name) {
            return;
        }
        this->lastUsingName = name;

        if (UNLIKELY(name == this->stringArguments && !this->context->allowArguments)) {
            this->throwError("arguments is not valid in here");
        }

        bool contains = false;
        auto& v = this->currentBlockContext->m_usingNames;
        size_t size = v.size();

        if (UNLIKELY(size > 10)) {
            for (size_t i = 0; i < size; i++) {
                if (v[i] == name) {
                    contains = true;
                    break;
                }
            }
        } else {
            switch (size) {
            case 10:
                contains = v[9] == name;
                FALLTHROUGH;
#define TEST_ONCE(n)                             \
    case n:                                      \
        contains = contains || v[n - 1] == name; \
        FALLTHROUGH;
                TEST_ONCE(9)
                TEST_ONCE(8)
                TEST_ONCE(7)
                TEST_ONCE(6)
                TEST_ONCE(5)
                TEST_ONCE(4)
                TEST_ONCE(3)
                TEST_ONCE(2)
                TEST_ONCE(1)
#undef TEST_ONCE
            case 0:
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }

        ASSERT((VectorUtil::findInVector(this->currentBlockContext->m_usingNames, name) != VectorUtil::invalidIndex) == contains);
        if (!contains) {
            this->currentBlockContext->m_usingNames.push_back(name);
        }
    }

    void extractNamesFromFunctionParams(ParseFormalParametersResult& paramsResult)
    {
        if (this->isParsingSingleFunction) {
            return;
        }

        VectorWithInlineStorage<8, SyntaxNode, std::allocator<SyntaxNode>>& params = paramsResult.params;
        bool hasParameterOtherThanIdentifier = false;
        bool seenDefaultParameter = false;

        for (size_t i = 0; i < params.size(); i++) {
            switch (params[i]->type()) {
            case Identifier:
                if (LIKELY(!seenDefaultParameter)) {
                    this->currentScopeContext->m_functionLength++;
                }
                break;
            case AssignmentPattern:
                seenDefaultParameter = true;
                hasParameterOtherThanIdentifier = true;
                break;
            case ArrayPattern:
            case ObjectPattern:
                hasParameterOtherThanIdentifier = true;
                if (LIKELY(!seenDefaultParameter)) {
                    this->currentScopeContext->m_functionLength++;
                }
                break;
            case RestElement:
                hasParameterOtherThanIdentifier = true;
                break;
            default: {
                RELEASE_ASSERT_NOT_REACHED();
            }
            }
        }
        this->currentScopeContext->m_parameterCount = params.size();
        this->currentScopeContext->m_hasParameterOtherThanIdentifier = hasParameterOtherThanIdentifier;

        const auto& paramNames = paramsResult.paramSet;
#ifndef NDEBUG
        if (!hasParameterOtherThanIdentifier) {
            ASSERT(this->currentScopeContext->m_functionLength == params.size());
            ASSERT(this->currentScopeContext->m_functionLength == paramNames.size());
            ASSERT(this->currentScopeContext->m_functionLength == this->currentScopeContext->m_parameterCount);
        }
#endif
        this->currentScopeContext->m_parameters.resizeWithUninitializedValues(paramNames.size());
        LexicalBlockIndex functionBodyBlockIndex = this->currentScopeContext->m_functionBodyBlockIndex;
        for (size_t i = 0; i < paramNames.size(); i++) {
            ASSERT(paramNames[i].string()->length() > 0);
            this->currentScopeContext->m_parameters[i] = paramNames[i];
            this->currentScopeContext->insertVarName(paramNames[i], functionBodyBlockIndex, true, true, true);
        }

        // Check if any identifier names are duplicated.
        if (hasParameterOtherThanIdentifier || this->context->inArrowFunction) {
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

    bool tryToSkipFunctionParsing()
    {
        // try to skip the function parsing during the parsing of function call only
        if (!this->isParsingSingleFunction) {
            return false;
        }

        InterpretedCodeBlock* currentTarget = this->codeBlock;
        this->nextToken();

        InterpretedCodeBlock* childBlock = currentTarget->childBlockAt(this->subCodeBlockIndex);
        this->scanner->index = childBlock->src().length() + childBlock->functionStart().index - currentTarget->functionStart().index;

#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
        this->scanner->lineNumber = childBlock->bodyEndLOC().line;
        this->scanner->lineStart = childBlock->bodyEndLOC().index - childBlock->bodyEndLOC().column;
#else
        this->scanner->lineNumber = childBlock->functionStart().line;
        this->scanner->lineStart = childBlock->functionStart().index - childBlock->functionStart().column;
#endif
        this->lookahead.lineNumber = this->scanner->lineNumber;
        this->lookahead.lineStart = this->scanner->lineStart;
        this->lookahead.type = Token::PunctuatorToken;
        this->lookahead.valuePunctuatorKind = PunctuatorKind::RightBrace;
        this->nextToken();

        // increase subCodeBlockIndex because parsing of an internal function is skipped
        this->subCodeBlockIndex++;

        return true;
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
    void throwUnexpectedToken(const Scanner::SmallScannerResult& token, const char* message = nullptr)
    {
        ASSERT(token);
        const char* msg;
        if (message) {
            msg = message;
        } else {
            msg = Messages::UnexpectedToken;
        }

        String* value;
        if (token.type != InvalidToken) {
            if (!msg) {
                msg = checkTokenIdentifier(token.type);

                if (token.type == Token::KeywordToken) {
                    if (this->scanner->isFutureReservedWord(token.relatedSource(this->scanner->source))) {
                        msg = Messages::UnexpectedReserved;
                    } else if (this->context->strict && this->scanner->isStrictModeReservedWord(token.relatedSource(this->scanner->source))) {
                        msg = Messages::StrictReservedWord;
                    }
                }
            } else if (token.type == Token::EOFToken) {
                msg = Messages::UnexpectedEOS;
            }
            value = new StringView(this->scanner->sourceAsNormalView, token.start, token.end);
        } else {
            value = new ASCIIString("ILLEGAL");
        }

        // msg = msg.replace('%0', value);
        UTF16StringDataNonGCStd msgData;
        msgData.assign(msg, &msg[strlen(msg)]);
        UTF16StringDataNonGCStd valueData = UTF16StringDataNonGCStd(value->toUTF16StringData().data());
        if (msgData.find(u"%s") == std::string::npos) {
            msgData += u" %s";
        }
        replaceAll(msgData, UTF16StringDataNonGCStd(u"%s"), valueData);

        // if (token && typeof token.lineNumber == 'number') {
        if (token.type != InvalidToken) {
            const size_t index = token.start;
            const size_t line = token.lineNumber;
            const size_t column = token.start - this->lastMarker.lineStart + 1;
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

        if (this->context->strict && next->type == Token::IdentifierToken && this->scanner->isStrictModeReservedWord(next->relatedSource(this->scanner->source))) {
            this->scanner->convertToKeywordInStrictMode(next);
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

    MetaNode startNode(Scanner::SmallScannerResult* token)
    {
        ASSERT(token != nullptr);
        MetaNode n;
        n.index = token->start + this->baseMarker.index;
        n.line = token->lineNumber;
        n.column = token->start - token->lineStart;
        return n;
    }

    void rewind(const Marker& marker)
    {
        this->scanner->index = marker.index;
        this->scanner->lineNumber = marker.lineNumber;
        this->scanner->lineStart = marker.lineStart;
        this->nextToken();
    }

    // FIXME remove finalize methods
    template <typename T>
    ALWAYS_INLINE T* finalize(MetaNode meta, T* node)
    {
        node->m_loc = NodeLOC(meta.index);
        return node;
    }

    ALWAYS_INLINE SyntaxNode finalize(MetaNode meta, SyntaxNode node)
    {
        return node;
    }

    void addImplicitName(Node* rightNode, AtomicString name)
    {
        // add implicit name for function and class
        ASTNodeType type = rightNode->type();
        if (!this->isParsingSingleFunction && (type == ASTNodeType::FunctionExpression || type == ASTNodeType::ArrowFunctionExpression)) {
            if (this->lastPoppedScopeContext->m_functionName == AtomicString()) {
                this->lastPoppedScopeContext->m_functionName = name;
            }
        } else if (type == ASTNodeType::ClassExpression) {
            rightNode->asClassExpression()->tryToSetImplicitName(name);
        }
    }

    void addImplicitName(SyntaxNode rightNode, AtomicString name)
    {
        // add implicit name for function only
        ASTNodeType type = rightNode->type();
        if (!this->isParsingSingleFunction && (type == ASTNodeType::FunctionExpression || type == ASTNodeType::ArrowFunctionExpression)) {
            if (this->lastPoppedScopeContext->m_functionName == AtomicString()) {
                this->lastPoppedScopeContext->m_functionName = name;
            }
        }
    }

    // Expect the next token to match the specified punctuator.
    // If not, an exception will be thrown.
    void expect(PunctuatorKind value)
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type != Token::PunctuatorToken || token->valuePunctuatorKind != value) {
            this->throwUnexpectedToken(*token);
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
            this->throwUnexpectedToken(*token);
        }
    }

    // Return true if the next token matches the specified punctuator.

    bool match(PunctuatorKind value) const
    {
        return this->lookahead.type == Token::PunctuatorToken && this->lookahead.valuePunctuatorKind == value;
    }

    // Return true if the next token matches the specified keyword

    bool matchKeyword(KeywordKind keyword) const
    {
        return this->lookahead.type == Token::KeywordToken && this->lookahead.valueKeywordKind == keyword;
    }

    // Return true if the next token matches the specified contextual keyword
    // (where an identifier is sometimes a keyword depending on the context)

    template <const size_t len>
    bool matchContextualKeyword(const char (&keyword)[len])
    {
        return this->lookahead.type == Token::IdentifierToken && !this->lookahead.hasAllocatedString && this->lookahead.valueStringLiteral(this->scanner).equals(keyword);
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

    void consumeSemicolon()
    {
        if (this->match(PunctuatorKind::SemiColon)) {
            this->nextToken();
        } else if (!this->hasLineTerminator) {
            if (this->lookahead.type != Token::EOFToken && !this->match(PunctuatorKind::RightBrace)) {
                this->throwUnexpectedToken(this->lookahead);
            }
            this->lastMarker.index = this->startMarker.index;
            this->lastMarker.lineNumber = this->startMarker.lineNumber;
            this->lastMarker.lineStart = this->startMarker.lineStart;
        }
    }

    void throwIfSuperOperationIsNotAllowed()
    {
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (this->match(LeftParenthesis)) {
            if (!this->context->allowSuperCall) {
                this->throwUnexpectedToken(*token);
            }
        } else if (this->match(Period) || this->match(LeftSquareBracket)) {
            if (!this->context->allowSuperProperty) {
                this->throwUnexpectedToken(*token);
            }
        } else {
            this->throwUnexpectedToken(this->lookahead);
        }
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

    typedef VectorWithInlineStorage<8, Scanner::SmallScannerResult, std::allocator<Scanner::SmallScannerResult>> SmallScannerResultVector;

    struct IsolateCoverGrammarContext {
        bool previousIsBindingElement;
        bool previousIsAssignmentTarget;
        Scanner::SmallScannerResult previousFirstCoverInitializedNameError;
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
            this->throwUnexpectedToken(this->context->firstCoverInitializedNameError);
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

    template <class ASTBuilder, typename T>
    ALWAYS_INLINE ASTNode isolateCoverGrammar(ASTBuilder& builder, T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        ASTNode result = (this->*parseFunction)(builder);
        endIsolateCoverGrammar(&grammarContext);

        return result;
    }

    template <class ASTBuilder, typename T, typename... Args>
    ALWAYS_INLINE ASTNode isolateCoverGrammar(ASTBuilder& builder, T parseFunction, Args... args)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        ASTNode result = (this->*parseFunction)(builder, args...);
        endIsolateCoverGrammar(&grammarContext);

        return result;
    }

    template <class ASTBuilder, typename T>
    ALWAYS_INLINE ASTNode inheritCoverGrammar(ASTBuilder& builder, T parseFunction)
    {
        IsolateCoverGrammarContext grammarContext;
        startCoverGrammar(&grammarContext);

        ASTNode result = (this->*parseFunction)(builder);
        endInheritCoverGrammar(&grammarContext);

        return result;
    }

    template <class ASTBuilder>
    ASTNode finishIdentifier(ASTBuilder& builder, Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        ASTNode ret;
        ParserStringView sv = token->valueStringLiteral(this->scanner);
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            ret = builder.createIdentifierNode(this->escargotContext->staticStrings().asciiTable[firstCh]);
        } else {
            if (token->hasAllocatedString) {
                ret = builder.createIdentifierNode(AtomicString(this->escargotContext, token->valueStringLiteralData.m_stringIfNewlyAllocated));
            } else {
                ret = builder.createIdentifierNode(AtomicString(this->escargotContext, &sv));
            }
        }

        if (this->trackUsingNames) {
            this->insertUsingName(ret->asIdentifier()->name());
        }

        return ret;
    }

    template <class ASTBuilder>
    ASTNode finishClassPrivateIdentifier(ASTBuilder& builder, Scanner::ScannerResult* token)
    {
        ParserStringView s;
        if (token->type == Token::KeywordToken) {
            s = ParserStringView(this->scannerInstance.source, token->start, token->end);
        } else {
            if (token->hasAllocatedString) {
                if (!token->valueStringLiteralData.m_stringIfNewlyAllocated) {
                    token->valueStringLiteral(this->scanner);
                }
                StringBuilder sb;
                sb.appendChar('#');
                sb.appendString(token->valueStringLiteralData.m_stringIfNewlyAllocated);
                s = ParserStringView(sb.finalize());
            } else {
                s = ParserStringView(this->scannerInstance.source, token->start - 1, token->end);
            }
        }

        auto c = s.charAt(1);
        // throw error when starts with zwj char
        if (c == 0x200D || c == 0x200C) {
            this->throwError(Messages::InvalidClassElementName);
        }

        return builder.createIdentifierNode(AtomicString(this->escargotContext, &s));
    }

    template <class ASTBuilder>
    ASTNode parseNumericLiteralNode(ASTBuilder& builder, bool minus = false)
    {
        if (UNLIKELY(this->context->strict && this->lookahead.octal)) {
            this->throwUnexpectedToken(this->lookahead, Messages::StrictOctalLiteral);
        }
        if (UNLIKELY(this->context->strict && this->lookahead.startWithZero)) {
            this->throwUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
        }
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;
        MetaNode node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        // raw = this->getTokenRaw(token);
        if (builder.isNodeGenerator()) {
            auto d = token->valueNumberLiteral(this->scanner);
            if (LIKELY(!d.second)) {
                if (minus) {
                    d.first = Value(-d.first.asNumber());
                }
                if (this->context->inLoop || d.first.asNumber() == 0) {
                    this->insertNumeralLiteral(d.first);
                }
            } else {
                if (minus) {
                    d.first = Value(d.first.asBigInt()->negativeValue());
                }
            }
            return this->finalize(node, builder.createLiteralNode(d.first));
        } else {
            return this->finalize(node, builder.createLiteralNode(Value()));
        }
    }

    // ECMA-262 12.2 Primary Expressions

    template <class ASTBuilder>
    ASTNode parsePrimaryExpression(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        switch (this->lookahead.type) {
        case Token::IdentifierToken: {
            if ((this->sourceType == SourceType::Module || this->currentScopeContext->m_isAsync) && this->lookahead.relatedSource(this->scanner->source) == "await") {
                this->throwUnexpectedToken(this->lookahead);
            }
            if (this->matchAsyncFunction()) {
                return this->parseFunctionExpression(builder);
            }
            ALLOC_TOKEN(token);
            this->nextToken(token);
            return this->finalize(node, finishIdentifier(builder, token));
        }
        case Token::NumericLiteralToken:
            return parseNumericLiteralNode(builder);
        case Token::StringLiteralToken:
            if (UNLIKELY(this->context->strict && this->lookahead.octal)) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictOctalLiteral);
            }

            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                if (builder.willGenerateByteCode()) {
                    return this->finalize(node, builder.createLiteralNode(token->valueStringLiteralToValue(this->scanner)));
                } else {
                    return this->finalize(node, builder.createLiteralNode(Value()));
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
                bool value = token->relatedSource(this->scanner->source) == "true";
                if (builder.willGenerateByteCode()) {
                    this->insertNumeralLiteral(Value(value));
                }
                return this->finalize(node, builder.createLiteralNode(Value(value)));
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
            if (builder.willGenerateByteCode()) {
                this->insertNumeralLiteral(Value(Value::Null));
            }
            return this->finalize(node, builder.createLiteralNode(Value(Value::Null)));
        }
        case Token::TemplateToken:
            return this->parseTemplateLiteral(builder);

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead.valuePunctuatorKind;
            switch (value) {
            case LeftParenthesis:
                this->context->isBindingElement = false;
                return this->inheritCoverGrammar(builder, &Parser::parseGroupExpression<ASTBuilder>);
            case LeftSquareBracket:
                return this->inheritCoverGrammar(builder, &Parser::parseArrayInitializer<ASTBuilder>);
            case LeftBrace:
                return this->inheritCoverGrammar(builder, &Parser::parseObjectInitializer<ASTBuilder>);
            case Divide:
            case DivideEqual: {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->scanner->index = this->startMarker.index;
                ALLOC_TOKEN(token);
                this->nextRegexToken(token);
                // raw = this->getTokenRaw(token);
                return this->finalize(node, builder.createRegExpLiteralNode(token->valueRegExp.body, token->valueRegExp.flags));
            }
            default: {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                this->throwUnexpectedToken(*token);
            }
            }
            break;
        }

        case Token::KeywordToken:
            if (!this->context->strict && !this->context->allowYield && this->matchKeyword(YieldKeyword)) {
                return this->parseIdentifierName(builder);
            } else if (!this->context->strict && this->matchKeyword(LetKeyword)) {
                return this->parseIdentifierName(builder);
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(FunctionKeyword)) {
                    return this->parseFunctionExpression(builder);
                } else if (this->matchKeyword(ThisKeyword)) {
                    this->nextToken();
                    this->currentScopeContext->m_hasThisExpression = true;
                    return this->finalize(node, builder.createThisExpressionNode());
                } else if (this->matchKeyword(SuperKeyword)) {
                    throwIfSuperOperationIsNotAllowed();
                    return finishSuperExpression(builder, node, this->lookahead.valuePunctuatorKind == LeftParenthesis);
                } else if (this->matchKeyword(ClassKeyword)) {
                    return this->parseClassExpression(builder);
                } else {
                    ALLOC_TOKEN(token);
                    this->nextToken(token);
                    this->throwUnexpectedToken(*token);
                }
            }
            break;
        default: {
            ALLOC_TOKEN(token);
            this->nextToken(token);
            this->throwUnexpectedToken(*token);
        }
        }

        ASSERT_NOT_REACHED();
        return ASTNode();
    }

    void validateParam(ParseFormalParametersResult& options, const Scanner::SmallScannerResult& param, AtomicString name)
    {
        ASSERT(param);
        if (UNLIKELY(this->currentScopeContext->m_isGenerator && name.string()->equals("yield"))) {
            this->throwUnexpectedToken(param, Messages::UnexpectedToken);
        } else if (UNLIKELY(this->currentScopeContext->m_isAsync && name.string()->equals("await"))) {
            this->throwUnexpectedToken(param, Messages::UnexpectedToken);
        }

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
                options.firstRestricted = param;
                options.message = Messages::StrictParamDupe;
            }
        }

        options.paramSet.push_back(name);
    }

    template <class ASTBuilder>
    ASTNode parseRestElement(ASTBuilder& builder, SmallScannerResultVector& params)
    {
        MetaNode node = this->createNode();

        this->nextToken();
        ASTNode arg = this->parsePattern(builder, params);
        if (this->match(Substitution)) {
            this->throwError(Messages::DefaultRestParameter);
        }
        if (!this->match(RightParenthesis)) {
            this->throwError(Messages::ParameterAfterRestParameter);
        }
        return this->finalize(node, builder.createRestElementNode(arg));
    }

    template <class ASTBuilder>
    ASTNode parseBindingRestElement(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();
        this->nextToken();
        ASTNode arg = this->parsePattern(builder, params, kind, isExplicitVariableDeclaration);
        return this->finalize(node, builder.createRestElementNode(arg));
    }

    // ECMA-262 13.3.3 Destructuring Binding Patterns

    template <class ASTBuilder>
    ASTNode parseArrayPattern(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        this->nextToken();
        ASTNodeList elements;
        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
                elements.append(this->allocator, nullptr);
            } else {
                if (this->match(PeriodPeriodPeriod)) {
                    elements.append(this->allocator, this->parseBindingRestElement(builder, params, kind, isExplicitVariableDeclaration));
                    break;
                } else {
                    elements.append(this->allocator, this->parsePatternWithDefault(builder, params, kind, isExplicitVariableDeclaration));
                }
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        return this->finalize(node, builder.createArrayPatternNode(elements));
    }

    template <class ASTBuilder>
    ASTNode parsePropertyPattern(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        bool computed = false;
        bool shorthand = false;

        ASTNode keyNode = nullptr; //'': Node.PropertyKey;
        ASTNode valueNode = nullptr; //: Node.PropertyValue;

        if (this->lookahead.type == Token::IdentifierToken) {
            ALLOC_TOKEN(keyToken);
            *keyToken = this->lookahead;
            keyNode = this->parseVariableIdentifier(builder);
            AtomicString name = keyNode->asIdentifier()->name();

            if (this->match(Substitution)) {
                params.push_back(*keyToken);
                if (this->context->inParameterParsing) {
                    this->currentScopeContext->m_functionBodyBlockIndex = 1;
                }
                if (kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword) {
                    addDeclaredNameIntoContext(name, this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
                }
                shorthand = true;
                this->nextToken();

                ASTNode expr = this->parseAssignmentExpression<ASTBuilder, false>(builder);
                this->addImplicitName(expr, name);

                valueNode = this->finalize(this->startNode(keyToken), builder.createAssignmentPatternNode(keyNode, expr));
            } else if (!this->match(Colon)) {
                params.push_back(*keyToken);
                if (kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword) {
                    addDeclaredNameIntoContext(name, this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
                }
                shorthand = true;
                valueNode = keyNode;
            } else {
                this->expect(Colon);
                valueNode = this->parsePatternWithDefault(builder, params, kind, isExplicitVariableDeclaration);
            }
        } else {
            computed = this->match(LeftSquareBracket);
            if (computed) {
                this->nextToken();
                keyNode = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                this->expect(RightSquareBracket);
            } else {
                keyNode = this->parseObjectPropertyKey(builder);
            }
            this->expect(Colon);
            valueNode = this->parsePatternWithDefault(builder, params, kind, isExplicitVariableDeclaration);
        }

        return this->finalize(node, builder.createPropertyNode(keyNode, valueNode, PropertyNode::Kind::Init, computed, shorthand));
    }

    template <class ASTBuilder>
    ASTNode parseObjectPattern(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();
        ASTNodeList properties;
        bool hasRestElement = false;

        this->nextToken();
        while (!this->match(RightBrace)) {
            if (this->match(PeriodPeriodPeriod)) {
                hasRestElement = true;
                properties.append(this->allocator, this->parseBindingRestElement(builder, params, kind, isExplicitVariableDeclaration));
                break;
            } else {
                properties.append(this->allocator, this->parsePropertyPattern(builder, params, kind, isExplicitVariableDeclaration));
            }

            if (!this->match(RightBrace)) {
                this->expect(Comma);
            }
        }
        this->expect(RightBrace);

        return this->finalize(node, builder.createObjectPatternNode(properties, hasRestElement));
    }

    template <class ASTBuilder>
    ASTNode parsePattern(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        if (this->match(LeftSquareBracket)) {
            return this->parseArrayPattern(builder, params, kind, isExplicitVariableDeclaration);
        } else if (this->match(LeftBrace)) {
            return this->parseObjectPattern(builder, params, kind, isExplicitVariableDeclaration);
        } else {
            if (this->matchKeyword(LetKeyword) && (kind == ConstKeyword || kind == LetKeyword)) {
                this->throwUnexpectedToken(this->lookahead, Messages::UnexpectedToken);
            }
            params.push_back(this->lookahead);
            return this->parseVariableIdentifier(builder, kind, isExplicitVariableDeclaration);
        }
    }

    template <class ASTBuilder>
    ASTNode parsePatternWithDefault(ASTBuilder& builder, SmallScannerResultVector& params, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        ASTNode pattern = this->parsePattern(builder, params, kind, isExplicitVariableDeclaration);
        if (this->match(PunctuatorKind::Substitution)) {
            if (this->context->inParameterParsing) {
                this->currentScopeContext->m_functionBodyBlockIndex = 1;
            }
            this->nextToken();

            // yield expression is not allowed in parameter
            if (this->context->inParameterParsing && this->currentScopeContext->m_isGenerator && this->matchKeyword(YieldKeyword)) {
                this->throwUnexpectedToken(this->lookahead);
            }

            const bool previousInParameterNameParsing = this->context->inParameterNameParsing;
            this->context->inParameterNameParsing = false;

            ASTNode right = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            if (pattern->isIdentifier()) {
                this->addImplicitName(right, pattern->asIdentifier()->name());
            }

            this->context->inParameterNameParsing = previousInParameterNameParsing;
            return this->finalize(this->startNode(startToken), builder.createAssignmentPatternNode(pattern, right));
        }

        return pattern;
    }

    template <class ASTBuilder>
    ASTNode parseFormalParameter(ASTBuilder& builder, ParseFormalParametersResult& options)
    {
        ASTNode param = nullptr;
        SmallScannerResultVector params;
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::PeriodPeriodPeriod) {
            param = this->parseRestElement(builder, params);
        } else {
            param = this->parsePatternWithDefault(builder, params);
        }
        for (size_t i = 0; i < params.size(); i++) {
            AtomicString as(this->escargotContext, params[i].relatedSource(this->scanner->sourceAsNormalView));
            this->validateParam(options, params[i], as);
        }
        options.params.push_back(builder.convertToParameterSyntaxNode(param));

        return param;
    }

    template <class ASTBuilder>
    void parseFormalParameters(ASTBuilder& builder, ParseFormalParametersResult& options, Scanner::SmallScannerResult* firstRestricted = nullptr, bool allowTrailingComma = true)
    {
        // parseFormalParameters only scans the parameter list of function
        // should be invoked only during the global parsing (program)
        ASSERT(!builder.willGenerateByteCode() && !this->isParsingSingleFunction);

        bool oldInParameterParsing = this->context->inParameterParsing;
        bool oldInParameterNameParsing = this->context->inParameterNameParsing;

        this->context->inParameterParsing = true;
        this->context->inParameterNameParsing = true;

        if (firstRestricted) {
            options.firstRestricted = *firstRestricted;
        }

        if (!this->match(RightParenthesis)) {
            options.paramSet.clear();
            while (this->startMarker.index < this->scanner->length) {
                this->parseFormalParameter(builder, options);
                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expect(Comma);
                if (allowTrailingComma && this->match(RightParenthesis)) {
                    break;
                }
            }
        }
        this->expect(RightParenthesis);
        if (UNLIKELY(options.params.size() > 65535)) {
            this->throwError("too many parameters in function");
        }

        this->context->inParameterParsing = oldInParameterParsing;
        this->context->inParameterNameParsing = oldInParameterNameParsing;
    }

    bool matchAsyncFunction()
    {
        bool match = this->matchContextualKeyword("async");
        if (match) {
            ScanState state = this->scanner->saveState();
            this->collectComments();
            ALLOC_TOKEN(next);
            this->scanner->lex(next);
            this->scanner->restoreState(state);

            match = (state.lineNumber == next->lineNumber) && (next->type == Token::KeywordToken) && (next->valueKeywordKind == KeywordKind::FunctionKeyword);
        }

        return match;
    }

    bool matchLetArray()
    {
        bool match = this->matchKeyword(LetKeyword);
        if (match) {
            ScanState state = this->scanner->saveState();
            this->collectComments();
            ALLOC_TOKEN(next);
            this->scanner->lex(next);
            this->scanner->restoreState(state);

            match = (next->type == Token::PunctuatorToken) && (next->valuePunctuatorKind == LeftSquareBracket);
        }

        return match;
    }

    // ECMA-262 12.2.5 Array Initializer

    template <class ASTBuilder, bool checkLeftHasRestrictedWord>
    ASTNode parseSpreadElement(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        this->nextToken();
        ASTNode arg = this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, checkLeftHasRestrictedWord>);
        return this->finalize(node, builder.createSpreadElementNode(arg));
    }

    template <class ASTBuilder>
    ASTNode parseArrayInitializer(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        ASTNodeList elements;

        this->nextToken();

        bool hasSpreadElement = false;

        while (!this->match(RightSquareBracket)) {
            if (this->match(Comma)) {
                this->nextToken();
                elements.append(this->allocator, nullptr);
            } else if (this->match(PeriodPeriodPeriod)) {
                elements.append(this->allocator, this->parseSpreadElement<ASTBuilder, true>(builder));
                hasSpreadElement = true;
                if (!this->match(RightSquareBracket)) {
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    this->expect(Comma);
                }
            } else {
                elements.append(this->allocator, this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, true>));
                if (!this->match(RightSquareBracket)) {
                    this->expect(Comma);
                }
            }
        }
        this->expect(RightSquareBracket);

        return this->finalize(node, builder.createArrayExpressionNode(elements, AtomicString(), nullptr, hasSpreadElement, false));
    }

    // ECMA-262 12.2.6 Object Initializer

    template <class ASTBuilder>
    ASTNode parsePropertyMethod(ASTBuilder& builder, ParseFormalParametersResult& params)
    {
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        const bool previousStrict = this->context->strict;
        const bool previousAllowStrictDirective = this->context->allowStrictDirective;
        this->context->allowStrictDirective = !this->currentScopeContext->m_hasParameterOtherThanIdentifier;
        ASTNode body = this->isolateCoverGrammar(builder, &Parser::parseFunctionSourceElements<ASTBuilder>);
        if (this->context->strict && params.firstRestricted) {
            this->throwUnexpectedToken(params.firstRestricted, params.message);
        }
        if (this->context->strict && params.stricted) {
            this->throwUnexpectedToken(params.stricted, params.message);
        }
        this->context->strict = previousStrict;
        this->context->allowStrictDirective = previousAllowStrictDirective;

        return body;
    }

    template <class ASTBuilder>
    ASTNode parseClassStaticFieldInitializer(ASTBuilder& builder, const AtomicString& className, size_t debuggerLineStart)
    {
        ASSERT(this->match(Substitution));

        this->nextToken();

        const bool previousAllowArguments = this->context->allowArguments;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;

        this->context->allowArguments = false;
        this->context->allowSuperProperty = true;

        MetaNode node = this->createNode();
        ASTNode expr = this->parseAssignmentExpressionAndWrapIntoArrowFunction(builder, className, debuggerLineStart);
        this->consumeSemicolon();

        this->context->allowArguments = previousAllowArguments;
        this->context->allowSuperProperty = previousAllowSuperProperty;

        return expr;
    }

    template <class ASTBuilder>
    ASTNode parseClassFieldInitializer(ASTBuilder& builder, size_t debuggerLineStart)
    {
        ASSERT(this->match(Substitution));

        this->nextToken();

        const bool previousAllowArguments = this->context->allowArguments;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;

        this->context->allowArguments = false;
        this->context->allowSuperProperty = true;

        ASTNode expr = this->parseAssignmentExpressionAndWrapIntoArrowFunction(builder, AtomicString(), debuggerLineStart);
        this->consumeSemicolon();

        this->context->allowArguments = previousAllowArguments;
        this->context->allowSuperProperty = previousAllowSuperProperty;

        return expr;
    }

    template <class ASTBuilder>
    ASTNode parsePropertyMethodFunction(ASTBuilder& builder, bool allowSuperCall, bool isGenerator, bool isAsyncFunction, const MetaNode& functionStart)
    {
        MetaNode node = this->createNode();

        if (tryToSkipFunctionParsing()) {
            return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
        }

        const bool previousAllowNewTarget = this->context->allowNewTarget;
        const bool previousAllowArguments = this->context->allowArguments;
        const bool previousAllowSuperCall = this->context->allowSuperCall;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;
        const bool previousAllowYield = this->context->allowYield;
        const bool previousAwait = this->context->await;
        const bool previousInArrowFunction = this->context->inArrowFunction;

        this->context->allowNewTarget = true;
        this->context->allowArguments = true;
        this->context->allowSuperProperty = true;
        this->context->allowYield = false;
        this->context->inArrowFunction = false;

        if (allowSuperCall) {
            this->context->allowSuperCall = true;
        }

        this->expect(LeftParenthesis);
        BEGIN_FUNCTION_SCANNING(AtomicString());

        this->currentScopeContext->m_functionStartLOC.index = functionStart.index;
        this->currentScopeContext->m_functionStartLOC.column = functionStart.column;
        this->currentScopeContext->m_functionStartLOC.line = functionStart.line;
        this->currentScopeContext->m_allowSuperProperty = true;
        if (allowSuperCall) {
            this->currentScopeContext->m_allowSuperCall = true;
        }
        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionExpression;
        this->currentScopeContext->m_isGenerator = isGenerator;
        this->currentScopeContext->m_isAsync = isAsyncFunction;

        ParseFormalParametersResult params;
        this->parseFormalParameters(newBuilder, params);
        extractNamesFromFunctionParams(params);

        this->context->await = isAsyncFunction;
        this->context->allowYield = isGenerator;
        this->parsePropertyMethod(newBuilder, params);

        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;
        this->context->allowSuperCall = previousAllowSuperCall;
        this->context->allowSuperProperty = previousAllowSuperProperty;
        this->context->allowYield = previousAllowYield;
        this->context->await = previousAwait;
        this->context->inArrowFunction = previousInArrowFunction;

        END_FUNCTION_SCANNING();
        return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
    }

    template <class ASTBuilder>
    ASTNode parseObjectPropertyKey(ASTBuilder& builder, bool allowPrivate = false, bool* isPrivateOut = nullptr)
    {
        MetaNode node = this->createNode();
        if (UNLIKELY(allowPrivate)) {
            *isPrivateOut = this->match(PunctuatorKind::Hash);
            if (*isPrivateOut) {
                size_t oldStart = this->lookahead.start;
                this->nextToken();
                if (oldStart + 1 != this->lookahead.start
                    || this->lookahead.type != Token::IdentifierToken) {
                    this->throwUnexpectedToken(this->lookahead);
                }
            }
        }

        ALLOC_TOKEN(token);
        this->nextToken(token);

        ASTNode key = nullptr;
        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (UNLIKELY(this->context->strict && token->octal)) {
                this->throwUnexpectedToken(*token, Messages::StrictOctalLiteral);
            }
            if (UNLIKELY(this->context->strict && this->lookahead.startWithZero)) {
                this->throwUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            // const raw = this->getTokenRaw(token);
            {
                Value v;
                if (builder.isNodeGenerator()) {
                    if (builder.willGenerateByteCode()) {
                        if (token->type == Token::NumericLiteralToken) {
                            auto d = token->valueNumberLiteral(this->scanner);
                            if (LIKELY(!d.second)) {
                                if (this->context->inLoop || d.first.asNumber() == 0) {
                                    this->insertNumeralLiteral(d.first);
                                }
                            }
                            v = d.first;
                        } else {
                            v = token->valueStringLiteralToValue(this->scanner);
                        }
                    }
                } else {
                    if (token->type == Token::StringLiteralToken) {
                        builder.setValueStringLiteral(token->valueStringLiteral(this->scanner));
                    }
                }
                key = this->finalize(node, builder.createLiteralNode(v));
            }
            break;

        case Token::IdentifierToken:
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::KeywordToken: {
            {
                if (UNLIKELY(isPrivateOut && *isPrivateOut)) {
                    key = this->finalize(node, finishClassPrivateIdentifier(builder, token));
                } else {
                    TrackUsingNameBlocker blocker(this);
                    key = this->finalize(node, finishIdentifier(builder, token));
                }
            }
            break;
        }
        case Token::PunctuatorToken:
            if (token->valuePunctuatorKind == LeftSquareBracket) {
                key = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                this->expect(RightSquareBracket);
            } else {
                this->throwUnexpectedToken(*token);
            }
            break;

        default:
            this->throwUnexpectedToken(*token);
        }

        return key;
    }

    template <class ASTBuilder>
    ASTNode parseObjectProperty(ASTBuilder& builder, bool& propertyNameIs__proto__) //: Node.Property
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        MetaNode node = this->createNode();
        Marker startMarker = this->lastMarker;

        PropertyNode::Kind kind;
        ASTNode keyNode = nullptr; //'': Node.PropertyKey;
        ASTNode valueNode = nullptr; //: Node.PropertyValue;

        bool computed = false;
        bool method = false;
        bool shorthand = false;
        bool isProto = false;
        bool isGenerator = false;
        bool isAsync = false;

        if (token->type == Token::IdentifierToken) {
            this->nextToken();
            computed = this->match(LeftSquareBracket);
            isAsync = !this->hasLineTerminator && (token->relatedSource(this->scanner->source) == "async") && !this->match(Colon) && !this->match(LeftParenthesis) && !this->match(Comma);
            if (isAsync) {
                isGenerator = this->match(Multiply);
                if (isGenerator) {
                    this->nextToken();
                    computed = this->match(LeftSquareBracket);
                }
                keyNode = this->parseObjectPropertyKey(builder);
            } else {
                TrackUsingNameBlocker blocker(this);
                keyNode = this->finalize(node, finishIdentifier(builder, token));
            }
        } else if (this->match(PunctuatorKind::Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder);
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(&this->lookahead);
        bool isGet = false;
        bool isSet = false;
        if (token->type == Token::IdentifierToken && !isAsync && lookaheadPropertyKey) {
            ParserStringView sv = token->valueStringLiteral(this->scanner);
            const auto& d = sv.bufferAccessData();
            if (d.length == 3) {
                if (d.equalsSameLength("get")) {
                    isGet = true;
                    // if token->hasAllocatedString is true, the token has escaped string like '\uxxet'
                    if (UNLIKELY(token->hasAllocatedString)) {
                        this->throwUnexpectedToken(*token, Messages::KeywordMustNotContainEscapedCharacters);
                    }
                } else if (d.equalsSameLength("set")) {
                    isSet = true;
                    // if token->hasAllocatedString is true, the token has escaped string like '\uxxet'
                    if (UNLIKELY(token->hasAllocatedString)) {
                        this->throwUnexpectedToken(*token, Messages::KeywordMustNotContainEscapedCharacters);
                    }
                }
            }
        }

        if (isGet) {
            kind = PropertyNode::Kind::Get;
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder);
            valueNode = this->parseGetterMethod(builder, node);
        } else if (isSet) {
            kind = PropertyNode::Kind::Set;
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder);
            valueNode = this->parseSetterMethod(builder, node);
        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorKind == PunctuatorKind::Multiply && lookaheadPropertyKey) {
            kind = PropertyNode::Kind::Init;
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder);
            valueNode = this->parseGeneratorMethod(builder, node);
            method = true;
        } else {
            if (!keyNode) {
                this->throwUnexpectedToken(this->lookahead);
            }
            kind = PropertyNode::Kind::Init;
            if (this->match(PunctuatorKind::Colon) && !isAsync) {
                // FIXME check !this->isParsingSingleFunction
                isProto = !this->isParsingSingleFunction && builder.isPropertyKey(keyNode, "__proto__");
                if (!computed && isProto) {
                    propertyNameIs__proto__ = true;
                }
                this->nextToken();
                valueNode = this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, true>);

                if (!computed && !isProto && keyNode->isIdentifier()) {
                    this->addImplicitName(valueNode, keyNode->asIdentifier()->name());
                }
            } else if (this->match(LeftParenthesis)) {
                valueNode = this->parsePropertyMethodFunction(builder, false, isGenerator, isAsync, node);
                method = true;
            } else {
                if (token->type != Token::IdentifierToken) {
                    if (token->type == Token::KeywordToken && token->valueKeywordKind == KeywordKind::YieldKeyword) {
                        // yield is a valid Identifier in AssignmentProperty outside of strict mode and generator functions
                        if (this->context->allowYield) {
                            this->throwUnexpectedToken(*token);
                        }
                    } else if (token->type == Token::KeywordToken && token->valueKeywordKind == KeywordKind::LetKeyword) {
                        // In non-strict mode, let is a valid Identifier
                        if (this->context->strict) {
                            this->throwUnexpectedToken(*token);
                        }
                    } else {
                        ALLOC_TOKEN(token);
                        this->nextToken(token);
                        this->throwUnexpectedToken(*token);
                    }
                }

                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    ASTNode init = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                    valueNode = this->finalize(node, builder.createAssignmentPatternNode(keyNode, init));

                    if (!computed && keyNode->isIdentifier()) {
                        this->addImplicitName(init, keyNode->asIdentifier()->name());
                    }
                } else {
                    shorthand = true;
                    valueNode = keyNode;
                    // we should insert the name of valueNode here because the keyNode is blocked by TrackUsingNameBlocker above.
                    if (keyNode->isIdentifier()) {
                        this->insertUsingName(keyNode->asIdentifier()->name());
                    }
                }
            }
        }

        // check if restricted words are used as target in array/object initializer
        if (shorthand && this->context->strict && token->type == Token::IdentifierToken) {
            AtomicString name;
            name = keyNode->asIdentifier()->name();
            if (this->scanner->isRestrictedWord(name)) {
                this->context->hasRestrictedWordInArrayOrObjectInitializer = true;
            }
        }

        if (!computed && !isProto && keyNode->isIdentifier()) {
            this->addImplicitName(valueNode, keyNode->asIdentifier()->name());
        }

        if (!this->isParsingSingleFunction && (method || isGet || isSet)) {
            this->lastPoppedScopeContext->m_isObjectMethod = true;
        }

        return this->finalize(node, builder.createPropertyNode(keyNode, valueNode, kind, computed, shorthand));
    }

    template <class ASTBuilder>
    ASTNode parseObjectInitializer(ASTBuilder& builder)
    {
        this->nextToken();
        MetaNode node = this->createNode();
        ASTNodeList properties;

        size_t __proto__Count = 0;
        while (!this->match(RightBrace)) {
            if (this->match(PeriodPeriodPeriod)) {
                properties.append(this->allocator, this->parseSpreadElement<ASTBuilder, true>(builder));
            } else {
                bool propertyNameIs__proto__ = false;
                properties.append(this->allocator, this->parseObjectProperty(builder, propertyNameIs__proto__));
                if (propertyNameIs__proto__) {
                    __proto__Count++;
                }
            }
            if (!this->match(RightBrace)) {
                this->expectCommaSeparator();
            }
        }
        this->expect(RightBrace);

        // Annex B defines an early error for duplicate PropertyName of `__proto__`, in object initializers,
        // but this does not apply to Object Assignment patterns
        if (!this->match(PunctuatorKind::Substitution) && __proto__Count >= 2) {
            this->throwError(Messages::DuplicateProtoProperty);
        }

        return this->finalize(node, builder.createObjectExpressionNode(properties));
    }

    // ECMA-262 12.2.9 Template Literals
    TemplateElement* parseTemplateHead(bool isTaggedTemplate = false)
    {
        ASSERT(this->lookahead.type == Token::TemplateToken);

        ALLOC_TOKEN(token);
        this->nextToken(token);
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->valueRaw = token->valueTemplate->valueRaw;
        tm->tail = token->valueTemplate->tail;
        if (!isTaggedTemplate && token->valueTemplate->error) {
            throw token->valueTemplate->error.value();
        }
        return tm;
    }

    TemplateElement* parseTemplateElement(bool isTaggedTemplate = false)
    {
        if (!this->match(PunctuatorKind::RightBrace)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        // Re-scan the current token (right brace) as a template string.
        this->scanner->scanTemplate(&this->lookahead);

        ALLOC_TOKEN(token);
        this->nextToken(token);
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->valueRaw = token->valueTemplate->valueRaw;
        tm->tail = token->valueTemplate->tail;
        if (!isTaggedTemplate && token->valueTemplate->error) {
            throw token->valueTemplate->error.value();
        }
        return tm;
    }


    template <class ASTBuilder>
    ASTNode parseTemplateLiteral(ASTBuilder& builder, bool isTaggedTemplate = false)
    {
        MetaNode node = this->createNode();

        ASTNodeList expressions;
        TemplateElementVector* quasis = new TemplateElementVector;
        quasis->push_back(this->parseTemplateHead(isTaggedTemplate));
        while (!quasis->back()->tail) {
            expressions.append(this->allocator, this->parseExpression(builder));
            TemplateElement* quasi = this->parseTemplateElement(isTaggedTemplate);
            quasis->push_back(quasi);
        }
        return this->finalize(node, builder.createTemplateLiteralNode(quasis, expressions));
    }

    template <class ASTBuilder>
    ASTNode parseGroupExpression(ASTBuilder& builder)
    {
        ASTNode exprNode = nullptr;

        this->nextToken();
        if (this->match(RightParenthesis)) {
            this->nextToken();
            if (!this->match(Arrow)) {
                this->expect(Arrow);
            }

            exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode());
        } else {
            ALLOC_TOKEN(startToken);
            *startToken = this->lookahead;
            SmallScannerResultVector params;
            if (this->match(PeriodPeriodPeriod)) {
                exprNode = this->parseRestElement(builder, params);
                this->expect(RightParenthesis);
                if (!this->match(Arrow)) {
                    this->expect(Arrow);
                }
                ASTNodeList expressions;
                expressions.append(this->allocator, exprNode);
                exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(expressions));
            } else {
                bool arrow = false;
                this->context->isBindingElement = true;
                exprNode = this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);

                if (this->match(Comma)) {
                    ASTNodeList expressions;

                    this->context->isAssignmentTarget = false;
                    expressions.append(this->allocator, exprNode);
                    while (this->startMarker.index < this->scanner->length) {
                        if (!this->match(Comma)) {
                            break;
                        }
                        this->nextToken();

                        if (this->match(RightParenthesis)) {
                            this->nextToken();
                            for (ASTSentinelNode expression = expressions.begin(); expression != expressions.end(); expression = expression->next()) {
                                expression->setASTNode(builder.reinterpretExpressionAsPattern(expression->astNode()));
                            }
                            arrow = true;
                            exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(expressions));
                        } else if (this->match(PeriodPeriodPeriod)) {
                            if (!this->context->isBindingElement) {
                                this->throwUnexpectedToken(this->lookahead);
                            }
                            expressions.append(this->allocator, this->parseRestElement(builder, params));
                            this->expect(RightParenthesis);
                            if (!this->match(Arrow)) {
                                this->expect(Arrow);
                            }
                            this->context->isBindingElement = false;
                            for (ASTSentinelNode expression = expressions.begin(); expression != expressions.end(); expression = expression->next()) {
                                expression->setASTNode(builder.reinterpretExpressionAsPattern(expression->astNode()));
                            }
                            arrow = true;
                            exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(expressions));
                        } else {
                            expressions.append(this->allocator, this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>));
                        }
                        if (arrow) {
                            break;
                        }
                    }
                    if (!arrow) {
                        exprNode = this->finalize(this->startNode(startToken), builder.createSequenceExpressionNode(expressions));
                    }
                }

                if (!arrow) {
                    this->expect(RightParenthesis);
                    if (this->match(Arrow)) {
                        if (exprNode->type() == Identifier && exprNode->asIdentifier()->name() == "yield") {
                            arrow = true;
                            ASTNodeList expressions;
                            expressions.append(this->allocator, exprNode);
                            exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(expressions));
                        }
                        if (!arrow) {
                            if (!this->context->isBindingElement) {
                                this->throwUnexpectedToken(this->lookahead);
                            }

                            if (builder.isNodeGenerator() && (exprNode->type() == SequenceExpression)) {
                                ASTNodeList& expressions = exprNode->asSequenceExpression()->expressions();
                                for (ASTSentinelNode expression = expressions.begin(); expression != expressions.end(); expression = expression->next()) {
                                    expression->setASTNode(builder.reinterpretExpressionAsPattern(expression->astNode()));
                                }
                            } else {
                                exprNode = builder.reinterpretExpressionAsPattern(exprNode);
                            }

                            ASTNodeList params;
                            if (builder.isNodeGenerator() && (exprNode->type() == SequenceExpression)) {
                                params = exprNode->asSequenceExpression()->expressions();
                            } else {
                                params.append(this->allocator, exprNode);
                            }

                            exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(params));
                        }
                    }
                    this->context->isBindingElement = false;
                }
            }
        }

        return exprNode;
    }

    // ECMA-262 12.3 Left-Hand-Side Expressions

    template <class ASTBuilder>
    ASTNodeList parseArguments(ASTBuilder& builder)
    {
        this->nextToken();
        ASTNodeList args;
        if (!this->match(RightParenthesis)) {
            while (true) {
                ASTNode expr = nullptr;
                if (this->match(PeriodPeriodPeriod)) {
                    expr = this->parseSpreadElement<ASTBuilder, false>(builder);
                } else {
                    expr = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                }
                args.append(this->allocator, expr);
                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expectCommaSeparator();
                if (this->match(RightParenthesis)) {
                    break;
                }
            }
        }
        this->expect(RightParenthesis);
        if (UNLIKELY(args.size() > 65535)) {
            this->throwError("too many arguments in call");
        }

        return args;
    }

    bool isIdentifierName(Scanner::ScannerResult* token)
    {
        ASSERT(token != nullptr);
        return token->type == Token::IdentifierToken || token->type == Token::KeywordToken || token->type == Token::BooleanLiteralToken || token->type == Token::NullLiteralToken;
    }

    template <class ASTBuilder>
    ASTNode parseIdentifierName(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(*token);
        }
        return this->finalize(node, finishIdentifier(builder, token));
    }

    template <class ASTBuilder>
    ASTNode parseClassPrivateIdentifierName(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(*token);
        }
        return this->finalize(node, finishClassPrivateIdentifier(builder, token));
    }

    template <class ASTBuilder>
    ASTNode parseNewExpression(ASTBuilder& builder)
    {
        if (this->lookahead.end - this->lookahead.start != 3) {
            this->throwUnexpectedToken(this->lookahead, Messages::KeywordMustNotContainEscapedCharacters);
        }
        this->nextToken();

        if (UNLIKELY(this->match(Period))) {
            this->nextToken();
            if (this->lookahead.type == Token::IdentifierToken && this->context->allowNewTarget && this->lookahead.relatedSource(this->scanner->source) == "target") {
                this->nextToken();
                this->currentScopeContext->m_hasSuperOrNewTarget = true;
                MetaNode node = this->createNode();
                return this->finalize(node, builder.createMetaPropertyNode(MetaPropertyNode::NewTarget));
            } else {
                this->throwUnexpectedToken(this->lookahead);
            }
        }

        MetaNode node = this->createNode();
        bool seenLeftParenthesis = this->match(LeftParenthesis);
        ASTNode callee = this->isolateCoverGrammar(builder, &Parser::parseLeftHandSideExpression<ASTBuilder>);

        if (UNLIKELY(callee->type() == ASTNodeType::ImportCall && !seenLeftParenthesis)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        ASTNodeList args;
        if (this->match(LeftParenthesis)) {
            args = this->parseArguments(builder);
        }
        this->context->isAssignmentTarget = false;
        this->context->isBindingElement = false;

        return this->finalize(node, builder.createNewExpressionNode(callee, args));
    }

    template <class ASTBuilder>
    ASTNode parseImportExpression(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        // check escaped
        if (this->lookahead.end - this->lookahead.start != 6) {
            this->throwUnexpectedToken(this->lookahead);
        }
        this->expectKeyword(ImportKeyword);
        if (this->match(Period)) {
            if (this->sourceType != Module) {
                this->throwUnexpectedToken(this->lookahead, Messages::IllegalImportDeclaration);
            }

            this->nextToken();
            ALLOC_TOKEN(nameToken);
            *nameToken = this->lookahead;
            TrackUsingNameBlocker blocker(this);
            ASTNode property = this->parseIdentifierName(builder);
            AtomicString name = property->asIdentifier()->name();
            if (!name.string()->equals("meta") || nameToken->hasAllocatedString /* is escaped */) {
                this->throwUnexpectedToken(*nameToken);
            }
            return this->finalize(node, builder.createMetaPropertyNode(MetaPropertyNode::ImportMeta));
        } else if (this->match(LeftParenthesis)) {
            this->nextToken();
            ASTNode arg = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            this->expect(RightParenthesis);
            return this->finalize(node, builder.createImportCallNode(arg));
        } else {
            this->throwUnexpectedToken(this->lookahead);
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    template <class ASTBuilder>
    ASTNode parseAsyncArgument(ASTBuilder& builder)
    {
        ASTNode arg = this->parseAssignmentExpression<ASTBuilder, false>(builder);
        this->context->firstCoverInitializedNameError.reset();
        return arg;
    }

    template <class ASTBuilder>
    ASTNodeList parseAsyncArguments(ASTBuilder& builder)
    {
        this->nextToken();
        ASTNodeList args;

        if (!this->match(RightParenthesis)) {
            while (true) {
                ASTNode expr = this->match(PeriodPeriodPeriod) ? this->parseSpreadElement<ASTBuilder, false>(builder) : this->isolateCoverGrammar(builder, &Parser::parseAsyncArgument<ASTBuilder>);
                args.append(this->allocator, expr);
                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expectCommaSeparator();
                if (this->match(RightParenthesis)) {
                    break;
                }
            }
        }
        this->expect(RightParenthesis);
        if (UNLIKELY(args.size() > 65535)) {
            this->throwError("too many arguments in call");
        }

        return args;
    }

    template <class ASTBuilder>
    ASTNode parseLeftHandSideExpressionAllowCall(ASTBuilder& builder)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        bool maybeAsync = this->matchContextualKeyword("async");
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        ASTNode exprNode = nullptr;

        if (this->context->inFunctionBody && this->matchKeyword(SuperKeyword)) {
            throwIfSuperOperationIsNotAllowed();
            this->currentScopeContext->m_hasSuperOrNewTarget = true;

            exprNode = this->finishSuperExpression(builder, this->createNode(), this->lookahead.valuePunctuatorKind == LeftParenthesis);
        } else if (UNLIKELY(this->matchKeyword(NewKeyword))) {
            exprNode = this->inheritCoverGrammar(builder, &Parser::parseNewExpression<ASTBuilder>);
        } else if (UNLIKELY(this->matchKeyword(ImportKeyword))) {
            this->context->isBindingElement = false;
            this->context->isAssignmentTarget = false;
            exprNode = this->inheritCoverGrammar(builder, &Parser::parseImportExpression<ASTBuilder>);
        } else {
            exprNode = this->inheritCoverGrammar(builder, &Parser::parsePrimaryExpression<ASTBuilder>);
        }

        bool seenOptionalExpression = false;
        while (true) {
            bool isPunctuatorTokenLookahead = this->lookahead.type == Token::PunctuatorToken;
            if (isPunctuatorTokenLookahead) {
                if (this->lookahead.valuePunctuatorKind == Period) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    exprNode = parseMemberExpressionPreComputedCase<ASTBuilder>(builder, this->startNode(startToken), exprNode, seenOptionalExpression);
                } else if (this->lookahead.valuePunctuatorKind == LeftParenthesis) {
                    bool asyncArrow = maybeAsync && (startToken->lineNumber == this->lookahead.lineNumber);
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    ASTNodeList args = asyncArrow ? this->parseAsyncArguments(builder) : this->parseArguments(builder);
                    // check callee of CallExpressionNode
                    if (exprNode->isIdentifier() && exprNode->asIdentifier()->name() == escargotContext->staticStrings().eval) {
                        this->currentScopeContext->m_hasEval = true;
                    }
                    exprNode = this->finalize(this->startNode(startToken), builder.createCallExpressionNode(exprNode, args, false, seenOptionalExpression));
                    if (asyncArrow && this->match(Arrow)) {
                        for (ASTSentinelNode arg = args.begin(); arg != args.end(); arg = arg->next()) {
                            arg->setASTNode(builder.reinterpretExpressionAsPattern(arg->astNode()));
                        }
                        exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(args, true));
                    }
                } else if (this->lookahead.valuePunctuatorKind == LeftSquareBracket) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    ASTNode property = this->isolateCoverGrammar(builder, &Parser::parseExpression<ASTBuilder>);
                    exprNode = this->finalize(this->startNode(startToken), builder.createMemberExpressionNode(exprNode, property, false, seenOptionalExpression));
                    this->expect(RightSquareBracket);
                } else if (this->lookahead.valuePunctuatorKind == GuessDot) {
                    bool oldBindingElement = this->context->isBindingElement;
                    bool oldAssignmentTarget = this->context->isAssignmentTarget;
                    Marker startMarker = this->startMarker;

                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    this->nextToken();
                    if (this->match(PunctuatorKind::LeftSquareBracket)) {
                        seenOptionalExpression = true;
                        this->nextToken();
                        ASTNode property = this->isolateCoverGrammar(builder, &Parser::parseExpression<ASTBuilder>);
                        exprNode = this->finalize(this->startNode(startToken), builder.createMemberExpressionNode(exprNode, property, false, true));
                        this->expect(RightSquareBracket);
                        if (this->lookahead.type == Token::TemplateToken) {
                            this->throwUnexpectedToken(this->lookahead);
                        }
                    } else if (this->lookahead.type == Token::IdentifierToken || this->match(PunctuatorKind::Hash)) {
                        seenOptionalExpression = true;
                        exprNode = parseMemberExpressionPreComputedCase<ASTBuilder>(builder, this->startNode(startToken), exprNode, seenOptionalExpression);
                        if (this->lookahead.type == Token::TemplateToken) {
                            this->throwUnexpectedToken(this->lookahead);
                        }
                    } else if (this->match(PunctuatorKind::LeftParenthesis)) {
                        ASTNodeList args = this->parseArguments(builder);
                        // check callee of CallExpressionNode
                        if (exprNode->isIdentifier() && exprNode->asIdentifier()->name() == escargotContext->staticStrings().eval) {
                            this->currentScopeContext->m_hasEval = true;
                        }
                        exprNode = this->finalize(this->startNode(startToken), builder.createCallExpressionNode(exprNode, args, true));
                    } else {
                        this->context->isBindingElement = oldBindingElement;
                        this->context->isAssignmentTarget = oldAssignmentTarget;
                        this->scanner->index = startMarker.index;
                        this->scanner->lineNumber = startMarker.lineNumber;
                        this->scanner->lineStart = startMarker.lineStart;
                        this->nextToken();
                        ASSERT(this->match(PunctuatorKind::GuessDot));

                        this->scanner->index--;
                        this->lookahead.valuePunctuatorKind = GuessMark;
                        break;
                    }
                } else {
                    break;
                }
            } else if (this->lookahead.type == Token::TemplateToken && this->lookahead.valueTemplate->head) {
                ASTNode quasi = this->parseTemplateLiteral(builder, true);
                ASTNode convertedNode = builder.convertTaggedTemplateExpressionToCallExpression(quasi, exprNode, this->taggedTemplateExpressionIndex, this->currentScopeContext, escargotContext->staticStrings().raw);
                if (builder.willGenerateByteCode()) {
                    this->taggedTemplateExpressionIndex++;
                }

                // CodeBlock should allocate a RareData for caching
                this->currentScopeContext->m_needRareData = true;
                exprNode = this->finalize(this->startNode(startToken), builder.createTaggedTemplateExpressionNode(exprNode, quasi, convertedNode));
            } else {
                break;
            }
        }
        this->context->allowIn = previousAllowIn;

        return exprNode;
    }

    template <class ASTBuilder>
    ASTNode finishSuperExpression(ASTBuilder& builder, MetaNode node, bool isCall)
    {
        this->currentScopeContext->m_hasSuperOrNewTarget = true;
        return this->finalize(node, builder.createSuperExpressionNode(isCall));
    }

    template <class ASTBuilder>
    ASTNode parseSuper(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        this->expectKeyword(SuperKeyword);
        if (!this->match(LeftSquareBracket) && !this->match(Period)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        return this->finishSuperExpression(builder, node, false);
    }

    template <class ASTBuilder>
    ASTNode parseMemberExpressionPreComputedCase(ASTBuilder& builder, const MetaNode& node, ASTNode exprNode, bool seenOptionalExpression)
    {
        TrackUsingNameBlocker blocker(this);
        if (UNLIKELY(this->match(PunctuatorKind::Hash))) {
            auto lastIndex = this->lookahead.start;
            ALLOC_TOKEN(startToken);
            *startToken = this->lookahead;
            this->nextToken();
            if (UNLIKELY(this->lookahead.start != lastIndex + 1)) {
                this->throwUnexpectedToken(*startToken);
            }
            ASTNode property = this->parseClassPrivateIdentifierName(builder);
            auto type = exprNode->type();
            if (UNLIKELY(type == ASTNodeType::SuperExpression)) {
                this->throwError(Messages::CannnotUsePrivateFieldHere);
            }
            if (!this->isParsingSingleFunction) {
                this->currentScopeContext->m_hasClassPrivateNameExpression = true;
                if (this->currentClassInfo) {
                    this->currentClassInfo->insertClassPrivateUsingName(property->asIdentifier()->name(),
                                                                        startToken->start,
                                                                        startToken->lineNumber,
                                                                        startToken->lineStart);
                } else {
                    this->throwError(Messages::PrivateFieldMustBeDeclared, property->asIdentifier()->name().string());
                }
            }
            return this->finalize(node, builder.createMemberExpressionNode(exprNode, property, true, seenOptionalExpression, true));
        } else {
            ASTNode property = this->parseIdentifierName(builder);
            return this->finalize(node, builder.createMemberExpressionNode(exprNode, property, true, seenOptionalExpression));
        }
    }

    template <class ASTBuilder>
    ASTNode parseLeftHandSideExpression(ASTBuilder& builder)
    {
        // assert(this->context->allowIn, 'callee of new expression always allow in keyword.');
        ASSERT(this->context->allowIn);

        ASTNode exprNode = nullptr;

        if (this->matchKeyword(SuperKeyword) && this->context->inFunctionBody) {
            exprNode = this->parseSuper(builder);
        } else if (UNLIKELY(this->matchKeyword(NewKeyword))) {
            exprNode = this->inheritCoverGrammar(builder, &Parser::parseNewExpression<ASTBuilder>);
        } else if (UNLIKELY(this->matchKeyword(ImportKeyword))) {
            this->context->isBindingElement = false;
            this->context->isAssignmentTarget = false;
            exprNode = this->inheritCoverGrammar(builder, &Parser::parseImportExpression<ASTBuilder>);
        } else {
            exprNode = this->inheritCoverGrammar(builder, &Parser::parsePrimaryExpression<ASTBuilder>);
        }

        MetaNode node = this->startNode(&this->lookahead);
        bool seenOptionalExpression = false;
        while (true) {
            if (this->match(LeftSquareBracket)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->nextToken();
                ASTNode property = this->isolateCoverGrammar(builder, &Parser::parseExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createMemberExpressionNode(exprNode, property, false, seenOptionalExpression));
                this->expect(RightSquareBracket);
            } else if (this->match(Period)) {
                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = true;
                this->nextToken();
                exprNode = parseMemberExpressionPreComputedCase<ASTBuilder>(builder, node, exprNode, seenOptionalExpression);
            } else if (this->lookahead.type == Token::TemplateToken && this->lookahead.valueTemplate->head) {
                ASTNode quasi = this->parseTemplateLiteral(builder, true);
                ASTNode convertedNode = builder.convertTaggedTemplateExpressionToCallExpression(quasi, exprNode, this->taggedTemplateExpressionIndex, this->currentScopeContext, escargotContext->staticStrings().raw);
                if (builder.willGenerateByteCode()) {
                    this->taggedTemplateExpressionIndex++;
                }

                // CodeBlock should allocate a RareData for caching
                this->currentScopeContext->m_needRareData = true;
                exprNode = this->finalize(node, builder.createTaggedTemplateExpressionNode(exprNode, quasi, convertedNode));
            } else if (this->lookahead.valuePunctuatorKind == GuessDot) {
                bool oldBindingElement = this->context->isBindingElement;
                bool oldAssignmentTarget = this->context->isAssignmentTarget;
                Marker startMarker = this->startMarker;

                this->context->isBindingElement = false;
                this->context->isAssignmentTarget = false;
                this->nextToken();
                if (this->match(PunctuatorKind::LeftSquareBracket)) {
                    seenOptionalExpression = true;
                    this->nextToken();
                    ASTNode property = this->isolateCoverGrammar(builder, &Parser::parseExpression<ASTBuilder>);
                    exprNode = this->finalize(node, builder.createMemberExpressionNode(exprNode, property, false, true));
                    this->expect(RightSquareBracket);
                    if (this->lookahead.type == Token::TemplateToken) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                } else if (this->lookahead.type == Token::IdentifierToken) {
                    seenOptionalExpression = true;
                    TrackUsingNameBlocker blocker(this);
                    ASTNode property = this->parseIdentifierName(builder);
                    exprNode = this->finalize(node, builder.createMemberExpressionNode(exprNode, property, true, true));
                    if (this->lookahead.type == Token::TemplateToken) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                } else {
                    this->context->isBindingElement = oldBindingElement;
                    this->context->isAssignmentTarget = oldAssignmentTarget;
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();
                    ASSERT(this->match(PunctuatorKind::GuessDot));

                    this->scanner->index--;
                    this->lookahead.valuePunctuatorKind = GuessMark;
                    break;
                }
            } else {
                break;
            }
        }

        return exprNode;
    }

    // ECMA-262 12.4 Update Expressions

    template <class ASTBuilder>
    ASTNode parseUpdateExpression(ASTBuilder& builder)
    {
        ASTNode exprNode = nullptr;
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            ALLOC_TOKEN(token);
            this->nextToken(token);

            exprNode = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
            if (exprNode->isLiteral() || exprNode->type() == ASTNodeType::ThisExpression) {
                this->throwError(Messages::InvalidLHSInAssignment);
            }
            if (this->context->strict && exprNode->type() == Identifier && this->scanner->isRestrictedWord(exprNode->asIdentifier()->name())) {
                this->throwError(Messages::StrictLHSPrefix);
            }

            if (!this->context->isAssignmentTarget && this->context->strict) {
                this->throwError(Messages::InvalidLHSInAssignment);
            }

            auto exprNodeType = exprNode->type();
            if (UNLIKELY(exprNodeType == ASTNodeType::MemberExpression && exprNode->asMemberExpression()->isOptional())) {
                this->throwError(Messages::InvalidLHSInAssignment);
            }
            if (UNLIKELY(exprNodeType == ASTNodeType::ImportCall)) {
                this->throwError(Messages::InvalidLHSInAssignment);
            }

            MetaNode node = this->startNode(startToken);
            if (isPlus) {
                exprNode = this->finalize(node, builder.createUpdateExpressionIncrementPrefixNode(exprNode));
            } else {
                exprNode = this->finalize(node, builder.createUpdateExpressionDecrementPrefixNode(exprNode));
            }

            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        } else {
            exprNode = this->inheritCoverGrammar(builder, &Parser::parseLeftHandSideExpressionAllowCall<ASTBuilder>);
            if (!this->hasLineTerminator && this->lookahead.type == Token::PunctuatorToken && (this->match(PlusPlus) || this->match(MinusMinus))) {
                bool isPlus = this->match(PlusPlus);
                if (exprNode->isLiteral() || exprNode->type() == ASTNodeType::ThisExpression) {
                    this->throwError(Messages::InvalidLHSInAssignment);
                }
                if (this->context->strict && exprNode->isIdentifier() && this->scanner->isRestrictedWord(exprNode->asIdentifier()->name())) {
                    this->throwError(Messages::StrictLHSPostfix);
                }
                if (!this->context->isAssignmentTarget && this->context->strict) {
                    this->throwError(Messages::InvalidLHSInAssignment);
                }
                auto exprNodeType = exprNode->type();
                if (UNLIKELY(exprNodeType == ASTNodeType::MemberExpression && exprNode->asMemberExpression()->isOptional())) {
                    this->throwError(Messages::InvalidLHSInAssignment);
                }
                if (UNLIKELY(exprNodeType == ASTNodeType::ImportCall)) {
                    this->throwError(Messages::InvalidLHSInAssignment);
                }

                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                this->nextToken();

                if (isPlus) {
                    exprNode = this->finalize(this->startNode(startToken), builder.createUpdateExpressionIncrementPostfixNode(exprNode));
                } else {
                    exprNode = this->finalize(this->startNode(startToken), builder.createUpdateExpressionDecrementPostfixNode(exprNode));
                }
            }
        }

        return exprNode;
    }

    template <class ASTBuilder>
    ASTNode parseAwaitExpression(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        this->nextToken();
        ASTNode argument = this->parseUnaryExpression(builder);
        return this->finalize(node, builder.createAwaitExpressionNode(argument));
    }

    // ECMA-262 12.5 Unary Operators

    template <class ASTBuilder>
    ASTNode parseUnaryExpression(ASTBuilder& builder)
    {
        if (this->lookahead.type == Token::PunctuatorToken) {
            auto punctuatorsKind = this->lookahead.valuePunctuatorKind;
            ASTNode exprNode = nullptr;

            if (punctuatorsKind == Plus) {
                this->nextToken();
                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createUnaryExpressionPlusNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return exprNode;
            } else if (punctuatorsKind == Minus) {
                this->nextToken();
                if (this->lookahead.type == Token::NumericLiteralToken) {
                    return parseNumericLiteralNode(builder, true);
                } else {
                    MetaNode node = this->startNode(&this->lookahead);
                    ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                    exprNode = this->finalize(node, builder.createUnaryExpressionMinusNode(subExpr));
                    this->context->isAssignmentTarget = false;
                    this->context->isBindingElement = false;
                    return exprNode;
                }
            } else if (punctuatorsKind == Wave) {
                this->nextToken();
                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createUnaryExpressionBitwiseNotNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return exprNode;
            } else if (punctuatorsKind == ExclamationMark) {
                this->nextToken();
                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createUnaryExpressionLogicalNotNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return exprNode;
            }
        } else if (this->lookahead.type == Token::KeywordToken) {
            ASTNode exprNode = nullptr;

            if (this->lookahead.valueKeywordKind == DeleteKeyword) {
                this->nextToken();
                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);

                if (this->context->strict && subExpr->isIdentifier()) {
                    this->throwError(Messages::StrictDelete);
                }

                if (subExpr->type() == ASTNodeType::MemberExpression) {
                    if (subExpr->asMemberExpression()->isReferencePrivateField()) {
                        this->throwError(Messages::CannnotUsePrivateFieldHere);
                    }
                }

                exprNode = this->finalize(node, builder.createUnaryExpressionDeleteNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                return exprNode;
            } else if (this->lookahead.valueKeywordKind == VoidKeyword) {
                this->nextToken();
                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createUnaryExpressionVoidNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                return exprNode;
            } else if (this->lookahead.valueKeywordKind == TypeofKeyword) {
                this->nextToken();

                MetaNode node = this->startNode(&this->lookahead);
                ASTNode subExpr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);
                exprNode = this->finalize(node, builder.createUnaryExpressionTypeOfNode(subExpr));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                return exprNode;
            }
        } else if (this->matchContextualKeyword("await")) {
            bool parseAwait = false;
            if (this->context->await) {
                parseAwait = true;
            } else {
                if (inGlobalSourceCodeParsing()) {
                    Marker startMarker = this->startMarker;
                    this->nextToken();
                    if (!this->match(PunctuatorKind::Comma) && !this->match(PunctuatorKind::Substitution) && !this->match(PunctuatorKind::RightParenthesis)
                        && !this->match(PunctuatorKind::Colon) && !this->matchKeyword(KeywordKind::InstanceofKeyword)) {
                        this->currentScopeContext->m_isAsync = true;
                        parseAwait = true;
                    }

                    // rewind scanner
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();
                }
            }
            if (parseAwait) {
                ASTNode exprNode = this->parseAwaitExpression(builder);
                return exprNode;
            }
        }

        return this->parseUpdateExpression(builder);
    }

    template <class ASTBuilder>
    ASTNode parseExponentiationExpression(ASTBuilder& builder)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        bool seenMinus = this->match(Minus);
        ASTNode expr = this->inheritCoverGrammar(builder, &Parser::parseUnaryExpression<ASTBuilder>);

        if (!expr->isUnaryOperation() && this->match(Exponentiation)) {
            if (UNLIKELY(seenMinus)) {
                this->throwError("Unary operator used immediately before exponentiation expression. Parenthesis must be used to disambiguate operator precedence");
            }
            this->nextToken();
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            ASTNode left = expr;
            ASTNode right = this->isolateCoverGrammar(builder, &Parser::parseExponentiationExpression<ASTBuilder>);
            expr = this->finalize(this->startNode(startToken), builder.createBinaryExpressionExponentiationNode(left, right));
        }

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
            case NullishCoalescing:
                return 1;
            case LogicalOr:
                return 2;
            case LogicalAnd:
                return 3;
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

    template <class ASTBuilder>
    ASTNode parseBinaryExpression(ASTBuilder& builder)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;

        ASTNode expr = this->inheritCoverGrammar(builder, &Parser::parseExponentiationExpression<ASTBuilder>);

        ALLOC_TOKEN(token);
        *token = this->lookahead;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            typedef VectorWithInlineStorage<8, Scanner::SmallScannerResult, std::allocator<Scanner::SmallScannerResult>> SmallScannerResultVectorWithInlineStorage;

            SmallScannerResultVectorWithInlineStorage markers;
            markers.push_back(*startToken);
            markers.push_back(this->lookahead);
            ASTNode left = expr;
            ASTNode right = this->isolateCoverGrammar(builder, &Parser::parseExponentiationExpression<ASTBuilder>);

            VectorWithInlineStorage<8, ASTNode, std::allocator<ASTNode>> stack;
            SmallScannerResultVectorWithInlineStorage tokenStack;

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
                    Scanner::SmallScannerResult operator_ = tokenStack.back();
                    tokenStack.pop_back();
                    left = stack.back();
                    stack.pop_back();
                    markers.pop_back();
                    MetaNode node = this->startNode(&markers.back());
                    auto e = this->finalize(node, finishBinaryExpression(builder, left, right, &operator_));
                    stack.push_back(e);
                }

                // Shift.
                this->nextToken(token);
                token->prec = prec;
                tokenStack.push_back(*token);
                markers.push_back(this->lookahead);
                auto e = this->isolateCoverGrammar(builder, &Parser::parseExponentiationExpression<ASTBuilder>);
                stack.push_back(e);
            }

            // Final reduce to clean-up the stack.
            size_t i = stack.size() - 1;
            expr = stack[i];
            markers.pop_back();
            while (i > 0) {
                MetaNode node = this->startNode(&markers.back());
                expr = this->finalize(node, finishBinaryExpression(builder, stack[i - 1], expr, &tokenStack.back()));
                markers.pop_back();
                tokenStack.pop_back();
                i--;
            }
        }

        return expr;
    }

    template <class ASTBuilder>
    ASTNode finishBinaryExpression(ASTBuilder& builder, ASTNode left, ASTNode right, Scanner::SmallScannerResult* token)
    {
        if (token->type == Token::PunctuatorToken) {
            PunctuatorKind oper = token->valuePunctuatorKind;
            // Additive Operators
            switch (oper) {
            case Plus:
                return builder.createBinaryExpressionPlusNode(left, right);
            case Minus:
                return builder.createBinaryExpressionMinusNode(left, right);
            case LeftShift:
                return builder.createBinaryExpressionLeftShiftNode(left, right);
            case RightShift:
                return builder.createBinaryExpressionSignedRightShiftNode(left, right);
            case UnsignedRightShift:
                return builder.createBinaryExpressionUnsignedRightShiftNode(left, right);
            case Multiply:
                return builder.createBinaryExpressionMultiplyNode(left, right);
            case Divide:
                return builder.createBinaryExpressionDivisionNode(left, right);
            case Mod:
                return builder.createBinaryExpressionModNode(left, right);
            case LeftInequality:
                return builder.createBinaryExpressionLessThanNode(left, right);
            case RightInequality:
                return builder.createBinaryExpressionGreaterThanNode(left, right);
            case LeftInequalityEqual:
                return builder.createBinaryExpressionLessThanOrEqualNode(left, right);
            case RightInequalityEqual:
                return builder.createBinaryExpressionGreaterThanOrEqualNode(left, right);
            case Equal:
                return builder.createBinaryExpressionEqualNode(left, right);
            case NotEqual:
                return builder.createBinaryExpressionNotEqualNode(left, right);
            case StrictEqual:
                return builder.createBinaryExpressionStrictEqualNode(left, right);
            case NotStrictEqual:
                return builder.createBinaryExpressionNotStrictEqualNode(left, right);
            case BitwiseAnd:
                return builder.createBinaryExpressionBitwiseAndNode(left, right);
            case BitwiseXor:
                return builder.createBinaryExpressionBitwiseXorNode(left, right);
            case BitwiseOr:
                return builder.createBinaryExpressionBitwiseOrNode(left, right);
            case LogicalOr:
                return builder.createBinaryExpressionLogicalOrNode(left, right);
            case LogicalAnd:
                return builder.createBinaryExpressionLogicalAndNode(left, right);
            case NullishCoalescing:
                if (left->isLogicalOperation() || right->isLogicalOperation()) {
                    this->throwError(Messages::CannotChainLogicalWithNullish);
                }
                return builder.createBinaryExpressionNullishCoalescingNode(left, right);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ASSERT(token->type == Token::KeywordToken);
            switch (token->valueKeywordKind) {
            case InKeyword:
                return builder.createBinaryExpressionInNode(left, right);
            case KeywordKind::InstanceofKeyword:
                return builder.createBinaryExpressionInstanceOfNode(left, right);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }

    // ECMA-262 12.14 Conditional Operator

    template <class ASTBuilder>
    ASTNode parseConditionalExpression(ASTBuilder& builder)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        ASTNode exprNode = nullptr;

        exprNode = this->inheritCoverGrammar(builder, &Parser::parseBinaryExpression<ASTBuilder>);

        if (this->match(GuessMark)) {
            ASTNode consequent = nullptr;

            this->nextToken();

            bool previousAllowIn = this->context->allowIn;
            this->context->allowIn = true;
            consequent = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            this->context->allowIn = previousAllowIn;

            this->expect(Colon);
            ASTNode alternate = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            exprNode = this->finalize(this->startNode(startToken), builder.createConditionalExpressionNode(exprNode, consequent, alternate));

            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
        }

        return exprNode;
    }

    // ECMA-262 12.15 Assignment Operators

    template <class ASTBuilder, bool checkLeftHasRestrictedWord>
    ASTNode parseAssignmentExpression(ASTBuilder& builder)
    {
        ASTNode exprNode = nullptr;

        if (this->context->allowYield && this->matchKeyword(YieldKeyword)) {
            exprNode = this->parseYieldExpression(builder);
        } else {
            ALLOC_TOKEN(startToken);
            *startToken = this->lookahead;
            ALLOC_TOKEN(token);
            *token = *startToken;
            ASTNodeType type;
            MetaNode startNode = this->createNode();
            Marker startMarker = this->lastMarker;

            bool isAsync = false;
            exprNode = this->parseConditionalExpression(builder);

            if (token->type == Token::IdentifierToken && (token->lineNumber == this->lookahead.lineNumber) && token->relatedSource(this->scanner->source) == "async") {
                if (this->lookahead.type == Token::IdentifierToken || this->matchKeyword(YieldKeyword)) {
                    ASTNode arg = this->parsePrimaryExpression(builder);
                    arg = builder.reinterpretExpressionAsPattern(arg);
                    // We cannot use code below
                    // because SyntaxNode cannot save AST nodes(but we need for original code)
                    // this is original code
                    // ASTNodeList args;
                    // args.append(this->allocator, arg);
                    // exprNode = this->finalize(this->createNode(), builder.createArrowParameterPlaceHolderNode(args, true));
                    isAsync = true;
                    exprNode = arg;
                }
            }

            type = exprNode->type();
            if (type == ArrowParameterPlaceHolder || this->match(Arrow) || isAsync) {
                // ECMA-262 14.2 Arrow Function Definitions
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;

                // try to skip the parsing of arrow function
                if (this->isParsingSingleFunction) {
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();

                    InterpretedCodeBlock* currentTarget = this->codeBlock;
                    size_t orgIndex = this->lookahead.start;

                    InterpretedCodeBlock* childBlock = currentTarget->childBlockAt(this->subCodeBlockIndex);
                    this->scanner->index = childBlock->src().length() + childBlock->functionStart().index - currentTarget->functionStart().index;
                    this->scanner->lineNumber = childBlock->functionStart().line;
                    this->scanner->lineStart = childBlock->functionStart().index - childBlock->functionStart().column;

                    this->lookahead.lineNumber = this->scanner->lineNumber;
                    this->lookahead.lineStart = this->scanner->lineStart;
                    this->nextToken();

                    this->context->firstCoverInitializedNameError.reset();

                    // increase subCodeBlockIndex because parsing of an internal function is skipped
                    this->subCodeBlockIndex++;

                    return this->finalize(this->startNode(startToken), builder.createArrowFunctionExpressionNode(subCodeBlockIndex));
                }

                BEGIN_FUNCTION_SCANNING(AtomicString());

                ParseFormalParametersResult list;
                // FIXME reinterpretAsCoverFormalsList
                if (type == Identifier) {
                    ASSERT(exprNode->isIdentifier());
                    this->validateParam(list, this->lookahead, exprNode->asIdentifier()->name());
                    list.params.push_back(builder.convertToParameterSyntaxNode(exprNode));
                } else {
                    this->scanner->index = startMarker.index;
                    this->scanner->lineNumber = startMarker.lineNumber;
                    this->scanner->lineStart = startMarker.lineStart;
                    this->nextToken();

                    if (type == ArrowParameterPlaceHolder && exprNode->asArrowParameterPlaceHolder()->async()) {
                        this->nextToken();
                        this->currentScopeContext->m_isAsync = true;
                        this->context->await = true;
                    }

                    this->expect(LeftParenthesis);
                    this->currentScopeContext->m_hasArrowParameterPlaceHolder = true;

                    this->parseFormalParameters(newBuilder, list);
                }

                // validity check of ParseFormalParametersResult removed
                if (this->hasLineTerminator) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                this->context->firstCoverInitializedNameError.reset();

                bool previousStrict = this->context->strict;
                bool previousAllowYield = this->context->allowYield;
                bool previousAwait = this->context->await;
                bool previousInArrowFunction = this->context->inArrowFunction;
                bool previousAllowStrictDirective = this->context->allowStrictDirective;

                this->context->allowYield = false;
                this->context->inArrowFunction = true;

                this->currentScopeContext->m_allowSuperCall = this->context->allowSuperCall;
                this->currentScopeContext->m_allowSuperProperty = this->context->allowSuperProperty;
                this->currentScopeContext->m_allowArguments = this->context->allowArguments;

                this->currentScopeContext->m_functionStartLOC.index = startNode.index;
                this->currentScopeContext->m_functionStartLOC.column = startNode.column;
                this->currentScopeContext->m_functionStartLOC.line = startNode.line;

                extractNamesFromFunctionParams(list);

                this->expect(Arrow);

                this->context->allowStrictDirective = !this->currentScopeContext->m_hasParameterOtherThanIdentifier;

                MetaNode node = this->startNode(startToken);
                MetaNode bodyStart = this->createNode();

                if (this->match(LeftBrace)) {
                    this->parseFunctionSourceElements(newBuilder);
                } else {
                    auto oldNameCallback = this->nameDeclaredCallback;
#if defined(ESCARGOT_SMALL_CONFIG)
                    this->isolateCoverGrammar(newBuilder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
#else
                    this->isolateCoverGrammar(newBuilder, &Parser::parseAssignmentExpression<SyntaxChecker, false>);
#endif

                    this->currentScopeContext->m_bodyEndLOC.index = this->lastMarker.index;
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
                    this->currentScopeContext->m_bodyEndLOC.line = this->lastMarker.lineNumber;
                    this->currentScopeContext->m_bodyEndLOC.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif
                }
                this->currentScopeContext->m_lexicalBlockIndexFunctionLocatedIn = this->lexicalBlockIndex;

                if (this->context->strict && list.firstRestricted) {
                    this->throwUnexpectedToken(list.firstRestricted, list.message);
                }
                if (this->context->strict && list.stricted) {
                    this->throwUnexpectedToken(list.stricted, list.message);
                }

                this->currentScopeContext->m_isArrowFunctionExpression = true;
                this->currentScopeContext->m_nodeType = ASTNodeType::ArrowFunctionExpression;
                this->currentScopeContext->m_isGenerator = false;

                END_FUNCTION_SCANNING();
                exprNode = static_cast<ASTNode>(this->finalize(node, builder.createArrowFunctionExpressionNode(subCodeBlockIndex)));

                this->context->strict = previousStrict;
                this->context->allowYield = previousAllowYield;
                this->context->await = previousAwait;
                this->context->inArrowFunction = previousInArrowFunction;
                this->context->allowStrictDirective = previousAllowStrictDirective;
            } else {
                // check if restricted words are used as target in array/object initializer
                if (checkLeftHasRestrictedWord) {
                    if (this->context->strict && type == Identifier) {
                        AtomicString name;
                        name = exprNode->asIdentifier()->name();
                        if (this->scanner->isRestrictedWord(name)) {
                            this->context->hasRestrictedWordInArrayOrObjectInitializer = true;
                        }
                    }
                }
                if (this->matchAssign()) {
                    const bool isLeftAssignmentTarget = this->context->isAssignmentTarget;
                    if (!isLeftAssignmentTarget) {
                        if (type == ArrayExpression || type == ObjectExpression) {
                            this->throwError(Messages::InvalidLHSInAssignment);
                        }
                        auto exprNodeType = exprNode->type();
                        if (UNLIKELY(exprNodeType == ASTNodeType::MemberExpression && exprNode->asMemberExpression()->isOptional())) {
                            this->throwError(Messages::InvalidLHSInAssignment);
                        }
                        if (UNLIKELY(exprNodeType == ASTNodeType::ImportCall)) {
                            this->throwError(Messages::InvalidLHSInAssignment);
                        }
                        if (this->context->strict) {
                            this->throwError(Messages::InvalidLHSInAssignment);
                        }
                    }

                    if (this->context->strict) {
                        if (type == Identifier) {
                            AtomicString name = exprNode->asIdentifier()->name();
                            if (this->scanner->isRestrictedWord(name)) {
                                this->throwUnexpectedToken(*token, Messages::StrictLHSAssignment);
                            }
                            if (this->scanner->isStrictModeReservedWord(name)) {
                                this->throwUnexpectedToken(*token, Messages::StrictReservedWord);
                            }
                        } else if (type == ArrayExpression || type == ObjectExpression) {
                            if (this->context->hasRestrictedWordInArrayOrObjectInitializer) {
                                this->throwError(Messages::StrictLHSAssignment);
                            }
                        }
                    }

                    if (!this->match(Substitution)) {
                        this->context->isAssignmentTarget = false;
                        this->context->isBindingElement = false;
                    } else {
                        exprNode = builder.reinterpretExpressionAsPattern(exprNode);

                        // https://tc39.es/ecma262/#sec-static-semantics-assignmenttargettype
                        auto type = exprNode->type();
                        switch (type) {
                        case ASTNodeType::Identifier:
                        case ASTNodeType::MemberExpression:
                        case ASTNodeType::CallExpression:
                            break;
                        case ASTNodeType::ObjectPattern:
                        case ASTNodeType::ArrayPattern:
                            if (startToken->type == Token::PunctuatorToken
                                && startToken->valuePunctuatorKind == PunctuatorKind::LeftParenthesis) {
                                this->throwError(Messages::InvalidLHSInAssignment);
                            }
                            break;
                        default:
                            this->throwError(Messages::InvalidLHSInAssignment);
                            break;
                        }
                    }

                    this->nextToken(token);
                    ASTNode rightNode = nullptr;
                    ASTNode exprResult = nullptr;

                    rightNode = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);

                    switch (token->valuePunctuatorKind) {
                    case Substitution: {
                        if (type == ASTNodeType::Identifier && startToken->type == Token::IdentifierToken) {
                            this->addImplicitName(rightNode, exprNode->asIdentifier()->name());
                        }
                        exprResult = builder.createAssignmentExpressionSimpleNode(exprNode, rightNode);
                        break;
                    }
                    case PlusEqual:
                        exprResult = builder.createAssignmentExpressionPlusNode(exprNode, rightNode);
                        break;
                    case MinusEqual:
                        exprResult = builder.createAssignmentExpressionMinusNode(exprNode, rightNode);
                        break;
                    case MultiplyEqual:
                        exprResult = builder.createAssignmentExpressionMultiplyNode(exprNode, rightNode);
                        break;
                    case DivideEqual:
                        exprResult = builder.createAssignmentExpressionDivisionNode(exprNode, rightNode);
                        break;
                    case ModEqual:
                        exprResult = builder.createAssignmentExpressionModNode(exprNode, rightNode);
                        break;
                    case LeftShiftEqual:
                        exprResult = builder.createAssignmentExpressionLeftShiftNode(exprNode, rightNode);
                        break;
                    case RightShiftEqual:
                        exprResult = builder.createAssignmentExpressionSignedRightShiftNode(exprNode, rightNode);
                        break;
                    case UnsignedRightShiftEqual:
                        exprResult = builder.createAssignmentExpressionUnsignedRightShiftNode(exprNode, rightNode);
                        break;
                    case BitwiseXorEqual:
                        exprResult = builder.createAssignmentExpressionBitwiseXorNode(exprNode, rightNode);
                        break;
                    case BitwiseAndEqual:
                        exprResult = builder.createAssignmentExpressionBitwiseAndNode(exprNode, rightNode);
                        break;
                    case BitwiseOrEqual:
                        exprResult = builder.createAssignmentExpressionBitwiseOrNode(exprNode, rightNode);
                        break;
                    case ExponentiationEqual:
                        exprResult = builder.createAssignmentExpressionExponentiationNode(exprNode, rightNode);
                        break;

#define CHECK_LOGICAL_ASSIGNMENT()                          \
    if (!isLeftAssignmentTarget) {                          \
        this->throwError(Messages::InvalidLHSInAssignment); \
    }

                    case LogicalAndEqual:
                        CHECK_LOGICAL_ASSIGNMENT()
                        exprResult = builder.createAssignmentExpressionLogicalAndNode(exprNode, rightNode);
                        break;
                    case LogicalOrEqual:
                        CHECK_LOGICAL_ASSIGNMENT()
                        exprResult = builder.createAssignmentExpressionLogicalOrNode(exprNode, rightNode);
                        break;
                    case LogicalNullishEqual:
                        CHECK_LOGICAL_ASSIGNMENT()
                        exprResult = builder.createAssignmentExpressionLogicalNullishNode(exprNode, rightNode);
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }

#undef CHECK_LOGICAL_ASSIGNMENT

                    exprNode = this->finalize(this->startNode(startToken), exprResult);
                    this->context->firstCoverInitializedNameError.reset();
                }
            }
        }

        return exprNode;
    }

    template <class ASTBuilder>
    ASTNode parseAssignmentExpressionAndWrapIntoArrowFunction(ASTBuilder& builder, const AtomicString& paramName, size_t debuggerLineStart)
    {
        UNUSED_VARIABLE(debuggerLineStart);

        ASTNode result;
        if (!this->isParsingSingleFunction) {
            BEGIN_FUNCTION_SCANNING(AtomicString());
            auto startNode = this->createNode();

            if (paramName.string()->length()) {
                // insert optional paramter name
                this->currentScopeContext->m_functionLength = 1;
                this->currentScopeContext->m_parameterCount = 1;
                this->currentScopeContext->m_parameters.resizeWithUninitializedValues(1);
                this->currentScopeContext->m_parameters[0] = paramName;
                this->currentScopeContext->insertVarName(paramName, 0, true, true, true);
            }

            this->currentScopeContext->m_functionStartLOC.index = startNode.index;
            this->currentScopeContext->m_functionStartLOC.column = startNode.column;
            this->currentScopeContext->m_functionStartLOC.line = startNode.line;
            this->currentScopeContext->m_allowSuperCall = this->context->allowSuperCall;
            this->currentScopeContext->m_allowSuperProperty = this->context->allowSuperProperty;
            this->currentScopeContext->m_allowArguments = this->context->allowArguments;
#ifdef ESCARGOT_DEBUGGER
            this->currentScopeContext->m_needRareData = true;
            this->currentScopeContext->m_debuggerLineStart = debuggerLineStart;
#endif /* ESCARGOT_DEBUGGER */

            this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            auto lastContext = this->lastPoppedScopeContext;

            this->currentScopeContext->m_isArrowFunctionExpression = true;
            this->currentScopeContext->m_isOneExpressionOnlyVirtualArrowFunctionExpression = true;
            this->currentScopeContext->m_nodeType = ASTNodeType::ArrowFunctionExpression;

            this->currentScopeContext->m_bodyEndLOC.index = this->lastMarker.index;
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
            this->currentScopeContext->m_bodyEndLOC.line = this->lastMarker.lineNumber;
            this->currentScopeContext->m_bodyEndLOC.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif
            END_FUNCTION_SCANNING();

            this->lastPoppedScopeContext = lastContext;

            // wrap right expression with arrow function
            result = this->finalize(startNode, builder.createArrowFunctionExpressionNode(subCodeBlockIndex));
        } else {
            auto startNode = this->createNode();
            InterpretedCodeBlock* currentTarget = this->codeBlock;
            size_t orgIndex = this->lookahead.start;

            InterpretedCodeBlock* childBlock = currentTarget->childBlockAt(this->subCodeBlockIndex);
            this->scanner->index = childBlock->src().length() + childBlock->functionStart().index - currentTarget->functionStart().index;
            this->scanner->lineNumber = childBlock->functionStart().line;
            this->scanner->lineStart = childBlock->functionStart().index - childBlock->functionStart().column;

            this->lookahead.lineNumber = this->scanner->lineNumber;
            this->lookahead.lineStart = this->scanner->lineStart;
            this->nextToken();

            // increase subCodeBlockIndex because parsing of an internal function is skipped
            this->subCodeBlockIndex++;

            result = this->finalize(startNode, builder.createArrowFunctionExpressionNode(subCodeBlockIndex));
        }

        return result;
    }

    // ECMA-262 12.16 Comma Operator

    template <class ASTBuilder>
    ASTNode parseExpression(ASTBuilder& builder)
    {
        ALLOC_TOKEN(startToken);
        *startToken = this->lookahead;
        ASTNode exprNode = nullptr;

        exprNode = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);

        if (this->match(Comma)) {
            ASTNodeList expressions;
            expressions.append(this->allocator, exprNode);
            while (this->startMarker.index < this->scanner->length) {
                if (!this->match(Comma)) {
                    break;
                }
                this->nextToken();
                expressions.append(this->allocator, this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>));
            }

            exprNode = this->finalize(this->startNode(startToken), builder.createSequenceExpressionNode(expressions));
        }

        return exprNode;
    }

    // ECMA-262 13.2 Block

    template <class ASTBuilder>
    ASTNode parseStatementListItem(ASTBuilder& builder)
    {
        ASTNode statement = nullptr;
        this->context->isAssignmentTarget = true;
        this->context->isBindingElement = true;
        this->context->hasRestrictedWordInArrayOrObjectInitializer = false;
        if (this->lookahead.type == KeywordToken) {
            switch (this->lookahead.valueKeywordKind) {
            case FunctionKeyword:
                statement = this->parseFunctionDeclaration(builder);
                break;
            case ExportKeyword:
                if (this->sourceType != Module) {
                    this->throwUnexpectedToken(this->lookahead, Messages::IllegalExportDeclaration);
                }
                statement = this->parseExportDeclaration(builder);
                break;
            case ImportKeyword:
                statement = this->parseImportDeclaration(builder);
                break;
            case ConstKeyword:
                statement = this->parseVariableStatement(builder, KeywordKind::ConstKeyword);
                break;
            case LetKeyword:
                // The `let` contextual keyword must not contain Unicode escape sequences
                statement = (this->isLexicalDeclaration() && (this->lookahead.end - this->lookahead.start == 3)) ? this->parseLexicalDeclaration(builder, false) : this->parseStatement(builder);
                break;
            case ClassKeyword:
                statement = this->parseClassDeclaration(builder);
                break;
            default:
                statement = this->parseStatement(builder);
                break;
            }
        } else {
            statement = this->parseStatement(builder);
        }

        return statement;
    }

    struct ParserBlockContext {
        size_t lexicalBlockCountBefore;
        size_t lexicalBlockIndexBefore;
        size_t childLexicalBlockIndex;

        ASTBlockContext* oldBlockScopeContext;

        ParserBlockContext()
            : lexicalBlockCountBefore(SIZE_MAX)
            , lexicalBlockIndexBefore(SIZE_MAX)
            , childLexicalBlockIndex(SIZE_MAX)
            , oldBlockScopeContext(nullptr)
        {
        }
    };

    void openBlock(ParserBlockContext& ctx)
    {
        if (UNLIKELY(this->lexicalBlockCount == LEXICAL_BLOCK_INDEX_MAX - 1)) {
            this->throwError("too many lexical blocks in script", String::emptyString, String::emptyString, ErrorObject::RangeError);
        }

        this->lastUsingName = AtomicString();
        this->lexicalBlockCount++;
        ctx.lexicalBlockCountBefore = this->lexicalBlockCount;
        ctx.lexicalBlockIndexBefore = this->lexicalBlockIndex;
        ctx.childLexicalBlockIndex = this->lexicalBlockCount;
        ctx.oldBlockScopeContext = this->currentBlockContext;

        this->currentBlockContext = this->currentScopeContext->insertBlockScope(this->allocator, ctx.childLexicalBlockIndex, this->lexicalBlockIndex,
                                                                                ExtendedNodeLOC(this->lastMarker.lineNumber, this->lastMarker.index - this->lastMarker.lineStart + 1, this->lastMarker.index));
        this->lexicalBlockIndex = ctx.childLexicalBlockIndex;
    }

    void closeBlock(ParserBlockContext& ctx)
    {
        if (this->isParsingSingleFunction) {
            bool finded = false;
            for (size_t i = 0; i < this->codeBlock->blockInfos().size(); i++) {
                if (this->codeBlock->blockInfos()[i]->m_blockIndex == ctx.childLexicalBlockIndex) {
                    finded = true;
                    break;
                }
            }
            if (!finded) {
                ctx.childLexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
            }
        } else {
            // if there is no new variable in this block, merge this block into parent block
            auto currentFunctionScope = this->currentScopeContext;
            auto blockContext = this->currentBlockContext;

            auto essentialBlockMax = this->currentScopeContext->m_functionBodyBlockIndex;

            if (this->lexicalBlockIndex > essentialBlockMax && blockContext->m_names.size() == 0) {
                const auto currentBlockIndex = this->lexicalBlockIndex;
                LexicalBlockIndex parentBlockIndex = blockContext->m_parentBlockIndex;

                if (!blockContext->m_funcDeclNames.size()) { // if there is no function declaration, block collapse can start
                    for (size_t i = 0; i < currentFunctionScope->m_childBlockScopes.size(); i++) {
                        if (currentFunctionScope->m_childBlockScopes[i]->m_parentBlockIndex == currentBlockIndex) {
                            currentFunctionScope->m_childBlockScopes[i]->m_parentBlockIndex = parentBlockIndex;
                        }
                    }

                    ASTScopeContext* child = currentFunctionScope->m_firstChild;
                    while (child) {
                        if (child->m_lexicalBlockIndexFunctionLocatedIn == currentBlockIndex) {
                            child->m_lexicalBlockIndexFunctionLocatedIn = parentBlockIndex;
                        }
                        child = child->nextSibling();
                    }

                    ASSERT(currentFunctionScope->findBlockFromBackward(parentBlockIndex) == ctx.oldBlockScopeContext);
                    auto parentBlockContext = ctx.oldBlockScopeContext;
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
        this->currentBlockContext = ctx.oldBlockScopeContext;
        this->lexicalBlockIndex = ctx.lexicalBlockIndexBefore;
        this->lastUsingName = AtomicString();
    }

    template <class ASTBuilder>
    ASTNode parseBlock(ASTBuilder& builder)
    {
        this->nextToken();
        ASTNode referNode = nullptr;

        ParserBlockContext blockContext;
        openBlock(blockContext);

        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = true;

        ASTStatementContainer block = builder.createStatementContainer();
        while (true) {
            if (this->match(RightBrace)) {
                break;
            }
            referNode = block->appendChild(this->parseStatementListItem(builder), referNode);
        }
        this->expect(RightBrace);

        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        closeBlock(blockContext);

        MetaNode node = this->createNode();
        return this->finalize(node, builder.createBlockStatementNode(block, blockContext.childLexicalBlockIndex));
    }

    // ECMA-262 13.3.1 Let and Const Declarations

    template <class ASTBuilder>
    ASTNode parseLexicalBinding(ASTBuilder& builder, KeywordKind kind, bool inFor)
    {
        auto node = this->createNode();
        SmallScannerResultVector params;
        ASTNode idNode = nullptr;
        bool isIdentifier;
        AtomicString name;

        idNode = this->parsePattern(builder, params, kind, true);
        isIdentifier = (idNode->type() == Identifier);
        if (isIdentifier) {
            name = idNode->asIdentifier()->name();
        }

        // ECMA-262 12.2.1
        if (this->context->strict && isIdentifier && this->scanner->isRestrictedWord(name)) {
            this->throwError(Messages::StrictVarName);
        }

        ASTNode init = nullptr;
        if (kind == KeywordKind::ConstKeyword) {
            if (!this->matchKeyword(KeywordKind::InKeyword) && !this->matchContextualKeyword("of")) {
                this->expect(Substitution);
                init = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            }
        } else if ((!inFor && !isIdentifier) || this->match(Substitution)) {
            this->expect(Substitution);
            init = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
        }

        if (init && isIdentifier) {
            this->addImplicitName(init, name);
        }

        ASTNode variableNode = this->finalize(node, builder.createVariableDeclaratorNode(kind, idNode, init));
        return variableNode;
    }

    template <class ASTBuilder>
    ASTNodeList parseBindingList(ASTBuilder& builder, KeywordKind kind, bool inFor)
    {
        ASTNodeList list;
        list.append(this->allocator, this->parseLexicalBinding(builder, kind, inFor));
        while (this->match(Comma)) {
            this->nextToken();
            list.append(this->allocator, this->parseLexicalBinding(builder, kind, inFor));
        }
        return list;
    }

    template <class ASTBuilder>
    ASTNode parseLexicalDeclaration(ASTBuilder& builder, bool inFor)
    {
        auto node = this->createNode();
        ALLOC_TOKEN(token);
        this->nextToken(token);
        auto kind = token->valueKeywordKind;

        ASTNodeList declarations;
        declarations = this->parseBindingList(builder, kind, inFor);

        this->consumeSemicolon();

        return this->finalize(node, builder.createVariableDeclarationNode(declarations, kind));
    }

    bool isLexicalDeclaration()
    {
        ScanState state = this->scanner->saveState();
        this->collectComments();
        ALLOC_TOKEN(next);
        this->scanner->lex(next);
        this->scanner->restoreState(state);

        return (next->type == Token::IdentifierToken) || (next->type == Token::PunctuatorToken && next->valuePunctuatorKind == PunctuatorKind::LeftSquareBracket) || (next->type == Token::PunctuatorToken && next->valuePunctuatorKind == PunctuatorKind::LeftBrace) || (next->type == Token::KeywordToken && next->valueKeywordKind == KeywordKind::LetKeyword) || (next->type == Token::KeywordToken && next->valueKeywordKind == KeywordKind::YieldKeyword);
    }

    // ECMA-262 13.3.2 Variable Statement
    template <class ASTBuilder>
    ASTNode parseVariableIdentifier(ASTBuilder& builder, KeywordKind kind = KeywordKindEnd, bool isExplicitVariableDeclaration = false)
    {
        MetaNode node = this->createNode();

        ALLOC_TOKEN(token);
        this->nextToken(token);
        if (token->type == Token::KeywordToken && token->valueKeywordKind == YieldKeyword) {
            if (this->context->strict) {
                this->throwUnexpectedToken(*token, Messages::StrictReservedWord);
            }
            if (this->context->allowYield) {
                this->throwUnexpectedToken(*token);
            }
        } else if (token->type != Token::IdentifierToken) {
            if (this->context->strict && token->type == Token::KeywordToken && this->scanner->isStrictModeReservedWord(token->relatedSource(this->scanner->source))) {
                this->throwUnexpectedToken(*token, Messages::StrictReservedWord);
            } else {
                if (this->context->strict || token->relatedSource(this->scanner->source) != "let" || (kind != VarKeyword)) {
                    this->throwUnexpectedToken(*token);
                }
            }
        } else if ((this->sourceType == Module || this->currentScopeContext->m_isAsync) && token->type == Token::IdentifierToken && token->relatedSource(this->scanner->source) == "await") {
            this->throwUnexpectedToken(*token);
        } else if (UNLIKELY(this->context->strict && token->type == Token::IdentifierToken
                            && token->hasAllocatedString && this->scanner->isStrictModeReservedWord(token->valueStringLiteral(this->scanner)))) {
            this->throwUnexpectedToken(*token, Messages::StrictReservedWord);
        }

        ASTNode id;
        if (kind == KeywordKind::VarKeyword || kind == KeywordKind::LetKeyword || kind == KeywordKind::ConstKeyword) {
            TrackUsingNameBlocker blocker(this);
            id = finishIdentifier(builder, token);

            AtomicString declName = id->asIdentifier()->name();

            addDeclaredNameIntoContext(declName, this->lexicalBlockIndex, kind, isExplicitVariableDeclaration);
            if (UNLIKELY(declName == stringArguments && !this->isParsingSingleFunction)) {
                this->insertUsingName(stringArguments);
            }
        } else if (this->context->inParameterNameParsing) {
            TrackUsingNameBlocker blocker(this);
            id = finishIdentifier(builder, token);
        } else {
            id = finishIdentifier(builder, token);
        }

        return this->finalize(node, id);
    }

    void addDeclaredNameIntoContext(AtomicString name, LexicalBlockIndex blockIndex, KeywordKind kind, bool isExplicitVariableDeclaration = false)
    {
        ASSERT(kind == VarKeyword || kind == LetKeyword || kind == ConstKeyword);

        if (!this->isParsingSingleFunction) {
            /*
               we need this bunch of code for tolerate this error(we consider variable 'e' as lexically declared)
               try { } catch(e) { var e; }
            */
            if (this->context->inCatchClause && kind == KeywordKind::VarKeyword) {
                for (size_t i = 0; i < this->context->catchClauseSimplyDeclaredVariableNames.size(); i++) {
                    if (this->context->catchClauseSimplyDeclaredVariableNames[i].first == name) {
                        if (isExplicitVariableDeclaration) {
                            this->currentScopeContext->insertVarName(name, blockIndex, true, kind == VarKeyword);
                            return;
                        }
                    }
                }
            }
            /* code end */

            if (!this->currentScopeContext->canDeclareName(name, blockIndex, kind == VarKeyword, isExplicitVariableDeclaration)) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Identifier"), name.string());
            }
            if (kind == VarKeyword) {
                this->currentScopeContext->insertVarName(name, blockIndex, true, true);
            } else {
                this->currentScopeContext->insertNameAtBlock(name, blockIndex, kind == ConstKeyword);
            }
        }

        if (nameDeclaredCallback) {
            nameDeclaredCallback(name, blockIndex, kind, isExplicitVariableDeclaration);
        }
    }

    template <class ASTBuilder>
    ASTNode parseVariableDeclaration(ASTBuilder& builder, DeclarationOptions& options, bool& hasInit, ASTNodeType& leftSideType)
    {
        SmallScannerResultVector params;
        ASTNode idNode = nullptr;
        bool isIdentifier;
        AtomicString name;
        MetaNode node = this->createNode();

        idNode = this->parsePattern(builder, params, options.kind, true);
        leftSideType = idNode->type();
        isIdentifier = (leftSideType == Identifier);
        if (isIdentifier) {
            name = idNode->asIdentifier()->name();
        }

        // ECMA-262 12.2.1
        if (this->context->strict && isIdentifier && this->scanner->isRestrictedWord(name)) {
            this->throwError(Messages::StrictVarName);
        }

        if (options.kind != VarKeyword && !this->context->allowLexicalDeclaration) {
            this->throwError("Lexical declaration cannot appear in a single-statement context");
        }

        ASTNode initNode = nullptr;
        hasInit = false;
        if (this->match(Substitution)) {
            hasInit = true;
            this->nextToken();
            ASTNodeType type = ASTNodeType::ASTNodeTypeError;
            initNode = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
            if (isIdentifier) {
                this->addImplicitName(initNode, name);
            }
        } else if (!isIdentifier && !options.inFor) {
            this->expect(Substitution);
        }

        if (options.kind == ConstKeyword && !options.inFor) {
            if (!initNode) {
                this->throwError("Missing initializer in const identifier '%s' declaration", name.string());
            }
        }

        return this->finalize(node, builder.createVariableDeclaratorNode(options.kind, idNode, initNode));
    }

    template <class ASTBuilder>
    std::tuple<bool, bool, ASTNodeList> parseVariableDeclarationList(ASTBuilder& builder, DeclarationOptions& options)
    {
        DeclarationOptions opt;
        opt.inFor = options.inFor;
        opt.kind = options.kind;

        ASTNodeList list;
        bool hasInit;
        ASTNodeType leftSideType;
        list.append(this->allocator, this->parseVariableDeclaration(builder, opt, hasInit, leftSideType));
        while (this->match(Comma)) {
            this->nextToken();
            bool ignored;
            ASTNodeType ignored2;
            list.append(this->allocator, this->parseVariableDeclaration(builder, opt, ignored, ignored2));
        }

        bool leftIsArrayOrObjectPattern = (leftSideType == ArrayPattern || leftSideType == ObjectPattern);
        return std::make_tuple(hasInit, leftIsArrayOrObjectPattern, list);
    }

    template <class ASTBuilder>
    ASTNode parseVariableStatement(ASTBuilder& builder, KeywordKind kind = VarKeyword)
    {
        this->expectKeyword(kind);
        MetaNode node = this->createNode();
        DeclarationOptions opt;
        opt.inFor = false;
        opt.kind = kind;
        auto declarations = this->parseVariableDeclarationList(builder, opt);
        ASTNodeList declarationList = std::get<2>(declarations);
        this->consumeSemicolon();

        return this->finalize(node, builder.createVariableDeclarationNode(declarationList, kind));
    }

    // ECMA-262 13.4 Empty Statement

    template <class ASTBuilder>
    ASTNode parseEmptyStatement(ASTBuilder& builder)
    {
        this->expect(SemiColon);

        MetaNode node = this->createNode();
        return this->finalize(node, builder.createEmptyStatementNode());
    }

    // ECMA-262 13.5 Expression Statement

    template <class ASTBuilder>
    ASTNode parseExpressionStatement(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        ASTNode expr = this->parseExpression(builder);
        this->consumeSemicolon();
        return this->finalize(node, builder.createExpressionStatementNode(expr));
    }

    // ECMA-262 13.6 If statement
    template <class ASTBuilder>
    ASTNode parseIfStatement(ASTBuilder& builder)
    {
        ASTNode test = nullptr;
        ASTNode consequent = nullptr;
        ASTNode alternate = nullptr;

        this->expectKeyword(IfKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        test = this->parseExpression(builder);

        this->expect(RightParenthesis);

        bool allowFunctionDeclaration = !this->context->strict;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;

        if (allowFunctionDeclaration && this->matchKeyword(KeywordKind::FunctionKeyword)) {
            // In non-strict mode, functions can declared as the body of an if statement
            // https://tc39.es/ecma262/#sec-labelled-function-declarations
            consequent = this->parseSingleFunctionDeclarationAsBlockStatement(builder);
        } else {
            consequent = this->parseStatement(builder, false, true);
        }

        this->context->allowLexicalDeclaration = false;
        if (this->matchKeyword(ElseKeyword)) {
            this->nextToken();
            if (allowFunctionDeclaration && this->matchKeyword(KeywordKind::FunctionKeyword)) {
                // In non-strict mode, functions can declared as the body of an else statement
                // https://tc39.es/ecma262/#sec-labelled-function-declarations
                alternate = this->parseSingleFunctionDeclarationAsBlockStatement(builder);
            } else {
                alternate = this->parseStatement(builder, false, true);
            }
        }

        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        return this->finalize(node, builder.createIfStatementNode(test, consequent, alternate));
    }

    // ECMA-262 13.7.2 The do-while Statement

    template <class ASTBuilder>
    ASTNode parseDoWhileStatement(ASTBuilder& builder)
    {
        this->expectKeyword(DoKeyword);
        MetaNode node = this->createNode();

        bool previousInIteration = this->context->inIteration;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inIteration = true;

        ASTNode body = this->parseStatement(builder, false, true);
        this->context->inIteration = previousInIteration;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        this->expectKeyword(WhileKeyword);
        this->expect(LeftParenthesis);
        ASTNode test = this->parseExpression(builder);

        this->expect(RightParenthesis);
        if (this->match(SemiColon)) {
            this->nextToken();
        }

        return this->finalize(node, builder.createDoWhileStatementNode(test, body));
    }

    // ECMA-262 13.7.3 The while Statement

    template <class ASTBuilder>
    ASTNode parseWhileStatement(ASTBuilder& builder)
    {
        bool prevInLoop = this->context->inLoop;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inLoop = true;

        this->expectKeyword(WhileKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        ASTNode test = this->parseExpression(builder);
        this->expect(RightParenthesis);

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        ASTNode body = this->parseStatement(builder, false, true);
        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        return this->finalize(node, builder.createWhileStatementNode(test, body));
    }

    // ECMA-262 13.7.4 The for Statement
    // ECMA-262 13.7.5 The for-in and for-of Statements

    enum ForStatementType {
        statementTypeFor,
        statementTypeForIn,
        statementTypeForOf
    };

    template <class ASTBuilder>
    ASTNode parseForStatement(ASTBuilder& builder)
    {
        ASTNode init = nullptr;
        ASTNode test = nullptr;
        ASTNode update = nullptr;
        ASTNode left = nullptr;
        ASTNode right = nullptr;
        ForStatementType type = statementTypeFor;
        bool prevInLoop = this->context->inLoop;
        bool isLexicalDeclaration = false;
        bool seenAwait = false;

        this->expectKeyword(ForKeyword);
        MetaNode node = this->createNode();

        if (UNLIKELY(this->context->await || (this->sourceType == Module && inGlobalSourceCodeParsing()))) {
            seenAwait = this->matchContextualKeyword("await");
            if (seenAwait) {
                this->nextToken();
            }
        }

        this->expect(LeftParenthesis);

        ParserBlockContext headBlockContext;
        openBlock(headBlockContext);

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

                auto declarations = this->parseVariableDeclarationList(builder, opt);
                ASTNodeList declarationList = std::get<2>(declarations);
                this->context->allowIn = previousAllowIn;

                if (declarationList.size() == 1 && this->matchKeyword(InKeyword)) {
                    if (std::get<0>(declarations) && (std::get<1>(declarations) || this->context->strict)) {
                        this->throwError(Messages::ForInOfLoopInitializer, new ASCIIString("for-in"));
                    }
                    if (seenAwait) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    left = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, VarKeyword));
                    this->nextToken();
                    type = statementTypeForIn;
                } else if (declarationList.size() == 1 && !std::get<0>(declarations) && this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource(this->scanner->source) == "of") {
                    left = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, VarKeyword));
                    this->nextToken();
                    type = statementTypeForOf;
                } else {
                    if (seenAwait) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    init = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, VarKeyword));
                    this->expect(SemiColon);
                }
            } else if (this->matchKeyword(ConstKeyword) || this->matchKeyword(LetKeyword)) {
                Marker letContstMarker = this->lastMarker;
                Scanner::ScannerResult keywordToken = this->lookahead;
                KeywordKind kind = keywordToken.valueKeywordKind;
                this->nextToken();

                // for (let = 1;...)
                if (!this->context->strict && kind == LetKeyword && !this->matchKeyword(InKeyword) && !this->matchKeyword(YieldKeyword) && this->lookahead.type != Token::IdentifierToken
                    && !this->match(PunctuatorKind::LeftSquareBracket) && !this->match(PunctuatorKind::LeftBrace)) {
                    this->scanner->index = letContstMarker.index;
                    this->scanner->lineNumber = letContstMarker.lineNumber;
                    this->scanner->lineStart = letContstMarker.lineStart;
                    this->nextToken();

                    bool previousAllowIn = this->context->allowIn;
                    this->context->allowIn = false;
                    ASTNodeType initNodeType;
                    init = this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                    initNodeType = init->type();
                    this->context->allowIn = previousAllowIn;

                    if (this->match(Comma)) {
                        ASTNodeList initSeq;
                        initSeq.append(this->allocator, init);
                        while (this->match(Comma)) {
                            this->nextToken();
                            initSeq.append(this->allocator, this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>));
                        }
                        init = this->finalize(this->startNode(&keywordToken), builder.createSequenceExpressionNode(initSeq));
                    }
                    this->expect(SemiColon);
                } else {
                    const bool previousAllowLexicalDeclaration = this->context->allowLexicalDeclaration;
                    this->context->allowLexicalDeclaration = true;

                    if (!this->context->strict && this->matchKeyword(InKeyword)) {
                        if (seenAwait) {
                            this->throwUnexpectedToken(this->lookahead);
                        }
                        this->nextToken();
                        left = this->finalize(this->createNode(), builder.createIdentifierNode(AtomicString(this->escargotContext, keywordToken.relatedSource(this->scanner->sourceAsNormalView))));
                        init = nullptr;
                        type = statementTypeForIn;
                    } else {
                        isLexicalDeclaration = true;
                        const bool previousAllowIn = this->context->allowIn;
                        this->context->allowIn = false;

                        DeclarationOptions opt;
                        opt.inFor = true;
                        opt.kind = kind;

                        auto declarations = this->parseVariableDeclarationList(builder, opt);
                        ASTNodeList declarationList = std::get<2>(declarations);
                        this->context->allowIn = previousAllowIn;

                        if (declarationList.size() == 1 && !std::get<0>(declarations) && this->matchKeyword(InKeyword)) {
                            if (seenAwait) {
                                this->throwUnexpectedToken(this->lookahead);
                            }
                            left = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, kind));
                            this->nextToken();
                            type = statementTypeForIn;
                        } else if (declarationList.size() == 1 && !std::get<0>(declarations) && this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource(this->scanner->source) == "of") {
                            left = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, kind));
                            this->nextToken();
                            type = statementTypeForOf;
                        } else {
                            if (seenAwait) {
                                this->throwUnexpectedToken(this->lookahead);
                            }
                            init = this->finalize(this->createNode(), builder.createVariableDeclarationNode(declarationList, kind));
                            this->expect(SemiColon);
                        }
                    }
                    this->context->allowLexicalDeclaration = previousAllowLexicalDeclaration;
                }

            } else {
                ALLOC_TOKEN(initStartToken);
                *initStartToken = this->lookahead;
                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                ASTNodeType initNodeType;
                init = this->inheritCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>);
                initNodeType = init->type();
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(InKeyword)) {
                    if (UNLIKELY(initNodeType == ASTNodeType::Literal || (initNodeType >= ASTNodeType::AssignmentExpression && initNodeType <= ASTNodeType::AssignmentExpressionSimple) || initNodeType == ASTNodeType::ThisExpression || initNodeType == ASTNodeType::MetaProperty)) {
                        this->throwError(Messages::InvalidLHSInForIn);
                    }
                    if (seenAwait) {
                        this->throwUnexpectedToken(this->lookahead);
                    }

                    this->nextToken();
                    init = builder.reinterpretExpressionAsPattern(init);
                    left = init;
                    init = nullptr;
                    type = statementTypeForIn;
                } else if (this->lookahead.type == Token::IdentifierToken && this->lookahead.relatedSource(this->scanner->source) == "of") {
                    if (!this->context->isAssignmentTarget || (initNodeType >= ASTNodeType::AssignmentExpression && initNodeType <= ASTNodeType::AssignmentExpressionSimple)) {
                        this->throwError(Messages::InvalidLHSInForLoop);
                    }

                    this->nextToken();
                    init = builder.reinterpretExpressionAsPattern(init);
                    left = init;
                    init = nullptr;
                    type = statementTypeForOf;
                } else {
                    if (seenAwait) {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                    if (this->match(Comma)) {
                        ASTNodeList initSeq;
                        initSeq.append(this->allocator, init);
                        while (this->match(Comma)) {
                            this->nextToken();
                            initSeq.append(this->allocator, this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<ASTBuilder, false>));
                        }
                        init = this->finalize(this->startNode(initStartToken), builder.createSequenceExpressionNode(initSeq));
                    }
                    this->expect(SemiColon);
                }
            }
            this->context->firstCoverInitializedNameError.reset();
        }

        ParserBlockContext headRightBlockContext;
        ParserBlockContext iterationBlockContext;

        if (type == statementTypeFor) {
            openBlock(iterationBlockContext);
            this->context->inLoop = true;
            if (!this->match(SemiColon)) {
                test = this->parseExpression(builder);
            }
            this->expect(SemiColon);
            if (!this->match(RightParenthesis)) {
                update = this->parseExpression(builder);
            }
        } else {
            ASSERT(left);
            if (isLexicalDeclaration) {
                auto oldBlockContext = this->currentBlockContext;
                openBlock(headRightBlockContext);
                this->currentBlockContext->m_names = oldBlockContext->m_names;
            }
            if (type == statementTypeForIn) {
                right = this->parseExpression(builder);
            } else {
                right = this->parseAssignmentExpression<ASTBuilder, false>(builder);
            }
            if (isLexicalDeclaration) {
                closeBlock(headRightBlockContext);
            }
            openBlock(iterationBlockContext);
        }

        this->expect(RightParenthesis);

        ASTBlockContext* headBlockContextInstance = this->currentScopeContext->findBlockFromBackward(headBlockContext.childLexicalBlockIndex);
        if (headBlockContextInstance->m_names.size()) {
            // if there are names on headContext (let, const)
            // we should copy names into to iterationBlock
            this->currentScopeContext->findBlockFromBackward(iterationBlockContext.childLexicalBlockIndex)->m_names = headBlockContextInstance->m_names;
        }

        bool previousInIteration = this->context->inIteration;
        bool allowLexicalDeclarationBefore = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = false;
        this->context->inIteration = true;
        ASTNode body = nullptr;

        body = this->isolateCoverGrammar(builder, &Parser::parseStatement<ASTBuilder>, false, true);

        this->context->inIteration = previousInIteration;
        this->context->inLoop = prevInLoop;
        this->context->allowLexicalDeclaration = allowLexicalDeclarationBefore;

        closeBlock(iterationBlockContext);
        closeBlock(headBlockContext);

        if (type == statementTypeFor) {
            ASTNode forNode = builder.createForStatementNode(init, test, update, body, isLexicalDeclaration, headBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
            return this->finalize(node, forNode);
        }

        if (type == statementTypeForIn) {
            ASSERT(!seenAwait);
            ASTNode forInNode = builder.createForInOfStatementNode(left, right, body, true, isLexicalDeclaration, false, headBlockContext.childLexicalBlockIndex, headRightBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
            return this->finalize(node, forInNode);
        }

        ASSERT(type == statementTypeForOf);
        ASTNode forOfNode = builder.createForInOfStatementNode(left, right, body, false, isLexicalDeclaration, seenAwait, headBlockContext.childLexicalBlockIndex, headRightBlockContext.childLexicalBlockIndex, iterationBlockContext.childLexicalBlockIndex);
        return this->finalize(node, forOfNode);
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

    template <class ASTBuilder>
    ASTNode parseContinueStatement(ASTBuilder& builder)
    {
        this->expectKeyword(ContinueKeyword);

        AtomicString labelString;
        if (this->lookahead.type == IdentifierToken && !this->hasLineTerminator) {
            TrackUsingNameBlocker blocker(this);
            ASTNode labelNode = this->parseVariableIdentifier(builder);
            labelString = labelNode->asIdentifier()->name();

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

        MetaNode node = this->createNode();
        if (labelString.string()->length() != 0) {
            return this->finalize(node, builder.createContinueLabelStatementNode(labelString.string()));
        } else {
            return this->finalize(node, builder.createContinueStatementNode());
        }
    }

    // ECMA-262 13.9 The break statement

    template <class ASTBuilder>
    ASTNode parseBreakStatement(ASTBuilder& builder)
    {
        this->expectKeyword(BreakKeyword);

        AtomicString labelString;
        if (this->lookahead.type == IdentifierToken && !this->hasLineTerminator) {
            TrackUsingNameBlocker blocker(this);
            ASTNode labelNode = this->parseVariableIdentifier(builder);
            labelString = labelNode->asIdentifier()->name();

            if (!hasLabel(labelString)) {
                this->throwError(Messages::UnknownLabel, labelString.string());
            }
        } else if (!this->context->inIteration && !this->context->inSwitch) {
            this->throwError(Messages::IllegalBreak);
        }

        this->consumeSemicolon();

        MetaNode node = this->createNode();
        if (labelString.string()->length() != 0) {
            return this->finalize(node, builder.createBreakLabelStatementNode(labelString.string()));
        } else {
            return this->finalize(node, builder.createBreakStatementNode());
        }
    }

    // ECMA-262 13.10 The return statement

    template <class ASTBuilder>
    ASTNode parseReturnStatement(ASTBuilder& builder)
    {
        if (!this->context->inFunctionBody) {
            this->throwError(Messages::IllegalReturn);
        }

        MetaNode node = this->createNode();
        this->expectKeyword(ReturnKeyword);

        bool hasArgument = !this->match(SemiColon) && !this->match(RightBrace) && !this->hasLineTerminator && this->lookahead.type != EOFToken;
        ASTNode argument = nullptr;
        if (hasArgument) {
            argument = this->parseExpression(builder);
        }
        this->consumeSemicolon();

        return this->finalize(node, builder.createReturnStatementNode(argument));
    }

    // ECMA-262 13.11 The with statement

    template <class ASTBuilder>
    ASTNode parseWithStatement(ASTBuilder& builder)
    {
        if (this->context->strict) {
            this->throwError(Messages::StrictModeWith);
        }

        this->expectKeyword(WithKeyword);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        ASTNode object = this->parseExpression(builder);
        this->expect(RightParenthesis);

        this->currentScopeContext->m_hasWith = true;

        bool prevInWith = this->context->inWith;
        this->context->inWith = true;

        ASTNode body = this->parseStatement(builder, false, true);
        this->context->inWith = prevInWith;

        return this->finalize(node, builder.createWithStatementNode(object, body));
    }

    // ECMA-262 13.12 The switch statement

    template <class ASTBuilder>
    ASTNode parseSwitchCase(ASTBuilder& builder, bool& isDefaultNode)
    {
        MetaNode node = this->createNode();

        ASTNode test = nullptr;
        if (this->matchKeyword(DefaultKeyword)) {
            node = this->createNode();
            this->nextToken();
            isDefaultNode = true;
        } else {
            this->expectKeyword(CaseKeyword);
            node = this->createNode();
            test = this->parseExpression(builder);
            isDefaultNode = false;
        }
        this->expect(Colon);

        ASTStatementContainer consequent = builder.createStatementContainer();
        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(DefaultKeyword) || this->matchKeyword(CaseKeyword)) {
                break;
            }
            consequent->appendChild(this->parseStatementListItem(builder));
        }

        return this->finalize(node, builder.createSwitchCaseNode(test, consequent));
    }

    template <class ASTBuilder>
    ASTNode parseSwitchStatement(ASTBuilder& builder)
    {
        this->expectKeyword(SwitchKeyword);

        this->expect(LeftParenthesis);
        ASTNode discriminant = this->parseExpression(builder);
        this->expect(RightParenthesis);

        ParserBlockContext blockContext;
        openBlock(blockContext);
        this->currentBlockContext->m_nodeType = ASTNodeType::SwitchStatement;
        bool oldAllowLexicalDeclaration = this->context->allowLexicalDeclaration;
        this->context->allowLexicalDeclaration = true;

        bool previousInSwitch = this->context->inSwitch;
        this->context->inSwitch = true;

        ASTStatementContainer casesA, casesB;
        ASTNode deflt = nullptr;
        casesA = builder.createStatementContainer();
        casesB = builder.createStatementContainer();

        bool isDefaultNode = false;
        bool defaultFound = false;
        this->expect(LeftBrace);

        while (true) {
            if (this->match(RightBrace)) {
                break;
            }

            ASTNode clause = this->parseSwitchCase(builder, isDefaultNode);
            if (isDefaultNode) {
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

        this->context->allowLexicalDeclaration = oldAllowLexicalDeclaration;
        closeBlock(blockContext);

        this->context->inSwitch = previousInSwitch;
        return this->finalize(this->createNode(), builder.createSwitchStatementNode(discriminant, casesA, deflt, casesB, blockContext.childLexicalBlockIndex));
    }

    // ECMA-262 13.13 Labelled Statements

    template <class ASTBuilder>
    ASTNode parseLabelledStatement(ASTBuilder& builder, bool allowLabelledFunction = true, size_t multiLabelCount = 1)
    {
        MetaNode node = this->createNode();
        ASTNode expr = nullptr;
        AtomicString name;
        bool isIdentifier = false;

        expr = this->parseExpression(builder);
        if (expr->type() == Identifier) {
            isIdentifier = true;
            name = expr->asIdentifier()->name();
        }

        ASTNode statement = nullptr;
        if (isIdentifier && this->match(Colon)) {
            this->nextToken();

            if (hasLabel(name)) {
                this->throwError(Messages::Redeclaration, new ASCIIString("Label"), name.string());
            }

            this->context->labelSet.push_back(std::make_pair(name, false));

            if (this->lookahead.type == IdentifierToken && !this->matchAsyncFunction()) {
                ASTNode labelledBody = this->parseLabelledStatement(builder, allowLabelledFunction, multiLabelCount + 1);
                statement = builder.createLabelledStatementNode(labelledBody, name.string());
            } else {
                if (this->matchKeyword(DoKeyword) || this->matchKeyword(ForKeyword) || this->matchKeyword(WhileKeyword)) {
                    // Turn labels to accept continue references.
                    size_t end = this->context->labelSet.size() - 1;

                    for (size_t i = 0; i < multiLabelCount; i++) {
                        ASSERT(end >= i);
                        this->context->labelSet[end - i].second = true;
                    }
                }

                ASTNode labelledBody = this->parseStatement(builder, allowLabelledFunction && !this->context->strict, true);
                statement = builder.createLabelledStatementNode(labelledBody, name.string());
            }
            removeLabel(name);
        } else {
            this->consumeSemicolon();
            statement = builder.createExpressionStatementNode(expr);
        }

        return this->finalize(node, statement);
    }

    // ECMA-262 13.14 The throw statement

    template <class ASTBuilder>
    ASTNode parseThrowStatement(ASTBuilder& builder)
    {
        this->expectKeyword(ThrowKeyword);
        auto metaNode = this->createNode();

        if (this->hasLineTerminator) {
            this->throwError(Messages::NewlineAfterThrow);
        }

        ASTNode argument = this->parseExpression(builder);
        this->consumeSemicolon();

        return this->finalize(metaNode, builder.createThrowStatementNode(argument));
    }

    // ECMA-262 13.15 The try statement

    template <class ASTBuilder>
    ASTNode parseCatchClause(ASTBuilder& builder)
    {
        this->expectKeyword(CatchKeyword);
        if (this->match(LeftBrace)) {
            ParserBlockContext catchBlockContext;

            openBlock(catchBlockContext);
            bool oldInCatchClause = this->context->inCatchClause;

            this->context->inCatchClause = true;
            ASTNode body = this->parseBlock(builder);

            this->context->inCatchClause = oldInCatchClause;
            closeBlock(catchBlockContext);

            return this->finalize(this->createNode(), builder.createCatchClauseNode(nullptr, nullptr, body, catchBlockContext.childLexicalBlockIndex));
        } else {
            this->expect(LeftParenthesis);
            if (this->match(RightParenthesis)) {
                this->throwUnexpectedToken(this->lookahead);
            }

            ParserBlockContext catchBlockContext;
            openBlock(catchBlockContext);
            this->currentBlockContext->m_nodeType = ASTNodeType::CatchClause;

            SmallScannerResultVector params;
            ASTNode param = this->parsePattern(builder, params, KeywordKind::LetKeyword);

            if (this->context->strict && param->type() == Identifier) {
                if (this->scanner->isRestrictedWord(param->asIdentifier()->name())) {
                    this->throwError(Messages::StrictCatchVariable);
                }
            }

            this->expect(RightParenthesis);

            bool oldInCatchClause = this->context->inCatchClause;
            this->context->inCatchClause = true;

            bool gotSimplyDeclaredVariableName = param->isIdentifier();

            if (gotSimplyDeclaredVariableName) {
                this->context->catchClauseSimplyDeclaredVariableNames.push_back(std::make_pair(param->asIdentifier()->name(), this->lexicalBlockIndex));
            }

            if (!this->match(LeftBrace)) {
                this->throwUnexpectedToken(this->lookahead);
            }
            ASTNode body = this->parseBlock(builder);

            if (gotSimplyDeclaredVariableName) {
                this->context->catchClauseSimplyDeclaredVariableNames.pop_back();
            }

            this->context->inCatchClause = oldInCatchClause;

            closeBlock(catchBlockContext);

            return this->finalize(this->createNode(), builder.createCatchClauseNode(param, nullptr, body, catchBlockContext.childLexicalBlockIndex));
        }
    }

    template <class ASTBuilder>
    ASTNode parseFinallyClause(ASTBuilder& builder)
    {
        this->expectKeyword(FinallyKeyword);
        if (!this->match(LeftBrace)) {
            this->throwUnexpectedToken(this->lookahead);
        }
        return this->parseBlock(builder);
    }

    template <class ASTBuilder>
    ASTNode parseTryStatement(ASTBuilder& builder)
    {
        this->expectKeyword(TryKeyword);

        if (!this->match(LeftBrace)) {
            this->throwUnexpectedToken(this->lookahead);
        }
        ASTNode block = this->parseBlock(builder);
        ASTNode handler = nullptr;
        if (this->matchKeyword(CatchKeyword)) {
            handler = this->parseCatchClause(builder);
        }
        ASTNode finalizer = nullptr;
        if (this->matchKeyword(FinallyKeyword)) {
            finalizer = this->parseFinallyClause(builder);
        }

        if (!handler && !finalizer) {
            this->throwError(Messages::NoCatchOrFinally);
        }

        return this->finalize(this->createNode(), builder.createTryStatementNode(block, handler, finalizer));
    }

    // ECMA-262 13.16 The debugger statement
    template <class ASTBuilder>
    ASTNode parseDebuggerStatement(ASTBuilder& builder)
    {
        ESCARGOT_LOG_ERROR("debugger keyword is not supported yet");
        MetaNode node = this->createNode();
        this->expectKeyword(KeywordKind::DebuggerKeyword);
        this->consumeSemicolon();
        return this->finalize(node, builder.createDebuggerStatementNode());
    }

    template <class ASTBuilder>
    ASTNode parseSingleFunctionDeclarationAsBlockStatement(ASTBuilder& builder)
    {
        // parse single normal function declaration as a block statement
        ParserBlockContext blockContext;
        openBlock(blockContext);

        ASTStatementContainer block = builder.createStatementContainer();
        // async or generator function declaration is not allowed
        block->appendChild(this->parseFunctionDeclaration(builder, true), nullptr);

        closeBlock(blockContext);

        MetaNode node = this->createNode();
        return this->finalize(node, builder.createBlockStatementNode(block, blockContext.childLexicalBlockIndex));
    }

    // ECMA-262 13 Statements
    template <class ASTBuilder>
    ASTNode parseStatement(ASTBuilder& builder, bool allowFunctionDeclaration = true, bool shouldTopLevelDeclaration = false)
    {
        checkRecursiveLimit();
        ASTNode statement = nullptr;

        switch (this->lookahead.type) {
        case Token::BooleanLiteralToken:
        case Token::NullLiteralToken:
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
        case Token::TemplateToken:
        case Token::RegularExpressionToken:
            statement = this->parseExpressionStatement(builder);
            break;

        case Token::PunctuatorToken: {
            PunctuatorKind value = this->lookahead.valuePunctuatorKind;
            if (value == LeftBrace) {
                statement = this->parseBlock(builder);
            } else if (value == LeftParenthesis) {
                statement = this->parseExpressionStatement(builder);
            } else if (value == SemiColon) {
                statement = this->parseEmptyStatement(builder);
            } else {
                statement = this->parseExpressionStatement(builder);
            }
            break;
        }
        case Token::IdentifierToken:
            statement = this->matchAsyncFunction() ? this->parseFunctionDeclaration(builder, shouldTopLevelDeclaration) : this->parseLabelledStatement(builder, allowFunctionDeclaration);
            break;
        case Token::KeywordToken:
            switch (this->lookahead.valueKeywordKind) {
            case BreakKeyword:
                statement = this->parseBreakStatement(builder);
                break;
            case ContinueKeyword:
                statement = this->parseContinueStatement(builder);
                break;
            case DebuggerKeyword:
                statement = this->parseDebuggerStatement(builder);
                break;
            case DoKeyword:
                statement = this->parseDoWhileStatement(builder);
                break;
            case ForKeyword:
                statement = this->parseForStatement(builder);
                break;
            case FunctionKeyword: {
                if (UNLIKELY(!allowFunctionDeclaration)) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                statement = this->parseFunctionDeclaration(builder, shouldTopLevelDeclaration);
                break;
            }
            case IfKeyword:
                statement = this->parseIfStatement(builder);
                break;
            case ReturnKeyword:
                statement = this->parseReturnStatement(builder);
                break;
            case SwitchKeyword:
                statement = this->parseSwitchStatement(builder);
                break;
            case ThrowKeyword:
                statement = this->parseThrowStatement(builder);
                break;
            case TryKeyword:
                statement = this->parseTryStatement(builder);
                break;
            // Lexical declaration (let, const) cannot appear in a single-statement context
            case LetKeyword: {
                // check let array e.g. let [a]
                if (UNLIKELY(this->matchLetArray())) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                statement = this->parseExpressionStatement(builder);
                break;
            }
            case VarKeyword:
                statement = this->parseVariableStatement(builder, this->lookahead.valueKeywordKind);
                break;
            case WhileKeyword:
                statement = this->parseWhileStatement(builder);
                break;
            case WithKeyword:
                statement = this->parseWithStatement(builder);
                break;
            case ClassKeyword: {
                if (UNLIKELY(shouldTopLevelDeclaration)) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                statement = this->parseClassDeclaration(builder);
                break;
            }
            case YieldKeyword: {
                // TODO consider case that class method is generator function
                if (builder.isNodeGenerator()) {
                    statement = this->parseLabelledStatement(builder);
                } else {
                    statement = this->parseExpressionStatement(builder);
                }
                break;
            }
            default:
                statement = this->parseExpressionStatement(builder);
                break;
            }
            break;

        default:
            this->throwUnexpectedToken(this->lookahead);
        }

        return statement;
    }

    StatementContainer* parseFunctionParameters(NodeGenerator& builder)
    {
        ASSERT(this->isParsingSingleFunction);
        StatementContainer* container = builder.createStatementContainer();

        this->context->inParameterParsing = true;

        this->expect(LeftParenthesis);
        ParseFormalParametersResult options;
        MetaNode node = this->createNode();

        if (!this->match(RightParenthesis)) {
            size_t paramIndex = 0;
            while (this->startMarker.index < this->scanner->length) {
                Node* param = this->parseFormalParameter(builder, options);

                switch (param->type()) {
                case Identifier:
                case AssignmentPattern:
                case ArrayPattern:
                case ObjectPattern: {
                    Node* init = this->finalize(node, builder.createInitializeParameterExpressionNode(param, paramIndex));
                    Node* statement = this->finalize(node, builder.createExpressionStatementNode(init));
                    container->appendChild(statement);
                    break;
                }
                case RestElement: {
                    Node* statement = this->finalize(node, builder.createExpressionStatementNode(param));
                    container->appendChild(statement);
                    break;
                }
                default: {
                    ASSERT_NOT_REACHED();
                    break;
                }
                }

                if (this->match(RightParenthesis)) {
                    break;
                }
                this->expect(Comma);
                if (this->match(RightParenthesis)) {
                    break;
                }

                paramIndex++;
            }
        }
        this->expect(RightParenthesis);

        this->context->inParameterParsing = false;

        return container;
    }

    BlockStatementNode* parseFunctionBody(NodeGenerator& builder)
    {
        // only for parsing the body of callee function
        ASSERT(this->isParsingSingleFunction);

        this->context->inCatchClause = false;

        MetaNode nodeStart = this->createNode();
        StatementContainer* body = builder.createStatementContainer();

        this->expect(LeftBrace);

        LexicalBlockIndex blockIndex = 0;
        ParserBlockContext blockContext;
        if (currentScopeContext->m_functionBodyBlockIndex) {
            openBlock(blockContext);
            blockIndex = blockContext.childLexicalBlockIndex;
        }

        this->parseDirectivePrologues(builder, body);

        this->context->labelSet.clear();
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inFunctionBody = true;

        Node* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            if (this->match(RightBrace)) {
                break;
            }
            referNode = body->appendChild(this->parseStatementListItem(builder), referNode);
        }

        bool isEndedWithReturnNode = referNode && referNode->type() == ASTNodeType::ReturnStatement;
        if (!isEndedWithReturnNode) {
            referNode = body->appendChild(builder.createReturnStatementNode(nullptr));

#ifdef ESCARGOT_DEBUGGER
            referNode->m_loc.index = this->startMarker.index + this->baseMarker.index;
#endif /* ESCARGOT_DEBUGGER */
        }

        if (currentScopeContext->m_functionBodyBlockIndex) {
            closeBlock(blockContext);
        }

        this->expect(RightBrace);

        return this->finalize(nodeStart, builder.createBlockStatementNode(body, blockIndex));
    }

    // ECMA-262 14.1 Function Definition
    template <class ASTBuilder>
    ASTNode parseFunctionSourceElements(ASTBuilder& builder)
    {
        // parseFunctionSourceElements only scans the body of function
        // should be invoked only during the global parsing (program)
        ASSERT(!builder.willGenerateByteCode() && !this->isParsingSingleFunction);

        // return empty node because the function body node is never used now
        ASTNode result = nullptr;

        bool oldAllowLexicalDeclaration = this->context->allowLexicalDeclaration;
        auto oldNameCallback = this->nameDeclaredCallback;
        this->context->allowLexicalDeclaration = true;

        bool oldInCatchClause = this->context->inCatchClause;
        this->context->inCatchClause = false;

        auto oldCatchClauseSimplyDeclaredVariableNames = std::move(this->context->catchClauseSimplyDeclaredVariableNames);

        MetaNode nodeStart = this->createNode();
        ASTStatementContainer body = builder.createStatementContainer();

        this->expect(LeftBrace);
        this->parseDirectivePrologues(builder, body);

        auto previousLabelSet = this->context->labelSet;
        bool previousInIteration = this->context->inIteration;
        bool previousInSwitch = this->context->inSwitch;
        bool previousInFunctionBody = this->context->inFunctionBody;

        this->context->labelSet.clear();
        this->context->inIteration = false;
        this->context->inSwitch = false;
        this->context->inFunctionBody = true;

        ParserBlockContext blockContext;
        if (this->currentScopeContext->m_functionBodyBlockIndex) {
            openBlock(blockContext);
            this->currentScopeContext->m_functionBodyBlockIndex = blockContext.childLexicalBlockIndex;
        }

        while (this->startMarker.index < this->scanner->length) {
            if (this->match(RightBrace)) {
                break;
            }
            this->parseStatementListItem(builder);
        }

        if (this->currentScopeContext->m_functionBodyBlockIndex) {
            closeBlock(blockContext);
        }

        this->expect(RightBrace);

        this->context->inCatchClause = oldInCatchClause;
        this->context->catchClauseSimplyDeclaredVariableNames = std::move(oldCatchClauseSimplyDeclaredVariableNames);

        this->context->allowLexicalDeclaration = oldAllowLexicalDeclaration;
        this->nameDeclaredCallback = oldNameCallback;

        this->context->labelSet = previousLabelSet;
        this->context->inIteration = previousInIteration;
        this->context->inSwitch = previousInSwitch;
        this->context->inFunctionBody = previousInFunctionBody;

        this->currentScopeContext->m_bodyEndLOC.index = this->lastMarker.index;
#if !(defined NDEBUG) || defined ESCARGOT_DEBUGGER
        this->currentScopeContext->m_bodyEndLOC.line = this->lastMarker.lineNumber;
        this->currentScopeContext->m_bodyEndLOC.column = this->lastMarker.index - this->lastMarker.lineStart;
#endif

        return result;
    }

    template <class ASTBuilder, bool treatEmptyNameAsDefault = false>
    ASTNode parseFunctionDeclaration(ASTBuilder& builder, bool disallowAsyncOrGenerator = false)
    {
        MetaNode node = this->createNode();
        bool isAsync = this->matchContextualKeyword("async");
        if (isAsync) {
            this->nextToken();
        }

        this->expectKeyword(FunctionKeyword);

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        if (UNLIKELY(disallowAsyncOrGenerator && (isAsync || isGenerator))) {
            this->throwError(Messages::AsyncGeneratorFuncDeclLocationError);
        }

        const char* message = nullptr;
        ASTNode id = nullptr;
        Scanner::SmallScannerResult firstRestricted;

        if (treatEmptyNameAsDefault && this->match(PunctuatorKind::LeftParenthesis)) {
            id = builder.createIdentifierNode(this->escargotContext->staticStrings().stringDefault);
        } else {
            ALLOC_TOKEN(token);
            *token = this->lookahead;
            TrackUsingNameBlocker blocker(this);
            id = this->parseVariableIdentifier(builder);

            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource(this->scanner->source))) {
                    this->throwUnexpectedToken(*token, Messages::StrictFunctionName);
                }
            } else {
                if (this->scanner->isRestrictedWord(token->relatedSource(this->scanner->source))) {
                    firstRestricted = *token;
                    message = Messages::StrictFunctionName;
                } else if (this->scanner->isStrictModeReservedWord(token->relatedSource(this->scanner->source))) {
                    firstRestricted = *token;
                    message = Messages::StrictReservedWord;
                }
            }

            if (UNLIKELY(this->context->strict && isGenerator)) {
                if (id->asIdentifier()->name().string()->equals("yield")) {
                    this->throwUnexpectedToken(*token, Messages::UnexpectedReserved);
                }
            }
            // in global scope, await is allowed
            if (UNLIKELY(isAsync && !builder.willGenerateByteCode())) {
                if (id->asIdentifier()->name().string()->equals("await")) {
                    this->throwUnexpectedToken(*token, Messages::UnexpectedReserved);
                }
            }
        }

        ASSERT(id);
        AtomicString fnName = id->asIdentifier()->name();

        if (tryToSkipFunctionParsing()) {
            return this->finalize(node, builder.createFunctionDeclarationNode(subCodeBlockIndex, fnName));
        }

        MetaNode paramsStart = this->createNode();
        this->expect(LeftParenthesis);

        // https://www.ecma-international.org/ecma-262/10.0/#sec-web-compat-functiondeclarationinstantiation
        bool isTopScope = this->currentScopeContext->m_functionBodyBlockIndex == this->lexicalBlockIndex;
        if (!isTopScope) {
            this->currentScopeContext->insertFuncDeclNameAtBlock(fnName, this->lexicalBlockIndex);
        }

        if (this->context->strict) {
            if (isTopScope && UNLIKELY(this->sourceType != SourceType::Module)) {
                addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::VarKeyword);
            } else {
                addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::LetKeyword);
            }
        } else {
            // appendixB B.3.3.1~B.3.3.3
            if (UNLIKELY(this->currentScopeContext->hasNameAtBlock(fnName, this->lexicalBlockIndex))) {
                bool isCatchClauseVariableName = false;

                for (size_t i = 0; i < this->context->catchClauseSimplyDeclaredVariableNames.size(); i++) {
                    if (this->context->catchClauseSimplyDeclaredVariableNames[i].first == fnName) {
                        isCatchClauseVariableName = true;
                        // try {} catch(e) { function e() {} }
                        if ((this->context->catchClauseSimplyDeclaredVariableNames[i].second + 1) == this->lexicalBlockIndex) {
                            this->throwError(Messages::Redeclaration, new ASCIIString("Identifier"), fnName.string());
                        }

                        auto blockInfo = this->currentScopeContext->findBlockFromBackward(this->context->catchClauseSimplyDeclaredVariableNames[i].second);
                        for (size_t i = 0; i < blockInfo->m_names.size(); i++) {
                            if (blockInfo->m_names[i].name() == fnName) {
                                blockInfo->m_names.erase(i);
                                break;
                            }
                        }

                        this->currentScopeContext->insertVarName(fnName, this->lexicalBlockIndex, true, true);
                        break;
                    }
                }

                if (!isCatchClauseVariableName) {
                    addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::LetKeyword);
                }
            } else {
                if (isTopScope) {
                    addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::VarKeyword);
                } else {
                    if (UNLIKELY(isAsync || isGenerator)) {
                        addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::LetKeyword);
                    } else {
                        size_t r = this->currentScopeContext->findVarName(fnName);
                        if (r == SIZE_MAX) {
                            addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::VarKeyword);
                        } else if (this->currentScopeContext->m_varNames[r].isParameterName()) {
                            addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, KeywordKind::LetKeyword);
                        } else {
                            KeywordKind kind = KeywordKind::VarKeyword;
                            if (this->currentScopeContext->hasFuncDeclNameAtBlock(fnName, this->currentBlockContext->m_parentBlockIndex)) {
                                kind = KeywordKind::LetKeyword;
                            }
                            addDeclaredNameIntoContext(fnName, this->lexicalBlockIndex, kind, false);
                        }
                    }
                }
            }
        }
        this->insertUsingName(fnName);

        BEGIN_FUNCTION_SCANNING(fnName);

        if (!isTopScope) {
            this->currentScopeContext->insertVarName(fnName, 0, false);
        }

        this->currentScopeContext->m_isAsync = isAsync;
        this->currentScopeContext->m_isGenerator = isGenerator;
        this->currentScopeContext->m_functionStartLOC.index = node.index;
        this->currentScopeContext->m_functionStartLOC.column = node.column;
        this->currentScopeContext->m_functionStartLOC.line = node.line;
        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionDeclaration;

        bool previousAwait = this->context->await;
        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        bool previousAllowNewTarget = this->context->allowNewTarget;
        const bool previousAllowArguments = this->context->allowArguments;

        this->context->inArrowFunction = false;
        this->context->allowNewTarget = true;
        this->context->allowArguments = true;
        this->context->allowYield = false;

        ParseFormalParametersResult formalParameters;
        this->parseFormalParameters(newBuilder, formalParameters, &firstRestricted);
        Scanner::SmallScannerResult stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        extractNamesFromFunctionParams(formalParameters);

        this->context->await = isAsync;
        this->context->allowYield = isGenerator;

        bool previousStrict = this->context->strict;
        bool previousAllowStrictDirective = this->context->allowStrictDirective;
        this->context->allowStrictDirective = !this->currentScopeContext->m_hasParameterOtherThanIdentifier;

        this->parseFunctionSourceElements(newBuilder);
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->throwUnexpectedToken(stricted, message);
        }
        this->context->strict = previousStrict;
        this->context->await = previousAwait;
        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;
        this->context->allowStrictDirective = previousAllowStrictDirective;

        END_FUNCTION_SCANNING();
        return static_cast<ASTNode>(this->finalize(node, builder.createFunctionDeclarationNode(subCodeBlockIndex, fnName)));
    }

    template <class ASTBuilder>
    ASTNode parseFunctionExpression(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        bool isAsync = this->matchContextualKeyword("async");
        if (isAsync) {
            this->nextToken();
        }

        this->expectKeyword(FunctionKeyword);

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        const char* message = nullptr;
        ASTNode id = nullptr;
        Scanner::SmallScannerResult firstRestricted;

        bool previousAwait = this->context->await;
        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        bool previousAllowNewTarget = this->context->allowNewTarget;
        bool previousAllowArguments = this->context->allowArguments;

        this->context->inArrowFunction = false;
        this->context->allowNewTarget = true;
        this->context->allowArguments = true;

        if (!this->match(LeftParenthesis)) {
            ALLOC_TOKEN(token);
            *token = this->lookahead;
            TrackUsingNameBlocker blocker(this);
            id = (!this->context->strict && !isGenerator && this->matchKeyword(YieldKeyword)) ? this->parseIdentifierName(builder) : this->parseVariableIdentifier(builder);

            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource(this->scanner->source))) {
                    this->throwUnexpectedToken(*token, Messages::StrictFunctionName);
                }
            } else {
                if (this->scanner->isRestrictedWord(token->relatedSource(this->scanner->source))) {
                    firstRestricted = *token;
                    message = Messages::StrictFunctionName;
                } else if (this->scanner->isStrictModeReservedWord(token->relatedSource(this->scanner->source))) {
                    firstRestricted = *token;
                    message = Messages::StrictReservedWord;
                }
            }

            if (UNLIKELY(this->context->strict && isGenerator)) {
                if (id->asIdentifier()->name().string()->equals("yield")) {
                    this->throwUnexpectedToken(*token, Messages::UnexpectedReserved);
                }
            }
            // in global scope, await is allowed
            if (UNLIKELY(isAsync && !builder.willGenerateByteCode())) {
                if (id->asIdentifier()->name().string()->equals("await")) {
                    this->throwUnexpectedToken(*token, Messages::UnexpectedReserved);
                }
            }
        }

        AtomicString fnName = id ? id->asIdentifier()->name() : AtomicString();

        if (tryToSkipFunctionParsing()) {
            this->context->await = previousAwait;
            this->context->allowYield = previousAllowYield;
            this->context->inArrowFunction = previousInArrowFunction;
            this->context->allowNewTarget = previousAllowNewTarget;
            this->context->allowArguments = previousAllowArguments;
            return this->finalize(node, builder.createFunctionExpressionNode(subCodeBlockIndex, fnName));
        }

        MetaNode paramsStart = this->createNode();
        this->expect(LeftParenthesis);

        BEGIN_FUNCTION_SCANNING(fnName);

        if (id) {
            this->currentScopeContext->insertVarName(fnName, 0, false);
        }

        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionExpression;
        this->currentScopeContext->m_isGenerator = isGenerator;
        this->currentScopeContext->m_isAsync = isAsync;
        this->currentScopeContext->m_functionStartLOC.index = node.index;
        this->currentScopeContext->m_functionStartLOC.column = node.column;
        this->currentScopeContext->m_functionStartLOC.line = node.line;
        this->context->allowYield = false;

        ParseFormalParametersResult formalParameters;
        this->parseFormalParameters(newBuilder, formalParameters, &firstRestricted);
        Scanner::SmallScannerResult stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        extractNamesFromFunctionParams(formalParameters);

        this->context->await = isAsync;
        this->context->allowYield = isGenerator;
        bool previousStrict = this->context->strict;
        bool previousAllowStrictDirective = this->context->allowStrictDirective;
        this->context->allowStrictDirective = !this->currentScopeContext->m_hasParameterOtherThanIdentifier;

        this->parseFunctionSourceElements(newBuilder);
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->throwUnexpectedToken(stricted, message);
        }
        this->context->strict = previousStrict;
        this->context->await = previousAwait;
        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;
        this->context->allowStrictDirective = previousAllowStrictDirective;

        END_FUNCTION_SCANNING();
        return static_cast<ASTNode>(this->finalize(node, builder.createFunctionExpressionNode(subCodeBlockIndex, fnName)));
    }

    // ECMA-262 14.1.1 Directive Prologues

    template <class ASTBuilder>
    ASTNode parseDirective(ASTBuilder& builder, bool& isStrictDirective)
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        ASSERT(token->type == StringLiteralToken);
        ASSERT(!isStrictDirective);

        // check strict mode early before lexing of following tokens
        if (!token->hasAllocatedString && token->valueStringLiteral(this->scanner).equals("use strict")) {
            isStrictDirective = true;
            this->currentScopeContext->m_isStrict = this->context->strict = true;
            if (!this->context->allowStrictDirective) {
                this->throwError("Illegal 'use strict' directive in function with non-simple parameter list");
            }
        }

        MetaNode node = this->createNode();
        ASTNode expr = this->parseExpression(builder);
        this->consumeSemicolon();

        if (expr->type() == Literal) {
            return this->finalize(node, builder.createDirectiveNode(expr));
        }
        return this->finalize(node, builder.createExpressionStatementNode(expr));
    }

    template <class ASTBuilder>
    void parseDirectivePrologues(ASTBuilder& builder, ASTStatementContainer container)
    {
        ASSERT(container);
        Scanner::SmallScannerResult firstRestricted;

        ALLOC_TOKEN(token);
        while (true) {
            *token = this->lookahead;

            // https://github.com/tc39/proposal-hashbang
            // InputElementHashbangOrRegExp ::
            //   WhiteSpace
            //   LineTerminator
            //   Comment
            //   CommonToken
            //   HashbangComment
            //   RegularExpressionLiteral
            // HashbangComment ::
            //   `#!` SingleLineCommentChars?
            if (UNLIKELY(token->type == Token::PunctuatorToken
                         && token->valuePunctuatorKind == PunctuatorKind::HashBang)) {
                size_t tokenLineNumber = this->lookahead.lineNumber;
                this->scanner->skipSingleLine();
                this->scanner->scanComments();

                this->startMarker.index = this->scanner->index;
                this->startMarker.lineNumber = this->scanner->lineNumber;
                this->startMarker.lineStart = this->scanner->lineStart;

                this->scanner->lex(token);
                this->hasLineTerminator = tokenLineNumber != token->lineNumber;

                if (this->context->strict && token->type == Token::IdentifierToken
                    && this->scanner->isStrictModeReservedWord(token->relatedSource(this->scanner->source))) {
                    this->scanner->convertToKeywordInStrictMode(token);
                }

                this->lookahead = *token;
            }

            if (token->type != StringLiteralToken) {
                break;
            }

            bool isStrictDirective = false;
            ASTNode statement = this->parseDirective(builder, isStrictDirective);

            if (!isStrictDirective || !this->isParsingSingleFunction) {
                // skip adding the strict directive node only when its in the function scope
                // e.g. eval("'use strict'") == "use strict"
                container->appendChild(statement);
            }

            if (statement->type() != Directive) {
                break;
            }

            if (this->context->strict) {
                if (firstRestricted) {
                    this->throwUnexpectedToken(firstRestricted, Messages::StrictOctalLiteral);
                }
            } else {
                if (!firstRestricted && token->octal) {
                    firstRestricted = *token;
                }
            }
        }
    }

    // ECMA-262 14.3 Method Definitions

    template <class ASTBuilder>
    ASTNode parseGetterMethod(ASTBuilder& builder, const MetaNode& functionStart)
    {
        MetaNode node = this->createNode();

        bool isGenerator = false;

        if (tryToSkipFunctionParsing()) {
            return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
        }

        const bool previousAllowYield = this->context->allowYield;
        const bool previousInArrowFunction = this->context->inArrowFunction;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;
        const bool previousAllowNewTarget = this->context->allowNewTarget;
        const bool previousAllowArguments = this->context->allowArguments;

        this->context->allowYield = false;
        this->context->inArrowFunction = false;
        this->context->allowSuperProperty = true;
        this->context->allowNewTarget = true;
        this->context->allowArguments = true;

        this->expect(LeftParenthesis);
        this->expect(RightParenthesis);

        BEGIN_FUNCTION_SCANNING(AtomicString());

        this->currentScopeContext->m_functionStartLOC.index = functionStart.index;
        this->currentScopeContext->m_functionStartLOC.column = functionStart.column;
        this->currentScopeContext->m_functionStartLOC.line = functionStart.line;
        this->currentScopeContext->m_allowSuperProperty = true;
        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionExpression;
        this->currentScopeContext->m_isGenerator = isGenerator;

        ParseFormalParametersResult params;
        extractNamesFromFunctionParams(params);
        this->parsePropertyMethod(newBuilder, params);

        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowSuperProperty = previousAllowSuperProperty;
        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;

        END_FUNCTION_SCANNING();
        return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
    }

    template <class ASTBuilder>
    ASTNode parseSetterMethod(ASTBuilder& builder, const MetaNode& functionStart)
    {
        MetaNode node = this->createNode();

        bool isGenerator = false;

        if (tryToSkipFunctionParsing()) {
            return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
        }

        const bool previousAllowYield = this->context->allowYield;
        const bool previousInArrowFunction = this->context->inArrowFunction;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;
        const bool previousAllowNewTarget = this->context->allowNewTarget;
        const bool previousAllowArguments = this->context->allowArguments;

        this->context->allowYield = false;
        this->context->allowSuperProperty = true;
        this->context->inArrowFunction = false;
        this->context->allowNewTarget = true;
        this->context->allowArguments = true;

        this->expect(LeftParenthesis);

        BEGIN_FUNCTION_SCANNING(AtomicString());

        this->currentScopeContext->m_functionStartLOC.index = functionStart.index;
        this->currentScopeContext->m_functionStartLOC.column = functionStart.column;
        this->currentScopeContext->m_functionStartLOC.line = functionStart.line;
        this->currentScopeContext->m_allowSuperProperty = true;
        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionExpression;
        this->currentScopeContext->m_isGenerator = isGenerator;

        ParseFormalParametersResult formalParameters;
        this->parseFormalParameters(newBuilder, formalParameters, nullptr, false);
        if (formalParameters.params.size() != 1) {
            this->throwError(Messages::BadSetterArity);
        } else if (formalParameters.params[0]->type() == ASTNodeType::RestElement) {
            this->throwError(Messages::BadSetterRestParameter);
        }
        extractNamesFromFunctionParams(formalParameters);
        this->parsePropertyMethod(newBuilder, formalParameters);

        this->context->allowYield = previousAllowYield;
        this->context->allowSuperProperty = previousAllowSuperProperty;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;

        END_FUNCTION_SCANNING();
        return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
    }

    template <class ASTBuilder>
    ASTNode parseGeneratorMethod(ASTBuilder& builder, const MetaNode& functionStart)
    {
        MetaNode node = this->createNode();

        if (tryToSkipFunctionParsing()) {
            return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
        }

        const bool previousAllowYield = this->context->allowYield;
        const bool previousInArrowFunction = this->context->inArrowFunction;
        const bool previousAllowSuperProperty = this->context->allowSuperProperty;
        const bool previousAllowNewTarget = this->context->allowNewTarget;
        const bool previousAllowArguments = this->context->allowArguments;

        this->context->allowSuperProperty = true;
        this->context->inArrowFunction = false;
        this->context->allowNewTarget = true;
        this->context->allowArguments = true;

        this->expect(LeftParenthesis);

        BEGIN_FUNCTION_SCANNING(AtomicString());

        this->currentScopeContext->m_functionStartLOC.index = functionStart.index;
        this->currentScopeContext->m_functionStartLOC.column = functionStart.column;
        this->currentScopeContext->m_functionStartLOC.line = functionStart.line;
        this->currentScopeContext->m_allowSuperProperty = true;
        this->currentScopeContext->m_nodeType = ASTNodeType::FunctionExpression;
        this->currentScopeContext->m_isGenerator = true;

        this->context->allowYield = false;

        ParseFormalParametersResult formalParameters;
        this->parseFormalParameters(newBuilder, formalParameters);
        extractNamesFromFunctionParams(formalParameters);

        this->context->allowYield = true;
        this->parsePropertyMethod(newBuilder, formalParameters);

        this->context->allowYield = previousAllowYield;
        this->context->allowSuperProperty = previousAllowSuperProperty;
        this->context->inArrowFunction = previousInArrowFunction;
        this->context->allowNewTarget = previousAllowNewTarget;
        this->context->allowArguments = previousAllowArguments;

        END_FUNCTION_SCANNING();
        return this->finalize(node, builder.createFunctionExpressionNode(this->subCodeBlockIndex, AtomicString()));
    }

    // ECMA-262 14.4 Generator Function Definitions

    bool isStartOfExpression() const
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
            case KeywordKind::ImportKeyword:
                start = true;
                break;
            default:
                start = false;
            }
        }

        return start;
    }

    template <class ASTBuilder>
    ASTNode parseYieldExpression(ASTBuilder& builder)
    {
        if (this->context->inParameterParsing) {
            this->throwError("Cannot use yield expression within parameters");
        }

        MetaNode node = this->createNode();
        this->expectKeyword(YieldKeyword);

        ASTNode exprNode = nullptr;
        bool delegate = false;

        if (!this->hasLineTerminator) {
            const bool previousAllowYield = this->context->allowYield;
            this->context->allowYield = true;
            delegate = this->match(Multiply);

            if (delegate) {
                this->nextToken();
                exprNode = this->parseAssignmentExpression<ASTBuilder, false>(builder);
            } else if (isStartOfExpression()) {
                exprNode = this->parseAssignmentExpression<ASTBuilder, false>(builder);
            }
            this->context->allowYield = previousAllowYield;
        }

        return this->finalize(node, builder.createYieldExpressionNode(exprNode, delegate));
    }

    // ECMA-262 14.5 Class Definitions
    typedef VectorWithInlineStorage<12, AtomicString, std::allocator<AtomicString>> ParserAtomicStringVector;
    struct ClassPrivateNameData {
        AtomicString name;
        bool isField;
        bool isMethod;
        bool isGetter;
        bool isSetter;

        ClassPrivateNameData(AtomicString name = AtomicString())
            : name(name)
            , isField(false)
            , isMethod(false)
            , isGetter(false)
            , isSetter(false)
        {
        }
    };
    typedef VectorWithInlineStorage<12, ClassPrivateNameData, std::allocator<ClassPrivateNameData>> ParserClassPrivateNameVector;

    template <class ASTBuilder>
    ASTNode parseClassElement(ASTBuilder& builder, ASTNode& constructor, bool hasSuperClass, const AtomicString& className,
                              size_t& fieldCount, ParserClassPrivateNameVector& privateNames)
    {
        ALLOC_TOKEN(token);
        *token = this->lookahead;
        MetaNode node = this->createNode();
        MetaNode mayMethodStartNode(node);

        ClassElementNode::Kind kind = ClassElementNode::Kind::None;

        ASTNode keyNode = nullptr;
        ASTNode value = nullptr;

        bool computed = false;
        bool seenStaticForward = false;
        bool isStatic = false;
        bool isGenerator = false;
        bool isAsync = false;
        bool isPrivate = false;

#ifdef ESCARGOT_DEBUGGER
        size_t debuggerLineStart = token->lineNumber;
#else /* !ESCARGOT_DEBUGGER */
        size_t debuggerLineStart = 0;
#endif /* ESCARGOT_DEBUGGER */

        if (this->match(Multiply)) {
            this->nextToken();
        } else {
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);

            if (token->type == Token::KeywordToken && (this->qualifiedPropertyName(&this->lookahead) || this->match(Multiply) || this->match(Hash)) && token->relatedSource(this->scanner->source) == "static") {
                *token = this->lookahead;
                isStatic = true;
#ifdef ESCARGOT_DEBUGGER
                debuggerLineStart = token->lineNumber;
#endif /* ESCARGOT_DEBUGGER */
                mayMethodStartNode = this->createNode();
                computed = this->match(LeftSquareBracket);
                if (this->match(Multiply)) {
                    this->nextToken();
                } else {
                    keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);
                }
            }
            if ((token->type == Token::IdentifierToken) && !this->hasLineTerminator && (token->relatedSource(this->scanner->source) == "async")) {
                bool isPunctuator = this->lookahead.type == Token::PunctuatorToken;
                PunctuatorKind punctuator = this->lookahead.valuePunctuatorKind;
                isGenerator = isPunctuator && punctuator == Multiply;
                if (!isPunctuator || (punctuator != Colon && punctuator != LeftParenthesis)) {
                    isAsync = true;
                    if (isGenerator) {
                        this->nextToken();
                    }
                    computed = this->match(LeftSquareBracket);
                    *token = this->lookahead;
                    keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);
                    if (!isStatic && builder.isPropertyKey(keyNode, "constructor")) {
                        this->throwError(Messages::InvalidClassElementName);
                    }
                }
            }
        }

        bool lookaheadPropertyKey = this->qualifiedPropertyName(&this->lookahead) || this->match(PunctuatorKind::Hash);
        if (token->type == Token::IdentifierToken) {
            if (token->valueStringLiteral(this->scanner) == "get" && lookaheadPropertyKey) {
                kind = ClassElementNode::Kind::Get;
                computed = this->match(LeftSquareBracket);
                keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);
                value = this->parseGetterMethod(builder, mayMethodStartNode);
            } else if (token->valueStringLiteral(this->scanner) == "set" && lookaheadPropertyKey) {
                kind = ClassElementNode::Kind::Set;
                computed = this->match(LeftSquareBracket);
                keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);
                value = this->parseSetterMethod(builder, mayMethodStartNode);
            }
        } else if (lookaheadPropertyKey && token->type == Token::PunctuatorToken && token->valuePunctuatorKind == Multiply) {
            kind = ClassElementNode::Kind::Method;
            computed = this->match(LeftSquareBracket);
            keyNode = this->parseObjectPropertyKey(builder, !computed, &isPrivate);
            value = this->parseGeneratorMethod(builder, this->createNode());
            isGenerator = true;
        }

        if (kind == ClassElementNode::Kind::None && keyNode) {
            if (this->match(LeftParenthesis)) {
                kind = ClassElementNode::Kind::Method;
                bool allowSuperCall = false;
                if (hasSuperClass) {
                    if (builder.isPropertyKey(keyNode, "constructor")) {
                        allowSuperCall = true;
                    }
                }
                value = this->parsePropertyMethodFunction(builder, allowSuperCall, isGenerator, isAsync, mayMethodStartNode);
            } else if (this->match(Substitution)) {
                if (isStatic) {
                    kind = ClassElementNode::Kind::Field;
                    value = this->parseClassStaticFieldInitializer(builder, className, debuggerLineStart);
                } else {
                    kind = ClassElementNode::Kind::Field;
                    value = this->parseClassFieldInitializer(builder, debuggerLineStart);
                }
            } else if (this->match(SemiColon)) {
                auto metaNode = this->createNode();
                this->nextToken();

                kind = ClassElementNode::Kind::Field;
                value = this->finalize(metaNode, builder.createLiteralNode(Value()));
            } else {
                if (this->hasLineTerminator) {
                    kind = ClassElementNode::Kind::Field;
                    value = this->finalize(this->createNode(), builder.createLiteralNode(Value()));
                }
            }
        }

        if ((kind == ClassElementNode::Kind::Field && isStatic) && !computed) {
            if (builder.isPropertyKey(keyNode, "constructor") || builder.isPropertyKey(keyNode, "prototype")) {
                this->throwError(Messages::InvalidClassElementName);
            }
        }

        if (isPrivate) {
            if (isStatic) {
                if (builder.isPropertyKey(keyNode, "#constructor")) {
                    this->throwError(Messages::InvalidClassElementName);
                }
            } else {
                if (builder.isPropertyKey(keyNode, "#prototype") || builder.isPropertyKey(keyNode, "#constructor")) {
                    this->throwError(Messages::InvalidClassElementName);
                }
            }
        }

        if (kind == ClassElementNode::Kind::Field) {
            fieldCount++;
            if (builder.isPropertyKeyStartsWith(keyNode, 0x200C) || builder.isPropertyKeyStartsWith(keyNode, 0x200D)) {
                this->throwError(Messages::InvalidClassElementName);
            }
        }

        if (isPrivate) {
            auto s = keyNode->asIdentifier()->name();
            bool find = false;
            for (size_t i = 0; i < privateNames.size(); i++) {
                if (privateNames[i].name == s) {
                    bool isInvalid = false;
                    if (kind == ClassElementNode::Kind::Field || kind == ClassElementNode::Kind::Method) {
                        isInvalid = true;
                    } else if (kind == ClassElementNode::Kind::Get && !privateNames[i].isSetter) {
                        isInvalid = true;
                    } else if (kind == ClassElementNode::Kind::Set && !privateNames[i].isGetter) {
                        isInvalid = true;
                    }
                    if (isInvalid) {
                        this->lastMarker.index = mayMethodStartNode.index - this->baseMarker.index;
                        this->lastMarker.lineNumber = mayMethodStartNode.line;
                        this->lastMarker.lineStart = mayMethodStartNode.index - mayMethodStartNode.column;
                        this->throwError("duplicate class field name '%s'", s.string());
                    }
                    find = true;
                    break;
                }
            }

            if (!find) {
                ClassPrivateNameData data;
                data.name = s;
                if (kind == ClassElementNode::Kind::Field) {
                    data.isField = true;
                } else if (kind == ClassElementNode::Kind::Method) {
                    data.isMethod = true;
                } else if (kind == ClassElementNode::Kind::Get) {
                    data.isGetter = true;
                } else if (kind == ClassElementNode::Kind::Set) {
                    data.isSetter = true;
                } else {
                    this->throwUnexpectedToken(this->lookahead);
                }
                privateNames.pushBack(data);
            }
        }

        if (kind == ClassElementNode::Kind::None) {
            this->throwUnexpectedToken(this->lookahead);
        }

        if (isStatic && !computed && builder.isPropertyKey(keyNode, "prototype")) {
            this->throwUnexpectedToken(*token, Messages::StaticPrototype);
        }
        if (!isStatic && !computed && builder.isPropertyKey(keyNode, "constructor")) {
            if (kind != ClassElementNode::Kind::Method) {
                this->throwUnexpectedToken(*token, Messages::ConstructorSpecialMethod);
            }
            if (isGenerator) {
                this->throwUnexpectedToken(*token, Messages::ConstructorGenerator);
            }
            if (constructor) {
                this->throwUnexpectedToken(*token, Messages::DuplicateConstructor);
            } else {
                if (!this->isParsingSingleFunction) {
                    this->lastPoppedScopeContext->m_functionName = escargotContext->staticStrings().constructor;
                    this->lastPoppedScopeContext->m_isClassConstructor = true;
                    this->lastPoppedScopeContext->m_isDerivedClassConstructor = hasSuperClass;
                }
                constructor = value;
                ASTNode empty = nullptr;
                return empty;
            }
        }

        if (!computed && keyNode->isIdentifier()) {
            ASSERT(!!value);
            this->addImplicitName(value, keyNode->asIdentifier()->name());
        }

        if (!this->isParsingSingleFunction) {
            if (kind != ClassElementNode::Kind::Field) {
                if (isStatic) {
                    this->lastPoppedScopeContext->m_isClassStaticMethod = true;
                } else {
                    this->lastPoppedScopeContext->m_isClassMethod = true;
                }
            }
        }

        return this->finalize(node, builder.createClassElementNode(keyNode, value, kind, computed, isStatic, isPrivate));
    }

    void testClassPrivateNameRules(ASTClassInfo* info)
    {
        for (size_t i = 0; i < info->m_classPrivateNameUsingInfo.size(); i++) {
            auto name = info->m_classPrivateNameUsingInfo[i].m_name;

            ASTClassInfo* c = info;

            bool find = false;
            while (c) {
                for (size_t j = 0; j < c->m_classPrivateNames.size(); j++) {
                    if (c->m_classPrivateNames[j] == name) {
                        find = true;
                        break;
                    }
                }

                if (find) {
                    break;
                }

                c = c->m_parent;
            }
            if (!find) {
                // throw error
                this->lastMarker.index = info->m_classPrivateNameUsingInfo[i].m_index - this->baseMarker.index;
                this->lastMarker.lineNumber = info->m_classPrivateNameUsingInfo[i].m_lineNumber;
                this->lastMarker.lineStart = info->m_classPrivateNameUsingInfo[i].m_lineStart;
                this->throwError(Messages::PrivateFieldMustBeDeclared, name.string());
            }
        }

        if (info->m_parent) {
            for (size_t i = 0; i < info->m_classPrivateNames.size(); i++) {
                auto name = info->m_classPrivateNames[i];

                ASTClassInfo* c = info->m_parent;

                bool find = false;
                while (c) {
                    for (size_t j = 0; j < c->m_classPrivateNames.size(); j++) {
                        if (c->m_classPrivateNames[j] == name) {
                            find = true;
                            break;
                        }
                    }

                    if (find) {
                        info->m_hasSameNameOnParentClass = true;
                        break;
                    }

                    c = c->m_parent;
                }
            }
        }

        bool seenDirectEval = false;
        if (info->m_firstMethod) {
            auto s = info->m_firstMethod;
            while (true) {
                if (s->m_hasEval) {
                    seenDirectEval = true;
                    break;
                }
                if (s == info->m_lastMethod) {
                    break;
                }
                s = s->nextSibling();
            }
        }

        if (UNLIKELY(info->m_hasSameNameOnParentClass || seenDirectEval)) {
            ASTClassInfo* cur = info;
            while (cur) {
                if (cur->m_firstMethod) {
                    AtomicStringTightVector* privateNames = nullptr;
                    auto s = cur->m_firstMethod;
                    while (true) {
                        s->m_needRareData = true;
                        if (!s->m_classPrivateNames) {
                            if (!privateNames) {
                                privateNames = new AtomicStringTightVector();
                                privateNames->resize(cur->m_classPrivateNames.size());

                                for (size_t i = 0; i < cur->m_classPrivateNames.size(); i++) {
                                    (*privateNames)[i] = cur->m_classPrivateNames[i];
                                }
                            }
                            s->m_classPrivateNames = privateNames;
                        }

                        if (s == cur->m_lastMethod) {
                            break;
                        }
                        s = s->nextSibling();
                    }
                }

                cur = cur->m_parent;
            }
        }

        auto child = info->firstChild();
        while (child) {
            testClassPrivateNameRules(child);
            child = child->nextSibling();
        }
    }

    template <class ASTBuilder>
    ASTNode parseClassBody(ASTBuilder& builder, bool hasSuperClass, MetaNode& endNode, const AtomicString& className)
    {
        MetaNode node = this->createNode();

        ASTClassInfo* newClassInfo = new (allocator) ASTClassInfo();
        ASTClassInfo* oldClassInfo = this->currentClassInfo;

        this->currentClassInfo = newClassInfo;

        if (oldClassInfo) {
            oldClassInfo->appendChild(newClassInfo);
        }

        ASTNodeList body;
        ASTNode constructor = nullptr;

        size_t fieldCount = 0;
        ParserClassPrivateNameVector privateNames;

        this->expect(LeftBrace);
        while (!this->match(RightBrace)) {
            if (this->match(SemiColon)) {
                this->nextToken();
            } else {
                ASTNode classElement = this->parseClassElement(builder, constructor, hasSuperClass, className, fieldCount, privateNames);
                if (classElement) {
                    body.append(this->allocator, classElement);
                }
            }
        }
        endNode = this->createNode();
        endNode.index++; // advancing for '{'
        this->expect(RightBrace);

        if (UNLIKELY(fieldCount > std::numeric_limits<uint16_t>::max())) {
            this->throwError("too many fields in class", String::emptyString, String::emptyString, ErrorObject::RangeError);
        }

        if (privateNames.size() && !this->isParsingSingleFunction) {
            newClassInfo->m_classPrivateNames.resize(privateNames.size());
            for (size_t i = 0; i < privateNames.size(); i++) {
                newClassInfo->m_classPrivateNames[i] = privateNames[i].name;
            }
        }

        this->currentClassInfo = oldClassInfo;

        if (this->currentClassInfo == this->outerClassInfo && !this->isParsingSingleFunction) {
            // test class private field here
            testClassPrivateNameRules(newClassInfo);
        }

        return this->finalize(node, builder.createClassBodyNode(body, constructor));
    }

    template <class ASTBuilder, typename ClassType>
    ASTNode classDeclaration(ASTBuilder& builder, bool isClassExpression)
    {
        bool previousStrict = this->context->strict;
        this->context->strict = true;
        MetaNode startNode = this->createNode();
        this->expectKeyword(ClassKeyword);

        ASTNode idNode = nullptr;
        AtomicString id;

        if (!isClassExpression || this->lookahead.type == Token::IdentifierToken) {
            ALLOC_TOKEN(idToken);
            *idToken = this->lookahead;
            idNode = this->parseVariableIdentifier(builder);
            id = idNode->asIdentifier()->name();

            if (this->sourceType == Module && id.string()->equals("await")) {
                this->throwUnexpectedToken(*idToken, Messages::UnexpectedReserved);
            }
            if (this->scanner->isStrictModeReservedWord(id)) {
                this->throwUnexpectedToken(*idToken, Messages::UnexpectedReserved);
            }
        }

        if (!isClassExpression && id.string()->length()) {
            addDeclaredNameIntoContext(id, this->lexicalBlockIndex, KeywordKind::LetKeyword);
        }

        bool hasSuperClass = false;
        ASTNode superClass = nullptr;
        if (this->matchKeyword(ExtendsKeyword)) {
            hasSuperClass = true;
            this->nextToken();
            superClass = this->isolateCoverGrammar(builder, &Parser::parseLeftHandSideExpressionAllowCall<ASTBuilder>);
        }

        ParserBlockContext classBlockContext;
        openBlock(classBlockContext);
        if (id.string()->length()) {
            addDeclaredNameIntoContext(id, this->lexicalBlockIndex, KeywordKind::ConstKeyword);
        }

        MetaNode endNode;
        ASTNode classBody = this->parseClassBody(builder, hasSuperClass, endNode, id);

        this->context->strict = previousStrict;
        closeBlock(classBlockContext);

        return this->finalize(startNode, builder.template createClass<ClassType>(idNode, superClass, classBody, classBlockContext.childLexicalBlockIndex, StringView(this->scanner->sourceAsNormalView, startNode.index - this->baseMarker.index, endNode.index - this->baseMarker.index)));
    }

    template <class ASTBuilder>
    ASTNode parseClassDeclaration(ASTBuilder& builder)
    {
        return classDeclaration<ASTBuilder, ClassDeclarationNode>(builder, false);
    }

    template <class ASTBuilder>
    ASTNode parseClassExpression(ASTBuilder& builder)
    {
        return classDeclaration<ASTBuilder, ClassExpressionNode>(builder, true);
    }

    // ECMA-262 15.2 Modules
    // ECMA-262 15.2.2 Imports

    void appendToRequestedModulesIfNeeds(String* src)
    {
        for (size_t i = 0; i < this->moduleData->m_requestedModules.size(); i++) {
            if (src->equals(moduleData->m_requestedModules[i])) {
                return;
            }
        }

        this->moduleData->m_requestedModules.pushBack(src);
    }

    template <class ASTBuilder>
    ASTNode parseModuleSpecifier(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        if (this->lookahead.type != Token::StringLiteralToken) {
            this->throwError(Messages::InvalidModuleSpecifier);
        }

        ALLOC_TOKEN(token);
        this->nextToken(token);

        // const raw = this->getTokenRaw(token);
        ASSERT(token->type == Token::StringLiteralToken);
        return this->finalize(node, builder.createLiteralNode(token->valueStringLiteralToValue(this->scanner)));
    }

    // import {<foo as bar>} ...;
    template <class ASTBuilder>
    ASTNode parseImportSpecifier(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        ASTNode local = nullptr;
        ASTNode imported = this->parseIdentifierName(builder);
        if (this->matchContextualKeyword("as")) {
            this->nextToken();
            local = this->parseVariableIdentifier(builder);
        } else {
            local = imported;
        }

        return this->finalize(node, builder.createImportSpecifierNode(local, imported));
    }

    // {foo, bar as bas}
    template <class ASTBuilder>
    ASTNodeList parseNamedImports(ASTBuilder& builder)
    {
        this->expect(PunctuatorKind::LeftBrace);
        ASTNodeList specifiers;
        while (!this->match(PunctuatorKind::RightBrace)) {
            specifiers.append(this->allocator, this->parseImportSpecifier(builder));
            if (!this->match(PunctuatorKind::RightBrace)) {
                this->expect(PunctuatorKind::Comma);
            }
        }
        this->expect(PunctuatorKind::RightBrace);
        return specifiers;
    }

    // import <foo> ...;
    template <class ASTBuilder>
    ASTNode parseImportDefaultSpecifier(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();
        ASTNode local = this->parseIdentifierName(builder);
        return this->finalize(node, builder.createImportDefaultSpecifierNode(local));
    }

    // import <* as foo> ...;
    template <class ASTBuilder>
    ASTNode parseImportNamespaceSpecifier(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        this->expect(PunctuatorKind::Multiply);
        if (!this->matchContextualKeyword("as")) {
            this->throwError(Messages::NoAsAfterImportNamespace);
        }
        this->nextToken();
        ASTNode local = this->parseIdentifierName(builder);
        return this->finalize(node, builder.createImportNamespaceSpecifierNode(local));
    }

    template <class ASTBuilder>
    ASTNode parseImportDeclaration(ASTBuilder& builder)
    {
        Marker startMarker = this->startMarker;
        MetaNode node = this->createNode();
        this->expectKeyword(KeywordKind::ImportKeyword);

        ASTNode src = nullptr;

        Script::ImportEntryVector importEntrys;
        ASTNodeList specifiers;
        if (this->lookahead.type == Token::StringLiteralToken) {
            if (this->sourceType != Module) {
                this->throwUnexpectedToken(this->lookahead, Messages::IllegalImportDeclaration);
            }
            if (this->context->inFunctionBody || this->lexicalBlockIndex != 0) {
                this->throwError(Messages::IllegalImportDeclaration);
            }
            // import 'foo';
            src = this->parseModuleSpecifier(builder);
            appendToRequestedModulesIfNeeds(src->asLiteral()->value().asString());
        } else {
            if (this->match(PunctuatorKind::Period) || this->match(PunctuatorKind::LeftParenthesis)) {
                rewind(startMarker);
                auto expr = this->inheritCoverGrammar(builder, &Parser::parseExpression<ASTBuilder>);
                this->consumeSemicolon();
                return this->finalize(node, builder.createExpressionStatementNode(expr));
            }

            if (this->sourceType != Module) {
                this->throwUnexpectedToken(this->lookahead, Messages::IllegalImportDeclaration);
            }

            if (this->context->inFunctionBody || this->lexicalBlockIndex != 0) {
                this->throwError(Messages::IllegalImportDeclaration);
            }

            if (this->match(PunctuatorKind::LeftBrace)) {
                // import {bar}
                specifiers = this->parseNamedImports(builder);

                if (builder.willGenerateByteCode()) {
                    for (ASTSentinelNode specifier = specifiers.begin(); specifier != specifiers.end(); specifier = specifier->next()) {
                        Script::ImportEntry entry;
                        entry.m_importName = specifier->astNode()->asImportSpecifier()->imported()->name();
                        entry.m_localName = specifier->astNode()->asImportSpecifier()->local()->name();
                        importEntrys.push_back(entry);
                    }
                }
            } else if (this->match(PunctuatorKind::Multiply)) {
                // import * as foo
                specifiers.append(this->allocator, this->parseImportNamespaceSpecifier(builder));

                if (builder.willGenerateByteCode()) {
                    Script::ImportEntry entry;
                    entry.m_importName = this->escargotContext->staticStrings().asciiTable[(unsigned char)'*'];
                    entry.m_localName = specifiers.back()->astNode()->asImportNamespaceSpecifier()->local()->name();
                    importEntrys.push_back(entry);
                }
            } else if (this->isIdentifierName(&this->lookahead) && !this->matchKeyword(KeywordKind::DefaultKeyword)) {
                // import foo
                specifiers.append(this->allocator, this->parseImportDefaultSpecifier(builder));

                if (builder.willGenerateByteCode()) {
                    Script::ImportEntry entry;
                    entry.m_importName = this->escargotContext->staticStrings().stringDefault;
                    entry.m_localName = specifiers.back()->astNode()->asImportDefaultSpecifier()->local()->name();
                    importEntrys.push_back(entry);
                }

                if (this->match(PunctuatorKind::Comma)) {
                    this->nextToken();
                    if (this->match(PunctuatorKind::Multiply)) {
                        // import foo, * as foo
                        specifiers.append(this->allocator, this->parseImportNamespaceSpecifier(builder));

                        if (builder.willGenerateByteCode()) {
                            Script::ImportEntry entry;
                            entry.m_importName = this->escargotContext->staticStrings().asciiTable[(unsigned char)'*'];
                            entry.m_localName = specifiers.back()->astNode()->asImportNamespaceSpecifier()->local()->name();
                            importEntrys.push_back(entry);
                        }
                    } else if (this->match(PunctuatorKind::LeftBrace)) {
                        // import foo, {bar}
                        auto v = this->parseNamedImports(builder);

                        if (builder.willGenerateByteCode()) {
                            for (ASTSentinelNode n = v.begin(); n != v.end(); n = n->next()) {
                                Script::ImportEntry entry;
                                entry.m_importName = n->astNode()->asImportSpecifier()->imported()->name();
                                entry.m_localName = n->astNode()->asImportSpecifier()->local()->name();
                                importEntrys.push_back(entry);
                                specifiers.append(this->allocator, n->astNode());
                            }
                        }
                    } else {
                        this->throwUnexpectedToken(this->lookahead);
                    }
                }
            } else {
                ALLOC_TOKEN(token);
                this->nextToken(token);
                this->throwUnexpectedToken(*token);
            }

            if (!this->matchContextualKeyword("from")) {
                this->throwUnexpectedToken(this->lookahead);
            }
            this->nextToken();
            src = this->parseModuleSpecifier(builder);
            appendToRequestedModulesIfNeeds(src->asLiteral()->value().asString());
        }
        this->consumeSemicolon();

        if (builder.willGenerateByteCode()) {
            for (size_t i = 0; i < importEntrys.size(); i++) {
                importEntrys[i].m_moduleRequest = src->asLiteral()->value().asString();
                addDeclaredNameIntoContext(importEntrys[i].m_localName, this->lexicalBlockIndex, KeywordKind::ConstKeyword);
                this->moduleData->m_importEntries.insert(this->moduleData->m_importEntries.size(), importEntrys[i]);
            }
        }

        return this->finalize(node, builder.createImportDeclarationNode(specifiers, src));
    }

    // ECMA-262 15.2.3 Exports
    template <class ASTBuilder>
    ASTNode parseExportSpecifier(ASTBuilder& builder)
    {
        MetaNode node = this->createNode();

        ASTNode local = this->parseIdentifierName(builder);

        ASTNode exported = local;
        if (this->matchContextualKeyword("as")) {
            this->nextToken();
            exported = this->parseIdentifierName(builder);
        }

        return this->finalize(node, builder.createExportSpecifierNode(local, exported));
    }

    void checkDuplicateExportName(const Script::ExportEntryVector& v, AtomicString exportName)
    {
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].m_exportName.hasValue()) {
                if (v[i].m_exportName.value() == exportName) {
                    throwError("duplicate export name '%s'", exportName.string());
                }
            }
        }
    }

    void addExportDeclarationEntry(Script::ExportEntry ee)
    {
        // http://www.ecma-international.org/ecma-262/6.0/#sec-parsemodule
        // If ee.[[ModuleRequest]] is null, then
        if (!ee.m_moduleRequest.hasValue()) {
            // If ee.[[LocalName]] is not an element of importedBoundNames, then
            size_t localNameExistInImportedBoundNamesIndex = SIZE_MAX;
            for (size_t i = 0; i < this->moduleData->m_importEntries.size(); i++) {
                if (ee.m_localName == this->moduleData->m_importEntries[i].m_localName) {
                    localNameExistInImportedBoundNamesIndex = i;
                    break;
                }
            }

            if (localNameExistInImportedBoundNamesIndex == SIZE_MAX) {
                // Append ee to localExportEntries.
                this->moduleData->m_localExportEntries.push_back(ee);
            } else {
                // Let ie be the element of importEntries whose [[LocalName]] is the same as ee.[[LocalName]].
                Script::ImportEntry ie = this->moduleData->m_importEntries[localNameExistInImportedBoundNamesIndex];
                // If ie.[[ImportName]] is "*", then
                if (ie.m_importName == this->escargotContext->staticStrings().asciiTable[(unsigned char)'*']) {
                    // Assert: this is a re-export of an imported module namespace object.
                    // Append ee to localExportEntries.
                    this->moduleData->m_localExportEntries.push_back(ee);
                } else {
                    // Else, this is a re-export of a single name
                    // Append to indirectExportEntries the Record {[[ModuleRequest]]: ie.[[ModuleRequest]], [[ImportName]]: ie.[[ImportName]], [[LocalName]]: null, [[ExportName]]: ee.[[ExportName]] }.
                    Script::ExportEntry newEntry;
                    newEntry.m_moduleRequest = ie.m_moduleRequest;
                    newEntry.m_importName = ie.m_importName;
                    newEntry.m_exportName = ee.m_exportName;

                    this->moduleData->m_indirectExportEntries.push_back(newEntry);

                    appendToRequestedModulesIfNeeds(newEntry.m_moduleRequest.value());
                }
            }
        } else if (ee.m_importName == this->escargotContext->staticStrings().asciiTable[(unsigned char)'*']) {
            // Else, if ee.[[ImportName]] is "*", then
            this->moduleData->m_starExportEntries.push_back(ee);
            appendToRequestedModulesIfNeeds(ee.m_moduleRequest.value());
        } else {
            // Append ee to indirectExportEntries.
            this->moduleData->m_indirectExportEntries.push_back(ee);
            appendToRequestedModulesIfNeeds(ee.m_moduleRequest.value());
        }
    }

    template <class ASTBuilder>
    ASTNode parseExportDeclaration(ASTBuilder& builder)
    {
        if (this->context->inFunctionBody) {
            this->throwError(Messages::IllegalExportDeclaration);
        }

        if (this->lexicalBlockIndex != 0) {
            this->throwError(Messages::IllegalExportDeclaration);
        }

        MetaNode node = this->createNode();
        this->expectKeyword(KeywordKind::ExportKeyword);

        ASTNode exportDeclaration = nullptr;
        if (this->matchKeyword(KeywordKind::DefaultKeyword)) {
            // export default ...
            if (this->lookahead.end - this->lookahead.start != 7) {
                this->throwUnexpectedToken(this->lookahead);
            }

            this->nextToken();
            if (this->matchKeyword(KeywordKind::FunctionKeyword)) {
                // export default function foo () {}
                // export default function () {}
                ASTNode declaration = this->parseFunctionDeclaration<ASTBuilder, true>(builder);

                if (builder.willGenerateByteCode()) {
                    Script::ExportEntry entry;
                    entry.m_exportName = this->escargotContext->staticStrings().stringDefault;
                    AtomicString fnName = declaration->asFunctionDeclaration()->functionName();
                    entry.m_localName = fnName.string()->length() ? fnName : this->escargotContext->staticStrings().stringStarDefaultStar;
                    // addDeclaredNameIntoContext(entry.m_localName.value(), this->lexicalBlockIndex, KeywordKind::LetKeyword);
                    checkDuplicateExportName(this->moduleData->m_localExportEntries, entry.m_exportName.value());
                    addExportDeclarationEntry(entry);
                    exportDeclaration = this->finalize(node, builder.createExportDefaultDeclarationNode(declaration, entry.m_exportName.value(), entry.m_localName.value()));
                }
            } else if (this->matchKeyword(KeywordKind::ClassKeyword)) {
                // export default class foo {}
                auto classNode = this->parseClassExpression(builder);

                if (builder.willGenerateByteCode()) {
                    this->addImplicitName(classNode, this->escargotContext->staticStrings().stringDefault);

                    Script::ExportEntry entry;
                    entry.m_exportName = this->escargotContext->staticStrings().stringDefault;
                    entry.m_localName = classNode->asClassExpression()->classNode().id() ? classNode->asClassExpression()->classNode().id()->asIdentifier()->name() : this->escargotContext->staticStrings().stringStarDefaultStar;
                    addDeclaredNameIntoContext(entry.m_localName.value(), this->lexicalBlockIndex, KeywordKind::LetKeyword);
                    checkDuplicateExportName(this->moduleData->m_localExportEntries, entry.m_exportName.value());
                    addExportDeclarationEntry(entry);

                    exportDeclaration = this->finalize(node, builder.createExportDefaultDeclarationNode(classNode, entry.m_exportName.value(), entry.m_localName.value()));
                }
            } else if (this->matchContextualKeyword("async")) {
                // TODO
                this->throwUnexpectedToken(this->lookahead, "async keyword in export is not supported yet");
            } else {
                if (this->matchContextualKeyword("from")) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                // export default {};
                // export default [];
                // export default (1 + 2);
                Script::ExportEntry entry;
                entry.m_exportName = this->escargotContext->staticStrings().stringDefault;
                entry.m_localName = this->escargotContext->staticStrings().stringStarDefaultStar;

                ASTNode declaration;
                if (this->match(PunctuatorKind::LeftBrace)) {
                    declaration = this->parseObjectInitializer(builder);
                } else if (this->match(PunctuatorKind::LeftSquareBracket)) {
                    declaration = this->parseArrayInitializer(builder);
                } else {
                    declaration = this->parseAssignmentExpression<ASTBuilder, true>(builder);
                    this->addImplicitName(declaration, this->escargotContext->staticStrings().stringDefault);
                }
                exportDeclaration = this->finalize(node, builder.createExportDefaultDeclarationNode(declaration, entry.m_exportName.value(), entry.m_localName.value()));

                addDeclaredNameIntoContext(entry.m_localName.value(), this->lexicalBlockIndex, KeywordKind::LetKeyword);
                checkDuplicateExportName(this->moduleData->m_localExportEntries, entry.m_exportName.value());
                addExportDeclarationEntry(entry);

                this->consumeSemicolon();
            }

        } else if (this->match(PunctuatorKind::Multiply)) {
            this->nextToken();
            if (this->matchContextualKeyword("from")) {
                // export * from 'foo';
                this->nextToken();
                if (this->lookahead.type != Token::StringLiteralToken) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                ASTNode src = this->parseModuleSpecifier(builder);

                if (builder.willGenerateByteCode()) {
                    Script::ExportEntry entry;
                    entry.m_exportName = this->escargotContext->staticStrings().stringDefault;
                    entry.m_moduleRequest = src->asLiteral()->value().asString();
                    entry.m_importName = this->escargotContext->staticStrings().asciiTable[(unsigned char)'*'];
                    addExportDeclarationEntry(entry);
                }

                exportDeclaration = this->finalize(node, builder.createExportAllDeclarationNode(src));
            } else if (this->matchContextualKeyword("as")) {
                // export * as ns from "mod";
                this->nextToken();

                if (this->lookahead.type != Token::IdentifierToken) {
                    this->throwUnexpectedToken(this->lookahead);
                }

                ASTNode idNode;
                AtomicString name;
                {
                    TrackUsingNameBlocker blocker(this);
                    idNode = finishIdentifier(builder, &this->lookahead);
                    name = idNode->asIdentifier()->name();
                }
                this->nextToken();

                if (!this->matchContextualKeyword("from")) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                this->nextToken();

                ASTNode src = this->parseModuleSpecifier(builder);

                if (builder.willGenerateByteCode()) {
                    Script::ExportEntry entry;
                    entry.m_exportName = name;
                    entry.m_moduleRequest = src->asLiteral()->value().asString();
                    entry.m_importName = this->escargotContext->staticStrings().asciiTable[(unsigned char)'*'];
                    addDeclaredNameIntoContext(this->escargotContext->staticStrings().stringStarNamespaceStar, this->lexicalBlockIndex, KeywordKind::LetKeyword);
                    this->moduleData->m_indirectExportEntries.push_back(entry);
                    appendToRequestedModulesIfNeeds(src->asLiteral()->value().asString());
                }

                exportDeclaration = this->finalize(node, builder.createExportStarAsNamedFromDeclarationNode(idNode, src));
            } else {
                this->throwUnexpectedToken(this->lookahead);
            }
            this->consumeSemicolon();
        } else if (this->lookahead.type == Token::KeywordToken) {
            // export var f = 1;
            auto oldNameCallback = this->nameDeclaredCallback;
            AtomicStringVector declaredNames;
            if (builder.willGenerateByteCode()) {
                this->nameDeclaredCallback = [&declaredNames](AtomicString name, LexicalBlockIndex, KeywordKind, bool) {
                    for (size_t i = 0; i < declaredNames.size(); i++) {
                        if (declaredNames[i] == name) {
                            return;
                        }
                    }
                    declaredNames.push_back(name);
                };
            }
            AtomicStringVector declaredName;
            ASTNode declaration = nullptr;
            switch (this->lookahead.valueKeywordKind) {
            case KeywordKind::LetKeyword:
            case KeywordKind::ConstKeyword:
                declaration = this->parseLexicalDeclaration(builder, false);
                break;
            case KeywordKind::VarKeyword:
            case KeywordKind::ClassKeyword:
            case KeywordKind::FunctionKeyword:
                declaration = this->parseStatementListItem(builder);
                break;
            default:
                this->throwUnexpectedToken(this->lookahead);
                break;
            }

            for (size_t i = 0; i < declaredNames.size(); i++) {
                Script::ExportEntry entry;
                entry.m_exportName = declaredNames[i];
                entry.m_localName = declaredNames[i];

                checkDuplicateExportName(this->moduleData->m_localExportEntries, entry.m_exportName.value());
                addExportDeclarationEntry(entry);
            }
            exportDeclaration = this->finalize(node, builder.createExportNamedDeclarationNode(declaration, ASTNodeList(), nullptr));
            this->nameDeclaredCallback = oldNameCallback;
        } else if (this->matchAsyncFunction()) {
            ASTNode declaration = this->parseFunctionDeclaration(builder);
            exportDeclaration = this->finalize(node, builder.createExportNamedDeclarationNode(declaration, ASTNodeList(), nullptr));
        } else {
            ASTNodeList specifiers;
            ASTNode source = nullptr;
            bool isExportFromIdentifier = false;

            this->expect(PunctuatorKind::LeftBrace);
            while (!this->match(PunctuatorKind::RightBrace)) {
                isExportFromIdentifier = isExportFromIdentifier || this->matchKeyword(KeywordKind::DefaultKeyword);
                specifiers.append(this->allocator, this->parseExportSpecifier(builder));
                if (!this->match(PunctuatorKind::RightBrace)) {
                    this->expect(PunctuatorKind::Comma);
                }
            }
            this->expect(PunctuatorKind::RightBrace);

            bool seenFrom = false;
            if (this->matchContextualKeyword("from")) {
                seenFrom = true;
                // export {default} from 'foo';
                // export {foo} from 'foo';
                this->nextToken();
                source = this->parseModuleSpecifier(builder);
                appendToRequestedModulesIfNeeds(source->asLiteral()->value().asString());
                this->consumeSemicolon();
            } else if (isExportFromIdentifier) {
                // export {default}; // missing fromClause
                this->throwUnexpectedToken(this->lookahead);
            } else {
                // export {foo};
                this->consumeSemicolon();
            }

            if (builder.willGenerateByteCode()) {
                for (ASTSentinelNode specifier = specifiers.begin(); specifier != specifiers.end(); specifier = specifier->next()) {
                    Script::ExportEntry entry;
                    if (seenFrom) {
                        entry.m_exportName = specifier->astNode()->asExportSpecifier()->exported()->name();
                        entry.m_importName = specifier->astNode()->asExportSpecifier()->local()->name();
                        entry.m_moduleRequest = source->asLiteral()->value().asString();
                    } else {
                        entry.m_exportName = specifier->astNode()->asExportSpecifier()->exported()->name();
                        entry.m_localName = specifier->astNode()->asExportSpecifier()->local()->name();
                    }

                    checkDuplicateExportName(this->moduleData->m_localExportEntries, entry.m_exportName.value());
                    addExportDeclarationEntry(entry);
                }
            }

            exportDeclaration = this->finalize(node, builder.createExportNamedDeclarationNode(nullptr, specifiers, source));
        }

        return exportDeclaration;
    }

    ProgramNode* parseProgram(NodeGenerator& builder)
    {
        pushScopeContext(new (this->allocator) ASTScopeContext(this->allocator, this->context->strict));

        ParserBlockContext blockContext;
        openBlock(blockContext);

        this->context->allowLexicalDeclaration = true;
        StatementContainer* container = builder.createStatementContainer();
        this->parseDirectivePrologues(builder, container);
        StatementNode* referNode = nullptr;
        while (this->startMarker.index < this->scanner->length) {
            referNode = container->appendChild(this->parseStatementListItem(builder), referNode);
        }

        closeBlock(blockContext);

        MetaNode endNode = this->createNode();
        this->currentScopeContext->m_bodyEndLOC.index = endNode.index;
#if !(defined NDEBUG)
        this->currentScopeContext->m_bodyEndLOC.line = endNode.line;
        this->currentScopeContext->m_bodyEndLOC.column = endNode.column;
#endif
        ProgramNode* programNode = builder.createProgramNode(container, this->currentScopeContext, this->moduleData, std::move(this->numeralLiteralVector));

        if (UNLIKELY(this->outerClassInfo != nullptr)) {
            testClassPrivateNameRules(this->outerClassInfo);
        }

        return this->finalize(endNode, programNode);
    }

    FunctionNode* parseScriptFunction(NodeGenerator& builder)
    {
        ASSERT(this->isParsingSingleFunction);

        size_t squareBraketCount = 0;
        while (true) {
            bool isPunctuator = this->lookahead.type == Token::PunctuatorToken;
            if (isPunctuator) {
                if (this->lookahead.valuePunctuatorKind == PunctuatorKind::LeftParenthesis && squareBraketCount == 0) {
                    break;
                } else if (this->lookahead.valuePunctuatorKind == PunctuatorKind::LeftSquareBracket) {
                    squareBraketCount++;
                } else if (this->lookahead.valuePunctuatorKind == PunctuatorKind::RightSquareBracket) {
                    squareBraketCount--;
                }
            }
            this->nextToken();
        }

        const bool previousAllowNewTarget = context->allowNewTarget;
        context->allowNewTarget = true;
        MetaNode node = this->createNode();
        StatementContainer* params = this->parseFunctionParameters(builder);
        BlockStatementNode* body = this->parseFunctionBody(builder);

        context->allowNewTarget = previousAllowNewTarget;
        return this->finalize(node, builder.createFunctionNode(params, body, std::move(this->numeralLiteralVector)));
    }

    FunctionNode* parseScriptArrowFunction(NodeGenerator& builder)
    {
        ASSERT(this->isParsingSingleFunction);

        MetaNode node = this->createNode();
        StatementContainer* params;
        BlockStatementNode* body;

        if (this->codeBlock->isAsync()) {
            this->nextToken();
        }

        if (UNLIKELY(this->codeBlock->isOneExpressionOnlyVirtualArrowFunctionExpression())) {
            params = builder.createStatementContainer();

            if (this->codeBlock->parameterNames().size()) {
                // static field initialization case
                // add a virtual argument for class constructor
                ASSERT(this->codeBlock->parameterNames().size() == 1);

                Node* param = builder.createIdentifierNode(this->codeBlock->parameterNames()[0]);
                InitializeParameterExpressionNode* init = this->finalize(node, builder.createInitializeParameterExpressionNode(param, 0));
                params->appendChild(this->finalize(node, builder.createExpressionStatementNode(init)));
            }

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
            this->context->allowYield = false;

            MetaNode nodeStart = this->createNode();
            Node* expr = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<NodeGenerator, false>);

            StatementContainer* container = builder.createStatementContainer();
            Node* node = this->finalize(nodeStart, builder.createReturnStatementNode(expr));
            container->appendChild(node, nullptr);
            body = this->finalize(nodeStart, builder.createBlockStatementNode(container));

            this->context->strict = previousStrict;
            this->context->allowYield = previousAllowYield;

            this->context->labelSet = previousLabelSet;
            this->context->inIteration = previousInIteration;
            this->context->inSwitch = previousInSwitch;
            this->context->inFunctionBody = previousInFunctionBody;
        } else {
            // generate parameter statements
            if (this->codeBlock->hasArrowParameterPlaceHolder()) {
                params = parseFunctionParameters(builder);
            } else {
                Node* param = this->parseConditionalExpression(builder);
                ASSERT(param->type() == Identifier);
                params = builder.createStatementContainer();
                InitializeParameterExpressionNode* init = this->finalize(node, builder.createInitializeParameterExpressionNode(param, 0));
                Node* statement = this->finalize(node, builder.createExpressionStatementNode(init));
                params->appendChild(statement);
            }
            this->expect(Arrow);
            this->context->inArrowFunction = true;

            // generate body statements
            if (this->match(LeftBrace)) {
                body = parseFunctionBody(builder);
            } else {
                MetaNode nodeStart = this->createNode();
                StatementContainer* container = builder.createStatementContainer();

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

                Node* expr = this->isolateCoverGrammar(builder, &Parser::parseAssignmentExpression<NodeGenerator, false>);

                Node* node = this->finalize(nodeStart, builder.createReturnStatementNode(expr));
                container->appendChild(node, nullptr);

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

                body = this->finalize(nodeStart, builder.createBlockStatementNode(container));
            }
        }

        return this->finalize(node, builder.createFunctionNode(params, body, std::move(this->numeralLiteralVector)));
    }
};

ProgramNode* parseProgram(::Escargot::Context* ctx, StringView source, ASTClassInfo* outerClassInfo, bool isModule, bool strictFromOutside,
                          bool inWith, size_t stackRemain, bool allowSuperCallFromOutside, bool allowSuperPropertyFromOutside, bool allowNewTargetFromOutside, bool allowArgumentsFromOutside)
{
    // GC should be disabled during the parsing process
    ASSERT(GC_is_disabled());

    Parser parser(ctx, source, outerClassInfo, isModule, stackRemain);
    NodeGenerator builder(ctx->astAllocator());

    parser.context->strict |= strictFromOutside;
    parser.context->inWith = inWith;
    parser.context->allowSuperCall = allowSuperCallFromOutside;
    parser.context->allowSuperProperty = allowSuperPropertyFromOutside;
    parser.context->allowNewTarget = allowNewTargetFromOutside;
    parser.context->allowArguments = allowArgumentsFromOutside;

    ProgramNode* nd = parser.parseProgram(builder);
    return nd;
}

FunctionNode* parseSingleFunction(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock, size_t stackRemain)
{
    // GC should be disabled during the parsing process
    ASSERT(GC_is_disabled());
    ASSERT(ctx->astAllocator().isInitialized());

    Parser parser(ctx, codeBlock->src(), nullptr, codeBlock->script()->isModule(), stackRemain, codeBlock->functionStart());
    NodeGenerator builder(ctx->astAllocator());

    parser.trackUsingNames = false;
    parser.context->inFunctionBody = true;
    parser.context->allowLexicalDeclaration = true;
    parser.context->allowSuperCall = true;
    parser.context->allowSuperProperty = true;
    parser.context->allowNewTarget = true;
    parser.context->allowYield = codeBlock->isGenerator();
    parser.context->await = codeBlock->isAsync();

    parser.isParsingSingleFunction = true;
    parser.codeBlock = codeBlock;

    ASTScopeContext* scopeContext = new (ctx->astAllocator()) ASTScopeContext(ctx->astAllocator(), codeBlock->isStrict());
    parser.pushScopeContext(scopeContext);
    parser.currentScopeContext->m_functionBodyBlockIndex = codeBlock->functionBodyBlockIndex();

    Parser::ParserBlockContext blockContext;
    parser.openBlock(blockContext);

    parser.lexicalBlockIndex = 0;
    parser.lexicalBlockCount = 0;

    if (codeBlock->isArrowFunctionExpression()) {
        return parser.parseScriptArrowFunction(builder);
    }
    return parser.parseScriptFunction(builder);
}

ASTClassInfo* generateClassInfoFrom(::Escargot::Context* ctx, InterpretedCodeBlock* cb)
{
    ASTClassInfo* ret = nullptr;
    ASTClassInfo* currentClassInfo = nullptr;
    InterpretedCodeBlock* c = cb;
    while (c) {
        auto privateNames = c->classPrivateNames();
        if (privateNames) {
            ASTClassInfo* newInfo = new (ctx->astAllocator()) ASTClassInfo();
            if (privateNames->size()) {
                newInfo->m_classPrivateNames.resizeWithUninitializedValues(privateNames->size());
                memcpy(newInfo->m_classPrivateNames.data(), privateNames->data(), sizeof(AtomicString) * privateNames->size());
            }
            if (currentClassInfo) {
                currentClassInfo->m_parent = newInfo;
                currentClassInfo = newInfo;
            } else {
                ret = currentClassInfo = newInfo;
            }
        }
        c = c->parent();
    }
    return ret;
}

} // namespace esprima
} // namespace Escargot
