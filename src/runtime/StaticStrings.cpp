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
#include "StaticStrings.h"
#include "Context.h"

namespace Escargot {

void StaticStrings::initStaticStrings(AtomicStringMap* atomicStringMap)
{
    atomicStringMap->insert(String::emptyString);
    NegativeInfinity.init(atomicStringMap, "-Infinity", strlen("-Infinity"));
    stringTrue.init(atomicStringMap, "true", strlen("true"));
    stringFalse.init(atomicStringMap, "false", strlen("false"));
    stringPublic.init(atomicStringMap, "public", strlen("public"));
    stringProtected.init(atomicStringMap, "protected", strlen("protected"));
    stringPrivate.init(atomicStringMap, "private", strlen("private"));
    stringStatic.init(atomicStringMap, "static", strlen("static"));
    stringCatch.init(atomicStringMap, "catch", strlen("catch"));
    stringDelete.init(atomicStringMap, "delete", strlen("delete"));
    stringFor.init(atomicStringMap, "for", strlen("for"));
    stringDefault.init(atomicStringMap, "default", strlen("default"));
    stringThis.init(atomicStringMap, "this", strlen("this"));

    stringIf.init(atomicStringMap, "if", strlen("if"));
    stringIn.init(atomicStringMap, "in", strlen("in"));
    stringDo.init(atomicStringMap, "do", strlen("do"));
    stringVar.init(atomicStringMap, "var", strlen("var"));
    stringNew.init(atomicStringMap, "new", strlen("new"));
    stringTry.init(atomicStringMap, "try", strlen("try"));
    stringElse.init(atomicStringMap, "else", strlen("else"));
    stringCase.init(atomicStringMap, "case", strlen("case"));
    stringVoid.init(atomicStringMap, "void", strlen("void"));
    stringWith.init(atomicStringMap, "with", strlen("with"));
    stringEnum.init(atomicStringMap, "enum", strlen("enum"));
    stringAwait.init(atomicStringMap, "await", strlen("await"));
    stringWhile.init(atomicStringMap, "while", strlen("while"));
    stringBreak.init(atomicStringMap, "break", strlen("break"));
    stringThrow.init(atomicStringMap, "throw", strlen("throw"));
    stringConst.init(atomicStringMap, "const", strlen("const"));
    stringClass.init(atomicStringMap, "class", strlen("class"));
    stringSuper.init(atomicStringMap, "super", strlen("super"));
    stringReturn.init(atomicStringMap, "return", strlen("return"));
    stringTypeof.init(atomicStringMap, "typeof", strlen("typeof"));
    stringSwitch.init(atomicStringMap, "switch", strlen("switch"));
    stringExport.init(atomicStringMap, "export", strlen("export"));
    stringImport.init(atomicStringMap, "import", strlen("import"));
    stringFinally.init(atomicStringMap, "finally", strlen("finally"));
    stringExtends.init(atomicStringMap, "extends", strlen("extends"));
    stringFunction.init(atomicStringMap, "function", strlen("function"));
    stringContinue.init(atomicStringMap, "continue", strlen("continue"));
    stringDebugger.init(atomicStringMap, "debugger", strlen("debugger"));
    stringInstanceof.init(atomicStringMap, "instanceof", strlen("instanceof"));
    stringImplements.init(atomicStringMap, "implements", strlen("implements"));
    stringInterface.init(atomicStringMap, "interface", strlen("interface"));
    stringPackage.init(atomicStringMap, "package", strlen("package"));
    stringYield.init(atomicStringMap, "yield", strlen("yield"));
    stringLet.init(atomicStringMap, "let", strlen("let"));
    stringError.init(atomicStringMap, "error", strlen("error"));

    defaultRegExpString.init(atomicStringMap, "(?:)", strlen("(?:)"));

    get__proto__.init(atomicStringMap, "get __proto__", strlen("get __proto__"));
    set__proto__.init(atomicStringMap, "set __proto__", strlen("set __proto__"));
    getbyteLength.init(atomicStringMap, "get byteLength", strlen("get byteLength"));
    getbyteOffset.init(atomicStringMap, "get byteOffset", strlen("get byteOffset"));
    getLength.init(atomicStringMap, "get length", strlen("get length"));
    getBuffer.init(atomicStringMap, "get buffer", strlen("get buffer"));
    getSize.init(atomicStringMap, "get size", strlen("get size"));
#ifdef ESCARGOT_ENABLE_ES2015
    getFlags.init(atomicStringMap, "get flags", strlen("get flags"));
    getGlobal.init(atomicStringMap, "get global", strlen("get global"));
    getIgnoreCase.init(atomicStringMap, "get ignoreCase", strlen("get ignoreCase"));
    getMultiline.init(atomicStringMap, "get multiline", strlen("get multiline"));
    getSource.init(atomicStringMap, "get source", strlen("get source"));
    getSticky.init(atomicStringMap, "get sticky", strlen("get sticky"));
    getUnicode.init(atomicStringMap, "get unicode", strlen("get unicode"));
#endif
    getSymbolSpecies.init(atomicStringMap, "get [Symbol.species]", strlen("get [Symbol.species]"));
    getSymbolSearch.init(atomicStringMap, "get [Symbol.search]", strlen("get [Symbol.search]"));
    getSymbolToStringTag.init(atomicStringMap, "get [Symbol.toStringTag]", strlen("get [Symbol.toStringTag]"));
    symbolSplit.init(atomicStringMap, "[Symbol.split]", strlen("[Symbol.split]"));
    $Ampersand.init(atomicStringMap, "$&", strlen("$&"));
    $PlusSign.init(atomicStringMap, "$+", strlen("$+"));
    $GraveAccent.init(atomicStringMap, "$`", strlen("$`"));
    $Apostrophe.init(atomicStringMap, "$'", strlen("$'"));

    for (unsigned i = 0; i < ESCARGOT_ASCII_TABLE_MAX; i++) {
        LChar buf[2];
        buf[0] = i;
        buf[1] = 0;
        asciiTable[i].init(atomicStringMap, buf, 1);
    }

    for (unsigned i = 0; i < ESCARGOT_STRINGS_NUMBERS_MAX; i++) {
        ASCIIStringData s = ::Escargot::dtoa(i);
        numbers[i].init(atomicStringMap, s.data(), s.size());
    }

#define INIT_STATIC_STRING(name) name.init(atomicStringMap, #name, strlen(#name));
    FOR_EACH_STATIC_STRING(INIT_STATIC_STRING)
#undef INIT_STATIC_STRING
}

::Escargot::String* StaticStrings::dtoa(double d) const
{
    size_t size = dtoaCache.size();

    for (size_t i = 0; i < size; ++i) {
        if (dtoaCache[i].first == d) {
            return dtoaCache[i].second;
        }
    }

    ::Escargot::String* s = String::fromDouble(d);
    dtoaCache.insert(0, std::make_pair(d, s));
    if (dtoaCache.size() > dtoaCacheSize) {
        dtoaCache.erase(dtoaCache.size() - 1);
    }

    return s;
}
}
