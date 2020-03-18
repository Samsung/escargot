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

#ifndef __EscargotRegExpObject__
#define __EscargotRegExpObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace JSC {
namespace Yarr {
struct YarrPattern;
struct BytecodePattern;
}
}

namespace Escargot {

struct RegexMatchResult {
    struct RegexMatchResultPiece {
        unsigned m_start, m_end;
    };
    COMPILE_ASSERT((sizeof(RegexMatchResultPiece)) == (sizeof(unsigned) * 2), sizeof_RegexMatchResultPiece_wrong);
    int m_subPatternNum;
    std::vector<std::vector<RegexMatchResultPiece>> m_matchResults;
};

class RegExpObject : public Object {
    void initRegExpObject(ExecutionState& state, bool hasLastIndex = true);

public:
    enum Option {
        None = 0 << 0,
        Global = 1 << 0,
        IgnoreCase = 1 << 1,
        MultiLine = 1 << 2,
        Sticky = 1 << 3,
        Unicode = 1 << 4,
        DotAll = 1 << 5,
    };

    struct RegExpCacheKey {
        RegExpCacheKey(const String* body, Option option)
            : m_body(body)
            , m_multiline(option & RegExpObject::Option::MultiLine)
            , m_ignoreCase(option & RegExpObject::Option::IgnoreCase)
        {
        }

        bool operator==(const RegExpCacheKey& otherKey) const
        {
            return (m_body == otherKey.m_body) && (m_multiline == otherKey.m_multiline) && (m_ignoreCase == otherKey.m_ignoreCase);
        }
        const String* m_body;
        const bool m_multiline : 1;
        const bool m_ignoreCase : 1;
    };

    struct RegExpCacheEntry {
        RegExpCacheEntry(const char* yarrError = nullptr, JSC::Yarr::YarrPattern* yarrPattern = nullptr, JSC::Yarr::BytecodePattern* bytecodePattern = nullptr)
            : m_yarrError(yarrError)
            , m_yarrPattern(yarrPattern)
            , m_bytecodePattern(bytecodePattern)
        {
        }

        const char* m_yarrError;
        JSC::Yarr::YarrPattern* m_yarrPattern;
        JSC::Yarr::BytecodePattern* m_bytecodePattern;
    };

    RegExpObject(ExecutionState& state, String* source, String* option);
    RegExpObject(ExecutionState& state, Object* proto, String* source, String* option);

    RegExpObject(ExecutionState& state, String* source, unsigned int option);
    RegExpObject(ExecutionState& state, Object* proto, String* source, unsigned int option);

    void init(ExecutionState& state, String* source, String* option = String::emptyString);
    void initWithOption(ExecutionState& state, String* source, Option option);

    double computedLastIndex(ExecutionState& state)
    {
        // NOTE(ES6): if global is false and sticy is false, let lastIndex be 0
        // Compute the result first with toInteger() and then examine if it is global or sticky
        double res = lastIndex().toLength(state);
        if (!(option() & (Option::Global | Option::Sticky)))
            return 0;
        return res;
    }

    bool match(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);
    bool matchNonGlobally(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);

    String* source()
    {
        return m_source;
    }

    String* regexpOptionString()
    {
        return m_optionString;
    }

    Option option()
    {
        return m_option;
    }

    JSC::Yarr::YarrPattern* yarrPatern()
    {
        return m_yarrPattern;
    }

    JSC::Yarr::BytecodePattern* bytecodePattern()
    {
        return m_bytecodePattern;
    }

    Value lastIndex()
    {
        return m_lastIndex;
    }

    void setLastIndex(ExecutionState& state, const Value& v);
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

    virtual bool isRegExpObject();

    void createRegexMatchResult(ExecutionState& state, String* str, RegexMatchResult& result);
    ArrayObject* createMatchedArray(ExecutionState& state, String* str, RegexMatchResult& result);
    ArrayObject* createRegExpMatchedArray(ExecutionState& state, const RegexMatchResult& result, String* input);
    void pushBackToRegExpMatchedArray(ExecutionState& state, ArrayObject* array, size_t& index, const size_t limit, const RegexMatchResult& result, String* str);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty(ExecutionState& state)
    {
        return "RegExp";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    explicit RegExpObject(ExecutionState& state, Object* proto, bool hasLastIndex = true);

private:
    void setOption(const Option& option);
    void internalInit(ExecutionState& state, String* source, String* options = String::emptyString);

    static RegExpCacheEntry& getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option);

    void parseOption(ExecutionState& state, String* optionString);


    String* m_source;
    String* m_optionString;
    Option m_option;
    JSC::Yarr::YarrPattern* m_yarrPattern;
    JSC::Yarr::BytecodePattern* m_bytecodePattern;
    SmallValue m_lastIndex;
    const String* m_lastExecutedString;
};

typedef std::unordered_map<RegExpObject::RegExpCacheKey, RegExpObject::RegExpCacheEntry,
                           std::hash<RegExpObject::RegExpCacheKey>, std::equal_to<RegExpObject::RegExpCacheKey>,
                           GCUtil::gc_malloc_allocator<std::pair<const RegExpObject::RegExpCacheKey, RegExpObject::RegExpCacheEntry>>>
    RegExpCacheMap;
}

namespace std {

template <>
struct hash<Escargot::RegExpObject::RegExpCacheKey> {
    size_t operator()(Escargot::RegExpObject::RegExpCacheKey const& x) const
    {
        return x.m_body->hashValue();
    }
};

template <>
struct equal_to<Escargot::RegExpObject::RegExpCacheKey> {
    bool operator()(Escargot::RegExpObject::RegExpCacheKey const& a, Escargot::RegExpObject::RegExpCacheKey const& b) const
    {
        return a == b;
    }
};
}

#endif
