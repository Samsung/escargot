#include <jni.h>
#include <android/log.h>

#include <EscargotPublic.h>
using namespace Escargot;

#define  LOG_TAG    "Escargot"
#define  LOGUNK(...)  __android_log_print(ANDROID_LOG_UNKNOWN,LOG_TAG,__VA_ARGS__)
#define  LOGDEF(...)  __android_log_print(ANDROID_LOG_DEFAULT,LOG_TAG,__VA_ARGS__)
#define  LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGF(...)  __android_log_print(ANDROID_FATAL_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGS(...)  __android_log_print(ANDROID_SILENT_ERROR,LOG_TAG,__VA_ARGS__)

static bool stringEndsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

ValueRef* builtinPrint(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 1) {
        StringRef* printMsg;
        if (argv[0]->isSymbol()) {
            printMsg = argv[0]->asSymbol()->symbolDescriptiveString();
            LOGI("%s", printMsg->toStdUTF8String().data());
            state->context()->printDebugger(printMsg);
        } else {
            printMsg = argv[0]->toString(state);
            LOGI("%s", printMsg->toStdUTF8String().data());
            state->context()->printDebugger(printMsg->toString(state));
        }
    } else {
        LOGI("%s", "undefined");
    }
    return ValueRef::createUndefined();
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
                    src = StringRef::createFromUTF8ToCompressibleString(state->context()->vmInstance(), utf8Str.data(), utf8Str.length(), false);
                } else {
                    src = StringRef::createFromLatin1ToCompressibleString(state->context()->vmInstance(), str.data(), str.length());
                }
            } else {
                if (hasNonLatin1Content) {
                    src = StringRef::createFromUTF8(utf8Str.data(), utf8Str.length(), false);
                } else {
                    src = StringRef::createFromLatin1(str.data(), str.length());
                }
            }
        } else {
            if (hasNonLatin1Content) {
                src = StringRef::createFromUTF8(utf8Str.data(), utf8Str.length(), false);
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
            LOGI("%s", msg);
        }
        return nullptr;
    }
}

static ValueRef* builtinLoad(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 1) {
        auto f = argv[0]->toString(state)->toStdUTF8String();
        const char* fileName = f.data();
        StringRef* src = builtinHelperFileRead(state, fileName, "load").value();
        bool isModule = stringEndsWith(f, "mjs");

        auto script = state->context()->scriptParser()->initializeScript(src, argv[0]->toString(state), isModule).fetchScriptThrowsExceptionIfParseError(state);
        return script->execute(state);
    } else {
        return ValueRef::createUndefined();
    }
}

static ValueRef* builtinGc(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    Memory::gc();
    return ValueRef::createUndefined();
}

PersistentRefHolder<ContextRef> createEscargotContext(VMInstanceRef* instance, bool isMainThread = true);

class ShellPlatform : public PlatformRef {
public:
    bool m_canBlock;

    ShellPlatform()
            : m_canBlock(true)
    {
    }

    void setCanBlock(bool b)
    {
        m_canBlock = b;
    }

    virtual void markJSJobEnqueued(ContextRef* relatedContext) override
    {
        // ignore. we always check pending job after eval script
    }

    virtual void markJSJobFromAnotherThreadExists(ContextRef* relatedContext) override
    {
        // ignore. we always check pending job after eval script
    }

