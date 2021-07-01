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
        for (size_t i = 0; i < evalResult.stackTraceData.size(); i++) {
            snprintf(str, sizeof(str), "%s (%d:%d)\n", evalResult.stackTraceData[i].src->toStdUTF8String().data(), (int)evalResult.stackTraceData[i].loc.line, (int)evalResult.stackTraceData[i].loc.column);
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

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    Globals::initialize();

    Memory::setGCFrequency(24);

    ShellPlatform* platform = new ShellPlatform();
    PersistentRefHolder<VMInstanceRef> g_instance = VMInstanceRef::create(platform);
    g_instance->setOnVMInstanceDelete([](VMInstanceRef* instance) {
        delete instance->platform();
    });

    g_context = ContextRef::create(g_instance.get());

    return RUN_ALL_TESTS();
}

TEST(ValueRef, Basic1) {
    auto val = ValueRef::create(-1);
    auto vector = ValueVectorRef::create(5);
    vector->set(0, val);
    auto ii = vector->at(0)->asInt32();
    EXPECT_EQ(ii, -1);
}

TEST(ValueRef, Basic2) {
    auto val = ValueRef::create(123);
    auto vector = ValueVectorRef::create(5);
    vector->set(0, val);
    auto ii = vector->at(0)->asInt32();
    EXPECT_EQ(ii, 123);
}

TEST(ValueRef, Basic3) {
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto value = ValueRef::create(-1);
        uint32_t result1 = value->toArrayIndex(state);
        int32_t result2 = value->asInt32();
        EXPECT_EQ(result1, ValueRef::InvalidArrayIndexValue);
        EXPECT_EQ(result2, -1);

        uint32_t maxValue = std::numeric_limits<uint32_t>::max();
        value = ValueRef::create(maxValue);
        result1 = value->toArrayIndex(state);
        result2 = value->toIndex(state);
        EXPECT_EQ(result1, maxValue);
        EXPECT_EQ(result2, maxValue);

        return ValueRef::createUndefined();
    });
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

    tpl->setNativeDataAccessorProperty(StringRef::createFromASCII("asdf"), data);

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
}

TEST(ObjectTemplate, Basic4)
{
    ObjectTemplateRef* tpl = ObjectTemplateRef::create();

    ObjectTemplateNamedPropertyHandlerData handler;
    handler.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName) -> OptionalRef<ValueRef> {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("aaa", 3)) {
            return (ValueRef*)self->extraData();
        }
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("ccc", 3)) {
            return ValueRef::create(555.555);
        }
        return OptionalRef<ValueRef>();
    };

    handler.setter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName, ValueRef* value) -> OptionalRef<ValueRef> {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("aaa", 3)) {
            self->setExtraData(value);
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("ccc", 3)) {
            self->setExtraData(ValueRef::create(123));
            return OptionalRef<ValueRef>(ValueRef::create(true));
        }

        return OptionalRef<ValueRef>();
    };

    handler.query = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName) -> TemplatePropertyAttribute {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("aaa", 3)) {
            return (TemplatePropertyAttribute)(TemplatePropertyAttribute::TemplatePropertyAttributeWritable | TemplatePropertyAttribute::TemplatePropertyAttributeEnumerable | TemplatePropertyAttribute::TemplatePropertyAttributeConfigurable);
        }
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("ccc", 3)) {
            return (TemplatePropertyAttribute)(TemplatePropertyAttribute::TemplatePropertyAttributeWritable);
        }
        return TemplatePropertyAttribute::TemplatePropertyAttributeNotExist;
    };

    handler.enumerator = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data) -> TemplateNamedPropertyHandlerEnumerationCallbackResultVector {
        TemplateNamedPropertyHandlerEnumerationCallbackResultVector v(2);
        v[0] = StringRef::createFromASCII("aaa");
        v[1] = StringRef::createFromASCII("ccc");
        return v;
    };

    handler.deleter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName) -> OptionalRef<ValueRef> {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("ddd", 3)) {
            return OptionalRef<ValueRef>(ValueRef::create(false));
        }

        return OptionalRef<ValueRef>();
    };

    handler.descriptor = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName) -> OptionalRef<ValueRef> {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("eee", 3)) {
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
    ObjectTemplateNamedPropertyHandlerData handler2;
    handler2.getter = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName) -> OptionalRef<ValueRef> {
        return OptionalRef<ValueRef>();
    };
    handler2.definer = [](ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, const TemplatePropertyNameRef& propertyName, const ObjectPropertyDescriptorRef& desc) -> OptionalRef<ValueRef> {
        if (propertyName.value()->isString() && propertyName.value()->asString()->equalsWithASCIIString("ttt", 3)) {
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
    auto ft = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "parent"), 0,
                                          true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
                                              return ValueRef::createUndefined();
                                          });
    ft->prototypeTemplate()->set(StringRef::createFromASCII("asdf1"), ValueRef::create(1), true, true, true);

    auto ftchild = FunctionTemplateRef::create(AtomicStringRef::create(g_context.get(), "asdf"), 2,
                                               true, true, [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget) -> ValueRef* {
                                                   return ValueRef::create(123);
                                               });
    auto ftchildobj = ftchild->instanceTemplate();
    ftchildobj->set(StringRef::createFromASCII("asdf"), ValueRef::create(0), true, true, true);

    ftchild->prototypeTemplate()->set(StringRef::createFromASCII("asdf2"), ValueRef::create(2), true, true, true);
    ftchild->inherit(ft);

    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state, FunctionTemplateRef* ftchild) -> ValueRef* {
        ObjectRef* ref = ftchild->instanceTemplate()->instantiate(state->context());

        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("asdf"))->equalsTo(state, ValueRef::create(0)));
        EXPECT_TRUE(ref->hasOwnProperty(state, StringRef::createFromASCII("asdf")));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("asdf1"))->equalsTo(state, ValueRef::create(1)));
        EXPECT_TRUE(ref->get(state, StringRef::createFromASCII("asdf2"))->equalsTo(state, ValueRef::create(2)));

        return ValueRef::createUndefined();
    },
                       ftchild);
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

TEST(BackingStore, Basic1)
{
    Evaluator::execute(g_context.get(), [](ExecutionStateRef* state) -> ValueRef* {
        auto bs = BackingStoreRef::create(1024);
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

        abo->allocateBuffer(state, 1024);
        EXPECT_TRUE(abo->byteLength() == 1024);

        // there is no error
        bs = BackingStoreRef::create(calloc(1024, 1), 1024, [](void* data, size_t length, void* deleterData) {
            free(data);
        },
                                     nullptr);
        EXPECT_FALSE(bs->isShared());
        EXPECT_TRUE(bs->byteLength() == 1024);
        bs = nullptr;

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
                if (propertyName->isUInt32()) {
                    indexes.push_back(propertyName->asUInt32());
                } else {
                    if (propertyName->isSymbol()) {
                        symbols->pushBack(propertyName);
                    } else {
                        auto index = propertyName->tryToUseAsArrayIndex(state);
                        if (index == ValueRef::InvalidArrayIndexValue) {
                            strings->pushBack(propertyName);
                        } else {
                            indexes.push_back(index);
                        }
                    }
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
