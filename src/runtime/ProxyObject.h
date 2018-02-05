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

#if ESCARGOT_ENABLE_PROXY

#ifndef __EscargotProxyObject__
#define __EscargotProxyObject__

#include "runtime/Object.h"

namespace Escargot {

class ProxyObject : public Object {
public:
    ProxyObject(ExecutionState& state);

    virtual bool isProxyObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Proxy";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

};

}

#endif // __EscargotProxyObject__
#endif // ESCARGOT_ENABLE_PROXY
