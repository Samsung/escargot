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
    std::vector<std::vector<RegexMatchResultPiece, gc_malloc_pointer_free_allocator<RegexMatchResultPiece>>, gc_allocator_ignore_off_page<std::vector<RegexMatchResultPiece, gc_malloc_pointer_free_allocator<RegexMatchResultPiece>>>> m_matchResults;
};

class RegExpObject : public Object {
public:
    enum Option {
        None = 1,
        Global = 1 << 1,
        IgnoreCase = 1 << 2,
        MultiLine = 1 << 3,
        Sticky = 1 << 4,
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
    RegExpObject(ExecutionState& state, String* source, String* option);

    static RegExpCacheEntry& getCacheEntryAndCompileIfNeeded(ExecutionState& state, String* source, const Option& option);
    static Option parseOption(ExecutionState& state, String* optionStr);

    bool match(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);
    bool matchNonGlobally(ExecutionState& state, String* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);

    void setSource(ExecutionState& state, String* src);
    void setOption(ExecutionState& state, String* optionStr);
    void setOption(const Option& option);
    void setLastIndex(const Value& lastIndex);

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

    Value& lastIndex()
    {
        return m_lastIndex;
    }

    virtual bool isRegExpObject()
    {
        return true;
    }

private:
    void setBytecodePattern(JSC::Yarr::BytecodePattern* pattern)
    {
        m_bytecodePattern = pattern;
    }

    String* m_source;
    Option m_option;
    JSC::Yarr::YarrPattern* m_yarrPattern;
    JSC::Yarr::BytecodePattern* m_bytecodePattern;

    Value m_lastIndex;
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
