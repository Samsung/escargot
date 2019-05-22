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

#ifndef __EscargotLexer__
#define __EscargotLexer__

#include "parser/esprima_cpp/esprima.h"

namespace Escargot {

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

enum PunctuatorKind {
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
    Wave,
    UnsignedRightShift,
    RightShift,
    LeftShift,
    Plus,
    Minus,
    Multiply,
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
    SubstitutionEnd,

    Arrow,
    PunctuatorKindEnd,
};

enum KeywordKind {
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
    AwaitKeyword,
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

struct ScanTemplteResult : public gc {
    UTF16StringData valueCooked;
    StringView raw;
    bool head;
    bool tail;
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

class ErrorHandler : public gc {
public:
    ErrorHandler()
    {
    }

    void throwError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code);
};

namespace Messages {
extern const char* UnexpectedTokenIllegal;
extern const char* UnterminatedRegExp;
extern const char* TemplateOctalLiteral;
}

class Scanner : public gc {
public:
    class ScannerResult {
    public:
        ScannerResult()
            : scanner(nullptr)
            , type(InvalidToken)
            , startWithZero(false)
            , octal(false)
            , plain(false)
            , hasKeywordButUseString(false)
            , prec(0)
            , lineNumber(0)
            , lineStart(0)
            , start(0)
            , end(0)
            , valueRegexp()
        {
        }

        ~ScannerResult() {}
        Scanner* scanner;
        unsigned char type : 4;
        bool startWithZero : 1;
        bool octal : 1;
        bool plain : 1;
        bool hasKeywordButUseString : 1;
        char prec : 8; // max prec is 11
        // we don't needs init prec.
        // prec is initialized by another function before use

        size_t lineNumber;
        size_t lineStart;
        size_t start;
        size_t end;

        union {
            PunctuatorKind valuePunctuatorKind;
            StringView* valueStringLiteralData;
            double valueNumber;
            ScanTemplteResult* valueTemplate;
            ScanRegExpResult valueRegexp;
            KeywordKind valueKeywordKind;
        };

        StringView relatedSource();
        StringView* valueStringLiteral();
        Value valueStringLiteralForAST();

        // ScannerResult always allocated on the stack
        MAKE_STACK_ALLOCATED();

        inline operator bool() const
        {
            return this->type != InvalidToken;
        }

        inline void reset()
        {
            this->type = InvalidToken;
        }

        void setResult(Scanner* scanner, Token type, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            ASSERT(scanner != nullptr);
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = false;
            this->octal = false;
            this->plain = false;
            this->hasKeywordButUseString = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueNumber = 0;
        }

        void setResult(Scanner* scanner, Token type, StringView* valueString, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool plain)
        {
            ASSERT(scanner != nullptr);
            ASSERT(valueString != nullptr);
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = false;
            this->octal = false;
            this->plain = plain;
            this->hasKeywordButUseString = true;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueStringLiteralData = valueString;
        }

        void setResult(Scanner* scanner, Token type, double value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            ASSERT(scanner != nullptr);
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = false;
            this->octal = false;
            this->plain = false;
            this->hasKeywordButUseString = true;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueNumber = value;
        }

        void setResult(Scanner* scanner, Token type, ScanTemplteResult* value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
        {
            ASSERT(scanner != nullptr);
            ASSERT(value != nullptr);
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = false;
            this->octal = false;
            this->plain = false;
            this->hasKeywordButUseString = true;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
            this->valueTemplate = value;
        }

    private:
        void constructStringLiteral();
        void constructStringLiteralHelperAppendUTF16(char16_t ch, UTF16StringDataNonGCStd& stringUTF16, bool& isEveryCharLatin1);
    };

    // ScannerResult should be allocated on the stack by ALLOCA
    COMPILE_ASSERT(sizeof(ScannerResult) < 512, "");

    StringView source;
    ::Escargot::Context* escargotContext;
    ErrorHandler* errorHandler;
    // trackComment: boolean;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;

    ~Scanner()
    {
    }

    Scanner(::Escargot::Context* escargotContext, StringView code, ErrorHandler* handler, size_t startLine = 0, size_t startColumn = 0);

    bool eof()
    {
        return index >= length;
    }

    ALWAYS_INLINE void throwUnexpectedToken(const char* message = Messages::UnexpectedTokenIllegal)
    {
        this->errorHandler->throwError(this->index, this->lineNumber, this->index - this->lineStart + 1, new ASCIIString(message), ErrorObject::SyntaxError);
    }

    // ECMA-262 11.4 Comments

    void skipSingleLineComment(void);
    void skipMultiLineComment(void);

    ALWAYS_INLINE void scanComments()
    {
        bool start = (this->index == 0);
        while (LIKELY(!this->eof())) {
            char16_t ch = this->source.bufferedCharAt(this->index);

            if (isWhiteSpace(ch)) {
                ++this->index;
            } else if (isLineTerminator(ch)) {
                ++this->index;
                if (ch == 0x0D && this->source.bufferedCharAt(this->index) == 0x0A) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                start = true;
            } else if (ch == 0x2F) { // U+002F is '/'
                ch = this->source.bufferedCharAt(this->index + 1);
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
                if ((this->source.bufferedCharAt(this->index + 1) == 0x2D) && (this->source.bufferedCharAt(this->index + 2) == 0x3E)) {
                    // '-->' is a single-line comment
                    this->index += 3;
                    this->skipSingleLineComment();
                } else {
                    break;
                }
            } else if (ch == 0x3C) { // U+003C is '<'
                if (this->length > this->index + 4) {
                    if (this->source.bufferedCharAt(this->index + 1) == '!'
                        && this->source.bufferedCharAt(this->index + 2) == '-'
                        && this->source.bufferedCharAt(this->index + 3) == '-') {
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

    bool isFutureReservedWord(const StringView& id);

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
        cp = this->source.bufferedCharAt(i);
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            second = this->source.bufferedCharAt(i + 1);
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
    ALWAYS_INLINE char16_t peekChar()
    {
        return this->source.bufferedCharAt(this->index);
    }

    char32_t scanHexEscape(char prefix);
    char32_t scanUnicodeCodePointEscape();

    uint16_t octalToDecimal(char16_t ch, bool octal);

    StringView* getIdentifier();
    StringView* getComplexIdentifier();

    // ECMA-262 11.7 Punctuators
    void scanPunctuator(Scanner::ScannerResult* token, char16_t ch0);

    // ECMA-262 11.8.3 Numeric Literals
    void scanHexLiteral(Scanner::ScannerResult* token, size_t start);
    void scanBinaryLiteral(Scanner::ScannerResult* token, size_t start);
    void scanOctalLiteral(Scanner::ScannerResult* token, char16_t prefix, size_t start);

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
