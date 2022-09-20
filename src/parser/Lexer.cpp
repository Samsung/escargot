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
#include "parser/Lexer.h"
#include "parser/UnicodeIdentifierTables.h"
#include "parser/esprima_cpp/ParserContext.h"

// These two must be the last because they overwrite the ASSERT macro.
#include "double-conversion.h"
#include "ieee.h"

using namespace Escargot::EscargotLexer;

namespace Escargot {

#define IDENT_RANGE_LONG 200

/* The largest code-point that an UTF16 surrogate pair can represent is 0x10ffff,
 * so any codepoint above this can be a valid value for empty. The UINT32_MAX is
 * chosen because it is a valid immediate for machine instructions. */
#define EMPTY_CODE_POINT UINT32_MAX

/* The largest octal value is 255, so any higher
 * value can represent an invalid octal value. */
#define NON_OCTAL_VALUE 256

char EscargotLexer::g_asciiRangeCharMap[128] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    LexerIsCharWhiteSpace,
    LexerIsCharLineTerminator,
    LexerIsCharWhiteSpace,
    LexerIsCharWhiteSpace,
    LexerIsCharLineTerminator,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    LexerIsCharWhiteSpace,
    0,
    0,
    0,
    LexerIsCharIdentStart | LexerIsCharIdent,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    LexerIsCharIdent,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    0,
    LexerIsCharIdentStart | LexerIsCharIdent,
    0,
    0,
    LexerIsCharIdentStart | LexerIsCharIdent,
    0,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    LexerIsCharIdentStart | LexerIsCharIdent,
    0,
    0,
    0,
    0,
    0
};

NEVER_INLINE bool EscargotLexer::isWhiteSpaceSlowCase(char16_t ch)
{
    ASSERT(ch >= 0x80);

    if (LIKELY(ch < 0x1680)) {
        return (ch == 0xA0);
    }

    return (ch == 0x1680 || ch == 0x2000 || ch == 0x2001
            || ch == 0x2002 || ch == 0x2003 || ch == 0x2004 || ch == 0x2005 || ch == 0x2006
            || ch == 0x2007 || ch == 0x2008 || ch == 0x2009 || ch == 0x200A || ch == 0x202F
            || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF);
}

static NEVER_INLINE bool isIdentifierPartSlow(char32_t ch)
{
    int bottom = 0;
    int top = (EscargotLexer::basic_plane_length / sizeof(uint16_t)) - 1;

    while (true) {
        int middle = (bottom + top) >> 1;
        char32_t rangeStart = identRangeStart[middle];

        if (ch >= rangeStart) {
            if (ch < identRangeStart[middle + 1]) {
                char32_t length = identRangeLength[middle];

                if (UNLIKELY(length >= IDENT_RANGE_LONG)) {
                    length = identRangeLongLength[length - IDENT_RANGE_LONG];
                }
                return ch <= rangeStart + length;
            }

            bottom = middle + 1;
        } else {
            top = middle;
        }

        if (bottom == top) {
            return false;
        }
    }
}

static NEVER_INLINE bool isIdentifierPartSlowSupplementary(char32_t ch)
{
    int bottom = 0;
    int top = (EscargotLexer::supplementary_plane_length / sizeof(uint32_t)) - 1;

    while (true) {
        int middle = (bottom + top) >> 1;
        char32_t rangeStart = identRangeStartSupplementaryPlane[middle];

        if (ch >= rangeStart) {
            if (ch < identRangeStartSupplementaryPlane[middle + 1]) {
                char32_t length = identRangeLengthSupplementaryPlane[middle];
                return ch <= rangeStart + length;
            }

            bottom = middle + 1;
        } else {
            top = middle;
        }

        if (bottom == top) {
            return false;
        }
    }
}

static ALWAYS_INLINE bool isIdentifierPart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & LexerIsCharIdent;
    }

    return isIdentifierPartSlow(ch) || isIdentifierPartSlowSupplementary(ch);
}

static ALWAYS_INLINE bool isIdentifierStart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & LexerIsCharIdentStart;
    }

    return isIdentifierPartSlow(ch) || isIdentifierPartSlowSupplementary(ch);
}

static ALWAYS_INLINE bool isDecimalDigit(char16_t ch)
{
    return (ch >= '0' && ch <= '9');
}

static ALWAYS_INLINE bool isDecimalDigitOrUnderscore(char16_t ch, bool& seenUnderScore)
{
    if (UNLIKELY(ch == '_')) {
        seenUnderScore = true;
        return true;
    }
    return (ch >= '0' && ch <= '9');
}

