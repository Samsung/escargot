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

#include <string.h>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono>

#if defined(_MSC_VER)
#include <stdlib.h>
#define realpath(N, R) _fullpath((R), (N), _MAX_PATH)
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(ESCARGOT_GOOGLE_PERF)
#include <gperftools/profiler.h>
#endif

#include "api/EscargotPublic.h"

#if defined(ESCARGOT_ENABLE_TEST)
// these header & function below are used for Escargot internal development
// general client doesn't need this

#if defined(ANDROID)
#include <unwind.h>
#include <dlfcn.h>
#endif

#if !defined(__APPLE__) && !defined(_WINDOWS) && !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#include <execinfo.h>

void btSighandler(int sig, struct sigcontext ctx)
{
    if (sig == SIGSEGV) {
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64) \
    || defined(i386) || defined(__i386) || defined(__i386__) || defined(__IA32__) || defined(_M_IX86) || defined(__X86__)      \
    || defined(_X86_) || defined(__THW_INTEL__) || defined(__I86__) || defined(__INTEL__) || defined(__386)
        printf("Got signal %d, pid %d, faulty address is %p\n",
               sig, (int)getpid(), (void*)ctx.cr2);
#elif defined(__arm__) || defined(__thumb__) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARMT) || defined(__arm) || defined(__arm) || defined(__aarch64__)
        printf("Got signal %d, pid %d, faulty address is %p\n",
               sig, (int)getpid(), (void*)ctx.fault_address);
#else
        printf("Got signal %d, pid %d\n", sig, (int)getpid());
#endif
    } else {
        printf("Got signal %d, pid %d\n", sig, (int)getpid());
    }

    printf("[bt] Execution path:\n");
#if defined(ANDROID)
    struct BacktraceContext {
        int ignoreCount; // ignore signal handler
        int currentDepth;
        int maxDepth;
    } backtraceContext = { 1, 0, 128 };

    _Unwind_Backtrace([](struct _Unwind_Context* ctx, void* data) -> _Unwind_Reason_Code {
        BacktraceContext* backtraceContext = (BacktraceContext*)data;
        if (backtraceContext->ignoreCount < backtraceContext->currentDepth) {
            void* pc = (void*)_Unwind_GetIP(ctx);

            Dl_info dlInfo;
            dladdr(pc, &dlInfo);

            void* computedPc = (void*)((size_t)pc - (size_t)dlInfo.dli_fbase);

            printf("[bt] #%d pc: %p - %s(%s)", backtraceContext->currentDepth, computedPc, dlInfo.dli_sname, dlInfo.dli_fname);
            printf("\n");
        }
        if (backtraceContext->currentDepth++ < backtraceContext->maxDepth) {
            return _URC_NO_REASON;
        }
        return _URC_END_OF_STACK;
    },
                      &backtraceContext);
#else
    void* trace[128];
    char** messages = (char**)NULL;
    int i, traceSize = 0;
    traceSize = backtrace(trace, 128);

    messages = backtrace_symbols(trace, traceSize);
    /* skip first stack frame (points here) */
    for (i = 1; i < traceSize; ++i) {
        printf("[bt] #%d %s\n", i, messages[i]);

        char syscom[256];
        std::string temp = messages[i];
        auto moduleEnd = temp.find("(");
        auto addrStart = temp.find("+");
        auto addrEnd = temp.find(")");
        if (moduleEnd != std::string::npos && addrStart != std::string::npos && addrEnd != std::string::npos) {
            std::string modulePath = temp.substr(0, moduleEnd);
            std::string addr = temp.substr(addrStart, addrEnd - addrStart);
            sprintf(syscom, "addr2line %s -e %s", addr.c_str(),
                    modulePath.c_str());
            system(syscom);
        }
    }
#endif

    fflush(stdout);
    fflush(stderr);

    // this is the trick: it will trigger the core dump
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
}
#endif

#include <GCUtil.h>
void doFullGCWithoutSeeingStack()
{
    GC_register_mark_stack_func([]() {
        // do nothing for skip stack
        // assume there is no gc-object on stack
    });
    GC_gcollect();
    GC_gcollect();
    GC_gcollect_and_unmap();
    GC_register_mark_stack_func(nullptr);
}

