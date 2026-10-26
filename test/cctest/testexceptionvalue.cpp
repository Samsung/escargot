/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#include "Escargot.h"
#include "runtime/ExceptionValue.h"
#include "runtime/String.h"
#include "gtest/gtest.h"
#include "gc/gc_mark.h"
#include <exception>

using namespace Escargot;

namespace {
struct StoredException {
    std::exception_ptr exception;
    void** weak;
};

NEVER_INLINE StoredException captureException(bool copy)
{
    try {
        throw ExceptionValue(new ASCIIString("original exception", 18));
    } catch (const Value& value) {
        auto** weak = static_cast<void**>(GC_MALLOC_ATOMIC_UNCOLLECTABLE(sizeof(void*)));
        *weak = value.asPointerValue();
        GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(weak, *weak);
        if (copy) {
            try {
                throw ExceptionValue(static_cast<const ExceptionValue&>(value));
            } catch (const Value&) {
                return { std::current_exception(), weak };
            }
        }
        return { std::current_exception(), weak };
    }
}

NEVER_INLINE void collectWithoutNativeRoots()
{
    volatile unsigned char cleared[128 * 1024];
    for (size_t i = 0; i < sizeof(cleared); ++i) {
        cleared[i] = 0;
    }
    GC_clear_stack(nullptr);
    GC_gcollect();
}

NEVER_INLINE void checkStoredException(const std::exception_ptr& exception)
{
    try {
        std::rethrow_exception(exception);
    } catch (const Value& value) {
        ASSERT_TRUE(value.isString());
        EXPECT_TRUE(value.asString()->equals("original exception"));
    }
}
} // namespace

TEST(ExceptionValue, RetainedExceptionSurvivesCollection)
{
    auto stored = captureException(false);
    collectWithoutNativeRoots();
    ASSERT_NE(*stored.weak, nullptr);
    checkStoredException(stored.exception);
    GC_FREE(stored.weak);
}

TEST(ExceptionValue, CopiedExceptionOwnsIndependentRoot)
{
    auto stored = captureException(true);
    collectWithoutNativeRoots();
    ASSERT_NE(*stored.weak, nullptr);
    checkStoredException(stored.exception);
    GC_FREE(stored.weak);
}

TEST(ExceptionValue, ConsumedExceptionReleasesRoot)
{
    auto stored = captureException(false);
    auto rootedBytes = GC_get_non_gc_bytes();
    stored.exception = nullptr;
    // Conservative stack retention, particularly under ASAN, may delay the
    // value's collection. Check that its uncollectable root is freed now.
    EXPECT_LT(GC_get_non_gc_bytes(), rootedBytes);
    EXPECT_FALSE(GC_is_disabled());
    GC_FREE(stored.weak);
}
