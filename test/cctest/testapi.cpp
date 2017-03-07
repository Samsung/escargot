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
#include "runtime/VMInstance.h"
#include "runtime/ExecutionContext.h"
#include "runtime/Value.h"
#include "util/Vector.h"
#include "api/EscargotPublic.h"

int main(int argc, char* argv[])
{
#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    printf("BEGIN testapi\n");
    Escargot::Globals::initialize();
    Escargot::VMInstanceRef* instance = Escargot::VMInstanceRef::create();
    Escargot::ContextRef* context = Escargot::ContextRef::create(instance);
    Escargot::ExecutionStateRef* stateForInit = Escargot::ExecutionStateRef::create(context);
    Escargot::Globals::finalize();
    printf(" PASS testapi\n");

    return 0;
}
