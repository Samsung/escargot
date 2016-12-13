#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ErrorObject.h"
#include "parser/ScriptParser.h"

namespace Escargot {

#ifdef ESCARGOT_SHELL
static Value builtinPrint(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    puts(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}

static Value builtinLoad(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto f = argv[0].toString(state)->toUTF8StringData();
    const char* fileName = f.data();
    FILE* fp = fopen(fileName, "r");
    String* src = String::emptyString;
    if (fp) {
        std::string str;
        char buf[512];
        while (fgets(buf, sizeof buf, fp) != NULL) {
            str += buf;
        }
        fclose(fp);

        src = new UTF16String(std::move(utf8StringToUTF16String(str.data(), str.length())));
    }

    Context* context = state.context();
    auto result = context->scriptParser().parse(src, argv[0].toString(state));
    if (!result.m_error) {
        result.m_script->execute(context);
    }
    return Value();
}
#endif

static Value builtinGc(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GC_gcollect_and_unmap();
    return Value();
}

static Value builtinEval(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return state.context()->globalObject()->eval(state, argv[0], nullptr);
}

Value GlobalObject::eval(ExecutionState& state, const Value& arg, CodeBlock* parentCodeBlock)
{
    if (arg.isString()) {
        ScriptParser parser(state.context());
        const char* s = "eval input";
        ScriptParser::ScriptParserResult parserResult = parser.parse(StringView(arg.asString(), 0, arg.asString()->length()), String::fromUTF8(s, strlen(s)), parentCodeBlock);
        if (parserResult.m_error) {
            TypeErrorObject* err = new TypeErrorObject(state, parserResult.m_error->message);
            state.throwException(err);
        }
        if (!parentCodeBlock) {
            return parserResult.m_script->execute(state.context());
        } else {
            return parserResult.m_script->executeLocal(state);
        }
    }
    return arg;
}

void GlobalObject::installOthers(ExecutionState& state)
{
    m_eval = new FunctionObject(state,
                                NativeFunctionInfo(state.context()->staticStrings().eval, builtinEval, 1, nullptr, NativeFunctionInfo::Strict), false);
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().eval),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_eval, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
#ifdef ESCARGOT_SHELL
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().print),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
                                                                                              NativeFunctionInfo(state.context()->staticStrings().print, builtinPrint, 1, nullptr, NativeFunctionInfo::Strict), false),
                                                                           (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().load),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
                                                                                              NativeFunctionInfo(state.context()->staticStrings().load, builtinLoad, 1, nullptr, NativeFunctionInfo::Strict), false),
                                                                           (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif
    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().gc),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state,
                                                                                              NativeFunctionInfo(state.context()->staticStrings().gc, builtinGc, 0, nullptr, NativeFunctionInfo::Strict), false),
                                                                           (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
}
}
