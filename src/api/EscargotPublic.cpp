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
}

VMInstanceRef::VMInstanceRef()
{
}

ContextRef::ContextRef(VMInstanceRef vminstance)
{
}

ExecutionStateRef::ExecutionStateRef(ContextRef vminstance)
{
}

ASCIIStringRef::ASCIIStringRef(const char* str)
{
}

ASCIIStringRef::ASCIIStringRef(const char* str, size_t len)
{
}

ASCIIStringRef::ASCIIStringRef(const char16_t* str, size_t len)
{
}

}
