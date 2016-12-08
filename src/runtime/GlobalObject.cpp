#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"

namespace Escargot {

#ifdef ESCARGOT_SHELL
static Value builtinPrint(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    puts(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}
#endif

static Value builtinGc(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GC_gcollect_and_unmap();
    return Value();
}

void GlobalObject::installOthers(ExecutionState& state)
{
#ifdef ESCARGOT_SHELL
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().print),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
                                                                                              NativeFunctionInfo(state.context()->staticStrings().print, builtinPrint, 1, nullptr, NativeFunctionInfo::Strict), false),
                                                                           (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

#endif
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().gc),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
                                                                                              NativeFunctionInfo(state.context()->staticStrings().gc, builtinGc, 0, nullptr, NativeFunctionInfo::Strict), false),
                                                                           (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
}
}