static ALWAYS_INLINE bool isHexDigit(char16_t ch)
{
    return isDecimalDigit(ch) || ((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f');
}

static ALWAYS_INLINE bool isHexDigitOrUnderscore(char16_t ch, bool& seenUnderScore)
{
    return isDecimalDigitOrUnderscore(ch, seenUnderScore) || ((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f');
}

static ALWAYS_INLINE bool isOctalDigit(char16_t ch)
{
    return (ch >= '0' && ch <= '7');
}

static ALWAYS_INLINE char16_t octalValue(char16_t ch)
{
    ASSERT(isOctalDigit(ch));
    return ch - '0';
}

static ALWAYS_INLINE uint8_t toHexNumericValue(char16_t ch)
{
    return ch < 'A' ? ch - '0' : ((ch - 'A' + 10) & 0xF);
}

static int hexValue(char16_t ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    ASSERT((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f');

    return (ch | 0x20) - ('a' - 10);
}

struct ParserCharPiece {
    char16_t data[3];
    size_t length;

    ParserCharPiece(const char32_t a)
    {
        if (a < 0x10000) {
            data[0] = a;
            data[1] = 0;
            length = 1;
        } else {
            data[0] = (char16_t)(0xD800 + ((a - 0x10000) >> 10));
            data[1] = (char16_t)(0xDC00 + ((a - 0x10000) & 1023));
            data[2] = 0;
            length = 2;
        }
    }
};

AtomicString keywordToString(::Escargot::Context* ctx, KeywordKind keyword)
{
    switch (keyword) {
    case IfKeyword:
        return ctx->staticStrings().stringIf;
    case InKeyword:
        return ctx->staticStrings().stringIn;
    case DoKeyword:
        return ctx->staticStrings().stringDo;
    case VarKeyword:
        return ctx->staticStrings().var;
    case ForKeyword:
        return ctx->staticStrings().stringFor;
    case NewKeyword:
        return ctx->staticStrings().stringNew;
    case TryKeyword:
        return ctx->staticStrings().stringTry;
    case ThisKeyword:
        return ctx->staticStrings().stringThis;
    case ElseKeyword:
        return ctx->staticStrings().stringElse;
    case CaseKeyword:
        return ctx->staticStrings().stringCase;
    case VoidKeyword:
        return ctx->staticStrings().stringVoid;
    case WithKeyword:
        return ctx->staticStrings().with;
    case EnumKeyword:
        return ctx->staticStrings().stringEnum;
    case WhileKeyword:
        return ctx->staticStrings().stringWhile;
    case BreakKeyword:
        return ctx->staticStrings().stringBreak;
    case CatchKeyword:
        return ctx->staticStrings().stringCatch;
    case ThrowKeyword:
        return ctx->staticStrings().stringThrow;
    case ConstKeyword:
        return ctx->staticStrings().stringConst;
    case ClassKeyword:
        return ctx->staticStrings().stringClass;
    case SuperKeyword:
        return ctx->staticStrings().super;
    case ReturnKeyword:
        return ctx->staticStrings().stringReturn;
    case TypeofKeyword:
        return ctx->staticStrings().stringTypeof;
    case DeleteKeyword:
        return ctx->staticStrings().stringDelete;
    case SwitchKeyword:
        return ctx->staticStrings().stringSwitch;
    case ExportKeyword:
        return ctx->staticStrings().stringExport;
    case ImportKeyword:
        return ctx->staticStrings().stringImport;
    case DefaultKeyword:
        return ctx->staticStrings().stringDefault;
    case FinallyKeyword:
        return ctx->staticStrings().finally;
    case ExtendsKeyword:
        return ctx->staticStrings().extends;
    case FunctionKeyword:
        return ctx->staticStrings().function;
    case ContinueKeyword:
        return ctx->staticStrings().stringContinue;
    case DebuggerKeyword:
        return ctx->staticStrings().debugger;
    case InstanceofKeyword:
        return ctx->staticStrings().instanceof ;
    case ImplementsKeyword:
        return ctx->staticStrings().implements;
    case InterfaceKeyword:
        return ctx->staticStrings().interface;
    case PackageKeyword:
        return ctx->staticStrings().package;
    case PrivateKeyword:
        return ctx->staticStrings().stringPrivate;
    case ProtectedKeyword:
        return ctx->staticStrings().stringProtected;
    case PublicKeyword:
        return ctx->staticStrings().stringPublic;
    case StaticKeyword:
        return ctx->staticStrings().stringStatic;
    case YieldKeyword:
        return ctx->staticStrings().yield;
    case LetKeyword:
        return ctx->staticStrings().let;
    case NullKeyword:
        return ctx->staticStrings().null;
    case TrueKeyword:
        return ctx->staticStrings().stringTrue;
    case FalseKeyword:
        return ctx->staticStrings().stringFalse;
    case GetKeyword:
        return ctx->staticStrings().get;
    case SetKeyword:
        return ctx->staticStrings().set;
    case EvalKeyword:
        return ctx->staticStrings().eval;
    case ArgumentsKeyword:
        return ctx->staticStrings().arguments;
    case OfKeyword:
        return ctx->staticStrings().of;
    case AsyncKeyword:
        return ctx->staticStrings().async;
    case AwaitKeyword:
        return ctx->staticStrings().await;
    case AsKeyword:
        return ctx->staticStrings().as;
    case FromKeyword:
        return ctx->staticStrings().from;
    default:
        ASSERT_NOT_REACHED();
        return ctx->staticStrings().error;
    }
}

void ErrorHandler::throwError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
{
    UTF16StringDataNonGCStd msg = u"Line ";
    const size_t bufferLength = 64;
    char lineStringBuf[bufferLength];
    char* bufPtr = lineStringBuf + bufferLength - 2;

    /* Adds ": " at the end. */
    bufPtr[0] = ':';
    bufPtr[1] = ' ';

    size_t value = line;
    do {
        ASSERT(bufPtr > lineStringBuf);
        --bufPtr;
        *bufPtr = value % 10 + '0';
        value /= 10;
    } while (value > 0);

    msg += UTF16StringDataNonGCStd(bufPtr, lineStringBuf + bufferLength);

    if (description->length()) {
        msg += UTF16StringDataNonGCStd(description->toUTF16StringData().data());
    }

    esprima::Error* error = new (NoGC) esprima::Error(new UTF16String(msg.data(), msg.length()));
    error->index = index;
    error->lineNumber = line;
    error->column = col;
    error->description = description;
    error->errorCode = code;

    throw error;
};

ParserStringView Scanner::SmallScannerResult::relatedSource(const ParserStringView& source) const
{
    return ParserStringView(source, this->start, this->end);
}

StringView Scanner::SmallScannerResult::relatedSource(const StringView& source) const
{
    return StringView(source, this->start, this->end);
}

ParserStringView Scanner::ScannerResult::relatedSource(const ParserStringView& source)
{
    return ParserStringView(source, this->start, this->end);
}

StringView Scanner::ScannerResult::relatedSource(const StringView& source)
{
    return StringView(source, this->start, this->end);
}

Value Scanner::ScannerResult::valueStringLiteralToValue(Scanner* scannerInstance)
{
    ASSERT(this->type == Token::StringLiteralToken);

    if (UNLIKELY(this->hasAllocatedString)) {
        if (!this->valueStringLiteralData.m_stringIfNewlyAllocated) {
            constructStringLiteral(scannerInstance);
        }
        return this->valueStringLiteralData.m_stringIfNewlyAllocated;
    }

    // check if string is one of typeof strings
    // we only consider the most common cases which are undefined, object, function
    size_t start = this->valueStringLiteralData.m_start;
    size_t end = this->valueStringLiteralData.m_end;
    size_t length = end - start;
    if (length > 5 && length < 10) {
        ParserStringView str(scannerInstance->source, start, end);
        switch (str.bufferedCharAt(0)) {
        case 'o': {
            if (length == 6 && str.equalsSameLength("object", 1)) {
                return scannerInstance->escargotContext->staticStrings().object.string();
            }
            break;
        }
        case 'f': {
            if (length == 8 && str.equalsSameLength("function", 1)) {
                return scannerInstance->escargotContext->staticStrings().function.string();
            }
            break;
        }
        case 'u': {
            if (length == 9 && str.equalsSameLength("undefined", 1)) {
                return scannerInstance->escargotContext->staticStrings().undefined.string();
            }
            break;
        }
        default: {
            return new StringView(scannerInstance->sourceAsNormalView, start, end);
        }
        }
    }

    return new StringView(scannerInstance->sourceAsNormalView, start, end);
}

ParserStringView Scanner::ScannerResult::valueStringLiteral(Scanner* scannerInstance)
{
    if (this->type == Token::KeywordToken) {
        AtomicString as = keywordToString(scannerInstance->escargotContext, this->valueKeywordKind);
        return ParserStringView(as.string(), 0, as.string()->length());
    }
    if (this->hasAllocatedString) {
        if (!this->valueStringLiteralData.m_stringIfNewlyAllocated) {
            constructStringLiteral(scannerInstance);
        }
        return ParserStringView(this->valueStringLiteralData.m_stringIfNewlyAllocated);
    }
    return ParserStringView(scannerInstance->source, this->valueStringLiteralData.m_start, this->valueStringLiteralData.m_end);
}

std::pair<Value, bool> Scanner::ScannerResult::valueNumberLiteral(Scanner* scannerInstance)
{
    if (this->hasNonComputedNumberLiteral) {
        const auto& bd = scannerInstance->source.bufferAccessData();
        char* buffer;
        int length = this->end - this->start;

        if (UNLIKELY(this->hasNumberSeparatorOnNumberLiteral)) {
            buffer = ALLOCA(this->end - this->start, char, ec);
            int underScoreCount = 0;
            for (int i = 0; i < length; i++) {
                auto c = bd.charAt(i + this->start);
                if (c == '_') {
                    underScoreCount++;
                } else {
                    buffer[i - underScoreCount] = c;
                }
            }
            length -= underScoreCount;
            ASSERT(underScoreCount != 0);
        } else {
            if (bd.has8BitContent) {
                buffer = ((char*)bd.buffer) + this->start;
            } else {
                buffer = ALLOCA(this->end - this->start, char, ec);

                for (int i = 0; i < length; i++) {
                    buffer[i] = bd.uncheckedCharAtFor16Bit(i + this->start);
                }
            }
        }

        // bigint case
        if (UNLIKELY(buffer[length - 1] == 'n')) {
            return std::make_pair(Value(BigInt::parseString(buffer, length - 1).value()), true);
        }

        int lengthDummy;
        double_conversion::StringToDoubleConverter converter(double_conversion::StringToDoubleConverter::ALLOW_HEX
                                                                 | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES
                                                                 | double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES,
                                                             0.0, double_conversion::Double::NaN(),
                                                             "Infinity", "NaN");
        double ll = converter.StringToDouble(buffer, length, &lengthDummy);

        this->valueNumber = ll;
        this->hasNonComputedNumberLiteral = false;
    }
    return std::make_pair(Value(this->valueNumber), false);
}

void Scanner::ScannerResult::constructStringLiteralHelperAppendUTF16(Scanner* scannerInstance, char16_t ch, UTF16StringDataNonGCStd& stringUTF16, bool& isEveryCharLatin1)
{
    switch (ch) {
    case 'u':
    case 'x': {
        char32_t param;
        if (scannerInstance->peekChar() == '{') {
            ++scannerInstance->index;
            param = scannerInstance->scanUnicodeCodePointEscape();
        } else {
            param = scannerInstance->scanHexEscape(ch);
        }
        ParserCharPiece piece(param);
        stringUTF16.append(piece.data, piece.data + piece.length);
        if (piece.length != 1 || piece.data[0] >= 256) {
            isEveryCharLatin1 = false;
        }
        return;
    }
    case 'n':
        stringUTF16 += '\n';
        return;
    case 'r':
        stringUTF16 += '\r';
        return;
    case 't':
        stringUTF16 += '\t';
        return;
    case 'b':
        stringUTF16 += '\b';
        return;
    case 'f':
        stringUTF16 += '\f';
        return;
    case 'v':
        stringUTF16 += '\x0B';
        return;

    default:
        if (ch && isOctalDigit(ch)) {
            uint16_t octToDec = scannerInstance->octalToDecimal(ch, true);
            stringUTF16 += octToDec;
            ASSERT(octToDec < 256);
        } else {
            stringUTF16 += ch;
            if (ch >= 256) {
                isEveryCharLatin1 = false;
            }
        }
        return;
    }
}

void Scanner::ScannerResult::constructStringLiteral(Scanner* scannerInstance)
{
    size_t indexBackup = scannerInstance->index;
    size_t lineNumberBackup = scannerInstance->lineNumber;
    size_t lineStartBackup = scannerInstance->lineStart;

    scannerInstance->index = this->start;
    char16_t quote = scannerInstance->peekChar();
    ASSERT((quote == '\'' || quote == '"'));
    // 'String literal must starts with a quote');

    ++scannerInstance->index;
    bool isEveryCharLatin1 = true;

    UTF16StringDataNonGCStd stringUTF16;
    while (true) {
        char16_t ch = scannerInstance->peekChar();
        ++scannerInstance->index;
        if (ch == quote) {
            quote = '\0';
            break;
        } else if (UNLIKELY(ch == '\\')) {
            ch = scannerInstance->peekChar();
            ++scannerInstance->index;
            if (!ch || !isLineTerminator(ch)) {
                this->constructStringLiteralHelperAppendUTF16(scannerInstance, ch, stringUTF16, isEveryCharLatin1);
            } else {
                ++scannerInstance->lineNumber;
                char16_t bufferedChar = scannerInstance->peekChar();
                if ((ch == '\r' && bufferedChar == '\n') || (ch == '\n' && bufferedChar == '\r')) {
                    ++scannerInstance->index;
                }
                scannerInstance->lineStart = scannerInstance->index;
            }
        } else if (UNLIKELY(isLineTerminator(ch))) {
            break;
        } else {
            stringUTF16 += ch;
            if (ch >= 256) {
                isEveryCharLatin1 = false;
            }
        }
    }

    scannerInstance->index = indexBackup;
    scannerInstance->lineNumber = lineNumberBackup;
    scannerInstance->lineStart = lineStartBackup;

    String* newStr;
    if (isEveryCharLatin1) {
        newStr = String::fromLatin1(stringUTF16.data(), stringUTF16.length());
    } else {
        newStr = new UTF16String(stringUTF16.data(), stringUTF16.length());
    }
    this->valueStringLiteralData.m_stringIfNewlyAllocated = newStr;
}

Scanner::Scanner(::Escargot::Context* escargotContext, ::Escargot::esprima::ParserContext* parserContext, StringView code, bool isModule, size_t startLine, size_t startColumn)
    : source(code, 0, code.length())
    , sourceAsNormalView(code)
    , escargotContext(escargotContext)
    , parserContext(parserContext)
    , sourceCodeAccessData(code.bufferAccessData())
    , isModule(isModule)
    , length(code.length())
    , index(0)
    , lineNumber(startLine)
    , lineStart(startColumn)
{
    ASSERT(escargotContext != nullptr);
    // trackComment = false;
}

void Scanner::resetSource(StringView code)
{
    this->source = ParserStringView(code, 0, code.length());
    this->sourceAsNormalView = code;
    this->sourceCodeAccessData = code.bufferAccessData();
    this->length = code.length();
    this->index = 0;
    this->lineNumber = 1;
    this->lineStart = 0;
}

void Scanner::skipSingleLine()
{
    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        ++this->index;

        if (isLineTerminator(ch)) {
            if (ch == 13 && this->peekCharWithoutEOF() == 10) {
                ++this->index;
            }
            ++this->lineNumber;
            this->lineStart = this->index;
            return;
        }
    }
}

void Scanner::skipSingleLineComment(void)
{
    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        ++this->index;

        if (isLineTerminator(ch)) {
            if (ch == 13 && this->peekCharWithoutEOF() == 10) {
                ++this->index;
            }
            ++this->lineNumber;
            this->lineStart = this->index;
            // return comments;
            return;
        }
    }
}

void Scanner::skipMultiLineComment(void)
{
    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        ++this->index;

        if (isLineTerminator(ch)) {
            if (ch == 0x0D && this->peekCharWithoutEOF() == 0x0A) {
                ++this->index;
            }
            ++this->lineNumber;
            this->lineStart = this->index;
        } else if (ch == 0x2A && this->peekCharWithoutEOF() == 0x2F) {
            // Block comment ends with '*/'.
            ++this->index;
            return;
        }
    }

    throwUnexpectedToken();
}

char32_t Scanner::scanHexEscape(char prefix)
{
    size_t len = (prefix == 'u') ? 4 : 2;
    char32_t code = 0;

    for (size_t i = 0; i < len; ++i) {
        if (!this->eof() && isHexDigit(this->peekCharWithoutEOF())) {
            code = code * 16 + hexValue(this->peekCharWithoutEOF());
            ++this->index;
        } else {
            return EMPTY_CODE_POINT;
        }
    }

    return code;
}

char32_t Scanner::scanUnicodeCodePointEscape()
{
    // At least, one hex digit is required.
    if (this->eof() || this->peekCharWithoutEOF() == '}') {
        this->throwUnexpectedToken();
    }

    char32_t code = 0;
    char16_t ch;

    while (!this->eof()) {
        ch = this->peekCharWithoutEOF();
        ++this->index;
        if (!isHexDigit(ch)) {
            break;
        }
        code = code * 16 + hexValue(ch);
    }

    if (code > 0x10FFFF || ch != '}') {
        this->throwUnexpectedToken();
    }

    return code;
}

Scanner::ScanIDResult Scanner::getIdentifier()
{
    const size_t start = this->index;
    ++this->index;
    while (UNLIKELY(!this->eof())) {
        const char16_t ch = this->peekCharWithoutEOF();
        if (UNLIKELY(ch == 0x5C)) {
            // Blackslash (U+005C) marks Unicode escape sequence.
            this->index = start;
            return this->getComplexIdentifier();
        } else if (UNLIKELY(ch >= 0xD800 && ch < 0xDFFF)) {
            // Need to handle surrogate pairs.
            this->index = start;
            return this->getComplexIdentifier();
        }
        if (isIdentifierPart(ch)) {
            ++this->index;
        } else {
            break;
        }
    }

    const auto& srcData = this->source.bufferAccessData();
    StringBufferAccessData ad(srcData.has8BitContent, this->index - start,
                              srcData.has8BitContent ? reinterpret_cast<void*>(((LChar*)srcData.buffer) + start) : reinterpret_cast<void*>(((char16_t*)srcData.buffer) + start));

    return std::make_tuple(ad, nullptr);
}

Scanner::ScanIDResult Scanner::getComplexIdentifier()
{
    char16_t cp = this->codePointAt(this->index);
    ParserCharPiece piece = ParserCharPiece(cp);
    UTF16StringDataNonGCStd id(piece.data, piece.length);
    this->index += id.length();

    // '\u' (U+005C, U+0075) denotes an escaped character.
    char32_t ch;
    if (cp == 0x5C) {
        if (this->peekChar() != 0x75) {
            this->throwUnexpectedToken();
        }
        ++this->index;
        if (this->peekChar() == '{') {
            ++this->index;
            ch = this->scanUnicodeCodePointEscape();
        } else {
            ch = this->scanHexEscape('u');
            cp = ch;
            if (ch == EMPTY_CODE_POINT || ch == '\\' || !isIdentifierStart(cp)) {
                this->throwUnexpectedToken();
            }
        }
        id = ch;
    }

    while (!this->eof()) {
        ch = this->codePointAt(this->index);
        if (!isIdentifierPart(ch)) {
            break;
        }

        piece = ParserCharPiece(ch);
        id += UTF16StringDataNonGCStd(piece.data, piece.length);
        this->index += piece.length;

        // '\u' (U+005C, U+0075) denotes an escaped character.
        if (ch == 0x5C) {
            // id = id.substr(0, id.length - 1);
            id.erase(id.length() - 1);

            if (this->peekChar() != 0x75) {
                this->throwUnexpectedToken();
            }
            ++this->index;
            if (this->peekChar() == '{') {
                ++this->index;
                ch = this->scanUnicodeCodePointEscape();
            } else {
                ch = this->scanHexEscape('u');
                cp = ch;
                if (ch == EMPTY_CODE_POINT || ch == '\\' || !isIdentifierPart(cp)) {
                    this->throwUnexpectedToken();
                }
            }

            if (!isIdentifierPart(ch)) {
                break;
            }

            piece = ParserCharPiece(ch);
            id += UTF16StringDataNonGCStd(piece.data, piece.length);
        }
    }

    String* str = new UTF16String(id.data(), id.length());

    if (UNLIKELY(this->parserContext->await && id == u"await")) {
        this->throwUnexpectedToken(Messages::KeywordMustNotContainEscapedCharacters);
    }

    return std::make_tuple(str->bufferAccessData(), str);
}

uint16_t Scanner::octalToDecimal(char16_t ch, bool octal)
{
    // \0 is not octal escape sequence
    char16_t code = octalValue(ch);

    octal |= (ch != '0');

    if (!this->eof() && isOctalDigit(this->peekChar())) {
        octal = true;
        code = code * 8 + octalValue(this->peekChar());
        ++this->index;

        // 3 digits are only allowed when string starts
        // with 0, 1, 2, 3
        // if ('0123'.indexOf(ch) >= 0 && !this->eof() && Character.isOctalDigit(this->source.charCodeAt(this->index))) {
        if ((ch >= '0' && ch <= '3') && !this->eof() && isOctalDigit(this->peekChar())) {
            code = code * 8 + octalValue(this->peekChar());
            ++this->index;
        }
    }

    ASSERT(!octal || code < NON_OCTAL_VALUE);
    return octal ? code : NON_OCTAL_VALUE;
};

void Scanner::scanPunctuator(Scanner::ScannerResult* token, char16_t ch)
{
    const size_t start = this->index;
    PunctuatorKind kind;
    // Check for most common single-character punctuators.
    ++this->index;

    switch (ch) {
    case '(':
        kind = LeftParenthesis;
        break;

    case '{':
        kind = LeftBrace;
        break;

    case '.':
        kind = Period;
        if (this->peekChar() == '.' && this->sourceCharAt(this->index + 1) == '.') {
            // Spread operator "..."
            this->index += 2;
            kind = PeriodPeriodPeriod;
        }
        break;

    case '}':
        kind = RightBrace;
        break;
    case ')':
        kind = RightParenthesis;
        break;
    case ';':
        kind = SemiColon;
        break;
    case ',':
        kind = Comma;
        break;
    case '[':
        kind = LeftSquareBracket;
        break;
    case ']':
        kind = RightSquareBracket;
        break;
    case ':':
        kind = Colon;
        break;
    case '?':
        kind = GuessMark;
        ch = this->peekChar();
        if (ch == '?') {
            ++this->index;
            kind = NullishCoalescing;
            if (this->peekChar() == '=') {
                kind = LogicalNullishEqual;
                ++this->index;
            }
        } else if (ch == '.') {
            ++this->index;
            kind = GuessDot;
        }
        break;
    case '~':
        kind = Wave;
        break;

    case '>':
        ch = this->peekChar();
        kind = RightInequality;

        if (ch == '>') {
            ++this->index;
            ch = this->peekChar();
            kind = RightShift;

            if (ch == '>') {
                ++this->index;
                kind = UnsignedRightShift;

                if (this->peekChar() == '=') {
                    ++this->index;
                    kind = UnsignedRightShiftEqual;
                }
            } else if (ch == '=') {
                kind = RightShiftEqual;
                ++this->index;
            }
        } else if (ch == '=') {
            kind = RightInequalityEqual;
            ++this->index;
        }
        break;

    case '<':
        ch = this->peekChar();
        kind = LeftInequality;

        if (ch == '<') {
            ++this->index;
            kind = LeftShift;

            if (this->peekChar() == '=') {
                kind = LeftShiftEqual;
                ++this->index;
            }
        } else if (ch == '=') {
            kind = LeftInequalityEqual;
            ++this->index;
        }
        break;

    case '=':
        ch = this->peekChar();
        kind = Substitution;

        if (ch == '=') {
            ++this->index;
            kind = Equal;

            if (this->peekChar() == '=') {
                kind = StrictEqual;
                ++this->index;
            }
        } else if (ch == '>') {
            kind = Arrow;
            ++this->index;
        }
        break;

    case '!':
        kind = ExclamationMark;

        if (this->peekChar() == '=') {
            ++this->index;
            kind = NotEqual;

            if (this->peekChar() == '=') {
                kind = NotStrictEqual;
                ++this->index;
            }
        }
        break;

    case '&':
        ch = this->peekChar();
        kind = BitwiseAnd;

        if (ch == '&') {
            kind = LogicalAnd;
            ++this->index;

            if (this->peekChar() == '=') {
                ++this->index;
                kind = LogicalAndEqual;
            }
        } else if (ch == '=') {
            kind = BitwiseAndEqual;
            ++this->index;
        }
        break;
    case '|':
        ch = this->peekChar();
        kind = BitwiseOr;

        if (ch == '|') {
            kind = LogicalOr;
            ++this->index;

            if (this->peekChar() == '=') {
                ++this->index;
                kind = LogicalOrEqual;
            }
        } else if (ch == '=') {
            kind = BitwiseOrEqual;
            ++this->index;
        }
        break;

    case '^':
        kind = BitwiseXor;

        if (this->peekChar() == '=') {
            kind = BitwiseXorEqual;
            ++this->index;
        }
        break;

    case '+':
        ch = this->peekChar();
        kind = Plus;

        if (ch == '+') {
            kind = PlusPlus;
            ++this->index;
        } else if (ch == '=') {
            kind = PlusEqual;
            ++this->index;
        }
        break;

    case '-':
        ch = this->peekChar();
        kind = Minus;

        if (ch == '-') {
            kind = MinusMinus;
            ++this->index;
        } else if (ch == '=') {
            kind = MinusEqual;
            ++this->index;
        }
        break;

    case '*':
        ch = this->peekChar();
        kind = Multiply;

        if (ch == '=') {
            kind = MultiplyEqual;
            ++this->index;
        } else if (ch == '*') {
            kind = Exponentiation;
            ++this->index;

            if (this->peekChar() == '=') {
                kind = ExponentiationEqual;
                ++this->index;
            }
        }
        break;

    case '/':
        kind = Divide;

        if (this->peekChar() == '=') {
            kind = DivideEqual;
            ++this->index;
        }
        break;

    case '%':
        kind = Mod;

        if (this->peekChar() == '=') {
            kind = ModEqual;
            ++this->index;
        }
        break;

    case '#':
        kind = Hash;
        if (this->index == 1 && this->peekChar() == '!') {
            kind = HashBang;
            ++this->index;
        }
        break;

    default:
        this->throwUnexpectedToken();
        kind = PunctuatorKindEnd;
        break;
    }

    token->setPunctuatorResult(this->lineNumber, this->lineStart, start, this->index, kind);
}

void Scanner::testNumericSeparator(size_t start, bool isBigInt, bool isHex, bool isBinary, bool isOctal)
{
    for (size_t i = start; i < this->index - 1; i++) {
        char16_t ch = this->sourceCharAt(i);
        if (UNLIKELY(ch == '_' && this->sourceCharAt(i + 1) == '_')) {
            ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Only one underscore is allowed as numeric separator"), ErrorObject::SyntaxError);
        }
        if (UNLIKELY(isHex && (ch == 'x' || ch == 'X') && this->sourceCharAt(i + 1) == '_')) {
            this->throwUnexpectedToken();
        }
        if (UNLIKELY(isBinary && (ch == 'b' || ch == 'B') && this->sourceCharAt(i + 1) == '_')) {
            this->throwUnexpectedToken();
        }
        if (UNLIKELY(isOctal && (ch == 'o' || ch == 'O') && this->sourceCharAt(i + 1) == '_')) {
            this->throwUnexpectedToken();
        }
    }
    if (this->sourceCharAt(this->index - 1) == '_' || (isBigInt && this->sourceCharAt(this->index - 2) == '_')) {
        ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Numeric separators are not allowed at the end of numeric literals"), ErrorObject::SyntaxError);
    }
}

void Scanner::scanHexLiteral(Scanner::ScannerResult* token, size_t start)
{
    ASSERT(token != nullptr);
    uint64_t number = 0;
    double numberDouble = 0.0;
    bool shouldUseDouble = false;
    bool scanned = false;
    bool seenUnderscore = false;

    size_t shiftCount = 0;
    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        if (!isHexDigitOrUnderscore(ch, seenUnderscore)) {
            break;
        }
        if (UNLIKELY(ch == '_')) {
            this->index++;
            continue;
        }
        if (shouldUseDouble) {
            numberDouble = numberDouble * 16 + toHexNumericValue(ch);
        } else {
            number = (number << 4) + toHexNumericValue(ch);
            if (++shiftCount >= 16) {
                shouldUseDouble = true;
                numberDouble = number;
                number = 0;
            }
        }
        this->index++;
        scanned = true;
    }

    if (!scanned) {
        this->throwUnexpectedToken();
    }

    bool isEof = this->eof();
    bool isBigInt = !isEof && this->peekChar() == 'n';

    if (UNLIKELY(isBigInt)) {
        ++this->index;
    }

    if (UNLIKELY(!isEof && isIdentifierStart(this->peekChar()))) {
        this->throwUnexpectedToken();
    }

    if (UNLIKELY(seenUnderscore)) {
        testNumericSeparator(start, isBigInt, true, false, false);
    }

    if (shouldUseDouble) {
        ASSERT(number == 0);
        token->setNumericLiteralResult(numberDouble, this->lineNumber, this->lineStart, start, this->index, isBigInt, seenUnderscore);
    } else {
        ASSERT(numberDouble == 0.0);
        token->setNumericLiteralResult(number, this->lineNumber, this->lineStart, start, this->index, isBigInt, seenUnderscore);
    }
}

void Scanner::scanBinaryLiteral(Scanner::ScannerResult* token, size_t start)
{
    ASSERT(token != nullptr);
    uint64_t number = 0;
    bool scanned = false;
    bool seenUnderscore = false;

    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        if (ch == '0' || ch == '1') {
            number = (number << 1) + ch - '0';
            this->index++;
            scanned = true;
        } else if (ch == '_') {
            this->index++;
            seenUnderscore = true;
        } else {
            break;
        }
    }

    if (!scanned) {
        // only 0b or 0B
        this->throwUnexpectedToken();
    }

    bool isEof = this->eof();
    bool isBigInt = !isEof && this->peekChar() == 'n';

    if (UNLIKELY(isBigInt)) {
        ++this->index;
    }

    if (UNLIKELY(!isEof && (isIdentifierStart(this->peekChar()) || isDecimalDigit(this->peekChar())))) {
        this->throwUnexpectedToken();
    }

    if (UNLIKELY(seenUnderscore)) {
        testNumericSeparator(start, isBigInt, false, true, false);
    }

    token->setNumericLiteralResult(number, this->lineNumber, this->lineStart, start, this->index, isBigInt, seenUnderscore);
}

void Scanner::scanOctalLiteral(Scanner::ScannerResult* token, char16_t prefix, size_t start, bool isLegacyOctal)
{
    ASSERT(token != nullptr);
    uint64_t number = 0;
    bool scanned = false;
    bool octal = isOctalDigit(prefix);
    bool seenUnderscore = false;

    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        if (!isLegacyOctal) {
            if (UNLIKELY(ch == '_')) {
                this->index++;
                seenUnderscore = true;
                continue;
            }
        }

        if (!isOctalDigit(ch)) {
            break;
        }

        number = (number << 3) + ch - '0';
        this->index++;
        scanned = true;
    }

    if (!octal && !scanned) {
        // only 0o or 0O
        throwUnexpectedToken();
    }

    bool isEof = this->eof();
    bool isBigInt = !isEof && !isLegacyOctal && this->peekChar() == 'n';

    if (UNLIKELY(isBigInt)) {
        ++this->index;
    }

    char16_t ch = this->peekChar();
    if (isIdentifierStart(ch) || isDecimalDigit(ch)) {
        throwUnexpectedToken();
    }

    if (UNLIKELY(seenUnderscore)) {
        testNumericSeparator(start, isBigInt, false, false, true);
    }

    token->setNumericLiteralResult(number, this->lineNumber, this->lineStart, start, this->index, isBigInt, seenUnderscore);
    token->octal = octal;
}