void printEveryReachableGCObjects()
{
    printf("print reachable pointers -->\n");
    GC_gcollect();
    GC_disable();
    size_t totalRemainSize = 0;
    GC_enumerate_reachable_objects_inner(
        [](void* obj, size_t bytes, void* cd) {
            size_t size;
            int kind = GC_get_kind_and_size(obj, &size);
#if defined(NDEBUG)
            void* ptr = obj;
#else
            void* ptr = GC_USR_PTR_FROM_BASE(obj);
#endif
            size_t* totalSize = (size_t*)cd;
            *totalSize += size;
            printf("@@@ kind %d pointer %p size %d\n", (int)kind, ptr, (int)size);
#if !defined(NDEBUG) && !defined(_WINDOWS)
            GC_print_backtrace(ptr);
#endif
        },
        &totalRemainSize);
    GC_enable();
    printf("<-- end of print reachable pointers %fKB\n", totalRemainSize / 1024.f);
}

// <---- these header & function above are used for Escargot internal development
#endif

using namespace Escargot;

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
                for (size_t i = 0; i < readLen; i++) {
                    unsigned char ch = buf[i];
                    if (ch & 0x80) {
                        // check non-latin1 character
                        hasNonLatin1Content = true;
                        fseek(fp, 0, SEEK_SET);
                        break;
                    }
                    str += ch;
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
        if (state) {
            const size_t maxNameLength = 990;
            if ((strnlen(builtinName, maxNameLength) + strnlen(fileName, maxNameLength)) < maxNameLength) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "GlobalObject.%s: cannot open file %s", builtinName, fileName);
                state->throwException(URIErrorObjectRef::create(state.get(), StringRef::createFromUTF8(msg, strnlen(msg, sizeof msg))));
            } else {
                state->throwException(URIErrorObjectRef::create(state.get(), StringRef::createFromASCII("invalid file name")));
            }
        } else {
            puts(fileName);
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
                            fprintf(stderr, "Uncaught %s: in agent\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // sleep 10ms
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
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(d);
    return ValueRef::create((uint64_t)s.count() * 1000UL + std::chrono::duration_cast<std::chrono::microseconds>(d - s).count() / 1000UL);
}

static ValueRef* builtin262AgentSleep(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructCall)
{
    double m = argv[0]->toNumber(state);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(m)));
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
        bool isAbs = false;
        if (loadedModule->src()->length() && loadedModule->src()->charAt(0) == '/') {
            isAbs = true;
        } else if (loadedModule->src()->length() > 3 && loadedModule->src()->charAt(1) == ':' && loadedModule->src()->charAt(2) == '\\') {
            isAbs = true;
        }

        if (referrer && loadedModule->src()->length() && !isAbs) {
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
        fprintf(stderr, "Script parsing error: ");
        switch (scriptInitializeResult.parseErrorCode) {
        case Escargot::ErrorObjectRef::Code::SyntaxError:
            fprintf(stderr, "SyntaxError");
            break;
        case Escargot::ErrorObjectRef::Code::EvalError:
            fprintf(stderr, "EvalError");
            break;
        case Escargot::ErrorObjectRef::Code::RangeError:
            fprintf(stderr, "RangeError");
            break;
        case Escargot::ErrorObjectRef::Code::ReferenceError:
            fprintf(stderr, "ReferenceError");
            break;
        case Escargot::ErrorObjectRef::Code::TypeError:
            fprintf(stderr, "TypeError");
            break;
        case Escargot::ErrorObjectRef::Code::URIError:
            fprintf(stderr, "URIError");
            break;
        default:
            break;
        }
        fprintf(stderr, ": %s\n", scriptInitializeResult.parseErrorMessage->toStdUTF8String().data());
        return false;
    }

    auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
        return script->execute(state);
    },
                                         scriptInitializeResult.script.get());

    if (!evalResult.isSuccessful()) {
        fprintf(stderr, "Uncaught %s:\n", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
        for (size_t i = 0; i < evalResult.stackTrace.size(); i++) {
            fprintf(stderr, "%s (%d:%d)\n", evalResult.stackTrace[i].srcName->toStdUTF8String().data(), (int)evalResult.stackTrace[i].loc.line, (int)evalResult.stackTrace[i].loc.column);
        }
        return false;
    }

    if (shouldPrintScriptResult) {
        puts(evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
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
                    fprintf(stderr, "Uncaught %s:\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                    result = false;
                } else {
                    fprintf(stderr, "%s\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                }
            }
        }
    }
    return result;
}

// making this function with lambda causes "cannot compile this forwarded non-trivially copyable parameter yet" on Windows/ClangCL
static void globalObjectProxyCallback(ExecutionStateRef* state, GlobalObjectProxyObjectRef* proxy, GlobalObjectRef* targetGlobalObject, GlobalObjectProxyObjectRef::AccessOperationType operationType, OptionalRef<AtomicStringRef> nonIndexedStringPropertyNameIfExists)
{
    // TODO check security
}

PersistentRefHolder<ContextRef> createEscargotContext(VMInstanceRef* instance, bool isMainThread)
{
    PersistentRefHolder<ContextRef> context = ContextRef::create(instance);

    Evaluator::execute(context, [](ExecutionStateRef* state, bool isMainThread) -> ValueRef* {
        ContextRef* context = state->context();

        GlobalObjectProxyObjectRef* proxy = GlobalObjectProxyObjectRef::create(state, state->context()->globalObject(), globalObjectProxyCallback);
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

#if defined(ESCARGOT_ENABLE_TEST)
        // There is no specific standard for the [@@toStringTag] property of global object.
        // But "global" string is added here to pass legacy TCs
        context->globalObject()->defineDataProperty(state, context->vmInstance()->toStringTagSymbol(), ObjectRef::DataPropertyDescriptor(AtomicStringRef::create(context, "global")->string(), (ObjectRef::PresentAttribute)(ObjectRef::NonWritablePresent | ObjectRef::NonEnumerablePresent | ObjectRef::ConfigurablePresent)));

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "uneval"), builtinUneval, 1, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("uneval"), buildFunctionObjectRef, true, true, true);
        }

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "drainJobQueue"), builtinDrainJobQueue, 0, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("drainJobQueue"), buildFunctionObjectRef, true, true, true);
        }

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "addPromiseReactions"), builtinAddPromiseReactions, 3, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("addPromiseReactions"), buildFunctionObjectRef, true, true, true);
        }

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "createNewGlobalObject"), builtinCreateNewGlobalObject, 0, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("createNewGlobalObject"), buildFunctionObjectRef, true, true, true);
        }

        {
            FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "newGlobal"), builtinCreateNewGlobalObject, 0, true, false);
            FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("newGlobal"), buildFunctionObjectRef, true, true, true);
        }

        // https://github.com/tc39/test262/blob/master/INTERPRETING.md
        {
            ObjectRef* dollor262Object = ObjectRef::create(state);

            {
                FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "gc"), builtinGc, 0, true, false);
                FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("gc"), buildFunctionObjectRef, true, true, true);
            }

            {
                FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "createRealm"), builtin262CreateRealm, 0, true, false);
                FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("createRealm"), buildFunctionObjectRef, true, true, true);
            }

            {
                FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "detachArrayBuffer"), builtin262DetachArrayBuffer, 1, true, false);
                FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("detachArrayBuffer"), buildFunctionObjectRef, true, true, true);
            }

            {
                FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "evalScript"), builtin262EvalScript, 1, true, false);
                FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("evalScript"), buildFunctionObjectRef, true, true, true);
            }

            {
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("global"), context->globalObjectProxy(), true, true, true);
            }

            {
                FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "IsHTMLDDA"), builtin262IsHTMLDDA, 0, true, false);
                FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                buildFunctionObjectRef->setIsHTMLDDA();
                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("IsHTMLDDA"), buildFunctionObjectRef, true, true, true);
            }

            if (Globals::supportsThreading()) {
                ObjectRef* agentObject = ObjectRef::create(state);
                if (isMainThread) {
                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "start"), builtin262AgentStart, 1, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("start"), buildFunctionObjectRef, true, true, true);
                    }

                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "broadcast"), builtin262AgentBroadcast, 2, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("broadcast"), buildFunctionObjectRef, true, true, true);
                    }

                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "getReport"), builtin262AgentGetReport, 0, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("getReport"), buildFunctionObjectRef, true, true, true);
                    }
                } else {
                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "leaving"), builtin262AgentLeaving, 0, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("leaving"), buildFunctionObjectRef, true, true, true);
                    }

                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "receiveBroadcast"), builtin262AgentReceiveBroadcast, 1, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("receiveBroadcast"), buildFunctionObjectRef, true, true, true);
                    }

                    {
                        FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "report"), builtin262AgentReport, 1, true, false);
                        FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                        agentObject->defineDataProperty(state, StringRef::createFromASCII("report"), buildFunctionObjectRef, true, true, true);
                    }
                }

                {
                    FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "monotonicNow"), builtin262AgentMonotonicNow, 0, true, false);
                    FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                    agentObject->defineDataProperty(state, StringRef::createFromASCII("monotonicNow"), buildFunctionObjectRef, true, true, true);
                }

                {
                    FunctionObjectRef::NativeFunctionInfo nativeFunctionInfo(AtomicStringRef::create(context, "sleep"), builtin262AgentSleep, 1, true, false);
                    FunctionObjectRef* buildFunctionObjectRef = FunctionObjectRef::create(state, nativeFunctionInfo);
                    agentObject->defineDataProperty(state, StringRef::createFromASCII("sleep"), buildFunctionObjectRef, true, true, true);
                }

                dollor262Object->defineDataProperty(state, StringRef::createFromASCII("agent"), agentObject, true, false, true);
            }

            context->globalObject()->defineDataProperty(state, StringRef::createFromASCII("$262"), dollor262Object, true, false, true);
        }
