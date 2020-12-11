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

#ifndef __EscargotLexer__
#define __EscargotLexer__

#include "parser/esprima_cpp/esprima.h"
#include "parser/ParserStringView.h"

namespace Escargot {

namespace esprima {
struct ParserContext;
}

namespace EscargotLexer {

enum Token {
    EOFToken,
    IdentifierToken,
    BooleanLiteralToken,
    KeywordToken,
    NullLiteralToken,
    NumericLiteralToken,
    PunctuatorToken,
    StringLiteralToken,
    RegularExpressionToken,
    TemplateToken,
    InvalidToken
};

enum PunctuatorKind : uint8_t {
    LeftParenthesis,
    RightParenthesis,
    LeftBrace,
    RightBrace,
    Period,
    PeriodPeriodPeriod,
    Comma,
    Colon,
    SemiColon,
    LeftSquareBracket,
    RightSquareBracket,
    GuessMark,
    GuessDot,
    NullishCoalescing,
    Wave,
    UnsignedRightShift,
    RightShift,
    LeftShift,
    Plus,
    Minus,
    Multiply,
    Exponentiation,
    Divide,
    Mod,
    ExclamationMark,
    StrictEqual,
    NotStrictEqual,
    Equal,
    NotEqual,
    LogicalAnd,
    LogicalOr,
    PlusPlus,
    MinusMinus,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    LeftInequality,
    RightInequality,
    InPunctuator,
    InstanceOfPunctuator,

    Substitution,
    UnsignedRightShiftEqual,
    RightShiftEqual,
    LeftShiftEqual,
    PlusEqual,
    MinusEqual,
    MultiplyEqual,
    DivideEqual,
    ModEqual,
    // ExclamationMarkEqual,
    BitwiseAndEqual,
    BitwiseOrEqual,
    BitwiseXorEqual,
    LeftInequalityEqual,
    RightInequalityEqual,
    ExponentiationEqual,
    SubstitutionEnd,

    Arrow,
    PunctuatorKindEnd,
};

enum KeywordKind : uint8_t {
    NotKeyword,
    IfKeyword,
    InKeyword,
    DoKeyword,
    VarKeyword,
    ForKeyword,
    NewKeyword,
    TryKeyword,
    ThisKeyword,
    ElseKeyword,
    CaseKeyword,
    VoidKeyword,
    WithKeyword,
    EnumKeyword,
    WhileKeyword,
    BreakKeyword,
    CatchKeyword,
    ThrowKeyword,
    ConstKeyword,
    ClassKeyword,
    SuperKeyword,
    ReturnKeyword,
    TypeofKeyword,
    DeleteKeyword,
    SwitchKeyword,
    ExportKeyword,
    ImportKeyword,
    DefaultKeyword,
    FinallyKeyword,
    ExtendsKeyword,
    FunctionKeyword,
    ContinueKeyword,
    DebuggerKeyword,
    InstanceofKeyword,
    StrictModeReservedWord,
    ImplementsKeyword,
    InterfaceKeyword,
    PackageKeyword,
    PrivateKeyword,
    ProtectedKeyword,
    PublicKeyword,
    StaticKeyword,
    YieldKeyword,
    LetKeyword,
    KeywordKindEnd
};

enum {
    LexerIsCharIdentStart = (1 << 0),
    LexerIsCharIdent = (1 << 1),
    LexerIsCharWhiteSpace = (1 << 2),
    LexerIsCharLineTerminator = (1 << 3)
};

extern char g_asciiRangeCharMap[128];

// ECMA-262 11.2 White Space
NEVER_INLINE bool isWhiteSpaceSlowCase(char16_t ch);
ALWAYS_INLINE bool isWhiteSpace(char16_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & LexerIsCharWhiteSpace;
    }
    return isWhiteSpaceSlowCase(ch);
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & LexerIsCharLineTerminator;
    }
    return UNLIKELY(ch == 0x2028 || ch == 0x2029);
}

ALWAYS_INLINE bool isWhiteSpaceOrLineTerminator(char16_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & (LexerIsCharWhiteSpace | LexerIsCharLineTerminator);
    }
    return UNLIKELY(ch == 0x2028 || ch == 0x2029 || isWhiteSpaceSlowCase(ch));
}

struct ScanTemplateResult {
    // ScanTemplateResult is allocated on the GC heap
    // because it holds GC pointers, raw and member pointer of valueCooked.
    inline void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new[](size_t size) = delete;

