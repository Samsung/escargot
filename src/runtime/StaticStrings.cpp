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
    defaultRegExpString.init(atomicStringMap, "(?:)", strlen("(?:)"));

    get__proto__.init(atomicStringMap, "get __proto__", strlen("get __proto__"));
    set__proto__.init(atomicStringMap, "set __proto__", strlen("set __proto__"));
    getbyteLength.init(atomicStringMap, "get byteLength", strlen("get byteLength"));
    getbyteOffset.init(atomicStringMap, "get byteOffset", strlen("get byteOffset"));
    getLength.init(atomicStringMap, "get length", strlen("get length"));
    getBuffer.init(atomicStringMap, "get buffer", strlen("get buffer"));
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
    auto iter = dtoaCache.begin();

    while (iter != dtoaCache.end()) {
        if ((*iter).first == d) {
            return (*iter).second;
        }
        iter++;
    }

    ::Escargot::String* s = String::fromDouble(d);
    dtoaCache.push_front(std::make_pair(d, s));
    if (dtoaCache.size() > dtoaCacheSize) {
        dtoaCache.pop_back();
    }
    return s;
}
}
