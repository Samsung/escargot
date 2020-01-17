/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ASTContext_h
#define ASTContext_h

#include "util/Util.h"

namespace Escargot {

class ASTFunctionScopeContextNameInfo {
public:
    ASTFunctionScopeContextNameInfo()
    {
        m_isParameterName = false;
        m_isExplicitlyDeclaredOrParameterName = false;
        m_isVarDeclaration = false;
        m_lexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;
        m_name = AtomicString();
    }

    AtomicString name() const
    {
        return m_name;
    }

    void setName(AtomicString name)
    {
        m_name = name;
    }

    bool isParameterName() const
    {
        return m_isParameterName;
    }

    void setIsParameterName(bool v)
    {
        m_isParameterName = v;
    }

    bool isExplicitlyDeclaredOrParameterName() const
    {
        return m_isExplicitlyDeclaredOrParameterName;
    }

    void setIsExplicitlyDeclaredOrParameterName(bool v)
    {
        m_isExplicitlyDeclaredOrParameterName = v;
    }

    bool isVarDeclaration() const
    {
        return m_isVarDeclaration;
    }

    void setIsVarDeclaration(bool v)
    {
        m_isVarDeclaration = v;
    }

    LexicalBlockIndex lexicalBlockIndex() const
    {
        return m_lexicalBlockIndex;
    }

    void setLexicalBlockIndex(LexicalBlockIndex v)
    {
        m_lexicalBlockIndex = v;
    }

private:
    bool m_isParameterName : 1;
    bool m_isExplicitlyDeclaredOrParameterName : 1;
    bool m_isVarDeclaration : 1;
    LexicalBlockIndex m_lexicalBlockIndex : 16;
    AtomicString m_name;
};

typedef TightVector<ASTFunctionScopeContextNameInfo, GCUtil::gc_malloc_atomic_allocator<ASTFunctionScopeContextNameInfo>> ASTFunctionScopeContextNameInfoVector;

// store only let, const names
class ASTBlockScopeContextNameInfo {
public:
    ASTBlockScopeContextNameInfo()
    {
        m_value = (size_t)AtomicString().string();
    }

    AtomicString name() const
    {
        return AtomicString((String *)((size_t)m_value & ~1));
    }

    void setName(AtomicString name)
    {
        m_value = (size_t)name.string() | isConstBinding();
    }

    bool isConstBinding() const
    {
        return m_value & 1;
    }

    void setIsConstBinding(bool v)
    {
        if (v)
            m_value = m_value | 1;
        else
            m_value = m_value & ~1;
    }

private:
    size_t m_value;
};

typedef TightVector<ASTBlockScopeContextNameInfo, GCUtil::gc_malloc_atomic_allocator<ASTBlockScopeContextNameInfo>> ASTBlockScopeContextNameInfoVector;
// context for block in function or program
struct ASTBlockScopeContext {
    ASTNodeType m_nodeType;
    LexicalBlockIndex m_blockIndex;
    LexicalBlockIndex m_parentBlockIndex;
    ASTBlockScopeContextNameInfoVector m_names;
    AtomicStringVector m_usingNames;
#ifndef NDEBUG
    ExtendedNodeLOC m_loc;
#endif

    // ASTBlockScopeContext is allocated by ASTAllocator
    inline void *operator new(size_t size, ASTAllocator &allocator)
    {
        return allocator.allocate(size);
    }

    void *operator new(size_t size) = delete;
    void *operator new[](size_t size) = delete;
    void operator delete(void *) = delete;
    void operator delete[](void *) = delete;

