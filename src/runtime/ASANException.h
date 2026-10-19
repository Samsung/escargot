/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 or later.
 */
#ifndef __EscargotASANException__
#define __EscargotASANException__

#ifdef ASAN_ENABLED
#include "runtime/Value.h"

namespace Escargot {

// BDWGC cannot reliably scan ASAN exception handlers. The exception object
// owns the GC pause through unwinding, including handlers that consume it.
// Each copy owns a nested pause, including non-elided throw temporaries.
class ASANExceptionValue final : public Value {
public:
    explicit ASANExceptionValue(const Value& value)
        : Value(value)
    {
        GC_disable();
    }

    ASANExceptionValue(const ASANExceptionValue& other)
        : Value(other)
    {
        GC_disable();
    }

    ASANExceptionValue& operator=(const ASANExceptionValue&) = delete;

    ~ASANExceptionValue() noexcept
    {
        GC_enable();
    }
};

} // namespace Escargot
#endif // ASAN_ENABLED

#endif // __EscargotASANException__
