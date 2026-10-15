#include "Escargot.h"

#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCacheReaderWriter.h"
#include "runtime/Context.h"
#include "runtime/ThreadLocal.h"
#include "runtime/VMInstance.h"
#include "gtest/gtest.h"

using namespace Escargot;

namespace {
template <typename T>
void checkTypedReadBoundaries()
{
    for (size_t remaining = 0; remaining <= sizeof(T); ++remaining) {
        CodeCacheReader::CacheBuffer buffer;
        buffer.resize(sizeof(bool) + remaining);
        memset(buffer.data(), 0, sizeof(bool) + remaining);
        EXPECT_FALSE(buffer.get<bool>());
        if (remaining < sizeof(T)) {
            EXPECT_THROW(buffer.get<T>(), CodeCacheReader::Error);
            EXPECT_EQ(buffer.index(), sizeof(bool));
        } else {
            EXPECT_EQ(buffer.get<T>(), T());
            EXPECT_EQ(buffer.index(), sizeof(bool) + sizeof(T));
            EXPECT_THROW(buffer.get<T>(), CodeCacheReader::Error);
        }
    }
}
} // namespace

TEST(CodeCacheReader, TypedReadBoundaries)
{
    checkTypedReadBoundaries<bool>();
    checkTypedReadBoundaries<uint8_t>();
    checkTypedReadBoundaries<uint16_t>();
    checkTypedReadBoundaries<uint32_t>();
    checkTypedReadBoundaries<uint64_t>();
    checkTypedReadBoundaries<size_t>();
    checkTypedReadBoundaries<int>();
    checkTypedReadBoundaries<slimb_t>();
    checkTypedReadBoundaries<limb_t>();
}

TEST(CodeCacheReader, LengthPrefixedReadBoundaries)
{
    for (size_t count : { size_t(0), size_t(1), size_t(2), size_t(3), SIZE_MAX, SIZE_MAX / sizeof(uint16_t) + 1 }) {
        CodeCacheReader::CacheBuffer buffer;
        buffer.resize(2 * sizeof(uint16_t));
        memset(buffer.data(), 0, 2 * sizeof(uint16_t));
        uint16_t destination[2] = { 7, 7 };
        if (count <= 2) {
            EXPECT_NO_THROW(buffer.getData(destination, count));
            EXPECT_EQ(buffer.index(), count * sizeof(uint16_t));
        } else {
            EXPECT_THROW(buffer.getData(destination, count), CodeCacheReader::Error);
            EXPECT_EQ(buffer.index(), 0u);
            EXPECT_EQ(destination[0], 7);
            EXPECT_EQ(destination[1], 7);
        }
    }
}

TEST(CodeCacheReader, TruncatedStringsRejectBeforeAllocation)
{
    for (bool is8Bit : { false, true }) {
        CodeCacheReader::CacheBuffer buffer;
        const size_t length = 3;
        const size_t bytes = sizeof(bool) + sizeof(size_t) + length * (is8Bit ? 1 : sizeof(char16_t)) - 1;
        buffer.resize(bytes);
        memset(buffer.data(), 0, bytes);
        memcpy(buffer.data(), &is8Bit, sizeof(is8Bit));
        memcpy(buffer.data() + sizeof(is8Bit), &length, sizeof(length));
        EXPECT_THROW(buffer.getString(), CodeCacheReader::Error);
        EXPECT_EQ(buffer.index(), sizeof(bool) + sizeof(size_t));
    }
}

TEST(CodeCacheReader, BigIntLengthOverflowRejectsBeforeAllocation)
{
    CodeCacheReader::CacheBuffer buffer;
    const size_t bytes = sizeof(int) + sizeof(slimb_t) + sizeof(limb_t);
    buffer.resize(bytes);
    memset(buffer.data(), 0, bytes);
    const limb_t length = std::numeric_limits<limb_t>::max();
    memcpy(buffer.data() + sizeof(int) + sizeof(slimb_t), &length, sizeof(length));
    EXPECT_THROW(buffer.getBF(ThreadLocal::bfContext()), CodeCacheReader::Error);
}

TEST(CodeCacheReader, TruncatedRecordAndShortFile)
{
    std::unique_ptr<FILE, int (*)(FILE*)> file(tmpfile(), fclose);
    ASSERT_NE(file.get(), nullptr);
    const bool has16BitString = false;
    const size_t one = 1;
    ASSERT_EQ(fwrite(&has16BitString, sizeof(has16BitString), 1, file.get()), 1u);
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(fwrite(&one, sizeof(one), 1, file.get()), 1u);
    }
    const size_t recordSize = sizeof(bool) + 3 * sizeof(size_t);
    rewind(file.get());
    CodeCacheReader reader;
    ASSERT_TRUE(reader.loadData(file.get(), recordSize));
    auto* instance = new VMInstance();
    auto* context = new Context(instance);
    EXPECT_THROW(reader.loadStringTable(context), CodeCacheReader::Error);
    reader.clearBuffer();
    rewind(file.get());
    EXPECT_FALSE(reader.loadData(file.get(), recordSize + 1));
    EXPECT_EQ(reader.bufferIndex(), 0u);
}
#endif
