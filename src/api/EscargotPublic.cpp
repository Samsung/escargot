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

#include <cstdlib> // size_t

#include "Escargot.h"
#include "GCUtil.h"
#include "util/Vector.h"
#include "EscargotPublic.h"
#include "parser/ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/ExecutionContext.h"
#include "runtime/FunctionObject.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "EscargotAPICast.h"

namespace Escargot {

StringRef* StringRef::fromASCII(const char* s)
{
    return toRef(String::fromASCII(s));
}

void Globals::initialize()
{
    Heap::initialize();
}

void Globals::finalize()
{
    Heap::finalize();
}

bool evaluateScript(ExecutionStateRef* es, StringRef* str, StringRef* filename)
{
    ASSERT(es);
    ASSERT(str);
    ASSERT(filename);
    ExecutionState* esi = toImpl(es);
    Context* ctx = esi->context();
    ASSERT(ctx);
    String* script = toImpl(str);
    String* file = toImpl(filename);
    bool shouldPrintScriptResult = true;

    auto parseRet = ctx->scriptParser().parse(script, file);
    if (parseRet.m_error) {
        static char msg[10240];
        auto err = parseRet.m_error->message->toUTF8StringData();
        puts(err.data());
        return false;
    }

    auto execRet = parseRet.m_script->sandboxExecute(ctx);
    Escargot::ExecutionState state(ctx);
    if (execRet.result.isEmpty()) {
        printf("Uncaught %s:\n",
               execRet.msgStr->toUTF8StringData().data());
        for (size_t i = 0; i < execRet.error.stackTrace.size(); i++) {
            auto stacktrace = execRet.error.stackTrace[i];
            printf("%s (%d:%d)\n",
                   stacktrace.fileName->toUTF8StringData().data(),
                   (int)stacktrace.line, (int)stacktrace.column);
        }
        return false;
    }
    if (shouldPrintScriptResult)
        puts(execRet.msgStr->toUTF8StringData().data());

    return true;
}


VMInstanceRef* VMInstanceRef::create()
{
    return toRef(new VMInstance());
}


void VMInstanceRef::destroy()
{
    VMInstance* imp = toImpl(this);
    delete imp;
}


ContextRef* ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = toImpl(vminstanceref);
    return toRef(new Context(vminstance));
}


void ContextRef::destroy()
{
    Context* imp = toImpl(this);
    delete imp;
}


ObjectRef* ContextRef::globalObject()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->globalObject());
}


ExecutionStateRef* ExecutionStateRef::create(ContextRef* ctxref)
{
    Context* ctx = toImpl(ctxref);
    return toRef(new ExecutionState(ctx));
}


void ExecutionStateRef::destroy()
{
    ExecutionState* imp = toImpl(this);
    delete imp;
}

ValueRef::ValueRef(intptr_t val) : v(val) { }

ValueRef ValueRef::makeBoolean(ExecutionStateRef* es, bool value)
{
    UNUSED_PARAMETER(es);
    return ValueRef(SmallValue(Value(value)).payload());
}

ValueRef ValueRef::makeNumber(ExecutionStateRef* es, double value)
{
    UNUSED_PARAMETER(es);
    return ValueRef(SmallValue(Value(value)).payload());
}

ValueRef ValueRef::makeNull(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return ValueRef(SmallValue(Value(Value::Null)).payload());
}

ValueRef ValueRef::makeUndefined(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return ValueRef(SmallValue(Value(Value::Undefined)).payload());
}

bool ValueRef::isBoolean(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return Value(SmallValue::fromPayload(v)).isBoolean();
}

bool ValueRef::isNumber(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return Value(SmallValue::fromPayload(v)).isNumber();
}

bool ValueRef::isNull(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return Value(SmallValue::fromPayload(v)).isNull();
}

bool ValueRef::isUndefined(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return Value(SmallValue::fromPayload(v)).isUndefined();
}

bool ValueRef::isObject(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return Value(SmallValue::fromPayload(v)).isObject();
}

bool ValueRef::toBoolean(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(v)).toBoolean(*esi);
}

double ValueRef::toNumber(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(v)).toNumber(*esi);
}

ObjectRef::ObjectRef(intptr_t val) : v(val) { }

ObjectRef ObjectRef::makeObject(ExecutionStateRef* es)
{
    Object* o = new Object(*toImpl(es));
    return ObjectRef(reinterpret_cast<intptr_t>(o));
}

ObjectRef::operator ValueRef()
{
    return ValueRef(v);
}
}
