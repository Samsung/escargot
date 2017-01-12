#include "Escargot.h"
#include "RegExpObject.h"
#include "Context.h"
#include "ArrayObject.h"

#include "Yarr.h"

namespace Escargot {

RegExpObject::RegExpObject(ExecutionState& state, const Value& source, const Value& option)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5, true)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    for (size_t i = 0; i < ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5; i++)
        m_values[i] = Value();
    m_structure = state.context()->defaultStructureForRegExpObject();
    setPrototype(state, state.context()->globalObject()->regexpPrototype());
    init(state, source, option);
}

RegExpObject::RegExpObject(ExecutionState& state)
    : RegExpObject(state, String::emptyString, String::emptyString)
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
                builder.appendSubString(patternStr, start, start + i);
                builder.appendChar('\\');
                builder.appendChar('/');

                start = start + i + 1;
                i = 0;
                break;
            }
        }
        if (start + i >= len) {
            if (UNLIKELY(slashFlag)) {
                builder.appendSubString(patternStr, start, start + i);
            }
            break;
        }
    }
    if (!slashFlag)
        return patternStr;
    else
        return builder.finalize();
}

void RegExpObject::init(ExecutionState& state, const Value& source, const Value& option)
{
    String* defaultRegExpString = state.context()->staticStrings().defaultRegExpString.string();

    m_source = (source.isUndefined()) ? defaultRegExpString : source.toString(state);
    m_source = m_source->length() ? m_source : defaultRegExpString;
    m_source = escapeSlashInPattern(m_source);

    m_option = parseOption(state, option.isUndefined() ? String::emptyString : option.toString(state));

    auto entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
    if (entry.m_yarrError)
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid source");

    m_yarrPattern = entry.m_yarrPattern;
    m_bytecodePattern = entry.m_bytecodePattern;
}

RegExpObject::Option RegExpObject::parseOption(ExecutionState& state, const String* optionString)
{
    RegExpObject::Option option = RegExpObject::Option::None;

    for (size_t i = 0; i < optionString->length(); i++) {
        switch (optionString->charAt(i)) {
        case 'g':
            if (option & Option::Global)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'g' flags");
            option = (Option)(option | Option::Global);
            break;
        case 'i':
            if (option & Option::IgnoreCase)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'i' flags");
            option = (Option)(option | Option::IgnoreCase);
            break;
        case 'm':
            if (option & Option::MultiLine)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'm' flags");
            option = (Option)(option | Option::MultiLine);
            break;
        case 'y':
            if (option & Option::Sticky)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'y' flags");
            option = (Option)(option | Option::Sticky);
            break;
        case 'u':
            if (option & Option::Unicode)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'u' flags");
            option = (Option)(option | Option::Unicode);
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid flag");
        }
    }

    return option;
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
    unsigned* outputBuf = ALLOCA(sizeof(unsigned) * 2 * (subPatternNum + 1), unsigned int, state);
    outputBuf[1] = start;
    do {
        start = outputBuf[1];
        memset(outputBuf, -1, sizeof(unsigned) * 2 * (subPatternNum + 1));
        if (start > length)
            break;
        if (str->hasASCIIContent())
            result = JSC::Yarr::interpret(m_bytecodePattern, str->characters8(), length, start, outputBuf);
        else
            result = JSC::Yarr::interpret(m_bytecodePattern, (const UChar*)str->characters16(), length, start, outputBuf);

        if (result != JSC::Yarr::offsetNoMatch) {
            if (UNLIKELY(testOnly)) {
                // outputBuf[1] should be set to lastIndex
                if (isGlobal) {
                    setLastIndex(Value(outputBuf[1]));
                }
                return true;
            }
            std::vector<RegexMatchResult::RegexMatchResultPiece> piece;
            piece.resize(subPatternNum + 1);

            for (unsigned i = 0; i < subPatternNum + 1; i++) {
                RegexMatchResult::RegexMatchResultPiece p;
                p.m_start = outputBuf[i * 2];
                p.m_end = outputBuf[i * 2 + 1];
                piece[i] = p;
            }

            matchResult.m_matchResults.push_back(std::vector<RegexMatchResult::RegexMatchResultPiece>(std::move(piece)));
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
    }
    return matchResult.m_matchResults.size();
}

void RegExpObject::createRegexMatchResult(ExecutionState& state, String* str, RegexMatchResult& result)
{
    size_t len = 0, previousLastIndex = 0;
    bool testResult;
    RegexMatchResult temp;
    temp.m_matchResults.push_back(result.m_matchResults[0]);
    result.m_matchResults.clear();
    do {
        const size_t maximumReasonableMatchSize = 1000000000;
        if (len > maximumReasonableMatchSize) {
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::RangeError, "Maximum Reasonable match size exceeded.");
        }

        if (lastIndex().toIndex(state) == previousLastIndex) {
            setLastIndex(Value(previousLastIndex++));
        } else {
            previousLastIndex = lastIndex().toIndex(state);
        }

        size_t end = temp.m_matchResults[0][0].m_end;
        size_t length = end - temp.m_matchResults[0][0].m_start;
        if (!length) {
            ++end;
        }
        for (size_t i = 0; i < temp.m_matchResults.size(); i++) {
            result.m_matchResults.push_back(temp.m_matchResults[i]);
        }
        len++;
        temp.m_matchResults.clear();
        testResult = matchNonGlobally(state, str, temp, false, end);
    } while (testResult);
}

ArrayObject* RegExpObject::createMatchedArray(ExecutionState& state, String* str, RegexMatchResult& result)
{
    ArrayObject* ret = new ArrayObject(state);
    createRegexMatchResult(state, str, result);
    size_t len = result.m_matchResults.size();
    ret->setThrowsException(state, state.context()->staticStrings().length, Value(len), ret);
    for (size_t idx = 0; idx < len; idx++) {
        ret->defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), ObjectPropertyDescriptor(Value(new StringView(str, result.m_matchResults[idx][0].m_start, result.m_matchResults[idx][0].m_end)), ObjectPropertyDescriptor::AllPresent));
    }
    return ret;
}

ArrayObject* RegExpObject::createRegExpMatchedArray(ExecutionState& state, const RegexMatchResult& result, String* input)
{
    ArrayObject* arr = new ArrayObject(state);

    arr->defineOwnPropertyThrowsException(state, state.context()->staticStrings().index, ObjectPropertyDescriptor(Value(result.m_matchResults[0][0].m_start)));
    arr->defineOwnPropertyThrowsException(state, state.context()->staticStrings().input, ObjectPropertyDescriptor(Value(input)));

    size_t idx = 0;
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            if (result.m_matchResults[i][j].m_start == std::numeric_limits<unsigned>::max()) {
                arr->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(idx++)), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
            } else {
                arr->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(idx++)), ObjectPropertyDescriptor(Value(new StringView(input, result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end)), ObjectPropertyDescriptor::AllPresent));
            }
        }
    }
    return arr;
}

void RegExpObject::pushBackToRegExpMatchedArray(ExecutionState& state, ArrayObject* array, size_t& index, const size_t limit, const RegexMatchResult& result, String* str)
{
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            if (i == 0 && j == 0)
                continue;

            if (std::numeric_limits<unsigned>::max() == result.m_matchResults[i][j].m_start) {
                array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(index++)), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
            } else {
                array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(index++)), ObjectPropertyDescriptor(str->subString(result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end), ObjectPropertyDescriptor::AllPresent));
            }
            if (index == limit)
                return;
        }
    }
}
}
