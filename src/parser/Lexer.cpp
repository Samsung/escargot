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
#include "parser/Lexer.h"

// These two must be the last because they overwrite the ASSERT macro.
#include "double-conversion.h"
#include "ieee.h"

using namespace Escargot::EscargotLexer;

namespace Escargot {

const char* Messages::UnexpectedTokenIllegal = "Unexpected token ILLEGAL";
const char* Messages::UnterminatedRegExp = "Invalid regular expression: missing /";
const char* Messages::TemplateOctalLiteral = "Octal literals are not allowed in template strings.";

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

    return (ch == 0x1680 || ch == 0x180E || ch == 0x2000 || ch == 0x2001
            || ch == 0x2002 || ch == 0x2003 || ch == 0x2004 || ch == 0x2005 || ch == 0x2006
            || ch == 0x2007 || ch == 0x2008 || ch == 0x2009 || ch == 0x200A || ch == 0x202F
            || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF);
}

/* Starting codepoints of identifier ranges. */
static const uint16_t identRangeStart[429] = {
    170, 181, 183, 186, 192, 216, 248, 710, 736, 748, 750, 768, 886, 890, 895, 902, 908, 910, 931, 1015, 1155, 1162,
    1329, 1369, 1377, 1425, 1471, 1473, 1476, 1479, 1488, 1520, 1552, 1568, 1646, 1749, 1759, 1770, 1791, 1808, 1869,
    1984, 2042, 2048, 2112, 2208, 2276, 2406, 2417, 2437, 2447, 2451, 2474, 2482, 2486, 2492, 2503, 2507, 2519, 2524,
    2527, 2534, 2561, 2565, 2575, 2579, 2602, 2610, 2613, 2616, 2620, 2622, 2631, 2635, 2641, 2649, 2654, 2662, 2689,
    2693, 2703, 2707, 2730, 2738, 2741, 2748, 2759, 2763, 2768, 2784, 2790, 2817, 2821, 2831, 2835, 2858, 2866, 2869,
    2876, 2887, 2891, 2902, 2908, 2911, 2918, 2929, 2946, 2949, 2958, 2962, 2969, 2972, 2974, 2979, 2984, 2990, 3006,
    3014, 3018, 3024, 3031, 3046, 3072, 3077, 3086, 3090, 3114, 3133, 3142, 3146, 3157, 3160, 3168, 3174, 3201, 3205,
    3214, 3218, 3242, 3253, 3260, 3270, 3274, 3285, 3294, 3296, 3302, 3313, 3329, 3333, 3342, 3346, 3389, 3398, 3402,
    3415, 3424, 3430, 3450, 3458, 3461, 3482, 3507, 3517, 3520, 3530, 3535, 3542, 3544, 3558, 3570, 3585, 3648, 3664,
    3713, 3716, 3719, 3722, 3725, 3732, 3737, 3745, 3749, 3751, 3754, 3757, 3771, 3776, 3782, 3784, 3792, 3804, 3840,
    3864, 3872, 3893, 3895, 3897, 3902, 3913, 3953, 3974, 3993, 4038, 4096, 4176, 4256, 4295, 4301, 4304, 4348, 4682,
    4688, 4696, 4698, 4704, 4746, 4752, 4786, 4792, 4800, 4802, 4808, 4824, 4882, 4888, 4957, 4969, 4992, 5024, 5121,
    5743, 5761, 5792, 5870, 5888, 5902, 5920, 5952, 5984, 5998, 6002, 6016, 6103, 6108, 6112, 6155, 6160, 6176, 6272,
    6320, 6400, 6432, 6448, 6470, 6512, 6528, 6576, 6608, 6656, 6688, 6752, 6783, 6800, 6823, 6832, 6912, 6992, 7019,
    7040, 7168, 7232, 7245, 7376, 7380, 7416, 7424, 7676, 7960, 7968, 8008, 8016, 8025, 8027, 8029, 8031, 8064, 8118,
    8126, 8130, 8134, 8144, 8150, 8160, 8178, 8182, 8204, 8255, 8276, 8305, 8319, 8336, 8400, 8417, 8421, 8450, 8455,
    8458, 8469, 8472, 8484, 8486, 8488, 8490, 8508, 8517, 8526, 8544, 11264, 11312, 11360, 11499, 11520, 11559, 11565,
    11568, 11631, 11647, 11680, 11688, 11696, 11704, 11712, 11720, 11728, 11736, 11744, 12293, 12321, 12337, 12344,
    12353, 12441, 12449, 12540, 12549, 12593, 12704, 12784, 13312, 19968, 40960, 42192, 42240, 42512, 42560, 42612,
    42623, 42655, 42775, 42786, 42891, 42896, 42928, 42999, 43072, 43136, 43216, 43232, 43259, 43264, 43312, 43360,
    43392, 43471, 43488, 43520, 43584, 43600, 43616, 43642, 43739, 43744, 43762, 43777, 43785, 43793, 43808, 43816,
    43824, 43868, 43876, 43968, 44012, 44016, 44032, 55216, 55243, 63744, 64112, 64256, 64275, 64285, 64298, 64312,
    64318, 64320, 64323, 64326, 64467, 64848, 64914, 65008, 65024, 65056, 65075, 65101, 65136, 65142, 65296, 65313,
    65343, 65345, 65382, 65474, 65482, 65490, 65498, 65535
};