bool Scanner::isImplicitOctalLiteral()
{
    // Implicit octal, unless there is a non-octal digit.
    // (Annex B.1.1 on Numeric Literals)
    for (size_t i = this->index + 1; i < this->length; ++i) {
        const char16_t ch = this->sourceCharAt(i);
        if (ch == '8' || ch == '9') {
            return false;
        }
        if (!isOctalDigit(ch)) {
            return true;
        }
    }
    return true;
}

void Scanner::scanNumericLiteral(Scanner::ScannerResult* token)
{
    ASSERT(token != nullptr);
    const size_t start = this->index;
    char16_t ch = this->peekChar();
    char16_t startChar = ch;
    ASSERT(isDecimalDigit(ch) || (ch == '.'));
    // 'Numeric literal must start with a decimal digit or a decimal point');

    bool seenDotOrE = false;
    bool seenUnderscore = false;

    if (ch != '.') {
        auto number = this->peekChar();
        ++this->index;
        ch = this->peekChar();

        // Hex number starts with '0x'.
        // Octal number starts with '0'.
        // Octal number in ES6 starts with '0o'.
        // Binary number in ES6 starts with '0b'.
        if (number == '0') {
            if (ch == 'x' || ch == 'X') {
                ++this->index;
                return this->scanHexLiteral(token, start);
            }
            if (ch == 'b' || ch == 'B') {
                ++this->index;
                return this->scanBinaryLiteral(token, start);
            }
            if (ch == 'o' || ch == 'O') {
                ++this->index;
                return this->scanOctalLiteral(token, ch, start, false);
            }

            if (ch && isOctalDigit(ch) && this->isImplicitOctalLiteral()) {
                return this->scanOctalLiteral(token, ch, start, true);
            }
        }

        while (isDecimalDigitOrUnderscore(this->peekChar(), seenUnderscore)) {
            ++this->index;
        }
        ch = this->peekChar();
    }

    if (ch == '.') {
        seenDotOrE = true;
        ++this->index;
        while (isDecimalDigitOrUnderscore(this->peekChar(), seenUnderscore)) {
            ++this->index;
        }
        ch = this->peekChar();
    }

    if (ch == 'e' || ch == 'E') {
        seenDotOrE = true;
        ++this->index;

        ch = this->peekChar();
        if (ch == '+' || ch == '-') {
            ++this->index;
            ch = this->peekChar();
        }

        if (isDecimalDigit(ch)) {
            do {
                ++this->index;
                ch = this->peekChar();
            } while (isDecimalDigitOrUnderscore(ch, seenUnderscore));
        } else {
            this->throwUnexpectedToken();
        }
    }

    bool isEof = this->eof();
    bool isBigInt = !isEof && this->peekChar() == 'n';

    if (UNLIKELY(isBigInt)) {
        if (seenDotOrE || (startChar == '0' && (this->index - start) > 1)) {
            this->throwUnexpectedToken();
        }
        ++this->index;
    }

    if (UNLIKELY(!isEof && isIdentifierStart(this->peekChar()))) {
        this->throwUnexpectedToken();
    }

    if (UNLIKELY(seenUnderscore)) {
        if (this->sourceCharAt(start) == '0') {
            ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Numeric separator can not be used after leading 0"), ErrorObject::SyntaxError);
        }

        for (size_t i = start; i < this->index - 1; i++) {
            char16_t ch = this->sourceCharAt(i);
            if (UNLIKELY(ch == '_' && this->sourceCharAt(i + 1) == '_')) {
                ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Only one underscore is allowed as numeric separator"), ErrorObject::SyntaxError);
            }
            if (UNLIKELY(ch == '_' && (this->sourceCharAt(i + 1) == 'e' || this->sourceCharAt(i + 1) == 'E'))) {
                ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Numeric separator may not appear adjacent to ExponentPart"), ErrorObject::SyntaxError);
            }
            if (UNLIKELY(ch == '.' && this->sourceCharAt(i + 1) == '_')) {
                this->throwUnexpectedToken();
            }
        }
        if (this->sourceCharAt(this->index - 1) == '_' || (isBigInt && this->sourceCharAt(this->index - 2) == '_')) {
            ErrorHandler::throwError(start, this->lineNumber, start - this->lineStart + 1, new ASCIIString("Numeric separators are not allowed at the end of numeric literals"), ErrorObject::SyntaxError);
        }
    }

    token->setNumericLiteralResult(0, this->lineNumber, this->lineStart, start, this->index, true, seenUnderscore);
    if (UNLIKELY(startChar == '0' && !seenDotOrE && (this->index - start) > (isBigInt ? 2 : 1))) {
        token->startWithZero = true;
    }
}

