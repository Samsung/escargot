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
#include "ThreadLocal.h"
#include "RegExpObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "ArrayObject.h"

#include "WTFBridge.h"
#include "Yarr.h"
#include "YarrPattern.h"
#include "YarrInterpreter.h"

namespace Escargot {

RegExpObject::RegExpObject(ExecutionState& state, String* source, String* option)
    : RegExpObject(state, state.context()->globalObject()->regexpPrototype(), source, option)
{
}

RegExpObject::RegExpObject(ExecutionState& state, Object* proto, String* source, String* option)
    : RegExpObject(state, proto, true)
{
    init(state, source, option);
}

RegExpObject::RegExpObject(ExecutionState& state, String* source, unsigned int option)
    : RegExpObject(state, state.context()->globalObject()->regexpPrototype(), source, option)
{
}

RegExpObject::RegExpObject(ExecutionState& state, Object* proto, String* source, unsigned int option)
    : RegExpObject(state, proto, true)
{
    internalInit(state, source, (Option)option);
    m_optionString = String::emptyString;
}

RegExpObject::RegExpObject(ExecutionState& state, Object* proto, bool hasLastIndex)
    : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + (hasLastIndex ? 5 : 4))
    , m_source(NULL)
    , m_optionString(NULL)
    , m_legacyFeaturesEnabled(true)
    , m_hasNonWritableLastIndexRegExpObject(false)
    , m_hasOwnPropertyWhichHasDefinedFromRegExpPrototype(false)
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
{
    setOptionValueForGC(None);
    initRegExpObject(state, hasLastIndex);
}

void RegExpObject::initRegExpObject(ExecutionState& state, bool hasLastIndex)
{
    if (hasLastIndex) {
        for (size_t i = 0; i < ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 5; i++)
            m_values[i] = Value();
        m_structure = state.context()->defaultStructureForRegExpObject();
    } else {
        for (size_t i = 0; i < ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 4; i++)
            m_values[i] = Value();
        m_structure = state.context()->defaultStructureForObject();
    }
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
            if (UNLIKELY(accessData.charAt(start + i) == '/') && (i > 0 || len == 1)) {
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

void RegExpObject::internalInit(ExecutionState& state, String* source, Option option)
{
    String* defaultRegExpString = state.context()->staticStrings().defaultRegExpString.string();

    String* previousSource = m_source;
    RegExpObject::Option previousOptions = this->option();

    setOptionValueForGC(option);
    m_source = source->length() ? source : defaultRegExpString;
    m_source = escapeSlashInPattern(m_source);

    auto& entry = getCacheEntryAndCompileIfNeeded(state, m_source, this->option());
    if (entry.m_yarrError) {
        m_source = previousSource;
        setOptionValueForGC(previousOptions);
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, entry.m_yarrError);
    }

    setLastIndex(state, Value(0));
    m_yarrPattern = entry.m_yarrPattern;
    m_bytecodePattern = entry.m_bytecodePattern;
}

void RegExpObject::init(ExecutionState& state, String* source, String* option)
{
    Option optionVals = parseOption(state, option);
    internalInit(state, source, optionVals);
    m_optionString = option;
}

bool RegExpObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    bool returnValue = DerivedObject::defineOwnProperty(state, P, desc);
    if (!P.isUIntType() && returnValue) {
        if (P.objectStructurePropertyName().isPlainString()) {
            auto name = P.objectStructurePropertyName().plainString();

            if (name->equals(state.context()->staticStrings().global.string())
                || name->equals(state.context()->staticStrings().ignoreCase.string())
                || name->equals(state.context()->staticStrings().multiline.string())
                || name->equals(state.context()->staticStrings().unicode.string())
                || name->equals(state.context()->staticStrings().sticky.string())
                || name->equals(state.context()->staticStrings().dotAll.string())
                || name->equals(state.context()->staticStrings().source.string())
                || name->equals(state.context()->staticStrings().flags.string())) {
                m_hasOwnPropertyWhichHasDefinedFromRegExpPrototype = true;
            }

            if (name->equals(state.context()->staticStrings().lastIndex.string())) {
                if (!structure()->readProperty((size_t)ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER).m_descriptor.isWritable()) {
                    m_hasNonWritableLastIndexRegExpObject = true;
                }
            }
        }
    }
    return returnValue;
}

