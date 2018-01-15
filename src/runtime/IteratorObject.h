/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotIteratorObject__
#define __EscargotIteratorObject__

#include "runtime/Object.h"

namespace Escargot {

class ArrayIteratorObject;
class StringIteratorObject;
class MapIteratorObject;
class SetIteratorObject;

class IteratorObject : public Object {
public:
    IteratorObject(ExecutionState& state);

    virtual bool isIteratorObject() const
    {
        return true;
    }

    virtual bool isArrayIteratorObject() const
    {
        return false;
    }

    virtual bool isStringIteratorObject() const
    {
        return false;
    }

    virtual bool isMapIteratorObject() const
    {
        return false;
    }

    virtual bool isSetIteratorObject() const
    {
        return false;
    }

    ArrayIteratorObject* asArrayIteratorObject()
    {
        ASSERT(isArrayIteratorObject());
        return (ArrayIteratorObject*)this;
    }

    StringIteratorObject* asStringIteratorObject()
    {
        ASSERT(isStringIteratorObject());
        return (StringIteratorObject*)this;
    }

    MapIteratorObject* asMapIteratorObject()
    {
        ASSERT(isMapIteratorObject());
        return (MapIteratorObject*)this;
    }

    SetIteratorObject* asSetIteratorObject()
    {
        ASSERT(isSetIteratorObject());
        return (SetIteratorObject*)this;
    }

    Value next(ExecutionState& state);

    virtual const char* internalClassProperty() = 0;
    virtual std::pair<Value, bool> advance(ExecutionState& state) = 0; // implemention should return pair<Value, done>
protected:
};
}

#endif
