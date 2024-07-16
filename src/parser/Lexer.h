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

class AtomicString;

namespace esprima {
struct ParserContext;
}

namespace EscargotLexer {

enum Token : uint8_t {
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
    Arrow,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    Colon,
    Comma,
    Divide,
    Equal,
    ExclamationMark,
    Exponentiation,
    GuessDot,
    GuessMark,
    Hash,
    HashBang,
    InPunctuator,
    InstanceOfPunctuator,
    LeftBrace,
    LeftInequality,
    LeftParenthesis,
    LeftShift,
    LeftSquareBracket,
    LogicalAnd,
    LogicalOr,
    Minus,
    MinusMinus,
    Mod,
    Multiply,
    NotEqual,
    NotStrictEqual,
    NullishCoalescing,
    Period,
    PeriodPeriodPeriod,
    Plus,
    PlusPlus,
    RightBrace,
    RightInequality,
    RightParenthesis,
    RightShift,
    RightSquareBracket,
    SemiColon,
    StrictEqual,
    UnsignedRightShift,
    Wave,

    Substitution,
    BitwiseAndEqual,
    BitwiseOrEqual,
    BitwiseXorEqual,
    DivideEqual,
    ExponentiationEqual,
    LeftInequalityEqual,
    LeftShiftEqual,
    LogicalAndEqual,
    LogicalNullishEqual,
    LogicalOrEqual,
    MinusEqual,
    ModEqual,
    MultiplyEqual,
    PlusEqual,
    RightInequalityEqual,
    RightShiftEqual,
    UnsignedRightShiftEqual,
    SubstitutionEnd,

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
    StrictModeReservedWord, // Start of strict mode reserved keywords
    ImplementsKeyword,
    InterfaceKeyword,
    PackageKeyword,
    PrivateKeyword,
    ProtectedKeyword,
    PublicKeyword,
    StaticKeyword,
    YieldKeyword,
    LetKeyword,
    OtherKeyword, // Other keywords, which can be identifiers or tokens
    NullKeyword,
    TrueKeyword,
    FalseKeyword,
    GetKeyword,
    SetKeyword,
    EvalKeyword,
    ArgumentsKeyword,
    OfKeyword,
    AsyncKeyword,
    AwaitKeyword,
    AsKeyword,
    FromKeyword,
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

struct ScanState {
    ScanState(size_t idx, size_t lineNum, size_t lineSt)
        : index(idx)
        , lineNumber(lineNum)
        , lineStart(lineSt)
    {
    }
    size_t index;
    size_t lineNumber;
    size_t lineStart;
};

class ErrorHandler {
public:
    ErrorHandler()
    {
    }

    static void throwError(size_t index, size_t line, size_t col, String* description, ErrorCode code);
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
            , secondaryKeywordKind(NotKeyword)
            , startWithZero(false)
            , octal(false)
            , hasAllocatedString(false)
            , hasNonComputedNumberLiteral(false)
            , hasNumberSeparatorOnNumberLiteral(false)
            , lineNumber(0)
            , lineStart(0)
            , start(0)
            , end(0)
            , valueRegExp()
        {
        }

        // ScannerResult always allocated on the stack
        MAKE_STACK_ALLOCATED();

        uint8_t type;
        union {
            uint8_t secondaryKeywordKind;
            // Init prec is unnuecessary, since prec is initialized by another function before use
            uint8_t prec; // max prec is 11
        };

        // Boolean fields.
        bool startWithZero : 1;
        bool octal : 1;
        bool hasAllocatedString : 1;
        bool hasNonComputedNumberLiteral : 1;
        bool hasNumberSeparatorOnNumberLiteral : 1;

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

        void resetResult()
        {
            this->type = InvalidToken;
            this->startWithZero = false;
            this->octal = false;
            this->hasAllocatedString = false;
            this->hasNonComputedNumberLiteral = false;
            this->secondaryKeywordKind = NotKeyword;
        }

        void setResult(Token type, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            this->type = type;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueNumber = 0;
        }

        void setPunctuatorResult(size_t lineNumber, size_t lineStart, size_t start, size_t end, PunctuatorKind p)
        {
            this->type = Token::PunctuatorToken;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valuePunctuatorKind = p;
        }

        void setKeywordResult(size_t lineNumber, size_t lineStart, size_t start, size_t end, KeywordKind p)
        {
            this->type = Token::KeywordToken;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueKeywordKind = p;
        }

        void setResult(Token type, String* s, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool octal = false)
        {
            this->type = type;
            this->octal = octal;
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
            this->octal = octal;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;

            ASSERT(!this->hasAllocatedString);
            this->valueStringLiteralData.m_start = stringStart;
            this->valueStringLiteralData.m_end = stringEnd;
            ASSERT(this->valueStringLiteralData.m_start <= this->valueStringLiteralData.m_end);
        }

        void setNumericLiteralResult(double value, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool hasNonComputedNumberLiteral, bool hasNumberSeparatorOnNumberLiteral)
        {
            this->type = Token::NumericLiteralToken;
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
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueTemplate = value;
        }

        bool isStrictModeReservedWord() const
        {
            ASSERT(this->type == KeywordToken || this->type == IdentifierToken);
            return (this->secondaryKeywordKind > StrictModeReservedWord && this->secondaryKeywordKind < OtherKeyword);
        }