/* Lengths of identifier ranges. */
static const uint8_t identRangeLength[428] = {
    1, 1, 1, 1, 23, 31, 200, 12, 5, 1, 1, 117, 2, 4, 1, 5, 1, 20, 83, 139, 5, 166, 38, 1, 39, 45, 1, 2, 2, 1, 27, 3,
    11, 74, 102, 8, 10, 19, 1, 59, 101, 54, 1, 46, 28, 19, 128, 10, 19, 8, 2, 22, 7, 1, 4, 9, 2, 4, 1, 2, 5, 12, 3, 6,
    2, 22, 7, 2, 2, 2, 1, 5, 2, 3, 1, 4, 1, 16, 3, 9, 3, 22, 7, 2, 5, 10, 3, 3, 1, 4, 10, 3, 8, 2, 22, 7, 2, 5, 9, 2,
    3, 2, 2, 5, 10, 1, 2, 6, 3, 4, 2, 1, 2, 2, 3, 12, 5, 3, 4, 1, 1, 10, 4, 8, 3, 23, 16, 8, 3, 4, 2, 2, 4, 10, 3, 8,
    3, 23, 10, 5, 9, 3, 4, 2, 1, 4, 10, 2, 3, 8, 3, 41, 8, 3, 5, 1, 4, 10, 6, 2, 18, 24, 9, 1, 7, 1, 6, 1, 8, 10, 2,
    58, 15, 10, 2, 1, 2, 1, 1, 4, 7, 3, 1, 1, 2, 13, 3, 5, 1, 6, 10, 4, 1, 2, 10, 1, 1, 1, 10, 36, 20, 18, 36, 1, 74,
    78, 38, 1, 1, 43, 201, 4, 7, 1, 4, 41, 4, 33, 4, 7, 1, 4, 15, 57, 4, 67, 3, 9, 16, 85, 202, 17, 26, 75, 11, 13, 7,
    21, 20, 13, 3, 2, 84, 1, 2, 10, 3, 10, 88, 43, 70, 31, 12, 12, 40, 5, 44, 26, 11, 28, 63, 29, 11, 10, 1, 14, 76,
    10, 9, 116, 56, 10, 49, 3, 35, 2, 203, 204, 6, 38, 6, 8, 1, 1, 1, 31, 53, 7, 1, 3, 7, 4, 6, 13, 3, 7, 2, 2, 1, 1,
    1, 13, 13, 1, 12, 1, 1, 10, 1, 6, 1, 1, 1, 16, 4, 5, 1, 41, 47, 47, 133, 9, 38, 1, 1, 56, 1, 24, 7, 7, 7, 7, 7, 7,
    7, 7, 32, 3, 15, 5, 5, 86, 7, 90, 4, 41, 94, 27, 16, 205, 206, 207, 46, 208, 28, 48, 10, 31, 83, 9, 103, 4, 30, 2,
    49, 52, 69, 10, 24, 1, 46, 36, 29, 65, 11, 31, 55, 14, 10, 23, 73, 3, 16, 5, 6, 6, 6, 7, 7, 43, 4, 2, 43, 2, 10,
    209, 23, 49, 210, 106, 7, 5, 12, 13, 5, 1, 2, 2, 108, 211, 64, 54, 12, 16, 14, 2, 3, 5, 135, 10, 26, 1, 26, 89, 6,
    6, 6, 3
};

/* Lengths of identifier ranges greater than IDENT_RANGE_LONG. */
static const uint16_t identRangeLongLength[12] = {
    458, 333, 620, 246, 282, 6582, 20941, 1165, 269, 11172, 366, 363
};