void Scanner::scanStringLiteral(Scanner::ScannerResult* token)
{
    ASSERT(token != nullptr);
    const size_t start = this->index;
    char16_t quote = this->peekChar();
    ASSERT((quote == '\'' || quote == '"'));
    // 'String literal must starts with a quote');

    ++this->index;
    bool octal = false;
    bool isPlainCase = true;

    while (LIKELY(!this->eof())) {
        char16_t ch = this->peekCharWithoutEOF();
        ++this->index;
        if (ch == quote) {
            quote = '\0';
            break;
        } else if (UNLIKELY(ch == '\\')) {
            ch = this->peekChar();
            ++this->index;
            isPlainCase = false;
            if (!ch || !isLineTerminator(ch)) {
                switch (ch) {
                case 'u':
                    if (this->peekChar() == '{') {
                        ++this->index;
                        this->scanUnicodeCodePointEscape();
                    } else if (this->scanHexEscape(ch) == EMPTY_CODE_POINT) {
                        this->throwUnexpectedToken(Messages::InvalidHexEscapeSequence);
                    }
                    break;
                case 'x':
                    if (this->scanHexEscape(ch) == EMPTY_CODE_POINT) {
                        this->throwUnexpectedToken(Messages::InvalidHexEscapeSequence);
                    }
                    break;
                case 'n':
                case 'r':
                case 't':
                case 'b':
                case 'f':
                case 'v':
                    break;

                default:
                    if (ch && isOctalDigit(ch)) {
                        octal |= (this->octalToDecimal(ch, false) != NON_OCTAL_VALUE);
                    } else if (isDecimalDigit(ch)) {
                        octal = true;
                    }
                    break;
                }
            } else {
                ++this->lineNumber;
                if (ch == '\r' && this->peekChar() == '\n') {
                    ++this->index;
                } else if (ch == '\n' && this->peekChar() == '\r') {
                    ++this->index;
                }
                this->lineStart = this->index;
            }
        } else if (UNLIKELY(ch < 128 && (g_asciiRangeCharMap[ch] & LexerIsCharLineTerminator))) {
            // while parsing string literal, we should not end parsing string token with 0x2028 or 0x2029
            break;
        }
    }

    if (quote != '\0') {
        this->index = start;
        this->throwUnexpectedToken();
    }

    if (isPlainCase) {
        token->setResult(Token::StringLiteralToken, start + 1, this->index - 1, this->lineNumber, this->lineStart, start, this->index, octal);
    } else {
        // build string if needs
        token->setResult(Token::StringLiteralToken, (String*)nullptr, this->lineNumber, this->lineStart, start, this->index, octal);
    }
}

