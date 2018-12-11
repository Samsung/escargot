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
#include "double-conversion.h"
#include "ieee.h"

#include "wtfbridge.h"

using namespace JSC::Yarr;

namespace Escargot {

namespace esprima {

enum Token {
    BooleanLiteralToken = 1,
    EOFToken = 2,
    IdentifierToken = 3,
    KeywordToken = 4,
    NullLiteralToken = 5,
    NumericLiteralToken = 6,
    PunctuatorToken = 7,
    StringLiteralToken = 8,
    RegularExpressionToken = 9,
    TemplateToken = 10
};

/*
enum PlaceHolders {
    ArrowParameterPlaceHolder
};
*/

enum PunctuatorsKind {
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
    PunctuatorsKindEnd,
};

enum KeywordKind {
    NotKeyword,
    If,
    In,
    Do,
    Var,
    For,
    New,
    Try,
    This,
    Else,
    Case,
    Void,
    With,
    Enum,
    Await,
    While,
    Break,
    Catch,
    Throw,
    Const,
    Class,
    Super,
    Return,
    Typeof,
    Delete,
    Switch,
    Export,
    Import,
    Default,
    Finally,
    Extends,
    Function,
    Continue,
    Debugger,
    InstanceofKeyword,
    StrictModeReservedWord,
    Implements,
    Interface,
    Package,
    Private,
    Protected,
    Public,
    Static,
    Yield,
    Let,
    KeywordKindEnd
};

NEVER_INLINE bool isWhiteSpaceSlowCase(char16_t ch)
{
    return (ch == 0xA0) || UNLIKELY(ch >= 0x1680 && (ch == 0x1680 || ch == 0x180E || ch == 0x2000 || ch == 0x2001
                                                     || ch == 0x2002 || ch == 0x2003 || ch == 0x2004 || ch == 0x2005 || ch == 0x2006
                                                     || ch == 0x2007 || ch == 0x2008 || ch == 0x2009 || ch == 0x200A || ch == 0x202F
                                                     || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF));
}

ALWAYS_INLINE bool isDecimalDigit(char16_t ch)
{
    return (ch >= '0' && ch <= '9'); // 0..9
}

ALWAYS_INLINE bool isHexDigit(char16_t ch)
{
    return isDecimalDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

ALWAYS_INLINE bool isOctalDigit(char16_t ch)
{
    return (ch >= '0' && ch < '8'); // 0..7
}

ALWAYS_INLINE char16_t octalValue(char16_t ch)
{
    ASSERT(isOctalDigit(ch));
    return ch - '0';
}

ALWAYS_INLINE uint8_t toHexNumericValue(char16_t ch)
{
    return ch < 'A' ? ch - '0' : (ch - 'A' + 10) & 0xF;
}

struct ParserCharPiece {
    char16_t data[3];
    size_t length;

    ParserCharPiece(const char16_t a)
    {
        data[0] = a;
        data[1] = 0;
        length = 1;
    }

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

    ParserCharPiece(const char16_t a, const char16_t b)
    {
        data[0] = a;
        data[1] = b;
        data[2] = 0;
        length = 2;
    }
};

// ECMA-262 11.6 Identifier Names and Identifiers
ALWAYS_INLINE ParserCharPiece fromCodePoint(char32_t cp)
{
    if (cp < 0x10000) {
        return ParserCharPiece((char16_t)cp);
    } else {
        return ParserCharPiece((char16_t)(0xD800 + ((cp - 0x10000) >> 10)), (char16_t)(0xDC00 + ((cp - 0x10000) & 1023)));
    }
}

NEVER_INLINE bool isIdentifierPartSlow(char32_t ch)
{
    return (ch == 0xAA) || (ch == 0xB5) || (ch == 0xB7) || (ch == 0xBA) || (0xC0 <= ch && ch <= 0xD6) || (0xD8 <= ch && ch <= 0xF6) || (0xF8 <= ch && ch <= 0x02C1) || (0x02C6 <= ch && ch <= 0x02D1) || (0x02E0 <= ch && ch <= 0x02E4) || (ch == 0x02EC) || (ch == 0x02EE) || (0x0300 <= ch && ch <= 0x0374) || (ch == 0x0376) || (ch == 0x0377) || (0x037A <= ch && ch <= 0x037D) || (ch == 0x037F) || (0x0386 <= ch && ch <= 0x038A) || (ch == 0x038C) || (0x038E <= ch && ch <= 0x03A1) || (0x03A3 <= ch && ch <= 0x03F5) || (0x03F7 <= ch && ch <= 0x0481) || (0x0483 <= ch && ch <= 0x0487) || (0x048A <= ch && ch <= 0x052F) || (0x0531 <= ch && ch <= 0x0556) || (ch == 0x0559) || (0x0561 <= ch && ch <= 0x0587) || (0x0591 <= ch && ch <= 0x05BD) || (ch == 0x05BF) || (ch == 0x05C1) || (ch == 0x05C2) || (ch == 0x05C4) || (ch == 0x05C5) || (ch == 0x05C7) || (0x05D0 <= ch && ch <= 0x05EA) || (0x05F0 <= ch && ch <= 0x05F2) || (0x0610 <= ch && ch <= 0x061A) || (0x0620 <= ch && ch <= 0x0669) || (0x066E <= ch && ch <= 0x06D3) || (0x06D5 <= ch && ch <= 0x06DC) || (0x06DF <= ch && ch <= 0x06E8) || (0x06EA <= ch && ch <= 0x06FC) || (ch == 0x06FF) || (0x0710 <= ch && ch <= 0x074A) || (0x074D <= ch && ch <= 0x07B1) || (0x07C0 <= ch && ch <= 0x07F5) || (ch == 0x07FA) || (0x0800 <= ch && ch <= 0x082D) || (0x0840 <= ch && ch <= 0x085B) || (0x08A0 <= ch && ch <= 0x08B2) || (0x08E4 <= ch && ch <= 0x0963) || (0x0966 <= ch && ch <= 0x096F) || (0x0971 <= ch && ch <= 0x0983) || (0x0985 <= ch && ch <= 0x098C) || (ch == 0x098F) || (ch == 0x0990) || (0x0993 <= ch && ch <= 0x09A8) || (0x09AA <= ch && ch <= 0x09B0) || (ch == 0x09B2) || (0x09B6 <= ch && ch <= 0x09B9) || (0x09BC <= ch && ch <= 0x09C4) || (ch == 0x09C7) || (ch == 0x09C8) || (0x09CB <= ch && ch <= 0x09CE) || (ch == 0x09D7) || (ch == 0x09DC) || (ch == 0x09DD) || (0x09DF <= ch && ch <= 0x09E3) || (0x09E6 <= ch && ch <= 0x09F1) || (0x0A01 <= ch && ch <= 0x0A03) || (0x0A05 <= ch && ch <= 0x0A0A) || (ch == 0x0A0F) || (ch == 0x0A10) || (0x0A13 <= ch && ch <= 0x0A28) || (0x0A2A <= ch && ch <= 0x0A30) || (ch == 0x0A32) || (ch == 0x0A33) || (ch == 0x0A35) || (ch == 0x0A36) || (ch == 0x0A38) || (ch == 0x0A39) || (ch == 0x0A3C) || (0x0A3E <= ch && ch <= 0x0A42) || (ch == 0x0A47) || (ch == 0x0A48) || (0x0A4B <= ch && ch <= 0x0A4D) || (ch == 0x0A51) || (0x0A59 <= ch && ch <= 0x0A5C) || (ch == 0x0A5E) || (0x0A66 <= ch && ch <= 0x0A75) || (0x0A81 <= ch && ch <= 0x0A83) || (0x0A85 <= ch && ch <= 0x0A8D) || (0x0A8F <= ch && ch <= 0x0A91) || (0x0A93 <= ch && ch <= 0x0AA8) || (0x0AAA <= ch && ch <= 0x0AB0) || (ch == 0x0AB2) || (ch == 0x0AB3) || (0x0AB5 <= ch && ch <= 0x0AB9) || (0x0ABC <= ch && ch <= 0x0AC5) || (0x0AC7 <= ch && ch <= 0x0AC9) || (0x0ACB <= ch && ch <= 0x0ACD) || (ch == 0x0AD0) || (0x0AE0 <= ch && ch <= 0x0AE3) || (0x0AE6 <= ch && ch <= 0x0AEF) || (0x0B01 <= ch && ch <= 0x0B03) || (0x0B05 <= ch && ch <= 0x0B0C) || (ch == 0x0B0F) || (ch == 0x0B10) || (0x0B13 <= ch && ch <= 0x0B28) || (0x0B2A <= ch && ch <= 0x0B30) || (ch == 0x0B32) || (ch == 0x0B33) || (0x0B35 <= ch && ch <= 0x0B39) || (0x0B3C <= ch && ch <= 0x0B44) || (ch == 0x0B47) || (ch == 0x0B48) || (0x0B4B <= ch && ch <= 0x0B4D) || (ch == 0x0B56) || (ch == 0x0B57) || (ch == 0x0B5C) || (ch == 0x0B5D) || (0x0B5F <= ch && ch <= 0x0B63) || (0x0B66 <= ch && ch <= 0x0B6F) || (ch == 0x0B71) || (ch == 0x0B82) || (ch == 0x0B83) || (0x0B85 <= ch && ch <= 0x0B8A) || (0x0B8E <= ch && ch <= 0x0B90) || (0x0B92 <= ch && ch <= 0x0B95) || (ch == 0x0B99) || (ch == 0x0B9A) || (ch == 0x0B9C) || (ch == 0x0B9E) || (ch == 0x0B9F) || (ch == 0x0BA3) || (ch == 0x0BA4) || (0x0BA8 <= ch && ch <= 0x0BAA) || (0x0BAE <= ch && ch <= 0x0BB9) || (0x0BBE <= ch && ch <= 0x0BC2) || (0x0BC6 <= ch && ch <= 0x0BC8) || (0x0BCA <= ch && ch <= 0x0BCD) || (ch == 0x0BD0) || (ch == 0x0BD7) || (0x0BE6 <= ch && ch <= 0x0BEF) || (0x0C00 <= ch && ch <= 0x0C03) || (0x0C05 <= ch && ch <= 0x0C0C) || (0x0C0E <= ch && ch <= 0x0C10) || (0x0C12 <= ch && ch <= 0x0C28) || (0x0C2A <= ch && ch <= 0x0C39) || (0x0C3D <= ch && ch <= 0x0C44) || (0x0C46 <= ch && ch <= 0x0C48) || (0x0C4A <= ch && ch <= 0x0C4D) || (ch == 0x0C55) || (ch == 0x0C56) || (ch == 0x0C58) || (ch == 0x0C59) || (0x0C60 <= ch && ch <= 0x0C63) || (0x0C66 <= ch && ch <= 0x0C6F) || (0x0C81 <= ch && ch <= 0x0C83) || (0x0C85 <= ch && ch <= 0x0C8C) || (0x0C8E <= ch && ch <= 0x0C90) || (0x0C92 <= ch && ch <= 0x0CA8) || (0x0CAA <= ch && ch <= 0x0CB3) || (0x0CB5 <= ch && ch <= 0x0CB9) || (0x0CBC <= ch && ch <= 0x0CC4) || (0x0CC6 <= ch && ch <= 0x0CC8) || (0x0CCA <= ch && ch <= 0x0CCD) || (ch == 0x0CD5) || (ch == 0x0CD6) || (ch == 0x0CDE) || (0x0CE0 <= ch && ch <= 0x0CE3) || (0x0CE6 <= ch && ch <= 0x0CEF) || (ch == 0x0CF1) || (ch == 0x0CF2) || (0x0D01 <= ch && ch <= 0x0D03) || (0x0D05 <= ch && ch <= 0x0D0C) || (0x0D0E <= ch && ch <= 0x0D10) || (0x0D12 <= ch && ch <= 0x0D3A) || (0x0D3D <= ch && ch <= 0x0D44) || (0x0D46 <= ch && ch <= 0x0D48) || (0x0D4A <= ch && ch <= 0x0D4E) || (ch == 0x0D57) || (0x0D60 <= ch && ch <= 0x0D63) || (0x0D66 <= ch && ch <= 0x0D6F) || (0x0D7A <= ch && ch <= 0x0D7F) || (ch == 0x0D82) || (ch == 0x0D83) || (0x0D85 <= ch && ch <= 0x0D96) || (0x0D9A <= ch && ch <= 0x0DB1) || (0x0DB3 <= ch && ch <= 0x0DBB) || (ch == 0x0DBD) || (0x0DC0 <= ch && ch <= 0x0DC6) || (ch == 0x0DCA) || (0x0DCF <= ch && ch <= 0x0DD4) || (ch == 0x0DD6) || (0x0DD8 <= ch && ch <= 0x0DDF) || (0x0DE6 <= ch && ch <= 0x0DEF) || (ch == 0x0DF2) || (ch == 0x0DF3) || (0x0E01 <= ch && ch <= 0x0E3A) || (0x0E40 <= ch && ch <= 0x0E4E) || (0x0E50 <= ch && ch <= 0x0E59) || (ch == 0x0E81) || (ch == 0x0E82) || (ch == 0x0E84) || (ch == 0x0E87) || (ch == 0x0E88) || (ch == 0x0E8A) || (ch == 0x0E8D) || (0x0E94 <= ch && ch <= 0x0E97) || (0x0E99 <= ch && ch <= 0x0E9F) || (0x0EA1 <= ch && ch <= 0x0EA3) || (ch == 0x0EA5) || (ch == 0x0EA7) || (ch == 0x0EAA) || (ch == 0x0EAB) || (0x0EAD <= ch && ch <= 0x0EB9) || (0x0EBB <= ch && ch <= 0x0EBD) || (0x0EC0 <= ch && ch <= 0x0EC4) || (ch == 0x0EC6) || (0x0EC8 <= ch && ch <= 0x0ECD) || (0x0ED0 <= ch && ch <= 0x0ED9) || (0x0EDC <= ch && ch <= 0x0EDF) || (ch == 0x0F00) || (ch == 0x0F18) || (ch == 0x0F19) || (0x0F20 <= ch && ch <= 0x0F29) || (ch == 0x0F35) || (ch == 0x0F37) || (ch == 0x0F39) || (0x0F3E <= ch && ch <= 0x0F47) || (0x0F49 <= ch && ch <= 0x0F6C) || (0x0F71 <= ch && ch <= 0x0F84) || (0x0F86 <= ch && ch <= 0x0F97) || (0x0F99 <= ch && ch <= 0x0FBC) || (ch == 0x0FC6) || (0x1000 <= ch && ch <= 0x1049) || (0x1050 <= ch && ch <= 0x109D) || (0x10A0 <= ch && ch <= 0x10C5) || (ch == 0x10C7) || (ch == 0x10CD) || (0x10D0 <= ch && ch <= 0x10FA) || (0x10FC <= ch && ch <= 0x1248) || (0x124A <= ch && ch <= 0x124D) || (0x1250 <= ch && ch <= 0x1256) || (ch == 0x1258) || (0x125A <= ch && ch <= 0x125D) || (0x1260 <= ch && ch <= 0x1288) || (0x128A <= ch && ch <= 0x128D) || (0x1290 <= ch && ch <= 0x12B0) || (0x12B2 <= ch && ch <= 0x12B5) || (0x12B8 <= ch && ch <= 0x12BE) || (ch == 0x12C0) || (0x12C2 <= ch && ch <= 0x12C5) || (0x12C8 <= ch && ch <= 0x12D6) || (0x12D8 <= ch && ch <= 0x1310) || (0x1312 <= ch && ch <= 0x1315) || (0x1318 <= ch && ch <= 0x135A) || (0x135D <= ch && ch <= 0x135F) || (0x1369 <= ch && ch <= 0x1371) || (0x1380 <= ch && ch <= 0x138F) || (0x13A0 <= ch && ch <= 0x13F4) || (0x1401 <= ch && ch <= 0x166C) || (0x166F <= ch && ch <= 0x167F) || (0x1681 <= ch && ch <= 0x169A) || (0x16A0 <= ch && ch <= 0x16EA) || (0x16EE <= ch && ch <= 0x16F8) || (0x1700 <= ch && ch <= 0x170C) || (0x170E <= ch && ch <= 0x1714) || (0x1720 <= ch && ch <= 0x1734) || (0x1740 <= ch && ch <= 0x1753) || (0x1760 <= ch && ch <= 0x176C) || (0x176E <= ch && ch <= 0x1770) || (ch == 0x1772) || (ch == 0x1773) || (0x1780 <= ch && ch <= 0x17D3) || (ch == 0x17D7) || (ch == 0x17DC) || (ch == 0x17DD) || (0x17E0 <= ch && ch <= 0x17E9) || (0x180B <= ch && ch <= 0x180D) || (0x1810 <= ch && ch <= 0x1819) || (0x1820 <= ch && ch <= 0x1877) || (0x1880 <= ch && ch <= 0x18AA) || (0x18B0 <= ch && ch <= 0x18F5) || (0x1900 <= ch && ch <= 0x191E) || (0x1920 <= ch && ch <= 0x192B) || (0x1930 <= ch && ch <= 0x193B) || (0x1946 <= ch && ch <= 0x196D) || (0x1970 <= ch && ch <= 0x1974) || (0x1980 <= ch && ch <= 0x19AB) || (0x19B0 <= ch && ch <= 0x19C9) || (0x19D0 <= ch && ch <= 0x19DA) || (0x1A00 <= ch && ch <= 0x1A1B) || (0x1A20 <= ch && ch <= 0x1A5E) || (0x1A60 <= ch && ch <= 0x1A7C) || (0x1A7F <= ch && ch <= 0x1A89) || (0x1A90 <= ch && ch <= 0x1A99) || (ch == 0x1AA7) || (0x1AB0 <= ch && ch <= 0x1ABD) || (0x1B00 <= ch && ch <= 0x1B4B) || (0x1B50 <= ch && ch <= 0x1B59) || (0x1B6B <= ch && ch <= 0x1B73) || (0x1B80 <= ch && ch <= 0x1BF3) || (0x1C00 <= ch && ch <= 0x1C37) || (0x1C40 <= ch && ch <= 0x1C49) || (0x1C4D <= ch && ch <= 0x1C7D) || (0x1CD0 <= ch && ch <= 0x1CD2) || (0x1CD4 <= ch && ch <= 0x1CF6) || (ch == 0x1CF8) || (ch == 0x1CF9) || (0x1D00 <= ch && ch <= 0x1DF5) || (0x1DFC <= ch && ch <= 0x1F15) || (0x1F18 <= ch && ch <= 0x1F1D) || (0x1F20 <= ch && ch <= 0x1F45) || (0x1F48 <= ch && ch <= 0x1F4D) || (0x1F50 <= ch && ch <= 0x1F57) || (ch == 0x1F59) || (ch == 0x1F5B) || (ch == 0x1F5D) || (0x1F5F <= ch && ch <= 0x1F7D) || (0x1F80 <= ch && ch <= 0x1FB4) || (0x1FB6 <= ch && ch <= 0x1FBC) || (ch == 0x1FBE) || (0x1FC2 <= ch && ch <= 0x1FC4) || (0x1FC6 <= ch && ch <= 0x1FCC) || (0x1FD0 <= ch && ch <= 0x1FD3) || (0x1FD6 <= ch && ch <= 0x1FDB) || (0x1FE0 <= ch && ch <= 0x1FEC) || (0x1FF2 <= ch && ch <= 0x1FF4) || (0x1FF6 <= ch && ch <= 0x1FFC) || (ch == 0x200C) || (ch == 0x200D) || (ch == 0x203F) || (ch == 0x2040) || (ch == 0x2054) || (ch == 0x2071) || (ch == 0x207F) || (0x2090 <= ch && ch <= 0x209C) || (0x20D0 <= ch && ch <= 0x20DC) || (ch == 0x20E1) || (0x20E5 <= ch && ch <= 0x20F0) || (ch == 0x2102) || (ch == 0x2107) || (0x210A <= ch && ch <= 0x2113) || (ch == 0x2115) || (0x2118 <= ch && ch <= 0x211D) || (ch == 0x2124) || (ch == 0x2126) || (ch == 0x2128) || (0x212A <= ch && ch <= 0x2139) || (0x213C <= ch && ch <= 0x213F) || (0x2145 <= ch && ch <= 0x2149) || (ch == 0x214E) || (0x2160 <= ch && ch <= 0x2188) || (0x2C00 <= ch && ch <= 0x2C2E) || (0x2C30 <= ch && ch <= 0x2C5E) || (0x2C60 <= ch && ch <= 0x2CE4) || (0x2CEB <= ch && ch <= 0x2CF3) || (0x2D00 <= ch && ch <= 0x2D25) || (ch == 0x2D27) || (ch == 0x2D2D) || (0x2D30 <= ch && ch <= 0x2D67) || (ch == 0x2D6F) || (0x2D7F <= ch && ch <= 0x2D96) || (0x2DA0 <= ch && ch <= 0x2DA6) || (0x2DA8 <= ch && ch <= 0x2DAE) || (0x2DB0 <= ch && ch <= 0x2DB6) || (0x2DB8 <= ch && ch <= 0x2DBE) || (0x2DC0 <= ch && ch <= 0x2DC6) || (0x2DC8 <= ch && ch <= 0x2DCE) || (0x2DD0 <= ch && ch <= 0x2DD6) || (0x2DD8 <= ch && ch <= 0x2DDE) || (0x2DE0 <= ch && ch <= 0x2DFF) || (0x3005 <= ch && ch <= 0x3007) || (0x3021 <= ch && ch <= 0x302F) || (0x3031 <= ch && ch <= 0x3035) || (0x3038 <= ch && ch <= 0x303C) || (0x3041 <= ch && ch <= 0x3096) || (0x3099 <= ch && ch <= 0x309F) || (0x30A1 <= ch && ch <= 0x30FA) || (0x30FC <= ch && ch <= 0x30FF) || (0x3105 <= ch && ch <= 0x312D) || (0x3131 <= ch && ch <= 0x318E) || (0x31A0 <= ch && ch <= 0x31BA) || (0x31F0 <= ch && ch <= 0x31FF) || (0x3400 <= ch && ch <= 0x4DB5) || (0x4E00 <= ch && ch <= 0x9FCC) || (0xA000 <= ch && ch <= 0xA48C) || (0xA4D0 <= ch && ch <= 0xA4FD) || (0xA500 <= ch && ch <= 0xA60C) || (0xA610 <= ch && ch <= 0xA62B) || (0xA640 <= ch && ch <= 0xA66F) || (0xA674 <= ch && ch <= 0xA67D) || (0xA67F <= ch && ch <= 0xA69D) || (0xA69F <= ch && ch <= 0xA6F1) || (0xA717 <= ch && ch <= 0xA71F) || (0xA722 <= ch && ch <= 0xA788) || (0xA78B <= ch && ch <= 0xA78E) || (0xA790 <= ch && ch <= 0xA7AD) || (ch == 0xA7B0) || (ch == 0xA7B1) || (0xA7F7 <= ch && ch <= 0xA827) || (0xA840 <= ch && ch <= 0xA873) || (0xA880 <= ch && ch <= 0xA8C4) || (0xA8D0 <= ch && ch <= 0xA8D9) || (0xA8E0 <= ch && ch <= 0xA8F7) || (ch == 0xA8FB) || (0xA900 <= ch && ch <= 0xA92D) || (0xA930 <= ch && ch <= 0xA953) || (0xA960 <= ch && ch <= 0xA97C) || (0xA980 <= ch && ch <= 0xA9C0) || (0xA9CF <= ch && ch <= 0xA9D9) || (0xA9E0 <= ch && ch <= 0xA9FE) || (0xAA00 <= ch && ch <= 0xAA36) || (0xAA40 <= ch && ch <= 0xAA4D) || (0xAA50 <= ch && ch <= 0xAA59) || (0xAA60 <= ch && ch <= 0xAA76) || (0xAA7A <= ch && ch <= 0xAAC2) || (0xAADB <= ch && ch <= 0xAADD) || (0xAAE0 <= ch && ch <= 0xAAEF) || (0xAAF2 <= ch && ch <= 0xAAF6) || (0xAB01 <= ch && ch <= 0xAB06) || (0xAB09 <= ch && ch <= 0xAB0E) || (0xAB11 <= ch && ch <= 0xAB16) || (0xAB20 <= ch && ch <= 0xAB26) || (0xAB28 <= ch && ch <= 0xAB2E) || (0xAB30 <= ch && ch <= 0xAB5A) || (0xAB5C <= ch && ch <= 0xAB5F) || (ch == 0xAB64) || (ch == 0xAB65) || (0xABC0 <= ch && ch <= 0xABEA) || (ch == 0xABEC) || (ch == 0xABED) || (0xABF0 <= ch && ch <= 0xABF9) || (0xAC00 <= ch && ch <= 0xD7A3) || (0xD7B0 <= ch && ch <= 0xD7C6) || (0xD7CB <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFA6D) || (0xFA70 <= ch && ch <= 0xFAD9) || (0xFB00 <= ch && ch <= 0xFB06) || (0xFB13 <= ch && ch <= 0xFB17) || (0xFB1D <= ch && ch <= 0xFB28) || (0xFB2A <= ch && ch <= 0xFB36) || (0xFB38 <= ch && ch <= 0xFB3C) || (ch == 0xFB3E) || (ch == 0xFB40) || (ch == 0xFB41) || (ch == 0xFB43) || (ch == 0xFB44) || (0xFB46 <= ch && ch <= 0xFBB1) || (0xFBD3 <= ch && ch <= 0xFD3D) || (0xFD50 <= ch && ch <= 0xFD8F) || (0xFD92 <= ch && ch <= 0xFDC7) || (0xFDF0 <= ch && ch <= 0xFDFB) || (0xFE00 <= ch && ch <= 0xFE0F) || (0xFE20 <= ch && ch <= 0xFE2D) || (ch == 0xFE33) || (ch == 0xFE34) || (0xFE4D <= ch && ch <= 0xFE4F) || (0xFE70 <= ch && ch <= 0xFE74) || (0xFE76 <= ch && ch <= 0xFEFC) || (0xFF10 <= ch && ch <= 0xFF19) || (0xFF21 <= ch && ch <= 0xFF3A) || (ch == 0xFF3F) || (0xFF41 <= ch && ch <= 0xFF5A) || (0xFF66 <= ch && ch <= 0xFFBE) || (0xFFC2 <= ch && ch <= 0xFFC7) || (0xFFCA <= ch && ch <= 0xFFCF) || (0xFFD2 <= ch && ch <= 0xFFD7) || (0xFFDA <= ch && ch <= 0xFFDC);
}

ALWAYS_INLINE bool isIdentifierPart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & ESPRIMA_IS_IDENT;
    } else {
        return isIdentifierPartSlow(ch);
    }
}

NEVER_INLINE bool isIdentifierStartSlow(char32_t ch)
{
    return (ch == 0xAA) || (ch == 0xB5) || (ch == 0xB7) || (ch == 0xBA) || (0xC0 <= ch && ch <= 0xD6) || (0xD8 <= ch && ch <= 0xF6) || (0xF8 <= ch && ch <= 0x02C1) || (0x02C6 <= ch && ch <= 0x02D1) || (0x02E0 <= ch && ch <= 0x02E4) || (ch == 0x02EC) || (ch == 0x02EE) || (0x0300 <= ch && ch <= 0x0374) || (ch == 0x0376) || (ch == 0x0377) || (0x037A <= ch && ch <= 0x037D) || (ch == 0x037F) || (0x0386 <= ch && ch <= 0x038A) || (ch == 0x038C) || (0x038E <= ch && ch <= 0x03A1) || (0x03A3 <= ch && ch <= 0x03F5) || (0x03F7 <= ch && ch <= 0x0481) || (0x0483 <= ch && ch <= 0x0487) || (0x048A <= ch && ch <= 0x052F) || (0x0531 <= ch && ch <= 0x0556) || (ch == 0x0559) || (0x0561 <= ch && ch <= 0x0587) || (0x0591 <= ch && ch <= 0x05BD) || (ch == 0x05BF) || (ch == 0x05C1) || (ch == 0x05C2) || (ch == 0x05C4) || (ch == 0x05C5) || (ch == 0x05C7) || (0x05D0 <= ch && ch <= 0x05EA) || (0x05F0 <= ch && ch <= 0x05F2) || (0x0610 <= ch && ch <= 0x061A) || (0x0620 <= ch && ch <= 0x0669) || (0x066E <= ch && ch <= 0x06D3) || (0x06D5 <= ch && ch <= 0x06DC) || (0x06DF <= ch && ch <= 0x06E8) || (0x06EA <= ch && ch <= 0x06FC) || (ch == 0x06FF) || (0x0710 <= ch && ch <= 0x074A) || (0x074D <= ch && ch <= 0x07B1) || (0x07C0 <= ch && ch <= 0x07F5) || (ch == 0x07FA) || (0x0800 <= ch && ch <= 0x082D) || (0x0840 <= ch && ch <= 0x085B) || (0x08A0 <= ch && ch <= 0x08B2) || (0x08E4 <= ch && ch <= 0x0963) || (0x0966 <= ch && ch <= 0x096F) || (0x0971 <= ch && ch <= 0x0983) || (0x0985 <= ch && ch <= 0x098C) || (ch == 0x098F) || (ch == 0x0990) || (0x0993 <= ch && ch <= 0x09A8) || (0x09AA <= ch && ch <= 0x09B0) || (ch == 0x09B2) || (0x09B6 <= ch && ch <= 0x09B9) || (0x09BC <= ch && ch <= 0x09C4) || (ch == 0x09C7) || (ch == 0x09C8) || (0x09CB <= ch && ch <= 0x09CE) || (ch == 0x09D7) || (ch == 0x09DC) || (ch == 0x09DD) || (0x09DF <= ch && ch <= 0x09E3) || (0x09E6 <= ch && ch <= 0x09F1) || (0x0A01 <= ch && ch <= 0x0A03) || (0x0A05 <= ch && ch <= 0x0A0A) || (ch == 0x0A0F) || (ch == 0x0A10) || (0x0A13 <= ch && ch <= 0x0A28) || (0x0A2A <= ch && ch <= 0x0A30) || (ch == 0x0A32) || (ch == 0x0A33) || (ch == 0x0A35) || (ch == 0x0A36) || (ch == 0x0A38) || (ch == 0x0A39) || (ch == 0x0A3C) || (0x0A3E <= ch && ch <= 0x0A42) || (ch == 0x0A47) || (ch == 0x0A48) || (0x0A4B <= ch && ch <= 0x0A4D) || (ch == 0x0A51) || (0x0A59 <= ch && ch <= 0x0A5C) || (ch == 0x0A5E) || (0x0A66 <= ch && ch <= 0x0A75) || (0x0A81 <= ch && ch <= 0x0A83) || (0x0A85 <= ch && ch <= 0x0A8D) || (0x0A8F <= ch && ch <= 0x0A91) || (0x0A93 <= ch && ch <= 0x0AA8) || (0x0AAA <= ch && ch <= 0x0AB0) || (ch == 0x0AB2) || (ch == 0x0AB3) || (0x0AB5 <= ch && ch <= 0x0AB9) || (0x0ABC <= ch && ch <= 0x0AC5) || (0x0AC7 <= ch && ch <= 0x0AC9) || (0x0ACB <= ch && ch <= 0x0ACD) || (ch == 0x0AD0) || (0x0AE0 <= ch && ch <= 0x0AE3) || (0x0AE6 <= ch && ch <= 0x0AEF) || (0x0B01 <= ch && ch <= 0x0B03) || (0x0B05 <= ch && ch <= 0x0B0C) || (ch == 0x0B0F) || (ch == 0x0B10) || (0x0B13 <= ch && ch <= 0x0B28) || (0x0B2A <= ch && ch <= 0x0B30) || (ch == 0x0B32) || (ch == 0x0B33) || (0x0B35 <= ch && ch <= 0x0B39) || (0x0B3C <= ch && ch <= 0x0B44) || (ch == 0x0B47) || (ch == 0x0B48) || (0x0B4B <= ch && ch <= 0x0B4D) || (ch == 0x0B56) || (ch == 0x0B57) || (ch == 0x0B5C) || (ch == 0x0B5D) || (0x0B5F <= ch && ch <= 0x0B63) || (0x0B66 <= ch && ch <= 0x0B6F) || (ch == 0x0B71) || (ch == 0x0B82) || (ch == 0x0B83) || (0x0B85 <= ch && ch <= 0x0B8A) || (0x0B8E <= ch && ch <= 0x0B90) || (0x0B92 <= ch && ch <= 0x0B95) || (ch == 0x0B99) || (ch == 0x0B9A) || (ch == 0x0B9C) || (ch == 0x0B9E) || (ch == 0x0B9F) || (ch == 0x0BA3) || (ch == 0x0BA4) || (0x0BA8 <= ch && ch <= 0x0BAA) || (0x0BAE <= ch && ch <= 0x0BB9) || (0x0BBE <= ch && ch <= 0x0BC2) || (0x0BC6 <= ch && ch <= 0x0BC8) || (0x0BCA <= ch && ch <= 0x0BCD) || (ch == 0x0BD0) || (ch == 0x0BD7) || (0x0BE6 <= ch && ch <= 0x0BEF) || (0x0C00 <= ch && ch <= 0x0C03) || (0x0C05 <= ch && ch <= 0x0C0C) || (0x0C0E <= ch && ch <= 0x0C10) || (0x0C12 <= ch && ch <= 0x0C28) || (0x0C2A <= ch && ch <= 0x0C39) || (0x0C3D <= ch && ch <= 0x0C44) || (0x0C46 <= ch && ch <= 0x0C48) || (0x0C4A <= ch && ch <= 0x0C4D) || (ch == 0x0C55) || (ch == 0x0C56) || (ch == 0x0C58) || (ch == 0x0C59) || (0x0C60 <= ch && ch <= 0x0C63) || (0x0C66 <= ch && ch <= 0x0C6F) || (0x0C81 <= ch && ch <= 0x0C83) || (0x0C85 <= ch && ch <= 0x0C8C) || (0x0C8E <= ch && ch <= 0x0C90) || (0x0C92 <= ch && ch <= 0x0CA8) || (0x0CAA <= ch && ch <= 0x0CB3) || (0x0CB5 <= ch && ch <= 0x0CB9) || (0x0CBC <= ch && ch <= 0x0CC4) || (0x0CC6 <= ch && ch <= 0x0CC8) || (0x0CCA <= ch && ch <= 0x0CCD) || (ch == 0x0CD5) || (ch == 0x0CD6) || (ch == 0x0CDE) || (0x0CE0 <= ch && ch <= 0x0CE3) || (0x0CE6 <= ch && ch <= 0x0CEF) || (ch == 0x0CF1) || (ch == 0x0CF2) || (0x0D01 <= ch && ch <= 0x0D03) || (0x0D05 <= ch && ch <= 0x0D0C) || (0x0D0E <= ch && ch <= 0x0D10) || (0x0D12 <= ch && ch <= 0x0D3A) || (0x0D3D <= ch && ch <= 0x0D44) || (0x0D46 <= ch && ch <= 0x0D48) || (0x0D4A <= ch && ch <= 0x0D4E) || (ch == 0x0D57) || (0x0D60 <= ch && ch <= 0x0D63) || (0x0D66 <= ch && ch <= 0x0D6F) || (0x0D7A <= ch && ch <= 0x0D7F) || (ch == 0x0D82) || (ch == 0x0D83) || (0x0D85 <= ch && ch <= 0x0D96) || (0x0D9A <= ch && ch <= 0x0DB1) || (0x0DB3 <= ch && ch <= 0x0DBB) || (ch == 0x0DBD) || (0x0DC0 <= ch && ch <= 0x0DC6) || (ch == 0x0DCA) || (0x0DCF <= ch && ch <= 0x0DD4) || (ch == 0x0DD6) || (0x0DD8 <= ch && ch <= 0x0DDF) || (0x0DE6 <= ch && ch <= 0x0DEF) || (ch == 0x0DF2) || (ch == 0x0DF3) || (0x0E01 <= ch && ch <= 0x0E3A) || (0x0E40 <= ch && ch <= 0x0E4E) || (0x0E50 <= ch && ch <= 0x0E59) || (ch == 0x0E81) || (ch == 0x0E82) || (ch == 0x0E84) || (ch == 0x0E87) || (ch == 0x0E88) || (ch == 0x0E8A) || (ch == 0x0E8D) || (0x0E94 <= ch && ch <= 0x0E97) || (0x0E99 <= ch && ch <= 0x0E9F) || (0x0EA1 <= ch && ch <= 0x0EA3) || (ch == 0x0EA5) || (ch == 0x0EA7) || (ch == 0x0EAA) || (ch == 0x0EAB) || (0x0EAD <= ch && ch <= 0x0EB9) || (0x0EBB <= ch && ch <= 0x0EBD) || (0x0EC0 <= ch && ch <= 0x0EC4) || (ch == 0x0EC6) || (0x0EC8 <= ch && ch <= 0x0ECD) || (0x0ED0 <= ch && ch <= 0x0ED9) || (0x0EDC <= ch && ch <= 0x0EDF) || (ch == 0x0F00) || (ch == 0x0F18) || (ch == 0x0F19) || (0x0F20 <= ch && ch <= 0x0F29) || (ch == 0x0F35) || (ch == 0x0F37) || (ch == 0x0F39) || (0x0F3E <= ch && ch <= 0x0F47) || (0x0F49 <= ch && ch <= 0x0F6C) || (0x0F71 <= ch && ch <= 0x0F84) || (0x0F86 <= ch && ch <= 0x0F97) || (0x0F99 <= ch && ch <= 0x0FBC) || (ch == 0x0FC6) || (0x1000 <= ch && ch <= 0x1049) || (0x1050 <= ch && ch <= 0x109D) || (0x10A0 <= ch && ch <= 0x10C5) || (ch == 0x10C7) || (ch == 0x10CD) || (0x10D0 <= ch && ch <= 0x10FA) || (0x10FC <= ch && ch <= 0x1248) || (0x124A <= ch && ch <= 0x124D) || (0x1250 <= ch && ch <= 0x1256) || (ch == 0x1258) || (0x125A <= ch && ch <= 0x125D) || (0x1260 <= ch && ch <= 0x1288) || (0x128A <= ch && ch <= 0x128D) || (0x1290 <= ch && ch <= 0x12B0) || (0x12B2 <= ch && ch <= 0x12B5) || (0x12B8 <= ch && ch <= 0x12BE) || (ch == 0x12C0) || (0x12C2 <= ch && ch <= 0x12C5) || (0x12C8 <= ch && ch <= 0x12D6) || (0x12D8 <= ch && ch <= 0x1310) || (0x1312 <= ch && ch <= 0x1315) || (0x1318 <= ch && ch <= 0x135A) || (0x135D <= ch && ch <= 0x135F) || (0x1369 <= ch && ch <= 0x1371) || (0x1380 <= ch && ch <= 0x138F) || (0x13A0 <= ch && ch <= 0x13F4) || (0x1401 <= ch && ch <= 0x166C) || (0x166F <= ch && ch <= 0x167F) || (0x1681 <= ch && ch <= 0x169A) || (0x16A0 <= ch && ch <= 0x16EA) || (0x16EE <= ch && ch <= 0x16F8) || (0x1700 <= ch && ch <= 0x170C) || (0x170E <= ch && ch <= 0x1714) || (0x1720 <= ch && ch <= 0x1734) || (0x1740 <= ch && ch <= 0x1753) || (0x1760 <= ch && ch <= 0x176C) || (0x176E <= ch && ch <= 0x1770) || (ch == 0x1772) || (ch == 0x1773) || (0x1780 <= ch && ch <= 0x17D3) || (ch == 0x17D7) || (ch == 0x17DC) || (ch == 0x17DD) || (0x17E0 <= ch && ch <= 0x17E9) || (0x180B <= ch && ch <= 0x180D) || (0x1810 <= ch && ch <= 0x1819) || (0x1820 <= ch && ch <= 0x1877) || (0x1880 <= ch && ch <= 0x18AA) || (0x18B0 <= ch && ch <= 0x18F5) || (0x1900 <= ch && ch <= 0x191E) || (0x1920 <= ch && ch <= 0x192B) || (0x1930 <= ch && ch <= 0x193B) || (0x1946 <= ch && ch <= 0x196D) || (0x1970 <= ch && ch <= 0x1974) || (0x1980 <= ch && ch <= 0x19AB) || (0x19B0 <= ch && ch <= 0x19C9) || (0x19D0 <= ch && ch <= 0x19DA) || (0x1A00 <= ch && ch <= 0x1A1B) || (0x1A20 <= ch && ch <= 0x1A5E) || (0x1A60 <= ch && ch <= 0x1A7C) || (0x1A7F <= ch && ch <= 0x1A89) || (0x1A90 <= ch && ch <= 0x1A99) || (ch == 0x1AA7) || (0x1AB0 <= ch && ch <= 0x1ABD) || (0x1B00 <= ch && ch <= 0x1B4B) || (0x1B50 <= ch && ch <= 0x1B59) || (0x1B6B <= ch && ch <= 0x1B73) || (0x1B80 <= ch && ch <= 0x1BF3) || (0x1C00 <= ch && ch <= 0x1C37) || (0x1C40 <= ch && ch <= 0x1C49) || (0x1C4D <= ch && ch <= 0x1C7D) || (0x1CD0 <= ch && ch <= 0x1CD2) || (0x1CD4 <= ch && ch <= 0x1CF6) || (ch == 0x1CF8) || (ch == 0x1CF9) || (0x1D00 <= ch && ch <= 0x1DF5) || (0x1DFC <= ch && ch <= 0x1F15) || (0x1F18 <= ch && ch <= 0x1F1D) || (0x1F20 <= ch && ch <= 0x1F45) || (0x1F48 <= ch && ch <= 0x1F4D) || (0x1F50 <= ch && ch <= 0x1F57) || (ch == 0x1F59) || (ch == 0x1F5B) || (ch == 0x1F5D) || (0x1F5F <= ch && ch <= 0x1F7D) || (0x1F80 <= ch && ch <= 0x1FB4) || (0x1FB6 <= ch && ch <= 0x1FBC) || (ch == 0x1FBE) || (0x1FC2 <= ch && ch <= 0x1FC4) || (0x1FC6 <= ch && ch <= 0x1FCC) || (0x1FD0 <= ch && ch <= 0x1FD3) || (0x1FD6 <= ch && ch <= 0x1FDB) || (0x1FE0 <= ch && ch <= 0x1FEC) || (0x1FF2 <= ch && ch <= 0x1FF4) || (0x1FF6 <= ch && ch <= 0x1FFC) || (ch == 0x200C) || (ch == 0x200D) || (ch == 0x203F) || (ch == 0x2040) || (ch == 0x2054) || (ch == 0x2071) || (ch == 0x207F) || (0x2090 <= ch && ch <= 0x209C) || (0x20D0 <= ch && ch <= 0x20DC) || (ch == 0x20E1) || (0x20E5 <= ch && ch <= 0x20F0) || (ch == 0x2102) || (ch == 0x2107) || (0x210A <= ch && ch <= 0x2113) || (ch == 0x2115) || (0x2118 <= ch && ch <= 0x211D) || (ch == 0x2124) || (ch == 0x2126) || (ch == 0x2128) || (0x212A <= ch && ch <= 0x2139) || (0x213C <= ch && ch <= 0x213F) || (0x2145 <= ch && ch <= 0x2149) || (ch == 0x214E) || (0x2160 <= ch && ch <= 0x2188) || (0x2C00 <= ch && ch <= 0x2C2E) || (0x2C30 <= ch && ch <= 0x2C5E) || (0x2C60 <= ch && ch <= 0x2CE4) || (0x2CEB <= ch && ch <= 0x2CF3) || (0x2D00 <= ch && ch <= 0x2D25) || (ch == 0x2D27) || (ch == 0x2D2D) || (0x2D30 <= ch && ch <= 0x2D67) || (ch == 0x2D6F) || (0x2D7F <= ch && ch <= 0x2D96) || (0x2DA0 <= ch && ch <= 0x2DA6) || (0x2DA8 <= ch && ch <= 0x2DAE) || (0x2DB0 <= ch && ch <= 0x2DB6) || (0x2DB8 <= ch && ch <= 0x2DBE) || (0x2DC0 <= ch && ch <= 0x2DC6) || (0x2DC8 <= ch && ch <= 0x2DCE) || (0x2DD0 <= ch && ch <= 0x2DD6) || (0x2DD8 <= ch && ch <= 0x2DDE) || (0x2DE0 <= ch && ch <= 0x2DFF) || (0x3005 <= ch && ch <= 0x3007) || (0x3021 <= ch && ch <= 0x302F) || (0x3031 <= ch && ch <= 0x3035) || (0x3038 <= ch && ch <= 0x303C) || (0x3041 <= ch && ch <= 0x3096) || (0x3099 <= ch && ch <= 0x309F) || (0x30A1 <= ch && ch <= 0x30FA) || (0x30FC <= ch && ch <= 0x30FF) || (0x3105 <= ch && ch <= 0x312D) || (0x3131 <= ch && ch <= 0x318E) || (0x31A0 <= ch && ch <= 0x31BA) || (0x31F0 <= ch && ch <= 0x31FF) || (0x3400 <= ch && ch <= 0x4DB5) || (0x4E00 <= ch && ch <= 0x9FCC) || (0xA000 <= ch && ch <= 0xA48C) || (0xA4D0 <= ch && ch <= 0xA4FD) || (0xA500 <= ch && ch <= 0xA60C) || (0xA610 <= ch && ch <= 0xA62B) || (0xA640 <= ch && ch <= 0xA66F) || (0xA674 <= ch && ch <= 0xA67D) || (0xA67F <= ch && ch <= 0xA69D) || (0xA69F <= ch && ch <= 0xA6F1) || (0xA717 <= ch && ch <= 0xA71F) || (0xA722 <= ch && ch <= 0xA788) || (0xA78B <= ch && ch <= 0xA78E) || (0xA790 <= ch && ch <= 0xA7AD) || (ch == 0xA7B0) || (ch == 0xA7B1) || (0xA7F7 <= ch && ch <= 0xA827) || (0xA840 <= ch && ch <= 0xA873) || (0xA880 <= ch && ch <= 0xA8C4) || (0xA8D0 <= ch && ch <= 0xA8D9) || (0xA8E0 <= ch && ch <= 0xA8F7) || (ch == 0xA8FB) || (0xA900 <= ch && ch <= 0xA92D) || (0xA930 <= ch && ch <= 0xA953) || (0xA960 <= ch && ch <= 0xA97C) || (0xA980 <= ch && ch <= 0xA9C0) || (0xA9CF <= ch && ch <= 0xA9D9) || (0xA9E0 <= ch && ch <= 0xA9FE) || (0xAA00 <= ch && ch <= 0xAA36) || (0xAA40 <= ch && ch <= 0xAA4D) || (0xAA50 <= ch && ch <= 0xAA59) || (0xAA60 <= ch && ch <= 0xAA76) || (0xAA7A <= ch && ch <= 0xAAC2) || (0xAADB <= ch && ch <= 0xAADD) || (0xAAE0 <= ch && ch <= 0xAAEF) || (0xAAF2 <= ch && ch <= 0xAAF6) || (0xAB01 <= ch && ch <= 0xAB06) || (0xAB09 <= ch && ch <= 0xAB0E) || (0xAB11 <= ch && ch <= 0xAB16) || (0xAB20 <= ch && ch <= 0xAB26) || (0xAB28 <= ch && ch <= 0xAB2E) || (0xAB30 <= ch && ch <= 0xAB5A) || (0xAB5C <= ch && ch <= 0xAB5F) || (ch == 0xAB64) || (ch == 0xAB65) || (0xABC0 <= ch && ch <= 0xABEA) || (ch == 0xABEC) || (ch == 0xABED) || (0xABF0 <= ch && ch <= 0xABF9) || (0xAC00 <= ch && ch <= 0xD7A3) || (0xD7B0 <= ch && ch <= 0xD7C6) || (0xD7CB <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFA6D) || (0xFA70 <= ch && ch <= 0xFAD9) || (0xFB00 <= ch && ch <= 0xFB06) || (0xFB13 <= ch && ch <= 0xFB17) || (0xFB1D <= ch && ch <= 0xFB28) || (0xFB2A <= ch && ch <= 0xFB36) || (0xFB38 <= ch && ch <= 0xFB3C) || (ch == 0xFB3E) || (ch == 0xFB40) || (ch == 0xFB41) || (ch == 0xFB43) || (ch == 0xFB44) || (0xFB46 <= ch && ch <= 0xFBB1) || (0xFBD3 <= ch && ch <= 0xFD3D) || (0xFD50 <= ch && ch <= 0xFD8F) || (0xFD92 <= ch && ch <= 0xFDC7) || (0xFDF0 <= ch && ch <= 0xFDFB) || (0xFE00 <= ch && ch <= 0xFE0F) || (0xFE20 <= ch && ch <= 0xFE2D) || (ch == 0xFE33) || (ch == 0xFE34) || (0xFE4D <= ch && ch <= 0xFE4F) || (0xFE70 <= ch && ch <= 0xFE74) || (0xFE76 <= ch && ch <= 0xFEFC) || (0xFF10 <= ch && ch <= 0xFF19) || (0xFF21 <= ch && ch <= 0xFF3A) || (ch == 0xFF3F) || (0xFF41 <= ch && ch <= 0xFF5A) || (0xFF66 <= ch && ch <= 0xFFBE) || (0xFFC2 <= ch && ch <= 0xFFC7) || (0xFFCA <= ch && ch <= 0xFFCF) || (0xFFD2 <= ch && ch <= 0xFFD7) || (0xFFDA <= ch && ch <= 0xFFDC);
}

ALWAYS_INLINE bool isIdentifierStart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return g_asciiRangeCharMap[ch] & ESPRIMA_START_IDENT;
    } else {
        return isIdentifierStartSlow(ch);
    }
}

