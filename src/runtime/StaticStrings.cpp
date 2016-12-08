#include "Escargot.h"
#include "StaticStrings.h"
#include "Context.h"

namespace Escargot {

void StaticStrings::initStaticStrings(AtomicStringMap* atomicStringMap)
{
    NegativeInfinity.init(atomicStringMap, "-Infinity", strlen("-Infinity"));
    stringTrue.init(atomicStringMap, "true", strlen("true"));
    stringFalse.init(atomicStringMap, "false", strlen("false"));
    stringPublic.init(atomicStringMap, "public", strlen("public"));
    stringProtected.init(atomicStringMap, "protected", strlen("protected"));
    stringPrivate.init(atomicStringMap, "private", strlen("private"));
    stringStatic.init(atomicStringMap, "static", strlen("static"));
    stringCatch.init(atomicStringMap, "catch", strlen("catch"));
    defaultRegExpString.init(atomicStringMap, "(?:)", strlen("(?:)"));

    for (unsigned i = 0; i < ESCARGOT_ASCII_TABLE_MAX; i++) {
        char buf[2];
        buf[0] = i;
        buf[1] = 0;
        asciiTable[i].init(atomicStringMap, buf, 1);
    }

    for (unsigned i = 0; i < ESCARGOT_STRINGS_NUMBERS_MAX; i++) {
        ASCIIStringData s = dtoa(i);
        numbers[i].init(atomicStringMap, s.data(), s.size());
    }

#define INIT_STATIC_STRING(name) name.init(atomicStringMap, #name, strlen(#name));
    FOR_EACH_STATIC_STRING(INIT_STATIC_STRING)
#undef INIT_STATIC_STRING
}
}
