#include <cstdlib> // size_t

#include "Escargot.h"
#include "GCUtil.h"
#include "util/Vector.h"
#include "EscargotPublic.h"
#include "parser/ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/ExecutionContext.h"
#include "runtime/FunctionObject.h"
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

bool evaluateScript(ContextRef* context, StringRef* str, StringRef* filename)
{
    Context* ctx = toImpl(context);
    String* script = toImpl(str);
    String* file = toImpl(filename);
    bool shouldPrintScriptResult = true;
    auto result = ctx->scriptParser().parse(script, file);
    if (result.m_error) {
        static char msg[10240];
        auto err = result.m_error->message->toUTF8StringData();
        puts(err.data());
        return false;
    } else {
        Escargot::Script::ScriptSandboxExecuteResult resultValue = result.m_script->sandboxExecute(ctx);
        Escargot::ExecutionState state(ctx);
        if (!resultValue.result.isEmpty()) {
            if (shouldPrintScriptResult)
                puts(resultValue.msgStr->toUTF8StringData().data());
        } else {
            printf("Uncaught %s:\n", resultValue.msgStr->toUTF8StringData().data());
            for (size_t i = 0; i < resultValue.error.stackTrace.size(); i++) {
                printf("%s (%d:%d)\n", resultValue.error.stackTrace[i].fileName->toUTF8StringData().data(), (int)resultValue.error.stackTrace[i].line, (int)resultValue.error.stackTrace[i].column);
            }
            return false;
        }
    }
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


ASCIIStringRef* ASCIIStringRef::create(const char* str)
{
    return nullptr;
}


ASCIIStringRef* ASCIIStringRef::create(const char* str, size_t len)
{
    return nullptr;
}


ASCIIStringRef* ASCIIStringRef::create(const char16_t* str, size_t len)
{
    return nullptr;
}

}