struct Curly {
    char m_curly[4];
    Curly() {}
    Curly(const char curly[4])
    {
        m_curly[0] = curly[0];
        m_curly[1] = curly[1];
        m_curly[2] = curly[2];
        m_curly[3] = curly[3];
    }
};

namespace Messages {
const char* UnexpectedToken = "Unexpected token %s";
const char* UnexpectedTokenIllegal = "Unexpected token ILLEGAL";
const char* UnexpectedNumber = "Unexpected number";
const char* UnexpectedString = "Unexpected string";
const char* UnexpectedIdentifier = "Unexpected identifier";
const char* UnexpectedReserved = "Unexpected reserved word";
const char* UnexpectedTemplate = "Unexpected quasi %s";
const char* UnexpectedEOS = "Unexpected end of input";
const char* NewlineAfterThrow = "Illegal newline after throw";
const char* InvalidRegExp = "Invalid regular expression";
const char* UnterminatedRegExp = "Invalid regular expression: missing /";
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
const char* TemplateOctalLiteral = "Octal literals are not allowed in template strings.";
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

/*
export interface Comment {
    multiLine: boolean;
    slice: number[];
    range: number[];
    loc: any;
}
*/

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

class Scanner;

StringView keywordToString(KeywordKind keyword)
{
    switch (keyword) {
    case If:
        return StringView("if", 2);
    case In:
        return StringView("in", 2);
    case Do:
        return StringView("do", 2);
    case Var:
        return StringView("var", 3);
    case For:
        return StringView("for", 3);
    case New:
        return StringView("new", 3);
    case Try:
        return StringView("try", 3);
    case This:
        return StringView("this", 4);
    case Else:
        return StringView("else", 4);
    case Case:
        return StringView("case", 4);
    case Void:
        return StringView("void", 4);
    case With:
        return StringView("with", 4);
    case Enum:
        return StringView("enum", 4);
    case Await:
        return StringView("await", 5);
    case While:
        return StringView("while", 5);
    case Break:
        return StringView("break", 5);
    case Catch:
        return StringView("catch", 5);
    case Throw:
        return StringView("throw", 5);
    case Const:
        return StringView("const", 5);
    case Class:
        return StringView("class", 5);
    case Super:
        return StringView("super", 5);
    case Return:
        return StringView("return", 6);
    case Typeof:
        return StringView("typeof", 6);
    case Delete:
        return StringView("delete", 6);
    case Switch:
        return StringView("switch", 6);
    case Export:
        return StringView("export", 6);
    case Import:
        return StringView("import", 6);
    case Default:
        return StringView("default", 7);
    case Finally:
        return StringView("finally", 7);
    case Extends:
        return StringView("extends", 7);
    case Function:
        return StringView("function", 8);
    case Continue:
        return StringView("continue", 8);
    case Debugger:
        return StringView("debugger", 8);
    case InstanceofKeyword:
        return StringView("instanceof", 10);
    case Implements:
        return StringView("implements", 10);
    case Interface:
        return StringView("interface", 9);
    case Package:
        return StringView("package", 7);
    case Private:
        return StringView("private", 7);
    case Protected:
        return StringView("protected", 9);
    case Public:
        return StringView("public", 6);
    case Static:
        return StringView("static", 6);
    case Yield:
        return StringView("yield", 5);
    case Let:
        return StringView("let", 3);
    default:
        ASSERT_NOT_REACHED();
        return StringView("error", 5);
    }
}

class ScannerResult : public RefCounted<ScannerResult> {
public:
    Scanner* scanner;
    unsigned char type : 4;
    bool startWithZero : 1;
    bool octal : 1;
    bool plain : 1;
    bool hasComplexString : 1;
    bool hasKeywordButUseString : 1;
    char prec : 8; // max prec is 11
    // we don't needs init prec.
    // prec is initialized by another function before use

