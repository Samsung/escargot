#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"

namespace Escargot {

#ifdef ESCARGOT_SHELL
static Value builtinPrint(ExecutionState& state, Value thisValue, Value* argv, bool isNewExpression)
{
    puts(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}
#endif

void GlobalObject::installOthers(ExecutionState& state)
{
#ifdef ESCARGOT_SHELL
    defineOwnProperty(state, PropertyName(state.context()->staticStrings().print),
        Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
            NativeFunctionInfo(state.context()->staticStrings().print, builtinPrint, 1), false),
            (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

#endif
}

}