        bool isRestrictedWord() const
        {
            ASSERT(this->type == KeywordToken || this->type == IdentifierToken);
            return (this->secondaryKeywordKind == EvalKeyword || this->secondaryKeywordKind == ArgumentsKeyword);
        }

        bool equalsToKeyword(KeywordKind keywordKind) const
        {
            ASSERT(this->type == Token::IdentifierToken || this->type == BooleanLiteralToken || this->type == KeywordToken);
            return (this->secondaryKeywordKind == keywordKind);
        }

        bool equalsToKeywordNoEscape(KeywordKind keywordKind) const
        {
            ASSERT(this->type == Token::IdentifierToken || this->type == BooleanLiteralToken || this->type == KeywordToken);
            return (this->secondaryKeywordKind == keywordKind && !this->hasAllocatedString);
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
            , valueKeywordKind(NotKeyword)
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
        uint8_t type;

        union {
            uint8_t secondaryKeywordKind;
            uint8_t prec; // max prec is 11
        };

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

        bool equalsToKeyword(KeywordKind keywordKind) const
        {
            ASSERT(this->type == Token::IdentifierToken || this->type == BooleanLiteralToken || this->type == KeywordToken);
            return (this->secondaryKeywordKind == keywordKind);
        }

        bool isStrictModeReservedWord() const
        {
            ASSERT(this->type == Token::KeywordToken || this->type == Token::IdentifierToken);
            return (this->secondaryKeywordKind > StrictModeReservedWord && this->secondaryKeywordKind < OtherKeyword);
        }

        bool isRestrictedWord() const
        {
            ASSERT(this->type == KeywordToken || this->type == IdentifierToken);
            return (this->secondaryKeywordKind == EvalKeyword || this->secondaryKeywordKind == ArgumentsKeyword);
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
    bool isModule;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;

    ~Scanner()
    {
    }

    Scanner(::Escargot::Context* escargotContext, ::Escargot::esprima::ParserContext* parserContext, const StringView& code, bool isModule, size_t startLine = 0, size_t startColumn = 0);

    // Scanner always allocated on the stack
    MAKE_STACK_ALLOCATED();

    void resetSource(StringView code);

    ScanState saveState()
    {
        return ScanState(this->index, this->lineNumber, this->lineStart);
    }

    void restoreState(ScanState& state)
    {
        this->index = state.index;
        this->lineNumber = state.lineNumber;
        this->lineStart = state.lineStart;
    }

    bool eof()
    {
        return index >= length;
    }

    ALWAYS_INLINE void throwUnexpectedToken(const char* message = Messages::UnexpectedTokenIllegal);

    ALWAYS_INLINE char16_t sourceCharAt(const size_t idx) const
    {
        ASSERT(idx < this->length);
        return sourceCodeAccessData.charAt(idx);
    }

    void skipSingleLine();

    // ECMA-262 11.4 Comments

    void skipSingleLineComment(void);
    void skipMultiLineComment(void);

    ALWAYS_INLINE void scanComments()
    {
        bool start = (this->index == 0);
        while (LIKELY(!this->eof())) {
            char16_t ch = this->peekCharWithoutEOF();

            if (isWhiteSpace(ch)) {
                ++this->index;
            } else if (isLineTerminator(ch)) {
                ++this->index;
                if (ch == 0x0D && this->peekChar(this->index) == 0x0A) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                start = true;
            } else if (ch == 0x2F) { // U+002F is '/'
                ch = this->peekChar(this->index + 1);
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
            } else if (start && ch == 0x2D && !this->isModule) { // U+002D is '-'
                // U+003E is '>'
                if ((this->peekChar(this->index + 1) == 0x2D) && (this->peekChar(this->index + 2) == 0x3E)) {
                    // '-->' is a single-line comment
                    this->index += 3;
                    this->skipSingleLineComment();
                } else {
                    break;
                }
            } else if (ch == 0x3C && !this->isModule) { // U+003C is '<'
                if (this->peekChar(this->index + 1) == '!'
                    && this->peekChar(this->index + 2) == '-'
                    && this->peekChar(this->index + 3) == '-') {
                    this->index += 4; // `<!--`
                    this->skipSingleLineComment();
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
        ASSERT(token->type == KeywordToken || token->type == Token::IdentifierToken);
        ASSERT(token->secondaryKeywordKind > StrictModeReservedWord && token->secondaryKeywordKind < OtherKeyword);

        token->type = Token::KeywordToken;
        token->valueKeywordKind = static_cast<KeywordKind>(token->secondaryKeywordKind);
    }

    static bool isStrictModeReservedWord(::Escargot::Context* ctx, const AtomicString& identifier);
    static bool isRestrictedWord(::Escargot::Context* ctx, const AtomicString& identifier);

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
        ASSERT(!this->eof());
        return this->sourceCharAt(this->index);
    }

    ALWAYS_INLINE char16_t peekChar()
    {
        return UNLIKELY(this->eof()) ? 0 : this->sourceCharAt(this->index);
    }

    ALWAYS_INLINE char16_t peekChar(size_t idx)
    {
        // check EOF
        return UNLIKELY(idx >= this->length) ? 0 : this->sourceCharAt(idx);
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
} // namespace EscargotLexer
} // namespace Escargot

#endif
