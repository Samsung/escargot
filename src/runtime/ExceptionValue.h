/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#ifndef __EscargotExceptionValue__
#define __EscargotExceptionValue__

#include "runtime/Value.h"

namespace Escargot {

// C++ exception storage is not scanned by BDWGC. Keep each thrown value
// rooted independently until its exception object is destroyed, including
// while handlers run JavaScript that throws and catches other exceptions.
class ExceptionValue final : public Value {
public:
    explicit ExceptionValue(const Value& value)
        : Value(value)
        , m_root(nullptr)
    {
#ifdef ASAN_ENABLED
        // ASAN can also hide live values on the unwound native stack.
        GC_disable();
#endif
        if (isPointerValue()) {
            // Keep a native pointer live across the root allocation, which
            // can itself collect before C++ exception storage is rooted.
            PointerValue* pointer = asPointerValue();
            m_root = static_cast<PointerValue**>(GC_MALLOC_UNCOLLECTABLE(sizeof(PointerValue*)));
            RELEASE_ASSERT(m_root);
            *m_root = pointer;
            GC_reachable_here(pointer);
        }
    }

    ExceptionValue(const ExceptionValue& other)
        : ExceptionValue(static_cast<const Value&>(other))
    {
    }

    ExceptionValue& operator=(const ExceptionValue&) = delete;

    ~ExceptionValue() noexcept
    {
        if (m_root) {
            GC_FREE(m_root);
        }
#ifdef ASAN_ENABLED
        GC_enable();
#endif
    }

private:
    PointerValue** m_root;
};

} // namespace Escargot

#endif // __EscargotExceptionValue__
