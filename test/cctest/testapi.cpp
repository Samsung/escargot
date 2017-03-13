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

#include "Escargot.h"
#include "api/EscargotPublic.h"

#define CHECK(name, cond) \
    printf(name" | %s\n", (cond) ? "pass" : "fail");

int main(int argc, char* argv[])
{
#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    printf("testapi begins\n");

    Escargot::Globals::initialize();
    Escargot::VMInstanceRef* vm = Escargot::VMInstanceRef::create();
    Escargot::ContextRef* ctx = Escargot::ContextRef::create(vm);
    Escargot::ExecutionStateRef* es
        = Escargot::ExecutionStateRef::create(ctx);

    const char* script = "4+3";
    const char* filename = "FileName.js";
    printf("evaluateScript %s=", script);

    Escargot::evaluateScript(es,
                             Escargot::StringRef::fromASCII(script),
                             Escargot::StringRef::fromASCII(filename));

    Escargot::ValueRef jsbool = Escargot::ValueRef::makeBoolean(es, true);
    Escargot::ValueRef jsnumber = Escargot::ValueRef::makeNumber(es, 123);
    Escargot::ValueRef jsundefined = Escargot::ValueRef::makeUndefined(es);
    Escargot::ValueRef jsnull = Escargot::ValueRef::makeNull(es);

    Escargot::ValueRef jstest = jsbool;
    CHECK("ValueRef type check  1", jstest.isBoolean(es));
    CHECK("ValueRef type check  2", !jstest.isNumber(es));
    CHECK("ValueRef type check  3", !jstest.isNull(es));
    CHECK("ValueRef type check  4", !jstest.isUndefined(es));
    jstest = jsnumber;
    CHECK("ValueRef type check  5", !jstest.isBoolean(es));
    CHECK("ValueRef type check  6", jstest.isNumber(es));
    CHECK("ValueRef type check  7", !jstest.isNull(es));
    CHECK("ValueRef type check  8", !jstest.isUndefined(es));
    jstest = jsnull;
    CHECK("ValueRef type check  9", !jstest.isBoolean(es));
    CHECK("ValueRef type check 10", !jstest.isNumber(es));
    CHECK("ValueRef type check 11", jstest.isNull(es));
    CHECK("ValueRef type check 12", !jstest.isUndefined(es));
    jstest = jsundefined;
    CHECK("ValueRef type check 13", !jstest.isBoolean(es));
    CHECK("ValueRef type check 14", !jstest.isNumber(es));
    CHECK("ValueRef type check 15", !jstest.isNull(es));
    CHECK("ValueRef type check 16", jstest.isUndefined(es));

    Escargot::Globals::finalize();
    es->destroy();
    ctx->destroy();
    vm->destroy();

    printf("testapi ended\n");

    return 0;
}
