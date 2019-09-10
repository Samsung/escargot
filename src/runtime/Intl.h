#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
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

#ifndef __EscargotIntlObject__
#define __EscargotIntlObject__

namespace Escargot {

class Intl {
public:
    typedef std::vector<std::string> (*LocaleDataImplFunction)(String* locale, size_t keyIndex);

    struct IntlMatcherResult {
        IntlMatcherResult()
        {
            extension = locale = String::emptyString;
            extensionIndex = SIZE_MAX;
        }

        String* locale;
        String* extension;
        size_t extensionIndex;
    };

    static ValueVector canonicalizeLocaleList(ExecutionState& state, Value locales);
    static StringMap* resolveLocale(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, StringMap* options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData);
    static Value supportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options);
    static Value getOption(ExecutionState& state, Object* options, Value property, String* type, ValueVector values, const Value& fallback);

    static std::vector<std::string> numberingSystemsForLocale(String* locale);
};
} // namespace Escargot

#endif
#endif
