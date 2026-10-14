/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#include "Escargot.h"
#include "api/EscargotPublic.h"
#include "runtime/AtomicString.h"
#include "runtime/RopeString.h"
#include "runtime/StringView.h"
#include "gtest/gtest.h"
#include "gc/gc_mark.h"

using namespace Escargot;

namespace {
const char leftText[] = "0123456789abcdefghijklmnopqrstuvwxyz";
const char16_t rightText[] = u"한글0123456789abcdefghijklmnopqrstuvwxyz";

NEVER_INLINE String* makeMixedRope()
{
    return RopeString::createRopeString(new ASCIIString(leftText, sizeof(leftText) - 1),
                                        new UTF16String(rightText, sizeof(rightText) / 2 - 1));
}

NEVER_INLINE StringView* makeOwnedView()
{
    return StringView::create(new ASCIIString(leftText, sizeof(leftText) - 1), 5, 31);
}

NEVER_INLINE std::pair<UTF16StringFromExternalMemory*, void**> makeExternalAlias()
{
    auto* victim = new ASCIIString(leftText, sizeof(leftText) - 1);
    auto** weak = static_cast<void**>(GC_MALLOC_ATOMIC_UNCOLLECTABLE(sizeof(void*)));
    *weak = victim;
    GC_general_register_disappearing_link(weak, victim);
    // An opaque, unread external address with the same low bits as a GC object.
    // A native marker must not reinterpret it as that object's cage offset.
    uintptr_t external = reinterpret_cast<uintptr_t>(victim) + (uint64_t(1) << 32);
    auto* string = new UTF16StringFromExternalMemory(reinterpret_cast<const char16_t*>(external), 1);
    return std::make_pair(string, weak);
}

NEVER_INLINE void collectAndChurn()
{
    volatile unsigned char cleared[128 * 1024];
    for (size_t i = 0; i < sizeof(cleared); ++i) {
        cleared[i] = 0;
    }
    GC_clear_stack(nullptr);
    GC_gcollect();
    // Reuse objects and buffers that would have been reclaimed by a missing edge.
    for (size_t i = 0; i < 4000; ++i) {
        new ASCIIString("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 40);
        new UTF16String(u"yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy", 40);
    }
    GC_gcollect();
}
} // namespace

TEST(CompactStrings, RopeChildrenAndFlattenedBufferSurviveGC)
{
    PersistentRefHolder<String> root(makeMixedRope());
    auto* rope = static_cast<RopeString*>(root.get());
    ASSERT_FALSE(rope->wasFlattened());
    collectAndChurn();
    ASSERT_FALSE(rope->wasFlattened());
    EXPECT_TRUE(rope->left()->equals(leftText));
    EXPECT_EQ(rope->right()->charAt(0), u'한');
    {
        auto data = rope->bufferAccessData();
        ASSERT_FALSE(data.has8BitContent);
        EXPECT_EQ(data.charAt(sizeof(leftText) - 1), u'한');
    }
    ASSERT_TRUE(rope->wasFlattened());
    collectAndChurn();
    EXPECT_EQ(rope->charAt(0), u'0');
    EXPECT_EQ(rope->charAt(sizeof(leftText)), u'글');
    std::printf("COMPACT_LAYOUT Rope sizeof=%zu allocated=%zu\n",
                sizeof(RopeString), GC_size(GC_base(rope)));
}

TEST(CompactStrings, ViewParentSurvivesGC)
{
    PersistentRefHolder<StringView> root(makeOwnedView());
    collectAndChurn();
    EXPECT_EQ(root->start(), 5u);
    EXPECT_EQ(root->length(), 26u);
    EXPECT_EQ(root->charAt(0), u'5');
    EXPECT_EQ(root->charAt(25), u'u');
    StringView nested(root.get(), 3, 20);
    EXPECT_EQ(nested.start(), 8u);
    EXPECT_EQ(nested.underlyingString(), root->underlyingString());
    collectAndChurn();
    EXPECT_EQ(nested.charAt(0), u'8');
    std::printf("COMPACT_LAYOUT View sizeof=%zu allocated=%zu\n",
                sizeof(StringView), GC_size(GC_base(root.get())));
}

TEST(CompactStrings, ViewAtomicStringIdentitySurvivesRepeatedLookup)
{
    AtomicStringMap names;
    AtomicString canonical(&names, "56789abcdefghijklmnopqrstu", 26);
    PersistentRefHolder<StringView> view(makeOwnedView());
    AtomicString first(&names, view.get());
    EXPECT_EQ(first.string(), canonical.string());
    ASSERT_TRUE(view->isAtomicStringSource());
    EXPECT_EQ(view->canonicalAtomicString(), first.string());
    AtomicString again(&names, view.get());
    EXPECT_EQ(again.string(), first.string());
    EXPECT_TRUE(again.string()->equals("56789abcdefghijklmnopqrstu"));
}

TEST(CompactStrings, HeapStringsWithExternalBuffersSurviveGC)
{
    PersistentRefHolder<ASCIIStringFromExternalMemory> left(new ASCIIStringFromExternalMemory(leftText, sizeof(leftText) - 1));
    PersistentRefHolder<UTF16StringFromExternalMemory> right(new UTF16StringFromExternalMemory(rightText, sizeof(rightText) / 2 - 1));
    StringView stackView(left.get(), 3, 30);
    StringView nested(stackView, 2, 20);
    StringView copy(nested);
    StringView assigned;
    EXPECT_EQ(assigned.underlyingString(), String::emptyString());
    assigned = nested;
    PersistentRefHolder<StringView> heapView(StringView::create(left.get(), 4, 32));
    PersistentRefHolder<String> root(RopeString::createRopeString(left.get(), right.get()));
    collectAndChurn();
    EXPECT_EQ(stackView.underlyingString(), left.get());
    EXPECT_EQ(nested.underlyingString(), left.get());
    EXPECT_EQ(nested.start(), 5u);
    EXPECT_EQ(nested.charAt(0), u'5');
    EXPECT_EQ(copy.underlyingString(), left.get());
    EXPECT_EQ(copy.charAt(0), u'5');
    EXPECT_EQ(assigned.underlyingString(), left.get());
    EXPECT_EQ(assigned.charAt(0), u'5');
    EXPECT_EQ(heapView->underlyingString(), left.get());
    EXPECT_EQ(heapView->charAt(0), u'4');
    auto* rope = static_cast<RopeString*>(root.get());
    EXPECT_EQ(rope->left(), left.get());
    EXPECT_EQ(rope->right(), right.get());
    {
        auto data = rope->bufferAccessData();
        EXPECT_EQ(data.charAt(sizeof(leftText) - 1), u'한');
    }
    collectAndChurn();
    EXPECT_EQ(rope->charAt(sizeof(leftText)), u'글');
}

TEST(CompactStrings, ExternalUTF16BufferDoesNotRetainCageAlias)
{
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    auto alias = makeExternalAlias();
    PersistentRefHolder<UTF16StringFromExternalMemory> root(alias.first);
    collectAndChurn();
    EXPECT_EQ(*alias.second, nullptr);
    GC_unregister_disappearing_link(alias.second);
    GC_FREE(alias.second);
#endif
}
