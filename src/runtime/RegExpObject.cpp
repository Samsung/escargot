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
#include "RegExpObject.h"
#include "Context.h"
#include "ArrayObject.h"
#include "VMInstance.h"

#ifdef ENABLE_QUICKJS_REGEXP
#include "libregexp.h"
#else /* !ENABLE_QUICKJS_REGEXP */
#include "WTFBridge.h"
#include "Yarr.h"
#include "YarrPattern.h"
#include "YarrInterpreter.h"
#endif /* ENABLE_QUICKJS_REGEXP */

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
    initWithOption(state, source, (Option)option);
}

RegExpObject::RegExpObject(ExecutionState& state, Object* proto, bool hasLastIndex)
    : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + (hasLastIndex ? 5 : 4))
    , m_source(NULL)
    , m_optionString(NULL)
    , m_option(None)
#ifdef ENABLE_QUICKJS_REGEXP
    , m_bytecode(NULL)
    , m_groupNames(NULL)
#else /* !ENABLE_QUICKJS_REGEXP */
    , m_yarrPattern(NULL)
    , m_bytecodePattern(NULL)
#endif /* ENABLE_QUICKJS_REGEXP */
    , m_lastIndex(Value(0))
    , m_lastExecutedString(NULL)
    , m_legacyFeaturesEnabled(true)
{
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
#ifdef ENABLE_QUICKJS_REGEXP
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_bytecode));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_groupNames));
#else /* !ENABLE_QUICKJS_REGEXP */
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_yarrPattern));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RegExpObject, m_bytecodePattern));
#endif /* ENABLE_QUICKJS_REGEXP */
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

void RegExpObject::internalInit(ExecutionState& state, String* source, String* options)
{
    String* defaultRegExpString = state.context()->staticStrings().defaultRegExpString.string();

    String* previousSource = m_source;
    RegExpObject::Option previousOptions = m_option;
    if (options->length() != 0) {
        parseOption(state, options);
    } else {
        m_option = RegExpObject::Option::None;
    }
    m_source = source->length() ? source : defaultRegExpString;
    m_source = escapeSlashInPattern(m_source);

    auto entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
    if (entry.m_error) {
        m_source = previousSource;
        m_option = previousOptions;
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, entry.m_error);
    }
    setLastIndex(state, Value(0));
#ifdef ENABLE_QUICKJS_REGEXP
    m_bytecode = entry.m_bytecode;
    m_groupNames = entry.m_groupNames;
#else /* !ENABLE_QUICKJS_REGEXP */
    m_yarrPattern = entry.m_yarrPattern;
    m_bytecodePattern = entry.m_bytecodePattern;
#endif /* ENABLE_QUICKJS_REGEXP */
}

void RegExpObject::init(ExecutionState& state, String* source, String* option)
{
    if (option == String::emptyString)
        this->internalInit(state, source);
    else
        this->internalInit(state, source, option);
}

void RegExpObject::initWithOption(ExecutionState& state, String* source, Option option)
{
    m_option = option;
    this->internalInit(state, source);
}

void RegExpObject::setLastIndex(ExecutionState& state, const Value& v)
{
    if (UNLIKELY(hasRareData() && rareData()->m_hasNonWritableLastIndexRegExpObject && (m_option & (Option::Sticky | Option::Global)))) {
        Object::throwCannotWriteError(state, ObjectStructurePropertyName(state.context()->staticStrings().lastIndex));
    }
    m_lastIndex = v;
}

bool RegExpObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc)
{
    bool returnValue = Object::defineOwnProperty(state, P, desc);
    if (!P.isUIntType() && returnValue && P.objectStructurePropertyName() == ObjectStructurePropertyName(state.context()->staticStrings().lastIndex)) {
        if (!structure()->readProperty((size_t)ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER).m_descriptor.isWritable()) {
            ensureRareData()->m_hasNonWritableLastIndexRegExpObject = true;
        } else {
            ensureRareData()->m_hasNonWritableLastIndexRegExpObject = false;
        }
    }
    return returnValue;
}