    Optional<UTF16StringData> valueCooked;
    UTF16StringData valueRaw;
    bool head;
    bool tail;
    Optional<esprima::Error*> error;
};

struct ScanRegExpResult {
    ScanRegExpResult()
        : body(nullptr)
        , flags(nullptr)
    {
    }
    String* body;
    String* flags;
};

class ErrorHandler {
public:
    ErrorHandler()
    {
    }

    static void throwError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code);
};

class Messages {
public:
    static constexpr const char* InvalidHexEscapeSequence = "Invalid hexadecimal escape sequence";
    static constexpr const char* UnexpectedTokenIllegal = "Unexpected token ILLEGAL";
    static constexpr const char* UnterminatedRegExp = "Invalid regular expression: missing /";
    static constexpr const char* TemplateOctalLiteral = "Octal literals are not allowed in template strings.";
    static constexpr const char* KeywordMustNotContainEscapedCharacters = "Keyword must not contain escaped characters";
};

class Scanner {
public:
    struct ScannerSourceStringView {
        union {
            String* m_stringIfNewlyAllocated;
            size_t m_start;
        };
        size_t m_end;
    };

    class ScannerResult {
    public:
        ScannerResult()
            : type(InvalidToken)
            , startWithZero(false)
            , octal(false)
            , hasAllocatedString(false)
            , hasNonComputedNumberLiteral(false)
            , hasNumberSeparatorOnNumberLiteral(false)
            , prec(0)
            , lineNumber(0)
            , lineStart(0)
            , start(0)
            , end(0)
            , valueRegExp()
        {
        }

        // ScannerResult always allocated on the stack
        MAKE_STACK_ALLOCATED();

        unsigned char type : 4;
        bool startWithZero : 1;
        bool octal : 1;
        bool hasAllocatedString : 1;
        bool hasNonComputedNumberLiteral : 1;
        bool hasNumberSeparatorOnNumberLiteral : 1;
        unsigned char prec : 8; // max prec is 11
        // we don't needs init prec.
        // prec is initialized by another function before use

        size_t lineNumber;
        size_t lineStart;
        size_t start;
        size_t end;

        union {
            PunctuatorKind valuePunctuatorKind;
            ScannerSourceStringView valueStringLiteralData;
            double valueNumber;
            ScanTemplateResult* valueTemplate;
            ScanRegExpResult valueRegExp;
            KeywordKind valueKeywordKind;
        };

        ParserStringView relatedSource(const ParserStringView& source);
        StringView relatedSource(const StringView& source);
        ParserStringView valueStringLiteral(Scanner* scannerInstance);
        Value valueStringLiteralToValue(Scanner* scannerInstance);
        // returns <Value, isBigInt>
        std::pair<Value, bool> valueNumberLiteral(Scanner* scannerInstance);

        inline operator bool() const
        {
            return this->type != InvalidToken;
        }

        inline void reset()
        {
            this->type = InvalidToken;
        }

        void setResult(Token type, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            this->type = type;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueNumber = 0;
        }

        void setPunctuatorResult(size_t lineNumber, size_t lineStart, size_t start, size_t end, PunctuatorKind p)
        {
            this->type = Token::PunctuatorToken;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valuePunctuatorKind = p;
        }

        void setKeywordResult(size_t lineNumber, size_t lineStart, size_t start, size_t end, KeywordKind p)
        {
            this->type = Token::KeywordToken;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueKeywordKind = p;
        }

        void setResult(Token type, String* s, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool octal = false)
        {
            this->type = type;
            this->startWithZero = false;
            this->octal = octal;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;

            this->hasAllocatedString = true;
            this->valueStringLiteralData.m_stringIfNewlyAllocated = s;
        }

        void setResult(Token type, size_t stringStart, size_t stringEnd, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool octal = false)
        {
            this->type = type;
            this->startWithZero = false;
            this->octal = octal;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;

            this->hasAllocatedString = false;
            this->valueStringLiteralData.m_start = stringStart;
            this->valueStringLiteralData.m_end = stringEnd;
            ASSERT(this->valueStringLiteralData.m_start <= this->valueStringLiteralData.m_end);
        }

        void setNumericLiteralResult(double value, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool hasNonComputedNumberLiteral, bool hasNumberSeparatorOnNumberLiteral)
        {
            this->type = Token::NumericLiteralToken;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = hasNonComputedNumberLiteral;
            this->hasNumberSeparatorOnNumberLiteral = hasNumberSeparatorOnNumberLiteral;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueNumber = value;
        }

