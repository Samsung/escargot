/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "api/EscargotPublic.h"

using namespace Escargot;

#include "gtest/gtest.h"

#include <vector>

static bool stringEndsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static const char32_t offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 0x03C82080UL, static_cast<char32_t>(0xFA082080UL), static_cast<char32_t>(0x82082080UL) };

char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen)
{
    unsigned length;
    const char sch = *sequence;
    valid = true;
    if ((sch & 0x80) == 0)
        length = 1;
    else {
        unsigned char ch2 = static_cast<unsigned char>(*(sequence + 1));
        if ((sch & 0xE0) == 0xC0
            && (ch2 & 0xC0) == 0x80)
            length = 2;
        else {
            unsigned char ch3 = static_cast<unsigned char>(*(sequence + 2));
            if ((sch & 0xF0) == 0xE0
                && (ch2 & 0xC0) == 0x80
                && (ch3 & 0xC0) == 0x80)
                length = 3;
            else {
                unsigned char ch4 = static_cast<unsigned char>(*(sequence + 3));
                if ((sch & 0xF8) == 0xF0
                    && (ch2 & 0xC0) == 0x80
                    && (ch3 & 0xC0) == 0x80
                    && (ch4 & 0xC0) == 0x80)
                    length = 4;
                else {
                    valid = false;
                    sequence++;
                    return -1;
                }
            }
        }
    }

    charlen = length;
    char32_t ch = 0;
    switch (length) {
    case 4:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // Fall through.
    case 3:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // Fall through.
    case 2:
        ch += static_cast<unsigned char>(*sequence++);
        ch <<= 6; // Fall through.
    case 1:
        ch += static_cast<unsigned char>(*sequence++);
    }
    return ch - offsetsFromUTF8[length - 1];
}

static std::string evalScript(ContextRef* context, StringRef* str, StringRef* fileName, bool isModule)
{
    if (stringEndsWith(fileName->toStdUTF8String(), "mjs")) {
        isModule = isModule || true;
    }

    auto scriptInitializeResult = context->scriptParser()->initializeScript(str, fileName, isModule);
    if (!scriptInitializeResult.script) {
        std::string result("Script parsing error: ");
        switch (scriptInitializeResult.parseErrorCode) {
        case Escargot::ErrorObjectRef::Code::SyntaxError:
            result += "SyntaxError";
            break;
        case Escargot::ErrorObjectRef::Code::EvalError:
            result += "EvalError";
            break;
        case Escargot::ErrorObjectRef::Code::RangeError:
            result += "RangeError";
            break;
        case Escargot::ErrorObjectRef::Code::ReferenceError:
            result += "ReferenceError";
            break;
        case Escargot::ErrorObjectRef::Code::TypeError:
            result += "TypeError";
            break;
        case Escargot::ErrorObjectRef::Code::URIError:
            result += "URIError";
            break;
        default:
            break;
        }
        char str[256];
        snprintf(str, sizeof(str), ": %s\n", scriptInitializeResult.parseErrorMessage->toStdUTF8String().data());
        result += str;
        return result;
    }

    std::string result;
    auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
        return script->execute(state);
    },
                                         scriptInitializeResult.script.get());

    char str[256];
    if (!evalResult.isSuccessful()) {
        snprintf(str, sizeof(str), "Uncaught %s:\n", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
        result += str;
        for (size_t i = 0; i < evalResult.stackTrace.size(); i++) {
            snprintf(str, sizeof(str), "%s (%d:%d)\n", evalResult.stackTrace[i].srcName->toStdUTF8String().data(), (int)evalResult.stackTrace[i].loc.line, (int)evalResult.stackTrace[i].loc.column);
            result += str;
        }
        return result;
    }

    result += evalResult.resultOrErrorToString(context)->toStdUTF8String();

    while (context->vmInstance()->hasPendingJob()) {
        context->vmInstance()->executePendingJob();
    }
    return result;
}

static ValueRef* eval(ContextRef* context, StringRef* str)
{
    auto scriptInitializeResult = context->scriptParser()->initializeScript(str, StringRef::createFromASCII("eval"), false);
    EXPECT_TRUE(scriptInitializeResult.script.hasValue());
    auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
        return script->execute(state);
    },
                                         scriptInitializeResult.script.get());

    EXPECT_TRUE(evalResult.isSuccessful());
    return evalResult.result;
}

static OptionalRef<StringRef> builtinHelperFileRead(OptionalRef<ExecutionStateRef> state, const char* fileName, const char* builtinName)
{
    FILE* fp = fopen(fileName, "r");
    if (fp) {
        StringRef* src = StringRef::emptyString();
        std::string utf8Str;
        std::basic_string<unsigned char, std::char_traits<unsigned char>> str;
        char buf[512];
        bool hasNonLatin1Content = false;
        size_t readLen;
        while ((readLen = fread(buf, 1, sizeof buf, fp))) {
            if (!hasNonLatin1Content) {
                const char* source = buf;
                int charlen;
                bool valid;
                while (source < buf + readLen) {
                    char32_t ch = readUTF8Sequence(source, valid, charlen);
                    if (ch > 255) {
                        hasNonLatin1Content = true;
                        fseek(fp, 0, SEEK_SET);
                        break;
                    } else {
                        str += (unsigned char)ch;
                    }
                }
            } else {
                utf8Str.append(buf, readLen);
            }
        }
        fclose(fp);
        if (StringRef::isCompressibleStringEnabled()) {
            if (state) {
                if (hasNonLatin1Content) {
                    src = StringRef::createFromUTF8ToCompressibleString(state->context()->vmInstance(), utf8Str.data(), utf8Str.length());
                } else {
                    src = StringRef::createFromLatin1ToCompressibleString(state->context()->vmInstance(), str.data(), str.length());
                }
            } else {
                if (hasNonLatin1Content) {
                    src = StringRef::createFromUTF8(utf8Str.data(), utf8Str.length());
                } else {
                    src = StringRef::createFromLatin1(str.data(), str.length());
                }
            }
        } else {
            if (hasNonLatin1Content) {
                src = StringRef::createFromUTF8(utf8Str.data(), utf8Str.length());
            } else {
                src = StringRef::createFromLatin1(str.data(), str.length());
            }
        }
        return src;
    } else {
        char msg[1024];
        snprintf(msg, sizeof(msg), "GlobalObject.%s: cannot open file %s", builtinName, fileName);
        if (state) {
            state->throwException(URIErrorObjectRef::create(state.get(), StringRef::createFromUTF8(msg, strnlen(msg, sizeof msg))));
        } else {
            puts(msg);
        }
        return nullptr;
    }
}

class ShellPlatform : public PlatformRef {
public:
    virtual void markJSJobEnqueued(ContextRef* relatedContext) override
    {
        // ignore. we always check pending job after eval script
    }

    static std::string dirnameOf(const std::string& fname)
    {
        size_t pos = fname.find_last_of("/");
        if (std::string::npos == pos) {
            pos = fname.find_last_of("\\/");
        }
        return (std::string::npos == pos)
            ? ""
            : fname.substr(0, pos);
    }

    static std::string absolutePath(const std::string& referrerPath, const std::string& src)
    {
        std::string utf8MayRelativePath = dirnameOf(referrerPath) + "/" + src;
        auto absPath = realpath(utf8MayRelativePath.data(), nullptr);
        if (!absPath) {
            return std::string();
        }
        std::string utf8AbsolutePath = absPath ? absPath : "";
        free(absPath);

        return utf8AbsolutePath;
    }

    static std::string absolutePath(const std::string& src)
    {
        auto absPath = realpath(src.data(), nullptr);
        std::string utf8AbsolutePath = absPath;
        free(absPath);

        return utf8AbsolutePath;
    }

    std::vector<std::tuple<std::string /* abs path */, ContextRef*, PersistentRefHolder<ScriptRef>>> loadedModules;
    virtual LoadModuleResult onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc) override
    {
        std::string referrerPath = whereRequestFrom->src()->toStdUTF8String();

        for (size_t i = 0; i < loadedModules.size(); i++) {
            if (std::get<2>(loadedModules[i]) == whereRequestFrom) {
                referrerPath = std::get<0>(loadedModules[i]);
                break;
            }
        }

        std::string absPath = absolutePath(referrerPath, moduleSrc->toStdUTF8String());
        if (absPath.length() == 0) {
            std::string s = "Error reading : " + moduleSrc->toStdUTF8String();
            return LoadModuleResult(ErrorObjectRef::Code::None, StringRef::createFromUTF8(s.data(), s.length()));
        }

        for (size_t i = 0; i < loadedModules.size(); i++) {
            if (std::get<0>(loadedModules[i]) == absPath && std::get<1>(loadedModules[i]) == relatedContext) {
                return LoadModuleResult(std::get<2>(loadedModules[i]));
            }
        }

        OptionalRef<StringRef> source = builtinHelperFileRead(nullptr, absPath.data(), "");
        if (!source) {
            std::string s = "Error reading : " + absPath;
            return LoadModuleResult(ErrorObjectRef::Code::None, StringRef::createFromUTF8(s.data(), s.length()));
        }

        auto parseResult = relatedContext->scriptParser()->initializeScript(source.value(), StringRef::createFromUTF8(absPath.data(), absPath.size()), true);
        if (!parseResult.isSuccessful()) {
            return LoadModuleResult(parseResult.parseErrorCode, parseResult.parseErrorMessage);
        }

        return LoadModuleResult(parseResult.script.get());
    }

    virtual void didLoadModule(ContextRef* relatedContext, OptionalRef<ScriptRef> referrer, ScriptRef* loadedModule) override
    {
        std::string path;
        if (referrer && loadedModule->src()->length() && loadedModule->src()->charAt(0) != '/') {
            path = absolutePath(referrer->src()->toStdUTF8String(), loadedModule->src()->toStdUTF8String());
        } else {
            path = absolutePath(loadedModule->src()->toStdUTF8String());
        }
        loadedModules.push_back(std::make_tuple(path, relatedContext, PersistentRefHolder<ScriptRef>(loadedModule)));
    }

    virtual void hostImportModuleDynamically(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, PromiseObjectRef* promise) override
    {
        LoadModuleResult loadedModuleResult = onLoadModule(relatedContext, referrer, src);
        notifyHostImportModuleDynamicallyResult(relatedContext, referrer, src, promise, loadedModuleResult);
    }
};

PersistentRefHolder<VMInstanceRef> g_instance;
PersistentRefHolder<ContextRef> g_context;

ValueRef* builtinPrint(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 1) {
        StringRef* printMsg;
        if (argv[0]->isSymbol()) {
            printMsg = argv[0]->asSymbol()->symbolDescriptiveString();
            puts(printMsg->toStdUTF8String().data());
            state->context()->printDebugger(printMsg);
        } else {
            printMsg = argv[0]->toString(state);
            puts(printMsg->toStdUTF8String().data());
            state->context()->printDebugger(printMsg->toString(state));
        }
    } else {
        puts("undefined");
    }
    return ValueRef::createUndefined();
}

ValueRef* builtinTestAssert(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 2) {
        EXPECT_TRUE(argv[0]->equalsTo(state, argv[1]));
    }
    return ValueRef::createUndefined();
}

PersistentRefHolder<ContextRef> createEscargotContext(VMInstanceRef* instance)
{
    PersistentRefHolder<ContextRef> context = ContextRef::create(instance);

    Evaluator::execute(context, [](ExecutionStateRef* state) -> ValueRef* {
        ContextRef* context = state->context();

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "print"), builtinPrint, 1, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("print"), buildFunctionObjectRef, true, true, true);
        }

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "testAssert"), builtinTestAssert, 2, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("testAssert"), buildFunctionObjectRef, true, true, true);
        }

        return ValueRef::createUndefined();
    });

    return context;
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    Globals::initialize(new ShellPlatform());

    Memory::setGCFrequency(24);

    g_instance = VMInstanceRef::create();
    g_context = createEscargotContext(g_instance.get());

    RUN_ALL_TESTS();

    g_context.release();
    g_instance.release();
    Globals::finalize();

    return 0;
}

