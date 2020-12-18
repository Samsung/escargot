/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#include "Escargot.h"
#include "Symbol.h"
#include "Value.h"
#include "VMInstance.h"

namespace Escargot {

Symbol* Symbol::fromGlobalSymbolRegistry(VMInstance* vm, String* stringKey)
{
    // Let stringKey be ? ToString(key).
    // For each element e of the GlobalSymbolRegistry List,
    auto& list = vm->globalSymbolRegistry();
    for (size_t i = 0; i < list.size(); i++) {
        // If SameValue(e.[[Key]], stringKey) is true, return e.[[Symbol]].
        if (list[i].key->equals(stringKey)) {
            return list[i].symbol;
        }
    }
    // Assert: GlobalSymbolRegistry does not currently contain an entry for stringKey.
    // Let newSymbol be a new unique Symbol value whose [[Description]] value is stringKey.
    Symbol* newSymbol = new Symbol(stringKey);
    // Append the Record { [[Key]]: stringKey, [[Symbol]]: newSymbol } to the GlobalSymbolRegistry List.
    GlobalSymbolRegistryItem item;
    item.key = stringKey;
    item.symbol = newSymbol;
    list.pushBack(item);
    // Return newSymbol.
    return newSymbol;
}

String* Symbol::symbolDescriptiveString() const
{
    StringBuilder sb;
    sb.appendString("Symbol(");
    if (description().hasValue())
        sb.appendString(description().value());
    sb.appendString(")");
    return sb.finalize();
}
} // namespace Escargot
