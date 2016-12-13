#include "Escargot.h"
#include "RegExpObject.h"
#include "Context.h"
#include "Yarr.h"

namespace Escargot {

RegExpObject::RegExpObject(ExecutionState& state, String* source, const Option& option)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5, true)
    , m_source(source)
    , m_option(option)
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    m_structure = state.context()->defaultStructureForRegExpObject();
    setPrototype(state, state.context()->globalObject()->regexpPrototype());
    setSource(state, m_source);
}

RegExpObject::RegExpObject(ExecutionState& state)
    : RegExpObject(state, NULL, Option::None)
{
}

String* escapeSlashInPattern(String* patternStr)
{
    if (patternStr->length() == 0)
        return patternStr;

    size_t len = patternStr->length();
    bool slashFlag = false;
    size_t i, start = 0;
    StringBuilder builder;
    while (true) {
        for (i = 0; start + i < len; i++) {
            if (UNLIKELY(patternStr->charAt(start + i) == '/' && (i == 0 || patternStr->charAt(start + i - 1) != '\\'))) {
                slashFlag = true;
                builder.appendSubString(patternStr, start, i);
                if (patternStr->isASCIIString()) {
                    builder.appendString("\\/");
                } else {
                    builder.appendChar('\\');
                    builder.appendChar('/');
                }

                start = start + i + 1;
                i = 0;
                break;
            }
        }
        if (start + i >= len) {
            if (UNLIKELY(slashFlag)) {
                builder.appendSubString(patternStr, start, i);
            }
            break;
        }
    }
    if (!slashFlag)
        return patternStr;
    else
        return builder.finalize();
}

void RegExpObject::setSource(ExecutionState& state, String* src)
{
    String* escapedSrc = escapeSlashInPattern(src);
    auto entry = getCacheEntryAndCompileIfNeeded(state, escapedSrc, m_option);
    if (entry.m_yarrError)
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid source");

    m_source = escapedSrc;
    m_yarrPattern = entry.m_yarrPattern;
    m_bytecodePattern = entry.m_bytecodePattern;
}

RegExpObject::Option RegExpObject::parseOption(ExecutionState& state, String* optionString)
{
    RegExpObject::Option option = RegExpObject::Option::None;

    for (size_t i = 0; i < optionString->length(); i++) {
        switch (optionString->charAt(i)) {
        case 'g':
            if (option & Option::Global)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'g' flags");
            option = (RegExpObject::Option)(option | Option::Global);
            break;
        case 'i':
            if (option & RegExpObject::Option::IgnoreCase)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'i' flags");
            option = (RegExpObject::Option)(option | RegExpObject::Option::IgnoreCase);
            break;
        case 'm':
            if (option & RegExpObject::Option::MultiLine)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'm' flags");
            option = (RegExpObject::Option)(option | RegExpObject::Option::MultiLine);
            break;
        /*
        case 'y':
            if (option & ESRegExpObject::Option::Sticky)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'y' flags");
            option = (ESRegExpObject::Option) (option | RegExpObject::Option::Sticky);
            break;
        */
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid flag");
        }
    }

    return option;
}

void RegExpObject::setOption(ExecutionState& state, String* optionStr)
{
    m_option = parseOption(state, optionStr);
}

void RegExpObject::setOption(const Option& option)
{
    if (((m_option & Option::MultiLine) != (option & Option::MultiLine))
        || ((m_option & Option::IgnoreCase) != (option & Option::IgnoreCase))) {
        ASSERT(!m_yarrPattern);
        m_bytecodePattern = NULL;
    }
    m_option = option;
}

void RegExpObject::setLastIndex(const Value& lastIndex)
{
    m_lastIndex = lastIndex;
}

RegExpObject::RegExpCacheEntry& RegExpObject::getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option)
{
    auto cache = state.context()->regexpCache();
    auto it = cache->find(RegExpCacheKey(source, option));
    if (it != cache->end()) {
        return it->second;
    } else {
        if (cache->size() > 256) {
            cache->clear();
        }

        const char* yarrError = nullptr;
        auto yarrPattern = new (PointerFreeGC) JSC::Yarr::YarrPattern(*source, option & Option::IgnoreCase, option & Option::MultiLine, &yarrError);
        return cache->insert(std::make_pair(RegExpCacheKey(source, option), RegExpCacheEntry(yarrError, yarrPattern))).first->second;
    }
}