TEST(ValueRef, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto minusValue = ValueRef::create(-1);
        auto vector = ValueVectorRef::create(3);
        vector->set(0, minusValue);

        EXPECT_TRUE(minusValue->isInt32());
        EXPECT_EQ(vector->at(0)->asInt32(), -1);
        EXPECT_EQ(minusValue->toIndex(state), -1);
        EXPECT_EQ(minusValue->tryToUseAsIndex(state), -1);
        EXPECT_EQ(minusValue->toIndex32(state), -1);
        EXPECT_EQ(minusValue->tryToUseAsIndex32(state), -1);
        EXPECT_EQ(minusValue->tryToUseAsIndexProperty(state), -1);

        uint32_t maximum = std::numeric_limits<uint32_t>::max();
        auto plusValue = ValueRef::create(maximum);
        vector->set(1, plusValue);

        // Note) ValueRef(EncodedValue) uses one bit for smi tagging
        // that means ValueRef cannot store maximum uint32_t value as uint32_t type
        EXPECT_FALSE(plusValue->isInt32());
        EXPECT_TRUE(plusValue->isNumber());
        EXPECT_EQ(vector->at(1)->asNumber(), maximum);
        EXPECT_EQ(plusValue->toIndex(state), maximum);
        EXPECT_EQ(plusValue->tryToUseAsIndex(state), maximum);

        uint32_t realMax = (std::numeric_limits<uint32_t>::max() >> 1);
        plusValue = ValueRef::create(realMax);
        vector->set(2, plusValue);

        EXPECT_TRUE(plusValue->isInt32());
        EXPECT_EQ(vector->at(2)->asInt32(), realMax);
        EXPECT_EQ(plusValue->toIndex(state), realMax);
        EXPECT_EQ(plusValue->tryToUseAsIndex(state), realMax);
        EXPECT_EQ(plusValue->toIndex32(state), realMax);
        EXPECT_EQ(plusValue->tryToUseAsIndex32(state), realMax);
        EXPECT_EQ(plusValue->tryToUseAsIndexProperty(state), realMax);

        return ValueRef::createUndefined();
    });
}

TEST(ValueRef, Basic2)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* minusInt = ValueRef::create(-1);
        ValueRef* plusInt = ValueRef::create(1);
        ValueRef* doubleValue = ValueRef::create(2.7);
        ValueRef* obj1 = ObjectRef::create(state);
        ValueRef* obj2 = reinterpret_cast<ValueRef*>(0xfffffff0);

        ValueVectorRef* valueVec = ValueVectorRef::create(3);
        valueVec->set(0, obj1);
        valueVec->set(1, obj2);
        valueVec->set(2, minusInt);

        EXPECT_TRUE(valueVec->at(0)->isObject() && valueVec->at(0)->asObject() == obj1);
        EXPECT_TRUE(valueVec->at(1) == obj2);
        EXPECT_TRUE(valueVec->at(2)->isNumber() && valueVec->at(2)->asNumber() == -1);

        valueVec->erase(1);
        valueVec->pushBack(doubleValue);
        EXPECT_TRUE(valueVec->size() == 3);
        EXPECT_TRUE(valueVec->at(0)->isObject() && valueVec->at(0)->asObject() == obj1);
        EXPECT_TRUE(valueVec->at(1)->isNumber() && valueVec->at(1)->asNumber() == -1);
        EXPECT_TRUE(!valueVec->at(2)->isInt32() && valueVec->at(2)->isNumber() && valueVec->at(2)->asNumber() == 2.7);

        MapObjectRef* map = MapObjectRef::create(state);
        map->set(state, plusInt, doubleValue);
        map->set(state, minusInt, minusInt);

        EXPECT_TRUE(map->get(state, plusInt)->equalsTo(state, doubleValue));
        EXPECT_TRUE(map->get(state, plusInt)->asNumber() == 2.7);
        EXPECT_TRUE(map->get(state, minusInt)->asNumber() == -1);

        return ValueRef::createUndefined();
    });
}

TEST(Evaluator, Basic)
{
    auto result = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        return ValueRef::create(123);
    });
    EXPECT_TRUE(result.isSuccessful());
    EXPECT_TRUE(!result.error);
    EXPECT_TRUE(result.result->isInt32());
    EXPECT_TRUE(result.result->asInt32() == 123);

    auto result2 = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        state->throwException(ValueRef::create(123));
        return ValueRef::create(456);
    });
    EXPECT_TRUE(!result2.isSuccessful());
    EXPECT_TRUE(result2.error);
    EXPECT_TRUE(result2.error->isInt32());
    EXPECT_TRUE(result2.error->asInt32() == 123);

    FunctionObjectRef* functionObjectRef = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
                                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(g_context.get(), "test"),
                                                                                                        [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall) -> ValueRef* {
                                                                                                            state->throwException(ValueRef::create(123));
                                                                                                            return ValueRef::createUndefined();
                                                                                                        },
                                                                                                        0, true, false);
                                               return FunctionObjectRef::create(state, nativeFunctionInfo);
                                           })
                                               .result->asFunctionObject();

    auto result3 = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionObjectRef* functionObjectRef) -> ValueRef* {
        functionObjectRef->call(state, ValueRef::createUndefined(), 0, nullptr);
        EXPECT_TRUE(false);
        return ValueRef::create(456);
    },
                                      functionObjectRef);

    EXPECT_TRUE(!result3.isSuccessful());
    EXPECT_TRUE(result3.error);
    EXPECT_TRUE(result3.error->isInt32());
    EXPECT_TRUE(result3.error->asInt32() == 123);
    EXPECT_TRUE(result3.stackTrace.size() == 1);
    EXPECT_TRUE(result3.stackTrace[0].isFunction);
    EXPECT_TRUE(result3.stackTrace[0].callee.value() == functionObjectRef);

    FunctionObjectRef* functionObjectRef2 = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
                                                return FunctionObjectRef::create(state, StringRef::createFromASCII("test_name"), AtomicStringRef::create(state->context(), "test"), 0, nullptr, StringRef::createFromASCII("throw 3;"));
                                            })
                                                .result->asFunctionObject();

    auto result4 = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionObjectRef* functionObjectRef) -> ValueRef* {
        functionObjectRef->call(state, ValueRef::createUndefined(), 0, nullptr);
        EXPECT_TRUE(false);
        return ValueRef::create(456);
    },
                                      functionObjectRef2);

    EXPECT_TRUE(!result4.isSuccessful());
    EXPECT_TRUE(result4.error);
    EXPECT_TRUE(result4.error->isInt32());
    EXPECT_TRUE(result4.error->asInt32() == 3);
    EXPECT_TRUE(result4.stackTrace.size() == 1);
    EXPECT_TRUE(result4.stackTrace[0].isFunction);
    EXPECT_TRUE(result4.stackTrace[0].callee.value() == functionObjectRef2);
    EXPECT_TRUE(result4.stackTrace[0].srcName->equalsWithASCIIString("test_name", 9));
}

TEST(EvalScript, Run)
{
    auto s = evalScript(g_context.get(), StringRef::createFromASCII("1 + 1"), StringRef::createFromASCII("test.js"), false);
    EXPECT_EQ(s, "2");
}

TEST(EvalScript, Run2)
{
    auto s = evalScript(g_context.get(), StringRef::createFromASCII("'1' - 1"), StringRef::createFromASCII("test.js"), false);
    EXPECT_EQ(s, "0");
}

TEST(EvalScript, Run3)
{
    // local eval call
    auto s1 = evalScript(g_context.get(), StringRef::createFromASCII(R"(
    var code = 'function tt() { throw new Error(); }; tt();';
    eval(code);
    )"),
                         StringRef::createFromASCII("evalTest.js"), false);
    EXPECT_EQ(s1, "Uncaught Error:\neval code (1:22)\neval code (1:39)\nevalTest.js (3:5)\n");

    // global eval call
    auto s2 = evalScript(g_context.get(), StringRef::createFromASCII(R"(
    var code = 'function tt() { throw new Error(); }; tt();';
    var indirectEval = eval;
    indirectEval(code);
    )"),
                         StringRef::createFromASCII("evalTest2.js"), false);
    EXPECT_EQ(s2, "Uncaught Error:\neval code (1:22)\neval code (1:39)\n");
}

TEST(EvalScript, ParseError)
{
    auto s = evalScript(g_context.get(), StringRef::createFromASCII("."), StringRef::createFromASCII("test.js"), false);
    EXPECT_TRUE(s.find("SyntaxError") != std::string::npos);
}

TEST(EvalScript, RuntimeError)
{
    auto s = evalScript(g_context.get(), StringRef::createFromASCII("throw 1"), StringRef::createFromASCII("test.js"), false);
    EXPECT_TRUE(s.find("Uncaught 1") == 0);
}

TEST(Object, ConstructorName)
{
    ObjectRef* testObj = eval(g_context.get(), StringRef::createFromASCII("function foo(){}; var ctorNameTest = new foo(); ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foo");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("ctorNameTest = {}; ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "Object");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("var ctorNameTest = new foo(); ctorNameTest.__proto__=null; ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foo");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("var ctorNameTest = {}; ctorNameTest.__proto__=foo.prototype; ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foo");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("class foobar{};  ctorNameTest = new foobar(); ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foobar");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("ctorNameTest = new foobar(); ctorNameTest.__proto__=foo.prototype;  ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foobar");
        return ValueRef::createUndefined();
    },
                       testObj);

    testObj = eval(g_context.get(), StringRef::createFromASCII("ctorNameTest = new foobar(); ctorNameTest.__proto__=null;  ctorNameTest;"))->asObject();
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* testObj) -> ValueRef* {
        StringRef* c = testObj->constructorName(state);
        EXPECT_TRUE(c->toStdUTF8String() == "foobar");
        return ValueRef::createUndefined();
    },
                       testObj);
}