static NEVER_INLINE bool isIdentifierPartSlow(char32_t ch)
{
    int bottom = 0;
    int top = (sizeof(identRangeStart) / sizeof(uint16_t)) - 1;

    while (true) {
        int middle = (bottom + top) >> 1;
        char32_t rangeStart = identRangeStart[middle];

        if (ch >= rangeStart) {
            if (ch < identRangeStart[middle + 1]) {
                char32_t length = identRangeLength[middle];

                if (UNLIKELY(length >= IDENT_RANGE_LONG)) {
                    length = identRangeLongLength[length - IDENT_RANGE_LONG];
                }
                return ch < rangeStart + length;
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

    return isIdentifierPartSlow(ch);
}

static ALWAYS_INLINE bool isIdentifierStart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & LexerIsCharIdentStart;
    }

    return isIdentifierPartSlow(ch);
}

static ALWAYS_INLINE bool isDecimalDigit(char16_t ch)
{
    return (ch >= '0' && ch <= '9');
}

static ALWAYS_INLINE bool isHexDigit(char16_t ch)
{
    return isDecimalDigit(ch) || ((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f');
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
        return ctx->staticStrings().stringVar;
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
        return ctx->staticStrings().stringWith;
    case EnumKeyword:
        return ctx->staticStrings().stringEnum;
    case AwaitKeyword:
        return ctx->staticStrings().stringAwait;
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
        return ctx->staticStrings().stringSuper;
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
        return ctx->staticStrings().stringFinally;
    case ExtendsKeyword:
        return ctx->staticStrings().stringExtends;
    case FunctionKeyword:
        return ctx->staticStrings().stringFunction;
    case ContinueKeyword:
        return ctx->staticStrings().stringContinue;
    case DebuggerKeyword:
        return ctx->staticStrings().stringDebugger;
    case InstanceofKeyword:
        return ctx->staticStrings().stringInstanceof;
    case ImplementsKeyword:
        return ctx->staticStrings().stringImplements;
    case InterfaceKeyword:
        return ctx->staticStrings().stringInterface;
    case PackageKeyword:
        return ctx->staticStrings().stringPackage;
    case PrivateKeyword:
        return ctx->staticStrings().stringPrivate;
    case ProtectedKeyword:
        return ctx->staticStrings().stringProtected;
    case PublicKeyword:
        return ctx->staticStrings().stringPublic;
    case StaticKeyword:
        return ctx->staticStrings().stringStatic;
    case YieldKeyword:
        return ctx->staticStrings().stringYield;
    case LetKeyword:
        return ctx->staticStrings().stringLet;
    default:
        ASSERT_NOT_REACHED();
        return ctx->staticStrings().stringError;
    }
}

Scanner::ScannerResult::~ScannerResult()
{
    if (this->scanner->isPoolEnabled) {
        if (this->scanner->initialResultMemoryPoolSize < SCANNER_RESULT_POOL_INITIAL_SIZE) {
            this->scanner->initialResultMemoryPool[this->scanner->initialResultMemoryPoolSize++] = this;
            return;
        }
        this->scanner->resultMemoryPool.push_back(this);
    }
}

StringView Scanner::ScannerResult::relatedSource()
{
    return StringView(scanner->source, this->start, this->end);
}

Value Scanner::ScannerResult::valueStringLiteralForAST()
{
    StringView sv = valueStringLiteral();
    if (this->plain) {
        return new SourceStringView(sv);
    }
    RELEASE_ASSERT(sv.string() != nullptr);
    return sv.string();
}

StringView Scanner::ScannerResult::valueStringLiteral()
{
    if (this->type == Token::KeywordToken && !this->hasKeywordButUseString) {
        AtomicString as = keywordToString(this->scanner->escargotContext, this->valueKeywordKind);
        return StringView(as.string(), 0, as.string()->length());
    }
    if (this->type == Token::StringLiteralToken && !plain && valueStringLiteralData.length() == 0) {
        constructStringLiteral();
    }
    ASSERT(valueStringLiteralData.getTagInFirstDataArea() == POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
    return valueStringLiteralData;
}

void Scanner::ScannerResult::constructStringLiteral()
{
    size_t indexBackup = this->scanner->index;
    size_t lineNumberBackup = this->scanner->lineNumber;
    size_t lineStartBackup = this->scanner->lineStart;

    this->scanner->index = this->start;
    const size_t start = this->start;
    char16_t quote = this->scanner->source.bufferedCharAt(start);
    ASSERT((quote == '\'' || quote == '"'));
    // 'String literal must starts with a quote');

    ++this->scanner->index;
    bool isEveryCharLatin1 = true;

    UTF16StringDataNonGCStd stringUTF16;
    while (true) {
        char16_t ch = this->scanner->source.bufferedCharAt(this->scanner->index++);

        if (ch == quote) {
            quote = '\0';
            break;
        } else if (UNLIKELY(ch == '\\')) {
            ch = this->scanner->source.bufferedCharAt(this->scanner->index++);
            if (!ch || !isLineTerminator(ch)) {
                switch (ch) {
                case 'u':
                case 'x':
                    if (this->scanner->source.bufferedCharAt(this->scanner->index) == '{') {
                        ++this->scanner->index;
                        ParserCharPiece piece(this->scanner->scanUnicodeCodePointEscape());
                        stringUTF16.append(piece.data, piece.data + piece.length);
                        if (piece.length != 1 || piece.data[0] >= 256) {
                            isEveryCharLatin1 = false;
                        }
                    } else {
                        const char32_t unescaped = this->scanner->scanHexEscape(ch);
                        ParserCharPiece piece(unescaped);
                        stringUTF16.append(piece.data, piece.data + piece.length);
                        if (piece.length != 1 || piece.data[0] >= 256) {
                            isEveryCharLatin1 = false;
                        }
                    }
                    break;
                case 'n':
                    stringUTF16 += '\n';
                    break;
                case 'r':
                    stringUTF16 += '\r';
                    break;
                case 't':
                    stringUTF16 += '\t';
                    break;
                case 'b':
                    stringUTF16 += '\b';
                    break;
                case 'f':
                    stringUTF16 += '\f';
                    break;
                case 'v':
                    stringUTF16 += '\x0B';
                    break;

                default:
                    if (ch && isOctalDigit(ch)) {
                        uint16_t octToDec = this->scanner->octalToDecimal(ch, true);
                        stringUTF16 += octToDec;
                        ASSERT(octToDec < 256);
                    } else if (isDecimalDigit(ch)) {
                        stringUTF16 += ch;
                        if (ch >= 256) {
                            isEveryCharLatin1 = false;
                        }
                    } else {
                        stringUTF16 += ch;
                        if (ch >= 256) {
                            isEveryCharLatin1 = false;
                        }
                    }
                    break;
                }
            } else {
                ++this->scanner->lineNumber;
                if (ch == '\r' && this->scanner->source.bufferedCharAt(this->scanner->index) == '\n') {
                    ++this->scanner->index;
                } else if (ch == '\n' && this->scanner->source.bufferedCharAt(this->scanner->index) == '\r') {
                    ++this->scanner->index;
                }
                this->scanner->lineStart = this->scanner->index;
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

    this->scanner->index = indexBackup;
    this->scanner->lineNumber = lineNumberBackup;
    this->scanner->lineStart = lineStartBackup;

    String* newStr;
    if (isEveryCharLatin1) {
        newStr = new Latin1String(stringUTF16.data(), stringUTF16.length());
    } else {
        newStr = new UTF16String(stringUTF16.data(), stringUTF16.length());
    }
    this->valueStringLiteralData = StringView(newStr, 0, newStr->length());
}

Scanner::Scanner(::Escargot::Context* escargotContext, StringView code, ErrorHandler* handler, size_t startLine, size_t startColumn)
{
    this->escargotContext = escargotContext;
    isPoolEnabled = true;
    source = code;
    errorHandler = handler;
    // trackComment = false;

    length = code.length();
    index = 0;
    lineNumber = ((length > 0) ? 1 : 0) + startLine;
    lineStart = startColumn;

    initialResultMemoryPoolSize = SCANNER_RESULT_POOL_INITIAL_SIZE;
    Scanner::ScannerResult* ptr = (Scanner::ScannerResult*)scannerResultInnerPool;
    for (size_t i = 0; i < SCANNER_RESULT_POOL_INITIAL_SIZE; i++) {
        ptr[i].scanner = this;
        initialResultMemoryPool[i] = &ptr[i];
    }
}

Scanner::ScannerResult* Scanner::createScannerResult()
{
    if (initialResultMemoryPoolSize) {
        initialResultMemoryPoolSize--;
        return initialResultMemoryPool[initialResultMemoryPoolSize];
    } else if (resultMemoryPool.size() == 0) {
        auto ret = (Scanner::ScannerResult*)GC_MALLOC(sizeof(Scanner::ScannerResult));
        return ret;
    } else {
        auto ret = resultMemoryPool.back();
        resultMemoryPool.pop_back();
        return ret;
    }
}

void Scanner::skipSingleLineComment(void)
{
    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
        ++this->index;

        if (isLineTerminator(ch)) {
            if (ch == 13 && this->source.bufferedCharAt(this->index) == 10) {
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
        char16_t ch = this->source.bufferedCharAt(this->index);
        if (isLineTerminator(ch)) {
            if (ch == 0x0D && this->source.bufferedCharAt(this->index + 1) == 0x0A) {
                ++this->index;
            }
            ++this->lineNumber;
            ++this->index;
            this->lineStart = this->index;
        } else if (ch == 0x2A) {
            // Block comment ends with '*/'.
            if (this->source.bufferedCharAt(this->index + 1) == 0x2F) {
                this->index += 2;
                return;
            }
            ++this->index;
        } else {
            ++this->index;
        }
    }

    throwUnexpectedToken();
}

char32_t Scanner::scanHexEscape(char prefix)
{
    size_t len = (prefix == 'u') ? 4 : 2;
    char32_t code = 0;

    for (size_t i = 0; i < len; ++i) {
        if (!this->eof() && isHexDigit(this->source.bufferedCharAt(this->index))) {
            code = code * 16 + hexValue(this->source.bufferedCharAt(this->index++));
        } else {
            return EMPTY_CODE_POINT;
        }
    }

    return code;
}

char32_t Scanner::scanUnicodeCodePointEscape()
{
    char16_t ch = this->source.bufferedCharAt(this->index);
    char32_t code = 0;

    // At least, one hex digit is required.
    if (ch == '}') {
        this->throwUnexpectedToken();
    }

    while (!this->eof()) {
        ch = this->source.bufferedCharAt(this->index++);
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

StringView Scanner::getIdentifier()
{
    const size_t start = this->index++;
    while (UNLIKELY(!this->eof())) {
        const char16_t ch = this->source.bufferedCharAt(this->index);
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

    return StringView(this->source, start, this->index);
}

StringView Scanner::getComplexIdentifier()
{
    char32_t cp = this->codePointAt(this->index);
    ParserCharPiece piece = ParserCharPiece(cp);
    UTF16StringDataNonGCStd id(piece.data, piece.length);
    this->index += id.length();

    // '\u' (U+005C, U+0075) denotes an escaped character.
    char32_t ch;
    if (cp == 0x5C) {
        if (this->source.bufferedCharAt(this->index) != 0x75) {
            this->throwUnexpectedToken();
        }
        ++this->index;
        if (this->source.bufferedCharAt(this->index) == '{') {
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
        cp = this->codePointAt(this->index);
        if (!isIdentifierPart(cp)) {
            break;
        }

        // ch = Character.fromCodePoint(cp);
        ch = cp;
        piece = ParserCharPiece(ch);
        id += UTF16StringDataNonGCStd(piece.data, piece.length);
        this->index += piece.length;

        // '\u' (U+005C, U+0075) denotes an escaped character.
        if (cp == 0x5C) {
            // id = id.substr(0, id.length - 1);
            id.erase(id.length() - 1);

            if (this->source.bufferedCharAt(this->index) != 0x75) {
                this->throwUnexpectedToken();
            }
            ++this->index;
            if (this->source.bufferedCharAt(this->index) == '{') {
                ++this->index;
                ch = this->scanUnicodeCodePointEscape();
            } else {
                ch = this->scanHexEscape('u');
                cp = ch;
                if (ch == EMPTY_CODE_POINT || ch == '\\' || !isIdentifierPart(cp)) {
                    this->throwUnexpectedToken();
                }
            }
            piece = ParserCharPiece(ch);
            id += UTF16StringDataNonGCStd(piece.data, piece.length);
        }
    }

    String* str = new UTF16String(id.data(), id.length());
    return StringView(str, 0, str->length());
}

uint16_t Scanner::octalToDecimal(char16_t ch, bool octal)
{
    // \0 is not octal escape sequence
    char16_t code = octalValue(ch);

    octal |= (ch != '0');

    if (!this->eof() && isOctalDigit(this->source.bufferedCharAt(this->index))) {
        octal = true;
        code = code * 8 + octalValue(this->source.bufferedCharAt(this->index++));

        // 3 digits are only allowed when string starts
        // with 0, 1, 2, 3
        // if ('0123'.indexOf(ch) >= 0 && !this->eof() && Character.isOctalDigit(this->source.charCodeAt(this->index))) {
        if ((ch >= '0' && ch <= '3') && !this->eof() && isOctalDigit(this->source.bufferedCharAt(this->index))) {
            code = code * 8 + octalValue(this->source.bufferedCharAt(this->index++));
        }
    }

    ASSERT(!octal || code < NON_OCTAL_VALUE);
    return octal ? code : NON_OCTAL_VALUE;
};

PassRefPtr<Scanner::ScannerResult> Scanner::scanPunctuator(char16_t ch0)
{
    PassRefPtr<Scanner::ScannerResult> token = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::PunctuatorToken, this->lineNumber, this->lineStart, this->index, this->index));

    PunctuatorKind kind;
    // Check for most common single-character punctuators.
    size_t start = this->index;
    char16_t ch1, ch2, ch3;
    switch (ch0) {
    case '(':
        ++this->index;
        kind = LeftParenthesis;
        break;

    case '{':
        ++this->index;
        kind = LeftBrace;
        break;

    case '.':
        ++this->index;
        kind = Period;
        if (this->source.bufferedCharAt(this->index) == '.' && this->source.bufferedCharAt(this->index + 1) == '.') {
            // Spread operator: ...
            this->index += 2;
            // resultStr = "...";
            kind = PeriodPeriodPeriod;
        }
        break;

    case '}':
        ++this->index;
        kind = RightBrace;
        break;
    case ')':
        kind = RightParenthesis;
        ++this->index;
        break;
    case ';':
        kind = SemiColon;
        ++this->index;
        break;
    case ',':
        kind = Comma;
        ++this->index;
        break;
    case '[':
        kind = LeftSquareBracket;
        ++this->index;
        break;
    case ']':
        kind = RightSquareBracket;
        ++this->index;
        break;
    case ':':
        kind = Colon;
        ++this->index;
        break;
    case '?':
        kind = GuessMark;
        ++this->index;
        break;
    case '~':
        kind = Wave;
        ++this->index;
        break;

    case '>':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '>') {
            ch2 = this->source.bufferedCharAt(this->index + 2);
            if (ch2 == '>') {
                ch3 = this->source.bufferedCharAt(this->index + 3);
                if (ch3 == '=') {
                    this->index += 4;
                    kind = UnsignedRightShiftEqual;
                } else {
                    kind = UnsignedRightShift;
                    this->index += 3;
                }
            } else if (ch2 == '=') {
                kind = RightShiftEqual;
                this->index += 3;
            } else {
                kind = RightShift;
                this->index += 2;
            }
        } else if (ch1 == '=') {
            kind = RightInequalityEqual;
            this->index += 2;
        } else {
            kind = RightInequality;
            this->index += 1;
        }
        break;

    case '<':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '<') {
            ch2 = this->source.bufferedCharAt(this->index + 2);
            if (ch2 == '=') {
                kind = LeftShiftEqual;
                this->index += 3;
            } else {
                kind = LeftShift;
                this->index += 2;
            }
        } else if (ch1 == '=') {
            kind = LeftInequalityEqual;
            this->index += 2;
        } else {
            kind = LeftInequality;
            this->index += 1;
        }
        break;

    case '=':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            ch2 = this->source.bufferedCharAt(this->index + 2);
            if (ch2 == '=') {
                kind = StrictEqual;
                this->index += 3;
            } else {
                kind = Equal;
                this->index += 2;
            }
        } else if (ch1 == '>') {
            kind = Arrow;
            this->index += 2;
        } else {
            kind = Substitution;
            this->index += 1;
        }
        break;

    case '!':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            ch2 = this->source.bufferedCharAt(this->index + 2);
            if (ch2 == '=') {
                kind = NotStrictEqual;
                this->index += 3;
            } else {
                kind = NotEqual;
                this->index += 2;
            }
        } else {
            kind = ExclamationMark;
            this->index += 1;
        }
        break;

    case '&':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '&') {
            kind = LogicalAnd;
            this->index += 2;
        } else if (ch1 == '=') {
            kind = BitwiseAndEqual;
            this->index += 2;
        } else {
            kind = BitwiseAnd;
            this->index += 1;
        }
        break;

    case '|':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '|') {
            kind = LogicalOr;
            this->index += 2;
        } else if (ch1 == '=') {
            kind = BitwiseOrEqual;
            this->index += 2;
        } else {
            kind = BitwiseOr;
            this->index += 1;
        }
        break;

    case '^':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            kind = BitwiseXorEqual;
            this->index += 2;
        } else {
            kind = BitwiseXor;
            this->index += 1;
        }
        break;

    case '+':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '+') {
            kind = PlusPlus;
            this->index += 2;
        } else if (ch1 == '=') {
            kind = PlusEqual;
            this->index += 2;
        } else {
            kind = Plus;
            this->index += 1;
        }
        break;

    case '-':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '-') {
            kind = MinusMinus;
            this->index += 2;
        } else if (ch1 == '=') {
            kind = MinusEqual;
            this->index += 2;
        } else {
            kind = Minus;
            this->index += 1;
        }
        break;

    case '*':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            kind = MultiplyEqual;
            this->index += 2;
        } else {
            kind = Multiply;
            this->index += 1;
        }
        break;

    case '/':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            kind = DivideEqual;
            this->index += 2;
        } else {
            kind = Divide;
            this->index += 1;
        }
        break;

    case '%':
        ch1 = this->source.bufferedCharAt(this->index + 1);
        if (ch1 == '=') {
            kind = ModEqual;
            this->index += 2;
        } else {
            kind = Mod;
            this->index += 1;
        }
        break;

    default:
        kind = PunctuatorKindEnd;
        break;
    }

    if (UNLIKELY(this->index == token->start)) {
        this->throwUnexpectedToken();
    }

    token->valuePunctuatorKind = kind;
    return token;
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanHexLiteral(size_t start)
{
    uint64_t number = 0;
    double numberDouble = 0.0;
    bool shouldUseDouble = false;
    bool scanned = false;

    size_t shiftCount = 0;
    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
        if (!isHexDigit(ch)) {
            break;
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

    if (isIdentifierStart(this->source.bufferedCharAt(this->index))) {
        this->throwUnexpectedToken();
    }

    if (shouldUseDouble) {
        ASSERT(number == 0);
        return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::NumericLiteralToken, numberDouble, this->lineNumber, this->lineStart, start, this->index));
    } else {
        ASSERT(numberDouble == 0.0);
        return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
    }
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanBinaryLiteral(size_t start)
{
    uint64_t number = 0;
    bool scanned = false;

    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
        if (ch != '0' && ch != '1') {
            break;
        }
        number = (number << 1) + ch - '0';
        this->index++;
        scanned = true;
    }

    if (!scanned) {
        // only 0b or 0B
        this->throwUnexpectedToken();
    }

    if (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
        /* istanbul ignore else */
        if (isIdentifierStart(ch) || isDecimalDigit(ch)) {
            this->throwUnexpectedToken();
        }
    }

    return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanOctalLiteral(char16_t prefix, size_t start)
{
    uint64_t number = 0;
    bool scanned = false;
    bool octal = isOctalDigit(prefix);

    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
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

    if (isIdentifierStart(this->source.bufferedCharAt(this->index)) || isDecimalDigit(this->source.bufferedCharAt(this->index))) {
        throwUnexpectedToken();
    }
    PassRefPtr<Scanner::ScannerResult> ret = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
    ret->octal = octal;

    return ret;
}

