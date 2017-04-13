/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    defaultRegExpString.init(atomicStringMap, "(?:)", strlen("(?:)"));

    get__proto__.init(atomicStringMap, "get __proto__", strlen("get __proto__"));
    set__proto__.init(atomicStringMap, "set __proto__", strlen("set __proto__"));
    getbyteLength.init(atomicStringMap, "get byteLength", strlen("get byteLength"));
    getbyteOffset.init(atomicStringMap, "get byteOffset", strlen("get byteOffset"));
    getLength.init(atomicStringMap, "get length", strlen("get length"));
    getBuffer.init(atomicStringMap, "get buffer", strlen("get buffer"));

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