bool Scanner::isFutureReservedWord(const ParserStringView& id)
{
    const StringBufferAccessData& data = id.bufferAccessData();
    switch (data.length) {
    case 4:
        return data.equalsSameLength("enum");
    case 5:
        return data.equalsSameLength("super");
    case 6:
        return data.equalsSameLength("export") || data.equalsSameLength("import");
    }
    return false;
}

bool Scanner::isStrictModeReservedWord(::Escargot::Context* ctx, const AtomicString& identifier)
{
    switch (identifier.string()->length()) {
    case 3: // let
        return identifier == ctx->staticStrings().let;
    case 5: // yield
        return identifier == ctx->staticStrings().yield;
    case 6: // static public
        return identifier == ctx->staticStrings().stringStatic || identifier == ctx->staticStrings().stringPublic;
    case 7: // private package
        return identifier == ctx->staticStrings().stringPrivate || identifier == ctx->staticStrings().package;
    case 9: // protected interface
        return identifier == ctx->staticStrings().stringProtected || identifier == ctx->staticStrings().interface;
    case 10: // implements
        return identifier == ctx->staticStrings().implements;
    }

    return false;
}

void Scanner::scanTemplate(Scanner::ScannerResult* token, bool head)
{
    ASSERT(token != nullptr);
    // TODO apply rope-string
    UTF16StringDataNonGCStd cooked;
    UTF16StringDataNonGCStd raw;
    bool terminated = false;
    Optional<esprima::Error*> error;
    size_t start = this->index;
    size_t indexForError = this->index;
    bool tail = false;

    try {
        while (!this->eof()) {
            char16_t ch = this->peekCharWithoutEOF();
            ++this->index;
            indexForError = this->index;
            if (ch == '`') {
                tail = true;
                terminated = true;
                break;
            } else if (ch == '$') {
                if (this->peekChar() == '{') {
                    ++this->index;
                    indexForError = this->index;
                    terminated = true;
                    break;
                }
                cooked += ch;
                raw += ch;
            } else if (ch == '\\') {
                raw += ch;
                ch = this->peekChar();
                if (!isLineTerminator(ch)) {
                    auto currentIndex = this->index;
                    ++this->index;
                    switch (ch) {
                    case 'n':
                        cooked += '\n';
                        break;
                    case 'r':
                        cooked += '\r';
                        break;
                    case 't':
                        cooked += '\t';
                        break;
                    case 'u':
                        if (this->peekChar() == '{') {
                            ++this->index;
                            cooked += this->scanUnicodeCodePointEscape();
                        } else {
                            const size_t restore = this->index;
                            const char32_t unescaped = this->scanHexEscape(ch);
                            if (unescaped != EMPTY_CODE_POINT) {
                                ParserCharPiece piece(unescaped);
                                cooked += UTF16StringDataNonGCStd(piece.data, piece.length);
                            } else {
                                this->throwUnexpectedToken(Messages::InvalidHexEscapeSequence);
                            }
                        }
                        break;
                    case 'x': {
                        const char32_t unescaped = this->scanHexEscape(ch);
                        if (unescaped == EMPTY_CODE_POINT) {
                            this->throwUnexpectedToken(Messages::InvalidHexEscapeSequence);
                        }
                        ParserCharPiece piece(unescaped);
                        cooked += UTF16StringDataNonGCStd(piece.data, piece.length);
                        break;
                    }
                    case 'b':
                        cooked += '\b';
                        break;
                    case 'f':
                        cooked += '\f';
                        break;
                    case 'v':
                        cooked += '\v';
                        break;
                    default:
                        if (ch == '0') {
                            if (isDecimalDigit(this->peekChar())) {
                                // Illegal: \01 \02 and so on
                                this->throwUnexpectedToken(Messages::TemplateOctalLiteral);
                            }
                            cooked += (char16_t)'\0';
                        } else if (isOctalDigit(ch)) {
                            // Illegal: \1 \2
                            this->throwUnexpectedToken(Messages::TemplateOctalLiteral);
                        } else {
                            cooked += ch;
                        }
                        break;
                    }
                    auto endIndex = this->index;
                    for (size_t i = currentIndex; i < endIndex; i++) {
                        raw += this->sourceCharAt(i);
                    }
                } else {
                    ++this->index;
                    indexForError = this->index;
                    ++this->lineNumber;
                    if (ch == '\r' && this->peekChar() == '\n') {
                        ++this->index;
                        indexForError = this->index;
                    }
                    if (ch == 0x2028 || ch == 0x2029) {
                        raw += ch;
                    } else {
                        raw += '\n';
                    }
                    this->lineStart = this->index;
                }
            } else if (isLineTerminator(ch)) {
                ++this->lineNumber;
                if (ch == '\r' && this->peekChar() == '\n') {
                    ++this->index;
                    indexForError = this->index;
                }
                if (ch == 0x2028 || ch == 0x2029) {
                    raw += ch;
                    cooked += ch;
                } else {
                    raw += '\n';
                    cooked += '\n';
                }
                this->lineStart = this->index;
            } else {
                cooked += ch;
                raw += ch;
            }
        }

        if (!terminated) {
            this->throwUnexpectedToken();
        }
    } catch (esprima::Error* err) {
        error = new (GC) esprima::Error(*err);
        delete err;
        this->index = indexForError;

        while (!this->eof()) {
            char16_t ch = this->peekCharWithoutEOF();
            ++this->index;
            if (ch == '`') {
                tail = true;
                terminated = true;
                break;
            } else if (ch == '$') {
                if (this->peekChar() == '{') {
                    ++this->index;
                    terminated = true;
                    break;
                }
                cooked += ch;
                raw += ch;
            } else if (isLineTerminator(ch)) {
                ++this->lineNumber;
                if (ch == '\r' && this->peekChar() == '\n') {
                    ++this->index;
                }
                if (ch == 0x2028 || ch == 0x2029) {
                    raw += ch;
                } else {
                    raw += '\n';
                }
                this->lineStart = this->index;
            } else {
                raw += ch;
            }
        }
    }

    ScanTemplateResult* result = new ScanTemplateResult();
    result->head = head;
    result->tail = tail;
    result->valueRaw = UTF16StringData(raw.data(), raw.length());
    if (error) {
        result->error = error;
    } else {
        result->valueCooked = UTF16StringData(cooked.data(), cooked.length());
    }

    if (head) {
        start--;
    }

    token->setTemplateTokenResult(result, this->lineNumber, this->lineStart, start, this->index);
}