#endif
        return ValueRef::createUndefined();
    },
                       isMainThread);

    return context;
}

#if defined(_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <windows.h> // for SetConsoleOutputCP
#endif

int main(int argc, char* argv[])
{
#if defined(_WINDOWS) || defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(65001);
#endif
#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

#if defined(ESCARGOT_ENABLE_TEST) && !defined(__APPLE__) && !defined(_WINDOWS) && !defined(_WIN32) && !defined(_WIN64)
    struct sigaction sa;
    sa.sa_handler = (void (*)(int))btSighandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#endif

#ifdef M_MMAP_THRESHOLD
    mallopt(M_MMAP_THRESHOLD, 2048);
#endif
#ifdef M_MMAP_MAX
    mallopt(M_MMAP_MAX, 1024 * 1024);
#endif

#if defined(ESCARGOT_GOOGLE_PERF)
    ProfilerStart("gperf_result");
#endif

    bool waitBeforeExit = false;

    ShellPlatform* platform = new ShellPlatform();
    Globals::initialize(platform);

    Memory::setGCFrequency(24);

    PersistentRefHolder<VMInstanceRef> instance = VMInstanceRef::create();
    instance->registerPromiseRejectCallback([](ExecutionStateRef* state, PromiseObjectRef* promise, ValueRef* value, VMInstanceRef::PromiseRejectEvent event) {
        if (event == VMInstanceRef::PromiseRejectWithNoHandler) {
            fprintf(stderr, "Unhandled promise reject %s:\n", value->toStringWithoutException(state->context())->toStdUTF8String().data());
        }
    });
    PersistentRefHolder<ContextRef> context = createEscargotContext(instance.get());

    if (getenv("GC_FREE_SPACE_DIVISOR") && strlen(getenv("GC_FREE_SPACE_DIVISOR"))) {
        int d = atoi(getenv("GC_FREE_SPACE_DIVISOR"));
        Memory::setGCFrequency(d);
    }

    bool runShell = true;
    bool seenModule = false;
    std::string fileName;

    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) >= 2 && argv[i][0] == '-') { // parse command line option
            if (argv[i][1] == '-') { // `--option` case
                if (strcmp(argv[i], "--shell") == 0) {
                    runShell = true;
                    continue;
                }
                if (strcmp(argv[i], "--module") == 0) {
                    seenModule = true;
                    continue;
                }
                if (strcmp(argv[i], "--canblock-is-false") == 0) {
                    platform->setCanBlock(false);
                    continue;
                }
                if (strstr(argv[i], "--filename-as=") == argv[i]) {
                    fileName = argv[i] + sizeof("--filename-as=") - 1;
                    continue;
                }
                if (strcmp(argv[i], "--start-debug-server") == 0) {
                    context->initDebuggerRemote(nullptr);
                    continue;
                }
                if (strcmp(argv[i], "--debugger-wait-source") == 0) {
                    StringRef* sourceName;
                    while (true) {
                        StringRef* clientSourceRef = context->getClientSource(&sourceName);
                        if (!clientSourceRef) {
                            break;
                        }
                        if (!evalScript(context, clientSourceRef, sourceName, false, false))
                            return 3;
                        runShell = false;
                    }
                    continue;
                }
                if (strcmp(argv[i], "--wait-before-exit") == 0) {
                    waitBeforeExit = true;
                    continue;
                }
            } else { // `-option` case
                if (strcmp(argv[i], "-e") == 0) {
                    runShell = false;
                    i++;
                    StringRef* src = StringRef::createFromUTF8(argv[i], strlen(argv[i]));
                    if (!evalScript(context, src, StringRef::createFromASCII("shell input"), false, false))
                        return 3;
                    continue;
                }
                if (strcmp(argv[i], "-f") == 0) {
                    continue;
                }
            }
            fprintf(stderr, "Cannot recognize option `%s`", argv[i]);
            // return 3;
            continue;
        }

        FILE* fp = fopen(argv[i], "r");
        if (fp) {
            fclose(fp);
            runShell = false;

            StringRef* src = Evaluator::execute(context, [](ExecutionStateRef* state, char* c) -> ValueRef* {
                                 return builtinHelperFileRead(state, c, "read").get();
                             },
                                                argv[i])
                                 .result->asString();

            if (fileName.length() == 0) {
                fileName = argv[i];
            }

            if (!evalScript(context, src, StringRef::createFromUTF8(fileName.data(), fileName.length()), false, seenModule)) {
                return 3;
            }
            seenModule = false;
            fileName.clear();
        } else {
            runShell = false;
            printf("Cannot open file %s\n", argv[i]);
            return 3;
        }
    }

    if (runShell && !context->isDebuggerRunning()) {
        printf("escargot version:%s, %s%s\n", Globals::version(), Globals::buildDate(), Globals::supportsThreading() ? "(supports threading)" : "");
    }

    if (waitBeforeExit || context->isWaitBeforeExit()) {
        auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
            return script->execute(state);
        },
                                             context->scriptParser()->initializeScript(StringRef::createFromASCII("/* Escargot is about to terminate.\n   Global values can be inspected before exit. */"), StringRef::createFromASCII("<ScriptEnd>"), seenModule).script.get());
    }

    while (runShell) {
        static char buf[2048];
        printf("escargot> ");
        if (!fgets(buf, sizeof buf, stdin)) {
            printf("ERROR: Cannot read interactive shell input\n");
            return 3;
        }
        StringRef* str = Escargot::StringRef::createFromUTF8(buf, strlen(buf));
        evalScript(context, str, StringRef::emptyString(), true, false);
    }

#if defined(ESCARGOT_ENABLE_TEST)
    while (true) {
        bool everyThreadIsEnded = true;
        {
            std::lock_guard<std::mutex> guard(workerMutex);
            for (auto& i : workerThreads) {
                if (!i.second.ended) {
                    everyThreadIsEnded = false;
                    break;
                }
            }
        }
        if (everyThreadIsEnded) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& i : workerThreads) {
        i.first.join();
    }
#endif

    context.release();
    instance.release();

    Globals::finalize();

#if defined(ESCARGOT_GOOGLE_PERF)
    ProfilerStop();
#endif

    return 0;
}