void RegExpObject::parseOption(ExecutionState& state, String* optionString)
{
    Option tempOption = RegExpObject::Option::None;


    auto bufferAccessData = optionString->bufferAccessData();
    for (size_t i = 0; i < bufferAccessData.length; i++) {
        switch (bufferAccessData.charAt(i)) {
        case 'g':
            if (tempOption & Option::Global)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'g' flags");
            tempOption = (Option)(tempOption | Option::Global);
            break;
        case 'i':
            if (tempOption & Option::IgnoreCase)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'i' flags");
            tempOption = (Option)(tempOption | Option::IgnoreCase);
            break;
        case 'm':
            if (tempOption & Option::MultiLine)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'm' flags");
            tempOption = (Option)(tempOption | Option::MultiLine);
            break;
        case 'u':
            if (tempOption & Option::Unicode)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'u' flags");
            tempOption = (Option)(tempOption | Option::Unicode);
            break;
        case 'y':
            if (tempOption & Option::Sticky)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 'y' flags");
            tempOption = (Option)(tempOption | Option::Sticky);
            break;
        case 's':
            if (tempOption & Option::DotAll)
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has multiple 's' flags");
            tempOption = (Option)(tempOption | Option::DotAll);
            break;
        default:
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "RegExp has invalid flag");
        }
    }
    m_option = tempOption;
    m_optionString = optionString;
}

void RegExpObject::setOption(const Option& option)
{
    ASSERT((m_option | Option::Global) == (option | Option::Global));
    m_option = option;
}

RegExpObject::RegExpCacheEntry& RegExpObject::getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option)
{
    auto cache = state.context()->regexpCache();
    auto it = cache->find(RegExpCacheKey(source, option));
    if (it != cache->end()) {
        return it->second;
    } else {
#ifdef ENABLE_QUICKJS_REGEXP
        char* error = nullptr;
        uint8_t* bytecode = nullptr;
        CaptureGroupNameVector* groupNames = nullptr;

        UTF8StringDataNonGCStd str = source->toNonGCUTF8StringData();

        int bytecodeLen;
        char errorMessage[128];
        int flags = 0;

        if (option & Option::IgnoreCase) {
            flags |= LRE_FLAG_IGNORECASE;
        }
        if (option & Option::MultiLine) {
            flags |= LRE_FLAG_MULTILINE;
        }
        if (option & Option::Sticky) {
            flags |= LRE_FLAG_STICKY;
        }
        if (option & Option::Unicode) {
            flags |= LRE_FLAG_UTF16;
        }
        if (option & Option::DotAll) {
            flags |= LRE_FLAG_DOTALL;
        }

        bytecode = lre_compile(&bytecodeLen, errorMessage, sizeof(errorMessage), str.data(), str.length(), flags, NULL);

        if (bytecode == nullptr) {
            size_t size = strlen(errorMessage);
            error = reinterpret_cast<char*>(GC_MALLOC(size + 1));
            memcpy(error, errorMessage, size + 1);
        } else {
            const char* groupNamesUtf8 = lre_get_groupnames(bytecode);

            if (groupNamesUtf8 != nullptr) {
                groupNames = new CaptureGroupNameVector();
                size_t subPatternNum = (size_t)lre_get_capture_count(bytecode);

                for (size_t index = 1; index < subPatternNum; index++) {
                    if (*groupNamesUtf8 == '\0') {
                        groupNamesUtf8++;
                        continue;
                    }

                    size_t length = strlen(groupNamesUtf8);

                    groupNames->pushBack(CaptureGroupName(String::fromUTF8(groupNamesUtf8, length), index));
                    groupNamesUtf8 += length + 1;
                }
            }
        }
        return cache->insert(std::make_pair(RegExpCacheKey(source, option), RegExpCacheEntry(error, bytecode, groupNames))).first->second;
#else /* !ENABLE_QUICKJS_REGEXP */
        const char* error = nullptr;
        JSC::Yarr::YarrPattern* yarrPattern = nullptr;
        try {
            JSC::Yarr::ErrorCode errorCode = JSC::Yarr::ErrorCode::NoError;
            yarrPattern = JSC::Yarr::YarrPattern::createYarrPattern(source, (JSC::Yarr::RegExpFlags)option, errorCode);
            error = JSC::Yarr::errorMessage(errorCode);
        } catch (const std::bad_alloc& e) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "got too complicated RegExp pattern to process");
        }
        return cache->insert(std::make_pair(RegExpCacheKey(source, option), RegExpCacheEntry(error, yarrPattern))).first->second;