RegExpObject::Option RegExpObject::parseOption(ExecutionState& state, String* optionString)
{
    Option tempOption = RegExpObject::Option::None;

    auto bufferAccessData = optionString->bufferAccessData();
    for (size_t i = 0; i < bufferAccessData.length; i++) {
        switch (bufferAccessData.charAt(i)) {
        case 'g':
            if (tempOption & Option::Global)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'g' flags");
            tempOption = (Option)(tempOption | Option::Global);
            break;
        case 'i':
            if (tempOption & Option::IgnoreCase)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'i' flags");
            tempOption = (Option)(tempOption | Option::IgnoreCase);
            break;
        case 'm':
            if (tempOption & Option::MultiLine)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'm' flags");
            tempOption = (Option)(tempOption | Option::MultiLine);
            break;
        case 'u':
            if (tempOption & Option::Unicode)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'u' flags");
            tempOption = (Option)(tempOption | Option::Unicode);
            break;
        case 'y':
            if (tempOption & Option::Sticky)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'y' flags");
            tempOption = (Option)(tempOption | Option::Sticky);
            break;
        case 's':
            if (tempOption & Option::DotAll)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 's' flags");
            tempOption = (Option)(tempOption | Option::DotAll);
            break;
        case 'v':
            if (tempOption & Option::UnicodeSets)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'v' flags");
            tempOption = (Option)(tempOption | Option::UnicodeSets);
            break;
        case 'd':
            if (tempOption & Option::HasIndices)
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has multiple 'd' flags");
            tempOption = (Option)(tempOption | Option::HasIndices);
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "RegExp has invalid flag");
        }
    }

    return tempOption;
}

void RegExpObject::setOption(const Option& option)
{
    Option currentOption = this->option();
    if (((currentOption & Option::MultiLine) != (option & Option::MultiLine))
        || ((currentOption & Option::IgnoreCase) != (option & Option::IgnoreCase))) {
        ASSERT(!m_yarrPattern);
        m_bytecodePattern = NULL;
    }
    setOptionValueForGC(option);
}

