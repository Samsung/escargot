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
    TemplateToken
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

struct ScanTemplteResult : public gc {
    UTF16StringData valueCooked;
    StringView raw;
    bool head;
    bool tail;
};

struct ScanRegExpResult {
    String* body;
    String* flags;
};

class ErrorHandler : public gc {
public:
    ErrorHandler()
    {
    }

    void tolerate(esprima::Error* error)
    {
        throw * error;
    }

    esprima::Error constructError(String* msg, size_t column)
    {
        esprima::Error* error = new (NoGC) esprima::Error(msg);
        error->column = column;
        return *error;
    }

    esprima::Error createError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
    {
        UTF16StringDataNonGCStd msg = u"Line ";
        char lineStringBuf[512];
        snprintf(lineStringBuf, sizeof(lineStringBuf), "%zu", line);
        std::string lineString = lineStringBuf;
        msg += UTF16StringDataNonGCStd(lineString.begin(), lineString.end());
        msg += u": ";
        if (description->length()) {
            msg += UTF16StringDataNonGCStd(description->toUTF16StringData().data());
        }
        esprima::Error error = constructError(new UTF16String(msg.data(), msg.length()), col);
        error.index = index;
        error.lineNumber = line;
        error.description = description;
        error.errorCode = code;
        return error;
    };

    void throwError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
    {
        throw this->createError(index, line, col, description, code);
    }

    void tolerateError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
    {
        esprima::Error error = this->createError(index, line, col, description, code);
        throw error;
    }
};

namespace Messages {
extern const char* UnexpectedTokenIllegal;
extern const char* UnterminatedRegExp;
extern const char* TemplateOctalLiteral;
}

#define SCANNER_RESULT_POOL_INITIAL_SIZE 128
class Scanner : public gc {
public:
    class ScannerResult : public RefCounted<ScannerResult> {
    public:
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
            StringView valueStringLiteralData;
            double valueNumber;
            ScanTemplteResult* valueTemplate;
            ScanRegExpResult valueRegexp;
            KeywordKind valueKeywordKind;
        };

        StringView relatedSource();
        StringView valueStringLiteral();
        Value valueStringLiteralForAST();

        ~ScannerResult();

        inline void operator delete(void* obj)
        {
        }

        inline void operator delete(void*, void*) {}
        inline void operator delete[](void* obj) {}
        inline void operator delete[](void*, void*) {}
        ScannerResult()
        {
        }

        ScannerResult(Scanner* scanner, Token type, size_t lineNumber, size_t lineStart, size_t start, size_t end)
            : valueNumber(0)
        {
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = this->octal = false;
            this->hasKeywordButUseString = true;
            this->plain = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
        }

        ScannerResult(Scanner* scanner, Token type, const StringView& valueString, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool plain)
            : valueStringLiteralData(valueString)
        {
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = this->octal = false;
            this->hasKeywordButUseString = true;
            this->plain = plain;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
        }

