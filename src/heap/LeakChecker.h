#ifndef __LeakChecker__
#define __LeakChecker__

//#define PROFILE_BDWGC
#ifdef PROFILE_BDWGC

#include "runtime/Value.h"

namespace Escargot {

class ExecutionState;

class HeapUsageVisualizer {
public:
    static void initialize();

private:
    static std::string m_outputFile;
};

class GCLeakChecker {
public:
    static void registerAddress(void* ptr, std::string description);
    static void dumpBackTrace(ExecutionState& state, const char* phase = NULL);

private:
    static void unregisterAddress(void* ptr);

    struct LeakCheckedAddr {
        void* ptr;
        std::string description;
        bool deallocated;
    };
    static size_t m_totalFreed;
    static std::vector<LeakCheckedAddr> m_leakCheckedAddrs;
};

Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
}

#endif // PROFILE_BDWGC

#endif // __LeakChecker__