String* Scanner::scanRegExpBody()
{
    char16_t ch = this->peekChar();
    ASSERT(ch == '/');
    // assert(ch == '/', 'Regular expression literal must start with a slash');

    // TODO apply rope-string
    char16_t ch0 = this->peekChar();
    ++this->index;
    UTF16StringDataNonGCStd str(&ch0, 1);
    bool classMarker = false;
    bool terminated = false;

    while (!this->eof()) {
        ch = this->peekCharWithoutEOF();
        ++this->index;
        str += ch;
        if (ch == '\\') {
            ch = this->peekChar();
            ++this->index;
            // ECMA-262 7.8.5
            if (isLineTerminator(ch)) {
                this->throwUnexpectedToken(Messages::UnterminatedRegExp);
            }
            str += ch;
        } else if (isLineTerminator(ch)) {
            this->throwUnexpectedToken(Messages::UnterminatedRegExp);
        } else if (classMarker) {
            if (ch == ']') {
                classMarker = false;
            }
        } else {
            if (ch == '/') {
                terminated = true;
                break;
            } else if (ch == '[') {
                classMarker = true;
            }
        }
    }

    if (!terminated) {
        this->throwUnexpectedToken(Messages::UnterminatedRegExp);
    }

    // Exclude leading and trailing slash.
    str = str.substr(1, str.length() - 2);
    if (isAllASCII(str.data(), str.length())) {
        return new ASCIIString(str.data(), str.length());
    }

    return new UTF16String(str.data(), str.length());
}