        ScannerResult(Scanner* scanner, Token type, double value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
            : valueNumber(value)
        {
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = this->octal = false;
            this->hasKeywordButUseString = true;
            this->plain = false;
            this->valueNumber = value;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
        }

        ScannerResult(Scanner* scanner, Token type, ScanTemplteResult* value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
            : valueTemplate(value)
        {
            this->scanner = scanner;
            this->type = type;
            this->startWithZero = this->octal = false;
            this->hasKeywordButUseString = true;
            this->plain = false;
            this->lineNumber = lineNumber;
            this->lineStart = lineStart;
            this->start = start;
            this->end = end;
        }

    private:
        void constructStringLiteral();
    };

    StringView source;
    ::Escargot::Context* escargotContext;
    ErrorHandler* errorHandler;
    // trackComment: boolean;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;
    bool isPoolEnabled;
    ScannerResult* initialResultMemoryPool[SCANNER_RESULT_POOL_INITIAL_SIZE];
    size_t initialResultMemoryPoolSize;
    std::vector<ScannerResult*, gc_allocator<ScannerResult*>> resultMemoryPool;
    char scannerResultInnerPool[SCANNER_RESULT_POOL_INITIAL_SIZE * sizeof(ScannerResult)];

    ~Scanner()
    {
        isPoolEnabled = false;
    }

    Scanner(::Escargot::Context* escargotContext, StringView code, ErrorHandler* handler, size_t startLine = 0, size_t startColumn = 0);

    ScannerResult* createScannerResult();

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

            if (esprima::isWhiteSpace(ch)) {
                ++this->index;
            } else if (esprima::isLineTerminator(ch)) {
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

    static bool equals(const char16_t* c1, const char* c2, size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            if (c1[i] != c2[i]) {
                return false;
            }
        }

        return true;
    }

// ECMA-262 11.6.2.2 Future Reserved Words
#define STRING_CMP(str, len)                               \
    if (LIKELY(data.has8BitContent)) {                     \
        return strncmp(str, (char*)data.buffer, len) == 0; \
    } else {                                               \
        return equals((char16_t*)data.buffer, str, len);   \
    }

#define STRING_CMP2(str1, str2, len)                                                                   \
    if (LIKELY(data.has8BitContent)) {                                                                 \
        if (strncmp(str1, (char*)data.buffer, len) == 0) {                                             \
            return true;                                                                               \
        } else if (strncmp(str2, (char*)data.buffer, len) == 0) {                                      \
            return true;                                                                               \
        }                                                                                              \
    } else {                                                                                           \
        return equals((char16_t*)data.buffer, str1, len) || equals((char16_t*)data.buffer, str2, len); \
    }

    bool isFutureReservedWord(const StringView& id)
    {
        const StringBufferAccessData& data = id.bufferAccessData();
        if (data.length == 4) {
            STRING_CMP("enum", 4)
        } else if (data.length == 5) {
            STRING_CMP("super", 5)
        } else if (data.length == 6) {
            STRING_CMP2("export", "import", 6)
        }
        return false;
    }

    template <typename T>
    ALWAYS_INLINE bool isStrictModeReservedWord(const T& id)
    {
        const StringBufferAccessData& data = id.bufferAccessData();
        switch (data.length) {
        case 3: // let
            STRING_CMP("let", 3)
            break;
        case 5: // yield
            STRING_CMP("yield", 3)
            break;
        case 6: // static public
            STRING_CMP2("static", "public", 6)
            break;
        case 7: // private package
            STRING_CMP2("private", "package", 7)
            break;
        case 9: // protected interface
            STRING_CMP2("protected", "interface", 9)
            break;
        case 10: // implements
            STRING_CMP("implements", 10)
            break;
        }

#undef STRING_CMP
#undef STRING_CMP2

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
    PassRefPtr<ScannerResult> scanTemplate(bool head = false);

    // ECMA-262 11.8.5 Regular Expression Literals
    PassRefPtr<ScannerResult> scanRegExp();

    PassRefPtr<ScannerResult> lex();

private:
    char32_t scanHexEscape(char prefix);
    char32_t scanUnicodeCodePointEscape();

    uint16_t octalToDecimal(char16_t ch, bool octal);

    StringView getIdentifier();
    StringView getComplexIdentifier();

    // ECMA-262 11.7 Punctuators
    PassRefPtr<ScannerResult> scanPunctuator(char16_t ch0);

    // ECMA-262 11.8.3 Numeric Literals
    PassRefPtr<ScannerResult> scanHexLiteral(size_t start);
    PassRefPtr<ScannerResult> scanBinaryLiteral(size_t start);
    PassRefPtr<ScannerResult> scanOctalLiteral(char16_t prefix, size_t start);

    bool isImplicitOctalLiteral();
    PassRefPtr<ScannerResult> scanNumericLiteral();

    // ECMA-262 11.8.4 String Literals
    PassRefPtr<ScannerResult> scanStringLiteral();

    // ECMA-262 11.6 Names and Keywords
    ALWAYS_INLINE PassRefPtr<ScannerResult> scanIdentifier(char16_t ch0);

    String* scanRegExpBody();
    String* scanRegExpFlags();
};
}
}

#endif