bool Scanner::isImplicitOctalLiteral()
{
    // Implicit octal, unless there is a non-octal digit.
    // (Annex B.1.1 on Numeric Literals)
    for (size_t i = this->index + 1; i < this->length; ++i) {
        const char16_t ch = this->source.bufferedCharAt(i);
        if (ch == '8' || ch == '9') {
            return false;
        }
        if (!isOctalDigit(ch)) {
            return true;
        }
    }
    return true;
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanNumericLiteral()
{
    const size_t start = this->index;
    char16_t ch = this->source.bufferedCharAt(start);
    char16_t startChar = ch;
    ASSERT(isDecimalDigit(ch) || (ch == '.'));
    // 'Numeric literal must start with a decimal digit or a decimal point');

    std::string number;
    number.reserve(32);

    if (ch != '.') {
        number = this->source.bufferedCharAt(this->index++);
        ch = this->source.bufferedCharAt(this->index);

        // Hex number starts with '0x'.
        // Octal number starts with '0'.
        // Octal number in ES6 starts with '0o'.
        // Binary number in ES6 starts with '0b'.
        if (number == "0") {
            if (ch == 'x' || ch == 'X') {
                ++this->index;
                return this->scanHexLiteral(start);
            }
            if (ch == 'b' || ch == 'B') {
                ++this->index;
                return this->scanBinaryLiteral(start);
            }
            if (ch == 'o' || ch == 'O') {
                return this->scanOctalLiteral(ch, start);
            }

            if (ch && isOctalDigit(ch)) {
                if (this->isImplicitOctalLiteral()) {
                    return this->scanOctalLiteral(ch, start);
                }
            }
        }

        while (isDecimalDigit(this->source.bufferedCharAt(this->index))) {
            number += this->source.bufferedCharAt(this->index++);
        }
        ch = this->source.bufferedCharAt(this->index);
    }

    if (ch == '.') {
        number += this->source.bufferedCharAt(this->index++);
        while (isDecimalDigit(this->source.bufferedCharAt(this->index))) {
            number += this->source.bufferedCharAt(this->index++);
        }
        ch = this->source.bufferedCharAt(this->index);
    }

    if (ch == 'e' || ch == 'E') {
        number += this->source.bufferedCharAt(this->index++);

        ch = this->source.bufferedCharAt(this->index);
        if (ch == '+' || ch == '-') {
            number += this->source.bufferedCharAt(this->index++);
        }
        if (isDecimalDigit(this->source.bufferedCharAt(this->index))) {
            while (isDecimalDigit(this->source.bufferedCharAt(this->index))) {
                number += this->source.bufferedCharAt(this->index++);
            }
        } else {
            this->throwUnexpectedToken();
        }
    }

    if (isIdentifierStart(this->source.bufferedCharAt(this->index))) {
        this->throwUnexpectedToken();
    }

    int length = number.length();
    int length_dummy;
    double_conversion::StringToDoubleConverter converter(double_conversion::StringToDoubleConverter::ALLOW_HEX
                                                             | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES
                                                             | double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES,
                                                         0.0, double_conversion::Double::NaN(),
                                                         "Infinity", "NaN");
    double ll = converter.StringToDouble(number.data(), length, &length_dummy);

    auto ret = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::NumericLiteralToken, ll, this->lineNumber, this->lineStart, start, this->index));
    if (startChar == '0' && length >= 2 && ll >= 1) {
        ret->startWithZero = true;
    }
    return ret;
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanStringLiteral()
{
    const size_t start = this->index;
    char16_t quote = this->source.bufferedCharAt(start);
    ASSERT((quote == '\'' || quote == '"'));
    // 'String literal must starts with a quote');

    ++this->index;
    bool octal = false;
    bool isPlainCase = true;

    while (LIKELY(!this->eof())) {
        char16_t ch = this->source.bufferedCharAt(this->index++);

        if (ch == quote) {
            quote = '\0';
            break;
        } else if (UNLIKELY(ch == '\\')) {
            ch = this->source.bufferedCharAt(this->index++);
            isPlainCase = false;
            if (!ch || !isLineTerminator(ch)) {
                switch (ch) {
                case 'u':
                case 'x':
                    if (this->source.bufferedCharAt(this->index) == '{') {
                        ++this->index;
                        this->scanUnicodeCodePointEscape();
                    } else if (this->scanHexEscape(ch) == EMPTY_CODE_POINT) {
                        this->throwUnexpectedToken();
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
                if (ch == '\r' && this->source.bufferedCharAt(this->index) == '\n') {
                    ++this->index;
                } else if (ch == '\n' && this->source.bufferedCharAt(this->index) == '\r') {
                    ++this->index;
                }
                this->lineStart = this->index;
            }
        } else if (UNLIKELY(isLineTerminator(ch))) {
            break;
        } else {
        }
    }

    if (quote != '\0') {
        this->index = start;
        this->throwUnexpectedToken();
    }

    if (isPlainCase) {
        StringView str(this->source, start + 1, this->index - 1);
        auto ret = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::StringLiteralToken, str, this->lineNumber, this->lineStart, start, this->index, true));
        ret->octal = octal;
        ret->plain = true;
        return ret;
    } else {
        // build string if needs
        auto ret = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::StringLiteralToken, StringView(), this->lineNumber, this->lineStart, start, this->index, false));
        ret->octal = octal;
        ret->plain = false;
        return ret;
    }
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanTemplate(bool head)
{
    // TODO apply rope-string
    UTF16StringDataNonGCStd cooked;
    bool terminated = false;
    size_t start = this->index;

    bool tail = false;
    size_t rawOffset = 2;

    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index++);
        if (ch == '`') {
            rawOffset = 1;
            tail = true;
            terminated = true;
            break;
        } else if (ch == '$') {
            if (this->source.bufferedCharAt(this->index) == '{') {
                ++this->index;
                terminated = true;
                break;
            }
            cooked += ch;
        } else if (ch == '\\') {
            ch = this->source.bufferedCharAt(this->index++);
            if (!isLineTerminator(ch)) {
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
                case 'x':
                    if (this->source.bufferedCharAt(this->index) == '{') {
                        ++this->index;
                        cooked += this->scanUnicodeCodePointEscape();
                    } else {
                        const size_t restore = this->index;
                        const char32_t unescaped = this->scanHexEscape(ch);
                        if (unescaped != EMPTY_CODE_POINT) {
                            ParserCharPiece piece(unescaped);
                            cooked += UTF16StringDataNonGCStd(piece.data, piece.length);
                        } else {
                            this->index = restore;
                            cooked += ch;
                        }
                    }
                    break;
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
                        if (isDecimalDigit(this->source.bufferedCharAt(this->index))) {
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
            } else {
                ++this->lineNumber;
                if (ch == '\r' && this->source.bufferedCharAt(this->index) == '\n') {
                    ++this->index;
                }
                this->lineStart = this->index;
            }
        } else if (isLineTerminator(ch)) {
            ++this->lineNumber;
            if (ch == '\r' && this->source.bufferedCharAt(this->index) == '\n') {
                ++this->index;
            }
            this->lineStart = this->index;
            cooked += '\n';
        } else {
            cooked += ch;
        }
    }

    if (!terminated) {
        this->throwUnexpectedToken();
    }

    ScanTemplteResult* result = new ScanTemplteResult();
    result->head = head;
    result->tail = tail;
    result->raw = StringView(this->source, start, this->index - rawOffset);
    result->valueCooked = UTF16StringData(cooked.data(), cooked.length());

    return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::TemplateToken, result, this->lineNumber, this->lineStart, start, this->index));
}

