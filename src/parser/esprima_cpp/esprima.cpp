#include "Escargot.h"
#include "parser/ast/AST.h"
#include "double-conversion.h"
#include "ieee.h"


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

enum PlaceHolders {
    ArrowParameterPlaceHolder
};

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

const char16_t* TokenName[] = {
    u"",
    u"Boolean",
    u"<end>",
    u"Identifier",
    u"Keyword",
    u"Null",
    u"Numeric",
    u"Punctuator",
    u"String",
    u"RegularExpression",
    u"Template",
};

const char16_t* PuncuatorsTokens[] = {
    u"(", u")", u"{", u"}", u".", u"...", u",", u":", u";", u"[", u"]",
    // binary/unary operators
    u"?", u"~", u">>>", u">>", u"<<", u"+", u"-", u"*", u"/", u"%", u"!",
    u"==", u"!==", u"==", u"!=", u"&&", u"||", u"++", u"--", u"&", u"|",
    u"^", u"<", u">", u"in", u"instanceof" ,
    // assignment operators
    u"=", u">>>=", u"<<=", u">>=", u"+=", u"-=", u"*=", u"/=", u"%=",
    u"&=", u"|=", u"^=", u"<=", u">=",
    // SubstitutionEnd
    u"",
    // arrow
    u"=>"
};

const char16_t* KeywordTokens[] = {
    u"", u"if", u"in", u"do", u"var", u"for", u"new", u"try", u"this",
    u"else", u"case", u"void", u"with", u"enum", u"while", u"break",
    u"catch", u"throw", u"const", u"class", u"super", u"return", u"typeof",
    u"delete", u"switch", u"export", u"import", u"default", u"finally",
    u"extends", u"function", u"continue", u"debugger", u"instanceof",
    u"", u"implements", u"interface", u"package", u"private", u"protected", u"public",
    u"static", u"yield", u"let"
};

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

ALWAYS_INLINE bool octalValue(char16_t ch)
{
    ASSERT(isOctalDigit(ch));
    return ch - '0';
}

ALWAYS_INLINE uint8_t toHexNumericValue(char16_t ch)
{
    return ch < 'A' ? ch - '0' : (ch - 'A' + 10) & 0xF;
}

// ECMA-262 11.2 White Space
ALWAYS_INLINE bool isWhiteSpace(char16_t ch)
{
    return (ch == 0x20) || (ch == 0x09) || (ch == 0x0B) || (ch == 0x0C) || (ch == 0xA0)
        || (ch >= 0x1680 && (ch == 0x1680 || ch == 0x180E  || ch == 0x2000 || ch == 0x2001
        || ch == 0x2002 || ch == 0x2003 || ch == 0x2004 || ch == 0x2005 || ch == 0x2006
        || ch == 0x2007 || ch == 0x2008 || ch == 0x2009 || ch == 0x200A || ch == 0x202F
        || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF));
}

