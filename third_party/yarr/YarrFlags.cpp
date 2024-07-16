/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WTFBridge.h"
#include "YarrFlags.h"

namespace JSC { namespace Yarr {

Optional<OptionSet<Flags>> parseFlags(StringView string)
{
    OptionSet<Flags> flags;

    for (size_t i = 0; i < string.length(); i ++) {
        char16_t character = string[i];
        switch (character) {
#define JSC_HANDLE_REGEXP_FLAG(key, name, lowerCaseName, _) \
        case key: \
            if (flags.contains(Flags::name)) \
                return nullptr; \
            flags.add(Flags::name); \
            break;

        JSC_REGEXP_FLAGS(JSC_HANDLE_REGEXP_FLAG)

#undef JSC_HANDLE_REGEXP_FLAG

        default:
            return nullptr;
        }
    }

    // Can only specify one of 'u' and 'v' flags.
    if (flags.contains(Flags::Unicode) && flags.contains(Flags::UnicodeSets))
        return nullptr;

    return flags;
}

} } // namespace JSC::Yarr