    size_t lineNumber;
    size_t lineStart;
    size_t start;
    size_t end;

    union {
        PunctuatorsKind valuePunctuatorsKind;
        StringView valueStringLiteralData;
        double valueNumber;
        ScanTemplteResult* valueTemplate;
        ScanRegExpResult valueRegexp;
        KeywordKind valueKeywordKind;
    };

    StringView relatedSource();
    StringView valueStringLiteral()
    {
        if (this->type == Token::KeywordToken && !this->hasKeywordButUseString) {
            return keywordToString(this->valueKeywordKind);
        }
        ASSERT(valueStringLiteralData.getTagInFirstDataArea() == POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
        return valueStringLiteralData;
    }

    Value valueStringLiteralForAST();

    inline ~ScannerResult();

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
        this->hasComplexString = false;
        this->lineNumber = lineNumber;
        this->lineStart = lineStart;
        this->start = start;
        this->end = end;
    }

    ScannerResult(Scanner* scanner, Token type, const StringView& valueString, size_t lineNumber, size_t lineStart, size_t start, size_t end, bool hasComplexString)
        : valueStringLiteralData(valueString)
    {
        this->scanner = scanner;
        this->type = type;
        this->startWithZero = this->octal = false;
        this->hasKeywordButUseString = true;
        this->plain = false;
        this->hasComplexString = hasComplexString;
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
        this->hasComplexString = false;
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
        this->hasComplexString = false;
        this->lineNumber = lineNumber;
        this->lineStart = lineStart;
        this->start = start;
        this->end = end;
    }
};

class ErrorHandler : public gc {
public:
    // errors: Error[];
    // tolerant: boolean;

