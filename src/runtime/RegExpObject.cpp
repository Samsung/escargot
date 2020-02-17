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
#include "RegExpObject.h"
#include "Context.h"
#include "ArrayObject.h"
#include "VMInstance.h"

#include "WTFBridge.h"
#include "Yarr.h"
#include "YarrPattern.h"
#include "YarrInterpreter.h"

namespace Escargot {

RegExpObject::RegExpObject(ExecutionState& state, String* source, String* option)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5, true)
    , m_source(NULL)
    , m_optionString(NULL)
    , m_option(None)
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    initRegExpObject(state, true);
    init(state, source, option);
}

RegExpObject::RegExpObject(ExecutionState& state, String* source, unsigned int option)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5, true)
    , m_source(NULL)
    , m_optionString(NULL)
    , m_option(None)
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    initRegExpObject(state, true);
    initWithOption(state, source, (Option)option);
}

RegExpObject::RegExpObject(ExecutionState& state, bool hasLastIndex)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + (hasLastIndex ? 5 : 4), true)
    , m_source(NULL)
    , m_optionString(NULL)
    , m_option(None)
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    initRegExpObject(state, hasLastIndex);
    init(state, String::emptyString, String::emptyString);
}

void RegExpObject::initRegExpObject(ExecutionState& state, bool hasLastIndex)
{
    if (hasLastIndex) {
        for (size_t i = 0; i < ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5; i++)
            m_values[i] = Value();
        m_structure = state.context()->defaultStructureForRegExpObject();
        setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->regexpPrototype());
    } else {
        for (size_t i = 0; i < ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 4; i++)
            m_values[i] = Value();
        m_structure = state.context()->defaultStructureForObject();
        setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->regexpPrototype());
    }
}

void* RegExpObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(RegExpObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_source));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_optionString));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_yarrPattern));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_bytecodePattern));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_lastIndex));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_lastExecutedString));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(RegExpObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

static String* escapeSlashInPattern(String* patternStr)
{
    if (patternStr->length() == 0) {
        return patternStr;
    }

    auto accessData = patternStr->bufferAccessData();

    const size_t& len = accessData.length;
    bool slashFlag = false;
    size_t i, start = 0;
    StringBuilder builder;
    while (true) {
        for (i = 0; start + i < len; i++) {
            if (UNLIKELY(accessData.charAt(start + i) == '/') && i > 0) {
                size_t backSlashCount = 0;
                size_t s = start + i - 1;
                while (true) {
                    if (accessData.charAt(s) == '\\') {
                        backSlashCount++;
                        if (s == 0) {
                            break;
                        }
                        s--;
                    } else {
                        break;
                    }
                }
                if (backSlashCount % 2 == 0) {
                    slashFlag = true;
                    builder.appendSubString(patternStr, start, start + i);
                    builder.appendChar('\\');
                    builder.appendChar('/');

                    start = start + i + 1;
                    i = 0;
                    backSlashCount = 0;
                    break;
                }
            }
        }
        if (start + i >= len) {
            if (UNLIKELY(slashFlag)) {
                builder.appendSubString(patternStr, start, start + i);
            }
            break;
        }
    }
    if (!slashFlag) {
        return patternStr;
    } else {
        return builder.finalize();
    }
}

void RegExpObject::internalInit(ExecutionState& state, String* source)
{
    String* defaultRegExpString = state.context()->staticStrings().defaultRegExpString.string();

    String* previousSource = m_source;
    // Last index should always be 0 on RegExp initialization
    m_lastIndex = Value(0);
    m_source = source->length() ? source : defaultRegExpString;
    m_source = escapeSlashInPattern(m_source);

    auto entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
    if (entry.m_yarrError) {
        if (previousSource != defaultRegExpString) {
            m_source = previousSource;
        }

        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, entry.m_yarrError);
    }

    m_yarrPattern = entry.m_yarrPattern;
    m_bytecodePattern = entry.m_bytecodePattern;
}

void RegExpObject::init(ExecutionState& state, String* source, String* option)
{
    parseOption(state, option);
    this->internalInit(state, source);
}

void RegExpObject::initWithOption(ExecutionState& state, String* source, Option option)
{
    m_option = option;
    this->internalInit(state, source);
}

void RegExpObject::setLastIndex(ExecutionState& state, const Value& v)
{
    if (UNLIKELY(rareData() && rareData()->m_hasNonWritableLastIndexRegexpObject && (m_option & (Option::Sticky | Option::Global)))) {
        Object::throwCannotWriteError(state, ObjectStructurePropertyName(state.context()->staticStrings().lastIndex));
    }
    m_lastIndex = v;
}

bool RegExpObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    bool returnValue = Object::defineOwnProperty(state, P, desc);
    if (!P.isUIntType() && returnValue && P.objectStructurePropertyName() == ObjectStructurePropertyName(state.context()->staticStrings().lastIndex)) {
        if (!structure()->readProperty((size_t)ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER).m_descriptor.isWritable()) {
            ensureObjectRareData()->m_hasNonWritableLastIndexRegexpObject = true;
        } else {
            ensureObjectRareData()->m_hasNonWritableLastIndexRegexpObject = false;
        }
    }
    return returnValue;
}

bool RegExpObject::isRegExpObject()
{
    return true;
}

