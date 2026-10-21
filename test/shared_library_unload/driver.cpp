// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

// This host deliberately links neither Escargot nor the embedding DSO.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <set>
#include <signal.h>
#include <string>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

template <typename T>
static T symbol(void* handle, const char* name)
{
    auto fn = reinterpret_cast<T>(dlsym(handle, name));
    require(fn, name);
    return fn;
}

static bool mapped(uintptr_t address)
{
    FILE* maps = std::fopen("/proc/self/maps", "r");
    require(maps, "open process mappings");
    char line[4096];
    bool found = false;
    while (std::fgets(line, sizeof(line), maps)) {
        unsigned long start, end;
        if (std::sscanf(line, "%lx-%lx", &start, &end) == 2 && address >= start && address < end) {
            found = true;
        }
    }
    std::fclose(maps);
    return found;
}

static std::set<std::string> entries(const char* path)
{
    DIR* dir = opendir(path);
    require(dir, "open resource directory");
    std::set<std::string> result;
    while (auto* entry = readdir(dir)) {
        if (entry->d_name[0] != '.' && (std::strcmp(path, "/proc/self/fd") != 0 || std::atoi(entry->d_name) != dirfd(dir))) {
            result.insert(entry->d_name);
        }
    }
    closedir(dir);
    return result;
}

template <typename F>
static void requireAbort(F fn)
{
    pid_t child = fork();
    require(child >= 0, "fork misuse test");
    if (!child) {
        struct rlimit limit = { 0, 0 };
        setrlimit(RLIMIT_CORE, &limit);
        std::freopen("/dev/null", "w", stderr);
        fn();
        _exit(0);
    }
    int status;
    require(waitpid(child, &status, 0) == child, "wait for misuse test");
    require(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "invalid lifetime operation was accepted");
}

static pthread_key_t nextKey()
{
    pthread_key_t key;
    require(pthread_key_create(&key, nullptr) == 0, "probe native TLS keys");
    require(pthread_key_delete(key) == 0, "release probe key");
    return key;
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    require(argc == 2, "expected embedding shared library path");
    const long tid = syscall(SYS_gettid);
    const auto initialThreads = entries("/proc/self/task");
    const auto initialFDs = entries("/proc/self/fd");
    const auto initialKey = nextKey();
    struct sigaction initialSegv, initialBus;
    require(sigaction(SIGSEGV, nullptr, &initialSegv) == 0, "read SIGSEGV handler");
    require(sigaction(SIGBUS, nullptr, &initialBus) == 0, "read SIGBUS handler");

    for (int cycle = 0; cycle < 40; ++cycle) {
        void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!library) {
            std::fprintf(stderr, "dlopen: %s\n", dlerror());
            return 1;
        }
        auto initialize = symbol<void (*)(unsigned)>(library, "unloadTestInitialize");
        auto finalize = symbol<void (*)()>(library, "unloadTestFinalize");
        auto destroy = symbol<void (*)()>(library, "unloadTestDestroyRuntime");
        auto exercise = symbol<bool (*)()>(library, "unloadTestExercise");
        auto initialized = symbol<bool (*)()>(library, "unloadTestIsInitialized");
        auto supportsThreading = symbol<bool (*)()>(library, "unloadTestSupportsThreading");
        auto initializeThread = symbol<void (*)()>(library, "unloadTestInitializeThread");
        auto finalizeThread = symbol<void (*)()>(library, "unloadTestFinalizeThread");
        auto swapGlobal = symbol<unsigned (*)(unsigned)>(library, "unloadTestSwapGlobal");
        auto swapTLS = symbol<unsigned (*)(unsigned)>(library, "unloadTestSwapTLS");
        auto runtimeAddress = symbol<void* (*)()>(library, "unloadTestRuntimeAddress");
        uintptr_t engineCode = reinterpret_cast<uintptr_t>(runtimeAddress());
        uintptr_t embeddingCode = reinterpret_cast<uintptr_t>(initialize);
        require(mapped(engineCode) && mapped(embeddingCode), "runtime code not mapped");
        require(!initialized(), "initialization flag survived unload");
        require(swapGlobal(41) == 0 && swapTLS(43) == 0, "embedding static/TLS survived unload");

        // Normal finalize keeps descriptor caches usable in the same mapping.
        for (int restart = 0; restart < 2; ++restart) {
            initialize(cycle % 2); // Alternate normal and incremental GC.
            require(initialized() && exercise(), "JavaScript failed on the host thread");
            if (!cycle && !restart) {
                requireAbort(destroy);
            }
            if (supportsThreading()) {
                bool workerSucceeded = false;
                std::thread worker([&] {
                    initializeThread();
                    workerSucceeded = exercise();
                    finalizeThread();
                });
                worker.join();
                require(workerSucceeded, "JavaScript failed on an auxiliary thread");
            }
            finalize();
            require(!initialized(), "finalize left the engine initialized");
        }

        require(swapGlobal(41) == 41 && swapTLS(43) == 43, "ordinary finalize reset embedding static/TLS");
        if (!cycle && supportsThreading()) {
            initialize(0);
            std::atomic<bool> finalized{ false }, mayExit{ false };
            std::thread worker([&] {
                initializeThread();
                finalizeThread();
                finalized = true;
                while (!mayExit) {
                    std::this_thread::yield();
                }
            });
            while (!finalized) {
                std::this_thread::yield();
            }
            finalize();
            // finalizeThread() alone keeps the foreign thread's GC runtime.
            requireAbort(destroy);
            mayExit = true;
            worker.join();
        }
        destroy();
        require(!initialized(), "runtime destruction initialized the engine");
        if (!cycle) {
            requireAbort([&] { initialize(0); });
            requireAbort(initializeThread);
            requireAbort(destroy);
        }
        require(dlclose(library) == 0, "close embedding shared library");
        require(!mapped(embeddingCode), "embedding shared library remained mapped");
        require(!mapped(engineCode), "Escargot remained mapped after embedding DSO unload");
        require(syscall(SYS_gettid) == tid, "host thread changed");
        require(entries("/proc/self/task") == initialThreads, "unload leaked a thread");
        auto finalFDs = entries("/proc/self/fd");
        if (finalFDs != initialFDs) {
            for (const auto& fd : finalFDs) {
                if (!initialFDs.count(fd)) {
                    char target[4096];
                    ssize_t length = readlink(("/proc/self/fd/" + fd).c_str(), target, sizeof(target) - 1);
                    if (length >= 0) {
                        target[length] = 0;
                        std::fprintf(stderr, "Leaked fd %s: %s\n", fd.c_str(), target);
                    }
                }
            }
        }
        require(finalFDs == initialFDs, "unload leaked a file descriptor");
        require(nextKey() == initialKey, "unload leaked a native TLS key");
        struct sigaction segv, bus;
        require(sigaction(SIGSEGV, nullptr, &segv) == 0 && sigaction(SIGBUS, nullptr, &bus) == 0, "read signal handlers after unload");
        require(segv.sa_sigaction == initialSegv.sa_sigaction && bus.sa_sigaction == initialBus.sa_sigaction, "GC signal handler survived unload");
        std::printf("PASS cycle %d: tid=%ld, JS and normal restart succeeded, embedding and runtime unmapped\n", cycle + 1, tid);
    }
}