        void setTemplateTokenResult(ScanTemplateResult* value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            this->type = Token::TemplateToken;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueTemplate = value;
        }

    private:
        void constructStringLiteral(Scanner* scannerInstance);
        void constructStringLiteralHelperAppendUTF16(Scanner* scannerInstance, char16_t ch, UTF16StringDataNonGCStd& stringUTF16, bool& isEveryCharLatin1);
    };

    class SmallScannerResult {
    public:
        SmallScannerResult()
            : type(InvalidToken)
            , prec(0)
            , lineNumber(0)
            , lineStart(0)
            , start(0)
            , end(0)
        {
        }

        SmallScannerResult(const ScannerResult& scan)
            : type(scan.type)
            , prec(scan.prec)
            , lineNumber(scan.lineNumber)
            , lineStart(scan.lineStart)
            , start(scan.start)
            , end(scan.end)
            , valueKeywordKind(scan.valueKeywordKind)
        {
        }

        ~SmallScannerResult() {}
        // SmallScannerResult always allocated on the stack
        MAKE_STACK_ALLOCATED();

        static void* operator new(size_t, void* p) { return p; } // for VectorWithInlineStorage
        unsigned char type : 4;
        char prec : 8; // max prec is 11

        size_t lineNumber;
        size_t lineStart;
        size_t start;
        size_t end;

        union {
            PunctuatorKind valuePunctuatorKind;
            KeywordKind valueKeywordKind;
        };

        inline operator bool() const
        {
            return this->type != InvalidToken;
        }

        inline void reset()
        {
            this->type = InvalidToken;
        }

        ParserStringView relatedSource(const ParserStringView& source) const;
        StringView relatedSource(const StringView& source) const;
    };

    // ScannerResult should be allocated on the stack by ALLOCA
    COMPILE_ASSERT(sizeof(ScannerResult) < 512, "");

    ParserStringView source;
    StringView sourceAsNormalView;
    ::Escargot::Context* escargotContext;
    ::Escargot::esprima::ParserContext* parserContext;
    StringBufferAccessData sourceCodeAccessData;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;

    ~Scanner()
    {
    }

    Scanner(::Escargot::Context* escargotContext, ::Escargot::esprima::ParserContext* parserContext, StringView code, size_t startLine = 0, size_t startColumn = 0);

    // Scanner always allocated on the stack
    MAKE_STACK_ALLOCATED();

    bool eof()
    {
        return index >= length;
    }

    ALWAYS_INLINE void throwUnexpectedToken(const char* message = Messages::UnexpectedTokenIllegal)
    {
        ErrorHandler::throwError(this->index, this->lineNumber, this->index - this->lineStart + 1, new ASCIIString(message), ErrorObject::SyntaxError);
    }

    ALWAYS_INLINE char16_t sourceCharAt(const size_t idx) const
    {
        return sourceCodeAccessData.charAt(idx);
    }

    // ECMA-262 11.4 Comments

    void skipSingleLineComment(void);
    void skipMultiLineComment(void);

    ALWAYS_INLINE void scanComments()
    {
        bool start = (this->index == 0);
        while (LIKELY(!this->eof())) {
            char16_t ch = this->sourceCharAt(this->index);

            if (isWhiteSpace(ch)) {
                ++this->index;
            } else if (isLineTerminator(ch)) {
                ++this->index;
                if (ch == 0x0D && this->sourceCharAt(this->index) == 0x0A) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                start = true;
            } else if (ch == 0x2F) { // U+002F is '/'
                ch = this->sourceCharAt(this->index + 1);
                if (ch == 0x2F) {
                    this->index += 2;
                    this->skipSingleLineComment();
                    start = true;
                } else if (ch == 0x2A) { // U+002A is '*'
                    this->index += 2;
                    this->skipMultiLineComment();
                } else {
                    break;
                }
            } else if (start && ch == 0x2D) { // U+002D is '-'
                // U+003E is '>'
                if ((this->sourceCharAt(this->index + 1) == 0x2D) && (this->sourceCharAt(this->index + 2) == 0x3E)) {
                    // '-->' is a single-line comment
                    this->index += 3;
                    this->skipSingleLineComment();
                } else {
                    break;
                }
            } else if (ch == 0x3C) { // U+003C is '<'
                if (this->length > this->index + 4) {
                    if (this->sourceCharAt(this->index + 1) == '!'
                        && this->sourceCharAt(this->index + 2) == '-'
                        && this->sourceCharAt(this->index + 3) == '-') {
                        this->index += 4; // `<!--`
                        this->skipSingleLineComment();
                    } else {
                        break;
                    }
                } else {
                    break;
                }

            } else {
                break;
            }
        }
    }

