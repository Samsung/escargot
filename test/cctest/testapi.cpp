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

    const char* script = "4+3";
    const char* filename = "FileName.js";
    printf("evaluateScript %s=", script);

    Escargot::ScriptRef* scriptRef = ctx->scriptParser()->parse(Escargot::StringRef::fromASCII(script, strlen(script)), Escargot::StringRef::fromASCII(filename, strlen(filename))).m_script;
    Escargot::SandBoxRef* sb = Escargot::SandBoxRef::create(ctx);
    auto sandBoxResult = sb->run([&](Escargot::ExecutionStateRef* state) -> Escargot::ValueRef* {
        return scriptRef->execute(state);
    });

    Escargot::ValueRef* evalResult = sandBoxResult.result;
    sb->destroy();

    Escargot::ExecutionStateRef* es = Escargot::ExecutionStateRef::create(ctx);
    puts(evalResult->toString(es)->toStdUTF8String().c_str());

    Escargot::ValueRef* jsbool = Escargot::ValueRef::create(true);
    Escargot::ValueRef* jsnumber = Escargot::ValueRef::create(123);
    Escargot::ValueRef* jsundefined = Escargot::ValueRef::createUndefined();
    Escargot::ValueRef* jsnull = Escargot::ValueRef::createNull();
    Escargot::ValueRef* jsobject = Escargot::ValueRef::create(Escargot::ObjectRef::create(es));

    Escargot::ValueRef* jstest = jsbool;
    CHECK("ValueRef type check  1",  jstest->isBoolean());
    CHECK("ValueRef type check  2", !jstest->isNumber());
    CHECK("ValueRef type check  3", !jstest->isNull());
    CHECK("ValueRef type check  4", !jstest->isUndefined());
    CHECK("ValueRef type check  5", !jstest->isObject());

    jstest = jsnumber;
    CHECK("ValueRef type check  6", !jstest->isBoolean());
    CHECK("ValueRef type check  7",  jstest->isNumber());
    CHECK("ValueRef type check  8", !jstest->isNull());
    CHECK("ValueRef type check  9", !jstest->isUndefined());
    CHECK("ValueRef type check 10", !jstest->isObject());

    jstest = jsnull;
    CHECK("ValueRef type check 11", !jstest->isBoolean());
    CHECK("ValueRef type check 12", !jstest->isNumber());
    CHECK("ValueRef type check 13",  jstest->isNull());
    CHECK("ValueRef type check 14", !jstest->isUndefined());
    CHECK("ValueRef type check 15", !jstest->isObject());

    jstest = jsundefined;
    CHECK("ValueRef type check 16", !jstest->isBoolean());
    CHECK("ValueRef type check 17", !jstest->isNumber());
    CHECK("ValueRef type check 18", !jstest->isNull());
    CHECK("ValueRef type check 19",  jstest->isUndefined());
    CHECK("ValueRef type check 20", !jstest->isObject());

    jstest = jsobject;
    CHECK("ValueRef type check 21", !jstest->isBoolean());
    CHECK("ValueRef type check 22", !jstest->isNumber());
    CHECK("ValueRef type check 23", !jstest->isNull());
    CHECK("ValueRef type check 24", !jstest->isUndefined());
    CHECK("ValueRef type check 25",  jstest->isObject());

    CHECK("ValueRef type conversion 1", jsbool->toBoolean(es));
    CHECK("ValueRef type conversion 2", jsnumber->toNumber(es) == 123);

    Escargot::ObjectRef* globalObject = ctx->globalObject();
    CHECK("globalObject() is not null", !Escargot::ValueRef::create(globalObject)->isNull());
    CHECK("globalObject() is object", Escargot::ValueRef::create(globalObject)->isObject());
    Escargot::ValueRef* jsmath
        = globalObject->get(es, Escargot::ValueRef::create(Escargot::StringRef::fromASCII("Math")));
    Escargot::ValueRef* jspi
        = jsmath->toObject(es)->get(es, Escargot::ValueRef::create(Escargot::StringRef::fromASCII("PI")));
    printf("Math.PI = %f\n", jspi->toNumber(es));

    // custom function & NativeDataAccessorProperty test
    {
        Escargot::FunctionObjectRef::NativeFunctionInfo info(Escargot::AtomicStringRef::create(ctx, "Custom"), [](Escargot::ExecutionStateRef* state, Escargot::ValueRef* thisValue, size_t argc, Escargot::ValueRef** argv, bool isNewExpression) -> Escargot::ValueRef* {
            puts("custom function called");
            return Escargot::ValueRef::createUndefined();
        }, 0, [](Escargot::ExecutionStateRef* state, size_t argc, Escargot::ValueRef** argv) -> Escargot::ObjectRef* {
            puts("custom ctor called");
            return Escargot::ObjectRef::create(state);
        });
        Escargot::FunctionObjectRef* fn = Escargot::FunctionObjectRef::create(es, info);

        Escargot::ObjectRef::NativeDataAccessorPropertyData* nativeData = new Escargot::ObjectRef::NativeDataAccessorPropertyData(true, true, true, [](Escargot::ExecutionStateRef* state, Escargot::ObjectRef* self, Escargot::ObjectRef::NativeDataAccessorPropertyData* data) -> Escargot::ValueRef* {
            puts("native getter called");
            return Escargot::ValueRef::create(120);
        }, [](Escargot::ExecutionStateRef* state, Escargot::ObjectRef* self, Escargot::ObjectRef::NativeDataAccessorPropertyData* data, Escargot::ValueRef* setterInputData) -> bool {
            puts("native Setter called");
            return true;
        });
        fn->defineNativeDataAccessorProperty(es, Escargot::ValueRef::create(Escargot::StringRef::fromASCII("native")), nativeData);

        globalObject->set(es, Escargot::ValueRef::create(Escargot::StringRef::fromASCII("Custom")), Escargot::ValueRef::create(fn));

        const char* script = "this.Custom.native = this.Custom.native; this.Custom(); new Custom();";
        const char* filename = "FileName.js";
        printf("evaluateScript %s\n", script);

        Escargot::ScriptRef* scriptRef = ctx->scriptParser()->parse(Escargot::StringRef::fromASCII(script, strlen(script)), Escargot::StringRef::fromASCII(filename, strlen(filename))).m_script;
        Escargot::SandBoxRef* sb = Escargot::SandBoxRef::create(ctx);
        auto sandBoxResult = sb->run([&](Escargot::ExecutionStateRef* state) -> Escargot::ValueRef* {
            return scriptRef->execute(state);
        });

        Escargot::ValueRef* evalResult = sandBoxResult.result;
        sb->destroy();
    }

    es->destroy();
    ctx->destroy();
    vm->destroy();

    Escargot::Globals::finalize();

    printf("testapi ended\n");

    return 0;
}