TEST(ObjectTemplate, Basic1)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    EXPECT_TRUE(tpl->isObjectTemplate());
    EXPECT_FALSE(tpl->isFunctionTemplate());

    int a = 100;
    int* testPtr = &a;

    tpl->setInstanceExtraData(testPtr);

    tpl->set(StringRef::createFromASCII("asdf"), StringRef::createFromASCII("asdfData"), false, false, false);

    ObjectTemplateRef* another = ObjectTemplateRef::create();
    tpl->set(StringRef::createFromASCII("another"), another, false, false, false);


    ObjectRef* obj = tpl->instantiate(g_context.get());

    EXPECT_TRUE(obj->extraData() == testPtr);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        auto desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("asdf"));
        auto value = desc->asObject()->get(state, StringRef::createFromASCII("value"));
        EXPECT_TRUE(value->asString()->equalsWithASCIIString("asdfData", 8));

        value = desc->asObject()->get(state, StringRef::createFromASCII("writable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("enumerable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("configurable"));
        EXPECT_TRUE(value->isFalse());

        desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("another"));
        EXPECT_TRUE(desc->asObject()->get(state, StringRef::createFromASCII("value"))->isObject());

        value = desc->asObject()->get(state, StringRef::createFromASCII("writable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("enumerable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("configurable"));
        EXPECT_TRUE(value->isFalse());

        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(ObjectTemplate, Basic2)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    auto getter = FunctionTemplateRef::create(AtomicStringRef::emptyAtomicString(), 1, true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        return ValueRef::create(12);
    });

    tpl->setAccessorProperty(StringRef::createFromASCII("asdf"), getter, nullptr, false, true);

    auto getter2 = FunctionTemplateRef::create(AtomicStringRef::emptyAtomicString(), 1, true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        return (ValueRef*)thisValue->asObject()->extraData();
    });
    auto setter = FunctionTemplateRef::create(AtomicStringRef::emptyAtomicString(), 1, true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        thisValue->asObject()->setExtraData(argv[0]);
        return ValueRef::createUndefined();
    });
    tpl->setAccessorProperty(StringRef::createFromASCII("asdf2"), getter2, setter, false, true);

    ObjectRef* obj = tpl->instantiate(g_context.get());

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        auto desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("asdf"));

        auto value = desc->asObject()->get(state, StringRef::createFromASCII("enumerable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("configurable"));
        EXPECT_TRUE(value->isTrue());

        value = desc->asObject()->get(state, StringRef::createFromASCII("get"));
        EXPECT_TRUE(value->isFunctionObject());

        value = desc->asObject()->get(state, StringRef::createFromASCII("set"));
        EXPECT_TRUE(value->isUndefined());

        EXPECT_TRUE(obj->get(state, StringRef::createFromASCII("asdf"))->equalsTo(state, ValueRef::create(12)));

        obj->set(state, StringRef::createFromASCII("asdf2"), StringRef::createFromASCII("test"));
        EXPECT_TRUE(obj->get(state, StringRef::createFromASCII("asdf2"))->equalsTo(state, StringRef::createFromASCII("test")));

        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(ObjectTemplate, Basic3)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    class TestNativeDataAccessorPropertyData : public ObjectRef::NativeDataAccessorPropertyData {
    public:
        TestNativeDataAccessorPropertyData()
            : ObjectRef::NativeDataAccessorPropertyData(false, false, false, nullptr, nullptr)
        {
            number = 10;
        }
        double number;
    };

    TestNativeDataAccessorPropertyData* data = new TestNativeDataAccessorPropertyData();
    data->m_isConfigurable = false;
    data->m_isEnumerable = false;
    data->m_isWritable = true;
    data->m_getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, ObjectRef::NativeDataAccessorPropertyData* data) -> ValueRef* {
        return ValueRef::create(((TestNativeDataAccessorPropertyData*)data)->number);
    };

    data->m_setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, ObjectRef::NativeDataAccessorPropertyData* data, ValueRef* setterInputData) -> bool {
        ((TestNativeDataAccessorPropertyData*)data)->number = setterInputData->toNumber(state) * 2;
        return true;
    };

    tpl->setNativeDataAccessorProperty(StringRef::createFromASCII("asdf"), data, true);

    class TestNativeDataAccessorPropertyData2 : public ObjectRef::NativeDataAccessorPropertyData {
    public:
        TestNativeDataAccessorPropertyData2()
            : ObjectRef::NativeDataAccessorPropertyData(false, false, false, nullptr, nullptr)
        {
            child = nullptr;
            parent = nullptr;
            value = ValueRef::createUndefined();

            getterCallCount = 0;
            setterCallCount = 0;
        }

        ObjectRef* child;
        ObjectRef* parent;
        ValueRef* value;

        int getterCallCount;
        int setterCallCount;
    };

    TestNativeDataAccessorPropertyData2* data2 = new TestNativeDataAccessorPropertyData2();
    data2->m_isConfigurable = false;
    data2->m_isEnumerable = false;
    data2->m_isWritable = true;
    data2->m_getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, ObjectRef::NativeDataAccessorPropertyData* data) -> ValueRef* {
        if (((TestNativeDataAccessorPropertyData2*)data)->getterCallCount == 0) {
            EXPECT_TRUE(((TestNativeDataAccessorPropertyData2*)data)->parent == self);
            EXPECT_TRUE(((TestNativeDataAccessorPropertyData2*)data)->child == receiver);
        }
        ((TestNativeDataAccessorPropertyData2*)data)->getterCallCount++;
        return ((TestNativeDataAccessorPropertyData2*)data)->value;
    };

    data2->m_setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, ObjectRef::NativeDataAccessorPropertyData* data, ValueRef* setterInputData) -> bool {
        ((TestNativeDataAccessorPropertyData2*)data)->setterCallCount++;
        ((TestNativeDataAccessorPropertyData2*)data)->value = ValueRef::create(setterInputData->asNumber() * 2);
        EXPECT_TRUE(((TestNativeDataAccessorPropertyData2*)data)->parent == self);
        EXPECT_TRUE(((TestNativeDataAccessorPropertyData2*)data)->child == receiver);
        return true;
    };

    tpl->setNativeDataAccessorProperty(StringRef::createFromASCII("asdf2"), data2, true);

    ObjectRef* obj = tpl->instantiate(g_context.get());

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj, TestNativeDataAccessorPropertyData2* data2, ObjectTemplateRef* tpl) -> ValueRef* {
        auto desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("asdf"));

        auto value = desc->asObject()->get(state, StringRef::createFromASCII("enumerable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("configurable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("writable"));
        EXPECT_TRUE(value->isTrue());

        obj->set(state, StringRef::createFromASCII("asdf"), ValueRef::create(20));

        EXPECT_TRUE(obj->get(state, StringRef::createFromASCII("asdf"))->equalsTo(state, ValueRef::create(40)));

        auto obj2 = ObjectRef::create(state);
        tpl->installTo(state->context(), obj2);

        desc = obj2->getOwnPropertyDescriptor(state, StringRef::createFromASCII("asdf"));

        value = desc->asObject()->get(state, StringRef::createFromASCII("enumerable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("configurable"));
        EXPECT_TRUE(value->isFalse());

        value = desc->asObject()->get(state, StringRef::createFromASCII("writable"));
        EXPECT_TRUE(value->isTrue());

        obj2->set(state, StringRef::createFromASCII("asdf"), ValueRef::create(20));

        EXPECT_TRUE(obj2->get(state, StringRef::createFromASCII("asdf"))->equalsTo(state, ValueRef::create(40)));

        ObjectRef* child = ObjectRef::create(state);
        child->setPrototype(state, obj);

        data2->child = child;
        data2->parent = obj;

        child->set(state, StringRef::createFromASCII("asdf2"), ValueRef::create(64));
        EXPECT_TRUE(data2->setterCallCount == 1);
        EXPECT_TRUE(data2->value->equalsTo(state, ValueRef::create(128)));

        child->get(state, StringRef::createFromASCII("asdf2"));
        EXPECT_TRUE(data2->getterCallCount == 1);

        EXPECT_TRUE(obj->hasOwnProperty(state, StringRef::createFromASCII("asdf2")));
        EXPECT_FALSE(child->hasOwnProperty(state, StringRef::createFromASCII("asdf2")));

        auto desc2 = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("asdf2"));
        desc2 = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc2);
        EXPECT_TRUE(desc2->toString(state)->toStdUTF8String() == "{\"value\":128,\"writable\":true,\"enumerable\":false,\"configurable\":false}");

        return ValueRef::createUndefined();
    },
                       obj, data2, tpl);

    // inline cache test if actsLikeJSGetterSetter is true
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        state->context()->globalObject()->set(state, StringRef::createFromASCII("asdf"), obj);
        return ValueRef::createUndefined();
    },
                       obj);

    evalScript(g_context.get(), StringRef::createFromASCII("var t = {}; t.__proto__ = asdf;"
                                                           "for(let i = 0;i < 10; i++) { t.asdf = 3 }"),
               StringRef::createFromASCII("test"), false);


    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        ValueRef* ret = state->context()->globalObject()->get(state, StringRef::createFromASCII("t"))->asObject()->get(state, StringRef::createFromASCII("asdf"));
        EXPECT_TRUE(ret->toNumber(state) == 6);
        state->context()->globalObject()->set(state, StringRef::createFromASCII("t"), ValueRef::createUndefined());
        state->context()->globalObject()->set(state, StringRef::createFromASCII("asdf"), ValueRef::createUndefined());
        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(ObjectTemplate, Basic4)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    ObjectTemplatePropertyHandlerConfiguration handler;
    handler.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            return (ValueRef*)self->extraData();
        }
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            return ValueRef::create(555.555);
        }
        return OptionalRef<ValueRef>();
    };

    handler.setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, ValueRef* value) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            self->setExtraData(value);
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            self->setExtraData(ValueRef::create(123));
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        return OptionalRef<ValueRef>();
    };

    handler.query = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> ObjectTemplatePropertyAttribute {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable | ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable | ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
        }
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable);
        }
        return ObjectTemplatePropertyAttribute::PropertyAttributeNotExist;
    };

    handler.enumerator = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data) -> ValueVectorRef* {
        ValueVectorRef* v = ValueVectorRef::create(2);
        v->set(0, StringRef::createFromASCII("aaa"));
        v->set(1, StringRef::createFromASCII("ccc"));
        return v;
    };

    handler.deleter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ddd", 3)) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }

        return OptionalRef<ValueRef>();
    };

    handler.descriptor = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("eee", 3)) {
            ObjectRef* desc = ObjectRef::create(state);
            desc->set(state, StringRef::createFromASCII("value"), ValueRef::createNull());
            return desc;
        }

        return OptionalRef<ValueRef>();
    };

    tpl->setNamedPropertyHandler(handler);

    ObjectRef* obj = tpl->instantiate(g_context.get());
    obj->setExtraData(ValueRef::create(123));

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_TRUE(obj->set(state, StringRef::createFromASCII("aaa"), ValueRef::createNull()));
        ValueRef* aaa = obj->get(state, StringRef::createFromASCII("aaa"));
        EXPECT_TRUE(aaa->isNull());

        ValueRef* bbb = obj->get(state, StringRef::createFromASCII("bbb"));
        EXPECT_TRUE(bbb->isUndefined());

        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("ccc"), ValueRef::create(333), true, true, true));
        ValueRef* cccResult = obj->get(state, StringRef::createFromASCII("aaa"));
        EXPECT_TRUE(cccResult->equalsTo(state, ValueRef::create(123)));

        ValueRef* desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("aaa"));

        ValueRef* json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":123,\"writable\":true,\"enumerable\":true,\"configurable\":true}");

        desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("ccc"));
        json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":555.555,\"writable\":true,\"enumerable\":false,\"configurable\":false}");

        int pcnt = 0;
        obj->enumerateObjectOwnProperties(state, [&](ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable) -> bool {
            if (pcnt == 0) {
                EXPECT_TRUE(propertyName->toString(state)->toStdUTF8String() == "aaa");
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(isEnumerable);
                EXPECT_TRUE(isConfigurable);
            } else if (pcnt == 1) {
                EXPECT_TRUE(propertyName->toString(state)->toStdUTF8String() == "ccc");
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(!isEnumerable);
                EXPECT_TRUE(!isConfigurable);
            }
            pcnt++;
            return true;
        });

        EXPECT_TRUE(pcnt == 2);

        EXPECT_TRUE(obj->deleteOwnProperty(state, StringRef::createFromASCII("fff")));
        EXPECT_FALSE(obj->deleteOwnProperty(state, StringRef::createFromASCII("ddd")));

        auto descObj = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("eee"));
        EXPECT_TRUE(descObj->isObject());
        EXPECT_TRUE(descObj->asObject()->get(state, StringRef::createFromASCII("value"))->isNull());

        return ValueRef::createUndefined();
    },
                       obj);


    ObjectTemplateRef* tpl2 = ObjectTemplateRef::create();
    ObjectTemplatePropertyHandlerConfiguration handler2;
    handler2.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        return OptionalRef<ValueRef>();
    };
    handler2.definer = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, const ObjectPropertyDescriptorRef& desc) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ttt", 3)) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }
        return OptionalRef<ValueRef>();
    };

    tpl2->setNamedPropertyHandler(handler2);

    obj = tpl2->instantiate(g_context.get());

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_FALSE(obj->defineDataProperty(state, StringRef::createFromASCII("ttt"), ValueRef::create(111), false, false, false));
        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(ObjectTemplate, Basic5)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    ObjectTemplatePropertyHandlerConfiguration handler;
    handler.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            return (ValueRef*)self->extraData();
        }
        if (indexProperty == 3) {
            return ValueRef::create(555.555);
        }
        return OptionalRef<ValueRef>();
    };

    handler.setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, ValueRef* value) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            self->setExtraData(value);
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }
        if (indexProperty == 3) {
            self->setExtraData(ValueRef::create(123));
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }
        return OptionalRef<ValueRef>();
    };

    handler.query = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> ObjectTemplatePropertyAttribute {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable | ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable | ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
        }
        if (indexProperty == 3) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable);
        }
        return ObjectTemplatePropertyAttribute::PropertyAttributeNotExist;
    };

    handler.enumerator = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data) -> ValueVectorRef* {
        ValueVectorRef* v = ValueVectorRef::create(2);
        v->set(0, StringRef::createFromASCII("1"));
        v->set(1, ValueRef::create(3));
        return v;
    };

    handler.deleter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 4) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }

        return OptionalRef<ValueRef>();
    };

    handler.descriptor = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 5) {
            ObjectRef* desc = ObjectRef::create(state);
            desc->set(state, StringRef::createFromASCII("value"), ValueRef::createNull());
            return desc;
        }

        return OptionalRef<ValueRef>();
    };

    tpl->setIndexedPropertyHandler(handler);

    ObjectRef* obj = tpl->instantiate(g_context.get());
    obj->setExtraData(ValueRef::create(123));

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_TRUE(obj->set(state, StringRef::createFromASCII("1"), ValueRef::createNull()));
        ValueRef* one = obj->get(state, ValueRef::create(1));
        EXPECT_TRUE(one->isNull());

        ValueRef* two = obj->get(state, ValueRef::create(2));
        EXPECT_TRUE(two->isUndefined());

        EXPECT_TRUE(obj->defineDataProperty(state, ValueRef::create(3), ValueRef::create(333), true, true, true));
        ValueRef* threeResult = obj->get(state, StringRef::createFromASCII("1"));
        EXPECT_TRUE(threeResult->equalsTo(state, ValueRef::create(123)));

        ValueRef* desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("1"));

        ValueRef* json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":123,\"writable\":true,\"enumerable\":true,\"configurable\":true}");

        desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("3"));
        json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":555.555,\"writable\":true,\"enumerable\":false,\"configurable\":false}");

        int pcnt = 0;
        obj->enumerateObjectOwnProperties(state, [&](ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable) -> bool {
            if (pcnt == 0) {
                EXPECT_TRUE(propertyName->tryToUseAsIndexProperty(state) == 1);
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(isEnumerable);
                EXPECT_TRUE(isConfigurable);
            } else if (pcnt == 1) {
                EXPECT_TRUE(propertyName->tryToUseAsIndexProperty(state) == 3);
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(!isEnumerable);
                EXPECT_TRUE(!isConfigurable);
            }
            pcnt++;
            return true;
        });

        EXPECT_TRUE(pcnt == 2);

        EXPECT_TRUE(obj->deleteOwnProperty(state, StringRef::createFromASCII("6")));
        EXPECT_FALSE(obj->deleteOwnProperty(state, ValueRef::create(4)));

        auto descObj = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("5"));
        EXPECT_TRUE(descObj->isObject());
        EXPECT_TRUE(descObj->asObject()->get(state, StringRef::createFromASCII("value"))->isNull());

        return ValueRef::createUndefined();
    },
                       obj);


    ObjectTemplateRef* tpl2 = ObjectTemplateRef::create();
    ObjectTemplatePropertyHandlerConfiguration handler2;
    handler2.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        return OptionalRef<ValueRef>();
    };
    handler2.definer = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, const ObjectPropertyDescriptorRef& desc) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 100) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }
        return OptionalRef<ValueRef>();
    };

    tpl2->setIndexedPropertyHandler(handler2);

    obj = tpl2->instantiate(g_context.get());

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_FALSE(obj->defineDataProperty(state, StringRef::createFromASCII("100"), ValueRef::create(111), false, false, false));
        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(ObjectTemplate, Basic6)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    ObjectTemplatePropertyHandlerConfiguration handler1;
    handler1.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            return (ValueRef*)self->extraData();
        }
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            return ValueRef::create(555.555);
        }
        return OptionalRef<ValueRef>();
    };

    handler1.setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, ValueRef* value) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            self->setExtraData(value);
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            self->setExtraData(ValueRef::create(123));
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        return OptionalRef<ValueRef>();
    };

    handler1.query = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> ObjectTemplatePropertyAttribute {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("aaa", 3)) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable | ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable | ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
        }
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ccc", 3)) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable);
        }
        return ObjectTemplatePropertyAttribute::PropertyAttributeNotExist;
    };

    handler1.enumerator = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data) -> ValueVectorRef* {
        ValueVectorRef* v = ValueVectorRef::create(2);
        v->set(0, StringRef::createFromASCII("aaa"));
        v->set(1, StringRef::createFromASCII("ccc"));
        return v;
    };

    handler1.deleter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("ddd", 3)) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }

        return OptionalRef<ValueRef>();
    };

    handler1.descriptor = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        if (propertyName->isString() && propertyName->asString()->equalsWithASCIIString("eee", 3)) {
            ObjectRef* desc = ObjectRef::create(state);
            desc->set(state, StringRef::createFromASCII("value"), ValueRef::createNull());
            return desc;
        }

        return OptionalRef<ValueRef>();
    };

    ObjectTemplatePropertyHandlerConfiguration handler2;
    handler2.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            return (ValueRef*)self->extraData();
        }
        if (indexProperty == 3) {
            return ValueRef::create(555.555);
        }
        return OptionalRef<ValueRef>();
    };

    handler2.setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, ValueRef* value) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            self->setExtraData(value);
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }
        if (indexProperty == 3) {
            self->setExtraData(ValueRef::create(123));
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }
        return OptionalRef<ValueRef>();
    };

    handler2.query = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> ObjectTemplatePropertyAttribute {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 1) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable | ObjectTemplatePropertyAttribute::PropertyAttributeEnumerable | ObjectTemplatePropertyAttribute::PropertyAttributeConfigurable);
        }
        if (indexProperty == 3) {
            return (ObjectTemplatePropertyAttribute)(ObjectTemplatePropertyAttribute::PropertyAttributeWritable);
        }
        return ObjectTemplatePropertyAttribute::PropertyAttributeNotExist;
    };

    handler2.enumerator = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data) -> ValueVectorRef* {
        ValueVectorRef* v = ValueVectorRef::create(2);
        v->set(0, StringRef::createFromASCII("1"));
        v->set(1, ValueRef::create(3));
        return v;
    };

    handler2.deleter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 4) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }

        return OptionalRef<ValueRef>();
    };

    handler2.descriptor = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName) -> OptionalRef<ValueRef> {
        uint32_t indexProperty = propertyName->tryToUseAsIndexProperty(state);
        EXPECT_TRUE(indexProperty != ValueRef::InvalidIndexPropertyValue);
        if (indexProperty == 5) {
            ObjectRef* desc = ObjectRef::create(state);
            desc->set(state, StringRef::createFromASCII("value"), ValueRef::createNull());
            return desc;
        }

        return OptionalRef<ValueRef>();
    };

    tpl->setNamedPropertyHandler(handler1);
    tpl->setIndexedPropertyHandler(handler2);

    ObjectRef* obj = tpl->instantiate(g_context.get());
    obj->setExtraData(ValueRef::create(123));

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_TRUE(obj->set(state, StringRef::createFromASCII("aaa"), ValueRef::createNull()));
        ValueRef* aaa = obj->get(state, StringRef::createFromASCII("aaa"));
        EXPECT_TRUE(aaa->isNull());

        ValueRef* bbb = obj->get(state, StringRef::createFromASCII("bbb"));
        EXPECT_TRUE(bbb->isUndefined());

        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("ccc"), ValueRef::create(333), true, true, true));
        ValueRef* cccResult = obj->get(state, StringRef::createFromASCII("aaa"));
        EXPECT_TRUE(cccResult->equalsTo(state, ValueRef::create(123)));

        ValueRef* desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("aaa"));

        ValueRef* json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":123,\"writable\":true,\"enumerable\":true,\"configurable\":true}");

        desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("ccc"));
        json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":555.555,\"writable\":true,\"enumerable\":false,\"configurable\":false}");

        int pcnt = 0;
        // FIXME enumration operation
        obj->enumerateObjectOwnProperties(state, [&](ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable) -> bool {
            if (pcnt == 0) {
                EXPECT_TRUE(propertyName->toString(state)->toStdUTF8String() == "aaa");
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(isEnumerable);
                EXPECT_TRUE(isConfigurable);
            } else if (pcnt == 1) {
                EXPECT_TRUE(propertyName->toString(state)->toStdUTF8String() == "ccc");
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(!isEnumerable);
                EXPECT_TRUE(!isConfigurable);
            }
            pcnt++;
            return true;
        });

        EXPECT_TRUE(pcnt == 4);

        EXPECT_TRUE(obj->deleteOwnProperty(state, StringRef::createFromASCII("fff")));
        EXPECT_FALSE(obj->deleteOwnProperty(state, StringRef::createFromASCII("ddd")));

        auto descObj = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("eee"));
        EXPECT_TRUE(descObj->isObject());
        EXPECT_TRUE(descObj->asObject()->get(state, StringRef::createFromASCII("value"))->isNull());

        return ValueRef::createUndefined();
    },
                       obj);

    obj->setExtraData(ValueRef::create(123));
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        EXPECT_TRUE(obj->set(state, StringRef::createFromASCII("1"), ValueRef::createNull()));
        ValueRef* one = obj->get(state, ValueRef::create(1));
        EXPECT_TRUE(one->isNull());

        ValueRef* two = obj->get(state, ValueRef::create(2));
        EXPECT_TRUE(two->isUndefined());

        EXPECT_TRUE(obj->defineDataProperty(state, ValueRef::create(3), ValueRef::create(333), true, true, true));
        ValueRef* threeResult = obj->get(state, StringRef::createFromASCII("1"));
        EXPECT_TRUE(threeResult->equalsTo(state, ValueRef::create(123)));

        ValueRef* desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("1"));

        ValueRef* json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":123,\"writable\":true,\"enumerable\":true,\"configurable\":true}");

        desc = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("3"));
        json = state->context()->globalObject()->jsonStringify()->call(state, ValueRef::createUndefined(), 1, &desc);
        EXPECT_TRUE(json->toString(state)->toStdUTF8String() == "{\"value\":555.555,\"writable\":true,\"enumerable\":false,\"configurable\":false}");

        int pcnt = 0;
        // FIXME enumration operation
        obj->enumerateObjectOwnProperties(state, [&](ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable) -> bool {
            if (pcnt == 2) {
                EXPECT_TRUE(propertyName->tryToUseAsIndexProperty(state) == 1);
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(isEnumerable);
                EXPECT_TRUE(isConfigurable);
            } else if (pcnt == 3) {
                EXPECT_TRUE(propertyName->tryToUseAsIndexProperty(state) == 3);
                EXPECT_TRUE(isWritable);
                EXPECT_TRUE(!isEnumerable);
                EXPECT_TRUE(!isConfigurable);
            }
            pcnt++;
            return true;
        });

        EXPECT_TRUE(pcnt == 4);

        EXPECT_TRUE(obj->deleteOwnProperty(state, StringRef::createFromASCII("6")));
        EXPECT_FALSE(obj->deleteOwnProperty(state, ValueRef::create(4)));

        auto descObj = obj->getOwnPropertyDescriptor(state, StringRef::createFromASCII("5"));
        EXPECT_TRUE(descObj->isObject());
        EXPECT_TRUE(descObj->asObject()->get(state, StringRef::createFromASCII("value"))->isNull());

        return ValueRef::createUndefined();
    },
                       obj);
}

