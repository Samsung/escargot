/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#include "Escargot.h"
#include "api/EscargotPublic.h"
#include "interpreter/ByteCode.h"
#include "runtime/Object.h"
#include "runtime/String.h"
#include "runtime/Symbol.h"
#include "gtest/gtest.h"
#include "gc/gc_mark.h"

using namespace Escargot;

namespace {
const char leftText[] = "0123456789abcdefghijklmnopqrstuvwxyz";

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
NEVER_INLINE ObjectStructureItem* makeItems()
{
    auto* items = CompressiblePointerAllocator<ObjectStructureItem>().allocate(2);
    auto* symbol = new Symbol(new ASCIIString(leftText, sizeof(leftText) - 1));
    auto* native = new ObjectPropertyNativeGetterSetterData(true, true, false, nullptr, nullptr);
    new (&items[0]) ObjectStructureItem(ObjectStructurePropertyName(symbol),
                                        ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(native));
    // After this helper returns, the intern table cannot keep this key alive.
    AtomicStringMap names;
    AtomicString atomic(&names, leftText, sizeof(leftText) - 1);
    new (&items[1]) ObjectStructureItem(ObjectStructurePropertyName(atomic),
                                        ObjectStructurePropertyDescriptor::createDataDescriptor());
    return items;
}

#endif

NEVER_INLINE CreateObjectPrepare::CreateObjectData* makeLiteralData()
{
    auto* data = new CreateObjectPrepare::CreateObjectData(false, false, false,
                                                           false, false, 3, nullptr, nullptr);
    for (size_t i = 0; i < 3; ++i) {
        auto* symbol = new Symbol(new ASCIIString(leftText, sizeof(leftText) - 1));
        data->m_properties.pushBack(ObjectStructureItem(ObjectStructurePropertyName(symbol),
                                                        ObjectStructurePropertyDescriptor::createDataDescriptor()));
    }
    return data;
}

NEVER_INLINE void checkLiteralData(CreateObjectPrepare::CreateObjectData* data)
{
    ASSERT_EQ(data->m_properties.size(), 3u);
    for (size_t i = 0; i < data->m_properties.size(); ++i) {
        ASSERT_TRUE(data->m_properties[i].m_propertyName.isSymbol());
        EXPECT_TRUE(data->m_properties[i].m_propertyName.symbol()->descriptionString()->equals(leftText));
    }
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

TEST(CompactObjectStructures, LiteralPropertyNamesSurviveGC)
{
    PersistentRefHolder<CreateObjectPrepare::CreateObjectData> root(makeLiteralData());
    collectAndChurn();
    // Keep decoded pointers out of this frame until after the collections.
    checkLiteralData(root.get());
}

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
TEST(CompactObjectStructures, PropertyItemNamesAndNativeDescriptorSurviveGC)
{
    PersistentRefHolder<ObjectStructureItem> root(makeItems());
    collectAndChurn();
    ASSERT_TRUE(root.get()[0].m_propertyName.isSymbol());
    EXPECT_TRUE(root.get()[0].m_propertyName.symbol()->descriptionString()->equals(leftText));
    auto& descriptor = root.get()[0].m_descriptor;
    ASSERT_TRUE(descriptor.isNativeAccessorProperty());
    EXPECT_TRUE(descriptor.isWritable());
    EXPECT_TRUE(descriptor.isEnumerable());
    EXPECT_FALSE(descriptor.isConfigurable());
    EXPECT_TRUE(root.get()[1].m_propertyName.hasAtomicString());
    EXPECT_TRUE(root.get()[1].m_propertyName.plainString()->equals(leftText));
    std::printf("COMPACT_LAYOUT Item sizeof=%zu allocated_two=%zu\n",
                sizeof(ObjectStructureItem), GC_size(GC_base(root.get())));
}
#endif
