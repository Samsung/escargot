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

#ifndef __EscargotValueAdapter__
#define __EscargotValueAdapter__

#include "runtime/Value.h"
#include "runtime/EncodedValue.h"

namespace Escargot {

class ValueRef;

inline ValueRef* toRef(const Value& v)
{
    ASSERT(!v.isEmpty());
    return reinterpret_cast<ValueRef*>(EncodedValue(v).payload());
}

inline Value toImpl(const ValueRef* v)
{
    ASSERT(v);
    return Value(EncodedValue::fromPayload(v));
}


#define DEFINE_CAST(ClassName)                       \
    class ClassName;                                 \
    class ClassName##Ref;                            \
    inline ClassName* toImpl(ClassName##Ref* v)      \
    {                                                \
        return reinterpret_cast<ClassName*>(v);      \
    }                                                \
    inline ClassName##Ref* toRef(ClassName* v)       \
    {                                                \
        return reinterpret_cast<ClassName##Ref*>(v); \
    }

DEFINE_CAST(VMInstance);
DEFINE_CAST(Context);
DEFINE_CAST(ExecutionState);
DEFINE_CAST(String);
DEFINE_CAST(RopeString);
DEFINE_CAST(Symbol);
DEFINE_CAST(BigInt);
DEFINE_CAST(PointerValue);
DEFINE_CAST(Object)
DEFINE_CAST(IteratorObject);
DEFINE_CAST(ArrayObject)
DEFINE_CAST(StringObject)
DEFINE_CAST(SymbolObject)
DEFINE_CAST(BigIntObject)
DEFINE_CAST(NumberObject)
DEFINE_CAST(BooleanObject)
DEFINE_CAST(RegExpObject)
DEFINE_CAST(ErrorObject)
DEFINE_CAST(ReferenceErrorObject);
DEFINE_CAST(TypeErrorObject);
DEFINE_CAST(SyntaxErrorObject);
DEFINE_CAST(RangeErrorObject);
DEFINE_CAST(URIErrorObject);
DEFINE_CAST(EvalErrorObject);
DEFINE_CAST(AggregateErrorObject);
DEFINE_CAST(GlobalObject);
DEFINE_CAST(GlobalObjectProxyObject);
DEFINE_CAST(FunctionObject);
DEFINE_CAST(DateObject);
DEFINE_CAST(PromiseObject);
DEFINE_CAST(ProxyObject);
DEFINE_CAST(Job);
DEFINE_CAST(Script);
DEFINE_CAST(ScriptParser);
DEFINE_CAST(ArrayBufferObject);
DEFINE_CAST(ArrayBufferView);
DEFINE_CAST(Int8ArrayObject);
DEFINE_CAST(Uint8ArrayObject);
DEFINE_CAST(Int16ArrayObject);
DEFINE_CAST(Uint16ArrayObject);
DEFINE_CAST(Int32ArrayObject);
DEFINE_CAST(Uint32ArrayObject);
DEFINE_CAST(Uint8ClampedArrayObject);
DEFINE_CAST(Float32ArrayObject);
DEFINE_CAST(Float64ArrayObject);
DEFINE_CAST(BigInt64ArrayObject);
DEFINE_CAST(BigUint64ArrayObject);
DEFINE_CAST(SetObject);
DEFINE_CAST(WeakSetObject);
DEFINE_CAST(MapObject);
DEFINE_CAST(WeakMapObject);
DEFINE_CAST(Template);
DEFINE_CAST(ObjectTemplate);
DEFINE_CAST(FunctionTemplate);

#undef DEFINE_CAST

} // namespace Escargot

#endif