TEST(FunctionTemplate, Basic1)
{
    auto ft = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "asdf"), 2, true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        if (newTarget) {
            EXPECT_TRUE(thisValue->isObject());
            return ValueRef::createNull();
        } else {
            EXPECT_TRUE(argc == 1);
            EXPECT_TRUE(argv[0]->toString(state)->toStdUTF8String() == "123");
            return argv[0];
        }
    });

    ft->setName(AtomicStringRef::create(g_context.get(), "asdf2"));
    ft->setLength(5);

    int a = 100;
    int* testPtr = &a;
    ft->setInstanceExtraData(testPtr);

    EXPECT_FALSE(ft->isObjectTemplate());
    EXPECT_TRUE(ft->isFunctionTemplate());

    FunctionObjectRef* fn = ft->instantiate(g_context.get())->asFunctionObject();

    EXPECT_TRUE(fn->extraData() == testPtr);

    // same instance on same context
    EXPECT_EQ(fn, ft->instantiate(g_context.get())->asFunctionObject());

    auto r = Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionObjectRef* fn) -> ValueRef* {
        ValueRef* arr[1] = { ValueRef::create(123) };
        EXPECT_TRUE(fn->call(state, ValueRef::createUndefined(), 1, arr)->equalsTo(state, ValueRef::create(123)));

        ValueRef* ret = fn->construct(state, 0, nullptr);
        EXPECT_TRUE(ret->isNull());

        ValueRef* name = fn->getOwnProperty(state, StringRef::createFromASCII("name", 4));
        EXPECT_TRUE(name->asString()->equalsWithASCIIString("asdf2", 5));

        ValueRef* length = fn->getOwnProperty(state, StringRef::createFromASCII("length", 6));
        EXPECT_TRUE(length->asNumber() == 5);

        // re-set function name property
        bool setNameResult = fn->setName(AtomicStringRef::create(g_context.get(), "asdf3"));
        EXPECT_TRUE(setNameResult);
        name = fn->getOwnProperty(state, StringRef::createFromASCII("name", 4));
        EXPECT_TRUE(name->asString()->equalsWithASCIIString("asdf3", 5));

        return ValueRef::createUndefined();
    },
                                fn);
    EXPECT_TRUE(r.isSuccessful());
}

TEST(FunctionTemplate, Basic2)
{
    auto ft = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "asdf"), 2, true, false, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        EXPECT_TRUE(argc == 1);
        return argv[0];
    });

    int a = 100;
    int* testPtr = &a;
    ft->setInstanceExtraData(testPtr);

    EXPECT_FALSE(ft->isObjectTemplate());
    EXPECT_TRUE(ft->isFunctionTemplate());

    FunctionObjectRef* fn = ft->instantiate(g_context.get())->asFunctionObject();

    EXPECT_TRUE(fn->extraData() == testPtr);

    // same instance on same context
    EXPECT_EQ(fn, ft->instantiate(g_context.get())->asFunctionObject());

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionObjectRef* fn) -> ValueRef* {
        ValueRef* arr[1] = { ValueRef::create(123) };
        EXPECT_TRUE(fn->call(state, ValueRef::createUndefined(), 1, arr)->equalsTo(state, ValueRef::create(123)));
        return ValueRef::createUndefined();
    },
                       fn);
}

