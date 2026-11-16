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
#include "StaticStrings.h"
#include "Context.h"

namespace Escargot {

void StaticStrings::initStaticStringImpl(AtomicString& as, const char* str, size_t len)
{
    as.initStaticString(m_atomicStringMap, new ASCIIStringFromExternalMemory(str, len));
}

void StaticStrings::initStaticStrings()
{
    AtomicStringMap* atomicStringMap = m_atomicStringMap;
    atomicStringMap->insert(String::emptyString());

#define INIT_STATIC_STRING(name) initStaticStringImpl(name, #name, sizeof(#name) - 1);
    FOR_EACH_STATIC_STRING(INIT_STATIC_STRING)
    FOR_EACH_STATIC_THREADING_STRING(INIT_STATIC_STRING)
#undef INIT_STATIC_STRING

#define INIT_STATIC_STRING(atomicString, name) initStaticStringImpl(atomicString, name, sizeof(name) - 1)

    INIT_STATIC_STRING(stringBreak, "break");
    INIT_STATIC_STRING(stringCase, "case");
    INIT_STATIC_STRING(stringCatch, "catch");
    INIT_STATIC_STRING(stringClass, "class");
    INIT_STATIC_STRING(stringConst, "const");
    INIT_STATIC_STRING(stringContinue, "continue");
    INIT_STATIC_STRING(stringDefault, "default");
    INIT_STATIC_STRING(stringDelete, "delete");
    INIT_STATIC_STRING(stringDo, "do");
    INIT_STATIC_STRING(stringElse, "else");
    INIT_STATIC_STRING(stringEnum, "enum");
    INIT_STATIC_STRING(stringExport, "export");
    INIT_STATIC_STRING(stringFalse, "false");
    INIT_STATIC_STRING(stringFor, "for");
    INIT_STATIC_STRING(stringIf, "if");
    INIT_STATIC_STRING(stringImport, "import");
    INIT_STATIC_STRING(stringIn, "in");
    INIT_STATIC_STRING(stringMutable, "mutable");
    INIT_STATIC_STRING(stringNew, "new");
    INIT_STATIC_STRING(stringPrivate, "private");
    INIT_STATIC_STRING(stringProtected, "protected");
    INIT_STATIC_STRING(stringPublic, "public");
    INIT_STATIC_STRING(stringReturn, "return");
    INIT_STATIC_STRING(stringStarDefaultStar, "*default*");
    INIT_STATIC_STRING(stringStarNamespaceStar, "*namespace*");
    INIT_STATIC_STRING(stringStatic, "static");
    INIT_STATIC_STRING(stringSwitch, "switch");
    INIT_STATIC_STRING(stringThis, "this");
    INIT_STATIC_STRING(stringThrow, "throw");
    INIT_STATIC_STRING(stringTrue, "true");
    INIT_STATIC_STRING(stringTry, "try");
    INIT_STATIC_STRING(stringTypeof, "typeof");
    INIT_STATIC_STRING(stringUsing, "using");
    INIT_STATIC_STRING(stringVoid, "void");
    INIT_STATIC_STRING(stringWhile, "while");

    INIT_STATIC_STRING(NegativeInfinity, "-Infinity");
    INIT_STATIC_STRING(defaultRegExpString, "(?:)");
    INIT_STATIC_STRING(getBuffer, "get buffer");
    INIT_STATIC_STRING(getLength, "get length");
    INIT_STATIC_STRING(getSymbolSpecies, "get [Symbol.species]");
    INIT_STATIC_STRING(getSymbolToStringTag, "get [Symbol.toStringTag]");
    INIT_STATIC_STRING(getbyteLength, "get byteLength");
    INIT_STATIC_STRING(getbyteOffset, "get byteOffset");
    INIT_STATIC_STRING(getmaxByteLength, "get maxByteLength");
    INIT_STATIC_STRING(getdisposed, "get disposed");
    INIT_STATIC_STRING(symbolIterator, "[Symbol.iterator]");

#undef INIT_STATIC_STRING

#define INIT_STATIC_NUMBER(num) numbers[num].initStaticString(atomicStringMap, new ASCIIString(#num, sizeof(#num) - 1));
    FOR_EACH_STATIC_NUMBER(INIT_STATIC_NUMBER)
#undef INIT_STATIC_NUMBER

    for (unsigned i = 0; i < ESCARGOT_ASCII_TABLE_MAX / 2; i++) {
        const char c = static_cast<char>(i);
        asciiTable[i].init(atomicStringMap, &c, 1);
    }

    for (unsigned i = ESCARGOT_ASCII_TABLE_MAX / 2; i < ESCARGOT_ASCII_TABLE_MAX; i++) {
        LChar buf[2];
        buf[0] = i;
        buf[1] = 0;
        asciiTable[i].initStaticString(atomicStringMap, new Latin1String(buf, 1));
    }

#define DECLARE_LAZY_STATIC_STRING(Name, unused) m_lazy##Name = AtomicString();
    FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_WASM_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING
}

#define DECLARE_LAZY_STATIC_STRING(Name, stringContent)                                                                                 \
    AtomicString StaticStrings::lazy##Name()                                                                                            \
    {                                                                                                                                   \
        if (UNLIKELY(m_lazy##Name.string() == String::emptyString())) {                                                                 \
            m_lazy##Name = AtomicString(m_atomicStringMap, stringContent, sizeof(stringContent) - 1, AtomicString::FromExternalMemory); \
        }                                                                                                                               \
        return m_lazy##Name;                                                                                                            \
    }
FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
FOR_EACH_LAZY_WASM_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING

} // namespace Escargot