String* Scanner::scanRegExpBody()
{
    char16_t ch = this->source.bufferedCharAt(this->index);
    ASSERT(ch == '/');
    // assert(ch == '/', 'Regular expression literal must start with a slash');

    // TODO apply rope-string
    char16_t ch0 = this->source.bufferedCharAt(this->index++);
    UTF16StringDataNonGCStd str(&ch0, 1);
    bool classMarker = false;
    bool terminated = false;

    while (!this->eof()) {
        ch = this->source.bufferedCharAt(this->index++);
        str += ch;
        if (ch == '\\') {
            ch = this->source.bufferedCharAt(this->index++);
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
        char16_t ch = this->source.bufferedCharAt(this->index);
        if (!isIdentifierPart(ch)) {
            break;
        }

        ++this->index;
        if (ch == '\\' && !this->eof()) {
            ch = this->source.bufferedCharAt(this->index);
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

    if (isAllASCII(flags.data(), flags.length())) {
        return new ASCIIString(flags.data(), flags.length());
    }

    return new UTF16String(flags.data(), flags.length());
}

PassRefPtr<Scanner::ScannerResult> Scanner::scanRegExp()
{
    const size_t start = this->index;

    String* body = this->scanRegExpBody();
    String* flags = this->scanRegExpFlags();
    // const value = this->testRegExp(body.value, flags.value);

    ScanRegExpResult result;
    result.body = body;
    result.flags = flags;
    PassRefPtr<Scanner::ScannerResult> res = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::RegularExpressionToken, this->lineNumber, this->lineStart, start, this->index));
    res->valueRegexp = result;
    return res;
}

// ECMA-262 11.6.2.1 Keywords
static ALWAYS_INLINE KeywordKind isKeyword(const StringBufferAccessData& data)
{
    // 'const' is specialized as Keyword in V8.
    // 'yield' and 'let' are for compatibility with SpiderMonkey and ES.next.
    // Some others are from future reserved words.

    size_t length = data.length;
    char16_t first = data.charAt(0);
    char16_t second;
    switch (first) {
    case 'a':
        // TODO await
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
                const char* env = getenv("ESCARGOT_TREAT_CONST_AS_VAR");
                if (env && strlen(env)) {
                    return VarKeyword;
                }
                return ConstKeyword;
            } else if (second == 'l' && data.equalsSameLength("class", 2)) {
                return ClassKeyword;
            }
        } else if (length == 8) {
            if (data.equalsSameLength("continue", 1)) {
                return ContinueKeyword;
            }
        }
        break;
    case 'd':
        if (length == 8) {
            if (data.equalsSameLength("debugger", 1)) {
                return DebuggerKeyword;
            }
        } else if (length == 2) {
            if (data.equalsSameLength("do", 1)) {
                return DoKeyword;
            }
        } else if (length == 6) {
            if (data.equalsSameLength("delete", 1)) {
                return DeleteKeyword;
            }
        } else if (length == 7) {
            if (data.equalsSameLength("default", 1)) {
                return DefaultKeyword;
            }
        }
        break;
    case 'e':
        if (length == 4) {
            second = data.charAt(1);
            if (second == 'l' && data.equalsSameLength("else", 2)) {
                return ElseKeyword;
            } else if (second == 'n' && data.equalsSameLength("enum", 2)) {
                return EnumKeyword;
            }
        } else if (length == 6) {
            if (data.equalsSameLength("export", 1)) {
                return ExportKeyword;
            }
        } else if (length == 7) {
            if (data.equalsSameLength("extends", 1)) {
                return ExtendsKeyword;
            }
        }
        break;
    case 'f':
        if (length == 3) {
            if (data.equalsSameLength("for", 1)) {
                return ForKeyword;
            }
        } else if (length == 7) {
            if (data.equalsSameLength("finally", 1)) {
                return FinallyKeyword;
            }
        } else if (length == 8) {
            if (data.equalsSameLength("function", 1)) {
                return FunctionKeyword;
            }
        }
        break;
    case 'i':
        if (length == 2) {
            second = data.charAt(1);
            if (second == 'f') {
                return IfKeyword;
            } else if (second == 'n') {
                return InKeyword;
            }
        } else if (length == 6) {
            if (data.equalsSameLength("import", 1)) {
                return ImportKeyword;
            }
        } else if (length == 10) {
            if (data.equalsSameLength("instanceof", 1)) {
                return InstanceofKeyword;
            }
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
        }
        break;
    case 'r':
        if (length == 6 && data.equalsSameLength("return", 1)) {
            return ReturnKeyword;
        }
        break;
    case 's':
        if (length == 5 && data.equalsSameLength("super", 1)) {
            return SuperKeyword;
        } else if (length == 6 && data.equalsSameLength("switch", 1)) {
            return SwitchKeyword;
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
            if (data.equalsSameLength("this", 1)) {
                return ThisKeyword;
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

ALWAYS_INLINE PassRefPtr<Scanner::ScannerResult> Scanner::scanIdentifier(char16_t ch0)
{
    Token type;
    const size_t start = this->index;

    // Backslash (U+005C) starts an escaped character.
    StringView id = UNLIKELY(ch0 == 0x5C) ? this->getComplexIdentifier() : this->getIdentifier();

    // There is no keyword or literal with only one character.
    // Thus, it must be an identifier.
    KeywordKind keywordKind;
    auto data = id.StringView::bufferAccessData();
    if (data.length == 1) {
        type = Token::IdentifierToken;
    } else if ((keywordKind = isKeyword(data))) {
        PassRefPtr<Scanner::ScannerResult> r = adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::KeywordToken, this->lineNumber, this->lineStart, start, this->index));
        r->valueKeywordKind = keywordKind;
        r->hasKeywordButUseString = false;
        return r;
    } else if (data.length == 4) {
        if (data.equalsSameLength("null")) {
            type = Token::NullLiteralToken;
        } else if (data.equalsSameLength("true")) {
            type = Token::BooleanLiteralToken;
        } else {
            type = Token::IdentifierToken;
        }
    } else if ((data.length == 5 && data.equalsSameLength("false"))) {
        type = Token::BooleanLiteralToken;
    } else {
        type = Token::IdentifierToken;
    }

    return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, type, id, this->lineNumber, this->lineStart, start, this->index, id.string() == this->source.string()));
}

