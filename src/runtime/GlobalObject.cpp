#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ErrorObject.h"
#include "StringObject.h"
#include "NumberObject.h"
#include "parser/ScriptParser.h"
#include "parser/esprima_cpp/esprima.h"
#include "heap/HeapProfiler.h"
#include "EnvironmentRecord.h"
#include "Environment.h"

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
        ExecutionContext* pec = state.executionContext()->parent();
        bool strictFromOutside = false;
        while (pec) {
            if (pec->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                strictFromOutside = pec->inStrictMode();
                break;
            } else if (pec->lexicalEnvironment()->record()->isGlobalEnvironmentRecord()) {
                strictFromOutside = pec->inStrictMode();
                break;
            }
            pec = pec->parent();
        }
        ScriptParser::ScriptParserResult parserResult = parser.parse(StringView(arg.asString(), 0, arg.asString()->length()), String::fromUTF8(s, strlen(s)), parentCodeBlock, strictFromOutside);
        if (parserResult.m_error) {
            ErrorObject* err = ErrorObject::createError(state, parserResult.m_error->errorCode, parserResult.m_error->message);
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

static int parseDigit(char16_t c, int radix)
{
    int digit = -1;

    if (c >= '0' && c <= '9')
        digit = c - '0';
    else if (c >= 'A' && c <= 'Z')
        digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        digit = c - 'a' + 10;

    if (digit >= radix)
        return -1;

    return digit;
}

static Value builtinParseInt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value ret;

    // 1. Let inputString be ToString(string).
    Value input = argv[0];
    String* s = input.toString(state);

    // 2. Let S be a newly created substring of inputString consisting of the first character that is not a StrWhiteSpaceChar
    //    and all characters following that character. (In other words, remove leading white space.)
    unsigned p = 0;
    unsigned strLen = s->length();

    for (; p < strLen; p++) {
        char16_t c = s->charAt(p);
        if (!(esprima::isWhiteSpace(c) || esprima::isLineTerminator(c)))
            break;
    }

    // 3. Let sign be 1.
    // 4. If S is not empty and the first character of S is a minus sign -, let sign be −1.
    // 5. If S is not empty and the first character of S is a plus sign + or a minus sign -, then remove the first character from S.
    double sign = 1;
    if (p < strLen) {
        if (s->charAt(p) == '+')
            p++;
        else if (s->charAt(p) == '-') {
            sign = -1;
            p++;
        }
    }

    // 6. Let R = ToInt32(radix).
    // 7. Let stripPrefix be true.
    // 8. If R ≠ 0, then
    //    b. If R 16, let stripPrefix be false.
    // 9. Else, R = 0
    //    a. Let R = 10.
    // 10. If stripPrefix is true, then
    //     a. If the length of S is at least 2 and the first two characters of S are either “0x” or “0X”, then remove the first two characters from S and let R = 16.
    // 11. If S contains any character that is not a radix-R digit, then let Z be the substring of S consisting of all characters
    //     before the first such character; otherwise, let Z be S.
    int radix = 0;
    if (argc >= 2) {
        radix = argv[1].toInt32(state);
    }
    if ((radix == 0 || radix == 16) && strLen - p >= 2 && s->charAt(p) == '0' && (s->charAt(p + 1) == 'x' || s->charAt(p + 1) == 'X')) {
        radix = 16;
        p += 2;
    }
    if (radix == 0)
        radix = 10;

    // 8.a If R < 2 or R > 36, then return NaN.
    if (radix < 2 || radix > 36)
        return Value(std::numeric_limits<double>::quiet_NaN());

    // 13. Let mathInt be the mathematical integer value that is represented by Z in radix-R notation,
    //     using the letters AZ and az for digits with values 10 through 35. (However, if R is 10 and Z contains more than 20 significant digits,
    //     every significant digit after the 20th may be replaced by a 0 digit, at the option of the implementation;
    //     and if R is not 2, 4, 8, 10, 16, or 32, then mathInt may be an implementation-dependent approximation to the mathematical integer value
    //     that is represented by Z in radix-R notation.)
    // 14. Let number be the Number value for mathInt.
    bool sawDigit = false;
    double number = 0.0;
    while (p < strLen) {
        int digit = parseDigit(s->charAt(p), radix);
        if (digit == -1)
            break;
        sawDigit = true;
        number *= radix;
        number += digit;
        p++;
    }

    // 12. If Z is empty, return NaN.
    if (!sawDigit)
        return Value(std::numeric_limits<double>::quiet_NaN());

    // 15. Return sign × number.
    return Value(sign * number);
}

static Value builtinIsFinite(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double num = argv[0].toNumber(state);
    if (std::isnan(num) || num == std::numeric_limits<double>::infinity() || num == -std::numeric_limits<double>::infinity())
        return Value(Value::False);
    else
        return Value(Value::True);
}

static Value builtinIsNaN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double num = argv[0].toNumber(state);
    return Value(std::isnan(num));
}

void GlobalObject::installOthers(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    defineOwnProperty(state, strings->Infinity, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::infinity()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    defineOwnProperty(state, strings->NaN, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::quiet_NaN()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
    defineOwnProperty(state, strings->undefined, ObjectPropertyDescriptor(Value(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));

    // $18.2.1 eval (x)
    m_eval = new FunctionObject(state,
                                NativeFunctionInfo(strings->eval, builtinEval, 1, nullptr, NativeFunctionInfo::Strict), false);
    defineOwnProperty(state, ObjectPropertyName(strings->eval),
                      ObjectPropertyDescriptor(m_eval, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $18.2.2 isFinite(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isFinite),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->isFinite, builtinIsFinite, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $18.2.3 isNaN(number)
    defineOwnProperty(state, ObjectPropertyName(strings->isNaN),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->isNaN, builtinIsNaN, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(strings->parseInt),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->parseInt, builtinParseInt, 2, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
#ifdef ESCARGOT_SHELL
    defineOwnProperty(state, ObjectPropertyName(strings->print),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->print, builtinPrint, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    defineOwnProperty(state, ObjectPropertyName(strings->load),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->load, builtinLoad, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif
    defineOwnProperty(state, ObjectPropertyName(strings->gc),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(strings->gc, builtinGc, 0, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));

#ifdef PROFILE_BDWGC
    AtomicString dumpBackTrace(state, "dumpBackTrace");
    defineOwnProperty(state, ObjectPropertyName(dumpBackTrace),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(dumpBackTrace, builtinDumpBackTrace, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString registerLeakCheck(state, "registerLeakCheck");
    defineOwnProperty(state, ObjectPropertyName(registerLeakCheck),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(registerLeakCheck, builtinRegisterLeakCheck, 2, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString setPhaseName(state, "setPhaseName");
    defineOwnProperty(state, ObjectPropertyName(setPhaseName),
                      ObjectPropertyDescriptor(new FunctionObject(state,
                                                                  NativeFunctionInfo(setPhaseName, builtinSetGCPhaseName, 1, nullptr, NativeFunctionInfo::Strict), false),
                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif

    m_stringProxyObject = new StringObject(state);
    m_numberProxyObject = new NumberObject(state);
}
}