    ASTBlockScopeContext()
        : m_nodeType(ASTNodeType::ASTNodeTypeError)
        , m_blockIndex(LEXICAL_BLOCK_INDEX_MAX)
        , m_parentBlockIndex(LEXICAL_BLOCK_INDEX_MAX)
#ifndef NDEBUG
        , m_loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#endif
    {
    }
};

typedef Vector<ASTBlockScopeContext *, GCUtil::gc_malloc_atomic_allocator<ASTBlockScopeContext *>, ComputeReservedCapacityFunctionWithLog2<>> ASTBlockScopeContextVector;
typedef std::unordered_map<AtomicString, StorePositiveIntergerAsOdd, std::hash<AtomicString>, std::equal_to<AtomicString>,
                           GCUtil::gc_malloc_allocator<std::pair<AtomicString const, StorePositiveIntergerAsOdd>>>
    FunctionContextVarMap;


// context for function or program
struct ASTFunctionScopeContext {
    bool m_isStrict : 1;
    bool m_hasEval : 1;
    bool m_hasWith : 1;
    bool m_inWith : 1;
    bool m_isArrowFunctionExpression : 1;
    bool m_isClassConstructor : 1;
    bool m_isDerivedClassConstructor : 1;
    bool m_isClassMethod : 1;
    bool m_isClassStaticMethod : 1;
    bool m_isGenerator : 1;
    bool m_isAsync : 1;
    bool m_hasSuperOrNewTarget : 1;
    bool m_hasArrowParameterPlaceHolder : 1;
    bool m_hasParameterOtherThanIdentifier : 1;
    bool m_needsToComputeLexicalBlockStuffs : 1;
    bool m_allowSuperCall : 1;
    bool m_allowSuperProperty : 1;
    unsigned int m_nodeType : 2; // it is actually NodeType but used on FunctionExpression, ArrowFunctionExpression and FunctionDeclaration only
    unsigned int m_parameterCount : 16;
    LexicalBlockIndex m_functionBodyBlockIndex : 16;
    LexicalBlockIndex m_lexicalBlockIndexFunctionLocatedIn : 16;
    ASTFunctionScopeContextNameInfoVector m_varNames;
    FunctionContextVarMap *m_varNamesMap;
    AtomicStringTightVector m_parameters;
    AtomicString m_functionName;

    ASTFunctionScopeContext *m_firstChild;
    ASTFunctionScopeContext *m_nextSibling;
    ASTBlockScopeContextVector m_childBlockScopes;

    ExtendedNodeLOC m_functionStartLOC;
#ifndef NDEBUG
    ExtendedNodeLOC m_bodyEndLOC;
#else
    NodeLOC m_bodyEndLOC;
#endif

    // ASTFunctionScopeContext is allocated by ASTAllocator
    inline void *operator new(size_t size, ASTAllocator &allocator)
    {
        return allocator.allocate(size);
    }

    void *operator new(size_t size) = delete;
    void *operator new[](size_t size) = delete;
    void operator delete(void *) = delete;
    void operator delete[](void *) = delete;

    void appendChild(ASTFunctionScopeContext *child)
    {
        ASSERT(child->m_nextSibling == nullptr);
        if (m_firstChild == nullptr) {
            m_firstChild = child;
        } else {
            ASTFunctionScopeContext *tail = firstChild();
            while (tail->m_nextSibling != nullptr) {
                tail = tail->m_nextSibling;
            }
            tail->m_nextSibling = child;
        }
    }

    ASTFunctionScopeContext *firstChild()
    {
        return m_firstChild;
    }

    ASTFunctionScopeContext *nextSibling()
    {
        return m_nextSibling;
    }

    size_t findVarName(const AtomicString &name)
    {
        if (UNLIKELY(hasVarNamesMap())) {
            auto iter = m_varNamesMap->find(name);
            if (iter == m_varNamesMap->end()) {
                return SIZE_MAX;
            } else {
                return iter->second;
            }
        }

        auto &v = this->m_varNames;
        size_t size = v.size();

        if (LIKELY(size <= 12)) {
            size_t idx = SIZE_MAX;
            switch (size) {
            case 12:
                if (v[11].name() == name) {
                    idx = 11;
                }
                FALLTHROUGH;
#define TEST_ONCE(n)                                      \
    case n:                                               \
        if (idx == SIZE_MAX && v[n - 1].name() == name) { \
            idx = n - 1;                                  \
        }                                                 \
        FALLTHROUGH;
                TEST_ONCE(11)
                TEST_ONCE(10)
                TEST_ONCE(9)
                TEST_ONCE(8)
                TEST_ONCE(7)
                TEST_ONCE(6)
                TEST_ONCE(5)
                TEST_ONCE(4)
                TEST_ONCE(3)
                TEST_ONCE(2)
                TEST_ONCE(1)
#undef TEST_ONCE
            case 0:
                break;
            default:
                ASSERT_NOT_REACHED();
            }

            return idx;
        } else {
            for (size_t i = 0; i < size; i++) {
                if (v[i].name() == name) {
                    return i;
                }
            }
            return SIZE_MAX;
        }
    }