TEST(FunctionTemplate, Basic3)
{
    auto super = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "super"), 0,
                                             true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
                                                 return ValueRef::createUndefined();
                                             });

    ObjectRef::NativeDataAccessorPropertyData* nativeAccessorData = new ObjectRef::NativeDataAccessorPropertyData(true, false, false,
                                                                                                                  [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, ObjectRef::NativeDataAccessorPropertyData* data) -> ValueRef* {
                                                                                                                      return ValueRef::create(12);
                                                                                                                  },
                                                                                                                  nullptr);

    super->prototypeTemplate()->set(StringRef::createFromASCII("b_p1"), ValueRef::create(3.14), true, true, true);
    super->instanceTemplate()->set(StringRef::createFromASCII("b_i1"), ValueRef::create(3), true, true, true);
    super->instanceTemplate()->setNativeDataAccessorProperty(StringRef::createFromASCII("b_ia"), nativeAccessorData, true);

    auto base = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "base"), 2,
                                            true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
                                                return ValueRef::create(123);
                                            });
    // inherit super
    base->inherit(super);

    base->prototypeTemplate()->set(StringRef::createFromASCII("v1"), ValueRef::create(20.1), true, true, true);
    base->instanceTemplate()->set(StringRef::createFromASCII("v2"), ValueRef::create(0), true, true, true);

    // test __proto__ chain
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionTemplateRef* super, FunctionTemplateRef* base) -> ValueRef* {
        ObjectRef* super1 = super->instantiate(state->context());
        ObjectRef* base1 = base->instantiate(state->context());

        // base1.prototype.__proto__ == super1.prototype
        EXPECT_TRUE(base1->asFunctionObject()->getFunctionPrototype(state)->asObject()->getPrototype(state) == super1->asFunctionObject()->getFunctionPrototype(state));
        return ValueRef::createUndefined();
    },
                       super, base);

    // test super's instance
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionTemplateRef* super) -> ValueRef* {
        ObjectRef* ref = super->instantiate(state->context());

        // ref.b_p1
        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_p1")));
        // ref.b_i1
        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_i1")));
        // ref.b_ia
        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_ia")));
        // ref.prototype.b_p1
        EXPECT_TRUE(ref->asFunctionObject()->getFunctionPrototype(state)->asObject()->hasOwnProperty(state, StringRef::createFromASCII("b_p1")));
        // ref.prototype.b_i1
        EXPECT_FALSE(ref->asFunctionObject()->getFunctionPrototype(state)->asObject()->hasOwnProperty(state, StringRef::createFromASCII("b_i1")));
        // ref.prototype.b_ia
        EXPECT_FALSE(ref->asFunctionObject()->getFunctionPrototype(state)->asObject()->hasOwnProperty(state, StringRef::createFromASCII("b_ia")));

        return ValueRef::createUndefined();
    },
                       super);

    // test super's new instance
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionTemplateRef* super) -> ValueRef* {
        ObjectRef* ref = super->instanceTemplate()->instantiate(state->context());

        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_p1")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("b_p1"))->equalsTo(state, ValueRef::create(3.14)));
        EXPECT_TRUE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_i1")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("b_i1"))->equalsTo(state, ValueRef::create(3)));
        EXPECT_TRUE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_ia")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("b_ia"))->equalsTo(state, ValueRef::create(12)));

        return ValueRef::createUndefined();
    },
                       super);

    // test base's new instance
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionTemplateRef* base) -> ValueRef* {
        ObjectRef* ref = base->instanceTemplate()->instantiate(state->context());

        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_p1")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("b_p1"))->equalsTo(state, ValueRef::create(3.14)));
        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_i1")));
        EXPECT_TRUE(ref->hasOwnProperty(state, StringRef::createFromASCII("b_ia")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("b_ia"))->equalsTo(state, ValueRef::create(12)));

        EXPECT_FALSE(ref->hasOwnProperty(state, StringRef::createFromASCII("v1")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("v1"))->equalsTo(state, ValueRef::create(20.1)));
        EXPECT_TRUE(ref->hasOwnProperty(state, StringRef::createFromASCII("v2")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("v2"))->equalsTo(state, ValueRef::create(0)));

        return ValueRef::createUndefined();
    },
                       base);
}

TEST(FunctionTemplate, Basic4)
{
    auto ft = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "Parent"), 0, false, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        if (newTarget.hasValue()) {
            return thisValue;
        }
        return ValueRef::createUndefined();
    });
    ft->prototypeTemplate()->set(StringRef::createFromUTF8("run"), FunctionTemplateRef::create(AtomicStringRef::emptyAtomicString(), 0, false, false, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
                                     return ValueRef::create(1);
                                 }),
                                 true, true, true);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj) -> ValueRef* {
        g_context.get()->globalObject()->set(state, StringRef::createFromASCII("Parent"), obj);
        return ValueRef::createUndefined();
    },
                       ft->instantiate(g_context.get()));

    auto s = evalScript(g_context.get(), StringRef::createFromASCII("class Child extends Parent { constructor() { super(); } run() { return super.run() + 2; }}; var c = new Child(); c.run(); "), StringRef::createFromASCII("test.js"), false);
    EXPECT_EQ(s, "3");

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        bool result = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("Parent"));
        EXPECT_TRUE(result);

        return ValueRef::createUndefined();
    });
}

TEST(FunctionTemplate, Basic5)
{
    FunctionTemplateRef* ft = FunctionTemplateRef::create(AtomicStringRef::emptyAtomicString(), 0, false, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
        return ValueRef::createUndefined();
    });

    auto symbol = SymbolRef::create(StringRef::createFromASCII("asdf"));
    ft->prototypeTemplate()->set(symbol, ValueRef::create(true), true, true, true);
    auto obj = ft->instanceTemplate()->instantiate(g_context.get());
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, ObjectRef* obj, SymbolRef* symbol) -> ValueRef* {
        EXPECT_TRUE(obj->has(state, symbol));
        return ValueRef::createUndefined();
    },
                       obj, symbol);
}

TEST(BackingStore, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto bs = BackingStoreRef::createDefaultNonSharedBackingStore(1024);
        EXPECT_FALSE(bs->isShared());
        EXPECT_TRUE(bs->byteLength() == 1024);
        auto abo = ArrayBufferObjectRef::create(state);
        abo->attachBuffer(bs);
        EXPECT_TRUE(bs->data() == abo->rawBuffer());
        EXPECT_TRUE(abo->byteLength() == 1024);
        abo->detachArrayBuffer();
        EXPECT_TRUE(abo->byteLength() == 0);
        EXPECT_TRUE(abo->isDetachedBuffer());
        EXPECT_TRUE(abo->rawBuffer() == nullptr);
        EXPECT_FALSE(abo->backingStore().hasValue());

        bs->reallocate(300);
        abo->attachBuffer(bs);
        EXPECT_TRUE(abo->byteLength() == 300);
        EXPECT_FALSE(abo->isDetachedBuffer());
        EXPECT_TRUE(abo->rawBuffer() != nullptr);

        abo->allocateBuffer(state, 1024);
        EXPECT_TRUE(abo->byteLength() == 1024);

        // there is no error
        bs = BackingStoreRef::createNonSharedBackingStore(calloc(1024, 1), 1024, [](void* data, size_t length, void* deleterData) {
            free(data);
        },
                                                          nullptr);
        EXPECT_FALSE(bs->isShared());
        EXPECT_TRUE(bs->byteLength() == 1024);
        bs = nullptr;

        return ValueRef::createUndefined();
    });
}

TEST(SharedArrayBufferObject, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto bs = BackingStoreRef::createDefaultSharedBackingStore(10);
        auto sa = SharedArrayBufferObjectRef::create(state, bs);
        EXPECT_TRUE(sa->byteLength() == 10);

        return ValueRef::createUndefined();
    });
}

TEST(ObjectPropertyDescriptor, Basic1)
{
    {
        ObjectPropertyDescriptorRef desc(ValueRef::createUndefined());
        desc.setConfigurable(true);
        EXPECT_TRUE(desc.value()->isUndefined());
        EXPECT_TRUE(desc.hasValue());
        EXPECT_FALSE(desc.hasSetter());
        EXPECT_FALSE(desc.hasGetter());
        EXPECT_TRUE(desc.hasConfigurable());
        EXPECT_TRUE(desc.isConfigurable());
        EXPECT_FALSE(desc.hasEnumerable());
        EXPECT_FALSE(desc.hasWritable());
        desc.setConfigurable(false);
        EXPECT_TRUE(desc.hasConfigurable());
        EXPECT_FALSE(desc.isConfigurable());
    }
    {
        ObjectPropertyDescriptorRef desc(ValueRef::createUndefined(), false);
        EXPECT_TRUE(desc.value()->isUndefined());
        EXPECT_TRUE(desc.hasValue());
        EXPECT_FALSE(desc.hasSetter());
        EXPECT_FALSE(desc.hasGetter());
        EXPECT_FALSE(desc.hasEnumerable());
        EXPECT_FALSE(desc.hasConfigurable());
        EXPECT_TRUE(desc.hasWritable());
        EXPECT_FALSE(desc.isWritable());
    }
}

TEST(RegExp, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto re = RegExpObjectRef::create(state, StringRef::createFromASCII("foo"),
                                          RegExpObjectRef::RegExpObjectOption::None);
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("foo")));
        EXPECT_TRUE(re->option() == RegExpObjectRef::RegExpObjectOption::None);

        re = RegExpObjectRef::create(state, StringRef::createFromASCII("bar"),
                                     (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::Global));
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("bar")));
        EXPECT_TRUE(re->option() == (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::Global));

        re = RegExpObjectRef::create(state, StringRef::createFromASCII("baz"),
                                     (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::MultiLine));
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("baz")));
        EXPECT_TRUE(re->option() == (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::MultiLine));

        re = RegExpObjectRef::create(state, StringRef::createFromASCII("baz"),
                                     (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::Unicode | RegExpObjectRef::RegExpObjectOption::Sticky));
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("baz")));
        EXPECT_TRUE(re->option() == (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::Unicode | RegExpObjectRef::RegExpObjectOption::Sticky));

        re = RegExpObjectRef::create(state, StringRef::createFromASCII("foobar"), RegExpObjectRef::RegExpObjectOption::None);
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("foobar")));
        EXPECT_TRUE(re->option() == RegExpObjectRef::RegExpObjectOption::None);

        re = RegExpObjectRef::create(state, StringRef::createFromASCII("foobarbaz"),
                                     (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::MultiLine));
        EXPECT_TRUE(re->isRegExpObject());
        EXPECT_TRUE(re->source()->equals(StringRef::createFromASCII("foobarbaz")));
        EXPECT_TRUE(re->option() == (RegExpObjectRef::RegExpObjectOption)(RegExpObjectRef::RegExpObjectOption::IgnoreCase | RegExpObjectRef::RegExpObjectOption::MultiLine));

        return ValueRef::createUndefined();
    });
}

TEST(RegExp, Basic2)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto re = RegExpObjectRef::create(state, StringRef::createFromASCII("a.c"),
                                          RegExpObjectRef::RegExpObjectOption::None);
        EXPECT_TRUE(re->isRegExpObject());
        {
            RegExpObjectRef::RegexMatchResult result;
            EXPECT_TRUE(re->match(state, StringRef::createFromASCII("abc"), result, false, 0));
            EXPECT_TRUE(result.m_matchResults.size() == 1);
        }
        {
            RegExpObjectRef::RegexMatchResult result;
            EXPECT_FALSE(re->match(state, StringRef::createFromASCII("abd"), result, false, 0));
            EXPECT_TRUE(result.m_matchResults.size() == 0);
        }

        return ValueRef::createUndefined();
    });
}

TEST(EnumerateObjectOwnProperties, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ObjectRef* obj = ObjectRef::create(state);

        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("a_enum"), ValueRef::createUndefined(), true, true, true));
        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("b_non_enum"), ValueRef::createUndefined(), true, false, true));
        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("100"), ValueRef::createUndefined(), true, true, true));
        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("-20"), ValueRef::createUndefined(), true, true, true));
        EXPECT_TRUE(obj->defineDataProperty(state, ValueRef::create(10), ValueRef::createUndefined(), true, true, true));
        EXPECT_TRUE(obj->defineDataProperty(state, StringRef::createFromASCII("1"), ValueRef::createUndefined(), true, false, true));
        EXPECT_TRUE(obj->defineDataProperty(state, SymbolRef::create(StringRef::createFromASCII("sym_enum")), ValueRef::createUndefined(), true, true, true));
        EXPECT_TRUE(obj->defineDataProperty(state, SymbolRef::create(StringRef::createFromASCII("sym_non_enum")), ValueRef::createUndefined(), true, false, true));

        std::vector<uint32_t> indexes;
        ValueVectorRef* strings = ValueVectorRef::create();
        ValueVectorRef* symbols = ValueVectorRef::create();

        // iterate on enumerable properties including symbol keys
        obj->enumerateObjectOwnProperties(state, [&](ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable) -> bool {
            if (isEnumerable) {
                auto indexProperty = propertyName->tryToUseAsIndexProperty(state);
                if (indexProperty != ValueRef::InvalidIndexPropertyValue) {
                    indexes.push_back(indexProperty);
                } else if (propertyName->isSymbol()) {
                    symbols->pushBack(propertyName);
                } else {
                    strings->pushBack(propertyName);
                }
            }

            return true;
        },
                                          false);

        EXPECT_TRUE(indexes.size() == 2);
        EXPECT_TRUE(strings->size() == 2);
        EXPECT_TRUE(symbols->size() == 1);

        EXPECT_TRUE(indexes[0] == 100);
        EXPECT_TRUE(indexes[1] == 10);
        EXPECT_TRUE(strings->at(0)->equalsTo(state, StringRef::createFromASCII("a_enum")));
        EXPECT_TRUE(strings->at(1)->equalsTo(state, StringRef::createFromASCII("-20")));
        EXPECT_TRUE(symbols->at(0)->asSymbol()->description()->equalsTo(state, StringRef::createFromASCII("sym_enum")));

        return ValueRef::createUndefined();
    });
}