PassRefPtr<Scanner::ScannerResult> Scanner::lex()
{
    if (UNLIKELY(this->eof())) {
        return adoptRef(new (createScannerResult()) Scanner::ScannerResult(this, Token::EOFToken, this->lineNumber, this->lineStart, this->index, this->index));
    }

    const char16_t cp = this->source.bufferedCharAt(this->index);

    if (isIdentifierStart(cp)) {
        goto ScanID;
    }

    // Very common: ( and ) and ;
    /*
    if (cp == 0x28 || cp == 0x29 || cp == 0x3B) {
        return this->scanPunctuator(cp0);
    }
    */

    // String literal starts with single quote (U+0027) or double quote (U+0022).
    if (cp == 0x27 || cp == 0x22) {
        return this->scanStringLiteral();
    }

    // Dot (.) U+002E can also start a floating-point number, hence the need
    // to check the next character.
    if (UNLIKELY(cp == 0x2E)) {
        if (isDecimalDigit(this->source.bufferedCharAt(this->index + 1))) {
            return this->scanNumericLiteral();
        }
        return this->scanPunctuator(cp);
    }

    if (isDecimalDigit(cp)) {
        return this->scanNumericLiteral();
    }

    if (UNLIKELY(cp == '`')) {
        ++this->index;
        return this->scanTemplate(true);
    }

    // Possible identifier start in a surrogate pair.
    if (UNLIKELY(cp >= 0xD800 && cp < 0xDFFF)) {
        if (isIdentifierStart(this->codePointAt(this->index))) {
            goto ScanID;
        }
    }
    return this->scanPunctuator(cp);

ScanID:
    return this->scanIdentifier(cp);
}
}