    virtual LoadModuleResult onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc, ModuleType type) override
    {
        std::string referrerPath = whereRequestFrom->src()->toStdUTF8String();
        auto& loadedModules = *reinterpret_cast<std::vector<std::tuple<std::string, ContextRef*, PersistentRefHolder<ScriptRef>>>*>(threadLocalCustomData());

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

        ScriptParserRef::InitializeScriptResult parseResult;
        StringRef* srcName = StringRef::createFromUTF8(absPath.data(), absPath.size());

        if (type == ModuleJSON) {
            parseResult = relatedContext->scriptParser()->initializeJSONModule(source.value(), srcName);
        } else {
            parseResult = relatedContext->scriptParser()->initializeScript(source.value(), srcName, true);
        }

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
        auto& loadedModules = *reinterpret_cast<std::vector<std::tuple<std::string, ContextRef*, PersistentRefHolder<ScriptRef>>>*>(threadLocalCustomData());
        loadedModules.push_back(std::make_tuple(path, relatedContext, PersistentRefHolder<ScriptRef>(loadedModule)));
    }

    virtual void hostImportModuleDynamically(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, ModuleType type, PromiseObjectRef* promise) override
    {
        LoadModuleResult loadedModuleResult = onLoadModule(relatedContext, referrer, src, type);
        notifyHostImportModuleDynamicallyResult(relatedContext, referrer, src, promise, loadedModuleResult);
    }

    virtual bool canBlockExecution(ContextRef* relatedContext) override
    {
        return m_canBlock;
    }

    virtual void* allocateThreadLocalCustomData() override
    {
        return new std::vector<std::tuple<std::string /* abs path */, ContextRef*, PersistentRefHolder<ScriptRef>>>();
    }

    virtual void deallocateThreadLocalCustomData() override
    {
        delete reinterpret_cast<std::vector<std::tuple<std::string, ContextRef*, PersistentRefHolder<ScriptRef>>>*>(threadLocalCustomData());
    }

private:
    std::string dirnameOf(const std::string& fname)
    {
        size_t pos = fname.find_last_of("/");
        if (std::string::npos == pos) {
            pos = fname.find_last_of("\\/");
        }
        return (std::string::npos == pos)
               ? ""
               : fname.substr(0, pos);
    }

    std::string absolutePath(const std::string& referrerPath, const std::string& src)
    {
        std::string utf8MayRelativePath = dirnameOf(referrerPath) + "/" + src;
        auto absPath = realpath(utf8MayRelativePath.data(), nullptr);
        if (!absPath) {
            return std::string();
        }
        std::string utf8AbsolutePath = absPath;
        free(absPath);

        return utf8AbsolutePath;
    }

    std::string absolutePath(const std::string& src)
    {
        auto absPath = realpath(src.data(), nullptr);
        if (!absPath) {
            return std::string();
        }
        std::string utf8AbsolutePath = absPath;
        free(absPath);

        return utf8AbsolutePath;
    }
};

