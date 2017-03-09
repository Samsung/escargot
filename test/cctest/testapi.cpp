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

int main(int argc, char* argv[])
{
#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    printf("BEGIN testapi\n");
    Escargot::Globals::initialize();
    Escargot::VMInstanceRef* vm = Escargot::VMInstanceRef::create();
    Escargot::ContextRef* ctx = Escargot::ContextRef::create(vm);
    Escargot::ExecutionStateRef* es
        = Escargot::ExecutionStateRef::create(ctx);
    Escargot::StringRef* scriptstr = Escargot::StringRef::fromASCII("4+3");
    Escargot::StringRef* filestr = Escargot::StringRef::fromASCII("scgt.js");
    Escargot::evaluateScript(ctx, scriptstr, filestr);
    Escargot::Globals::finalize();
    printf(" PASS testapi\n");

    return 0;
}