TEST(ErrorCallback, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register ErrorCreationCallback
        g_instance.get()->registerErrorCreationCallback([](ExecutionStateRef* state, ErrorObjectRef* err) -> void {
            // customize stack property in ErrorObject
            err->defineDataProperty(state, StringRef::createFromASCII("stack"), ValueRef::create(1), true, false, true);

            // compute stack trace data
            auto stackTraceVector = state->computeStackTrace();

            // save stack info
            ObjectRef* result = ObjectRef::create(state);
            result->set(state, StringRef::createFromASCII("src"), stackTraceVector[1].srcName);
            result->set(state, StringRef::createFromASCII("functionName"), stackTraceVector[1].functionName);

            g_context.get()->globalObject()->set(state, StringRef::createFromASCII("errorCBResult"), result);
        });
        return ValueRef::createUndefined();
    });

    auto result = evalScript(g_context.get(), StringRef::createFromASCII("function test() { try { throw new TypeError(); } catch (e) { this.errorCBResult.stack = e.stack } } test();"), StringRef::createFromASCII("testErrorCreationCallback.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* cbResult = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("errorCBResult"));
        EXPECT_TRUE(cbResult->isObject());
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("src"))->asString()->equalsWithASCIIString("testErrorCreationCallback.js", 28));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("functionName"))->asString()->equalsWithASCIIString("test", 4));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("stack"))->asNumber() == 1);

        EXPECT_TRUE(g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("errorCBResult")));

        // Unregister ErrorCallback
        g_instance.get()->unregisterErrorCreationCallback();
        g_instance.get()->unregisterErrorThrowCallback();
        return ValueRef::createUndefined();
    });

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register ErrorThrowCallback
        g_instance.get()->registerErrorThrowCallback([](ExecutionStateRef* state, ErrorObjectRef* err) -> void {
            // customize stack property in ErrorObject
            err->defineDataProperty(state, StringRef::createFromASCII("stack"), ValueRef::create(2), true, false, true);

            // compute stack trace data
            auto stackTraceVector = state->computeStackTrace();

            // save stack info
            ObjectRef* result = ObjectRef::create(state);
            result->set(state, StringRef::createFromASCII("src"), stackTraceVector[0].srcName);
            result->set(state, StringRef::createFromASCII("functionName"), stackTraceVector[0].functionName);

            g_context.get()->globalObject()->set(state, StringRef::createFromASCII("errorCBResult"), result);
        });
        return ValueRef::createUndefined();
    });

    result = evalScript(g_context.get(), StringRef::createFromASCII("function test() { try { throw new TypeError(); } catch (e) { this.errorCBResult.stack = e.stack } } test();"), StringRef::createFromASCII("testErrorThrowCallback.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* cbResult = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("errorCBResult"));
        EXPECT_TRUE(cbResult->isObject());
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("src"))->asString()->equalsWithASCIIString("testErrorThrowCallback.js", 25));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("functionName"))->asString()->equalsWithASCIIString("test", 4));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("stack"))->asNumber() == 2);

        EXPECT_TRUE(g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("errorCBResult")));

        // Unregister ErrorCallback
        g_instance.get()->unregisterErrorCreationCallback();
        g_instance.get()->unregisterErrorThrowCallback();
        return ValueRef::createUndefined();
    });
}

TEST(ErrorCallback, Basic2)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register ErrorCreationCallback
        g_instance.get()->registerErrorCreationCallback([](ExecutionStateRef* state, ErrorObjectRef* err) -> void {
            // customize stack property in ErrorObject
            err->defineDataProperty(state, StringRef::createFromASCII("stack"), ValueRef::create(1), true, false, true);

            // compute stack trace data
            auto stackTraceVector = state->computeStackTrace();

            // save stack info
            ObjectRef* result = ObjectRef::create(state);
            result->set(state, StringRef::createFromASCII("src"), stackTraceVector[1].srcName);
            result->set(state, StringRef::createFromASCII("functionName"), stackTraceVector[1].functionName);

            g_context.get()->globalObject()->set(state, StringRef::createFromASCII("errorCBResult"), result);
        });

        // Register ErrorThrowCallback
        g_instance.get()->registerErrorThrowCallback([](ExecutionStateRef* state, ErrorObjectRef* err) -> void {
            // customize stack property in ErrorObject
            err->defineDataProperty(state, StringRef::createFromASCII("stack"), ValueRef::create(2), true, false, true);

            // compute stack trace data
            auto stackTraceVector = state->computeStackTrace();

            // save stack info
            ObjectRef* result = ObjectRef::create(state);
            result->set(state, StringRef::createFromASCII("src"), stackTraceVector[0].srcName);
            result->set(state, StringRef::createFromASCII("functionName"), stackTraceVector[0].functionName);

            g_context.get()->globalObject()->set(state, StringRef::createFromASCII("errorCBResult"), result);
        });
        return ValueRef::createUndefined();
    });

    auto result1 = evalScript(g_context.get(), StringRef::createFromASCII("var func = new Function('return new SyntaxError();'); var result = func(); errorCBResult.stack = result.stack;"), StringRef::createFromASCII("testErrorCallback1.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* cbResult = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("errorCBResult"));
        EXPECT_TRUE(cbResult->isObject());
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("src"))->asString()->equalsWithASCIIString("testErrorCallback1.js", 21));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("functionName"))->asString()->equalsWithASCIIString("anonymous", 9));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("stack"))->asNumber() == 1);

        EXPECT_TRUE(g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("errorCBResult")));

        return ValueRef::createUndefined();
    });

    auto result2 = evalScript(g_context.get(), StringRef::createFromASCII("eval('new SyntaxError();');"), StringRef::createFromASCII("testErrorCallback2.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* cbResult = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("errorCBResult"));
        EXPECT_TRUE(cbResult->isObject());
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("src"))->asString()->equalsWithASCIIString("eval code", 9));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("functionName"))->asString()->length() == 0);

        EXPECT_TRUE(g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("errorCBResult")));

        return ValueRef::createUndefined();
    });

    auto result3 = evalScript(g_context.get(), StringRef::createFromASCII("function test() { try { throw new TypeError(); } catch (e) { this.errorCBResult.stack = e.stack } } test();"), StringRef::createFromASCII("testErrorCallback3.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* cbResult = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("errorCBResult"));
        EXPECT_TRUE(cbResult->isObject());
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("src"))->asString()->equalsWithASCIIString("testErrorCallback3.js", 21));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("functionName"))->asString()->equalsWithASCIIString("test", 4));
        EXPECT_TRUE(cbResult->asObject()->get(state, StringRef::createFromASCII("stack"))->asNumber() == 2);

        EXPECT_TRUE(g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("errorCBResult")));

        // Unregister ErrorCallback
        g_instance.get()->unregisterErrorCreationCallback();
        g_instance.get()->unregisterErrorThrowCallback();
        return ValueRef::createUndefined();
    });
}

TEST(PromiseHook, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register PromiseHook
        g_instance.get()->registerPromiseHook([](ExecutionStateRef* state, VMInstanceRef::PromiseHookType type, PromiseObjectRef* promise, ValueRef* parent) -> void {
            GlobalObjectRef* globalObj = g_context.get()->globalObject();
            switch (type) {
            case VMInstanceRef::PromiseHookType::Init: {
                StringRef* prop = StringRef::createFromASCII("hookInit");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::Resolve: {
                StringRef* prop = StringRef::createFromASCII("hookResolve");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::Before: {
                StringRef* prop = StringRef::createFromASCII("hookBefore");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::After: {
                StringRef* prop = StringRef::createFromASCII("hookAfter");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            default:
                break;
            }
        });
        return ValueRef::createUndefined();
    });

    auto result = evalScript(g_context.get(), StringRef::createFromASCII("var p = new Promise((resolve, reject) => { resolve(1); }); p.then((v) => {});"), StringRef::createFromASCII("testPromiseHook.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* initProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookInit"));
        ValueRef* resolveProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookResolve"));
        ValueRef* beforeProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookBefore"));
        ValueRef* afterProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookAfter"));
        EXPECT_TRUE(initProperty->isInt32() && initProperty->asInt32() == 2);
        EXPECT_TRUE(resolveProperty->isInt32() && resolveProperty->asInt32() == 2);
        EXPECT_TRUE(beforeProperty->isInt32() && beforeProperty->asInt32() == 1);
        EXPECT_TRUE(afterProperty->isInt32() && afterProperty->asInt32() == 1);

        bool removeInit = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookInit"));
        bool removeResolve = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookResolve"));
        bool removeBefore = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookBefore"));
        bool removeAfter = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookAfter"));
        EXPECT_TRUE(removeInit);
        EXPECT_TRUE(removeResolve);
        EXPECT_TRUE(removeBefore);
        EXPECT_TRUE(removeAfter);

        // Unregister PromiseHook
        g_instance.get()->unregisterPromiseHook();
        return ValueRef::createUndefined();
    });
}

TEST(PromiseHook, Basic2)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register PromiseHook
        g_instance.get()->registerPromiseHook([](ExecutionStateRef* state, VMInstanceRef::PromiseHookType type, PromiseObjectRef* promise, ValueRef* parent) -> void {
            GlobalObjectRef* globalObj = g_context.get()->globalObject();
            switch (type) {
            case VMInstanceRef::PromiseHookType::Init: {
                StringRef* prop = StringRef::createFromASCII("hookInit");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::Resolve: {
                StringRef* prop = StringRef::createFromASCII("hookResolve");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::Before: {
                StringRef* prop = StringRef::createFromASCII("hookBefore");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseHookType::After: {
                StringRef* prop = StringRef::createFromASCII("hookAfter");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            default:
                break;
            }
        });

        // trigger PromiseHook
        auto p = PromiseObjectRef::create(state);
        p->then(state, ValueRef::createUndefined());
        p->fulfill(state, ValueRef::create(1));

        // execute pending jobs
        while (g_instance.get()->hasPendingJob()) {
            g_instance.get()->executePendingJob();
        }

        ValueRef* initProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookInit"));
        ValueRef* resolveProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookResolve"));
        ValueRef* beforeProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookBefore"));
        ValueRef* afterProperty = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("hookAfter"));
        EXPECT_TRUE(initProperty->isInt32() && initProperty->asInt32() == 2);
        EXPECT_TRUE(resolveProperty->isInt32() && resolveProperty->asInt32() == 2);
        EXPECT_TRUE(beforeProperty->isInt32() && beforeProperty->asInt32() == 1);
        EXPECT_TRUE(afterProperty->isInt32() && afterProperty->asInt32() == 1);

        bool removeInit = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookInit"));
        bool removeResolve = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookResolve"));
        bool removeBefore = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookBefore"));
        bool removeAfter = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("hookAfter"));
        EXPECT_TRUE(removeInit);
        EXPECT_TRUE(removeResolve);
        EXPECT_TRUE(removeBefore);
        EXPECT_TRUE(removeAfter);

        // Unregister PromiseHook
        g_instance.get()->unregisterPromiseHook();
        return ValueRef::createUndefined();
    });
}