bool RegExpObject::matchNonGlobally(ExecutionState& state, String* str, RegexMatchResult& matchResult, bool testOnly, size_t startIndex)
{
    Option prevOption = option();
    setOption((Option)(prevOption & ~Option::Global));
    bool ret = match(state, str, matchResult, testOnly, startIndex);
    setOption(prevOption);
    return ret;
}

bool RegExpObject::match(ExecutionState& state, String* str, RegexMatchResult& matchResult, bool testOnly, size_t startIndex)
{
    m_lastExecutedString = str;

    if (!m_bytecodePattern) {
        RegExpCacheEntry& entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
        if (entry.m_yarrError) {
            matchResult.m_subPatternNum = 0;
            return false;
        }
        m_yarrPattern = entry.m_yarrPattern;

        if (entry.m_bytecodePattern) {
            m_bytecodePattern = entry.m_bytecodePattern;
        } else {
            WTF::BumpPointerAllocator* bumpAlloc = state.context()->bumpPointerAllocator();
            JSC::Yarr::OwnPtr<JSC::Yarr::BytecodePattern> ownedBytecode = JSC::Yarr::byteCompile(*m_yarrPattern, bumpAlloc);
            m_bytecodePattern = ownedBytecode.leakPtr();
            entry.m_bytecodePattern = m_bytecodePattern;
        }
    }

    unsigned subPatternNum = m_bytecodePattern->m_body->m_numSubpatterns;
    matchResult.m_subPatternNum = (int)subPatternNum;
    size_t length = str->length();
    size_t start = startIndex;
    unsigned result = 0;
    bool isGlobal = option() & RegExpObject::Option::Global;
    unsigned* outputBuf;
    outputBuf = (unsigned int*)GC_MALLOC_ATOMIC(sizeof(unsigned) * 2 * (subPatternNum + 1));
    // TODO
    //    ALLOCA_WRAPPER(ESVMInstance::currentInstance(), outputBuf, unsigned int*, sizeof(unsigned) * 2 * (subPatternNum + 1), true);
    outputBuf[1] = start;
    do {
        start = outputBuf[1];
        memset(outputBuf, -1, sizeof(unsigned) * 2 * (subPatternNum + 1));
        if (start > length)
            break;
        if (str->isASCIIString())
            result = JSC::Yarr::interpret(m_bytecodePattern, (const char*)str->toUTF8StringData().data(), length, start, outputBuf);
        else
            result = JSC::Yarr::interpret(m_bytecodePattern, (const UChar*)str->toUTF16StringData().data(), length, start, outputBuf);
        if (result != JSC::Yarr::offsetNoMatch) {
            if (UNLIKELY(testOnly)) {
                // outputBuf[1] should be set to lastIndex
                if (isGlobal) {
                    setLastIndex(Value(outputBuf[1]));
                    //                   set(strings->lastIndex, Value(outputBuf[1]), true);
                }
                return true;
            }
            std::vector<RegexMatchResult::RegexMatchResultPiece, gc_malloc_pointer_free_allocator<RegexMatchResult::RegexMatchResultPiece> > piece;
            piece.reserve(subPatternNum + 1);

            for (unsigned i = 0; i < subPatternNum + 1; i++) {
                RegexMatchResult::RegexMatchResultPiece p;
                p.m_start = outputBuf[i * 2];
                p.m_end = outputBuf[i * 2 + 1];
                piece.push_back(p);
            }
            matchResult.m_matchResults.push_back(std::move(piece));
            if (!isGlobal)
                break;
            if (start == outputBuf[1]) {
                outputBuf[1]++;
                if (outputBuf[1] > length) {
                    break;
                }
            }
        } else {
            break;
        }
    } while (result != JSC::Yarr::offsetNoMatch);
    if (UNLIKELY(testOnly)) {
        setLastIndex(Value(0));
        //        set(strings->lastIndex, ESValue(0), true);
    }
    return matchResult.m_matchResults.size();
}
}
