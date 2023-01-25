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

static ValueRef* builtinRead(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 1) {
        auto f = argv[0]->toString(state)->toStdUTF8String();
        const char* fileName = f.data();
        StringRef* src = builtinHelperFileRead(state, fileName, "read").value();
        return src;
    } else {
        return StringRef::emptyString();
    }
}

static ValueRef* builtinRun(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 1) {
        double startTime = DateObjectRef::currentTime();

        auto f = argv[0]->toString(state)->toStdUTF8String();
        const char* fileName = f.data();
        StringRef* src = builtinHelperFileRead(state, fileName, "run").value();
        bool isModule = stringEndsWith(f, "mjs");
        auto script = state->context()->scriptParser()->initializeScript(src, argv[0]->toString(state), isModule).fetchScriptThrowsExceptionIfParseError(state);
        script->execute(state);
        return ValueRef::create(DateObjectRef::currentTime() - startTime);
    } else {
        return ValueRef::create(0);
    }
}

static ValueRef* builtinGc(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    Memory::gc();
    return ValueRef::createUndefined();
}

PersistentRefHolder<ContextRef> createEscargotContext(VMInstanceRef* instance, bool isMainThread = true);

#if defined(ESCARGOT_ENABLE_TEST)

static bool evalScript(ContextRef* context, StringRef* source, StringRef* srcName, bool shouldPrintScriptResult, bool isModule);

static ValueRef* builtinUneval(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc) {
        if (argv[0]->isSymbol()) {
            return argv[0]->asSymbol()->symbolDescriptiveString();
        }
        return argv[0]->toString(state);
    }
    return StringRef::emptyString();
}

static ValueRef* builtinDrainJobQueue(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    ContextRef* context = state->context();
    while (context->vmInstance()->hasPendingJob()) {
        auto jobResult = context->vmInstance()->executePendingJob();
        if (jobResult.error) {
            return ValueRef::create(false);
        }
    }
    return ValueRef::create(true);
}

static ValueRef* builtinAddPromiseReactions(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argc >= 3) {
        PromiseObjectRef* promise = argv[0]->toObject(state)->asPromiseObject();
        promise->then(state, argv[1], argv[2]);
    } else {
        state->throwException(TypeErrorObjectRef::create(state, StringRef::emptyString()));
    }
    return ValueRef::createUndefined();
}

static ValueRef* builtinCreateNewGlobalObject(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    return ContextRef::create(state->context()->vmInstance())->globalObject();
}

static ValueRef* builtin262CreateRealm(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    auto newContext = createEscargotContext(state->context()->vmInstance(), false);
    return newContext->globalObject()->get(state, StringRef::createFromASCII("$262"));
}

static ValueRef* builtin262DetachArrayBuffer(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    if (argv[0]->isArrayBufferObject()) {
        argv[0]->asArrayBufferObject()->detachArrayBuffer();
    }

    return ValueRef::createUndefined();
}

static ValueRef* builtin262EvalScript(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    StringRef* src = argv[0]->toString(state);
    auto script = state->context()->scriptParser()->initializeScript(src, StringRef::createFromASCII("$262.evalScript input"), false).fetchScriptThrowsExceptionIfParseError(state);
    return script->execute(state);
}

static ValueRef* builtin262IsHTMLDDA(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    return ValueRef::createNull();
}

struct WorkerThreadData {
    std::string message;
    bool running;
    volatile bool ended;

    WorkerThreadData()
        : running(true)
        , ended(false)
    {
    }
};
std::mutex workerMutex;
std::vector<std::pair<std::thread, WorkerThreadData>> workerThreads;
std::vector<std::string> messagesFromWorkers;