String* Scanner::scanRegExpFlags()
{
    // UTF16StringData str = '';
    UTF16StringDataNonGCStd flags;
    while (!this->eof()) {
        char16_t ch = this->peekCharWithoutEOF();
        if (!isIdentifierPart(ch)) {
            break;
        }

        ++this->index;
        if (ch == '\\' && !this->eof()) {
            ch = this->peekChar();
            if (ch == 'u') {
                ++this->index;
                const size_t restore = this->index;
                char32_t ch32 = this->scanHexEscape('u');
                if (ch32 != EMPTY_CODE_POINT) {
                    ParserCharPiece piece(ch32);
                    flags += UTF16StringDataNonGCStd(piece.data, piece.length);
                    /*
                    for (str += '\\u'; restore < this->index; ++restore) {
                        str += this->source[restore];
                    }*/
                } else {
                    this->index = restore;
                    flags += 'u';
                    // str += '\\u';
                }
                this->throwUnexpectedToken();
            } else {
                // str += '\\';
                this->throwUnexpectedToken();
            }
        } else {
            flags += ch;
            // str += ch;
        }
    }

    if (!flags.length()) {
        return String::emptyString;
    }

    if (isAllASCII(flags.data(), flags.length())) {
        return String::fromLatin1(flags.data(), flags.length());
    }

    return new UTF16String(flags.data(), flags.length());
}

void Scanner::scanRegExp(Scanner::ScannerResult* token)
{
    ASSERT(token != nullptr);
    const size_t start = this->index;

    String* body = this->scanRegExpBody();
    String* flags = this->scanRegExpFlags();
    // const value = this->testRegExp(body.value, flags.value);

    ScanRegExpResult result;
    result.body = body;
    result.flags = flags;
    token->setResult(Token::RegularExpressionToken, this->lineNumber, this->lineStart, start, this->index);
    token->valueRegExp = result;
}

