#include "Escargot.h"

#include "LeakChecker.h"
#include "heap/Allocator.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

#ifdef PROFILE_BDWGC

std::string HeapUsageVisualizer::m_outputFile = "bdwgcUsage.dat";

size_t GCLeakChecker::m_totalFreed = 0;
std::vector<GCLeakChecker::LeakCheckedAddr> GCLeakChecker::m_leakCheckedAddrs;

static std::string s_gcLogPhaseName = "initial phase";

void HeapUsageVisualizer::initialize()
{
    remove(HeapUsageVisualizer::m_outputFile.c_str());
    FILE* fp = fopen(HeapUsageVisualizer::m_outputFile.c_str(), "a");
    if (fp) {
        fprintf(fp, "GC_no    PeakRSS   TotalHeap    Marked  # Phase\n");
        fclose(fp);
    }
    GC_set_on_collection_event([](GC_EventType evtType) {
        if (GC_EVENT_RECLAIM_START == evtType) {
            GC_dump_for_graph(HeapUsageVisualizer::m_outputFile.c_str(),
                              s_gcLogPhaseName.c_str());
        }
    });
}


void GCLeakChecker::registerAddress(void* ptr, std::string description)
{
    RELEASE_ASSERT(ptr);
    m_leakCheckedAddrs.push_back(LeakCheckedAddr{ (void*)((size_t)ptr + 1), description, false });

    printf("GCLeakChecker::registerAddress %p (%zu - %zu = %zu)\n", ptr,
           m_leakCheckedAddrs.size(), m_totalFreed, m_leakCheckedAddrs.size() - m_totalFreed);

    GC_REGISTER_FINALIZER_NO_ORDER(ptr, [](void* obj, void* cd) {
        GCLeakChecker::unregisterAddress(obj);
    },
                                   NULL, NULL, NULL);
}

void GCLeakChecker::unregisterAddress(void* ptr)
{
    RELEASE_ASSERT(ptr);
    m_totalFreed++;

    printf("GCLeakChecker::unregisterAddress %p (%zu - %zu = %zu)\n", ptr,
           m_leakCheckedAddrs.size(), m_totalFreed, m_leakCheckedAddrs.size() - m_totalFreed);

    for (auto& it : m_leakCheckedAddrs) {
        if (it.ptr == (void*)((size_t)ptr + 1)) {
            it.deallocated = true;
            return;
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void GCLeakChecker::dumpBackTrace(ExecutionState& state, const char* phase)
{
    if (phase)
        s_gcLogPhaseName = phase;

#ifdef GC_DEBUG
    auto stream = stderr;
    fprintf(stderr, "GCLeakChecker::dumpBackTrace %s start >>>>>>>>>>\n", s_gcLogPhaseName.c_str());
    GC_gcollect();
    for (const auto& it : m_leakCheckedAddrs) {
        if (it.deallocated) {
            fprintf(stderr, "%s (%p) deallocated\n", it.description.c_str(), (void*)((size_t)it.ptr - 1));
        } else {
            fprintf(stderr, "Backtrace of %s (%p):\n", it.description.c_str(), (void*)((size_t)it.ptr - 1));
            GC_print_backtrace((void*)((size_t)it.ptr - 1));
        }
    }
    fprintf(stderr, "GCLeakChecker::dumpBackTrace %s end <<<<<<<<<<\n", s_gcLogPhaseName.c_str());
#else
    fprintf(stderr, "Cannot print the backtrace of leaked address.\n");
    fprintf(stderr, "Please re-configure bdwgc with `--enable-gc-debug`, ");
    fprintf(stderr, "and re-build escargot with `-DGC_DEBUG`.\n");
    for (const auto& it : m_leakCheckedAddrs) {
        if (it.deallocated) {
            fprintf(stderr, "%s (%p) deallocated\n", it.description.c_str(), (void*)((size_t)it.ptr - 1));
        } else {
            fprintf(stderr, "%s (%p) still allocated\n", it.description.c_str(), (void*)((size_t)it.ptr - 1));
        }
    }
#endif
}

/* Usage in JS: registerLeakCheck( object: PointerValue, description: String ); */
Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isPointerValue())
        state.throwException(new ErrorObject(state, new ASCIIString("builtinRegisterLeakCheck should get pointer-type argument")));

    PointerValue* ptr = argv[0].asPointerValue();
    std::string description = argv[1].toString(state)->toUTF8StringData().data();
    RELEASE_ASSERT(ptr);

    GCLeakChecker::registerAddress(ptr, description);

    return Value();
}

/* Usage in JS: dumpBackTrace( phase: String ); */
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GCLeakChecker::dumpBackTrace(state, argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}

/* Usage in JS: setPhaseName( phase: String ); */
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    s_gcLogPhaseName = argv[0].toString(state)->toUTF8StringData().data();
    return Value();
}

#endif // PROFILE_BDWGC
}