    bool isFutureReservedWord(const ParserStringView& id);

    void convertToKeywordInStrictMode(ScannerResult* token)
    {
        ASSERT(token->type == Token::IdentifierToken);

        const auto& keyword = token->relatedSource(this->source);
        if (keyword.equals("let")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::LetKeyword);
        } else if (keyword.equals("yield")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::YieldKeyword);
        } else if (keyword.equals("static")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::StaticKeyword);
        } else if (keyword.equals("public")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::PublicKeyword);
        } else if (keyword.equals("private")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::PrivateKeyword);
        } else if (keyword.equals("package")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::PackageKeyword);
        } else if (keyword.equals("protected")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::ProtectedKeyword);
        } else if (keyword.equals("interface")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::InterfaceKeyword);
        } else if (keyword.equals("implements")) {
            token->setKeywordResult(token->lineNumber, token->lineStart, token->start, token->end, KeywordKind::ImplementsKeyword);
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    template <typename T>
    bool isStrictModeReservedWord(const T& id)
    {
        const StringBufferAccessData& data = id.bufferAccessData();
        switch (data.length) {
        case 3: // let
            return data.equalsSameLength("let");
        case 5: // yield
            return data.equalsSameLength("yield");
        case 6: // static public
            return data.equalsSameLength("static") || data.equalsSameLength("public");
        case 7: // private package
            return data.equalsSameLength("private") || data.equalsSameLength("package");
        case 9: // protected interface
            return data.equalsSameLength("protected") || data.equalsSameLength("interface");
        case 10: // implements
            return data.equalsSameLength("implements");
        }

        return false;
    }

    template <typename T>
    bool isRestrictedWord(const T& id)
    {
        const StringBufferAccessData& data = id.bufferAccessData();
        if (data.length == 4) {
            return data.equalsSameLength("eval");
        } else if (data.length == 9) {
            return data.equalsSameLength("arguments");
        } else {
            return false;
        }
    }

    char32_t codePointAt(size_t i)
    {
        char32_t cp, first, second;
        cp = this->sourceCharAt(i);
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            second = this->sourceCharAt(i + 1);
            if (second >= 0xDC00 && second <= 0xDFFF) {
                first = cp;
                cp = (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000;
            }
        }

        return cp;
    }

    // ECMA-262 11.8.6 Template Literal Lexical Components
    void scanTemplate(Scanner::ScannerResult* token, bool head = false);

    // ECMA-262 11.8.5 Regular Expression Literals
    void scanRegExp(Scanner::ScannerResult* token);

    void lex(Scanner::ScannerResult* token);

private:
    ALWAYS_INLINE char16_t peekCharWithoutEOF()
    {
        return this->sourceCharAt(this->index);
    }

    ALWAYS_INLINE char16_t peekChar()
    {
        return UNLIKELY(this->eof()) ? 0 : this->sourceCharAt(this->index);
    }

    char32_t scanHexEscape(char prefix);
    char32_t scanUnicodeCodePointEscape();

    uint16_t octalToDecimal(char16_t ch, bool octal);

    typedef std::tuple<StringBufferAccessData, String*> ScanIDResult;
    ScanIDResult getIdentifier();
    ScanIDResult getComplexIdentifier();

    // ECMA-262 11.7 Punctuators
    void scanPunctuator(Scanner::ScannerResult* token, char16_t ch0);

    // ECMA-262 11.8.3 Numeric Literals
    void testNumericSeparator(size_t start, bool isBigInt, bool isHex, bool isBinary, bool isOctal);
    void scanHexLiteral(Scanner::ScannerResult* token, size_t start);
    void scanBinaryLiteral(Scanner::ScannerResult* token, size_t start);
    void scanOctalLiteral(Scanner::ScannerResult* token, char16_t prefix, size_t start, bool allowUnderscore);

    bool isImplicitOctalLiteral();
    void scanNumericLiteral(Scanner::ScannerResult* token);

    // ECMA-262 11.8.4 String Literals
    void scanStringLiteral(Scanner::ScannerResult* token);

    // ECMA-262 11.6 Names and Keywords
    ALWAYS_INLINE void scanIdentifier(Scanner::ScannerResult* token, char16_t ch0);

    String* scanRegExpBody();
    String* scanRegExpFlags();
};
}
}

#endif