// ECMA-262 11.6.2.1 Keywords
static ALWAYS_INLINE KeywordKind getKeyword(const StringBufferAccessData& data)
{
    // 'const' is specialized as Keyword in V8.
    // 'yield' and 'let' are for compatibility with SpiderMonkey and ES.next.
    // Some others are from future reserved words.

    size_t length = data.length;
    char16_t first = data.charAt(0);
    char16_t second;

    switch (first) {
    case 'a':
        switch (length) {
        case 2:
            if (data.charAt(1) == 's') {
                return AsKeyword;
            }
            break;
        case 5:
            second = data.charAt(1);
            if (second == 's' && data.equalsSameLength("async", 2)) {
                return AsyncKeyword;
            } else if (second == 'w' && data.equalsSameLength("await", 2)) {
                return AwaitKeyword;
            }
            break;
        case 9:
            if (data.equalsSameLength("arguments", 1)) {
                return ArgumentsKeyword;
            }
            break;
        }
        break;
    case 'b':
        if (length == 5 && data.equalsSameLength("break", 1)) {
            return BreakKeyword;
        }
        break;
    case 'c':
        if (length == 4) {
            if (data.equalsSameLength("case", 1)) {
                return CaseKeyword;
            }
        } else if (length == 5) {
            second = data.charAt(1);
            if (second == 'a' && data.equalsSameLength("catch", 2)) {
                return CatchKeyword;
            } else if (second == 'o' && data.equalsSameLength("const", 2)) {
                return ConstKeyword;
            } else if (second == 'l' && data.equalsSameLength("class", 2)) {
                return ClassKeyword;
            }
        } else if (length == 8 && data.equalsSameLength("continue", 1)) {
            return ContinueKeyword;
        }
        break;
    case 'd':
        switch (length) {
        case 2:
            if (data.charAt(1) == 'o') {
                return DoKeyword;
            }
            break;
        case 6:
            if (data.equalsSameLength("delete", 1)) {
                return DeleteKeyword;
            }
            break;
        case 7:
            if (data.equalsSameLength("default", 1)) {
                return DefaultKeyword;
            }
            break;
        case 8:
            if (data.equalsSameLength("debugger", 1)) {
                return DebuggerKeyword;
            }
            break;
        }
        break;
    case 'e':
        switch (length) {
        case 4:
            second = data.charAt(1);
            if (second == 'l' && data.equalsSameLength("else", 2)) {
                return ElseKeyword;
            } else if (second == 'n' && data.equalsSameLength("enum", 2)) {
                return EnumKeyword;
            } else if (second == 'v' && data.equalsSameLength("eval", 2)) {
                return EvalKeyword;
            }
            break;
        case 6:
            if (data.equalsSameLength("export", 1)) {
                return ExportKeyword;
            }
            break;
        case 7:
            if (data.equalsSameLength("extends", 1)) {
                return ExtendsKeyword;
            }
            break;
        }
        break;
    case 'f':
        switch (length) {
        case 3:
            if (data.equalsSameLength("for", 1)) {
                return ForKeyword;
            }
            break;
        case 4:
            if (data.equalsSameLength("from", 1)) {
                return FromKeyword;
            }
            break;
        case 5:
            if (data.equalsSameLength("false", 1)) {
                return FalseKeyword;
            }
            break;
        case 7:
            if (data.equalsSameLength("finally", 1)) {
                return FinallyKeyword;
            }
            break;
        case 8:
            if (data.equalsSameLength("function", 1)) {
                return FunctionKeyword;
            }
            break;
        }
        break;
    case 'g':
        if (length == 3 && data.equalsSameLength("get", 1)) {
            return GetKeyword;
        }
        break;
    case 'i':
        switch (length) {
        case 2:
            second = data.charAt(1);
            if (second == 'f') {
                return IfKeyword;
            } else if (second == 'n') {
                return InKeyword;
            }
            break;
        case 6:
            if (data.equalsSameLength("import", 1)) {
                return ImportKeyword;
            }
            break;
        case 9:
            if (data.equalsSameLength("interface", 1)) {
                return InterfaceKeyword;
            }
            break;
        case 10:
            second = data.charAt(1);
            if (second == 'n' && data.equalsSameLength("instanceof", 2)) {
                return InstanceofKeyword;
            } else if (second == 'm' && data.equalsSameLength("implements", 2)) {
                return ImplementsKeyword;
            }
            break;
        }
        break;
    case 'l':
        if (length == 3 && data.equalsSameLength("let", 1)) {
            return LetKeyword;
        }
        break;
    case 'n':
        if (length == 3 && data.equalsSameLength("new", 1)) {
            return NewKeyword;
        } else if (length == 4 && data.equalsSameLength("null", 1)) {
            return NullKeyword;
        }
        break;
    case 'o':
        if (length == 2 && data.charAt(1) == 'f') {
            return OfKeyword;
        }
        break;
    case 'p':
        switch (length) {
        case 6:
            if (data.equalsSameLength("public", 1)) {
                return PublicKeyword;
            }
            break;
        case 7:
            second = data.charAt(1);
            if (second == 'a' && data.equalsSameLength("package", 2)) {
                return PackageKeyword;
            } else if (second == 'r' && data.equalsSameLength("private", 2)) {
                return PrivateKeyword;
            }
            break;
        case 9:
            if (data.equalsSameLength("protected", 1)) {
                return ProtectedKeyword;
            }
            break;
        }
        break;
    case 'r':
        if (length == 6 && data.equalsSameLength("return", 1)) {
            return ReturnKeyword;
        }
        break;
    case 's':
        switch (length) {
        case 3:
            if (data.equalsSameLength("set", 1)) {
                return SetKeyword;
            }
            break;
        case 5:
            if (data.equalsSameLength("super", 1)) {
                return SuperKeyword;
            }
            break;
        case 6:
            second = data.charAt(1);
            if (second == 'w' && data.equalsSameLength("switch", 2)) {
                return SwitchKeyword;
            } else if (second == 't' && data.equalsSameLength("static", 2)) {
                return StaticKeyword;
            }
            break;
        }
        break;
    case 't':
        switch (length) {
        case 3:
            if (data.equalsSameLength("try", 1)) {
                return TryKeyword;
            }
            break;
        case 4:
            second = data.charAt(1);
            if (second == 'h' && data.equalsSameLength("this", 2)) {
                return ThisKeyword;
            } else if (second == 'r' && data.equalsSameLength("true", 2)) {
                return TrueKeyword;
            }
            break;
        case 5:
            if (data.equalsSameLength("throw", 1)) {
                return ThrowKeyword;
            }
            break;
        case 6:
            if (data.equalsSameLength("typeof", 1)) {
                return TypeofKeyword;
            }
            break;
        }
        break;
    case 'v':
        if (length == 3 && data.equalsSameLength("var", 1)) {
            return VarKeyword;
        } else if (length == 4 && data.equalsSameLength("void", 1)) {
            return VoidKeyword;
        }
        break;
    case 'w':
        if (length == 4 && data.equalsSameLength("with", 1)) {
            return WithKeyword;
        } else if (length == 5 && data.equalsSameLength("while", 1)) {
            return WhileKeyword;
        }
        break;
    case 'y':
        if (length == 5 && data.equalsSameLength("yield", 1)) {
            return YieldKeyword;
        }
        break;
    }
    return NotKeyword;
}

ALWAYS_INLINE void Scanner::scanIdentifier(Scanner::ScannerResult* token, char16_t ch0)
{
    ASSERT(token != nullptr);
    Token type = Token::IdentifierToken;
    const size_t start = this->index;

    // Backslash (U+005C) starts an escaped character.
    ScanIDResult id = UNLIKELY(ch0 == 0x5C) ? this->getComplexIdentifier() : this->getIdentifier();
    const auto& data = std::get<0>(id);
    const size_t end = this->index;

    // There is no keyword or literal with only one character.
    // Thus, it must be an identifier.
    if (data.length > 1) {
        KeywordKind keywordKind = getKeyword(data);

        token->secondaryKeywordKind = keywordKind;

        switch (keywordKind) {
        case NotKeyword:
            break;
        case NullKeyword:
            type = Token::NullLiteralToken;
            break;
        case TrueKeyword:
        case FalseKeyword:
            type = BooleanLiteralToken;
            break;
        case YieldKeyword:
        case LetKeyword:
            token->setKeywordResult(this->lineNumber, this->lineStart, start, this->index, keywordKind);
            return;
        default:
            if (keywordKind >= StrictModeReservedWord) {
                break;
            }
            token->setKeywordResult(this->lineNumber, this->lineStart, start, this->index, keywordKind);
            return;
        }
    }

    if (UNLIKELY(std::get<1>(id) != nullptr)) {
        token->setResult(type, std::get<1>(id), this->lineNumber, this->lineStart, start, end);
    } else {
        token->setResult(type, start, end, this->lineNumber, this->lineStart, start, end);
    }
}

void Scanner::lex(Scanner::ScannerResult* token)
{
    ASSERT(token != nullptr);

    token->resetResult();

    if (UNLIKELY(this->eof())) {
        token->setResult(Token::EOFToken, this->lineNumber, this->lineStart, this->index, this->index);
        return;
    }

    char16_t cp = this->peekCharWithoutEOF();

    if (UNLIKELY(cp >= 128 && cp >= 0xD800 && cp < 0xDFFF)) {
        ++this->index;
        char32_t ch2 = this->peekChar();
        if (U16_IS_TRAIL(ch2)) {
            cp = U16_GET_SUPPLEMENTARY(cp, ch2);
        } else {
            this->throwUnexpectedToken();
        }
    }

    if (isIdentifierStart(cp)) {
        goto ScanID;
    }
    // String literal starts with single quote (U+0027) or double quote (U+0022).
    if (cp == 0x27 || cp == 0x22) {
        this->scanStringLiteral(token);
        return;
    }

    // Dot (.) U+002E can also start a floating-point number, hence the need
    // to check the next character.
    if (UNLIKELY(cp == 0x2E) && isDecimalDigit(this->sourceCharAt(this->index + 1))) {
        this->scanNumericLiteral(token);
        return;
    }

    if (isDecimalDigit(cp)) {
        this->scanNumericLiteral(token);
        return;
    }

    if (UNLIKELY(cp == '`')) {
        ++this->index;
        this->scanTemplate(token, true);
        return;
    }
    // Possible identifier start in a surrogate pair.
    if (UNLIKELY(cp >= 0xD800 && cp < 0xDFFF) && isIdentifierStart(this->codePointAt(this->index))) {
        goto ScanID;
    }

    this->scanPunctuator(token, cp);
    return;

ScanID:
    this->scanIdentifier(token, cp);
    return;
}
} // namespace Escargot