// ECMA-262 11.3 Line Terminators
ALWAYS_INLINE bool isLineTerminator(char16_t ch)
{
    return (ch == 0x0A) || (ch == 0x0D) || (ch == 0x2028) || (ch == 0x2029);
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

bool isIdentifierPartSlow(char32_t ch)
{
    return (ch == 0xAA) || (ch == 0xB5) || (ch == 0xB7) || (ch == 0xBA) || (0xC0 <= ch && ch <= 0xD6) || (0xD8 <= ch && ch <= 0xF6) || (0xF8 <= ch && ch <= 0x02C1) || (0x02C6 <= ch && ch <= 0x02D1) || (0x02E0 <= ch && ch <= 0x02E4) || (ch == 0x02EC) || (ch == 0x02EE) || (0x0300 <= ch && ch <= 0x0374) || (ch == 0x0376) || (ch == 0x0377) || (0x037A <= ch && ch <= 0x037D) || (ch == 0x037F) || (0x0386 <= ch && ch <= 0x038A) || (ch == 0x038C) || (0x038E <= ch && ch <= 0x03A1) || (0x03A3 <= ch && ch <= 0x03F5) || (0x03F7 <= ch && ch <= 0x0481) || (0x0483 <= ch && ch <= 0x0487) || (0x048A <= ch && ch <= 0x052F) || (0x0531 <= ch && ch <= 0x0556) || (ch == 0x0559) || (0x0561 <= ch && ch <= 0x0587) || (0x0591 <= ch && ch <= 0x05BD) || (ch == 0x05BF) || (ch == 0x05C1) || (ch == 0x05C2) || (ch == 0x05C4) || (ch == 0x05C5) || (ch == 0x05C7) || (0x05D0 <= ch && ch <= 0x05EA) || (0x05F0 <= ch && ch <= 0x05F2) || (0x0610 <= ch && ch <= 0x061A) || (0x0620 <= ch && ch <= 0x0669) || (0x066E <= ch && ch <= 0x06D3) || (0x06D5 <= ch && ch <= 0x06DC) || (0x06DF <= ch && ch <= 0x06E8) || (0x06EA <= ch && ch <= 0x06FC) || (ch == 0x06FF) || (0x0710 <= ch && ch <= 0x074A) || (0x074D <= ch && ch <= 0x07B1) || (0x07C0 <= ch && ch <= 0x07F5) || (ch == 0x07FA) || (0x0800 <= ch && ch <= 0x082D) || (0x0840 <= ch && ch <= 0x085B) || (0x08A0 <= ch && ch <= 0x08B2) || (0x08E4 <= ch && ch <= 0x0963) || (0x0966 <= ch && ch <= 0x096F) || (0x0971 <= ch && ch <= 0x0983) || (0x0985 <= ch && ch <= 0x098C) || (ch == 0x098F) || (ch == 0x0990) || (0x0993 <= ch && ch <= 0x09A8) || (0x09AA <= ch && ch <= 0x09B0) || (ch == 0x09B2) || (0x09B6 <= ch && ch <= 0x09B9) || (0x09BC <= ch && ch <= 0x09C4) || (ch == 0x09C7) || (ch == 0x09C8) || (0x09CB <= ch && ch <= 0x09CE) || (ch == 0x09D7) || (ch == 0x09DC) || (ch == 0x09DD) || (0x09DF <= ch && ch <= 0x09E3) || (0x09E6 <= ch && ch <= 0x09F1) || (0x0A01 <= ch && ch <= 0x0A03) || (0x0A05 <= ch && ch <= 0x0A0A) || (ch == 0x0A0F) || (ch == 0x0A10) || (0x0A13 <= ch && ch <= 0x0A28) || (0x0A2A <= ch && ch <= 0x0A30) || (ch == 0x0A32) || (ch == 0x0A33) || (ch == 0x0A35) || (ch == 0x0A36) || (ch == 0x0A38) || (ch == 0x0A39) || (ch == 0x0A3C) || (0x0A3E <= ch && ch <= 0x0A42) || (ch == 0x0A47) || (ch == 0x0A48) || (0x0A4B <= ch && ch <= 0x0A4D) || (ch == 0x0A51) || (0x0A59 <= ch && ch <= 0x0A5C) || (ch == 0x0A5E) || (0x0A66 <= ch && ch <= 0x0A75) || (0x0A81 <= ch && ch <= 0x0A83) || (0x0A85 <= ch && ch <= 0x0A8D) || (0x0A8F <= ch && ch <= 0x0A91) || (0x0A93 <= ch && ch <= 0x0AA8) || (0x0AAA <= ch && ch <= 0x0AB0) || (ch == 0x0AB2) || (ch == 0x0AB3) || (0x0AB5 <= ch && ch <= 0x0AB9) || (0x0ABC <= ch && ch <= 0x0AC5) || (0x0AC7 <= ch && ch <= 0x0AC9) || (0x0ACB <= ch && ch <= 0x0ACD) || (ch == 0x0AD0) || (0x0AE0 <= ch && ch <= 0x0AE3) || (0x0AE6 <= ch && ch <= 0x0AEF) || (0x0B01 <= ch && ch <= 0x0B03) || (0x0B05 <= ch && ch <= 0x0B0C) || (ch == 0x0B0F) || (ch == 0x0B10) || (0x0B13 <= ch && ch <= 0x0B28) || (0x0B2A <= ch && ch <= 0x0B30) || (ch == 0x0B32) || (ch == 0x0B33) || (0x0B35 <= ch && ch <= 0x0B39) || (0x0B3C <= ch && ch <= 0x0B44) || (ch == 0x0B47) || (ch == 0x0B48) || (0x0B4B <= ch && ch <= 0x0B4D) || (ch == 0x0B56) || (ch == 0x0B57) || (ch == 0x0B5C) || (ch == 0x0B5D) || (0x0B5F <= ch && ch <= 0x0B63) || (0x0B66 <= ch && ch <= 0x0B6F) || (ch == 0x0B71) || (ch == 0x0B82) || (ch == 0x0B83) || (0x0B85 <= ch && ch <= 0x0B8A) || (0x0B8E <= ch && ch <= 0x0B90) || (0x0B92 <= ch && ch <= 0x0B95) || (ch == 0x0B99) || (ch == 0x0B9A) || (ch == 0x0B9C) || (ch == 0x0B9E) || (ch == 0x0B9F) || (ch == 0x0BA3) || (ch == 0x0BA4) || (0x0BA8 <= ch && ch <= 0x0BAA) || (0x0BAE <= ch && ch <= 0x0BB9) || (0x0BBE <= ch && ch <= 0x0BC2) || (0x0BC6 <= ch && ch <= 0x0BC8) || (0x0BCA <= ch && ch <= 0x0BCD) || (ch == 0x0BD0) || (ch == 0x0BD7) || (0x0BE6 <= ch && ch <= 0x0BEF) || (0x0C00 <= ch && ch <= 0x0C03) || (0x0C05 <= ch && ch <= 0x0C0C) || (0x0C0E <= ch && ch <= 0x0C10) || (0x0C12 <= ch && ch <= 0x0C28) || (0x0C2A <= ch && ch <= 0x0C39) || (0x0C3D <= ch && ch <= 0x0C44) || (0x0C46 <= ch && ch <= 0x0C48) || (0x0C4A <= ch && ch <= 0x0C4D) || (ch == 0x0C55) || (ch == 0x0C56) || (ch == 0x0C58) || (ch == 0x0C59) || (0x0C60 <= ch && ch <= 0x0C63) || (0x0C66 <= ch && ch <= 0x0C6F) || (0x0C81 <= ch && ch <= 0x0C83) || (0x0C85 <= ch && ch <= 0x0C8C) || (0x0C8E <= ch && ch <= 0x0C90) || (0x0C92 <= ch && ch <= 0x0CA8) || (0x0CAA <= ch && ch <= 0x0CB3) || (0x0CB5 <= ch && ch <= 0x0CB9) || (0x0CBC <= ch && ch <= 0x0CC4) || (0x0CC6 <= ch && ch <= 0x0CC8) || (0x0CCA <= ch && ch <= 0x0CCD) || (ch == 0x0CD5) || (ch == 0x0CD6) || (ch == 0x0CDE) || (0x0CE0 <= ch && ch <= 0x0CE3) || (0x0CE6 <= ch && ch <= 0x0CEF) || (ch == 0x0CF1) || (ch == 0x0CF2) || (0x0D01 <= ch && ch <= 0x0D03) || (0x0D05 <= ch && ch <= 0x0D0C) || (0x0D0E <= ch && ch <= 0x0D10) || (0x0D12 <= ch && ch <= 0x0D3A) || (0x0D3D <= ch && ch <= 0x0D44) || (0x0D46 <= ch && ch <= 0x0D48) || (0x0D4A <= ch && ch <= 0x0D4E) || (ch == 0x0D57) || (0x0D60 <= ch && ch <= 0x0D63) || (0x0D66 <= ch && ch <= 0x0D6F) || (0x0D7A <= ch && ch <= 0x0D7F) || (ch == 0x0D82) || (ch == 0x0D83) || (0x0D85 <= ch && ch <= 0x0D96) || (0x0D9A <= ch && ch <= 0x0DB1) || (0x0DB3 <= ch && ch <= 0x0DBB) || (ch == 0x0DBD) || (0x0DC0 <= ch && ch <= 0x0DC6) || (ch == 0x0DCA) || (0x0DCF <= ch && ch <= 0x0DD4) || (ch == 0x0DD6) || (0x0DD8 <= ch && ch <= 0x0DDF) || (0x0DE6 <= ch && ch <= 0x0DEF) || (ch == 0x0DF2) || (ch == 0x0DF3) || (0x0E01 <= ch && ch <= 0x0E3A) || (0x0E40 <= ch && ch <= 0x0E4E) || (0x0E50 <= ch && ch <= 0x0E59) || (ch == 0x0E81) || (ch == 0x0E82) || (ch == 0x0E84) || (ch == 0x0E87) || (ch == 0x0E88) || (ch == 0x0E8A) || (ch == 0x0E8D) || (0x0E94 <= ch && ch <= 0x0E97) || (0x0E99 <= ch && ch <= 0x0E9F) || (0x0EA1 <= ch && ch <= 0x0EA3) || (ch == 0x0EA5) || (ch == 0x0EA7) || (ch == 0x0EAA) || (ch == 0x0EAB) || (0x0EAD <= ch && ch <= 0x0EB9) || (0x0EBB <= ch && ch <= 0x0EBD) || (0x0EC0 <= ch && ch <= 0x0EC4) || (ch == 0x0EC6) || (0x0EC8 <= ch && ch <= 0x0ECD) || (0x0ED0 <= ch && ch <= 0x0ED9) || (0x0EDC <= ch && ch <= 0x0EDF) || (ch == 0x0F00) || (ch == 0x0F18) || (ch == 0x0F19) || (0x0F20 <= ch && ch <= 0x0F29) || (ch == 0x0F35) || (ch == 0x0F37) || (ch == 0x0F39) || (0x0F3E <= ch && ch <= 0x0F47) || (0x0F49 <= ch && ch <= 0x0F6C) || (0x0F71 <= ch && ch <= 0x0F84) || (0x0F86 <= ch && ch <= 0x0F97) || (0x0F99 <= ch && ch <= 0x0FBC) || (ch == 0x0FC6) || (0x1000 <= ch && ch <= 0x1049) || (0x1050 <= ch && ch <= 0x109D) || (0x10A0 <= ch && ch <= 0x10C5) || (ch == 0x10C7) || (ch == 0x10CD) || (0x10D0 <= ch && ch <= 0x10FA) || (0x10FC <= ch && ch <= 0x1248) || (0x124A <= ch && ch <= 0x124D) || (0x1250 <= ch && ch <= 0x1256) || (ch == 0x1258) || (0x125A <= ch && ch <= 0x125D) || (0x1260 <= ch && ch <= 0x1288) || (0x128A <= ch && ch <= 0x128D) || (0x1290 <= ch && ch <= 0x12B0) || (0x12B2 <= ch && ch <= 0x12B5) || (0x12B8 <= ch && ch <= 0x12BE) || (ch == 0x12C0) || (0x12C2 <= ch && ch <= 0x12C5) || (0x12C8 <= ch && ch <= 0x12D6) || (0x12D8 <= ch && ch <= 0x1310) || (0x1312 <= ch && ch <= 0x1315) || (0x1318 <= ch && ch <= 0x135A) || (0x135D <= ch && ch <= 0x135F) || (0x1369 <= ch && ch <= 0x1371) || (0x1380 <= ch && ch <= 0x138F) || (0x13A0 <= ch && ch <= 0x13F4) || (0x1401 <= ch && ch <= 0x166C) || (0x166F <= ch && ch <= 0x167F) || (0x1681 <= ch && ch <= 0x169A) || (0x16A0 <= ch && ch <= 0x16EA) || (0x16EE <= ch && ch <= 0x16F8) || (0x1700 <= ch && ch <= 0x170C) || (0x170E <= ch && ch <= 0x1714) || (0x1720 <= ch && ch <= 0x1734) || (0x1740 <= ch && ch <= 0x1753) || (0x1760 <= ch && ch <= 0x176C) || (0x176E <= ch && ch <= 0x1770) || (ch == 0x1772) || (ch == 0x1773) || (0x1780 <= ch && ch <= 0x17D3) || (ch == 0x17D7) || (ch == 0x17DC) || (ch == 0x17DD) || (0x17E0 <= ch && ch <= 0x17E9) || (0x180B <= ch && ch <= 0x180D) || (0x1810 <= ch && ch <= 0x1819) || (0x1820 <= ch && ch <= 0x1877) || (0x1880 <= ch && ch <= 0x18AA) || (0x18B0 <= ch && ch <= 0x18F5) || (0x1900 <= ch && ch <= 0x191E) || (0x1920 <= ch && ch <= 0x192B) || (0x1930 <= ch && ch <= 0x193B) || (0x1946 <= ch && ch <= 0x196D) || (0x1970 <= ch && ch <= 0x1974) || (0x1980 <= ch && ch <= 0x19AB) || (0x19B0 <= ch && ch <= 0x19C9) || (0x19D0 <= ch && ch <= 0x19DA) || (0x1A00 <= ch && ch <= 0x1A1B) || (0x1A20 <= ch && ch <= 0x1A5E) || (0x1A60 <= ch && ch <= 0x1A7C) || (0x1A7F <= ch && ch <= 0x1A89) || (0x1A90 <= ch && ch <= 0x1A99) || (ch == 0x1AA7) || (0x1AB0 <= ch && ch <= 0x1ABD) || (0x1B00 <= ch && ch <= 0x1B4B) || (0x1B50 <= ch && ch <= 0x1B59) || (0x1B6B <= ch && ch <= 0x1B73) || (0x1B80 <= ch && ch <= 0x1BF3) || (0x1C00 <= ch && ch <= 0x1C37) || (0x1C40 <= ch && ch <= 0x1C49) || (0x1C4D <= ch && ch <= 0x1C7D) || (0x1CD0 <= ch && ch <= 0x1CD2) || (0x1CD4 <= ch && ch <= 0x1CF6) || (ch == 0x1CF8) || (ch == 0x1CF9) || (0x1D00 <= ch && ch <= 0x1DF5) || (0x1DFC <= ch && ch <= 0x1F15) || (0x1F18 <= ch && ch <= 0x1F1D) || (0x1F20 <= ch && ch <= 0x1F45) || (0x1F48 <= ch && ch <= 0x1F4D) || (0x1F50 <= ch && ch <= 0x1F57) || (ch == 0x1F59) || (ch == 0x1F5B) || (ch == 0x1F5D) || (0x1F5F <= ch && ch <= 0x1F7D) || (0x1F80 <= ch && ch <= 0x1FB4) || (0x1FB6 <= ch && ch <= 0x1FBC) || (ch == 0x1FBE) || (0x1FC2 <= ch && ch <= 0x1FC4) || (0x1FC6 <= ch && ch <= 0x1FCC) || (0x1FD0 <= ch && ch <= 0x1FD3) || (0x1FD6 <= ch && ch <= 0x1FDB) || (0x1FE0 <= ch && ch <= 0x1FEC) || (0x1FF2 <= ch && ch <= 0x1FF4) || (0x1FF6 <= ch && ch <= 0x1FFC) || (ch == 0x200C) || (ch == 0x200D) || (ch == 0x203F) || (ch == 0x2040) || (ch == 0x2054) || (ch == 0x2071) || (ch == 0x207F) || (0x2090 <= ch && ch <= 0x209C) || (0x20D0 <= ch && ch <= 0x20DC) || (ch == 0x20E1) || (0x20E5 <= ch && ch <= 0x20F0) || (ch == 0x2102) || (ch == 0x2107) || (0x210A <= ch && ch <= 0x2113) || (ch == 0x2115) || (0x2118 <= ch && ch <= 0x211D) || (ch == 0x2124) || (ch == 0x2126) || (ch == 0x2128) || (0x212A <= ch && ch <= 0x2139) || (0x213C <= ch && ch <= 0x213F) || (0x2145 <= ch && ch <= 0x2149) || (ch == 0x214E) || (0x2160 <= ch && ch <= 0x2188) || (0x2C00 <= ch && ch <= 0x2C2E) || (0x2C30 <= ch && ch <= 0x2C5E) || (0x2C60 <= ch && ch <= 0x2CE4) || (0x2CEB <= ch && ch <= 0x2CF3) || (0x2D00 <= ch && ch <= 0x2D25) || (ch == 0x2D27) || (ch == 0x2D2D) || (0x2D30 <= ch && ch <= 0x2D67) || (ch == 0x2D6F) || (0x2D7F <= ch && ch <= 0x2D96) || (0x2DA0 <= ch && ch <= 0x2DA6) || (0x2DA8 <= ch && ch <= 0x2DAE) || (0x2DB0 <= ch && ch <= 0x2DB6) || (0x2DB8 <= ch && ch <= 0x2DBE) || (0x2DC0 <= ch && ch <= 0x2DC6) || (0x2DC8 <= ch && ch <= 0x2DCE) || (0x2DD0 <= ch && ch <= 0x2DD6) || (0x2DD8 <= ch && ch <= 0x2DDE) || (0x2DE0 <= ch && ch <= 0x2DFF) || (0x3005 <= ch && ch <= 0x3007) || (0x3021 <= ch && ch <= 0x302F) || (0x3031 <= ch && ch <= 0x3035) || (0x3038 <= ch && ch <= 0x303C) || (0x3041 <= ch && ch <= 0x3096) || (0x3099 <= ch && ch <= 0x309F) || (0x30A1 <= ch && ch <= 0x30FA) || (0x30FC <= ch && ch <= 0x30FF) || (0x3105 <= ch && ch <= 0x312D) || (0x3131 <= ch && ch <= 0x318E) || (0x31A0 <= ch && ch <= 0x31BA) || (0x31F0 <= ch && ch <= 0x31FF) || (0x3400 <= ch && ch <= 0x4DB5) || (0x4E00 <= ch && ch <= 0x9FCC) || (0xA000 <= ch && ch <= 0xA48C) || (0xA4D0 <= ch && ch <= 0xA4FD) || (0xA500 <= ch && ch <= 0xA60C) || (0xA610 <= ch && ch <= 0xA62B) || (0xA640 <= ch && ch <= 0xA66F) || (0xA674 <= ch && ch <= 0xA67D) || (0xA67F <= ch && ch <= 0xA69D) || (0xA69F <= ch && ch <= 0xA6F1) || (0xA717 <= ch && ch <= 0xA71F) || (0xA722 <= ch && ch <= 0xA788) || (0xA78B <= ch && ch <= 0xA78E) || (0xA790 <= ch && ch <= 0xA7AD) || (ch == 0xA7B0) || (ch == 0xA7B1) || (0xA7F7 <= ch && ch <= 0xA827) || (0xA840 <= ch && ch <= 0xA873) || (0xA880 <= ch && ch <= 0xA8C4) || (0xA8D0 <= ch && ch <= 0xA8D9) || (0xA8E0 <= ch && ch <= 0xA8F7) || (ch == 0xA8FB) || (0xA900 <= ch && ch <= 0xA92D) || (0xA930 <= ch && ch <= 0xA953) || (0xA960 <= ch && ch <= 0xA97C) || (0xA980 <= ch && ch <= 0xA9C0) || (0xA9CF <= ch && ch <= 0xA9D9) || (0xA9E0 <= ch && ch <= 0xA9FE) || (0xAA00 <= ch && ch <= 0xAA36) || (0xAA40 <= ch && ch <= 0xAA4D) || (0xAA50 <= ch && ch <= 0xAA59) || (0xAA60 <= ch && ch <= 0xAA76) || (0xAA7A <= ch && ch <= 0xAAC2) || (0xAADB <= ch && ch <= 0xAADD) || (0xAAE0 <= ch && ch <= 0xAAEF) || (0xAAF2 <= ch && ch <= 0xAAF6) || (0xAB01 <= ch && ch <= 0xAB06) || (0xAB09 <= ch && ch <= 0xAB0E) || (0xAB11 <= ch && ch <= 0xAB16) || (0xAB20 <= ch && ch <= 0xAB26) || (0xAB28 <= ch && ch <= 0xAB2E) || (0xAB30 <= ch && ch <= 0xAB5A) || (0xAB5C <= ch && ch <= 0xAB5F) || (ch == 0xAB64) || (ch == 0xAB65) || (0xABC0 <= ch && ch <= 0xABEA) || (ch == 0xABEC) || (ch == 0xABED) || (0xABF0 <= ch && ch <= 0xABF9) || (0xAC00 <= ch && ch <= 0xD7A3) || (0xD7B0 <= ch && ch <= 0xD7C6) || (0xD7CB <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFA6D) || (0xFA70 <= ch && ch <= 0xFAD9) || (0xFB00 <= ch && ch <= 0xFB06) || (0xFB13 <= ch && ch <= 0xFB17) || (0xFB1D <= ch && ch <= 0xFB28) || (0xFB2A <= ch && ch <= 0xFB36) || (0xFB38 <= ch && ch <= 0xFB3C) || (ch == 0xFB3E) || (ch == 0xFB40) || (ch == 0xFB41) || (ch == 0xFB43) || (ch == 0xFB44) || (0xFB46 <= ch && ch <= 0xFBB1) || (0xFBD3 <= ch && ch <= 0xFD3D) || (0xFD50 <= ch && ch <= 0xFD8F) || (0xFD92 <= ch && ch <= 0xFDC7) || (0xFDF0 <= ch && ch <= 0xFDFB) || (0xFE00 <= ch && ch <= 0xFE0F) || (0xFE20 <= ch && ch <= 0xFE2D) || (ch == 0xFE33) || (ch == 0xFE34) || (0xFE4D <= ch && ch <= 0xFE4F) || (0xFE70 <= ch && ch <= 0xFE74) || (0xFE76 <= ch && ch <= 0xFEFC) || (0xFF10 <= ch && ch <= 0xFF19) || (0xFF21 <= ch && ch <= 0xFF3A) || (ch == 0xFF3F) || (0xFF41 <= ch && ch <= 0xFF5A) || (0xFF66 <= ch && ch <= 0xFFBE) || (0xFFC2 <= ch && ch <= 0xFFC7) || (0xFFCA <= ch && ch <= 0xFFCF) || (0xFFD2 <= ch && ch <= 0xFFD7) || (0xFFDA <= ch && ch <= 0xFFDC);
}

ALWAYS_INLINE bool isIdentifierPart(char32_t ch)
{
    // TODO
    return (ch >= 97 && ch <= 122) // a..z
        || (ch >= 65 && ch <= 90) // A..Z
        || (ch >= 48 && ch <= 57) // 0..9
        || (ch == 36) || (ch == 95) // $ (dollar) and _ (underscore)
        || (ch == 92) // \ (backslash)
        || isIdentifierPartSlow(ch);
}

bool isIdentifierStartSlow(char32_t ch)
{
    return (ch == 0xAA) || (ch == 0xB5) || (ch == 0xB7) || (ch == 0xBA) || (0xC0 <= ch && ch <= 0xD6) || (0xD8 <= ch && ch <= 0xF6) || (0xF8 <= ch && ch <= 0x02C1) || (0x02C6 <= ch && ch <= 0x02D1) || (0x02E0 <= ch && ch <= 0x02E4) || (ch == 0x02EC) || (ch == 0x02EE) || (0x0300 <= ch && ch <= 0x0374) || (ch == 0x0376) || (ch == 0x0377) || (0x037A <= ch && ch <= 0x037D) || (ch == 0x037F) || (0x0386 <= ch && ch <= 0x038A) || (ch == 0x038C) || (0x038E <= ch && ch <= 0x03A1) || (0x03A3 <= ch && ch <= 0x03F5) || (0x03F7 <= ch && ch <= 0x0481) || (0x0483 <= ch && ch <= 0x0487) || (0x048A <= ch && ch <= 0x052F) || (0x0531 <= ch && ch <= 0x0556) || (ch == 0x0559) || (0x0561 <= ch && ch <= 0x0587) || (0x0591 <= ch && ch <= 0x05BD) || (ch == 0x05BF) || (ch == 0x05C1) || (ch == 0x05C2) || (ch == 0x05C4) || (ch == 0x05C5) || (ch == 0x05C7) || (0x05D0 <= ch && ch <= 0x05EA) || (0x05F0 <= ch && ch <= 0x05F2) || (0x0610 <= ch && ch <= 0x061A) || (0x0620 <= ch && ch <= 0x0669) || (0x066E <= ch && ch <= 0x06D3) || (0x06D5 <= ch && ch <= 0x06DC) || (0x06DF <= ch && ch <= 0x06E8) || (0x06EA <= ch && ch <= 0x06FC) || (ch == 0x06FF) || (0x0710 <= ch && ch <= 0x074A) || (0x074D <= ch && ch <= 0x07B1) || (0x07C0 <= ch && ch <= 0x07F5) || (ch == 0x07FA) || (0x0800 <= ch && ch <= 0x082D) || (0x0840 <= ch && ch <= 0x085B) || (0x08A0 <= ch && ch <= 0x08B2) || (0x08E4 <= ch && ch <= 0x0963) || (0x0966 <= ch && ch <= 0x096F) || (0x0971 <= ch && ch <= 0x0983) || (0x0985 <= ch && ch <= 0x098C) || (ch == 0x098F) || (ch == 0x0990) || (0x0993 <= ch && ch <= 0x09A8) || (0x09AA <= ch && ch <= 0x09B0) || (ch == 0x09B2) || (0x09B6 <= ch && ch <= 0x09B9) || (0x09BC <= ch && ch <= 0x09C4) || (ch == 0x09C7) || (ch == 0x09C8) || (0x09CB <= ch && ch <= 0x09CE) || (ch == 0x09D7) || (ch == 0x09DC) || (ch == 0x09DD) || (0x09DF <= ch && ch <= 0x09E3) || (0x09E6 <= ch && ch <= 0x09F1) || (0x0A01 <= ch && ch <= 0x0A03) || (0x0A05 <= ch && ch <= 0x0A0A) || (ch == 0x0A0F) || (ch == 0x0A10) || (0x0A13 <= ch && ch <= 0x0A28) || (0x0A2A <= ch && ch <= 0x0A30) || (ch == 0x0A32) || (ch == 0x0A33) || (ch == 0x0A35) || (ch == 0x0A36) || (ch == 0x0A38) || (ch == 0x0A39) || (ch == 0x0A3C) || (0x0A3E <= ch && ch <= 0x0A42) || (ch == 0x0A47) || (ch == 0x0A48) || (0x0A4B <= ch && ch <= 0x0A4D) || (ch == 0x0A51) || (0x0A59 <= ch && ch <= 0x0A5C) || (ch == 0x0A5E) || (0x0A66 <= ch && ch <= 0x0A75) || (0x0A81 <= ch && ch <= 0x0A83) || (0x0A85 <= ch && ch <= 0x0A8D) || (0x0A8F <= ch && ch <= 0x0A91) || (0x0A93 <= ch && ch <= 0x0AA8) || (0x0AAA <= ch && ch <= 0x0AB0) || (ch == 0x0AB2) || (ch == 0x0AB3) || (0x0AB5 <= ch && ch <= 0x0AB9) || (0x0ABC <= ch && ch <= 0x0AC5) || (0x0AC7 <= ch && ch <= 0x0AC9) || (0x0ACB <= ch && ch <= 0x0ACD) || (ch == 0x0AD0) || (0x0AE0 <= ch && ch <= 0x0AE3) || (0x0AE6 <= ch && ch <= 0x0AEF) || (0x0B01 <= ch && ch <= 0x0B03) || (0x0B05 <= ch && ch <= 0x0B0C) || (ch == 0x0B0F) || (ch == 0x0B10) || (0x0B13 <= ch && ch <= 0x0B28) || (0x0B2A <= ch && ch <= 0x0B30) || (ch == 0x0B32) || (ch == 0x0B33) || (0x0B35 <= ch && ch <= 0x0B39) || (0x0B3C <= ch && ch <= 0x0B44) || (ch == 0x0B47) || (ch == 0x0B48) || (0x0B4B <= ch && ch <= 0x0B4D) || (ch == 0x0B56) || (ch == 0x0B57) || (ch == 0x0B5C) || (ch == 0x0B5D) || (0x0B5F <= ch && ch <= 0x0B63) || (0x0B66 <= ch && ch <= 0x0B6F) || (ch == 0x0B71) || (ch == 0x0B82) || (ch == 0x0B83) || (0x0B85 <= ch && ch <= 0x0B8A) || (0x0B8E <= ch && ch <= 0x0B90) || (0x0B92 <= ch && ch <= 0x0B95) || (ch == 0x0B99) || (ch == 0x0B9A) || (ch == 0x0B9C) || (ch == 0x0B9E) || (ch == 0x0B9F) || (ch == 0x0BA3) || (ch == 0x0BA4) || (0x0BA8 <= ch && ch <= 0x0BAA) || (0x0BAE <= ch && ch <= 0x0BB9) || (0x0BBE <= ch && ch <= 0x0BC2) || (0x0BC6 <= ch && ch <= 0x0BC8) || (0x0BCA <= ch && ch <= 0x0BCD) || (ch == 0x0BD0) || (ch == 0x0BD7) || (0x0BE6 <= ch && ch <= 0x0BEF) || (0x0C00 <= ch && ch <= 0x0C03) || (0x0C05 <= ch && ch <= 0x0C0C) || (0x0C0E <= ch && ch <= 0x0C10) || (0x0C12 <= ch && ch <= 0x0C28) || (0x0C2A <= ch && ch <= 0x0C39) || (0x0C3D <= ch && ch <= 0x0C44) || (0x0C46 <= ch && ch <= 0x0C48) || (0x0C4A <= ch && ch <= 0x0C4D) || (ch == 0x0C55) || (ch == 0x0C56) || (ch == 0x0C58) || (ch == 0x0C59) || (0x0C60 <= ch && ch <= 0x0C63) || (0x0C66 <= ch && ch <= 0x0C6F) || (0x0C81 <= ch && ch <= 0x0C83) || (0x0C85 <= ch && ch <= 0x0C8C) || (0x0C8E <= ch && ch <= 0x0C90) || (0x0C92 <= ch && ch <= 0x0CA8) || (0x0CAA <= ch && ch <= 0x0CB3) || (0x0CB5 <= ch && ch <= 0x0CB9) || (0x0CBC <= ch && ch <= 0x0CC4) || (0x0CC6 <= ch && ch <= 0x0CC8) || (0x0CCA <= ch && ch <= 0x0CCD) || (ch == 0x0CD5) || (ch == 0x0CD6) || (ch == 0x0CDE) || (0x0CE0 <= ch && ch <= 0x0CE3) || (0x0CE6 <= ch && ch <= 0x0CEF) || (ch == 0x0CF1) || (ch == 0x0CF2) || (0x0D01 <= ch && ch <= 0x0D03) || (0x0D05 <= ch && ch <= 0x0D0C) || (0x0D0E <= ch && ch <= 0x0D10) || (0x0D12 <= ch && ch <= 0x0D3A) || (0x0D3D <= ch && ch <= 0x0D44) || (0x0D46 <= ch && ch <= 0x0D48) || (0x0D4A <= ch && ch <= 0x0D4E) || (ch == 0x0D57) || (0x0D60 <= ch && ch <= 0x0D63) || (0x0D66 <= ch && ch <= 0x0D6F) || (0x0D7A <= ch && ch <= 0x0D7F) || (ch == 0x0D82) || (ch == 0x0D83) || (0x0D85 <= ch && ch <= 0x0D96) || (0x0D9A <= ch && ch <= 0x0DB1) || (0x0DB3 <= ch && ch <= 0x0DBB) || (ch == 0x0DBD) || (0x0DC0 <= ch && ch <= 0x0DC6) || (ch == 0x0DCA) || (0x0DCF <= ch && ch <= 0x0DD4) || (ch == 0x0DD6) || (0x0DD8 <= ch && ch <= 0x0DDF) || (0x0DE6 <= ch && ch <= 0x0DEF) || (ch == 0x0DF2) || (ch == 0x0DF3) || (0x0E01 <= ch && ch <= 0x0E3A) || (0x0E40 <= ch && ch <= 0x0E4E) || (0x0E50 <= ch && ch <= 0x0E59) || (ch == 0x0E81) || (ch == 0x0E82) || (ch == 0x0E84) || (ch == 0x0E87) || (ch == 0x0E88) || (ch == 0x0E8A) || (ch == 0x0E8D) || (0x0E94 <= ch && ch <= 0x0E97) || (0x0E99 <= ch && ch <= 0x0E9F) || (0x0EA1 <= ch && ch <= 0x0EA3) || (ch == 0x0EA5) || (ch == 0x0EA7) || (ch == 0x0EAA) || (ch == 0x0EAB) || (0x0EAD <= ch && ch <= 0x0EB9) || (0x0EBB <= ch && ch <= 0x0EBD) || (0x0EC0 <= ch && ch <= 0x0EC4) || (ch == 0x0EC6) || (0x0EC8 <= ch && ch <= 0x0ECD) || (0x0ED0 <= ch && ch <= 0x0ED9) || (0x0EDC <= ch && ch <= 0x0EDF) || (ch == 0x0F00) || (ch == 0x0F18) || (ch == 0x0F19) || (0x0F20 <= ch && ch <= 0x0F29) || (ch == 0x0F35) || (ch == 0x0F37) || (ch == 0x0F39) || (0x0F3E <= ch && ch <= 0x0F47) || (0x0F49 <= ch && ch <= 0x0F6C) || (0x0F71 <= ch && ch <= 0x0F84) || (0x0F86 <= ch && ch <= 0x0F97) || (0x0F99 <= ch && ch <= 0x0FBC) || (ch == 0x0FC6) || (0x1000 <= ch && ch <= 0x1049) || (0x1050 <= ch && ch <= 0x109D) || (0x10A0 <= ch && ch <= 0x10C5) || (ch == 0x10C7) || (ch == 0x10CD) || (0x10D0 <= ch && ch <= 0x10FA) || (0x10FC <= ch && ch <= 0x1248) || (0x124A <= ch && ch <= 0x124D) || (0x1250 <= ch && ch <= 0x1256) || (ch == 0x1258) || (0x125A <= ch && ch <= 0x125D) || (0x1260 <= ch && ch <= 0x1288) || (0x128A <= ch && ch <= 0x128D) || (0x1290 <= ch && ch <= 0x12B0) || (0x12B2 <= ch && ch <= 0x12B5) || (0x12B8 <= ch && ch <= 0x12BE) || (ch == 0x12C0) || (0x12C2 <= ch && ch <= 0x12C5) || (0x12C8 <= ch && ch <= 0x12D6) || (0x12D8 <= ch && ch <= 0x1310) || (0x1312 <= ch && ch <= 0x1315) || (0x1318 <= ch && ch <= 0x135A) || (0x135D <= ch && ch <= 0x135F) || (0x1369 <= ch && ch <= 0x1371) || (0x1380 <= ch && ch <= 0x138F) || (0x13A0 <= ch && ch <= 0x13F4) || (0x1401 <= ch && ch <= 0x166C) || (0x166F <= ch && ch <= 0x167F) || (0x1681 <= ch && ch <= 0x169A) || (0x16A0 <= ch && ch <= 0x16EA) || (0x16EE <= ch && ch <= 0x16F8) || (0x1700 <= ch && ch <= 0x170C) || (0x170E <= ch && ch <= 0x1714) || (0x1720 <= ch && ch <= 0x1734) || (0x1740 <= ch && ch <= 0x1753) || (0x1760 <= ch && ch <= 0x176C) || (0x176E <= ch && ch <= 0x1770) || (ch == 0x1772) || (ch == 0x1773) || (0x1780 <= ch && ch <= 0x17D3) || (ch == 0x17D7) || (ch == 0x17DC) || (ch == 0x17DD) || (0x17E0 <= ch && ch <= 0x17E9) || (0x180B <= ch && ch <= 0x180D) || (0x1810 <= ch && ch <= 0x1819) || (0x1820 <= ch && ch <= 0x1877) || (0x1880 <= ch && ch <= 0x18AA) || (0x18B0 <= ch && ch <= 0x18F5) || (0x1900 <= ch && ch <= 0x191E) || (0x1920 <= ch && ch <= 0x192B) || (0x1930 <= ch && ch <= 0x193B) || (0x1946 <= ch && ch <= 0x196D) || (0x1970 <= ch && ch <= 0x1974) || (0x1980 <= ch && ch <= 0x19AB) || (0x19B0 <= ch && ch <= 0x19C9) || (0x19D0 <= ch && ch <= 0x19DA) || (0x1A00 <= ch && ch <= 0x1A1B) || (0x1A20 <= ch && ch <= 0x1A5E) || (0x1A60 <= ch && ch <= 0x1A7C) || (0x1A7F <= ch && ch <= 0x1A89) || (0x1A90 <= ch && ch <= 0x1A99) || (ch == 0x1AA7) || (0x1AB0 <= ch && ch <= 0x1ABD) || (0x1B00 <= ch && ch <= 0x1B4B) || (0x1B50 <= ch && ch <= 0x1B59) || (0x1B6B <= ch && ch <= 0x1B73) || (0x1B80 <= ch && ch <= 0x1BF3) || (0x1C00 <= ch && ch <= 0x1C37) || (0x1C40 <= ch && ch <= 0x1C49) || (0x1C4D <= ch && ch <= 0x1C7D) || (0x1CD0 <= ch && ch <= 0x1CD2) || (0x1CD4 <= ch && ch <= 0x1CF6) || (ch == 0x1CF8) || (ch == 0x1CF9) || (0x1D00 <= ch && ch <= 0x1DF5) || (0x1DFC <= ch && ch <= 0x1F15) || (0x1F18 <= ch && ch <= 0x1F1D) || (0x1F20 <= ch && ch <= 0x1F45) || (0x1F48 <= ch && ch <= 0x1F4D) || (0x1F50 <= ch && ch <= 0x1F57) || (ch == 0x1F59) || (ch == 0x1F5B) || (ch == 0x1F5D) || (0x1F5F <= ch && ch <= 0x1F7D) || (0x1F80 <= ch && ch <= 0x1FB4) || (0x1FB6 <= ch && ch <= 0x1FBC) || (ch == 0x1FBE) || (0x1FC2 <= ch && ch <= 0x1FC4) || (0x1FC6 <= ch && ch <= 0x1FCC) || (0x1FD0 <= ch && ch <= 0x1FD3) || (0x1FD6 <= ch && ch <= 0x1FDB) || (0x1FE0 <= ch && ch <= 0x1FEC) || (0x1FF2 <= ch && ch <= 0x1FF4) || (0x1FF6 <= ch && ch <= 0x1FFC) || (ch == 0x200C) || (ch == 0x200D) || (ch == 0x203F) || (ch == 0x2040) || (ch == 0x2054) || (ch == 0x2071) || (ch == 0x207F) || (0x2090 <= ch && ch <= 0x209C) || (0x20D0 <= ch && ch <= 0x20DC) || (ch == 0x20E1) || (0x20E5 <= ch && ch <= 0x20F0) || (ch == 0x2102) || (ch == 0x2107) || (0x210A <= ch && ch <= 0x2113) || (ch == 0x2115) || (0x2118 <= ch && ch <= 0x211D) || (ch == 0x2124) || (ch == 0x2126) || (ch == 0x2128) || (0x212A <= ch && ch <= 0x2139) || (0x213C <= ch && ch <= 0x213F) || (0x2145 <= ch && ch <= 0x2149) || (ch == 0x214E) || (0x2160 <= ch && ch <= 0x2188) || (0x2C00 <= ch && ch <= 0x2C2E) || (0x2C30 <= ch && ch <= 0x2C5E) || (0x2C60 <= ch && ch <= 0x2CE4) || (0x2CEB <= ch && ch <= 0x2CF3) || (0x2D00 <= ch && ch <= 0x2D25) || (ch == 0x2D27) || (ch == 0x2D2D) || (0x2D30 <= ch && ch <= 0x2D67) || (ch == 0x2D6F) || (0x2D7F <= ch && ch <= 0x2D96) || (0x2DA0 <= ch && ch <= 0x2DA6) || (0x2DA8 <= ch && ch <= 0x2DAE) || (0x2DB0 <= ch && ch <= 0x2DB6) || (0x2DB8 <= ch && ch <= 0x2DBE) || (0x2DC0 <= ch && ch <= 0x2DC6) || (0x2DC8 <= ch && ch <= 0x2DCE) || (0x2DD0 <= ch && ch <= 0x2DD6) || (0x2DD8 <= ch && ch <= 0x2DDE) || (0x2DE0 <= ch && ch <= 0x2DFF) || (0x3005 <= ch && ch <= 0x3007) || (0x3021 <= ch && ch <= 0x302F) || (0x3031 <= ch && ch <= 0x3035) || (0x3038 <= ch && ch <= 0x303C) || (0x3041 <= ch && ch <= 0x3096) || (0x3099 <= ch && ch <= 0x309F) || (0x30A1 <= ch && ch <= 0x30FA) || (0x30FC <= ch && ch <= 0x30FF) || (0x3105 <= ch && ch <= 0x312D) || (0x3131 <= ch && ch <= 0x318E) || (0x31A0 <= ch && ch <= 0x31BA) || (0x31F0 <= ch && ch <= 0x31FF) || (0x3400 <= ch && ch <= 0x4DB5) || (0x4E00 <= ch && ch <= 0x9FCC) || (0xA000 <= ch && ch <= 0xA48C) || (0xA4D0 <= ch && ch <= 0xA4FD) || (0xA500 <= ch && ch <= 0xA60C) || (0xA610 <= ch && ch <= 0xA62B) || (0xA640 <= ch && ch <= 0xA66F) || (0xA674 <= ch && ch <= 0xA67D) || (0xA67F <= ch && ch <= 0xA69D) || (0xA69F <= ch && ch <= 0xA6F1) || (0xA717 <= ch && ch <= 0xA71F) || (0xA722 <= ch && ch <= 0xA788) || (0xA78B <= ch && ch <= 0xA78E) || (0xA790 <= ch && ch <= 0xA7AD) || (ch == 0xA7B0) || (ch == 0xA7B1) || (0xA7F7 <= ch && ch <= 0xA827) || (0xA840 <= ch && ch <= 0xA873) || (0xA880 <= ch && ch <= 0xA8C4) || (0xA8D0 <= ch && ch <= 0xA8D9) || (0xA8E0 <= ch && ch <= 0xA8F7) || (ch == 0xA8FB) || (0xA900 <= ch && ch <= 0xA92D) || (0xA930 <= ch && ch <= 0xA953) || (0xA960 <= ch && ch <= 0xA97C) || (0xA980 <= ch && ch <= 0xA9C0) || (0xA9CF <= ch && ch <= 0xA9D9) || (0xA9E0 <= ch && ch <= 0xA9FE) || (0xAA00 <= ch && ch <= 0xAA36) || (0xAA40 <= ch && ch <= 0xAA4D) || (0xAA50 <= ch && ch <= 0xAA59) || (0xAA60 <= ch && ch <= 0xAA76) || (0xAA7A <= ch && ch <= 0xAAC2) || (0xAADB <= ch && ch <= 0xAADD) || (0xAAE0 <= ch && ch <= 0xAAEF) || (0xAAF2 <= ch && ch <= 0xAAF6) || (0xAB01 <= ch && ch <= 0xAB06) || (0xAB09 <= ch && ch <= 0xAB0E) || (0xAB11 <= ch && ch <= 0xAB16) || (0xAB20 <= ch && ch <= 0xAB26) || (0xAB28 <= ch && ch <= 0xAB2E) || (0xAB30 <= ch && ch <= 0xAB5A) || (0xAB5C <= ch && ch <= 0xAB5F) || (ch == 0xAB64) || (ch == 0xAB65) || (0xABC0 <= ch && ch <= 0xABEA) || (ch == 0xABEC) || (ch == 0xABED) || (0xABF0 <= ch && ch <= 0xABF9) || (0xAC00 <= ch && ch <= 0xD7A3) || (0xD7B0 <= ch && ch <= 0xD7C6) || (0xD7CB <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFA6D) || (0xFA70 <= ch && ch <= 0xFAD9) || (0xFB00 <= ch && ch <= 0xFB06) || (0xFB13 <= ch && ch <= 0xFB17) || (0xFB1D <= ch && ch <= 0xFB28) || (0xFB2A <= ch && ch <= 0xFB36) || (0xFB38 <= ch && ch <= 0xFB3C) || (ch == 0xFB3E) || (ch == 0xFB40) || (ch == 0xFB41) || (ch == 0xFB43) || (ch == 0xFB44) || (0xFB46 <= ch && ch <= 0xFBB1) || (0xFBD3 <= ch && ch <= 0xFD3D) || (0xFD50 <= ch && ch <= 0xFD8F) || (0xFD92 <= ch && ch <= 0xFDC7) || (0xFDF0 <= ch && ch <= 0xFDFB) || (0xFE00 <= ch && ch <= 0xFE0F) || (0xFE20 <= ch && ch <= 0xFE2D) || (ch == 0xFE33) || (ch == 0xFE34) || (0xFE4D <= ch && ch <= 0xFE4F) || (0xFE70 <= ch && ch <= 0xFE74) || (0xFE76 <= ch && ch <= 0xFEFC) || (0xFF10 <= ch && ch <= 0xFF19) || (0xFF21 <= ch && ch <= 0xFF3A) || (ch == 0xFF3F) || (0xFF41 <= ch && ch <= 0xFF5A) || (0xFF66 <= ch && ch <= 0xFFBE) || (0xFFC2 <= ch && ch <= 0xFFC7) || (0xFFCA <= ch && ch <= 0xFFCF) || (0xFFD2 <= ch && ch <= 0xFFD7) || (0xFFDA <= ch && ch <= 0xFFDC);
}

ALWAYS_INLINE bool isIdentifierStart(char32_t ch)
{
    // TODO
    return (ch >= 97 && ch <= 122) // a..z
        || (ch >= 65 && ch <= 90) // A..Z
        || (ch == 36) || (ch == 95) // $ (dollar) and _ (underscore)
        || (ch == 92) // \ (backslash)
        || isIdentifierStartSlow(ch);
}


struct Curly {
    char m_curly[4];
    Curly() { }
    Curly(const char curly[4])
    {
        m_curly[0] = curly[0];
        m_curly[1] = curly[1];
        m_curly[2] = curly[2];
        m_curly[3] = curly[3];
    }
};
/*
struct ParseStatus : public gc {
    Token m_type;
    StringView m_value;
    bool m_octal;
    size_t m_lineNumber;
    size_t m_lineStart;
    size_t m_start;
    size_t m_end;
    int m_prec;

    bool m_head;
    bool m_tail;

    double m_valueNumber;

    StringView m_regexBody;
    StringView m_regexFlag;

    PunctuatorsKind m_punctuatorsKind;
    KeywordKind m_keywordKind;

    void* operator new(size_t, void* p) { return p; }
    void* operator new[](size_t, void* p) { return p; }
    void* operator new(size_t size);
    void operator delete(void* p);
    void* operator new[](size_t size)
    {
        RELEASE_ASSERT_NOT_REACHED();
        return malloc(size);
    }
    void operator delete[](void* p)
    {
        RELEASE_ASSERT_NOT_REACHED();
        return free(p);
    }
};

struct ParseContext {
    ParseContext(String* src, bool strict)
        : m_sourceString(src)
        , m_index(0)
        , m_lineNumber(src->length() > 0 ? 1 : 0)
        , m_lineStart(0)
        , m_startIndex(m_index)
        , m_startLineNumber(m_lineNumber)
        , m_startLineStart(m_lineStart)
        , m_length(src->length())
        , m_allowIn(true)
        , m_allowYield(true)
        , m_inFunctionBody(false)
        , m_inIteration(false)
        , m_inSwitch(false)
        , m_inCatch(false)
        , m_lastCommentStart(-1)
        , m_strict(strict)
        , m_scanning(false)
        , m_isBindingElement(false)
        , m_isAssignmentTarget(false)
        , m_isFunctionIdentifier(false)
        , m_firstCoverInitializedNameError(NULL)
        , m_lookahead(nullptr)
        , m_parenthesizedCount(0)
        , m_stackCounter(0)
        // , m_currentBody(nullptr)
    {
    }

    String* m_sourceString;
    size_t m_index;
    size_t m_lineNumber;
    size_t m_lineStart;
    size_t m_startIndex;
    size_t m_startLineNumber;
    size_t m_startLineStart;
    size_t m_lastIndex;
    size_t m_lastLineNumber;
    size_t m_lastLineStart;
    size_t m_length;
    bool m_allowIn;
    bool m_allowYield;
    std::vector<std::pair<String *, bool>, gc_allocator<std::pair<String *, bool>>> m_labelSet;
    bool m_inFunctionBody;
    bool m_inIteration;
    bool m_inSwitch;
    bool m_inCatch;
    int m_lastCommentStart;
    std::vector<Curly> m_curlyStack;
    bool m_strict;
    bool m_scanning;
    bool m_hasLineTerminator;
    bool m_isBindingElement;
    bool m_isAssignmentTarget;
    bool m_isFunctionIdentifier;
    ParseStatus* m_firstCoverInitializedNameError;
    ParseStatus* m_lookahead;
    int m_parenthesizedCount;
    int m_stackCounter;
    // escargot::StatementNodeVector* m_currentBody;
};

*/


namespace Messages {
const char* UnexpectedToken = "Unexpected token %0";
const char* UnexpectedTokenIllegal = "Unexpected token ILLEGAL";
const char* UnexpectedNumber = "Unexpected number";
const char* UnexpectedString = "Unexpected string";
const char* UnexpectedIdentifier = "Unexpected identifier";
const char* UnexpectedReserved = "Unexpected reserved word";
const char* UnexpectedTemplate = "Unexpected quasi %0";
const char* UnexpectedEOS = "Unexpected end of input";
const char* NewlineAfterThrow = "Illegal newline after throw";
const char* InvalidRegExp = "Invalid regular expression";
const char* UnterminatedRegExp = "Invalid regular expression: missing /";
const char* InvalidLHSInAssignment = "Invalid left-hand side in assignment";
const char* InvalidLHSInForIn = "Invalid left-hand side in for-in";
const char* InvalidLHSInForLoop = "Invalid left-hand side in for-loop";
const char* MultipleDefaultsInSwitch = "More than one default clause in switch statement";
const char* NoCatchOrFinally = "Missing catch or finally after try";
const char* UnknownLabel = "Undefined label \'%0\'";
const char* Redeclaration = "%0 \'%1\' has already been declared";
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
const char* DuplicateBinding = "Duplicate binding %0";
const char* ForInOfLoopInitializer = "%0 loop variable declaration may not have an initializer";
}

struct Config : public gc {
    bool range;
    bool loc;
    String* source;
    bool tokens;
    bool comment;
    bool tolerant;
};


struct Context : public gc {
    bool allowIn;
    bool allowYield;
    // firstCoverInitializedNameError: any;
    bool isAssignmentTarget;
    bool isBindingElement;
    bool inFunctionBody;
    bool inIteration;
    bool inSwitch;
    // labelSet: any;
    bool strict;
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
        : ParserError(index, line, col, new ASCIIString(description))
    {
    }
};

struct ScanTemplteResult {
    UTF16StringData valueCooked;
    StringView raw;
    size_t head;
    size_t tail;
};

struct ScanRegExpResult {
    String* body;
    String* flags;
};

struct ScannerResult : public gc {
    Token type;
    bool octal;
    size_t lineNumber;
    size_t lineStart;
    size_t start;
    size_t end;
    size_t index;

    union {
        StringView valueString;
        PunctuatorsKind punctuatorsKind;
        double valueNumber;
        ScanTemplteResult valueTemplate;
        ScanRegExpResult valueRegexp;
    };

    ScannerResult(Token type, size_t lineNumber, size_t lineStart, size_t start, size_t end)
    {
        this->type = type;
        this->octal = false;
        this->valueString = valueString;
        this->lineNumber = lineNumber;
        this->valueNumber = 0;
        this->lineStart = lineStart;
        this->start = start;
        this->end = this->index = end;
        punctuatorsKind = PunctuatorsKindEnd;
    }

    ScannerResult(Token type, StringView valueString, size_t lineNumber, size_t lineStart, size_t start, size_t end)
    {
        this->type = type;
        this->octal = false;
        this->valueString = valueString;
        this->lineNumber = lineNumber;
        this->valueNumber = 0;
        this->lineStart = lineStart;
        this->start = start;
        this->end = this->index = end;
        punctuatorsKind = PunctuatorsKindEnd;
    }

    ScannerResult(Token type, double value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
    {
        this->type = type;
        this->octal = false;
        this->valueNumber = value;
        this->lineNumber = lineNumber;
        this->lineStart = lineStart;
        this->start = start;
        this->end = this->index = end;
        if (end != SIZE_MAX) {
            this->end = end;
        }
        punctuatorsKind = PunctuatorsKindEnd;
    }

    ScannerResult(Token type, ScanTemplteResult value, size_t lineNumber, size_t lineStart, size_t start, size_t end)
    {
        this->type = type;
        this->octal = false;
        this->valueNumber = 0;
        this->valueTemplate = value;
        this->lineNumber = lineNumber;
        this->lineStart = lineStart;
        this->start = start;
        this->end = this->index = end;
        if (end != SIZE_MAX) {
            this->end = end;
        }
        punctuatorsKind = PunctuatorsKindEnd;
    }
};

class Scanner {
    String* source;
    // errorHandler: ErrorHandler;
    // trackComment: boolean;

    size_t length;
    size_t index;
    size_t lineNumber;
    size_t lineStart;
    std::vector<Curly> curlyStack;

    Scanner(String* code/*, handler: ErrorHandler*/)
    {
        source = code;
        // errorHandler = handler;
        // trackComment = false;

        length = code->length();
        index = 0;
        lineNumber = (code->length() > 0) ? 1 : 0;
        lineStart = 0;
    }

    bool eof()
    {
        return index >= length;
    }

    void throwUnexpectedToken(const char *message = Messages::UnexpectedTokenIllegal)
    {
        throw new ParserError(index, lineNumber, index - lineStart + 1, message);
    }
/*
    tolerateUnexpectedToken() {
        this.errorHandler.tolerateError(this.index, this.lineNumber,
            this.index - this.lineStart + 1, Messages.UnexpectedTokenIllegal);
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
        if (this.trackComment) {
            comments = [];
            start = this.index - offset;
            loc = {
                start: {
                    line: this.lineNumber,
                    column: this.index - this.lineStart - offset
                },
                end: {}
            };
        }*/

        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
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
                if (ch == 13 && this->source->charAt(this->index) == 10) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                // return comments;
                return;
            }
        }

        /*
        if (this.trackComment) {
            loc.end = {
                line: this.lineNumber,
                column: this.index - this.lineStart
            };
            const entry: Comment = {
                multiLine: false,
                slice: [start + offset, this.index],
                range: [start, this.index],
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
        if (this.trackComment) {
            comments = [];
            start = this.index - 2;
            loc = {
                start: {
                    line: this.lineNumber,
                    column: this.index - this.lineStart - 2
                },
                end: {}
            };
        }
         */
        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
            if (isLineTerminator(ch)) {
                if (ch == 0x0D && this->source->charAt(this->index + 1) == 0x0A) {
                    ++this->index;
                }
                ++this->lineNumber;
                ++this->index;
                this->lineStart = this->index;
            } else if (ch == 0x2A) {
                // Block comment ends with '*/'.
                if (this->source->charAt(this->index + 1) == 0x2F) {
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
        if (this.trackComment) {
            loc.end = {
                line: this.lineNumber,
                column: this.index - this.lineStart
            };
            const entry: Comment = {
                multiLine: true,
                slice: [start + 2, this.index],
                range: [start, this.index],
                loc: loc
            };
            comments.push(entry);
        }*/

        tolerateUnexpectedToken();
        // return comments;
        return;
    }

    void scanComments()
    {
        /*
        let comments;
        if (this->trackComment) {
            comments = [];
        }*/

        bool start = (this->index == 0);
        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);

            if (isWhiteSpace(ch)) {
                ++this->index;
            } else if (isLineTerminator(ch)) {
                ++this->index;
                if (ch == 0x0D && this->source->charAt(this->index) == 0x0A) {
                    ++this->index;
                }
                ++this->lineNumber;
                this->lineStart = this->index;
                start = true;
            } else if (ch == 0x2F) { // U+002F is '/'
                ch = this->source->charAt(this->index + 1);
                if (ch == 0x2F) {
                    this->index += 2;
                    /*
                    const comment = this->skipSingleLineComment(2);
                    if (this->trackComment) {
                        comments = comments.concat(comment);
                    }
                    */
                    this->skipSingleLineComment(2);
                    start = true;
                } else if (ch == 0x2A) {  // U+002A is '*'
                    this->index += 2;
                    /*
                    const comment = this->skipMultiLineComment();
                    if (this->trackComment) {
                        comments = comments.concat(comment);
                    }*/
                    this->skipMultiLineComment();
                } else {
                    break;
                }
            } else if (start && ch == 0x2D) { // U+002D is '-'
                // U+003E is '>'
                if ((this->source->charAt(this->index + 1) == 0x2D) && (this->source->charAt(this->index + 2) == 0x3E)) {
                    // '-->' is a single-line comment
                    this->index += 3;
                    /*
                    const comment = this->skipSingleLineComment(3);
                    if (this->trackComment) {
                        comments = comments.concat(comment);
                    }*/
                    this->skipSingleLineComment(3);
                } else {
                    break;
                }
            } else if (ch == 0x3C) { // U+003C is '<'
                // if (this->source.slice(this->index + 1, this->index + 4) == '!--') {
                StringView sv(this->source, this->index + 1, this->index + 4);
                if (sv == "!--") {
                    this->index += 4; // `<!--`
                    /*
                    const comment = this->skipSingleLineComment(4);
                    if (this->trackComment) {
                        comments = comments.concat(comment);
                    }*/
                    this->skipSingleLineComment(4);
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        // return comments;
        return;
    }

    // ECMA-262 11.6.2.2 Future Reserved Words

    bool isFutureReservedWord(const StringView& id)
    {
        if (id.length() == 4 || id.length() == 5) {
            if (id == "enum" || id == "export" || id == "import" || id == "super") {
                return true;
            }
        }
        return false;
    }

    bool isStrictModeReservedWord(const StringView& id)
    {
        if (id == "let") {
            return true;
        } else if (id == "yield") {
            return true;
        } else if (id == "static") {
            return true;
        } else if (id == "public") {
            return true;
        } else if (id == "protected") {
            return true;
        } else if (id == "private") {
            return true;
        } else if (id == "package") {
            return true;
        } else if (id == "implements") {
            return true;
        } else if (id == "interface") {
            return true;
        }
        return false;
    }

    bool isRestrictedWord(const StringView& id)
    {
        return id == "eval" || id == "arguments";
    }

    // ECMA-262 11.6.2.1 Keywords
    ALWAYS_INLINE KeywordKind isKeyword(const StringView& id)
    {
        // 'const' is specialized as Keyword in V8.
        // 'yield' and 'let' are for compatibility with SpiderMonkey and ES.next.
        // Some others are from future reserved words.

        char16_t first = id[0];

        switch (id.length()) {
        case 2:
            if (first == 'i') {
                if (id == "if") {
                    return If;
                } else if (id == "in") {
                    return In;
                }

            } else if (first == 'd' && id == "do") {
                return Do;
            }
            break;
        case 3:
            if (first == 'v' && id == "var") {
                return Var;
            } else if (first == 'f' && id == "for") {
                return For;
            } else if (first == 'n' && id == "new") {
                return New;
            } else if (first == 't' && id == "try") {
                return Try;
                /*
            } else if (first == 'l' && id == "let") {
                return Let;
                */
            }

            break;
        case 4:
            if (first == 't' && id == "this") {
                return This;
            } else if (first == 'e' && id == "else") {
                return Else;
            } else if (first == 'c' && id == "case") {
                return Case;
            } else if (first == 'v' && id == "void") {
                return Void;
            } else if (first == 'w' && id == "with") {
                return With;
            } else if (first == 'e' && id == "enum") {
                return Enum;
            }
            break;
        case 5:
            if (first == 'w' && id == "while") {
                return While;
            } else if (first == 'b' && id == "break") {
                return Break;
            } else if (first == 'c') {
                if (id == "catch") {
                    return Catch;
                } else if (id == "const") {
                    return Const;
                } else if (id == "class") {
                    return Class;
                }
            } else if (first == 't' && id == "throw") {
                return Throw;
                /*
            } else if (first == 'y' && id == "yield") {
                return Yield;
                */
            } else if (first == 's' && id == "super") {
                return Super;
            }
            break;
        case 6:
            if (first == 'r' && id == "return") {
                return Return;
            } else if (first == 't' && id == "typeof") {
                return Typeof;
            } else if (first == 'd' && id == "delete") {
                return Delete;
            } else if (first == 's' && id == "switch") {
                return Switch;
            } else if (first == 'e' && id == "export") {
                return Export;
            } else if (first == 'i' && id == "import") {
                return Import;
            }
            break;
        case 7:
            if (first == 'd' && id == "default") {
                return Default;
            } else if (first == 'f' && id == "finally") {
                return Finally;
            } else if (first == 'e' && id == "extends") {
                return Extends;
            }
            break;
        case 8:
            if (first == 'f' && id == "function") {
                return Function;
            } else if (first == 'c' && id == "continue") {
                return Continue;
            } else if (first == 'd' && id == "debugger") {
                return Debugger;
            }
            break;
        case 10:
            if (first == 'i' && id == "instanceof") {
                return InstanceofKeyword;
            }
            break;
        }

        return NotKeyword;
    }

    char32_t codePointAt(size_t i)
    {
        char32_t cp, first, second;
        cp = this->source->charAt(i);
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            cp = this->source->charAt(i + 1);
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
    }

    char32_t scanHexEscape(char prefix)
    {
        size_t len = (prefix == 'u') ? 4 : 2;
        char32_t code = 0;

        for (size_t i = 0; i < len; ++i) {
            if (!this->eof() && isHexDigit(this->source->charAt(this->index))) {
                code = code * 16 + hexValue(this->source->charAt(this->index++));
            } else {
                return 0;
            }
        }

        return code;
    }

    char32_t scanUnicodeCodePointEscape()
    {
        char16_t ch = this->source->charAt(this->index);
        char32_t code = 0;

        // At least, one hex digit is required.
        if (ch == '}') {
            this->throwUnexpectedToken();
        }

        while (!this->eof()) {
            ch = this->source->charAt(this->index++);
            if (!isHexDigit(ch)) {
                break;
            }
            code = code * 16 + hexValue(ch);
        }

        if (code > 0x10FFFF || ch != '}') {
            this->throwUnexpectedToken();
        }

        return code;
    };

    StringView getIdentifier()
    {
        const size_t start = this->index++;
        while (!this->eof()) {
            const char16_t ch = this->source->charAt(this->index);
            if (ch == 0x5C) {
                // Blackslash (U+005C) marks Unicode escape sequence.
                this->index = start;
                return this->getComplexIdentifier();
            } else if (ch >= 0xD800 && ch < 0xDFFF) {
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
        UTF16StringData id(piece.data);
        this->index += id.length();

        // '\u' (U+005C, U+0075) denotes an escaped character.
        char32_t ch;
        if (cp == 0x5C) {
            if (this->source->charAt(this->index) != 0x75) {
                this->throwUnexpectedToken();
            }
            ++this->index;
            if (this->source->charAt(this->index) == '{') {
                ++this->index;
                ch = this->scanUnicodeCodePointEscape();
            } else {
                ch = this->scanHexEscape('u');
                cp = ch;
                if (!ch || ch == '\\' || !isIdentifierStart(cp)) {
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
            id += piece.data;
            this->index += piece.length;

            // '\u' (U+005C, U+0075) denotes an escaped character.
            if (cp == 0x5C) {
                // id = id.substr(0, id.length - 1);
                id.pop_back();

                if (this->source->charAt(this->index) != 0x75) {
                    this->throwUnexpectedToken();
                }
                ++this->index;
                if (this->source->charAt(this->index) == '{') {
                    ++this->index;
                    ch = this->scanUnicodeCodePointEscape();
                } else {
                    ch = this->scanHexEscape('u');
                    cp = ch;
                    if (!ch || ch == '\\' || !isIdentifierPart(cp)) {
                        this->throwUnexpectedToken();
                    }
                }
                piece = ParserCharPiece(ch);
                id += piece.data;
            }
        }

        String* str = new UTF16String(std::move(id));
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

        if (!this->eof() && isOctalDigit(this->source->charAt(this->index))) {
            octal = true;
            code = code * 8 + octalValue(this->source->charAt(this->index++));

            // 3 digits are only allowed when string starts
            // with 0, 1, 2, 3
            // if ('0123'.indexOf(ch) >= 0 && !this->eof() && Character.isOctalDigit(this->source.charCodeAt(this->index))) {
            if ((ch >= '0' && ch <= '3') && !this->eof() && isOctalDigit(this->source->charAt(this->index))) {
                code = code * 8 + octalValue(this->source->charAt(this->index++));
            }
        }

        return OctalToDecimalResult(code, octal);
    };

    // ECMA-262 11.6 Names and Keywords

    ScannerResult* scanIdentifier()
    {
        Token type;
        const size_t start = this->index;

        // Backslash (U+005C) starts an escaped character.
        StringView id = (this->source->charAt(start) == 0x5C) ? this->getComplexIdentifier() : this->getIdentifier();

        // There is no keyword or literal with only one character.
        // Thus, it must be an identifier.
        if (id.length() == 1) {
            type = Token::IdentifierToken;
        } else if (this->isKeyword(id)) {
            type = Token::KeywordToken;
        } else if (id == "null") {
            type = Token::NullLiteralToken;
        } else if (id == "true" || id == "false") {
            type = Token::BooleanLiteralToken;
        } else {
            type = Token::IdentifierToken;
        }

        return new ScannerResult(type, id, this->lineNumber, this->lineStart, start, this->index);
    }

    // ECMA-262 11.7 Punctuators
    ScannerResult* scanPunctuator()
    {
        ScannerResult* token = new ScannerResult(Token::PunctuatorToken, StringView(), this->lineNumber, this->lineStart, this->index, this->index);

        PunctuatorsKind kind;
        // Check for most common single-character punctuators.
        char16_t ch0 = this->source->charAt(this->index);
        char ch1, ch2, ch3;
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
            if (this->source->charAt(this->index) == '.' && this->source->charAt(this->index + 1) == '.') {
                // Spread operator: ...
                this->index += 2;
                // resultStr = "...";
                kind = PeriodPeriodPeriod;
            }
            break;

        case '}':
            ++this->index;
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '>') {
                ch2 = this->source->charAt(this->index + 2);
                if (ch2 == '>') {
                    ch3 = this->source->charAt(this->index + 3);
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '<') {
                ch2 = this->source->charAt(this->index + 2);
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                ch2 = this->source->charAt(this->index + 2);
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                ch2 = this->source->charAt(this->index + 2);
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
            ch1 = this->source->charAt(this->index + 1);
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
            ch1 = this->source->charAt(this->index + 1);
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                kind = BitwiseXorEqual;
                this->index += 2;
            } else {
                kind = BitwiseXor;
                this->index += 1;
            }
            break;
        case '+':
            ch1 = this->source->charAt(this->index + 1);
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
            ch1 = this->source->charAt(this->index + 1);
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
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                kind = MultiplyEqual;
                this->index += 2;
            } else {
                kind = Multiply;
                this->index += 1;
            }
            break;
        case '/':
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                kind = DivideEqual;
                this->index += 2;
            } else {
                kind = Divide;
                this->index += 1;
            }
            break;
        case '%':
            ch1 = this->source->charAt(this->index + 1);
            if (ch1 == '=') {
                kind = ModEqual;
                this->index += 2;
            } else {
                kind = Mod;
                this->index += 1;
            }
            break;
        default:
            break;
        }

        if (this->index == token->start) {
            this->throwUnexpectedToken();
        }

        token->end = this->index;
        token->punctuatorsKind = kind;
        return token;
    }

    // ECMA-262 11.8.3 Numeric Literals

    ScannerResult* scanHexLiteral(size_t start)
    {
        uint64_t number = 0;
        double numberDouble = 0.0;
        bool shouldUseDouble = false;
        bool scanned = false;

        size_t shiftCount = 0;
        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
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

        if (isIdentifierStart(this->source->charAt(this->index))) {
            this->throwUnexpectedToken();
        }

        double valueNumber;
        if (shouldUseDouble) {
            ASSERT(number == 0);
            valueNumber = numberDouble;
        } else {
            ASSERT(numberDouble == 0.0);
            valueNumber = number;
        }

        return new ScannerResult(Token::NumericLiteralToken, valueNumber, this->lineNumber, this->lineStart, start, this->index);
    }

    ScannerResult* scanBinaryLiteral(size_t start)
    {
        uint64_t number = 0;
        bool scanned = false;

        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
            if (ch != '0' && ch != '1') {
                break;
            }
            number = (number << 1) +  ch - '0';
            this->index++;
            scanned = true;
        }

        if (!scanned) {
            // only 0b or 0B
            this->throwUnexpectedToken();
        }

        if (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
            /* istanbul ignore else */
            if (isIdentifierStart(ch) || isDecimalDigit(ch)) {
                this->throwUnexpectedToken();
            }
        }

        return new ScannerResult(Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index);
    }

    ScannerResult* scanOctalLiteral(char16_t prefix, size_t start)
    {
        uint64_t number = 0;
        bool scanned = false;
        bool octal = isOctalDigit(prefix);

        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
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

        if (isIdentifierStart(this->source->charAt(this->index)) || isDecimalDigit(this->source->charAt(this->index))) {
            throwUnexpectedToken();
        }
        return new ScannerResult(Token::NumericLiteralToken, number, this->lineNumber, this->lineStart, start, this->index);
    }

    bool isImplicitOctalLiteral()
    {
        // Implicit octal, unless there is a non-octal digit.
        // (Annex B.1.1 on Numeric Literals)
        for (size_t i = this->index + 1; i < this->length; ++i) {
            const char16_t ch = this->source->charAt(i);
            if (ch == '8' || ch == '9') {
                return false;
            }
            if (!isOctalDigit(ch)) {
                return true;
            }
        }
        return true;
    }

    ScannerResult* scanNumericLiteral()
    {
        const size_t start = this->index;
        char16_t ch = this->source->charAt(start);
        ASSERT(isDecimalDigit(ch) || (ch == '.'));
            // 'Numeric literal must start with a decimal digit or a decimal point');

        std::string number;
        number.reserve(32);

        if (ch != '.') {
            number = this->source->charAt(this->index++);
            ch = this->source->charAt(this->index);

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

            while (isDecimalDigit(this->source->charAt(this->index))) {
                number += this->source->charAt(this->index++);
            }
            ch = this->source->charAt(this->index);
        }

        if (ch == '.') {
            number += this->source->charAt(this->index++);
            while (isDecimalDigit(this->source->charAt(this->index))) {
                number += this->source->charAt(this->index++);
            }
            ch = this->source->charAt(this->index);
        }

        if (ch == 'e' || ch == 'E') {
            number += this->source->charAt(this->index++);

            ch = this->source->charAt(this->index);
            if (ch == '+' || ch == '-') {
                number += this->source->charAt(this->index++);
            }
            if (isDecimalDigit(this->source->charAt(this->index))) {
                while (isDecimalDigit(this->source->charAt(this->index))) {
                    number += this->source->charAt(this->index++);
                }
            } else {
                this->throwUnexpectedToken();
            }
        }

        if (isIdentifierStart(this->source->charAt(this->index))) {
            this->throwUnexpectedToken();
        }

        int length = number.length();
        int length_dummy;
        double_conversion::StringToDoubleConverter converter(double_conversion::StringToDoubleConverter::ALLOW_HEX
            | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES
            | double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES, 0.0, double_conversion::Double::NaN(),
            "Infinity", "NaN");
        double ll = converter.StringToDouble(number.data(), length, &length_dummy);

        return new ScannerResult(Token::NumericLiteralToken, ll, this->lineNumber, this->lineStart, start, this->index);
    }

    // ECMA-262 11.8.4 String Literals

    ScannerResult* scanStringLiteral()
    {
        // TODO apply rope-string
        const size_t start = this->index;
        char16_t quote = this->source->charAt(start);
        ASSERT((quote == '\'' || quote == '"'));
            // 'String literal must starts with a quote');

        ++this->index;
        bool octal = false;

        bool isPlainCase = true;

        UTF16StringData stringUTF16;
        StringView str;

#define CONVERT_UNPLAIN_CASE_IF_NEEDED() \
        if (isPlainCase) { \
            stringUTF16 = str.toUTF16StringData(); \
            isPlainCase = false; \
        } \

        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index++);

            if (ch == quote) {
                quote = '\0';
                break;
            } else if (ch == '\\') {
                ch = this->source->charAt(this->index++);
                if (!ch || !isLineTerminator(ch)) {
                    CONVERT_UNPLAIN_CASE_IF_NEEDED()
                    switch (ch) {
                        case 'u':
                        case 'x':
                            if (this->source->charAt(this->index) == '{') {
                                ++this->index;
                                ParserCharPiece piece(this->scanUnicodeCodePointEscape());
                                stringUTF16 += piece.data;
                            } else {
                                const char32_t unescaped = this->scanHexEscape(ch);
                                if (!unescaped) {
                                    this->throwUnexpectedToken();
                                }
                                ParserCharPiece piece(unescaped);
                                stringUTF16 += piece.data;
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
                        case '8':
                        case '9':
                            stringUTF16 += ch;
                            this->tolerateUnexpectedToken();
                            break;

                        default:
                            if (ch && isOctalDigit(ch)) {
                                OctalToDecimalResult octToDec = this->octalToDecimal(ch);

                                octal = octToDec.octal || octal;
                                stringUTF16 += octToDec.code;
                            } else {
                                stringUTF16 += ch;
                            }
                            break;
                    }
                } else {
                    ++this->lineNumber;
                    if (ch == '\r' && this->source->charAt(this->index) == '\n') {
                        ++this->index;
                    }
                    this->lineStart = this->index;
                }
            } else if (isLineTerminator(ch)) {
                break;
            } else {
                if (isPlainCase) {
                    str = StringView(this->source, start + 1, start + 1 + str.length() + 1);
                } else {
                    stringUTF16 += ch;
                }
            }
        }

        if (quote != '\0') {
            this->index = start;
            this->throwUnexpectedToken();
        }

        ScannerResult* ret;
        if (isPlainCase) {
            ret = new ScannerResult(Token::StringLiteralToken, str, /*octal, */this->lineNumber, this->lineStart, start, this->index);
        } else {
            String* newStr = new UTF16String(std::move(stringUTF16));
            ret = new ScannerResult(Token::StringLiteralToken, StringView(newStr, 0, newStr->length()), /*octal, */this->lineNumber, this->lineStart, start, this->index);
        }
        ret->octal = octal;
    }

    // ECMA-262 11.8.6 Template Literal Lexical Components

    ScannerResult* scanTemplate()
    {
        // TODO apply rope-string
        UTF16StringData cooked;
        bool terminated = false;
        size_t start = this->index;

        bool head = (this->source->charAt(start) == '`');
        bool tail = false;
        size_t rawOffset = 2;

        ++this->index;

        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index++);
            if (ch == '`') {
                rawOffset = 1;
                tail = true;
                terminated = true;
                break;
            } else if (ch == '$') {
                if (this->source->charAt(this->index) == '{') {
                    this->curlyStack.push_back(Curly("${\0"));
                    ++this->index;
                    terminated = true;
                    break;
                }
                cooked += ch;
            } else if (ch == '\\') {
                ch = this->source->charAt(this->index++);
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
                            if (this->source->charAt(this->index) == '{') {
                                ++this->index;
                                cooked += this->scanUnicodeCodePointEscape();
                            } else {
                                const size_t restore = this->index;
                                const char32_t unescaped = this->scanHexEscape(ch);
                                if (unescaped) {
                                    ParserCharPiece piece(unescaped);
                                    cooked += piece.data;
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
                                if (isDecimalDigit(this->source->charAt(this->index))) {
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
                    if (ch == '\r' && this->source->charAt(this->index) == '\n') {
                        ++this->index;
                    }
                    this->lineStart = this->index;
                }
            } else if (isLineTerminator(ch)) {
                ++this->lineNumber;
                if (ch == '\r' && this->source->charAt(this->index) == '\n') {
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

        ScanTemplteResult result;
        result.head = head;
        result.tail = tail;
        result.raw = StringView(this->source, start + 1, this->index - rawOffset);
        result.valueCooked = std::move(cooked);

        return new ScannerResult(Token::TemplateToken, result, this->lineNumber, this->lineStart, start, this->index);
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
        char16_t ch = this->source->charAt(this->index);
        ASSERT(ch == '/');
        // assert(ch == '/', 'Regular expression literal must start with a slash');

        // TODO apply rope-string
        char16_t ch0 = this->source->charAt(this->index++);
        UTF16StringData str(&ch0, 1);
        bool classMarker = false;
        bool terminated = false;

        while (!this->eof()) {
            ch = this->source->charAt(this->index++);
            str += ch;
            if (ch == '\\') {
                ch = this->source->charAt(this->index++);
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
        return new UTF16String(std::move(str));
    }

    String* scanRegExpFlags()
    {
        // UTF16StringData str = '';
        UTF16StringData flags;
        while (!this->eof()) {
            char16_t ch = this->source->charAt(this->index);
            if (!isIdentifierPart(ch)) {
                break;
            }

            ++this->index;
            if (ch == '\\' && !this->eof()) {
                ch = this->source->charAt(this->index);
                if (ch == 'u') {
                    ++this->index;
                    const size_t restore = this->index;
                    char32_t ch32;
                    ch32 = this->scanHexEscape('u');
                    if (ch32) {
                        ParserCharPiece piece(ch32);
                        flags += piece.data;
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
        return new UTF16String(std::move(flags));
    }

    ScannerResult* scanRegExp()
    {
        const size_t start = this->index;

        String* body = this->scanRegExpBody();
        String* flags = this->scanRegExpFlags();
        // const value = this->testRegExp(body.value, flags.value);

        ScanRegExpResult result;
        result.body = body;
        result.flags = flags;
        ScannerResult* res = new ScannerResult(Token::RegularExpressionToken, StringView(), this->lineNumber, this->lineStart, start, this->index);
        res->valueRegexp = result;
        return res;
    };

    ScannerResult* lex()
    {
        if (this->eof()) {
            return new ScannerResult(Token::EOFToken, this->lineNumber, this->lineStart, this->index, this->index);
        }

        const char16_t cp = this->source->charAt(this->index);

        if (isIdentifierStart(cp)) {
            return this->scanIdentifier();
        }

        // Very common: ( and ) and ;
        if (cp == 0x28 || cp == 0x29 || cp == 0x3B) {
            return this->scanPunctuator();
        }

        // String literal starts with single quote (U+0027) or double quote (U+0022).
        if (cp == 0x27 || cp == 0x22) {
            return this->scanStringLiteral();
        }

        // Dot (.) U+002E can also start a floating-point number, hence the need
        // to check the next character.
        if (cp == 0x2E) {
            if (isDecimalDigit(this->source->charAt(this->index + 1))) {
                return this->scanNumericLiteral();
            }
            return this->scanPunctuator();
        }

        if (isDecimalDigit(cp)) {
            return this->scanNumericLiteral();
        }

        // Template literals start with ` (U+0060) for template head
        // or } (U+007D) for template middle or template tail.
        if (cp == 0x60 || (cp == 0x7D && (strcmp(this->curlyStack.back().m_curly, "${") == 0))) {
            return this->scanTemplate();
        }

        // Possible identifier start in a surrogate pair.
        if (cp >= 0xD800 && cp < 0xDFFF) {
            if (isIdentifierStart(this->codePointAt(this->index))) {
                return this->scanIdentifier();
            }
        }
        return this->scanPunctuator();
    }

};

}

}
