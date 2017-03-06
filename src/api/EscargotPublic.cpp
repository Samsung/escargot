#include <cstdlib> // size_t

#include "Escargot.h"
#include "GCUtil.h"
#include "util/Vector.h"
#include "EscargotPublic.h"
#include "runtime/FunctionObject.h"
#include "runtime/ExecutionContext.h"
#include "runtime/VMInstance.h"

namespace Escargot {

void Globals::initialize()
{
    Heap::initialize();
}


void Globals::finalize()
{
    Heap::finalize();
}


VMInstanceRef* VMInstanceRef::create()
{
    VMInstance* instance = new VMInstance();
    return reinterpret_cast<VMInstanceRef*>(instance);
}


ContextRef* ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = reinterpret_cast<VMInstance*>(vminstanceref);
    Context* ctx = new Context(vminstance);
    return reinterpret_cast<ContextRef*>(ctx);
}

ExecutionStateRef* ExecutionStateRef::create(ContextRef* ctxref)
{
    Context* ctx = reinterpret_cast<Context*>(ctxref);
    ExecutionState* es = new ExecutionState(ctx);
    return reinterpret_cast<ExecutionStateRef*>(es);
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