    ErrorHandler()
    {
        // this->errors = [];
        // this->tolerant = false;
    }

    // recordError(error: Error): void {
    //     this->errors.push(error);
    // };

    void tolerate(Error* error)
    {
        /*
        if (this->tolerant) {
            this->recordError(error);
        } else {
            throw error;
        }*/
        throw error;
    }

    Error* constructError(String* msg, size_t column)
    {
        Error* error = new (NoGC) Error(msg);
        error->column = column;
        return error;
        // try {
        //     throw error;
        // } catch (base) {
        /* istanbul ignore else */
        //     if (Object.create && Object.defineProperty) {
        //         error = Object.create(base);
        //         Object.defineProperty(error, 'column', { value: column });
        //     }
        // } finally {
        //     return error;
        // }
    }

    Error* createError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
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
        Error* error = constructError(new UTF16String(msg.data(), msg.length()), col);
        error->index = index;
        error->lineNumber = line;
        error->description = description;
        error->errorCode = code;
        return error;
    };

    void throwError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
    {
        throw this->createError(index, line, col, description, code);
    }

    void tolerateError(size_t index, size_t line, size_t col, String* description, ErrorObject::Code code)
    {
        Error* error = this->createError(index, line, col, description, code);
        /*
        if (this->tolerant) {
            this->recordError(error);
        } else {
            throw error;
        }*/
        throw error;
    }
};

#define SCANNER_RESULT_POOL_INITIAL_SIZE 128
class Scanner : public gc {
public:
    StringView source;
    ErrorHandler* errorHandler;
    // trackComment: boolean;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;
    std::vector<Curly> curlyStack;
    bool isPoolEnabled;
    ScannerResult* initialResultMemoryPool[SCANNER_RESULT_POOL_INITIAL_SIZE];
    size_t initialResultMemoryPoolSize;
    std::vector<ScannerResult*, gc_allocator<ScannerResult*>> resultMemoryPool;
    char scannerResultInnerPool[SCANNER_RESULT_POOL_INITIAL_SIZE * sizeof(ScannerResult)];

    ~Scanner()
    {
        isPoolEnabled = false;
    }


    Scanner(StringView code, ErrorHandler* handler, size_t startLine = 0, size_t startColumn = 0)
    {
        curlyStack.reserve(128);
        isPoolEnabled = true;
        source = code;
        errorHandler = handler;
        // trackComment = false;

        length = code.length();
        index = 0;
        lineNumber = ((length > 0) ? 1 : 0) + startLine;
        lineStart = startColumn;

        initialResultMemoryPoolSize = SCANNER_RESULT_POOL_INITIAL_SIZE;
        ScannerResult* ptr = (ScannerResult*)scannerResultInnerPool;
        for (size_t i = 0; i < SCANNER_RESULT_POOL_INITIAL_SIZE; i++) {
            ptr[i].scanner = this;
            initialResultMemoryPool[i] = &ptr[i];
        }
    }

    ScannerResult* createScannerResult()
    {
        if (initialResultMemoryPoolSize) {
            initialResultMemoryPoolSize--;
            return initialResultMemoryPool[initialResultMemoryPoolSize];
        } else if (resultMemoryPool.size() == 0) {
            auto ret = (ScannerResult*)GC_MALLOC(sizeof(ScannerResult));
            return ret;
        } else {
            auto ret = resultMemoryPool.back();
            resultMemoryPool.pop_back();
            return ret;
        }
    }

    bool eof()
    {
        return index >= length;
    }

    void throwUnexpectedToken(const char* message = Messages::UnexpectedTokenIllegal)
    {
        this->errorHandler->throwError(this->index, this->lineNumber, this->index - this->lineStart + 1, new ASCIIString(message), ErrorObject::SyntaxError);
    }
    /*
    tolerateUnexpectedToken() {
        this->errorHandler.tolerateError(this->index, this->lineNumber,
            this->index - this->lineStart + 1, Messages.UnexpectedTokenIllegal);
    };
*/
    void tolerateUnexpectedToken()
    {
        throwUnexpectedToken();
    }
    // ECMA-262 11.4 Comments

    // skipSingleLineComment(offset: number): Comment[] {
    void skipSingleLineComment(size_t /*offset*/)
    {
        // let comments: Comment[];
        // size_t start, loc;

        /*
        if (this->trackComment) {
            comments = [];
            start = this->index - offset;
            loc = {
                start: {
                    line: this->lineNumber,
                    column: this->index - this->lineStart - offset
                },
                end: {}
            };
        }*/

        while (!this->eof()) {
            char16_t ch = this->source.bufferedCharAt(this->index);
            ++this->index;
            if (isLineTerminator(ch)) {
                /*
                if (this->trackComment) {
                    loc.end = {
                        line: this->lineNumber,
                        column: this->index - this->lineStart - 1
                    };
                    const entry: Comment = {
                        multiLine: false,
                        slice: [start + offset, this->index - 1],
                        range: [start, this->index - 1],
                        loc: loc
                    };
                    comments.push(entry);
                }*/
                if (ch == 13 && this->source.bufferedCharAt(this->index) == 10) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                // return comments;
                return;
            }
        }

        /*
        if (this->trackComment) {
            loc.end = {
                line: this->lineNumber,
                column: this->index - this->lineStart
            };
            const entry: Comment = {
                multiLine: false,
                slice: [start + offset, this->index],
                range: [start, this->index],
                loc: loc
            };
            comments.push(entry);
        }*/

        // return comments;
        return;
    }