#endif /* ENABLE_QUICKJS_REGEXP */
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

#ifdef ENABLE_QUICKJS_REGEXP
    if (!m_bytecode) {
        RegExpCacheEntry& entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
        if (entry.m_error) {
            matchResult.m_subPatternNum = 0;
            return false;
        }
        m_bytecode = entry.m_bytecode;
        m_groupNames = entry.m_groupNames;
    }

    int subPatternNum = lre_get_capture_count(m_bytecode);
    matchResult.m_subPatternNum = subPatternNum - 1;
    int length = (int)str->length();
    int start = (int)startIndex;
    int result;
    bool isGlobal = option() & RegExpObject::Option::Global;
    bool isSticky = option() & RegExpObject::Option::Sticky;
    bool gotResult = false;
    int buffer_type = str->has8BitContent() ? 0 : 1;
    const uint8_t* buffer = buffer_type == 0 ? str->characters8() : (const uint8_t*)str->characters16();
    uint8_t** outputBuf = ALLOCA(sizeof(uint8_t*) * 2 * subPatternNum, uint8_t*, state);

    while (true) {
        if (start > length) {
            break;
        }

        memset(outputBuf, 0, sizeof(uint8_t*) * 2 * subPatternNum);
        result = lre_exec(outputBuf, m_bytecode, buffer, start, length, buffer_type, NULL);

        if (result != 1) {
            break;
        }

        gotResult = true;
        unsigned maxMatchedIndex = subPatternNum - 1;

        bool lastParenInvalid = false;
        for (; maxMatchedIndex > 0; maxMatchedIndex--) {
            if (outputBuf[maxMatchedIndex * 2] != NULL) {
                break;
            } else {
                lastParenInvalid = true;
            }
        }

        // Details:{3, 10, 3, 10, 3, 6, 7, 10, 1684872, 806200}
        legacyFeatures.dollarCount = maxMatchedIndex;
        unsigned dollarEnd = std::min(maxMatchedIndex, (unsigned)9);
        for (unsigned i = 1; i <= dollarEnd; i++) {
            if (outputBuf[i * 2] == NULL) {
                legacyFeatures.dollars[i - 1] = StringView();
            } else {
                legacyFeatures.dollars[i - 1] = StringView(str, (outputBuf[i * 2] - buffer) >> buffer_type, (outputBuf[i * 2 + 1] - buffer) >> buffer_type);
            }
        }

        if (UNLIKELY(testOnly)) {
            // outputBuf[1] should be set to lastIndex
            if (isGlobal || isSticky) {
                setLastIndex(state, Value((outputBuf[1] - buffer) >> buffer_type));
            }
            if (!lastParenInvalid && subPatternNum > 1) {
                legacyFeatures.lastParen = StringView(str, (outputBuf[maxMatchedIndex * 2] - buffer) >> buffer_type, (outputBuf[maxMatchedIndex * 2 + 1] - buffer) >> buffer_type);
            } else {
                legacyFeatures.lastParen = StringView();
            }
            legacyFeatures.lastMatch = StringView(str, (outputBuf[0] - buffer) >> buffer_type, (outputBuf[1] - buffer) >> buffer_type);
            legacyFeatures.leftContext = StringView(str, 0, (outputBuf[0] - buffer) >> buffer_type);
            legacyFeatures.rightContext = StringView(str, (outputBuf[1] - buffer) >> buffer_type, length);
            return true;
        }
        std::vector<RegexMatchResult::RegexMatchResultPiece> piece;
        piece.resize(subPatternNum);

        for (int i = 0; i < subPatternNum; i++) {
            RegexMatchResult::RegexMatchResultPiece p;
            p.m_start = (outputBuf[i * 2] - buffer) >> buffer_type;
            p.m_end = (outputBuf[i * 2 + 1] - buffer) >> buffer_type;
            piece[i] = p;
        }

        if (!lastParenInvalid && subPatternNum > 1) {
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

        int new_start = (outputBuf[1] - buffer) >> buffer_type;
        if (start == new_start) {
            start++;
            if (start > length) {
                break;
            }
        } else {
            start = new_start;
        }
    }
#else /* !ENABLE_QUICKJS_REGEXP */
    if (!m_bytecodePattern) {
        RegExpCacheEntry& entry = getCacheEntryAndCompileIfNeeded(state, m_source, m_option);
        if (entry.m_error) {
            matchResult.m_subPatternNum = 0;
            return false;
        }
        m_yarrPattern = entry.m_yarrPattern;

        if (entry.m_bytecodePattern) {
            m_bytecodePattern = entry.m_bytecodePattern;
        } else {
            WTF::BumpPointerAllocator* bumpAlloc = VMInstance::bumpPointerAllocator();
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
    bool isSticky = option() & RegExpObject::Option::Sticky;
    bool gotResult = false;
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
#endif /* ENABLE_QUICKJS_REGEXP */

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

ArrayObject* RegExpObject::createRegExpMatchedArray(ExecutionState& state, const RegexMatchResult& result, String* input)
{
    uint64_t len = 0;
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            len++;
        }
    }

    ArrayObject* arr = new ArrayObject(state, len);

    arr->defineOwnPropertyThrowsException(state, state.context()->staticStrings().index, ObjectPropertyDescriptor(Value(result.m_matchResults[0][0].m_start)));
    arr->defineOwnPropertyThrowsException(state, state.context()->staticStrings().input, ObjectPropertyDescriptor(Value(input)));

    size_t idx = 0;
    for (unsigned i = 0; i < result.m_matchResults.size(); i++) {
        for (unsigned j = 0; j < result.m_matchResults[i].size(); j++) {
            if (result.m_matchResults[i][j].m_start == std::numeric_limits<unsigned>::max()) {
                arr->defineOwnIndexedPropertyWithExpandedLength(state, idx++, Value());
            } else {
                arr->defineOwnIndexedPropertyWithExpandedLength(state, idx++, Value(new StringView(input, result.m_matchResults[i][j].m_start, result.m_matchResults[i][j].m_end)));
            }
        }
    }

#ifdef ENABLE_QUICKJS_REGEXP
    if (m_groupNames == NULL) {
        arr->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
    } else {
        Object* groups = new Object(state);
        groups->setPrototype(state, Value(Value::Null));
        for (auto it = m_groupNames->begin(); it != m_groupNames->end(); ++it) {
            groups->defineOwnProperty(state, ObjectPropertyName(state, it->name),
                                      ObjectPropertyDescriptor(arr->getOwnProperty(state, ObjectPropertyName(state, it->index)).value(state, this), ObjectPropertyDescriptor::AllPresent));
        }
        arr->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(groups), ObjectPropertyDescriptor::AllPresent));
    }