static Evaluator::EvaluatorResult evalScript(ContextRef* context, StringRef* source, StringRef* srcName, bool shouldPrintScriptResult, bool isModule)
{
    if (stringEndsWith(srcName->toStdUTF8String(), "mjs")) {
        isModule = isModule || true;
    }

    auto scriptInitializeResult = context->scriptParser()->initializeScript(source, srcName, isModule);
    if (!scriptInitializeResult.script) {
        LOGI("Script parsing error: ");
        switch (scriptInitializeResult.parseErrorCode) {
            case Escargot::ErrorObjectRef::Code::SyntaxError:
                LOGI("SyntaxError");
                break;
            case Escargot::ErrorObjectRef::Code::EvalError:
                LOGI("EvalError");
                break;
            case Escargot::ErrorObjectRef::Code::RangeError:
                LOGI("RangeError");
                break;
            case Escargot::ErrorObjectRef::Code::ReferenceError:
                LOGI("ReferenceError");
                break;
            case Escargot::ErrorObjectRef::Code::TypeError:
                LOGI("TypeError");
                break;
            case Escargot::ErrorObjectRef::Code::URIError:
                LOGI("URIError");
                break;
            default:
                break;
        }
        LOGI(": %s\n", scriptInitializeResult.parseErrorMessage->toStdUTF8String().data());
        Evaluator::EvaluatorResult evalResult;
        evalResult.error = StringRef::createFromASCII("script parsing error");
        return evalResult;
    }

    auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
                                             return script->execute(state);
                                         },
                                         scriptInitializeResult.script.get());

    if (!evalResult.isSuccessful()) {
        LOGI("Uncaught %s:\n", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
        for (size_t i = 0; i < evalResult.stackTrace.size(); i++) {
            LOGI("%s (%d:%d)\n", evalResult.stackTrace[i].srcName->toStdUTF8String().data(), (int)evalResult.stackTrace[i].loc.line, (int)evalResult.stackTrace[i].loc.column);
        }
        return evalResult;
    }

    if (shouldPrintScriptResult) {
        LOGI("%s", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
    }

    bool result = true;
    while (context->vmInstance()->hasPendingJob() || context->vmInstance()->hasPendingJobFromAnotherThread()) {
        if (context->vmInstance()->waitEventFromAnotherThread(10)) {
            context->vmInstance()->executePendingJobFromAnotherThread();
        }
        if (context->vmInstance()->hasPendingJob()) {
            auto jobResult = context->vmInstance()->executePendingJob();
            if (shouldPrintScriptResult || jobResult.error) {
                if (jobResult.error) {
                    LOGI("Uncaught %s:(in promise job)\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                    result = false;
                } else {
                    LOGI("%s(in promise job)\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                }
            }
        }
    }
    return evalResult;
}


PersistentRefHolder<ContextRef> createEscargotContext(VMInstanceRef* instance, bool isMainThread)
{
    PersistentRefHolder<ContextRef> context = ContextRef::create(instance);

    Evaluator::execute(context, [](ExecutionStateRef* state, bool isMainThread) -> ValueRef* {
                           ContextRef* context = state->context();

                           GlobalObjectProxyObjectRef* proxy = GlobalObjectProxyObjectRef::create(state, state->context()->globalObject(), [](ExecutionStateRef* state, GlobalObjectProxyObjectRef* proxy, GlobalObjectRef* targetGlobalObject, GlobalObjectProxyObjectRef::AccessOperationType operationType, OptionalRef<AtomicStringRef> nonIndexedStringPropertyNameIfExists) {
                               // TODO check security
                           });
                           context->setGlobalObjectProxy(proxy);

                           {
                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "print"), builtinPrint, 1, true, false);
                               FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                               context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("print"), buildFunctionObjectRef, true, true, true);
                           }

                           {
                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "load"), builtinLoad, 1, true, false);
                               FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                               context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("load"), buildFunctionObjectRef, true, true, true);
                           }

                           {
                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "gc"), builtinGc, 0, true, false);
                               FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                               context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("gc"), buildFunctionObjectRef, true, true, true);
                           }

                           return ValueRef::createUndefined();
                       },
                       isMainThread);

    return context;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_initializeGlobals(JNIEnv *env, jclass clazz) {
    ShellPlatform* platform = new ShellPlatform();
    Globals::initialize(platform);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_finalizeGlobals(JNIEnv *env, jclass clazz) {
    Globals::finalize();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_samsung_lwe_escargot_Globals_version(JNIEnv *env, jclass clazz) {
    std::string version = Globals::version();
    std::basic_string<uint16_t> u16Version;

    for (auto c : version) {
        u16Version.push_back(c);
    }

    return env->NewString(u16Version.data(), u16Version.length());
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_samsung_lwe_escargot_Globals_buildDate(JNIEnv *env, jclass clazz) {
    std::string version = Globals::buildDate();
    std::basic_string<uint16_t> u16Version;

    for (auto c : version) {
        u16Version.push_back(c);
    }

    return env->NewString(u16Version.data(), u16Version.length());
}
extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Memory_gc(JNIEnv *env, jclass clazz) {
    Memory::gc();
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_samsung_lwe_escargot_Memory_heapSize(JNIEnv *env, jclass clazz) {
    return Memory::heapSize();
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_samsung_lwe_escargot_Memory_totalSize(JNIEnv *env, jclass clazz) {
    return Memory::totalSize();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Memory_setGCFrequency(JNIEnv *env, jclass clazz, jint value) {
    Memory::setGCFrequency(value);
}

static std::string fetchStringFromJavaOptionalString(JNIEnv *env, jobject optional)
{
    auto classOptionalString = env->GetObjectClass(optional);
    auto methodIsPresent = env->GetMethodID(classOptionalString, "isPresent", "()Z");
    if (env->CallBooleanMethod(optional, methodIsPresent)) {
        auto methodGet = env->GetMethodID(classOptionalString, "get", "()Ljava/lang/Object;");
        jboolean isSucceed;
        jstring value = static_cast<jstring>(env->CallObjectMethod(optional, methodGet));
        const char* str = env->GetStringUTFChars(
                value, &isSucceed);
        auto length = env->GetStringUTFLength(value);
        auto ret = std::string(str, length);
        env->ReleaseStringUTFChars(value, str);
        return ret;
    }
    return std::string();
}

template<typename NativeType>
static PersistentRefHolder<NativeType>* getPersistentPointerFromJava(JNIEnv *env, jclass clazz, jobject object)
{
    auto ptr = env->GetLongField(object, env->GetFieldID(clazz, "m_nativePointer", "J"));
    PersistentRefHolder<NativeType>* pVMRef = reinterpret_cast<PersistentRefHolder<NativeType>*>(ptr);
    return pVMRef;
}

template<typename NativeType>
static NativeType* getNativePointerFromJava(JNIEnv *env, jclass clazz, jobject object)
{
    return getPersistentPointerFromJava<NativeType>(env, clazz, object)->get();
}

template<typename NativeType>
static void setNativePointerToJava(JNIEnv *env, jclass clazz, jobject object, NativeType* ptr)
{
    env->SetLongField(object, env->GetFieldID(clazz, "m_nativePointer", "J"), reinterpret_cast<jlong>(ptr));
}

template<typename NativeType>
static void setPersistentPointerToJava(JNIEnv *env, jclass clazz, jobject object, PersistentRefHolder<NativeType>&& ptr)
{
    PersistentRefHolder<NativeType>* pRef = new PersistentRefHolder<NativeType>(std::move(ptr));
    setNativePointerToJava(env, clazz, object, pRef);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_VMInstance_create(JNIEnv *env, jclass clazz, jobject locale,
                                                jobject timezone) {
    std::string localeString = fetchStringFromJavaOptionalString(env, locale);
    std::string timezoneString = fetchStringFromJavaOptionalString(env, timezone);

    auto vmRef = VMInstanceRef::create(localeString.length() ? localeString.data() : nullptr,
                                       timezoneString.length() ? timezoneString.data() : nullptr);
    auto vmObject = env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "()V"));
    setPersistentPointerToJava<VMInstanceRef>(env, clazz, vmObject, std::move(vmRef));
    return vmObject;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_VMInstance_releaseNativePointer(JNIEnv *env, jobject thiz) {
    auto ptr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(thiz), thiz);
    delete ptr;
    setNativePointerToJava<VMInstanceRef>(env, env->GetObjectClass(thiz), thiz, nullptr);
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_Context_create(JNIEnv *env, jclass clazz, jobject vmInstance) {
    auto vmPtr = getPersistentPointerFromJava<VMInstanceRef>(env, env->GetObjectClass(vmInstance), vmInstance);
    auto contextRef = createEscargotContext(vmPtr->get());

    auto contextObject = env->NewObject(clazz, env->GetMethodID(clazz, "<init>", "()V"));
    setPersistentPointerToJava<ContextRef>(env, clazz, contextObject, std::move(contextRef));
    return contextObject;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Context_releaseNativePointer(JNIEnv *env, jobject thiz) {
    auto ptr = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(thiz), thiz);
    delete ptr;
    setNativePointerToJava<ContextRef>(env, env->GetObjectClass(thiz), thiz, nullptr);
}

StringRef* createJSStringFromJava(JNIEnv *env, jstring str)
{
    jboolean isSucceed;
    const char* cString = env->GetStringUTFChars(str, &isSucceed);
    StringRef* code = StringRef::createFromUTF8(cString, env->GetStringUTFLength(str));
    env->ReleaseStringUTFChars(str, cString);
    return code;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_Evaluator_evalScript(JNIEnv *env, jclass clazz, jobject context,
                                                   jstring source, jstring sourceFileName,
                                                   jboolean shouldPrintScriptResult) {
    auto ptr = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto result = evalScript(ptr->get(), createJSStringFromJava(env, source), createJSStringFromJava(env, sourceFileName), shouldPrintScriptResult, false);

    jclass optionalClazz = env->FindClass("java/util/Optional");
    if (result.isSuccessful()) {
        auto buf = result.resultOrErrorToString(ptr->get());
        jstring javaString = env->NewStringUTF(buf->toStdUTF8String().data());
        return env->CallStaticObjectMethod(optionalClazz,
                                                  env->GetStaticMethodID(optionalClazz, "of", "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                                  javaString);
    }
    return env->CallStaticObjectMethod(optionalClazz, env->GetStaticMethodID(optionalClazz, "empty", "()Ljava/util/Optional;"));
}

JavaVM* g_jvm;

OptionalRef<JNIEnv> fetchJNIEnvFromCallback()
{
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, NULL) != 0) {
            // give up
            return nullptr;
        }
    }
    return env;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_samsung_lwe_escargot_Bridge_register(JNIEnv *env, jclass clazz, jobject context,
                                              jstring objectName, jstring propertyName,
                                              jobject adapter) {
    auto contextPtr = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    auto jsObjectName = createJSStringFromJava(env, objectName);
    auto jsPropertyName = createJSStringFromJava(env, propertyName);

    adapter = env->NewGlobalRef(adapter);

    if (!g_jvm) {
        env->GetJavaVM(&g_jvm);
    }

    auto evalResult = Evaluator::execute(contextPtr->get(), [](ExecutionStateRef* state, JNIEnv *env, jobject adapter, StringRef* jsObjectName, StringRef* jsPropertyName) -> ValueRef* {
        auto globalObject = state->context()->globalObject();
        ObjectRef* targetObject;

        ValueRef* willBeTargetObject = globalObject->getOwnProperty(state, jsObjectName);
        if (willBeTargetObject->isObject()) {
            targetObject = willBeTargetObject->asObject();
        } else {
            targetObject = ObjectRef::create(state);
            globalObject->defineDataProperty(state, jsObjectName, targetObject, true, true, true);
        }

        // AtomicStringRef* name, NativeFunctionPointer fn, size_t argc, bool isStrict = true, bool isConstructor = true
        FunctionObjectRef::NativeFunctionInfo info(AtomicStringRef::emptyAtomicString(), [](ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall) -> ValueRef* {
            FunctionObjectRef* callee = state->resolveCallee().get();
            jobject jo = static_cast<jobject>(reinterpret_cast<FunctionObjectRef *>(callee)->extraData());
            auto env = fetchJNIEnvFromCallback();
            if (!env) {
                // give up
                LOGE("could not fetch env from callback");
            }

            jobject callbackArg;
            jclass optionalClazz = env->FindClass("java/util/Optional");
            if (argc) {
                auto buf = argv[0]->toString(state)->toStdUTF8String();
                jstring javaString = env->NewStringUTF(buf.data());
                callbackArg = env->CallStaticObjectMethod(optionalClazz,
                                                          env->GetStaticMethodID(optionalClazz, "of", "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                                          javaString);
            } else {
                callbackArg = env->CallStaticObjectMethod(optionalClazz,
                                                    env->GetStaticMethodID(optionalClazz, "empty", "()Ljava/util/Optional;"));
            }
            auto javaReturnValue = env->CallObjectMethod(jo,
                                             env->GetMethodID(env->GetObjectClass(jo), "callback", "(Ljava/util/Optional;)Ljava/util/Optional;"),
                                             callbackArg);

            auto methodIsPresent = env->GetMethodID(optionalClazz, "isPresent", "()Z");
            if (env->CallBooleanMethod(javaReturnValue, methodIsPresent)) {
                auto methodGet = env->GetMethodID(optionalClazz, "get", "()Ljava/lang/Object;");
                jstring value = static_cast<jstring>(env->CallObjectMethod(javaReturnValue, methodGet));

                jboolean isSucceed;
                const char* str = env->GetStringUTFChars(
                        value, &isSucceed);
                auto length = env->GetStringUTFLength(value);
                auto ret = StringRef::createFromUTF8(str, length);
                env->ReleaseStringUTFChars(value, str);
                return ret;
            }
            return ValueRef::createUndefined();
        }, 1, true, false);
        FunctionObjectRef* callback = FunctionObjectRef::create(state, info);
        targetObject->defineDataProperty(state, jsPropertyName, callback, true, true, true);
        return callback;
    }, env, adapter, jsObjectName, jsPropertyName);

    if (evalResult.isSuccessful()) {
        FunctionObjectRef* callback = evalResult.result->asFunctionObject();
        callback->setExtraData(adapter);
        Memory::gcRegisterFinalizer(callback, [](void* self) {
            jobject jo = static_cast<jobject>(reinterpret_cast<FunctionObjectRef *>(self)->extraData());
            auto env = fetchJNIEnvFromCallback();
            if (env) {
                env->DeleteGlobalRef(jo);
            }
        });
    } else {
        env->DeleteGlobalRef(adapter);
    }

    return evalResult.isSuccessful();
}