RegExpObject::RegExpCacheEntry& RegExpObject::getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option)
{
    auto cache = state.context()->regexpCache();
    auto it = cache->find(RegExpCacheKey(source, option));
    if (it != cache->end()) {
        return it.value();
    } else {
        const char* yarrError = nullptr;
        JSC::Yarr::YarrPattern* yarrPattern = nullptr;
        try {
            JSC::Yarr::ErrorCode errorCode = JSC::Yarr::ErrorCode::NoError;
            yarrPattern = new JSC::Yarr::YarrPattern(source, WTF::OptionSet<JSC::Yarr::Flags>((JSC::Yarr::Flags)option), errorCode);
            yarrError = JSC::Yarr::errorMessage(errorCode);
        } catch (const std::bad_alloc& e) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "got too complicated RegExp pattern to process");
        }
        auto iter = cache->insert(std::make_pair(RegExpCacheKey(source, option), RegExpCacheEntry(yarrError, yarrPattern))).first;
        return iter.value();
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
    Context::RegExpLegacyFeatures& legacyFeatures = state.context()->regexpLegacyFeatures();
    legacyFeatures.input = str;

    m_lastExecutedString = str;

    if (!m_bytecodePattern) {
        RegExpCacheEntry& entry = getCacheEntryAndCompileIfNeeded(state, m_source, option());
        if (entry.m_yarrError) {
            matchResult.m_subPatternNum = 0;
            return false;
        }
        m_yarrPattern = entry.m_yarrPattern;

        if (entry.m_bytecodePattern) {
            m_bytecodePattern = entry.m_bytecodePattern;
        } else {
            WTF::BumpPointerAllocator* bumpAlloc = ThreadLocal::bumpPointerAllocator();
            JSC::Yarr::ErrorCode errorCode = JSC::Yarr::ErrorCode::NoError;
            std::unique_ptr<JSC::Yarr::BytecodePattern> ownedBytecode = JSC::Yarr::byteCompile(*m_yarrPattern, bumpAlloc, errorCode);
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
    bool isSticky = option() & RegExpObject::Option::Sticky;
    bool gotResult = false;
    unsigned* outputBuf = ALLOCA(sizeof(unsigned) * 2 * (subPatternNum + 1), unsigned int);
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
            legacyFeatures.dollarCount = maxMatchedIndex;
            unsigned dollarEnd = std::min(maxMatchedIndex, (unsigned)9);
            for (unsigned i = 1; i <= dollarEnd; i++) {
                if (outputBuf[i * 2] == std::numeric_limits<unsigned>::max()) {
                    legacyFeatures.dollars[i - 1] = StringView();
                } else {
                    legacyFeatures.dollars[i - 1] = StringView(str, outputBuf[i * 2], outputBuf[i * 2 + 1]);
                }
            }

            if (UNLIKELY(testOnly)) {
                // outputBuf[1] should be set to lastIndex
                if (isGlobal || isSticky) {
                    setLastIndex(state, Value(outputBuf[1]));
                }
                if (!lastParenInvalid && subPatternNum) {
                    legacyFeatures.lastParen = StringView(str, outputBuf[maxMatchedIndex * 2], outputBuf[maxMatchedIndex * 2 + 1]);
                } else {
                    legacyFeatures.lastParen = StringView();
                }
                legacyFeatures.lastMatch = StringView(str, outputBuf[0], outputBuf[1]);
                legacyFeatures.leftContext = StringView(str, 0, outputBuf[0]);
                legacyFeatures.rightContext = StringView(str, outputBuf[1], length);
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
                legacyFeatures.lastParen = StringView(str, piece[maxMatchedIndex].m_start, piece[maxMatchedIndex].m_end);
            } else {
                legacyFeatures.lastParen = StringView();
            }

            legacyFeatures.leftContext = StringView(str, 0, piece[0].m_start);
            legacyFeatures.rightContext = StringView(str, piece[maxMatchedIndex].m_end, length);
            legacyFeatures.lastMatch = StringView(str, piece[0].m_start, piece[0].m_end);
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
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Maximum Reasonable match size exceeded.");
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

ArrayObject* RegExpObject::createRegExpMatchedArray(ExecutionState& state, const RegexMatchResult& result, String* input)
{
    uint64_t len = 0;
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            len++;
        }
    }

    ArrayObject* arr = new ArrayObject(state, len);
    arr->directDefineOwnProperty(state, state.context()->staticStrings().index, ObjectPropertyDescriptor(Value(result.m_matchResults[0][0].m_start)));
    arr->directDefineOwnProperty(state, state.context()->staticStrings().input, ObjectPropertyDescriptor(Value(input)));

    size_t idx = 0;
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            if (result.m_matchResults[i][j].m_start == std::numeric_limits<unsigned>::max()) {
                arr->defineOwnIndexedPropertyWithoutExpanding(state, idx++, Value());
            } else {
                arr->defineOwnIndexedPropertyWithoutExpanding(state, idx++, Value(new StringView(input, result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end)));
            }
        }
    }

    if (m_yarrPattern->m_namedGroupToParenIndices.empty()) {
        arr->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
    } else {
        Object* groups = new Object(state, Object::PrototypeIsNull);
        for (auto it = m_yarrPattern->m_captureGroupNames.begin(); it != m_yarrPattern->m_captureGroupNames.end(); ++it) {
            auto foundMapElement = m_yarrPattern->m_namedGroupToParenIndices.find(*it);
            if (foundMapElement != m_yarrPattern->m_namedGroupToParenIndices.end()) {
                groups->directDefineOwnProperty(state, ObjectPropertyName(state, it->impl()),
                                                ObjectPropertyDescriptor(arr->getOwnProperty(state, ObjectPropertyName(state, foundMapElement->second[0])).value(state, this), ObjectPropertyDescriptor::AllPresent));
            }
        }
        arr->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(groups), ObjectPropertyDescriptor::AllPresent));
    }

    // FIXME RegExp should have own Realm internal slot when allocated
    if (state.context() == this->getFunctionRealm(state)) {
        if (!this->legacyFeaturesEnabled()) {
            state.context()->regexpLegacyFeatures().invalidate();
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

bool RegExpObject::hasOwnRegExpProperty(ExecutionState& state, Object* obj)
{
    if (obj->isRegExpObject() && !obj->asRegExpObject()->m_hasOwnPropertyWhichHasDefinedFromRegExpPrototype
        && obj->getPrototypeObject(state) && obj->getPrototypeObject(state)->isRegExpPrototypeObject()) {
        return false;
    }
    return true;
}

String* RegExpObject::regexpSourceValue(ExecutionState& state, Object* obj)
{
    if (!hasOwnRegExpProperty(state, obj)) {
        return obj->asRegExpObject()->source();
    } else {
        return obj->get(state, state.context()->staticStrings().source).value(state, obj).toString(state);
    }
}

Value RegExpObject::regexpFlagsValue(ExecutionState& state, Object* obj)
{
    if (!hasOwnRegExpProperty(state, obj)) {
        return computeRegExpOptionString(state, obj);
    } else {
        return obj->get(state, state.context()->staticStrings().flags).value(state, obj);
    }
}

String* RegExpObject::computeRegExpOptionString(ExecutionState& state, Object* obj)
{
    char flags[8] = { 0 };
    size_t flagsIdx = 0;
    size_t cacheIndex = 0;

    if (!hasOwnRegExpProperty(state, obj)) {
        auto opt = obj->asRegExpObject()->option();
        if (opt & RegExpObject::Option::HasIndices) {
            flags[flagsIdx++] = 'd';
            cacheIndex |= 1 << 0;
        }

        if (opt & RegExpObject::Option::Global) {
            flags[flagsIdx++] = 'g';
            cacheIndex |= 1 << 1;
        }

        if (opt & RegExpObject::Option::IgnoreCase) {
            flags[flagsIdx++] = 'i';
            cacheIndex |= 1 << 2;
        }

        if (opt & RegExpObject::Option::MultiLine) {
            flags[flagsIdx++] = 'm';
            cacheIndex |= 1 << 3;
        }

        if (opt & RegExpObject::Option::DotAll) {
            flags[flagsIdx++] = 's';
            cacheIndex |= 1 << 4;
        }

        if (opt & RegExpObject::Option::Unicode) {
            flags[flagsIdx++] = 'u';
            cacheIndex |= 1 << 5;
        }

        if (opt & RegExpObject::Option::UnicodeSets) {
            flags[flagsIdx++] = 'v';
            cacheIndex |= 1 << 6;
        }

        if (opt & RegExpObject::Option::Sticky) {
            flags[flagsIdx++] = 'y';
            cacheIndex |= 1 << 7;
        }
    } else {
        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().hasIndices)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'd';
            cacheIndex |= 1 << 0;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().global)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'g';
            cacheIndex |= 1 << 1;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().ignoreCase)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'i';
            cacheIndex |= 1 << 2;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().multiline)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'm';
            cacheIndex |= 1 << 3;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().dotAll)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 's';
            cacheIndex |= 1 << 4;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicode)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'u';
            cacheIndex |= 1 << 5;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().unicodeSets)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'v';
            cacheIndex |= 1 << 6;
        }

        if (obj->get(state, ObjectPropertyName(state, state.context()->staticStrings().sticky)).value(state, obj).toBoolean()) {
            flags[flagsIdx++] = 'y';
            cacheIndex |= 1 << 7;
        }
    }

    ASCIIString* result;
    auto cache = state.context()->vmInstance()->regexpOptionStringCache();
    if (cache[cacheIndex]) {
        result = cache[cacheIndex];
    } else {
        result = cache[cacheIndex] = new ASCIIString(flags, flagsIdx);
    }

    return result;
}

RegExpStringIteratorObject::RegExpStringIteratorObject(ExecutionState& state, bool global, bool unicode, RegExpObject* regexp, String* string)
    : IteratorObject(state, state.context()->globalObject()->regexpStringIteratorPrototype())
    , m_isGlobal(global)
    , m_isUnicode(unicode)
    , m_isDone(false)
    , m_regexp(regexp)
    , m_string(string)
{
}

void* RegExpStringIteratorObject::operator new(size_t size)
{
    ASSERT(size == sizeof(RegExpStringIteratorObject));
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(RegExpStringIteratorObject)] = { 0 };
        fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(RegExpStringIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

} // namespace Escargot