TEST(PromiseRejectCallback, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        // Register PromiseRejectCallback
        g_instance.get()->registerPromiseRejectCallback([](ExecutionStateRef* state, PromiseObjectRef* promise, ValueRef* value, VMInstanceRef::PromiseRejectEvent event) -> void {
            GlobalObjectRef* globalObj = g_context.get()->globalObject();
            switch (event) {
            case VMInstanceRef::PromiseRejectEvent::PromiseRejectWithNoHandler: {
                EXPECT_TRUE(promise->promiseResult() == value);
                EXPECT_TRUE(value->isInt32() && value->asInt32() == 1);
                StringRef* prop = StringRef::createFromASCII("promiseRejectWithNoHandler");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            case VMInstanceRef::PromiseRejectEvent::PromiseHandlerAddedAfterReject: {
                EXPECT_TRUE(promise->promiseResult() == value);
                EXPECT_TRUE(value->isInt32() && value->asInt32() == 1);
                StringRef* prop = StringRef::createFromASCII("promiseHandlerAddedAfterReject");
                int count = 1;
                if (globalObj->has(state, prop)) {
                    count += globalObj->get(state, prop)->asInt32();
                }
                globalObj->set(state, prop, ValueRef::create(count));
                break;
            }
            default:
                break;
            }
        });
        return ValueRef::createUndefined();
    });

    auto result = evalScript(g_context.get(), StringRef::createFromASCII("var p = new Promise((resolve, reject) => { reject(1); }); p.then((v) => {}, (v) => {});"), StringRef::createFromASCII("testPromiseRejectCallback.js"), false);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        ValueRef* noHandler = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("promiseRejectWithNoHandler"));
        ValueRef* addedAfterHandler = g_context.get()->globalObject()->get(state, StringRef::createFromASCII("promiseHandlerAddedAfterReject"));
        EXPECT_TRUE(noHandler->isInt32() && noHandler->asInt32() == 1);
        EXPECT_TRUE(addedAfterHandler->isInt32() && addedAfterHandler->asInt32() == 1);

        bool removeNoHandler = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("promiseRejectWithNoHandler"));
        bool removeAddedAfterHandler = g_context.get()->globalObject()->deleteOwnProperty(state, StringRef::createFromASCII("promiseHandlerAddedAfterReject"));
        EXPECT_TRUE(removeNoHandler);
        EXPECT_TRUE(removeAddedAfterHandler);

        // Unregister PromiseRejectCallback
        g_instance.get()->unregisterPromiseRejectCallback();
        return ValueRef::createUndefined();
    });
}

TEST(Serializer, Basic1)
{
    std::ostringstream ostream;
    ValueRef* v1 = ValueRef::createUndefined();
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->isUndefined());
}

TEST(Serializer, Basic2)
{
    std::ostringstream ostream;
    ValueRef* v1 = ValueRef::createNull();
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->isNull());
}

TEST(Serializer, Basic3)
{
    std::ostringstream ostream;
    ValueRef* v1 = ValueRef::create(true);
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asBoolean());
}

TEST(Serializer, Basic4)
{
    std::ostringstream ostream;
    ValueRef* v1 = ValueRef::create(123123.0);
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asNumber() == 123123.0);
}

TEST(Serializer, Basic5)
{
    std::ostringstream ostream;
    ValueRef* v1 = StringRef::createFromUTF8("asdfhelohellohellohelloasdfasdflksadjf;laksvn;lasdkf;lkjasd;lfkj");
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asString()->equals(v1->asString()));
}

TEST(Serializer, Basic6)
{
    std::ostringstream ostream;
    ValueRef* v1 = BigIntRef::create(StringRef::createFromASCII("123123123123123123123123123123123123123123321"));
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asBigInt()->equals(v1->asBigInt()));
}

TEST(Serializer, Basic7)
{
    std::ostringstream ostream;
    ValueRef* v1 = SymbolRef::create(nullptr);
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_FALSE(v2->asSymbol()->description().hasValue());
}

TEST(Serializer, Basic8)
{
    std::ostringstream ostream;
    ValueRef* v1 = SymbolRef::create(StringRef::createFromASCII("asdfasdfasdf"));
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asSymbol()->description().value()->equalsWithASCIIString("asdfasdfasdf", 12));
}

TEST(Serializer, Basic9)
{
    std::ostringstream ostream;
    ValueRef* v1 = ValueRef::create(123123.0);
    SerializerRef::serializeInto(v1, ostream);
    v1 = StringRef::createFromUTF8("asdfhelohellohellohelloasdfasdflksadjf;laksvn;lasdkf;lkjasd;lfkj");
    SerializerRef::serializeInto(v1, ostream);
    std::istringstream istream(ostream.str());
    ValueRef* v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asNumber() == 123123.0);
    v2 = SerializerRef::deserializeFrom(g_context.get(), istream);
    EXPECT_TRUE(v2->asString()->equals(v1->asString()));
}

TEST(ExecutionState, TryCatchFinally)
{
    Evaluator::execute(g_context, [](ExecutionStateRef* state) -> ValueRef* {
        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(g_context.get(), "test"),
                                                                 [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall) -> ValueRef* {
                                                                     OptionalRef<ExecutionStateRef> e = state;
                                                                     if (argv[0]->asNumber() == 1) {
                                                                         while (e) {
                                                                             if (e->onTry()) {
                                                                                 return ValueRef::createUndefined();
                                                                             }
                                                                             e = e->parent();
                                                                         }
                                                                     } else if (argv[0]->asNumber() == 2) {
                                                                         while (e) {
                                                                             if (e->onCatch()) {
                                                                                 return ValueRef::createUndefined();
                                                                             }
                                                                             e = e->parent();
                                                                         }
                                                                     } else {
                                                                         while (e) {
                                                                             if (e->onFinally()) {
                                                                                 return ValueRef::createUndefined();
                                                                             }
                                                                             e = e->parent();
                                                                         }
                                                                     }
                                                                     EXPECT_TRUE(false);
                                                                     return ValueRef::createUndefined();
                                                                 },
                                                                 1, true, false);
        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
        g_context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("tryCatchTest"), buildFunctionObjectRef, true, true, true);
        return ValueRef::createUndefined();
    });

    evalScript(g_context.get(), StringRef::createFromASCII("try { tryCatchTest(1); throw 1; } catch(e) { tryCatchTest(2) } finally{ tryCatchTest(3) }"), StringRef::createFromASCII("test.js"), false);
}

TEST(IteratorObject, GenericIterator)
{
    std::vector<int> sampleData = { 0, 1, 2 };
    struct IteratorData {
        std::vector<int>& data;
        size_t index;

        IteratorData(std::vector<int>& data)
            : data(data)
            , index(0)
        {
        }
    };

    IteratorData d(sampleData);

    Evaluator::execute(g_context, [](ExecutionStateRef* state, IteratorData* data) -> ValueRef* {
        GenericIteratorObjectRef* o = GenericIteratorObjectRef::create(state, [](ExecutionStateRef* state, void* data) -> std::pair<ValueRef*, bool> {
            IteratorData* d = (IteratorData*)data;
            if (d->index >= d->data.size()) {
                return std::make_pair(ValueRef::createUndefined(), true);
            }
            return std::make_pair(ValueRef::create(d->data[d->index++]), false);
        },
                                                                       data);
        g_context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("genericIteratorTest"), o, true, true, true);
        return ValueRef::createUndefined();
    },
                       &d);

    evalScript(g_context.get(), StringRef::createFromASCII(R"(
    {
        let idx = 0;
        for(let xx of genericIteratorTest) {
            testAssert(xx, idx++)
        }
    }
)"),
               StringRef::createFromASCII("test.js"), false);
}

TEST(ReloadableString, Basic)
{
    char reloadableStringTestSource[] = "let x = 'test String'";
    EXPECT_TRUE(StringRef::isReloadableStringEnabled());

    struct Data {
        char* string;
        bool flag;
    };

    Data d;
    d.string = reloadableStringTestSource;
    d.flag = false;

    StringRef* string = StringRef::createReloadableString(g_context->vmInstance(), true, strlen(reloadableStringTestSource), &d, [](void* callbackData) -> void* {
        Data* d = static_cast<Data*>(callbackData);
        const char* reloadableStringTestSource = static_cast<const char*>(d->string);
        size_t len = strlen(reloadableStringTestSource);
        void* ptr = malloc(len);
        memcpy(ptr, reloadableStringTestSource, len);
        d->flag = true;
        return ptr; }, [](void* memoryPtr, void* callbackData) {
        Data* d = static_cast<Data*>(callbackData);
        d->flag = false;
        free(memoryPtr); });

    EXPECT_FALSE(d.flag);
    EXPECT_TRUE(string->has8BitContent());
    EXPECT_FALSE(d.flag);
    string->length();
    EXPECT_FALSE(d.flag);
    string->charAt(0);
    EXPECT_TRUE(d.flag);
    EXPECT_TRUE(string->equalsWithASCIIString(reloadableStringTestSource, strlen(reloadableStringTestSource)));
    g_context->vmInstance()->enterIdleMode();
    EXPECT_FALSE(d.flag);

    Evaluator::execute(g_context, [](ExecutionStateRef* state, StringRef* string, Data* d) -> ValueRef* {
        ValueRef* argv[1] = { StringRef::createFromASCII("asdf") };
        EXPECT_FALSE(d->flag);
        auto fn = FunctionObjectRef::create(state, AtomicStringRef::create(state->context(), "test"), 1, argv, string);
        // body string(string) is unloaded right after function creation because body string is no longer necessary when function creation finished
        EXPECT_FALSE(d->flag);
        EXPECT_TRUE(fn->toString(state)->toStdUTF8String() == "function test(asdf\n) {\nlet x = 'test String'\n}");
        EXPECT_FALSE(d->flag);
        EXPECT_TRUE(fn->toString(state)->has8BitContent());
        EXPECT_FALSE(d->flag);
        string->charAt(0);
        EXPECT_TRUE(d->flag);
        g_context->vmInstance()->enterIdleMode();
        EXPECT_FALSE(d->flag);

        return ValueRef::createUndefined();
    },
                       string, &d);
}

class DebuggerTest : public DebuggerOperationsRef::DebuggerClient {
public:
    DebuggerTest()
        : inEval(false)
        , stopAtBreakpointCount(0)
    {
        memset(codeRefs, 0, sizeof(codeRefs));
        memset(offsets, 0, sizeof(offsets));
    }

    virtual void parseCompleted(StringRef* source, StringRef* srcName, std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>& breakpointLocationsVector) override;
    virtual void parseError(StringRef* source, StringRef* srcName, StringRef* error) override;
    virtual void codeReleased(DebuggerOperationsRef::WeakCodeRef* weakCodeRef) override;
    virtual DebuggerOperationsRef::ResumeBreakpointOperation stopAtBreakpoint(DebuggerOperationsRef::BreakpointOperations& operations) override;

    bool inEval;
    int stopAtBreakpointCount;
    DebuggerOperationsRef::WeakCodeRef* codeRefs[2];
    uint32_t offsets[5];
};

static int debuggerParseCompletedCount = 0;
static int debuggerParseErrorCount = 0;
static char debuggerFileNameString[] = "test.js";
static char debuggerSourceString1[] = "var a = 1\n"
                                      "function func(arg) {\n"
                                      "   let x = 4, y = 'str'\n"
                                      "   a = 2\n"
                                      "   a = 3\n"
                                      "   const z = 6\n"
                                      "}\n"
                                      "func(null)\n"
                                      "a = 4\n";
static char debuggerSourceString2[] = "var a = ;";

void DebuggerTest::parseCompleted(StringRef* source, StringRef* srcName, std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>& breakpointLocationsVector)
{
    if (inEval) {
        return;
    }

    EXPECT_TRUE(source->equalsWithASCIIString(debuggerSourceString1, sizeof(debuggerSourceString1) - 1));
    EXPECT_TRUE(srcName->equalsWithASCIIString(debuggerFileNameString, sizeof(debuggerFileNameString) - 1));

    EXPECT_EQ(breakpointLocationsVector.size(), 2);
    EXPECT_TRUE(DebuggerOperationsRef::getFunctionName(breakpointLocationsVector[0]->weakCodeRef)->equalsWithASCIIString("", 0));
    EXPECT_EQ(breakpointLocationsVector[0]->breakpointLocations.size(), 3);
    EXPECT_EQ(breakpointLocationsVector[0]->breakpointLocations[0].line, 1);
    EXPECT_EQ(breakpointLocationsVector[0]->breakpointLocations[1].line, 8);
    EXPECT_EQ(breakpointLocationsVector[0]->breakpointLocations[2].line, 9);

    EXPECT_TRUE(DebuggerOperationsRef::getFunctionName(breakpointLocationsVector[1]->weakCodeRef)->equalsWithASCIIString("func", 4));
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations.size(), 5);
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations[0].line, 3);
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations[1].line, 4);
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations[2].line, 5);
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations[3].line, 6);
    EXPECT_EQ(breakpointLocationsVector[1]->breakpointLocations[4].line, 7);

    codeRefs[0] = breakpointLocationsVector[0]->weakCodeRef;
    codeRefs[1] = breakpointLocationsVector[1]->weakCodeRef;
    offsets[0] = breakpointLocationsVector[0]->breakpointLocations[0].offset;
    offsets[1] = breakpointLocationsVector[0]->breakpointLocations[1].offset;
    offsets[2] = breakpointLocationsVector[1]->breakpointLocations[0].offset;
    offsets[3] = breakpointLocationsVector[1]->breakpointLocations[2].offset;
    offsets[4] = breakpointLocationsVector[0]->breakpointLocations[2].offset;

    EXPECT_FALSE(DebuggerOperationsRef::updateBreakpoint(codeRefs[0], offsets[0], false));
    EXPECT_TRUE(DebuggerOperationsRef::updateBreakpoint(codeRefs[0], offsets[0], true));
    EXPECT_FALSE(DebuggerOperationsRef::updateBreakpoint(codeRefs[0], offsets[0], true));
    EXPECT_TRUE(DebuggerOperationsRef::updateBreakpoint(codeRefs[1], offsets[3], true));

    debuggerParseCompletedCount++;
}