    // skipMultiLineComment(): Comment[] {
    void skipMultiLineComment()
    {
        // let comments: Comment[];
        // size_t start, loc;
        /*
        if (this->trackComment) {
            comments = [];
            start = this->index - 2;
            loc = {
                start: {
                    line: this->lineNumber,
                    column: this->index - this->lineStart - 2
                },
                end: {}
            };
        }
         */
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
                    /*
                    if (this->trackComment) {
                        loc.end = {
                            line: this->lineNumber,
                            column: this->index - this->lineStart
                        };
                        const entry: Comment = {
                            multiLine: true,
                            slice: [start + 2, this->index - 2],
                            range: [start, this->index],
                            loc: loc
                        };
                        comments.push(entry);
                    }
                    return comments;
                    */
                    return;
                }
                ++this->index;
            } else {
                ++this->index;
            }
        }

        /*
        // Ran off the end of the file - the whole thing is a comment
        if (this->trackComment) {
            loc.end = {
                line: this->lineNumber,
                column: this->index - this->lineStart
            };
            const entry: Comment = {
                multiLine: true,
                slice: [start + 2, this->index],
                range: [start, this->index],
                loc: loc
            };
            comments.push(entry);
        }*/

        tolerateUnexpectedToken();
        // return comments;
        return;
    }

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
                    this->skipSingleLineComment(2);
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
                    this->skipSingleLineComment(3);
                } else {
                    break;
                }
            } else if (ch == 0x3C) { // U+003C is '<'
                if (this->length > this->index + 4) {
                    if (this->source.bufferedCharAt(this->index + 1) == '!'
                        && this->source.bufferedCharAt(this->index + 2) == '-'
                        && this->source.bufferedCharAt(this->index + 3) == '-') {
                        this->index += 4; // `<!--`
                        this->skipSingleLineComment(4);
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

        return;
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

    // ECMA-262 11.6.2.1 Keywords
    ALWAYS_INLINE KeywordKind isKeyword(const StringBufferAccessData& data)
    {
        // 'const' is specialized as Keyword in V8.
        // 'yield' and 'let' are for compatibility with SpiderMonkey and ES.next.
        // Some others are from future reserved words.

        register size_t length = data.length;
        char16_t first = data.charAt(0);
        char16_t second;
        switch (first) {
        case 'a':
            // TODO await
            break;
        case 'b':
            if (length == 5 && data.equalsSameLength("break", 1)) {
                return Break;
            }
            break;
        case 'c':
            if (length == 4) {
                if (data.equalsSameLength("case", 1)) {
                    return Case;
                }
            } else if (length == 5) {
                second = data.charAt(1);
                if (second == 'a' && data.equalsSameLength("catch", 2)) {
                    return Catch;
                } else if (second == 'o' && data.equalsSameLength("const", 2)) {
                    const char* env = getenv("ESCARGOT_TREAT_CONST_AS_VAR");
                    if (env && strlen(env)) {
                        return Var;
                    }
                    return Const;
                } else if (second == 'l' && data.equalsSameLength("class", 2)) {
                    return Class;
                }
            } else if (length == 8) {
                if (data.equalsSameLength("continue", 1)) {
                    return Continue;
                }
            }
            break;
        case 'd':
            if (length == 8) {
                if (data.equalsSameLength("debugger", 1)) {
                    return Debugger;
                }
            } else if (length == 2) {
                if (data.equalsSameLength("do", 1)) {
                    return Do;
                }
            } else if (length == 6) {
                if (data.equalsSameLength("delete", 1)) {
                    return Delete;
                }
            } else if (length == 7) {
                if (data.equalsSameLength("default", 1)) {
                    return Default;
                }
            }
            break;
        case 'e':
            if (length == 4) {
                second = data.charAt(1);
                if (second == 'l' && data.equalsSameLength("else", 2)) {
                    return Else;
                } else if (second == 'n' && data.equalsSameLength("enum", 2)) {
                    return Enum;
                }
            } else if (length == 6) {
                if (data.equalsSameLength("export", 1)) {
                    return Export;
                }
            } else if (length == 7) {
                if (data.equalsSameLength("extends", 1)) {
                    return Extends;
                }
            }
            break;
        case 'f':
            if (length == 3) {
                if (data.equalsSameLength("for", 1)) {
                    return For;
                }
            } else if (length == 7) {
                if (data.equalsSameLength("finally", 1)) {
                    return Finally;
                }
            } else if (length == 8) {
                if (data.equalsSameLength("function", 1)) {
                    return Function;
                }
            }
            break;
        case 'i':
            if (length == 2) {
                second = data.charAt(1);
                if (second == 'f') {
                    return If;
                } else if (second == 'n') {
                    return In;
                }
            } else if (length == 6) {
                if (data.equalsSameLength("import", 1)) {
                    return Import;
                }
            } else if (length == 10) {
                if (data.equalsSameLength("instanceof", 1)) {
                    return InstanceofKeyword;
                }
            }
            break;
        case 'l':
            if (length == 3 && data.equalsSameLength("let", 1)) {
                return Let;
            }
            break;
        case 'n':
            if (length == 3 && data.equalsSameLength("new", 1)) {
                return New;
            }
            break;
        case 'r':
            if (length == 6 && data.equalsSameLength("return", 1)) {
                return Return;
            }
            break;
        case 's':
            if (length == 5 && data.equalsSameLength("super", 1)) {
                return Super;
            } else if (length == 6 && data.equalsSameLength("switch", 1)) {
                return Switch;
            }
            break;
        case 't':
            switch (length) {
            case 3:
                if (data.equalsSameLength("try", 1)) {
                    return Try;
                }
                break;
            case 4:
                if (data.equalsSameLength("this", 1)) {
                    return This;
                }
                break;
            case 5:
                if (data.equalsSameLength("throw", 1)) {
                    return Throw;
                }
                break;
            case 6:
                if (data.equalsSameLength("typeof", 1)) {
                    return Typeof;
                }
                break;
            }
            break;
        case 'v':
            if (length == 3 && data.equalsSameLength("var", 1)) {
                return Var;
            } else if (length == 4 && data.equalsSameLength("void", 1)) {
                return Void;
            }
            break;
        case 'w':
            if (length == 4 && data.equalsSameLength("with", 1)) {
                return With;
            } else if (length == 5 && data.equalsSameLength("while", 1)) {
                return While;
            }
            break;
        case 'y':
            if (length == 5 && data.equalsSameLength("yield", 1)) {
                return Yield;
            }
            break;
        }
        return NotKeyword;
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

    int hexValue(char16_t ch)
    {
        int c = 0;
        if (ch >= '0' && ch <= '9') {
            c = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            c = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            c = ch - 'A' + 10;
        } else {
            RELEASE_ASSERT_NOT_REACHED();
        }
        return c;
    }

    struct CharOrEmptyResult {
        char32_t code;
        bool isEmpty;

        CharOrEmptyResult(char32_t code, bool isEmpty)
        {
            this->code = code;
            this->isEmpty = isEmpty;
        }
    };

    CharOrEmptyResult scanHexEscape(char prefix)
    {
        size_t len = (prefix == 'u') ? 4 : 2;
        char32_t code = 0;

        for (size_t i = 0; i < len; ++i) {
            if (!this->eof() && isHexDigit(this->source.bufferedCharAt(this->index))) {
                code = code * 16 + hexValue(this->source.bufferedCharAt(this->index++));
            } else {
                return CharOrEmptyResult(0, true);
            }
        }

        return CharOrEmptyResult(code, false);
    }

    char32_t scanUnicodeCodePointEscape()
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

    StringView getIdentifier()
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

    StringView getComplexIdentifier()
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
                CharOrEmptyResult res = this->scanHexEscape('u');
                ch = res.code;
                cp = ch;
                if (res.isEmpty || ch == '\\' || !isIdentifierStart(cp)) {
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
                    CharOrEmptyResult res = this->scanHexEscape('u');
                    ch = res.code;
                    cp = ch;
                    if (res.isEmpty || ch == '\\' || !isIdentifierPart(cp)) {
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

    struct OctalToDecimalResult {
        char16_t code;
        bool octal;

        OctalToDecimalResult(char16_t code, bool octal)
        {
            this->code = code;
            this->octal = octal;
        }
    };
    OctalToDecimalResult octalToDecimal(char16_t ch)
    {
        // \0 is not octal escape sequence
        bool octal = (ch != '0');
        char16_t code = octalValue(ch);

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

        return OctalToDecimalResult(code, octal);
    };

    // ECMA-262 11.6 Names and Keywords

    ALWAYS_INLINE PassRefPtr<ScannerResult> scanIdentifier(char16_t ch0)
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
        } else if ((keywordKind = this->isKeyword(data))) {
            PassRefPtr<ScannerResult> r = adoptRef(new (createScannerResult()) ScannerResult(this, Token::KeywordToken, this->lineNumber, this->lineStart, start, this->index));
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

        return adoptRef(new (createScannerResult()) ScannerResult(this, type, id, this->lineNumber, this->lineStart, start, this->index, id.string() != this->source.string()));
    }

    // ECMA-262 11.7 Punctuators
    PassRefPtr<ScannerResult> scanPunctuator(char16_t ch0)
    {
        PassRefPtr<ScannerResult> token = adoptRef(new (createScannerResult()) ScannerResult(this, Token::PunctuatorToken, this->lineNumber, this->lineStart, this->index, this->index));

        PunctuatorsKind kind;
        // Check for most common single-character punctuators.
        size_t start = this->index;
        char16_t ch1, ch2, ch3;
        switch (ch0) {
        case '(':
            ++this->index;
            kind = LeftParenthesis;
            break;

        case '{':
            this->curlyStack.push_back(Curly("{\0\0"));
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
            if (!this->curlyStack.size()) {
                this->throwUnexpectedToken();
            }
            this->curlyStack.pop_back();
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
            kind = PunctuatorsKindEnd;
            break;
        }

        if (UNLIKELY(this->index == token->start)) {
            this->throwUnexpectedToken();
        }

        token->valuePunctuatorsKind = kind;
        return token;
    }

    // ECMA-262 11.8.3 Numeric Literals

    PassRefPtr<ScannerResult> scanHexLiteral(size_t start)
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
            return adoptRef(new (createScannerResult()) ScannerResult(this, Token::NumericLiteralToken, numberDouble, this->lineNumber, this->lineStart, start, this->index));
        } else {
            ASSERT(numberDouble == 0.0);
            return adoptRef(new (createScannerResult()) ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
        }
    }

    PassRefPtr<ScannerResult> scanBinaryLiteral(size_t start)
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

        return adoptRef(new (createScannerResult()) ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
    }

    PassRefPtr<ScannerResult> scanOctalLiteral(char16_t prefix, size_t start)
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
        PassRefPtr<ScannerResult> ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index));
        ret->octal = octal;

        return ret;
    }

    bool isImplicitOctalLiteral()
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

    PassRefPtr<ScannerResult> scanNumericLiteral()
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

        auto ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::NumericLiteralToken, ll, this->lineNumber, this->lineStart, start, this->index));
        if (startChar == '0' && length >= 2 && ll >= 1) {
            ret->startWithZero = true;
        }
        return ret;
    }

    // ECMA-262 11.8.4 String Literals

    PassRefPtr<ScannerResult> scanStringLiteral()
    {
        // TODO apply rope-string
        const size_t start = this->index;
        char16_t quote = this->source.bufferedCharAt(start);
        ASSERT((quote == '\'' || quote == '"'));
        // 'String literal must starts with a quote');

        ++this->index;
        bool octal = false;
        bool isPlainCase = true;

        UTF16StringDataNonGCStd stringUTF16;
        size_t plainCaseStart = start + 1;
        size_t plainCaseEnd = start + 1;

#define CONVERT_UNPLAIN_CASE_IF_NEEDED()                                                   \
    if (isPlainCase) {                                                                     \
        auto temp = StringView(this->source, start + 1, plainCaseEnd).toUTF16StringData(); \
        stringUTF16.reserve(32);                                                           \
        stringUTF16.insert(stringUTF16.end(), temp.data(), &temp.data()[temp.length()]);   \
        isPlainCase = false;                                                               \
    }

        while (LIKELY(!this->eof())) {
            char16_t ch = this->source.bufferedCharAt(this->index++);

            if (ch == quote) {
                quote = '\0';
                break;
            } else if (UNLIKELY(ch == '\\')) {
                ch = this->source.bufferedCharAt(this->index++);
                CONVERT_UNPLAIN_CASE_IF_NEEDED()
                if (!ch || !isLineTerminator(ch)) {
                    switch (ch) {
                    case 'u':
                    case 'x':
                        if (this->source.bufferedCharAt(this->index) == '{') {
                            ++this->index;
                            ParserCharPiece piece(this->scanUnicodeCodePointEscape());
                            stringUTF16.append(piece.data, piece.data + piece.length);
                        } else {
                            CharOrEmptyResult res = this->scanHexEscape(ch);
                            if (res.isEmpty) {
                                this->throwUnexpectedToken();
                            }
                            const char32_t unescaped = res.code;
                            ParserCharPiece piece(unescaped);
                            stringUTF16.append(piece.data, piece.data + piece.length);
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
                            OctalToDecimalResult octToDec = this->octalToDecimal(ch);

                            octal = octToDec.octal || octal;
                            stringUTF16 += octToDec.code;
                        } else if (isDecimalDigit(ch)) {
                            octal = true;
                            stringUTF16 += ch;
                        } else {
                            stringUTF16 += ch;
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
                if (isPlainCase) {
                    plainCaseEnd++;
                } else {
                    stringUTF16 += ch;
                }
            }
        }

        if (quote != '\0') {
            this->index = start;
            this->throwUnexpectedToken();
        }

        if (isPlainCase) {
            bool isNewString = false;
            StringView str(this->source, plainCaseStart, plainCaseEnd);
            auto ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::StringLiteralToken, str, /*octal, */ this->lineNumber, this->lineStart, start, this->index, isNewString));
            ret->octal = octal;
            ret->plain = isPlainCase;
            return ret;
        } else {
            String* newStr;
            if (isAllLatin1(stringUTF16.data(), stringUTF16.length())) {
                newStr = new Latin1String(stringUTF16.data(), stringUTF16.length());
            } else {
                newStr = new UTF16String(stringUTF16.data(), stringUTF16.length());
            }
            auto ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::StringLiteralToken, StringView(newStr, 0, newStr->length()), /*octal, */ this->lineNumber, this->lineStart, start, this->index, true));
            ret->octal = octal;
            ret->plain = isPlainCase;
            return ret;
        }
    }

    // ECMA-262 11.8.6 Template Literal Lexical Components

    PassRefPtr<ScannerResult> scanTemplate()
    {
        // TODO apply rope-string
        UTF16StringDataNonGCStd cooked;
        bool terminated = false;
        size_t start = this->index;

        bool head = (this->source.bufferedCharAt(start) == '`');
        bool tail = false;
        size_t rawOffset = 2;

        ++this->index;

        while (!this->eof()) {
            char16_t ch = this->source.bufferedCharAt(this->index++);
            if (ch == '`') {
                rawOffset = 1;
                tail = true;
                terminated = true;
                break;
            } else if (ch == '$') {
                if (this->source.bufferedCharAt(this->index) == '{') {
                    this->curlyStack.push_back(Curly("${\0"));
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
                            CharOrEmptyResult res = this->scanHexEscape(ch);
                            const char32_t unescaped = res.code;
                            if (!res.isEmpty) {
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

        if (!head) {
            this->curlyStack.pop_back();
        }

        ScanTemplteResult* result = new ScanTemplteResult();
        result->head = head;
        result->tail = tail;
        result->raw = StringView(this->source, start + 1, this->index - rawOffset);
        result->valueCooked = UTF16StringData(cooked.data(), cooked.length());

        return adoptRef(new (createScannerResult()) ScannerResult(this, Token::TemplateToken, result, this->lineNumber, this->lineStart, start, this->index));
    }

    // ECMA-262 11.8.5 Regular Expression Literals
    /*
    testRegExp(pattern: string, flags: string) {
        // The BMP character to use as a replacement for astral symbols when
        // translating an ES6 "u"-flagged pattern to an ES5-compatible
        // approximation.
        // Note: replacing with '\uFFFF' enables false positives in unlikely
        // scenarios. For example, `[\u{1044f}-\u{10440}]` is an invalid
        // pattern that would not be detected by this substitution.
        const astralSubstitute = '\uFFFF';
        let tmp = pattern;
        let self = this;

        if (flags.indexOf('u') >= 0) {
            tmp = tmp
                // Replace every Unicode escape sequence with the equivalent
                // BMP character or a constant ASCII code point in the case of
                // astral symbols. (See the above note on `astralSubstitute`
                // for more information.)
                .replace(/\\u\{([0-9a-fA-F]+)\}|\\u([a-fA-F0-9]{4})/g, function($0, $1, $2) {
                    const codePoint = parseInt($1 || $2, 16);
                    if (codePoint > 0x10FFFF) {
                        self.throwUnexpectedToken(Messages.InvalidRegExp);
                    }
                    if (codePoint <= 0xFFFF) {
                        return String.fromCharCode(codePoint);
                    }
                    return astralSubstitute;
                })
                // Replace each paired surrogate with a single ASCII symbol to
                // avoid throwing on regular expressions that are only valid in
                // combination with the "u" flag.
                .replace(
                /[\uD800-\uDBFF][\uDC00-\uDFFF]/g,
                astralSubstitute
                );
        }

        // First, detect invalid regular expressions.
        try {
            RegExp(tmp);
        } catch (e) {
            this->throwUnexpectedToken(Messages.InvalidRegExp);
        }

        // Return a regular expression object for this pattern-flag pair, or
        // `null` in case the current environment doesn't support the flags it
        // uses.
        try {
            return new RegExp(pattern, flags);
        } catch (exception) {
            return null;
        }
    };
    */

    String* scanRegExpBody()
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
        /*
        return {
            value: body,
            literal: str
        };*/
        if (isAllASCII(str.data(), str.length())) {
            return new ASCIIString(str.data(), str.length());
        } else {
            return new UTF16String(str.data(), str.length());
        }
    }

    String* scanRegExpFlags()
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
                    char32_t ch32;
                    CharOrEmptyResult res = this->scanHexEscape('u');
                    ch32 = res.code;
                    if (!res.isEmpty) {
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
                    this->tolerateUnexpectedToken();
                } else {
                    // str += '\\';
                    this->tolerateUnexpectedToken();
                }
            } else {
                flags += ch;
                // str += ch;
            }
        }

        /*
        return {
            value: flags,
            literal: str
        };
        */
        if (isAllASCII(flags.data(), flags.length())) {
            return new ASCIIString(flags.data(), flags.length());
        } else {
            return new UTF16String(flags.data(), flags.length());
        }
    }

    PassRefPtr<ScannerResult> scanRegExp()
    {
        const size_t start = this->index;

        String* body = this->scanRegExpBody();
        String* flags = this->scanRegExpFlags();
        // const value = this->testRegExp(body.value, flags.value);

        ScanRegExpResult result;
        result.body = body;
        result.flags = flags;
        PassRefPtr<ScannerResult> res = adoptRef(new (createScannerResult()) ScannerResult(this, Token::RegularExpressionToken, this->lineNumber, this->lineStart, start, this->index));
        res->valueRegexp = result;
        return res;
    };

    ALWAYS_INLINE PassRefPtr<ScannerResult> lex()
    {
        if (UNLIKELY(this->eof())) {
            return adoptRef(new (createScannerResult()) ScannerResult(this, Token::EOFToken, this->lineNumber, this->lineStart, this->index, this->index));
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

        // Template literals start with ` (U+0060) for template head
        // or } (U+007D) for template middle or template tail.
        if (UNLIKELY(cp == 0x60 || (cp == 0x7D && (this->curlyStack.size() && strcmp(this->curlyStack.back().m_curly, "${") == 0)))) {
            return this->scanTemplate();
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
};

inline ScannerResult::~ScannerResult()
{
    if (this->scanner->isPoolEnabled) {
        if (this->scanner->initialResultMemoryPoolSize < SCANNER_RESULT_POOL_INITIAL_SIZE) {
            this->scanner->initialResultMemoryPool[this->scanner->initialResultMemoryPoolSize++] = this;
            return;
        }
        this->scanner->resultMemoryPool.push_back(this);
    }
}

StringView ScannerResult::relatedSource()
{
    return StringView(scanner->source, this->start, this->end);
}

Value ScannerResult::valueStringLiteralForAST()
{
    StringView sv = valueStringLiteral();
    if (this->hasComplexString) {
        return new StringView(sv);
    } else {
        return new SourceStringView(sv);
    }
}

struct Config : public gc {
    bool range : 1;
    bool loc : 1;
    bool tokens : 1;
    bool comment : 1;
    bool tolerant : 1;
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
    RefPtr<ScannerResult> firstCoverInitializedNameError;
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
    std::unordered_map<IdentifierNode*, RefPtr<ScannerResult>,
                       std::hash<IdentifierNode*>, std::equal_to<IdentifierNode*>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<IdentifierNode*, RefPtr<ScannerResult>>>>
        nodeExtraInfo;
    */

    enum SourceType {
        Script,
        Module
    };
    SourceType sourceType;
    RefPtr<ScannerResult> lookahead;
    bool hasLineTerminator;

    Context contextInstance;
    Context* context;
    std::vector<RefPtr<ScannerResult>, gc_allocator_ignore_off_page<RefPtr<ScannerResult>>> tokens;
    Marker baseMarker;
    Marker startMarker;
    Marker lastMarker;

    Vector<ASTScopeContext*, gc_allocator_ignore_off_page<ASTScopeContext*>> scopeContexts;
    bool trackUsingNames;
    size_t stackLimit;

    typedef std::pair<ASTNodeType, AtomicString> ScanExpressionResult;

    ASTScopeContext fakeContext;

    ASTScopeContext* popScopeContext(const MetaNode& node)
    {
        auto ret = scopeContexts.back();
        scopeContexts.pop_back();
        return ret;
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
            scopeContexts.push_back(&fakeContext);
            return;
        }
        auto parentContext = scopeContexts.back();
        scopeContexts.push_back(new ASTScopeContext(this->context->strict));
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
        , scannerInstance(code, this->errorHandler, startLine, startColumn)
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
        config.tolerant = false;
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
            tolerant: (typeof options.tolerant == 'boolean') && options.tolerant
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

        this->nextToken();
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
    Error* unexpectedTokenError(RefPtr<ScannerResult> token = nullptr, const char* message = nullptr)
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

    void throwUnexpectedToken(RefPtr<ScannerResult> token, const char* message = nullptr)
    {
        throw this->unexpectedTokenError(token, message);
    }

    void tolerateUnexpectedToken(RefPtr<ScannerResult> token, const char* message = nullptr)
    {
        // this->errorHandler.tolerate(this->unexpectedTokenError(token, message));
        throwUnexpectedToken(token, message);
    }

    ALWAYS_INLINE void collectComments()
    {
        this->scanner->scanComments();
        /*
        if (!this->config.comment) {
            this->scanner->scanComments();
        } else {
            const comments: Comment[] = this->scanner->scanComments();
            if (comments.length > 0 && this->delegate) {
                for (let i = 0; i < comments.length; ++i) {
                    const e: Comment = comments[i];
                    let node;
                    node = {
                        type: e.multiLine ? 'BlockComment' : 'LineComment',
                        value: this->scanner->source.slice(e.slice[0], e.slice[1])
                    };
                    if (this->config.range) {
                        node.range = e.range;
                    }
                    if (this->config.loc) {
                        node.loc = e.loc;
                    }
                    const metadata = {
                        start: {
                            line: e.loc.start.line,
                            column: e.loc.start.column,
                            offset: e.range[0]
                        },
                        end: {
                            line: e.loc.end.line,
                            column: e.loc.end.column,
                            offset: e.range[1]
                        }
                    };
                    this->delegate(node, metadata);
                }
            }
        }*/
    }

    PassRefPtr<ScannerResult> nextToken()
    {
        PassRefPtr<ScannerResult> token(this->lookahead.release());

        this->lastMarker.index = this->scanner->index;
        this->lastMarker.lineNumber = this->scanner->lineNumber;
        this->lastMarker.lineStart = this->scanner->lineStart;

        this->collectComments();

        this->startMarker.index = this->scanner->index;
        this->startMarker.lineNumber = this->scanner->lineNumber;
        this->startMarker.lineStart = this->scanner->lineStart;

        PassRefPtr<ScannerResult> next = this->scanner->lex();
        this->hasLineTerminator = (token && next) ? (token->lineNumber != next->lineNumber) : false;

        if (next && this->context->strict && next->type == Token::IdentifierToken) {
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

    PassRefPtr<ScannerResult> nextRegexToken()
    {
        this->collectComments();

        RefPtr<ScannerResult> token = this->scanner->scanRegExp();

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

    MetaNode startNode(const RefPtr<ScannerResult>& token)
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
                if (((IdentifierNode*)c->callee())->name().string()->equals("eval")) {
                    scopeContexts.back()->m_hasEval = true;
                    if (this->context->inArrowFunction) {
                        scopeContexts.back()->insertUsingName(this->escargotContext->staticStrings().stringThis);
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
    void expect(PunctuatorsKind value)
    {
        PassRefPtr<ScannerResult> token = this->nextToken();
        if (token->type != Token::PunctuatorToken || token->valuePunctuatorsKind != value) {
            this->throwUnexpectedToken(token);
        }
    }

    // Quietly expect a comma when in tolerant mode, otherwise delegates to expect().

    void expectCommaSeparator()
    {
        /*
        if (this->config.tolerant) {
            let token = this->lookahead;
            if (token.type == Token.Punctuator && token.value == ',') {
                this->nextToken();
            } else if (token.type == Token.Punctuator && token.value == ';') {
                this->nextToken();
                this->tolerateUnexpectedToken(token);
            } else {
                this->tolerateUnexpectedToken(token, Messages.UnexpectedToken);
            }
        } else {
            this->expect(',');
        }*/
        this->expect(PunctuatorsKind::Comma);
    }

    // Expect the next token to match the specified keyword.
    // If not, an exception will be thrown.

    void expectKeyword(KeywordKind keyword)
    {
        PassRefPtr<ScannerResult> token = this->nextToken();
        if (token->type != Token::KeywordToken || token->valueKeywordKind != keyword) {
            this->throwUnexpectedToken(token);
        }
    }

    // Return true if the next token matches the specified punctuator.

    bool match(PunctuatorsKind value)
    {
        return this->lookahead->type == Token::PunctuatorToken && this->lookahead->valuePunctuatorsKind == value;
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
        PunctuatorsKind op = this->lookahead->valuePunctuatorsKind;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        RefPtr<ScannerResult> previousFirstCoverInitializedNameError = this->context->firstCoverInitializedNameError;

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
        if (this->match(PunctuatorsKind::SemiColon)) {
            this->nextToken();
        } else if (!this->hasLineTerminator) {
            if (this->lookahead->type != Token::EOFToken && !this->match(PunctuatorsKind::RightBrace)) {
                this->throwUnexpectedToken(this->lookahead);
            }
            this->lastMarker.index = this->startMarker.index;
            this->lastMarker.lineNumber = this->startMarker.lineNumber;
            this->lastMarker.lineStart = this->startMarker.lineStart;
        }
    }

    IdentifierNode* finishIdentifier(PassRefPtr<ScannerResult> token, bool isScopeVariableName)
    {
        IdentifierNode* ret;
        StringView sv = token->valueStringLiteral();
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            ret = new IdentifierNode(this->escargotContext->staticStrings().asciiTable[firstCh]);
        } else {
            ret = new IdentifierNode(AtomicString(this->escargotContext, SourceStringView(sv)));
        }
        //nodeExtraInfo.insert(std::make_pair(ret, token));
        if (trackUsingNames) {
            scopeContexts.back()->insertUsingName(ret->name());
        }
        return ret;
    }

    ScanExpressionResult finishScanIdentifier(PassRefPtr<ScannerResult> token, bool isScopeVariableName)
    {
        AtomicString name;
        StringView sv = token->valueStringLiteral();
        const auto& a = sv.bufferAccessData();
        char16_t firstCh = a.charAt(0);
        if (a.length == 1 && firstCh < ESCARGOT_ASCII_TABLE_MAX) {
            name = this->escargotContext->staticStrings().asciiTable[firstCh];
        } else {
            name = AtomicString(this->escargotContext, SourceStringView(sv));
        }

        if (trackUsingNames) {
            scopeContexts.back()->insertUsingName(name);
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
            if (this->sourceType == SourceType::Module && this->lookahead->valueKeywordKind == KeywordKind::Await) {
                this->tolerateUnexpectedToken(this->lookahead);
            }
            return this->finalize(node, finishIdentifier(this->nextToken(), true));
            break;
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && this->lookahead->octal) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            {
                PassRefPtr<ScannerResult> token = this->nextToken();
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
            PassRefPtr<ScannerResult> token = this->nextToken();
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
            PassRefPtr<ScannerResult> token = this->nextToken();
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
            PunctuatorsKind value = this->lookahead->valuePunctuatorsKind;
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
                PassRefPtr<ScannerResult> token = this->nextRegexToken();
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
            if (!this->context->strict && this->context->allowYield && this->matchKeyword(KeywordKind::Yield)) {
                return this->parseIdentifierName();
            } else if (!this->context->strict && this->matchKeyword(KeywordKind::Let)) {
                throwUnexpectedToken(this->nextToken());
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(KeywordKind::Function)) {
                    return this->parseFunctionExpression();
                } else if (this->matchKeyword(KeywordKind::This)) {
                    if (this->context->inArrowFunction) {
                        scopeContexts.back()->insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
                    this->nextToken();
                    return this->finalize(node, new ThisExpressionNode());
                } else if (this->matchKeyword(KeywordKind::Class)) {
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
            if (this->sourceType == SourceType::Module && this->lookahead->valueKeywordKind == KeywordKind::Await) {
                this->tolerateUnexpectedToken(this->lookahead);
            }
            return finishScanIdentifier(this->nextToken(), true);
            break;
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && this->lookahead->octal) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
            }
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;
            {
                PassRefPtr<ScannerResult> token = this->nextToken();
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
            PassRefPtr<ScannerResult> token = this->nextToken();
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
            PassRefPtr<ScannerResult> token = this->nextToken();
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
            PunctuatorsKind value = this->lookahead->valuePunctuatorsKind;
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
                PassRefPtr<ScannerResult> token = this->nextRegexToken();
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
            if (!this->context->strict && this->context->allowYield && this->matchKeyword(KeywordKind::Yield)) {
                return this->scanIdentifierName();
            } else if (!this->context->strict && this->matchKeyword(KeywordKind::Let)) {
                throwUnexpectedToken(this->nextToken());
            } else {
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                if (this->matchKeyword(KeywordKind::Function)) {
                    this->parseFunctionExpression();
                    return std::make_pair(ASTNodeType::FunctionExpression, AtomicString());
                } else if (this->matchKeyword(KeywordKind::This)) {
                    if (this->context->inArrowFunction) {
                        scopeContexts.back()->insertUsingName(this->escargotContext->staticStrings().stringThis);
                    }
                    this->nextToken();
                    return std::make_pair(ASTNodeType::ThisExpression, AtomicString());
                } else if (this->matchKeyword(KeywordKind::Class)) {
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
        RefPtr<ScannerResult> stricted;
        const char* message;
        RefPtr<ScannerResult> firstRestricted;
        ParseParameterOptions()
        {
            firstRestricted = nullptr;
            stricted = nullptr;
            message = nullptr;
        }
    };


    void validateParam(ParseParameterOptions& options, const RefPtr<ScannerResult>& param, AtomicString name)
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

    PassRefPtr<RestElementNode> parseRestElement(std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>>& params)
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

    PassRefPtr<Node> parsePattern(std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
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
            if (this->matchKeyword(KeywordKind::Let) && (kind == KeywordKind::Const || kind == KeywordKind::Let)) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::UnexpectedToken);
            }
            params.push_back(this->lookahead);
            return this->parseVariableIdentifier(kind);
        }
    }

    std::pair<ASTNodeType, AtomicString> scanPattern(std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
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
            if (this->matchKeyword(KeywordKind::Let) && (kind == KeywordKind::Const || kind == KeywordKind::Let)) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::UnexpectedToken);
            }
            params.push_back(this->lookahead);
            return std::make_pair(ASTNodeType::Identifier, this->scanVariableIdentifier(kind));
        }
    }

    PassRefPtr<Node> parsePatternWithDefault(std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>>& params, KeywordKind kind = KeywordKindEnd)
    {
        RefPtr<ScannerResult> startToken = this->lookahead;

        PassRefPtr<Node> pattern = this->parsePattern(params, kind);
        if (this->match(PunctuatorsKind::Substitution)) {
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
        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> params;
        RefPtr<ScannerResult> token = this->lookahead;
        if (token->type == Token::PunctuatorToken && token->valuePunctuatorsKind == PunctuatorsKind::PeriodPeriodPeriod) {
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
        return !this->match(PunctuatorsKind::RightParenthesis);
    }

    struct ParseFormalParametersResult {
        PatternNodeVector params;
        RefPtr<ScannerResult> stricted;
        RefPtr<ScannerResult> firstRestricted;
        const char* message;
        bool valid;

        ParseFormalParametersResult()
            : message(nullptr)
            , valid(false)
        {
        }
        ParseFormalParametersResult(PatternNodeVector params, RefPtr<ScannerResult> stricted, RefPtr<ScannerResult> firstRestricted, const char* message)
        {
            this->params = std::move(params);
            this->stricted = stricted;
            this->firstRestricted = firstRestricted;
            this->message = nullptr;
            this->valid = true;
        }
    };

    ParseFormalParametersResult parseFormalParameters(RefPtr<ScannerResult> firstRestricted = nullptr)
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
        this->expect(PunctuatorsKind::PeriodPeriodPeriod);
        MetaNode node = this->createNode();

        RefPtr<Node> arg = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
        this->throwError("Spread element is not supported yet");
        return this->finalize(node, new SpreadElementNode(arg.get()));
    }

    ScanExpressionResult scanSpreadElement()
    {
        this->expect(PunctuatorsKind::PeriodPeriodPeriod);
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
            this->tolerateUnexpectedToken(params.firstRestricted, params.message);
        }
        if (this->context->strict && params.stricted) {
            this->tolerateUnexpectedToken(params.stricted, params.message);
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
        RefPtr<ScannerResult> token = this->nextToken();
        MetaNode node = this->createNode();

        RefPtr<Node> key;
        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && token->octal) {
                this->tolerateUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
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
            if (token->valuePunctuatorsKind == LeftSquareBracket) {
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
        RefPtr<ScannerResult> token = this->nextToken();
        ScanExpressionResult key(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        String* keyString = String::emptyString;
        switch (token->type) {
        case Token::NumericLiteralToken:
        case Token::StringLiteralToken:
            if (this->context->strict && token->octal) {
                this->tolerateUnexpectedToken(token, Messages::StrictOctalLiteral);
            }
            if (this->context->strict && this->lookahead->startWithZero) {
                this->tolerateUnexpectedToken(this->lookahead, Messages::StrictLeadingZeroLiteral);
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
            if (token->valuePunctuatorsKind == LeftSquareBracket) {
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

    bool qualifiedPropertyName(RefPtr<ScannerResult> token)
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
            return token->valuePunctuatorsKind == LeftSquareBracket;
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
        RefPtr<ScannerResult> token = this->lookahead;

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
        } else if (this->match(PunctuatorsKind::Multiply)) {
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

        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorsKind == PunctuatorsKind::Multiply && lookaheadPropertyKey) {
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
            if (this->match(PunctuatorsKind::Colon)) {
                if (!computed && this->isPropertyKey(key.get(), "__proto__")) {
                    if (hasProto) {
                        this->tolerateError(Messages::DuplicateProtoProperty);
                    }
                    hasProto = true;
                }
                this->nextToken();
                value = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);

                /* TODO(ES6) this part is only for es6
            } else if (this->match(LeftParenthesis)) {
                value = this->parsePropertyMethodFunction();
                method = true;

            } else if (token->type == Token::IdentifierToken) {
                Node* id = this->finalize(node, finishIdentifier(token, true));
                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    Node* init = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    value = this->finalize(node, new AssignmentExpressionSimpleNode(id, init));
                } else {
                    shorthand = true;
                    value = id;
                }
            */
            } else {
                this->throwUnexpectedToken(this->nextToken());
            }
        }

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

        // return this->finalize(node, new PropertyNode(kind, key, computed, value, method, shorthand));
        return this->finalize(node, new PropertyNode(key.get(), value.get(), kind, computed));
    }

    void scanObjectProperty(bool& hasProto, std::vector<std::pair<AtomicString, size_t>>& usedNames) //: Node.Property
    {
        RefPtr<ScannerResult> token = this->lookahead;

        PropertyNode::Kind kind;
        ScanExpressionResult key(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        String* keyString = String::emptyString;

        bool computed = false;
        bool method = false;
        bool shorthand = false;

        if (token->type == Token::IdentifierToken) {
            this->nextToken();
            key = finishScanIdentifier(token, true);
        } else if (this->match(PunctuatorsKind::Multiply)) {
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

        } else if (token->type == Token::PunctuatorToken && token->valuePunctuatorsKind == PunctuatorsKind::Multiply && lookaheadPropertyKey) {
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
            if (this->match(PunctuatorsKind::Colon)) {
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

                /* TODO(ES6) this part is only for es6
            } else if (this->match(LeftParenthesis)) {
                value = this->parsePropertyMethodFunction();
                method = true;

            } else if (token->type == Token::IdentifierToken) {
                Node* id = this->finalize(node, finishIdentifier(token, true));
                if (this->match(Substitution)) {
                    this->context->firstCoverInitializedNameError = this->lookahead;
                    this->nextToken();
                    shorthand = true;
                    Node* init = this->isolateCoverGrammar(&Parser::parseAssignmentExpression);
                    value = this->finalize(node, new AssignmentExpressionSimpleNode(id, init));
                } else {
                    shorthand = true;
                    value = id;
                }
            */
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

        RefPtr<ScannerResult> token = this->nextToken();
        MetaNode node = this->createNode();
        TemplateElement* tm = new TemplateElement();
        tm->value = token->valueTemplate->valueCooked;
        tm->raw = token->valueTemplate->raw;
        tm->tail = token->valueTemplate->tail;
        return tm;
    }

    TemplateElement* parseTemplateElement()
    {
        if (this->lookahead->type != Token::TemplateToken) {
            this->throwUnexpectedToken(this->lookahead);
        }

        RefPtr<ScannerResult> token = this->nextToken();
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
            RefPtr<ScannerResult> startToken = this->lookahead;

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
            RefPtr<ScannerResult> startToken = this->lookahead;

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

    bool isIdentifierName(RefPtr<ScannerResult> token)
    {
        return token->type == Token::IdentifierToken || token->type == Token::KeywordToken || token->type == Token::BooleanLiteralToken || token->type == Token::NullLiteralToken;
    }

    PassRefPtr<IdentifierNode> parseIdentifierName()
    {
        MetaNode node = this->createNode();
        RefPtr<ScannerResult> token = this->nextToken();
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(token);
        }
        return this->finalize(node, finishIdentifier(token, true));
    }

    ScanExpressionResult scanIdentifierName()
    {
        RefPtr<ScannerResult> token = this->nextToken();
        if (!this->isIdentifierName(token)) {
            this->throwUnexpectedToken(token);
        }

        return finishScanIdentifier(token, false);
    }

    PassRefPtr<Node> parseNewExpression()
    {
        MetaNode node = this->createNode();

        RefPtr<IdentifierNode> id = this->parseIdentifierName();
        // assert(id.name === 'new', 'New expression must start with `new`');

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
        MetaNode node = this->createNode();

        auto id = this->scanIdentifierName();
        // assert(id.name === 'new', 'New expression must start with `new`');

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
        RefPtr<ScannerResult> startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        RefPtr<Node> expr;
        if (this->context->inFunctionBody && this->matchKeyword(Super)) {
            MetaNode node = this->createNode();
            this->nextToken();
            this->throwError("super keyword is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // expr = this->finalize(node, new Node.Super());
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(this->lookahead);
            }
        } else {
            expr = this->inheritCoverGrammar(this->matchKeyword(New) ? &Parser::parseNewExpression : &Parser::parsePrimaryExpression);
        }

        while (true) {
            bool isPunctuatorTokenLookahead = this->lookahead->type == Token::PunctuatorToken;
            if (isPunctuatorTokenLookahead) {
                if (this->lookahead->valuePunctuatorsKind == Period) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    bool trackUsingNamesBefore = this->trackUsingNames;
                    this->trackUsingNames = false;
                    PassRefPtr<IdentifierNode> property = this->parseIdentifierName();
                    this->trackUsingNames = trackUsingNamesBefore;
                    expr = this->finalize(this->startNode(startToken), new MemberExpressionNode(expr.get(), property.get(), true));
                } else if (this->lookahead->valuePunctuatorsKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    expr = this->finalize(this->startNode(startToken), new CallExpressionNode(expr.get(), this->parseArguments()));
                } else if (this->lookahead->valuePunctuatorsKind == LeftSquareBracket) {
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
        RefPtr<ScannerResult> startToken = this->lookahead;
        bool previousAllowIn = this->context->allowIn;
        this->context->allowIn = true;

        ScanExpressionResult expr(std::make_pair(ASTNodeType::ASTNodeTypeError, AtomicString()));
        if (this->context->inFunctionBody && this->matchKeyword(Super)) {
            MetaNode node = this->createNode();
            this->nextToken();
            this->throwError("super keyword is not supported yet");
            RELEASE_ASSERT_NOT_REACHED();
            // expr = this->finalize(node, new Node.Super());
            if (!this->match(LeftParenthesis) && !this->match(Period) && !this->match(LeftSquareBracket)) {
                this->throwUnexpectedToken(this->lookahead);
            }
        } else {
            expr = this->scanInheritCoverGrammar(this->matchKeyword(New) ? &Parser::scanNewExpression : &Parser::scanPrimaryExpression);
        }

        while (true) {
            bool isPunctuatorTokenLookahead = this->lookahead->type == Token::PunctuatorToken;
            if (isPunctuatorTokenLookahead) {
                if (this->lookahead->valuePunctuatorsKind == Period) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = true;
                    this->nextToken();
                    bool trackUsingNamesBefore = this->trackUsingNames;
                    this->trackUsingNames = false;
                    ScanExpressionResult property = this->scanIdentifierName();
                    this->trackUsingNames = trackUsingNamesBefore;
                    expr.first = ASTNodeType::MemberExpression;
                } else if (this->lookahead->valuePunctuatorsKind == LeftParenthesis) {
                    this->context->isBindingElement = false;
                    this->context->isAssignmentTarget = false;
                    testCalleeExpressionInScan(expr);
                    this->scanArguments();
                    expr.first = ASTNodeType::CallExpression;
                } else if (this->lookahead->valuePunctuatorsKind == LeftSquareBracket) {
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
                scopeContexts.back()->insertUsingName(this->escargotContext->staticStrings().stringThis);
            }
        }
    }

    PassRefPtr<Node> parseSuper()
    {
        MetaNode node = this->createNode();

        this->expectKeyword(Super);
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

        this->expectKeyword(Super);
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
        RefPtr<Node> expr = (this->matchKeyword(Super) && this->context->inFunctionBody) ? this->parseSuper() : this->inheritCoverGrammar(this->matchKeyword(New) ? &Parser::parseNewExpression : &Parser::parsePrimaryExpression);

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
        ScanExpressionResult expr = (this->matchKeyword(Super) && this->context->inFunctionBody) ? this->scanSuper() : this->scanInheritCoverGrammar(this->matchKeyword(New) ? &Parser::scanNewExpression : &Parser::scanPrimaryExpression);

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
        RefPtr<ScannerResult> startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            MetaNode node = this->startNode(startToken);
            RefPtr<ScannerResult> token = this->nextToken();
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
        RefPtr<ScannerResult> startToken = this->lookahead;

        if (this->match(PlusPlus) || this->match(MinusMinus)) {
            bool isPlus = this->match(PlusPlus);
            MetaNode node = this->startNode(startToken);
            RefPtr<ScannerResult> token = this->nextToken();
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
            auto punctuatorsKind = this->lookahead->valuePunctuatorsKind;
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
            if (this->lookahead->valueKeywordKind == Delete) {
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
            } else if (this->lookahead->valueKeywordKind == Void) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->inheritCoverGrammar(&Parser::parseUnaryExpression);
                auto expr(this->finalize(node, new UnaryExpressionVoidNode(subExpr.get())));
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (this->lookahead->valueKeywordKind == Typeof) {
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
            auto punctuatorsKind = this->lookahead->valuePunctuatorsKind;
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
            if (this->lookahead->valueKeywordKind == Delete) {
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
            } else if (this->lookahead->valueKeywordKind == Void) {
                MetaNode node = this->startNode(this->lookahead);
                this->nextToken();
                auto subExpr = this->scanInheritCoverGrammar(&Parser::scanUnaryExpression);
                auto expr = ScanExpressionResult(ASTNodeType::UnaryExpressionVoid, AtomicString());
                this->context->isAssignmentTarget = false;
                this->context->isBindingElement = false;
                return expr;
            } else if (this->lookahead->valueKeywordKind == Typeof) {
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
        RefPtr<ScannerResult> startToken = this->lookahead;
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

    int binaryPrecedence(RefPtr<ScannerResult> token)
    {
        if (token->type == Token::PunctuatorToken) {
            if (token->valuePunctuatorsKind == Substitution) {
                return 0;
            } else if (token->valuePunctuatorsKind == LogicalOr) {
                return 1;
            } else if (token->valuePunctuatorsKind == LogicalAnd) {
                return 2;
            } else if (token->valuePunctuatorsKind == BitwiseOr) {
                return 3;
            } else if (token->valuePunctuatorsKind == BitwiseXor) {
                return 4;
            } else if (token->valuePunctuatorsKind == BitwiseAnd) {
                return 5;
            } else if (token->valuePunctuatorsKind == Equal) {
                return 6;
            } else if (token->valuePunctuatorsKind == NotEqual) {
                return 6;
            } else if (token->valuePunctuatorsKind == StrictEqual) {
                return 6;
            } else if (token->valuePunctuatorsKind == NotStrictEqual) {
                return 6;
            } else if (token->valuePunctuatorsKind == RightInequality) {
                return 7;
            } else if (token->valuePunctuatorsKind == LeftInequality) {
                return 7;
            } else if (token->valuePunctuatorsKind == RightInequalityEqual) {
                return 7;
            } else if (token->valuePunctuatorsKind == LeftInequalityEqual) {
                return 7;
            } else if (token->valuePunctuatorsKind == LeftShift) {
                return 8;
            } else if (token->valuePunctuatorsKind == RightShift) {
                return 8;
            } else if (token->valuePunctuatorsKind == UnsignedRightShift) {
                return 8;
            } else if (token->valuePunctuatorsKind == Plus) {
                return 9;
            } else if (token->valuePunctuatorsKind == Minus) {
                return 9;
            } else if (token->valuePunctuatorsKind == Multiply) {
                return 11;
            } else if (token->valuePunctuatorsKind == Divide) {
                return 11;
            } else if (token->valuePunctuatorsKind == Mod) {
                return 11;
            }
            return 0;
        } else if (token->type == Token::KeywordToken) {
            if (token->valueKeywordKind == In) {
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
        RefPtr<ScannerResult> startToken = this->lookahead;

        RefPtr<Node> expr = this->inheritCoverGrammar(&Parser::parseExponentiationExpression);

        RefPtr<ScannerResult> token = this->lookahead;
        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> tokenKeeper;
        int prec = this->binaryPrecedence(token);
        if (prec > 0) {
            this->nextToken();

            token->prec = prec;
            this->context->isAssignmentTarget = false;
            this->context->isBindingElement = false;

            std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> markers;
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
                while ((stack.size() > 2) && (prec <= ((ScannerResult*)stack[stack.size() - 2])->prec)) {
                    right = (Node*)stack.back();
                    right->deref();
                    stack.pop_back();
                    RefPtr<ScannerResult> operator_ = (ScannerResult*)stack.back();
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
                expr = this->finalize(node, finishBinaryExpression((Node*)stack[i - 2], expr.get(), (ScannerResult*)stack[i - 1]));
                ((Node*)stack[i - 2])->deref();
                ((ScannerResult*)stack[i - 1])->deref();
                i -= 2;
            }
            RELEASE_ASSERT(i == 0);
        }

        return expr.release();
    }

    ScanExpressionResult scanBinaryExpressions()
    {
        ScanExpressionResult expr = this->scanInheritCoverGrammar(&Parser::scanExponentiationExpression);

        RefPtr<ScannerResult> token = this->lookahead;
        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> tokenKeeper;
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
                while ((stack.size() > 2) && (prec <= ((ScannerResult*)stack[stack.size() - 2])->prec)) {
                    right = *((ScanExpressionResult*)stack.back());
                    delete (ScanExpressionResult*)stack.back();
                    stack.pop_back();
                    RefPtr<ScannerResult> operator_ = (ScannerResult*)stack.back();
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
                expr = scanBinaryExpression(*((ScanExpressionResult*)stack[i - 2]), expr, (ScannerResult*)stack[i - 1]);
                delete (ScanExpressionResult*)stack[i - 2];
                ((ScannerResult*)stack[i - 1])->deref();
                i -= 2;
            }
            RELEASE_ASSERT(i == 0);
        }

        return expr;
    }


    Node* finishBinaryExpression(Node* left, Node* right, ScannerResult* token)
    {
        Node* nd;
        if (token->type == Token::PunctuatorToken) {
            PunctuatorsKind oper = token->valuePunctuatorsKind;
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
            if (token->valueKeywordKind == KeywordKind::In) {
                nd = new BinaryExpressionInNode(left, right);
            } else if (token->valueKeywordKind == KeywordKind::InstanceofKeyword) {
                nd = new BinaryExpressionInstanceOfNode(left, right);
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        return nd;
    }

    ScanExpressionResult scanBinaryExpression(ScanExpressionResult left, ScanExpressionResult right, ScannerResult* token)
    {
        ScanExpressionResult nd(ASTNodeType::ASTNodeTypeError, AtomicString());
        if (token->type == Token::PunctuatorToken) {
            PunctuatorsKind oper = token->valuePunctuatorsKind;
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
            if (token->valueKeywordKind == KeywordKind::In) {
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
        RefPtr<ScannerResult> startToken = this->lookahead;

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
            RefPtr<ScannerResult> token = this->context->strict ? options.stricted : options.firstRestricted;
            this->throwUnexpectedToken(token, options.message);
        }

        return ParseFormalParametersResult(params, options.stricted, options.firstRestricted, options.message);
    }

    PassRefPtr<Node> parseAssignmentExpression()
    {
        RefPtr<Node> expr;

        if (!this->context->allowYield && this->matchKeyword(Yield)) {
            expr = this->parseYieldExpression();
        } else {
            RefPtr<ScannerResult> startToken = this->lookahead;
            RefPtr<ScannerResult> token = startToken;
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
                        this->tolerateUnexpectedToken(this->lookahead);
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
                        this->tolerateUnexpectedToken(list.stricted, list.message);
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
                            this->tolerateUnexpectedToken(token, Messages::StrictLHSAssignment);
                        }
                        if (this->scanner->isStrictModeReservedWord(id->name())) {
                            this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
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
                    if (token->valuePunctuatorsKind == Substitution) {
                        exprResult = new AssignmentExpressionSimpleNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == PlusEqual) {
                        exprResult = new AssignmentExpressionPlusNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == MinusEqual) {
                        exprResult = new AssignmentExpressionMinusNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == MultiplyEqual) {
                        exprResult = new AssignmentExpressionMultiplyNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == DivideEqual) {
                        exprResult = new AssignmentExpressionDivisionNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == ModEqual) {
                        exprResult = new AssignmentExpressionModNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == LeftShiftEqual) {
                        exprResult = new AssignmentExpressionLeftShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == RightShiftEqual) {
                        exprResult = new AssignmentExpressionSignedRightShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == UnsignedRightShiftEqual) {
                        exprResult = new AssignmentExpressionUnsignedShiftNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == BitwiseXorEqual) {
                        exprResult = new AssignmentExpressionBitwiseXorNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == BitwiseAndEqual) {
                        exprResult = new AssignmentExpressionBitwiseAndNode(expr.get(), right.get());
                    } else if (token->valuePunctuatorsKind == BitwiseOrEqual) {
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

        if (!this->context->allowYield && this->matchKeyword(Yield)) {
            expr = this->scanYieldExpression();
        } else {
            RefPtr<ScannerResult> startToken = this->lookahead;
            RefPtr<ScannerResult> token = startToken;
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
                        this->tolerateUnexpectedToken(this->lookahead);
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
                        this->tolerateUnexpectedToken(list.stricted, list.message);
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
                            this->tolerateUnexpectedToken(token, Messages::StrictLHSAssignment);
                        }
                        if (this->scanner->isStrictModeReservedWord(expr.second)) {
                            this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
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
                    if (token->valuePunctuatorsKind == Substitution) {
                        exprResult.first = ASTNodeType::AssignmentExpressionSimple;
                    } else if (token->valuePunctuatorsKind == PlusEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionPlus;
                    } else if (token->valuePunctuatorsKind == MinusEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMinus;
                    } else if (token->valuePunctuatorsKind == MultiplyEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMultiply;
                    } else if (token->valuePunctuatorsKind == DivideEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionDivision;
                    } else if (token->valuePunctuatorsKind == ModEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionMod;
                    } else if (token->valuePunctuatorsKind == LeftShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionLeftShift;
                    } else if (token->valuePunctuatorsKind == RightShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionSignedRightShift;
                    } else if (token->valuePunctuatorsKind == UnsignedRightShiftEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionUnsignedRightShift;
                    } else if (token->valuePunctuatorsKind == BitwiseXorEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionBitwiseXor;
                    } else if (token->valuePunctuatorsKind == BitwiseAndEqual) {
                        exprResult.first = ASTNodeType::AssignmentExpressionBitwiseAnd;
                    } else if (token->valuePunctuatorsKind == BitwiseOrEqual) {
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
        RefPtr<ScannerResult> startToken = this->lookahead;
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
            case Function:
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
                         this->tolerateUnexpectedToken(this->lookahead, Messages.IllegalExportDeclaration);
                     }
                     statement = this->parseExportDeclaration();
                     break;
                 case 'import':
                     if (this->sourceType !== 'module') {
                         this->tolerateUnexpectedToken(this->lookahead, Messages.IllegalImportDeclaration);
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
            case Function:
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
                         this->tolerateUnexpectedToken(this->lookahead, Messages.IllegalExportDeclaration);
                     }
                     statement = this->parseExportDeclaration();
                     break;
                 case 'import':
                     if (this->sourceType !== 'module') {
                         this->tolerateUnexpectedToken(this->lookahead, Messages.IllegalImportDeclaration);
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
                this->tolerateUnexpectedToken(this->lookahead, Messages.UnexpectedToken);
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

        RefPtr<ScannerResult> token = this->nextToken();
        if (token->type == Token::KeywordToken && token->valueKeywordKind == Yield) {
            if (this->context->strict) {
                this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
            }
            if (!this->context->allowYield) {
                this->throwUnexpectedToken(token);
            }
        } else if (token->type != Token::IdentifierToken) {
            if (this->context->strict && token->type == Token::KeywordToken && this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
            } else {
                if (this->context->strict || token->relatedSource() != "let" || (kind != KeywordKind::Var)) {
                    this->throwUnexpectedToken(token);
                }
            }
        } else if (this->sourceType == Module && token->type == Token::IdentifierToken && token->relatedSource() == "await") {
            this->tolerateUnexpectedToken(token);
        }

        return this->finalize(node, finishIdentifier(token, true));
    }

    AtomicString scanVariableIdentifier(KeywordKind kind = KeywordKindEnd)
    {
        RefPtr<ScannerResult> token = this->nextToken();
        if (token->type == Token::KeywordToken && token->valueKeywordKind == Yield) {
            if (this->context->strict) {
                this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
            }
            if (!this->context->allowYield) {
                this->throwUnexpectedToken(token);
            }
        } else if (token->type != Token::IdentifierToken) {
            if (this->context->strict && token->type == Token::KeywordToken && this->scanner->isStrictModeReservedWord(token->relatedSource())) {
                this->tolerateUnexpectedToken(token, Messages::StrictReservedWord);
            } else {
                if (this->context->strict || token->relatedSource() != "let" || (kind != KeywordKind::Var)) {
                    this->throwUnexpectedToken(token);
                }
            }
        } else if (this->sourceType == Module && token->type == Token::IdentifierToken && token->relatedSource() == "await") {
            this->tolerateUnexpectedToken(token);
        }

        return finishScanIdentifier(token, true).second;
    }

    struct DeclarationOptions {
        bool inFor;
    };

    PassRefPtr<VariableDeclaratorNode> parseVariableDeclaration(DeclarationOptions& options)
    {
        MetaNode node = this->createNode();

        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> params;
        RefPtr<Node> id = this->parsePattern(params, KeywordKind::Var);

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
        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> params;
        std::pair<ASTNodeType, AtomicString> id = this->scanPattern(params, KeywordKind::Var);

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
        this->expectKeyword(Var);
        MetaNode node = this->createNode();
        DeclarationOptions opt;
        opt.inFor = false;
        VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
        this->consumeSemicolon();

        return this->finalize(node, new VariableDeclarationNode(std::move(declarations) /*, 'var'*/));
    }

    void scanVariableStatement()
    {
        this->expectKeyword(Var);
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

        this->expectKeyword(If);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> test = this->parseExpression();

        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
            consequent = this->finalize(this->createNode(), new EmptyStatementNode());
        } else {
            this->expect(RightParenthesis);
            consequent = this->parseStatement(this->context->strict ? false : true);
            if (this->matchKeyword(Else)) {
                this->nextToken();
                alternate = this->parseStatement();
            }
        }

        return this->finalize(node, new IfStatementNode(test.get(), consequent.get(), alternate.get()));
    }

    void scanIfStatement()
    {
        this->expectKeyword(If);
        this->expect(LeftParenthesis);
        this->scanExpression();

        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
        } else {
            this->expect(RightParenthesis);
            this->scanStatement(this->context->strict ? false : true);
            if (this->matchKeyword(Else)) {
                this->nextToken();
                this->scanStatement();
            }
        }
    }

    // ECMA-262 13.7.2 The do-while Statement

    PassRefPtr<DoWhileStatementNode> parseDoWhileStatement()
    {
        this->expectKeyword(Do);
        MetaNode node = this->createNode();

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        RefPtr<Node> body = this->parseStatement(false);
        this->context->inIteration = previousInIteration;

        this->expectKeyword(While);
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
        this->expectKeyword(Do);
        MetaNode node = this->createNode();

        bool previousInIteration = this->context->inIteration;
        this->context->inIteration = true;
        this->scanStatement(false);
        this->context->inIteration = previousInIteration;

        this->expectKeyword(While);
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

        this->expectKeyword(While);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        RefPtr<Node> test = this->parseExpression();

        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
            body = this->finalize(this->createNode(), new EmptyStatementNode());
        } else {
            this->expect(RightParenthesis);

            bool previousInIteration = this->context->inIteration;
            this->context->inIteration = true;
            body = this->parseStatement(false);
            this->context->inIteration = previousInIteration;
        }

        this->context->inLoop = prevInLoop;

        return this->finalize(node, new WhileStatementNode(test.get(), body.get()));
    }

    void scanWhileStatement()
    {
        bool prevInLoop = this->context->inLoop;
        this->context->inLoop = true;

        this->expectKeyword(While);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);
        this->scanExpression();

        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
        } else {
            this->expect(RightParenthesis);
            bool previousInIteration = this->context->inIteration;
            this->context->inIteration = true;
            this->scanStatement(false);
            this->context->inIteration = previousInIteration;
        }

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

        this->expectKeyword(For);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(Var)) {
                MetaNode metaInit = this->createNode();
                this->nextToken();

                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                DeclarationOptions opt;
                opt.inFor = true;
                VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
                this->context->allowIn = previousAllowIn;

                if (declarations.size() == 1 && this->matchKeyword(In)) {
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
            } else if (this->matchKeyword(Const) || this->matchKeyword(Let)) {
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
                RefPtr<ScannerResult> initStartToken = this->lookahead;
                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                init = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(In)) {
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
        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
            body = this->finalize(this->createNode(), new EmptyStatementNode());
        } else {
            this->expect(RightParenthesis);

            bool previousInIteration = this->context->inIteration;
            this->context->inIteration = true;
            auto functor = std::bind(&Parser::parseStatement, std::ref(*this), false);
            body = this->isolateCoverGrammarWithFunctor(functor);
            this->context->inIteration = previousInIteration;
        }

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

        this->expectKeyword(For);
        MetaNode node = this->createNode();
        this->expect(LeftParenthesis);

        if (this->match(SemiColon)) {
            this->nextToken();
        } else {
            if (this->matchKeyword(Var)) {
                MetaNode metaInit = this->createNode();
                this->nextToken();

                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                DeclarationOptions opt;
                opt.inFor = true;
                VariableDeclaratorVector declarations = this->parseVariableDeclarationList(opt);
                this->context->allowIn = previousAllowIn;

                if (declarations.size() == 1 && this->matchKeyword(In)) {
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
            } else if (this->matchKeyword(Const) || this->matchKeyword(Let)) {
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
                RefPtr<ScannerResult> initStartToken = this->lookahead;
                bool previousAllowIn = this->context->allowIn;
                this->context->allowIn = false;
                init = this->inheritCoverGrammar(&Parser::parseAssignmentExpression);
                this->context->allowIn = previousAllowIn;

                if (this->matchKeyword(In)) {
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

        if (!this->match(RightParenthesis) && this->config.tolerant) {
            this->tolerateUnexpectedToken(this->nextToken());
        } else {
            this->expect(RightParenthesis);

            bool previousInIteration = this->context->inIteration;
            this->context->inIteration = true;
            auto functor = std::bind(&Parser::scanStatement, std::ref(*this), false);
            this->scanIsolateCoverGrammarWithFunctor(functor);
            this->context->inIteration = previousInIteration;
        }

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
        this->expectKeyword(Continue);
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
        this->expectKeyword(Continue);

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
        this->expectKeyword(Break);
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
        this->expectKeyword(Break);

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

        this->expectKeyword(Return);
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

        this->expectKeyword(Return);

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

        this->expectKeyword(With);
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
        if (this->matchKeyword(Default)) {
            node = this->createNode();
            this->nextToken();
            test = nullptr;
        } else {
            this->expectKeyword(Case);
            node = this->createNode();
            test = this->parseExpression();
        }
        this->expect(Colon);

        RefPtr<StatementContainer> consequent = StatementContainer::create();
        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(Default) || this->matchKeyword(Case)) {
                break;
            }
            consequent->appendChild(this->parseStatementListItem());
        }

        return this->finalize(node, new SwitchCaseNode(test.get(), consequent.get()));
    }

    bool scanSwitchCase()
    {
        bool isDefaultNode;
        if (this->matchKeyword(Default)) {
            this->nextToken();
            isDefaultNode = true;
        } else {
            this->expectKeyword(Case);
            this->scanExpression();
            isDefaultNode = false;
        }
        this->expect(Colon);

        while (true) {
            if (this->match(RightBrace) || this->matchKeyword(Default) || this->matchKeyword(Case)) {
                break;
            }
            this->scanStatementListItem();
        }
        return isDefaultNode;
    }

    PassRefPtr<SwitchStatementNode> parseSwitchStatement()
    {
        this->expectKeyword(Switch);
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
        this->expectKeyword(Switch);

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
        this->expectKeyword(Throw);

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
        this->expectKeyword(Throw);

        if (this->hasLineTerminator) {
            this->throwError(Messages::NewlineAfterThrow);
        }

        this->scanExpression();
        this->consumeSemicolon();
    }

    // ECMA-262 13.15 The try statement

    PassRefPtr<CatchClauseNode> parseCatchClause()
    {
        this->expectKeyword(Catch);

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

        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> params;
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
        this->expectKeyword(Catch);

        this->expect(LeftParenthesis);
        if (this->match(RightParenthesis)) {
            this->throwUnexpectedToken(this->lookahead);
        }

        bool prevInCatch = this->context->inCatch;
        this->context->inCatch = true;

        std::vector<FunctionDeclarationNode*> vecBefore = std::move(this->context->functionDeclarationsInDirectCatchScope);
        bool prevInDirectCatchScope = this->context->inDirectCatchScope;
        this->context->inDirectCatchScope = true;

        std::vector<RefPtr<ScannerResult>, GCUtil::gc_malloc_ignore_off_page_allocator<RefPtr<ScannerResult>>> params;
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
        this->expectKeyword(Finally);
        return this->parseBlock();
    }

    void scanFinallyClause()
    {
        this->expectKeyword(Finally);
        this->scanBlock();
    }

    PassRefPtr<TryStatementNode> parseTryStatement()
    {
        this->expectKeyword(Try);
        MetaNode node = this->createNode();

        RefPtr<BlockStatementNode> block = this->parseBlock();
        RefPtr<CatchClauseNode> handler = this->matchKeyword(Catch) ? this->parseCatchClause() : nullptr;
        RefPtr<BlockStatementNode> finalizer = this->matchKeyword(Finally) ? this->parseFinallyClause() : nullptr;

        if (!handler && !finalizer) {
            this->throwError(Messages::NoCatchOrFinally);
        }

        return this->finalize(node, new TryStatementNode(block.get(), handler.get(), CatchClauseNodeVector(), finalizer.get()));
    }

    void scanTryStatement()
    {
        this->expectKeyword(Try);

        this->scanBlock();
        bool meetHandler = this->matchKeyword(Catch) ? (this->scanCatchClause(), true) : false;
        bool meetFinalizer = this->matchKeyword(Finally) ? (this->scanFinallyClause(), true) : false;

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
            PunctuatorsKind value = this->lookahead->valuePunctuatorsKind;
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
            case Break:
                statement = asStatementNode(this->parseBreakStatement());
                break;
            case Continue:
                statement = asStatementNode(this->parseContinueStatement());
                break;
            case Debugger:
                statement = asStatementNode(this->parseDebuggerStatement());
                break;
            case Do:
                statement = asStatementNode(this->parseDoWhileStatement());
                break;
            case For:
                statement = asStatementNode(this->parseForStatement());
                break;
            case Function: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                statement = asStatementNode(this->parseFunctionDeclaration());
                break;
            }
            case If:
                statement = asStatementNode(this->parseIfStatement());
                break;
            case Return:
                statement = asStatementNode(this->parseReturnStatement());
                break;
            case Switch:
                statement = asStatementNode(this->parseSwitchStatement());
                break;
            case Throw:
                statement = asStatementNode(this->parseThrowStatement());
                break;
            case Try:
                statement = asStatementNode(this->parseTryStatement());
                break;
            case Var:
                statement = asStatementNode(this->parseVariableStatement());
                break;
            case While:
                statement = asStatementNode(this->parseWhileStatement());
                break;
            case With:
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
            PunctuatorsKind value = this->lookahead->valuePunctuatorsKind;
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
            case Break:
                this->scanBreakStatement();
                break;
            case Continue:
                this->scanContinueStatement();
                break;
            case Debugger:
                this->parseDebuggerStatement();
                break;
            case Do:
                this->scanDoWhileStatement();
                break;
            case For:
                this->scanForStatement();
                break;
            case Function: {
                if (!allowFunctionDeclaration) {
                    this->throwUnexpectedToken(this->lookahead);
                }
                this->parseFunctionDeclaration();
                break;
            }
            case If:
                this->scanIfStatement();
                break;
            case Return:
                this->scanReturnStatement();
                break;
            case Switch:
                this->scanSwitchStatement();
                break;
            case Throw:
                this->scanThrowStatement();
                break;
            case Try:
                this->scanTryStatement();
                break;
            case Var:
                this->scanVariableStatement();
                break;
            case While:
                this->scanWhileStatement();
                break;
            case With:
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
            this->tolerateUnexpectedToken(list.stricted, list.message);
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
                this->lookahead->valuePunctuatorsKind = PunctuatorsKind::RightBrace;
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
        this->expectKeyword(Function);
        MetaNode node = this->createNode();

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        const char* message = nullptr;
        RefPtr<IdentifierNode> id;
        RefPtr<ScannerResult> firstRestricted = nullptr;

        if (!identifierIsOptional || !this->match(LeftParenthesis)) {
            RefPtr<ScannerResult> token = this->lookahead;
            id = this->parseVariableIdentifier();
            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    this->tolerateUnexpectedToken(token, Messages::StrictFunctionName);
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
        RefPtr<ScannerResult> stricted = formalParameters.stricted;
        firstRestricted = formalParameters.firstRestricted;
        if (formalParameters.message) {
            message = formalParameters.message;
        }

        scopeContexts.back()->insertName(id->name(), true);
        scopeContexts.back()->insertUsingName(id->name());
        pushScopeContext(params, id->name());
        extractNamesFromFunctionParams(params);

        bool previousStrict = this->context->strict;
        RefPtr<Node> body = this->parseFunctionSourceElements();
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->tolerateUnexpectedToken(stricted, message);
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
        this->expectKeyword(Function);
        MetaNode node = this->createNode();

        bool isGenerator = this->match(Multiply);
        if (isGenerator) {
            this->nextToken();
        }

        const char* message = nullptr;
        RefPtr<IdentifierNode> id;
        RefPtr<ScannerResult> firstRestricted = nullptr;

        bool previousAllowYield = this->context->allowYield;
        bool previousInArrowFunction = this->context->inArrowFunction;
        this->context->allowYield = !isGenerator;
        this->context->inArrowFunction = false;

        if (!this->match(LeftParenthesis)) {
            RefPtr<ScannerResult> token = this->lookahead;
            id = (!this->context->strict && !isGenerator && this->matchKeyword(Yield)) ? this->parseIdentifierName() : this->parseVariableIdentifier();
            if (this->context->strict) {
                if (this->scanner->isRestrictedWord(token->relatedSource())) {
                    this->tolerateUnexpectedToken(token, Messages::StrictFunctionName);
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
        RefPtr<ScannerResult> stricted = formalParameters.stricted;
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
            scopeContexts.back()->insertUsingName(fnName);
        }

        extractNamesFromFunctionParams(params);
        bool previousStrict = this->context->strict;
        RefPtr<Node> body = this->parseFunctionSourceElements();
        if (this->context->strict && firstRestricted) {
            this->throwUnexpectedToken(firstRestricted, message);
        }
        if (this->context->strict && stricted) {
            this->tolerateUnexpectedToken(stricted, message);
        }
        this->context->strict = previousStrict;
        this->context->allowYield = previousAllowYield;
        this->context->inArrowFunction = previousInArrowFunction;

        return this->finalize(node, new FunctionExpressionNode(fnName, std::move(params), body.get(), popScopeContext(node), isGenerator));
    }

    // ECMA-262 14.1.1 Directive Prologues

    PassRefPtr<Node> parseDirective()
    {
        RefPtr<ScannerResult> token = this->lookahead;
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
        RefPtr<ScannerResult> firstRestricted = nullptr;

        this->context->inParsingDirective = true;
        RefPtr<StatementContainer> body = StatementContainer::create();
        while (true) {
            RefPtr<ScannerResult> token = this->lookahead;
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
                    this->tolerateUnexpectedToken(firstRestricted, Messages::StrictOctalLiteral);
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
            this->tolerateUnexpectedToken(this->lookahead);
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
        scopeContexts.push_back(new ASTScopeContext(this->context->strict));
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
    auto scopeCtx = new ASTScopeContext(codeBlock->isStrict());
    parser.scopeContexts.pushBack(scopeCtx);
    RefPtr<Node> nd;
    if (codeBlock->isArrowFunctionExpression()) {
        nd = parser.parseArrowFunctionSourceElements();
    } else {
        nd = parser.parseFunctionSourceElements();
    }
    return std::make_tuple(nd, scopeCtx);
}

char g_asciiRangeCharMap[128] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    ESPRIMA_IS_WHITESPACE,
    ESPRIMA_IS_LINE_TERMINATOR,
    ESPRIMA_IS_WHITESPACE,
    ESPRIMA_IS_WHITESPACE,
    ESPRIMA_IS_LINE_TERMINATOR,
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
    ESPRIMA_IS_WHITESPACE,
    0,
    0,
    0,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
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
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    ESPRIMA_IS_IDENT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    0,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    0,
    0,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    0,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    ESPRIMA_START_IDENT | ESPRIMA_IS_IDENT,
    0,
    0,
    0,
    0,
    0
};

} // namespace esprima
} // namespace Escargot
