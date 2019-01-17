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

static NEVER_INLINE bool isIdentifierPartSlow(char32_t ch)
{
    return (ch == 0xAA) || (ch == 0xB5) || (ch == 0xB7) || (ch == 0xBA) || (0xC0 <= ch && ch <= 0xD6) || (0xD8 <= ch && ch <= 0xF6) || (0xF8 <= ch && ch <= 0x02C1) || (0x02C6 <= ch && ch <= 0x02D1) || (0x02E0 <= ch && ch <= 0x02E4) || (ch == 0x02EC) || (ch == 0x02EE) || (0x0300 <= ch && ch <= 0x0374) || (ch == 0x0376) || (ch == 0x0377) || (0x037A <= ch && ch <= 0x037D) || (ch == 0x037F) || (0x0386 <= ch && ch <= 0x038A) || (ch == 0x038C) || (0x038E <= ch && ch <= 0x03A1) || (0x03A3 <= ch && ch <= 0x03F5) || (0x03F7 <= ch && ch <= 0x0481) || (0x0483 <= ch && ch <= 0x0487) || (0x048A <= ch && ch <= 0x052F) || (0x0531 <= ch && ch <= 0x0556) || (ch == 0x0559) || (0x0561 <= ch && ch <= 0x0587) || (0x0591 <= ch && ch <= 0x05BD) || (ch == 0x05BF) || (ch == 0x05C1) || (ch == 0x05C2) || (ch == 0x05C4) || (ch == 0x05C5) || (ch == 0x05C7) || (0x05D0 <= ch && ch <= 0x05EA) || (0x05F0 <= ch && ch <= 0x05F2) || (0x0610 <= ch && ch <= 0x061A) || (0x0620 <= ch && ch <= 0x0669) || (0x066E <= ch && ch <= 0x06D3) || (0x06D5 <= ch && ch <= 0x06DC) || (0x06DF <= ch && ch <= 0x06E8) || (0x06EA <= ch && ch <= 0x06FC) || (ch == 0x06FF) || (0x0710 <= ch && ch <= 0x074A) || (0x074D <= ch && ch <= 0x07B1) || (0x07C0 <= ch && ch <= 0x07F5) || (ch == 0x07FA) || (0x0800 <= ch && ch <= 0x082D) || (0x0840 <= ch && ch <= 0x085B) || (0x08A0 <= ch && ch <= 0x08B2) || (0x08E4 <= ch && ch <= 0x0963) || (0x0966 <= ch && ch <= 0x096F) || (0x0971 <= ch && ch <= 0x0983) || (0x0985 <= ch && ch <= 0x098C) || (ch == 0x098F) || (ch == 0x0990) || (0x0993 <= ch && ch <= 0x09A8) || (0x09AA <= ch && ch <= 0x09B0) || (ch == 0x09B2) || (0x09B6 <= ch && ch <= 0x09B9) || (0x09BC <= ch && ch <= 0x09C4) || (ch == 0x09C7) || (ch == 0x09C8) || (0x09CB <= ch && ch <= 0x09CE) || (ch == 0x09D7) || (ch == 0x09DC) || (ch == 0x09DD) || (0x09DF <= ch && ch <= 0x09E3) || (0x09E6 <= ch && ch <= 0x09F1) || (0x0A01 <= ch && ch <= 0x0A03) || (0x0A05 <= ch && ch <= 0x0A0A) || (ch == 0x0A0F) || (ch == 0x0A10) || (0x0A13 <= ch && ch <= 0x0A28) || (0x0A2A <= ch && ch <= 0x0A30) || (ch == 0x0A32) || (ch == 0x0A33) || (ch == 0x0A35) || (ch == 0x0A36) || (ch == 0x0A38) || (ch == 0x0A39) || (ch == 0x0A3C) || (0x0A3E <= ch && ch <= 0x0A42) || (ch == 0x0A47) || (ch == 0x0A48) || (0x0A4B <= ch && ch <= 0x0A4D) || (ch == 0x0A51) || (0x0A59 <= ch && ch <= 0x0A5C) || (ch == 0x0A5E) || (0x0A66 <= ch && ch <= 0x0A75) || (0x0A81 <= ch && ch <= 0x0A83) || (0x0A85 <= ch && ch <= 0x0A8D) || (0x0A8F <= ch && ch <= 0x0A91) || (0x0A93 <= ch && ch <= 0x0AA8) || (0x0AAA <= ch && ch <= 0x0AB0) || (ch == 0x0AB2) || (ch == 0x0AB3) || (0x0AB5 <= ch && ch <= 0x0AB9) || (0x0ABC <= ch && ch <= 0x0AC5) || (0x0AC7 <= ch && ch <= 0x0AC9) || (0x0ACB <= ch && ch <= 0x0ACD) || (ch == 0x0AD0) || (0x0AE0 <= ch && ch <= 0x0AE3) || (0x0AE6 <= ch && ch <= 0x0AEF) || (0x0B01 <= ch && ch <= 0x0B03) || (0x0B05 <= ch && ch <= 0x0B0C) || (ch == 0x0B0F) || (ch == 0x0B10) || (0x0B13 <= ch && ch <= 0x0B28) || (0x0B2A <= ch && ch <= 0x0B30) || (ch == 0x0B32) || (ch == 0x0B33) || (0x0B35 <= ch && ch <= 0x0B39) || (0x0B3C <= ch && ch <= 0x0B44) || (ch == 0x0B47) || (ch == 0x0B48) || (0x0B4B <= ch && ch <= 0x0B4D) || (ch == 0x0B56) || (ch == 0x0B57) || (ch == 0x0B5C) || (ch == 0x0B5D) || (0x0B5F <= ch && ch <= 0x0B63) || (0x0B66 <= ch && ch <= 0x0B6F) || (ch == 0x0B71) || (ch == 0x0B82) || (ch == 0x0B83) || (0x0B85 <= ch && ch <= 0x0B8A) || (0x0B8E <= ch && ch <= 0x0B90) || (0x0B92 <= ch && ch <= 0x0B95) || (ch == 0x0B99) || (ch == 0x0B9A) || (ch == 0x0B9C) || (ch == 0x0B9E) || (ch == 0x0B9F) || (ch == 0x0BA3) || (ch == 0x0BA4) || (0x0BA8 <= ch && ch <= 0x0BAA) || (0x0BAE <= ch && ch <= 0x0BB9) || (0x0BBE <= ch && ch <= 0x0BC2) || (0x0BC6 <= ch && ch <= 0x0BC8) || (0x0BCA <= ch && ch <= 0x0BCD) || (ch == 0x0BD0) || (ch == 0x0BD7) || (0x0BE6 <= ch && ch <= 0x0BEF) || (0x0C00 <= ch && ch <= 0x0C03) || (0x0C05 <= ch && ch <= 0x0C0C) || (0x0C0E <= ch && ch <= 0x0C10) || (0x0C12 <= ch && ch <= 0x0C28) || (0x0C2A <= ch && ch <= 0x0C39) || (0x0C3D <= ch && ch <= 0x0C44) || (0x0C46 <= ch && ch <= 0x0C48) || (0x0C4A <= ch && ch <= 0x0C4D) || (ch == 0x0C55) || (ch == 0x0C56) || (ch == 0x0C58) || (ch == 0x0C59) || (0x0C60 <= ch && ch <= 0x0C63) || (0x0C66 <= ch && ch <= 0x0C6F) || (0x0C81 <= ch && ch <= 0x0C83) || (0x0C85 <= ch && ch <= 0x0C8C) || (0x0C8E <= ch && ch <= 0x0C90) || (0x0C92 <= ch && ch <= 0x0CA8) || (0x0CAA <= ch && ch <= 0x0CB3) || (0x0CB5 <= ch && ch <= 0x0CB9) || (0x0CBC <= ch && ch <= 0x0CC4) || (0x0CC6 <= ch && ch <= 0x0CC8) || (0x0CCA <= ch && ch <= 0x0CCD) || (ch == 0x0CD5) || (ch == 0x0CD6) || (ch == 0x0CDE) || (0x0CE0 <= ch && ch <= 0x0CE3) || (0x0CE6 <= ch && ch <= 0x0CEF) || (ch == 0x0CF1) || (ch == 0x0CF2) || (0x0D01 <= ch && ch <= 0x0D03) || (0x0D05 <= ch && ch <= 0x0D0C) || (0x0D0E <= ch && ch <= 0x0D10) || (0x0D12 <= ch && ch <= 0x0D3A) || (0x0D3D <= ch && ch <= 0x0D44) || (0x0D46 <= ch && ch <= 0x0D48) || (0x0D4A <= ch && ch <= 0x0D4E) || (ch == 0x0D57) || (0x0D60 <= ch && ch <= 0x0D63) || (0x0D66 <= ch && ch <= 0x0D6F) || (0x0D7A <= ch && ch <= 0x0D7F) || (ch == 0x0D82) || (ch == 0x0D83) || (0x0D85 <= ch && ch <= 0x0D96) || (0x0D9A <= ch && ch <= 0x0DB1) || (0x0DB3 <= ch && ch <= 0x0DBB) || (ch == 0x0DBD) || (0x0DC0 <= ch && ch <= 0x0DC6) || (ch == 0x0DCA) || (0x0DCF <= ch && ch <= 0x0DD4) || (ch == 0x0DD6) || (0x0DD8 <= ch && ch <= 0x0DDF) || (0x0DE6 <= ch && ch <= 0x0DEF) || (ch == 0x0DF2) || (ch == 0x0DF3) || (0x0E01 <= ch && ch <= 0x0E3A) || (0x0E40 <= ch && ch <= 0x0E4E) || (0x0E50 <= ch && ch <= 0x0E59) || (ch == 0x0E81) || (ch == 0x0E82) || (ch == 0x0E84) || (ch == 0x0E87) || (ch == 0x0E88) || (ch == 0x0E8A) || (ch == 0x0E8D) || (0x0E94 <= ch && ch <= 0x0E97) || (0x0E99 <= ch && ch <= 0x0E9F) || (0x0EA1 <= ch && ch <= 0x0EA3) || (ch == 0x0EA5) || (ch == 0x0EA7) || (ch == 0x0EAA) || (ch == 0x0EAB) || (0x0EAD <= ch && ch <= 0x0EB9) || (0x0EBB <= ch && ch <= 0x0EBD) || (0x0EC0 <= ch && ch <= 0x0EC4) || (ch == 0x0EC6) || (0x0EC8 <= ch && ch <= 0x0ECD) || (0x0ED0 <= ch && ch <= 0x0ED9) || (0x0EDC <= ch && ch <= 0x0EDF) || (ch == 0x0F00) || (ch == 0x0F18) || (ch == 0x0F19) || (0x0F20 <= ch && ch <= 0x0F29) || (ch == 0x0F35) || (ch == 0x0F37) || (ch == 0x0F39) || (0x0F3E <= ch && ch <= 0x0F47) || (0x0F49 <= ch && ch <= 0x0F6C) || (0x0F71 <= ch && ch <= 0x0F84) || (0x0F86 <= ch && ch <= 0x0F97) || (0x0F99 <= ch && ch <= 0x0FBC) || (ch == 0x0FC6) || (0x1000 <= ch && ch <= 0x1049) || (0x1050 <= ch && ch <= 0x109D) || (0x10A0 <= ch && ch <= 0x10C5) || (ch == 0x10C7) || (ch == 0x10CD) || (0x10D0 <= ch && ch <= 0x10FA) || (0x10FC <= ch && ch <= 0x1248) || (0x124A <= ch && ch <= 0x124D) || (0x1250 <= ch && ch <= 0x1256) || (ch == 0x1258) || (0x125A <= ch && ch <= 0x125D) || (0x1260 <= ch && ch <= 0x1288) || (0x128A <= ch && ch <= 0x128D) || (0x1290 <= ch && ch <= 0x12B0) || (0x12B2 <= ch && ch <= 0x12B5) || (0x12B8 <= ch && ch <= 0x12BE) || (ch == 0x12C0) || (0x12C2 <= ch && ch <= 0x12C5) || (0x12C8 <= ch && ch <= 0x12D6) || (0x12D8 <= ch && ch <= 0x1310) || (0x1312 <= ch && ch <= 0x1315) || (0x1318 <= ch && ch <= 0x135A) || (0x135D <= ch && ch <= 0x135F) || (0x1369 <= ch && ch <= 0x1371) || (0x1380 <= ch && ch <= 0x138F) || (0x13A0 <= ch && ch <= 0x13F4) || (0x1401 <= ch && ch <= 0x166C) || (0x166F <= ch && ch <= 0x167F) || (0x1681 <= ch && ch <= 0x169A) || (0x16A0 <= ch && ch <= 0x16EA) || (0x16EE <= ch && ch <= 0x16F8) || (0x1700 <= ch && ch <= 0x170C) || (0x170E <= ch && ch <= 0x1714) || (0x1720 <= ch && ch <= 0x1734) || (0x1740 <= ch && ch <= 0x1753) || (0x1760 <= ch && ch <= 0x176C) || (0x176E <= ch && ch <= 0x1770) || (ch == 0x1772) || (ch == 0x1773) || (0x1780 <= ch && ch <= 0x17D3) || (ch == 0x17D7) || (ch == 0x17DC) || (ch == 0x17DD) || (0x17E0 <= ch && ch <= 0x17E9) || (0x180B <= ch && ch <= 0x180D) || (0x1810 <= ch && ch <= 0x1819) || (0x1820 <= ch && ch <= 0x1877) || (0x1880 <= ch && ch <= 0x18AA) || (0x18B0 <= ch && ch <= 0x18F5) || (0x1900 <= ch && ch <= 0x191E) || (0x1920 <= ch && ch <= 0x192B) || (0x1930 <= ch && ch <= 0x193B) || (0x1946 <= ch && ch <= 0x196D) || (0x1970 <= ch && ch <= 0x1974) || (0x1980 <= ch && ch <= 0x19AB) || (0x19B0 <= ch && ch <= 0x19C9) || (0x19D0 <= ch && ch <= 0x19DA) || (0x1A00 <= ch && ch <= 0x1A1B) || (0x1A20 <= ch && ch <= 0x1A5E) || (0x1A60 <= ch && ch <= 0x1A7C) || (0x1A7F <= ch && ch <= 0x1A89) || (0x1A90 <= ch && ch <= 0x1A99) || (ch == 0x1AA7) || (0x1AB0 <= ch && ch <= 0x1ABD) || (0x1B00 <= ch && ch <= 0x1B4B) || (0x1B50 <= ch && ch <= 0x1B59) || (0x1B6B <= ch && ch <= 0x1B73) || (0x1B80 <= ch && ch <= 0x1BF3) || (0x1C00 <= ch && ch <= 0x1C37) || (0x1C40 <= ch && ch <= 0x1C49) || (0x1C4D <= ch && ch <= 0x1C7D) || (0x1CD0 <= ch && ch <= 0x1CD2) || (0x1CD4 <= ch && ch <= 0x1CF6) || (ch == 0x1CF8) || (ch == 0x1CF9) || (0x1D00 <= ch && ch <= 0x1DF5) || (0x1DFC <= ch && ch <= 0x1F15) || (0x1F18 <= ch && ch <= 0x1F1D) || (0x1F20 <= ch && ch <= 0x1F45) || (0x1F48 <= ch && ch <= 0x1F4D) || (0x1F50 <= ch && ch <= 0x1F57) || (ch == 0x1F59) || (ch == 0x1F5B) || (ch == 0x1F5D) || (0x1F5F <= ch && ch <= 0x1F7D) || (0x1F80 <= ch && ch <= 0x1FB4) || (0x1FB6 <= ch && ch <= 0x1FBC) || (ch == 0x1FBE) || (0x1FC2 <= ch && ch <= 0x1FC4) || (0x1FC6 <= ch && ch <= 0x1FCC) || (0x1FD0 <= ch && ch <= 0x1FD3) || (0x1FD6 <= ch && ch <= 0x1FDB) || (0x1FE0 <= ch && ch <= 0x1FEC) || (0x1FF2 <= ch && ch <= 0x1FF4) || (0x1FF6 <= ch && ch <= 0x1FFC) || (ch == 0x200C) || (ch == 0x200D) || (ch == 0x203F) || (ch == 0x2040) || (ch == 0x2054) || (ch == 0x2071) || (ch == 0x207F) || (0x2090 <= ch && ch <= 0x209C) || (0x20D0 <= ch && ch <= 0x20DC) || (ch == 0x20E1) || (0x20E5 <= ch && ch <= 0x20F0) || (ch == 0x2102) || (ch == 0x2107) || (0x210A <= ch && ch <= 0x2113) || (ch == 0x2115) || (0x2118 <= ch && ch <= 0x211D) || (ch == 0x2124) || (ch == 0x2126) || (ch == 0x2128) || (0x212A <= ch && ch <= 0x2139) || (0x213C <= ch && ch <= 0x213F) || (0x2145 <= ch && ch <= 0x2149) || (ch == 0x214E) || (0x2160 <= ch && ch <= 0x2188) || (0x2C00 <= ch && ch <= 0x2C2E) || (0x2C30 <= ch && ch <= 0x2C5E) || (0x2C60 <= ch && ch <= 0x2CE4) || (0x2CEB <= ch && ch <= 0x2CF3) || (0x2D00 <= ch && ch <= 0x2D25) || (ch == 0x2D27) || (ch == 0x2D2D) || (0x2D30 <= ch && ch <= 0x2D67) || (ch == 0x2D6F) || (0x2D7F <= ch && ch <= 0x2D96) || (0x2DA0 <= ch && ch <= 0x2DA6) || (0x2DA8 <= ch && ch <= 0x2DAE) || (0x2DB0 <= ch && ch <= 0x2DB6) || (0x2DB8 <= ch && ch <= 0x2DBE) || (0x2DC0 <= ch && ch <= 0x2DC6) || (0x2DC8 <= ch && ch <= 0x2DCE) || (0x2DD0 <= ch && ch <= 0x2DD6) || (0x2DD8 <= ch && ch <= 0x2DDE) || (0x2DE0 <= ch && ch <= 0x2DFF) || (0x3005 <= ch && ch <= 0x3007) || (0x3021 <= ch && ch <= 0x302F) || (0x3031 <= ch && ch <= 0x3035) || (0x3038 <= ch && ch <= 0x303C) || (0x3041 <= ch && ch <= 0x3096) || (0x3099 <= ch && ch <= 0x309F) || (0x30A1 <= ch && ch <= 0x30FA) || (0x30FC <= ch && ch <= 0x30FF) || (0x3105 <= ch && ch <= 0x312D) || (0x3131 <= ch && ch <= 0x318E) || (0x31A0 <= ch && ch <= 0x31BA) || (0x31F0 <= ch && ch <= 0x31FF) || (0x3400 <= ch && ch <= 0x4DB5) || (0x4E00 <= ch && ch <= 0x9FCC) || (0xA000 <= ch && ch <= 0xA48C) || (0xA4D0 <= ch && ch <= 0xA4FD) || (0xA500 <= ch && ch <= 0xA60C) || (0xA610 <= ch && ch <= 0xA62B) || (0xA640 <= ch && ch <= 0xA66F) || (0xA674 <= ch && ch <= 0xA67D) || (0xA67F <= ch && ch <= 0xA69D) || (0xA69F <= ch && ch <= 0xA6F1) || (0xA717 <= ch && ch <= 0xA71F) || (0xA722 <= ch && ch <= 0xA788) || (0xA78B <= ch && ch <= 0xA78E) || (0xA790 <= ch && ch <= 0xA7AD) || (ch == 0xA7B0) || (ch == 0xA7B1) || (0xA7F7 <= ch && ch <= 0xA827) || (0xA840 <= ch && ch <= 0xA873) || (0xA880 <= ch && ch <= 0xA8C4) || (0xA8D0 <= ch && ch <= 0xA8D9) || (0xA8E0 <= ch && ch <= 0xA8F7) || (ch == 0xA8FB) || (0xA900 <= ch && ch <= 0xA92D) || (0xA930 <= ch && ch <= 0xA953) || (0xA960 <= ch && ch <= 0xA97C) || (0xA980 <= ch && ch <= 0xA9C0) || (0xA9CF <= ch && ch <= 0xA9D9) || (0xA9E0 <= ch && ch <= 0xA9FE) || (0xAA00 <= ch && ch <= 0xAA36) || (0xAA40 <= ch && ch <= 0xAA4D) || (0xAA50 <= ch && ch <= 0xAA59) || (0xAA60 <= ch && ch <= 0xAA76) || (0xAA7A <= ch && ch <= 0xAAC2) || (0xAADB <= ch && ch <= 0xAADD) || (0xAAE0 <= ch && ch <= 0xAAEF) || (0xAAF2 <= ch && ch <= 0xAAF6) || (0xAB01 <= ch && ch <= 0xAB06) || (0xAB09 <= ch && ch <= 0xAB0E) || (0xAB11 <= ch && ch <= 0xAB16) || (0xAB20 <= ch && ch <= 0xAB26) || (0xAB28 <= ch && ch <= 0xAB2E) || (0xAB30 <= ch && ch <= 0xAB5A) || (0xAB5C <= ch && ch <= 0xAB5F) || (ch == 0xAB64) || (ch == 0xAB65) || (0xABC0 <= ch && ch <= 0xABEA) || (ch == 0xABEC) || (ch == 0xABED) || (0xABF0 <= ch && ch <= 0xABF9) || (0xAC00 <= ch && ch <= 0xD7A3) || (0xD7B0 <= ch && ch <= 0xD7C6) || (0xD7CB <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFA6D) || (0xFA70 <= ch && ch <= 0xFAD9) || (0xFB00 <= ch && ch <= 0xFB06) || (0xFB13 <= ch && ch <= 0xFB17) || (0xFB1D <= ch && ch <= 0xFB28) || (0xFB2A <= ch && ch <= 0xFB36) || (0xFB38 <= ch && ch <= 0xFB3C) || (ch == 0xFB3E) || (ch == 0xFB40) || (ch == 0xFB41) || (ch == 0xFB43) || (ch == 0xFB44) || (0xFB46 <= ch && ch <= 0xFBB1) || (0xFBD3 <= ch && ch <= 0xFD3D) || (0xFD50 <= ch && ch <= 0xFD8F) || (0xFD92 <= ch && ch <= 0xFDC7) || (0xFDF0 <= ch && ch <= 0xFDFB) || (0xFE00 <= ch && ch <= 0xFE0F) || (0xFE20 <= ch && ch <= 0xFE2D) || (ch == 0xFE33) || (ch == 0xFE34) || (0xFE4D <= ch && ch <= 0xFE4F) || (0xFE70 <= ch && ch <= 0xFE74) || (0xFE76 <= ch && ch <= 0xFEFC) || (0xFF10 <= ch && ch <= 0xFF19) || (0xFF21 <= ch && ch <= 0xFF3A) || (ch == 0xFF3F) || (0xFF41 <= ch && ch <= 0xFF5A) || (0xFF66 <= ch && ch <= 0xFFBE) || (0xFFC2 <= ch && ch <= 0xFFC7) || (0xFFCA <= ch && ch <= 0xFFCF) || (0xFFD2 <= ch && ch <= 0xFFD7) || (0xFFDA <= ch && ch <= 0xFFDC);
}