    ASTBlockScopeContext *findBlock(LexicalBlockIndex blockIndex)
    {
        size_t b = m_childBlockScopes.size();
        for (size_t i = 0; i < b; i++) {
            if (m_childBlockScopes[i]->m_blockIndex == blockIndex) {
                return m_childBlockScopes[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ASTBlockScopeContext *findBlockFromBackward(LexicalBlockIndex blockIndex)
    {
        int32_t b = (int32_t)m_childBlockScopes.size() - 1;
        return m_childBlockScopes[findBlockFromBackward(blockIndex, b)];
    }

    LexicalBlockIndex findBlockFromBackward(LexicalBlockIndex blockIndex, int32_t fromIndex)
    {
        for (int32_t i = fromIndex; i >= 0; i--) {
            if (m_childBlockScopes[(size_t)i]->m_blockIndex == blockIndex) {
                return i;
            }
        }
        ASSERT_NOT_REACHED();
        return LEXICAL_BLOCK_INDEX_MAX;
    }

    bool hasName(AtomicString name, LexicalBlockIndex blockIndex)
    {
        return hasNameAtBlock(name, blockIndex) || findVarName(name) != SIZE_MAX;
    }

    bool hasNameAtBlock(AtomicString name, LexicalBlockIndex blockIndex)
    {
        while (blockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            ASTBlockScopeContext *blockContext = findBlock(blockIndex);
            for (size_t i = 0; i < blockContext->m_names.size(); i++) {
                if (blockContext->m_names[i].name() == name) {
                    return true;
                }
            }

            blockIndex = blockContext->m_parentBlockIndex;
        }
        return false;
    }

    ALWAYS_INLINE bool hasVarNamesMap()
    {
        return m_varNamesMap;
    }

    void insertVarName(AtomicString name, LexicalBlockIndex blockIndex, bool isExplicitlyDeclaredOrParameterName, bool isVarDeclaration = true, bool isParameterName = false)
    {
        size_t idx = findVarName(name);

        if (idx != SIZE_MAX) {
            ASSERT(m_varNames[idx].name() == name);
            if (isExplicitlyDeclaredOrParameterName) {
                m_varNames[idx].setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
            }
            return;
        }

        ASTFunctionScopeContextNameInfo info;
        info.setName(name);
        info.setIsParameterName(isParameterName);
        info.setIsExplicitlyDeclaredOrParameterName(isExplicitlyDeclaredOrParameterName);
        info.setIsVarDeclaration(isVarDeclaration);
        info.setLexicalBlockIndex(blockIndex);
        m_varNames.push_back(info);

        if (UNLIKELY(hasVarNamesMap())) {
            m_varNamesMap->insert(std::make_pair(name, m_varNames.size() - 1));
        } else if (UNLIKELY(m_varNames.size() > 36)) {
            m_varNamesMap = new (GC) FunctionContextVarMap;

            for (size_t i = 0; i < m_varNames.size(); i++) {
                m_varNamesMap->insert(std::make_pair(m_varNames[i].name(), i));
            }
        }
    }

    ASTBlockScopeContext *insertBlockScope(ASTAllocator &allocator, LexicalBlockIndex blockIndex, LexicalBlockIndex parentBlockIndex, ExtendedNodeLOC loc)
    {
#ifndef NDEBUG
        size_t b = m_childBlockScopes.size();
        for (size_t i = 0; i < b; i++) {
            if (m_childBlockScopes[i]->m_blockIndex == blockIndex) {
                ASSERT_NOT_REACHED();
            }
        }
#endif

        ASTBlockScopeContext *newContext = new (allocator) ASTBlockScopeContext();
        newContext->m_blockIndex = blockIndex;
        newContext->m_parentBlockIndex = parentBlockIndex;
#ifndef NDEBUG
        newContext->m_loc = loc;
#endif
        m_childBlockScopes.push_back(newContext);

        return newContext;
    }

    bool insertNameAtBlock(AtomicString name, LexicalBlockIndex blockIndex, bool isConstBinding)
    {
        ASTBlockScopeContext *blockContext = findBlockFromBackward(blockIndex);
        for (size_t i = 0; i < blockContext->m_names.size(); i++) {
            if (blockContext->m_names[i].name() == name) {
                return false;
            }
        }

        ASTBlockScopeContextNameInfo info;
        info.setName(name);
        info.setIsConstBinding(isConstBinding);
        blockContext->m_names.push_back(info);
        return true;
    }

    bool blockHasName(AtomicString name, size_t blockIndex)
    {
        ASTBlockScopeContext *blockContext = findBlockFromBackward(blockIndex);
        for (size_t i = 0; i < blockContext->m_names.size(); i++) {
            if (blockContext->m_names[i].name() == name) {
                return true;
            }
        }
        return false;
    }

    struct FindNameResult {
        bool isFinded;
        bool isVarName;
        LexicalBlockIndex lexicalBlockIndexIfExists;

        FindNameResult()
            : isFinded(false)
            , isVarName(false)
            , lexicalBlockIndexIfExists(LEXICAL_BLOCK_INDEX_MAX)
        {
        }
    };

    bool canDeclareName(AtomicString name, LexicalBlockIndex blockIndex, bool isVarName, bool isExplicitVariableDeclaration)
    {
        if (isVarName) {
            int32_t findBlockIndex = m_childBlockScopes.size() - 1;
            findBlockIndex = findBlockFromBackward(blockIndex, findBlockIndex);
            ASTBlockScopeContext *cb = m_childBlockScopes[findBlockIndex];
            while (true) {
                size_t siz = cb->m_names.size();
                for (size_t i = 0; i < siz; i++) {
                    if (cb->m_names[i].name() == name) {
                        return false;
                    }
                }

                if (!isExplicitVariableDeclaration) {
                    break;
                }

                if (cb->m_parentBlockIndex == LEXICAL_BLOCK_INDEX_MAX) {
                    break;
                }
                findBlockIndex = findBlockFromBackward(cb->m_parentBlockIndex, findBlockIndex - 1);
                cb = m_childBlockScopes[findBlockIndex];
            }
        } else {
            if (blockHasName(name, blockIndex)) {
                return false;
            }

            size_t idx = findVarName(name);
            if (idx != SIZE_MAX) {
                if (m_varNames[idx].lexicalBlockIndex() >= blockIndex) {
                    return false;
                }
            }
        }

        return true;
    }

    explicit ASTFunctionScopeContext(ASTAllocator &allocator, bool isStrict = false)
        : m_isStrict(isStrict)
        , m_hasEval(false)
        , m_hasWith(false)
        , m_inWith(false)
        , m_isArrowFunctionExpression(false)
        , m_isClassConstructor(false)
        , m_isDerivedClassConstructor(false)
        , m_isClassMethod(false)
        , m_isClassStaticMethod(false)
        , m_isGenerator(false)
        , m_isAsync(false)
        , m_hasSuperOrNewTarget(false)
        , m_hasArrowParameterPlaceHolder(false)
        , m_hasParameterOtherThanIdentifier(false)
        , m_needsToComputeLexicalBlockStuffs(false)
        , m_allowSuperCall(false)
        , m_allowSuperProperty(false)
        , m_nodeType(ASTNodeType::Program)
        , m_parameterCount(0)
        , m_functionBodyBlockIndex(0)
        , m_lexicalBlockIndexFunctionLocatedIn(LEXICAL_BLOCK_INDEX_MAX)
        , m_varNamesMap(nullptr)
        , m_firstChild(nullptr)
        , m_nextSibling(nullptr)
        , m_functionStartLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#ifndef NDEBUG
        , m_bodyEndLOC(SIZE_MAX, SIZE_MAX, SIZE_MAX)
#else
        , m_bodyEndLOC(SIZE_MAX)
#endif
    {
    }
};
}

namespace std {

template <>
struct is_fundamental<Escargot::ASTFunctionScopeContextNameInfo> : public true_type {
};
}

#endif
