/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __ESCARGOT_APICAST__
#define __ESCARGOT_APICAST__

namespace Escargot {

inline VMInstance* toImpl(VMInstanceRef* v)
{
    return reinterpret_cast<VMInstance*>(v);
}

inline VMInstanceRef* toRef(VMInstance* v)
{
    return reinterpret_cast<VMInstanceRef*>(v);
}

inline Context* toImpl(ContextRef* v)
{
    return reinterpret_cast<Context*>(v);
}

inline ContextRef* toRef(Context* v)
{
    return reinterpret_cast<ContextRef*>(v);
}

inline ExecutionState* toImpl(ExecutionStateRef* v)
{
    return reinterpret_cast<ExecutionState*>(v);
}

inline ExecutionStateRef* toRef(ExecutionState* v)
{
    return reinterpret_cast<ExecutionStateRef*>(v);
}

inline String* toImpl(StringRef* v)
{
    return reinterpret_cast<String*>(v);
}

inline StringRef* toRef(String* v)
{
    return reinterpret_cast<StringRef*>(v);
}

inline Value* toImpl(ValueRef* v)
{
    return reinterpret_cast<Value*>(v);
}

inline ValueRef* toRef(Value* v)
{
    return reinterpret_cast<ValueRef*>(v);
}

inline Object* toImpl(ObjectRef* v)
{
    return reinterpret_cast<Object*>(v);
}

inline ObjectRef* toRef(Object* v)
{
    return reinterpret_cast<ObjectRef*>(v);
}

} // namespace Escargot
#endif