static ALWAYS_INLINE bool isIdentifierPart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return esprima::g_asciiRangeCharMap[ch] & ESPRIMA_IS_IDENT;
    }

    return isIdentifierPartSlow(ch);
}

static ALWAYS_INLINE bool isIdentifierStart(char32_t ch)
{
    if (LIKELY(ch < 128)) {
        return esprima::g_asciiRangeCharMap[ch] & ESPRIMA_START_IDENT;
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
static ALWAYS_INLINE ParserCharPiece fromCodePoint(char32_t cp)
{
    if (cp < 0x10000) {
        return ParserCharPiece((char16_t)cp);
    } else {
        return ParserCharPiece((char16_t)(0xD800 + ((cp - 0x10000) >> 10)), (char16_t)(0xDC00 + ((cp - 0x10000) & 1023)));
    }
}

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

ScannerResult::~ScannerResult()
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
    if (this->plain) {
        return new SourceStringView(sv);
    }
    RELEASE_ASSERT(sv.string() != nullptr);
    return sv.string();
}

StringView ScannerResult::valueStringLiteral()
{
    if (this->type == Token::KeywordToken && !this->hasKeywordButUseString) {
        AtomicString as = keywordToString(this->scanner->escargotContext, this->valueKeywordKind);
        return StringView(as.string(), 0, as.string()->length());
    }
    if (this->type == Token::StringLiteralToken && !plain && valueStringLiteralData.length() == 0) {
        consturctStringLiteral();
    }
    ASSERT(valueStringLiteralData.getTagInFirstDataArea() == POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
    return valueStringLiteralData;
}

void ScannerResult::consturctStringLiteral()
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
    bool isEveryCharAllLatin1 = true;

    UTF16StringDataNonGCStd stringUTF16;
    while (true) {
        char16_t ch = this->scanner->source.bufferedCharAt(this->scanner->index++);

        if (ch == quote) {
            quote = '\0';
            break;
        } else if (UNLIKELY(ch == '\\')) {
            ch = this->scanner->source.bufferedCharAt(this->scanner->index++);
            if (!ch || !esprima::isLineTerminator(ch)) {
                switch (ch) {
                case 'u':
                case 'x':
                    if (this->scanner->source.bufferedCharAt(this->scanner->index) == '{') {
                        ++this->scanner->index;
                        ParserCharPiece piece(this->scanner->scanUnicodeCodePointEscape());
                        stringUTF16.append(piece.data, piece.data + piece.length);
                        if (piece.length != 1 || piece.data[0] >= 256) {
                            isEveryCharAllLatin1 = false;
                        }
                    } else {
                        auto res = this->scanner->scanHexEscape(ch);
                        const char32_t unescaped = res.code;
                        ParserCharPiece piece(unescaped);
                        stringUTF16.append(piece.data, piece.data + piece.length);
                        if (piece.length != 1 || piece.data[0] >= 256) {
                            isEveryCharAllLatin1 = false;
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
                        auto octToDec = this->scanner->octalToDecimal(ch);

                        stringUTF16 += octToDec.code;
                        if (octToDec.code >= 256) {
                            isEveryCharAllLatin1 = false;
                        }
                    } else if (isDecimalDigit(ch)) {
                        stringUTF16 += ch;
                        if (ch >= 256) {
                            isEveryCharAllLatin1 = false;
                        }
                    } else {
                        stringUTF16 += ch;
                        if (ch >= 256) {
                            isEveryCharAllLatin1 = false;
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
        } else if (UNLIKELY(esprima::isLineTerminator(ch))) {
            break;
        } else {
            stringUTF16 += ch;
            if (ch >= 256) {
                isEveryCharAllLatin1 = false;
            }
        }
    }

    this->scanner->index = indexBackup;
    this->scanner->lineNumber = lineNumberBackup;
    this->scanner->lineStart = lineStartBackup;

    String* newStr;
    if (isEveryCharAllLatin1) {
        newStr = new Latin1String(stringUTF16.data(), stringUTF16.length());
    } else {
        newStr = new UTF16String(stringUTF16.data(), stringUTF16.length());
    }
    this->valueStringLiteralData = StringView(newStr, 0, newStr->length());
}

Scanner::Scanner(::Escargot::Context* escargotContext, StringView code, ErrorHandler* handler, size_t startLine, size_t startColumn)
{
    this->escargotContext = escargotContext;
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

ScannerResult* Scanner::createScannerResult()
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

void Scanner::skipSingleLineComment(void)
{
    while (!this->eof()) {
        char16_t ch = this->source.bufferedCharAt(this->index);
        ++this->index;

        if (esprima::isLineTerminator(ch)) {
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
        if (esprima::isLineTerminator(ch)) {
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

Scanner::CharOrEmptyResult Scanner::scanHexEscape(char prefix)
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

Scanner::OctalToDecimalResult Scanner::octalToDecimal(char16_t ch)
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

PassRefPtr<ScannerResult> Scanner::scanPunctuator(char16_t ch0)
{
    PassRefPtr<ScannerResult> token = adoptRef(new (createScannerResult()) ScannerResult(this, Token::PunctuatorToken, this->lineNumber, this->lineStart, this->index, this->index));

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
        kind = PunctuatorKindEnd;
        break;
    }

    if (UNLIKELY(this->index == token->start)) {
        this->throwUnexpectedToken();
    }

    token->valuePunctuatorKind = kind;
    return token;
}

PassRefPtr<ScannerResult> Scanner::scanHexLiteral(size_t start)
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

PassRefPtr<ScannerResult> Scanner::scanBinaryLiteral(size_t start)
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

PassRefPtr<ScannerResult> Scanner::scanOctalLiteral(char16_t prefix, size_t start)
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

PassRefPtr<ScannerResult> Scanner::scanNumericLiteral()
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

PassRefPtr<ScannerResult> Scanner::scanStringLiteral()
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
            if (!ch || !esprima::isLineTerminator(ch)) {
                switch (ch) {
                case 'u':
                case 'x':
                    if (this->source.bufferedCharAt(this->index) == '{') {
                        ++this->index;
                        this->scanUnicodeCodePointEscape();
                    } else {
                        CharOrEmptyResult res = this->scanHexEscape(ch);
                        if (res.isEmpty) {
                            this->throwUnexpectedToken();
                        }
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
                        OctalToDecimalResult octToDec = this->octalToDecimal(ch);
                        octal = octToDec.octal || octal;
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
        } else if (UNLIKELY(esprima::isLineTerminator(ch))) {
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
        auto ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::StringLiteralToken, str, this->lineNumber, this->lineStart, start, this->index, true));
        ret->octal = octal;
        ret->plain = true;
        return ret;
    } else {
        // build string if needs
        auto ret = adoptRef(new (createScannerResult()) ScannerResult(this, Token::StringLiteralToken, StringView(), this->lineNumber, this->lineStart, start, this->index, false));
        ret->octal = octal;
        ret->plain = false;
        return ret;
    }
}

PassRefPtr<ScannerResult> Scanner::scanTemplate()
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
            if (!esprima::isLineTerminator(ch)) {
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
        } else if (esprima::isLineTerminator(ch)) {
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
            if (esprima::isLineTerminator(ch)) {
                this->throwUnexpectedToken(Messages::UnterminatedRegExp);
            }
            str += ch;
        } else if (esprima::isLineTerminator(ch)) {
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

PassRefPtr<ScannerResult> Scanner::scanRegExp()
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
}

PassRefPtr<ScannerResult> Scanner::lex()
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
}
