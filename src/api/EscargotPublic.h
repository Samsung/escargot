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

#ifndef __ESCARGOT_PUBLIC__
#define __ESCARGOT_PUBLIC__

#include <cstdlib> // size_t

namespace Escargot {

class StringRef {
public:
    static StringRef* fromASCII(const char* s);
};

class Globals {
public:
    static void initialize();
    static void finalize();
};

class VMInstanceRef {
public:
    static VMInstanceRef* create();
    void destroy();
};

class ObjectRef;
class ContextRef {
public:
    static ContextRef* create(VMInstanceRef* vminstance);
    void destroy();
    ObjectRef* globalObject();
};

class ExecutionStateRef {
public:
    static ExecutionStateRef* create(ContextRef* ctx);
    void destroy();
};

class ValueRef {
public:
    ValueRef(intptr_t val);
    static ValueRef makeBoolean(ExecutionStateRef* es, bool value);
    static ValueRef makeNumber(ExecutionStateRef* es, double value);
    static ValueRef makeNull(ExecutionStateRef* es);
    static ValueRef makeUndefined(ExecutionStateRef* es);
    bool isBoolean(ExecutionStateRef* es);
    bool isNumber(ExecutionStateRef* es);
    bool isNull(ExecutionStateRef* es);
    bool isUndefined(ExecutionStateRef* es);
    bool isObject(ExecutionStateRef* es);
    bool toBoolean(ExecutionStateRef* es);
    double toNumber(ExecutionStateRef* es);
private:
    intptr_t v;
};

class ObjectRef {
public:
    ObjectRef(intptr_t val);
    static ObjectRef makeObject(ExecutionStateRef* es);
    operator ValueRef();
private:
    intptr_t v;
};


bool evaluateScript(ExecutionStateRef* es, StringRef* script,
                    StringRef* fileName);

} // namespace Escargot
#endif