#else /* !ENABLE_QUICKJS_REGEXP */
    if (m_yarrPattern->m_namedGroupToParenIndex.empty()) {
        arr->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
    } else {
        Object* groups = new Object(state);
        groups->setPrototype(state, Value(Value::Null));
        for (auto it = m_yarrPattern->m_captureGroupNames.begin(); it != m_yarrPattern->m_captureGroupNames.end(); ++it) {
            auto foundMapElement = m_yarrPattern->m_namedGroupToParenIndex.find(*it);
            if (foundMapElement != m_yarrPattern->m_namedGroupToParenIndex.end()) {
                groups->defineOwnProperty(state, ObjectPropertyName(state, it->impl()),
                                          ObjectPropertyDescriptor(arr->getOwnProperty(state, ObjectPropertyName(state, foundMapElement->second)).value(state, this), ObjectPropertyDescriptor::AllPresent));
            }
        }
        arr->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().groups), ObjectPropertyDescriptor(Value(groups), ObjectPropertyDescriptor::AllPresent));
    }
#endif /* ENABLE_QUICKJS_REGEXP */

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

bool RegExpObject::hasNamedGroups()
{
#ifdef ENABLE_QUICKJS_REGEXP
    return lre_get_groupnames(m_bytecode) != NULL;
#else /* !ENABLE_QUICKJS_REGEXP */
    return m_yarrPattern->m_captureGroupNames.size() != 0;
#endif /* ENABLE_QUICKJS_REGEXP */
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
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(RegExpStringIteratorObject)] = { 0 };
        fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(RegExpStringIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

} // namespace Escargot
