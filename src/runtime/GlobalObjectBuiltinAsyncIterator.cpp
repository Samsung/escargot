/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"

namespace Escargot {

static Value builtinAsyncIteratorIterator(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return thisValue;
}

void GlobalObject::installAsyncIterator(ExecutionState& state)
{
    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-%iteratorprototype%-object
    m_asyncIteratorPrototype = new Object(state);
    m_asyncIteratorPrototype->markThisObjectDontNeedStructureTransitionTable();

    // https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asynciteratorprototype-asynciterator
    m_asyncIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().asyncIterator),
                                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(AtomicString(state, String::fromASCII("[Symbol.asyncIterator]")), builtinAsyncIteratorIterator, 0, NativeFunctionInfo::Strict)),
                                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