void DebuggerTest::parseError(StringRef* source, StringRef* srcName, StringRef* error)
{
    EXPECT_TRUE(source->equalsWithASCIIString(debuggerSourceString2, sizeof(debuggerSourceString2) - 1));
    EXPECT_TRUE(srcName->equalsWithASCIIString(debuggerFileNameString, sizeof(debuggerFileNameString) - 1));

    static char errorString[] = "Line 1: Unexpected token ;";
    EXPECT_TRUE(error->equalsWithASCIIString(errorString, sizeof(errorString) - 1));

    debuggerParseErrorCount++;
}

void DebuggerTest::codeReleased(DebuggerOperationsRef::WeakCodeRef* weakCodeRef)
{
    EXPECT_TRUE(weakCodeRef != nullptr);
}

DebuggerOperationsRef::ResumeBreakpointOperation DebuggerTest::stopAtBreakpoint(DebuggerOperationsRef::BreakpointOperations& operations)
{
    stopAtBreakpointCount++;

    switch (stopAtBreakpointCount) {
    case 1: {
        EXPECT_EQ(operations.weakCodeRef(), codeRefs[0]);
        EXPECT_EQ(operations.offset(), offsets[0]);
        EXPECT_TRUE(DebuggerOperationsRef::updateBreakpoint(codeRefs[0], offsets[0], false));
        return DebuggerOperationsRef::Next;
    }
    case 2: {
        EXPECT_EQ(operations.weakCodeRef(), codeRefs[0]);
        EXPECT_EQ(operations.offset(), offsets[1]);

        DebuggerOperationsRef::DebuggerStackTraceDataVector stackTrace;
        operations.getStackTrace(stackTrace);

        EXPECT_EQ(stackTrace.size(), 1);
        EXPECT_EQ(stackTrace[0].weakCodeRef, codeRefs[0]);
        EXPECT_EQ(stackTrace[0].line, 8);
        EXPECT_EQ(stackTrace[0].column, 1);
        EXPECT_EQ(stackTrace[0].depth, 0);

        DebuggerOperationsRef::LexicalScopeChainVector scopeChain;
        operations.getLexicalScopeChain(0, scopeChain);

        EXPECT_EQ(scopeChain.size(), 1);
        EXPECT_EQ(scopeChain[0], DebuggerOperationsRef::GLOBAL_ENVIRONMENT);

        inEval = true;
        StringRef* sourceCode = StringRef::createFromUTF8("a", 1);
        bool is_error;
        size_t objectRef;
        StringRef* result = operations.eval(sourceCode, is_error, objectRef);
        EXPECT_FALSE(is_error);
        EXPECT_EQ(objectRef, SIZE_MAX);
        EXPECT_TRUE(result->equalsWithASCIIString("1", 1));

        sourceCode = StringRef::createFromUTF8("aa", 2);
        result = operations.eval(sourceCode, is_error, objectRef);
        EXPECT_TRUE(is_error);
        EXPECT_EQ(objectRef, SIZE_MAX);
        static char errorMessage[] = "ReferenceError: aa is not defined";
        EXPECT_TRUE(result->equalsWithASCIIString(errorMessage, sizeof(errorMessage) - 1));
        inEval = false;

        return DebuggerOperationsRef::Step;
    }
    case 3: {
        EXPECT_EQ(operations.weakCodeRef(), codeRefs[1]);
        EXPECT_EQ(operations.offset(), offsets[2]);

        DebuggerOperationsRef::DebuggerStackTraceDataVector stackTrace;
        operations.getStackTrace(stackTrace);

        EXPECT_EQ(stackTrace.size(), 2);

        EXPECT_EQ(stackTrace[0].weakCodeRef, codeRefs[1]);
        EXPECT_EQ(stackTrace[0].line, 3);
        EXPECT_EQ(stackTrace[0].column, 8);
        EXPECT_EQ(stackTrace[0].depth, 0);

        EXPECT_EQ(stackTrace[1].weakCodeRef, codeRefs[0]);
        EXPECT_EQ(stackTrace[1].line, 8);
        EXPECT_EQ(stackTrace[1].column, 1);
        EXPECT_EQ(stackTrace[1].depth, 2);

        return DebuggerOperationsRef::Continue;
    }
    case 4: {
        EXPECT_EQ(operations.weakCodeRef(), codeRefs[1]);
        EXPECT_EQ(operations.offset(), offsets[3]);

        inEval = true;
        StringRef* sourceCode = StringRef::createFromUTF8("a", 1);
        bool is_error;
        size_t objectRef;
        StringRef* result = operations.eval(sourceCode, is_error, objectRef);
        EXPECT_TRUE(result->equalsWithASCIIString("2", 1));
        inEval = false;

        DebuggerOperationsRef::LexicalScopeChainVector scopeChain;
        operations.getLexicalScopeChain(0, scopeChain);

        EXPECT_EQ(scopeChain.size(), 3);
        EXPECT_EQ(scopeChain[0], DebuggerOperationsRef::DECLARATIVE_ENVIRONMENT);
        EXPECT_EQ(scopeChain[1], DebuggerOperationsRef::FUNCTION_ENVIRONMENT);
        EXPECT_EQ(scopeChain[2], DebuggerOperationsRef::GLOBAL_ENVIRONMENT);

        operations.getLexicalScopeChain(2, scopeChain);
        EXPECT_EQ(scopeChain[0], DebuggerOperationsRef::GLOBAL_ENVIRONMENT);

        DebuggerOperationsRef::PropertyKeyValueVector properties = operations.getLexicalScopeChainProperties(0, 0);
        EXPECT_EQ(properties.size(), 3);
        EXPECT_TRUE(properties[0].key->equalsWithASCIIString("x", 1));
        EXPECT_TRUE(properties[0].value.hasValue());
        EXPECT_TRUE(properties[0].value->isNumber());
        EXPECT_TRUE(properties[0].value->asNumber() == 4);
        EXPECT_TRUE(properties[1].key->equalsWithASCIIString("y", 1));
        EXPECT_TRUE(properties[1].value.hasValue());
        EXPECT_TRUE(properties[1].value->isString());
        EXPECT_TRUE(properties[1].value->asString()->equalsWithASCIIString("str", 3));
        EXPECT_TRUE(properties[2].key->equalsWithASCIIString("z", 1));
        EXPECT_FALSE(properties[2].value.hasValue());

        properties = operations.getLexicalScopeChainProperties(0, 1);
        EXPECT_EQ(properties.size(), 1);
        EXPECT_TRUE(properties[0].key->equalsWithASCIIString("arg", 3));
        EXPECT_TRUE(properties[0].value.hasValue());
        EXPECT_TRUE(properties[0].value->isNull());

        return DebuggerOperationsRef::Finish;
    }
    }

    EXPECT_EQ(stopAtBreakpointCount, 5);
    EXPECT_EQ(operations.weakCodeRef(), codeRefs[0]);
    EXPECT_EQ(operations.offset(), offsets[4]);
    return DebuggerOperationsRef::Continue;
}

TEST(Debugger, Basic)
{
    PersistentRefHolder<VMInstanceRef> instance = VMInstanceRef::create();
    PersistentRefHolder<ContextRef> context = createEscargotContext(instance.get());
    DebuggerTest* debuggerTest = new DebuggerTest();
    StringRef* fileName = StringRef::createFromUTF8(debuggerFileNameString, sizeof(debuggerFileNameString) - 1);

    context->initDebugger(debuggerTest);

    StringRef* source = StringRef::createFromUTF8(debuggerSourceString1, sizeof(debuggerSourceString1) - 1);
    evalScript(context, source, fileName, false);

    EXPECT_EQ(debuggerParseCompletedCount, 1);
    EXPECT_EQ(debuggerParseErrorCount, 0);

    source = StringRef::createFromUTF8(debuggerSourceString2, sizeof(debuggerSourceString2) - 1);
    evalScript(context, source, fileName, false);

    EXPECT_EQ(debuggerParseCompletedCount, 1);
    EXPECT_EQ(debuggerParseErrorCount, 1);
    EXPECT_EQ(debuggerTest->stopAtBreakpointCount, 5);

    context.release();
    instance.release();
}

TEST(Debugger, RemoteOption)
{
    PersistentRefHolder<VMInstanceRef> instance = VMInstanceRef::create();
    PersistentRefHolder<ContextRef> context = createEscargotContext(instance.get());
    // 100ms
    EXPECT_FALSE(context->initDebuggerRemote("--accept-timeout=100"));
    EXPECT_FALSE(context->isDebuggerRunning());
    EXPECT_FALSE(context->isWaitBeforeExit());

    context.release();
    instance.release();
}

static char debuggerSourceString3[] = "var a = { get x() { return {} } },\n"
                                      "    b = a\n"
                                      "a = b";

class DebuggerObjectTest : public DebuggerOperationsRef::DebuggerClient {
public:
    DebuggerObjectTest()
        : inEval(false)
        , stopAtBreakpointCount(0)
    {
    }

    virtual void parseCompleted(StringRef* source, StringRef* srcName, std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>& breakpointLocationsVector) override;
    virtual void parseError(StringRef* source, StringRef* srcName, StringRef* error) override;
    virtual void codeReleased(DebuggerOperationsRef::WeakCodeRef* weakCodeRef) override;
    virtual DebuggerOperationsRef::ResumeBreakpointOperation stopAtBreakpoint(DebuggerOperationsRef::BreakpointOperations& operations) override;

    bool inEval;
    int stopAtBreakpointCount;
};

void DebuggerObjectTest::parseCompleted(StringRef* source, StringRef* srcName, std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>& breakpointLocationsVector)
{
    if (inEval) {
        return;
    }

    EXPECT_TRUE(source->equalsWithASCIIString(debuggerSourceString3, sizeof(debuggerSourceString3) - 1));
    EXPECT_TRUE(srcName->equalsWithASCIIString(debuggerFileNameString, sizeof(debuggerFileNameString) - 1));
    EXPECT_EQ(breakpointLocationsVector.size(), 3);
}

void DebuggerObjectTest::parseError(StringRef* source, StringRef* srcName, StringRef* error)
{
    (void)source;
    (void)srcName;
    (void)error;

    EXPECT_TRUE(false);
}

void DebuggerObjectTest::codeReleased(DebuggerOperationsRef::WeakCodeRef* weakCodeRef)
{
    EXPECT_TRUE(weakCodeRef != nullptr);
}

DebuggerOperationsRef::ResumeBreakpointOperation DebuggerObjectTest::stopAtBreakpoint(DebuggerOperationsRef::BreakpointOperations& operations)
{
    stopAtBreakpointCount++;

    if (stopAtBreakpointCount <= 2) {
        return DebuggerOperationsRef::Next;
    }

    bool is_error;
    size_t objectRef1, objectRef2;
    StringRef* sourceCode = StringRef::createFromUTF8("a", 1);

    StringRef* result = operations.eval(sourceCode, is_error, objectRef1);
    EXPECT_FALSE(is_error);
    EXPECT_EQ(result, nullptr);

    sourceCode = StringRef::createFromUTF8("b", 1);

    result = operations.eval(sourceCode, is_error, objectRef2);
    EXPECT_FALSE(is_error);
    EXPECT_EQ(result, nullptr);

    EXPECT_EQ(objectRef1, objectRef2);

    sourceCode = StringRef::createFromUTF8("a.x", 1);

    result = operations.eval(sourceCode, is_error, objectRef2);
    EXPECT_FALSE(is_error);
    EXPECT_EQ(result, nullptr);
    EXPECT_TRUE(objectRef1 != objectRef2);

    return DebuggerOperationsRef::Continue;
}

TEST(Debugger, ObjectStore)
{
    PersistentRefHolder<VMInstanceRef> instance = VMInstanceRef::create();
    PersistentRefHolder<ContextRef> context = createEscargotContext(instance.get());
    DebuggerTest* debuggerTest = new DebuggerTest();
    StringRef* fileName = StringRef::createFromUTF8(debuggerFileNameString, sizeof(debuggerFileNameString) - 1);

    context->initDebugger(debuggerTest);

    StringRef* source = StringRef::createFromUTF8(debuggerSourceString1, sizeof(debuggerSourceString1) - 1);
    evalScript(context, source, fileName, false);

    source = StringRef::createFromUTF8(debuggerSourceString2, sizeof(debuggerSourceString2) - 1);
    evalScript(context, source, fileName, false);

    context.release();
    instance.release();
}
