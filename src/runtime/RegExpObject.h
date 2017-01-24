#ifndef __EscargotRegExpObject__
#define __EscargotRegExpObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace JSC {
namespace Yarr {
class YarrPattern;
class BytecodePattern;
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
    void initRegExpObject(ExecutionState& state);

public:
    enum Option {
        None = 1,
        Global = 1 << 1,
        IgnoreCase = 1 << 2,
        MultiLine = 1 << 3,
        // NOTE(ES6): Sticky and Unicode option is added in ES6
        Sticky = 1 << 4,
        Unicode = 1 << 5,
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
        const bool m_multiline;
        const bool m_ignoreCase;
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

    RegExpObject(ExecutionState& state);
    RegExpObject(ExecutionState& state, const Value& source, const Value& option);

    void init(ExecutionState& state, const Value& source, const Value& option);

    double computedLastIndex(ExecutionState& state)
    {
        // NOTE(ES6): if global is false and sticy is false, let lastIndex be 0
        if (!(option() & (Option::Global | Option::Sticky)))
            return 0;
        return lastIndex().toInteger(state);
    }

    bool match(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);
    bool matchNonGlobally(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);

    const String* source()
    {
        return m_source;
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

    void setLastIndex(const Value& v)
    {
        m_lastIndex = v;
    }

    virtual bool isRegExpObject()
    {
        return true;
    }

    void createRegexMatchResult(ExecutionState& state, String* str, RegexMatchResult& result);
    ArrayObject* createMatchedArray(ExecutionState& state, String* str, RegexMatchResult& result);
    ArrayObject* createRegExpMatchedArray(ExecutionState& state, const RegexMatchResult& result, String* input);
    void pushBackToRegExpMatchedArray(ExecutionState& state, ArrayObject* array, size_t& index, const size_t limit, const RegexMatchResult& result, String* str);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "RegExp";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    void setBytecodePattern(JSC::Yarr::BytecodePattern* pattern)
    {
        m_bytecodePattern = pattern;
    }

    void setOption(const Option& option);

    static RegExpCacheEntry& getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option);

    static Option parseOption(ExecutionState& state, const String* optionString);


    String* m_source;
    Option m_option;
    JSC::Yarr::YarrPattern* m_yarrPattern;
    JSC::Yarr::BytecodePattern* m_bytecodePattern;

    SmallValue m_lastIndex;
    const String* m_lastExecutedString;
};

typedef std::unordered_map<RegExpObject::RegExpCacheKey, RegExpObject::RegExpCacheEntry,
                           std::hash<RegExpObject::RegExpCacheKey>, std::equal_to<RegExpObject::RegExpCacheKey>,
                           gc_allocator<std::pair<RegExpObject::RegExpCacheKey, RegExpObject::RegExpCacheEntry>>>
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