void RegExpObject::parseOption(ExecutionState& state, const String* optionString)
{
    this->m_option = RegExpObject::Option::None;

    auto bufferAccessData = optionString->bufferAccessData();
    for (size_t i = 0; i < bufferAccessData.length; i++) {
        switch (bufferAccessData.charAt(i)) {
        case 'g':
            if (this->m_option & Option::Global)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'g' flags");
            this->m_option = (Option)(this->m_option | Option::Global);
            break;
        case 'i':
            if (this->m_option & Option::IgnoreCase)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'i' flags");
            this->m_option = (Option)(this->m_option | Option::IgnoreCase);
            break;
        case 'm':
            if (this->m_option & Option::MultiLine)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'm' flags");
            this->m_option = (Option)(this->m_option | Option::MultiLine);
            break;
        case 'u':
            if (this->m_option & Option::Unicode)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'u' flags");
            this->m_option = (Option)(this->m_option | Option::Unicode);
            break;
        case 'y':
            if (this->m_option & Option::Sticky)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'y' flags");
            this->m_option = (Option)(this->m_option | Option::Sticky);
            break;
        case 's':
            if (this->m_option & Option::DotAll)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 's' flags");
            this->m_option = (Option)(this->m_option | Option::DotAll);
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid flag");
        }
    }
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
        const char* yarrError = nullptr;
        JSC::Yarr::YarrPattern* yarrPattern = nullptr;
        try {
            JSC::Yarr::ErrorCode errorCode = JSC::Yarr::ErrorCode::NoError;
            yarrPattern = JSC::Yarr::YarrPattern::createYarrPattern(source, (JSC::Yarr::RegExpFlags)option, errorCode);
            yarrError = JSC::Yarr::errorMessage(errorCode);
        } catch (const std::bad_alloc& e) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "got too complicated RegExp pattern to process");
        }
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
    RegExpStatus& globalRegExpStatus = state.context()->globalObject()->regexp()->m_status;
    globalRegExpStatus.input = str;

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
            std::unique_ptr<JSC::Yarr::BytecodePattern> ownedBytecode = JSC::Yarr::byteCompile(*m_yarrPattern, bumpAlloc);
            m_bytecodePattern = ownedBytecode.release();
            entry.m_bytecodePattern = m_bytecodePattern;
        }
    }

    unsigned subPatternNum = m_bytecodePattern->m_body->m_numSubpatterns;
    matchResult.m_subPatternNum = (int)subPatternNum;
    size_t length = str->length();
    size_t start = startIndex;
    unsigned result = 0;
    bool isGlobal = option() & RegExpObject::Option::Global;
    bool gotResult = false;
    bool reachToEnd = false;
    unsigned* outputBuf = ALLOCA(sizeof(unsigned) * 2 * (subPatternNum + 1), unsigned int, state);
    outputBuf[1] = start;
    do {
        start = outputBuf[1];
        memset(outputBuf, -1, sizeof(unsigned) * 2 * (subPatternNum + 1));
        if (start > length) {
            break;
        }
        if (LIKELY(str->has8BitContent()))
            result = JSC::Yarr::interpret(m_bytecodePattern, str->characters8(), length, start, outputBuf);
        else
            result = JSC::Yarr::interpret(m_bytecodePattern, (const UChar*)str->characters16(), length, start, outputBuf);

        if (result != JSC::Yarr::offsetNoMatch) {
            gotResult = true;
            unsigned maxMatchedIndex = subPatternNum;

            bool lastParenInvalid = false;
            for (; maxMatchedIndex > 0; maxMatchedIndex--) {
                if (outputBuf[maxMatchedIndex * 2] != std::numeric_limits<unsigned>::max()) {
                    break;
                } else {
                    lastParenInvalid = true;
                }
            }

            // Details:{3, 10, 3, 10, 3, 6, 7, 10, 1684872, 806200}
            globalRegExpStatus.pairCount = maxMatchedIndex;
            unsigned pairEnd = std::min(maxMatchedIndex, (unsigned)9);
            for (unsigned i = 1; i <= pairEnd; i++) {
                if (outputBuf[i * 2] == std::numeric_limits<unsigned>::max()) {
                    globalRegExpStatus.pairs[i - 1] = StringView();
                } else {
                    globalRegExpStatus.pairs[i - 1] = StringView(str, outputBuf[i * 2], outputBuf[i * 2 + 1]);
                }
            }

            if (UNLIKELY(testOnly)) {
                // outputBuf[1] should be set to lastIndex
                if (isGlobal) {
                    setLastIndex(state, Value(outputBuf[1]));
                }
                if (!lastParenInvalid && subPatternNum) {
                    globalRegExpStatus.lastParen = StringView(str, outputBuf[maxMatchedIndex * 2], outputBuf[maxMatchedIndex * 2 + 1]);
                } else {
                    globalRegExpStatus.lastParen = StringView();
                }
                globalRegExpStatus.lastMatch = StringView(str, outputBuf[0], outputBuf[1]);
                globalRegExpStatus.leftContext = StringView(str, 0, outputBuf[0]);
                globalRegExpStatus.rightContext = StringView(str, outputBuf[1], length);
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

            if (!lastParenInvalid && subPatternNum) {
                globalRegExpStatus.lastParen = StringView(str, piece[maxMatchedIndex].m_start, piece[maxMatchedIndex].m_end);
            } else {
                globalRegExpStatus.lastParen = StringView();
            }

            globalRegExpStatus.leftContext = StringView(str, 0, piece[0].m_start);
            globalRegExpStatus.rightContext = StringView(str, piece[maxMatchedIndex].m_end, length);
            globalRegExpStatus.lastMatch = StringView(str, piece[0].m_start, piece[0].m_end);
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
            if (start) {
                reachToEnd = true;
            }
            break;
        }
    } while (result != JSC::Yarr::offsetNoMatch);

    if (!gotResult && ((option() & (RegExpObject::Option::Global | RegExpObject::Option::Sticky)))) {
        setLastIndex(state, Value(0));
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
            setLastIndex(state, Value(previousLastIndex++));
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
                array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(index++)), ObjectPropertyDescriptor(str->substring(result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end), ObjectPropertyDescriptor::AllPresent));
            }
            if (index == limit)
                return;
        }
    }
}
}