static ValueRef* builtin262AgentStart(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    std::string script = argv[0]->toString(state)->toStdUTF8String();

    std::thread worker([](std::string script) {
        Globals::initializeThread();

        Memory::setGCFrequency(24);

        PersistentRefHolder<VMInstanceRef> instance = VMInstanceRef::create();
        PersistentRefHolder<ContextRef> context = createEscargotContext(instance.get(), false);

        evalScript(context.get(), StringRef::createFromUTF8(script.data(), script.size()),
                   StringRef::createFromASCII("from main thread"), false, false);

        while (true) {
            {
                bool running = false;
                std::lock_guard<std::mutex> guard(workerMutex);
                for (size_t i = 0; i < workerThreads.size(); i++) {
                    if (workerThreads[i].first.get_id() == std::this_thread::get_id()) {
                        running = workerThreads[i].second.running;
                        break;
                    }
                }
                if (!running) {
                    break;
                }
            }

            // readmessage if exists
            std::string message;
            {
                std::lock_guard<std::mutex> guard(workerMutex);
                for (size_t i = 0; i < workerThreads.size(); i++) {
                    if (workerThreads[i].first.get_id() == std::this_thread::get_id()) {
                        message = std::move(workerThreads[i].second.message);
                        break;
                    }
                }
            }

            if (message.length()) {
                std::istringstream istream(message);
                ValueRef* val1 = SerializerRef::deserializeFrom(context.get(), istream);
                ValueRef* val2 = SerializerRef::deserializeFrom(context.get(), istream);

                ValueRef* callback = (ValueRef*)context.get()->globalObject()->extraData();
                if (callback) {
                    Evaluator::execute(context.get(), [](ExecutionStateRef* state, ValueRef* callback, ValueRef* v1, ValueRef* v2) -> ValueRef* {
                        ValueRef* argv[2] = { v1, v2 };
                        return callback->call(state, ValueRef::createUndefined(), 2, argv);
                    },
                                       callback, val1, val2);
                }
            }

            while (context->vmInstance()->hasPendingJob() || context->vmInstance()->hasPendingJobFromAnotherThread()) {
                if (context->vmInstance()->waitEventFromAnotherThread(10)) {
                    context->vmInstance()->executePendingJobFromAnotherThread();
                }
                if (context->vmInstance()->hasPendingJob()) {
                    auto jobResult = context->vmInstance()->executePendingJob();
                    if (jobResult.error) {
                        if (jobResult.error) {
                            LOGI("Uncaught %s: in agent\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                        }
                    }
                }
            }

            usleep(10 * 1000);
        }

        context.release();
        instance.release();

        Globals::finalizeThread();

        std::lock_guard<std::mutex> guard(workerMutex);
        for (size_t i = 0; i < workerThreads.size(); i++) {
            if (workerThreads[i].first.get_id() == std::this_thread::get_id()) {
                workerThreads[i].second.ended = true;
            }
        }
    },
                       script);

    {
        std::lock_guard<std::mutex> guard(workerMutex);
        workerThreads.push_back(std::make_pair(std::move(worker), WorkerThreadData()));
    }

    return ValueRef::createUndefined();
}

static ValueRef* builtin262AgentBroadcast(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    std::ostringstream ostream;
    if (argc > 0) {
        SerializerRef::serializeInto(argv[0], ostream);
    } else {
        SerializerRef::serializeInto(ValueRef::createUndefined(), ostream);
    }
    if (argc > 1) {
        SerializerRef::serializeInto(argv[1], ostream);
    } else {
        SerializerRef::serializeInto(ValueRef::createUndefined(), ostream);
    }


    std::string message(ostream.str());
    {
        std::lock_guard<std::mutex> guard(workerMutex);
        for (size_t i = 0; i < workerThreads.size(); i++) {
            workerThreads[i].second.message = message;
        }
    }

    while (true) {
        bool thereIsNoMessage = true;
        {
            std::lock_guard<std::mutex> guard(workerMutex);
            for (size_t i = 0; i < workerThreads.size(); i++) {
                if (workerThreads[i].second.message.size()) {
                    thereIsNoMessage = false;
                    break;
                }
            }
        }

        if (thereIsNoMessage) {
            break;
        }

        usleep(10 * 1000); // sleep 10ms
    }


    return ValueRef::createUndefined();
}

static ValueRef* builtin262AgentReport(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    std::string message;
    if (argc > 0) {
        message = argv[0]->toString(state)->toStdUTF8String();
    }
    std::lock_guard<std::mutex> guard(workerMutex);
    messagesFromWorkers.push_back(message);
    return ValueRef::createUndefined();
}

static ValueRef* builtin262AgentLeaving(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    std::lock_guard<std::mutex> guard(workerMutex);
    for (size_t i = 0; i < workerThreads.size(); i++) {
        if (workerThreads[i].first.get_id() == std::this_thread::get_id()) {
            workerThreads[i].second.running = false;
            break;
        }
    }
    return ValueRef::createUndefined();
}

static ValueRef* builtin262AgentReceiveBroadcast(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    state->context()->globalObject()->setExtraData(argv[0]);
    return ValueRef::createUndefined();
}

static ValueRef* builtin262AgentGetReport(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    std::lock_guard<std::mutex> guard(workerMutex);
    if (messagesFromWorkers.size()) {
        std::string message = messagesFromWorkers.front();
        messagesFromWorkers.erase(messagesFromWorkers.begin());
        return StringRef::createFromUTF8(message.data(), message.size());
    } else {
        return ValueRef::createNull();
    }
}

static ValueRef* builtin262AgentMonotonicNow(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ValueRef::create((uint64_t)tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
}

static ValueRef* builtin262AgentSleep(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    double m = argv[0]->toNumber(state);
    usleep(m * 1000.0);
    return ValueRef::createUndefined();
}

#endif

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

static bool evalScript(ContextRef* context, StringRef* source, StringRef* srcName, bool shouldPrintScriptResult, bool isModule)
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
        return false;
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
        return false;
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
                    LOGI("Uncaught %s:\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                    result = false;
                } else {
                    LOGI("%s\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                }
            }
        }
    }
    return result;
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
                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "read"), builtinRead, 1, true, false);
                               FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                               context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("read"), buildFunctionObjectRef, true, true, true);
                           }

                           {
                               FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "run"), builtinRun, 1, true, false);
                               FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                               context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("run"), buildFunctionObjectRef, true, true, true);
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

PersistentRefHolder<VMInstanceRef> g_instance;
PersistentRefHolder<ContextRef> g_context;

extern "C" JNIEXPORT void Java_com_samsung_lwe_Escargot_init(JNIEnv * env, jclass clz)
{
    ShellPlatform* platform = new ShellPlatform();
    Globals::initialize(platform);

    Memory::setGCFrequency(24);

    g_instance = VMInstanceRef::create();
    g_context = createEscargotContext(g_instance.get());
}

extern "C" JNIEXPORT void Java_com_samsung_lwe_Escargot_eval(JNIEnv * env, jclass clz, jstring code)
{
    jboolean isSucceed;
    const char *string = env->GetStringUTFChars(code, &isSucceed);
    StringRef* src = StringRef::createFromUTF8(string, strlen(string));
    evalScript(g_context.get(), src, StringRef::createFromASCII("java input"), true, false);
    env->ReleaseStringUTFChars(code, string);
}
