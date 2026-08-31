/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "runtime/ThreadLocal.h"
#include "heap/Heap.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/String.h"
#include "runtime/Value.h"
#include "parser/ASTAllocator.h"
#include "api/EscargotPublic.h"
#include "BumpPointerAllocator.h"
#include <gc/gc_mark.h>
#if defined(ENABLE_WASM)
#include "wasm.h"
#endif

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <processthreadsapi.h>
#ifndef _WIN32_WINNT
#define 0x0602
#endif
#elif defined(OS_BAREMETAL)
// stack base is obtained from the embedder's Platform (see
// PlatformRef::stackBase() in src/api/EscargotPublic.h) below, not from a
// free-standing extern "C" function.
#else
#include <pthread.h>
#endif

#if defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
#include <unistd.h> // getpagesize
#if defined(__BIONIC__)
#include <android/api-level.h>
#endif
#endif

// we needs to define new macro here since wtfbridge redefine original macro
#define ESCARGOT_RELEASE_ASSERT(assertion)                                         \
    do {                                                                           \
        if (!(assertion)) {                                                        \
            ESCARGOT_LOG_ERROR("RELEASE_ASSERT at %s (%d)\n", __FILE__, __LINE__); \
            abort();                                                               \
        }                                                                          \
    } while (0);

namespace Escargot {

MAY_THREAD_LOCAL bool ThreadLocal::inited;

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
size_t ThreadLocal::g_stackLimitTlsOffset;

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
size_t ThreadLocal::g_emptyStringTlsOffset;
#endif
size_t ThreadLocal::g_gcEpochTlsOffset;

#elif defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
ptrdiff_t ThreadLocal::g_stackLimitKeyOffset;
pthread_key_t ThreadLocal::g_stackLimitKey;

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
ptrdiff_t ThreadLocal::g_emptyStringKeyOffset;
pthread_key_t ThreadLocal::g_emptyStringKey;
#endif
ptrdiff_t ThreadLocal::g_gcEpochKeyOffset;
pthread_key_t ThreadLocal::g_gcEpochKey;
#endif

#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
static ASCIIStringFromExternalMemory g_emptyString("");
#endif

MAY_THREAD_LOCAL size_t ThreadLocal::g_stackLimit;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
MAY_THREAD_LOCAL String* ThreadLocal::g_emptyStringInstance;
#else
String* ThreadLocal::g_emptyStringInstance;
#endif

MAY_THREAD_LOCAL std::mt19937* ThreadLocal::g_randEngine;
MAY_THREAD_LOCAL bf_context_t ThreadLocal::g_bfContext;
#if defined(ENABLE_WASM)
MAY_THREAD_LOCAL WASMContext ThreadLocal::g_wasmContext;
#endif
MAY_THREAD_LOCAL GCEventListenerSet* ThreadLocal::g_gcEventListenerSet;
static MAY_THREAD_LOCAL GC_on_mark_stack_empty_proc g_previousMarkStackEmptyListener;
MAY_THREAD_LOCAL ASTAllocator* ThreadLocal::g_astAllocator;
MAY_THREAD_LOCAL WTF::BumpPointerAllocator* ThreadLocal::g_bumpPointerAllocator;
#if defined(ENABLE_TCO)
MAY_THREAD_LOCAL Value* ThreadLocal::g_tcoBuffer;
#endif
MAY_THREAD_LOCAL void* ThreadLocal::g_customData;
MAY_THREAD_LOCAL int ThreadLocal::g_pruningCompiledByteCodesVMCount = 0;
MAY_THREAD_LOCAL size_t ThreadLocal::g_gcEpoch = 0;

#if defined(ENABLE_THREADING)
class GlobalDeleteChecker {
public:
    GlobalDeleteChecker();
    ~GlobalDeleteChecker();
    // FIXME: using std::vector on { darwin, android } cause error
    size_t m_mappedMemoriesAllocatedSize = 8;
    size_t m_mappedMemoriesSize = 0;
    std::pair<void*, size_t>* m_mappedMemories;
};

#if defined(OS_BAREMETAL)
static std::unique_ptr<GlobalDeleteChecker> g_globalDeleteChecker;
#else
thread_local std::unique_ptr<GlobalDeleteChecker> g_globalDeleteChecker;
#endif

GlobalDeleteChecker::GlobalDeleteChecker()
{
    m_mappedMemories = reinterpret_cast<std::pair<void*, size_t>*>(malloc(sizeof(std::pair<void*, size_t>) * m_mappedMemoriesAllocatedSize));
    GC_set_on_os_get_mem([](void* ptr, size_t length) {
        if (g_globalDeleteChecker->m_mappedMemoriesSize == g_globalDeleteChecker->m_mappedMemoriesAllocatedSize) {
            std::pair<void*, size_t>* newBuf = reinterpret_cast<std::pair<void*, size_t>*>(
                malloc(sizeof(std::pair<void*, size_t>) * g_globalDeleteChecker->m_mappedMemoriesSize * 2));
            memcpy(reinterpret_cast<void*>(newBuf), g_globalDeleteChecker->m_mappedMemories, sizeof(std::pair<void*, size_t>) * g_globalDeleteChecker->m_mappedMemoriesSize);
            g_globalDeleteChecker->m_mappedMemoriesAllocatedSize *= 2;
            free(g_globalDeleteChecker->m_mappedMemories);
            g_globalDeleteChecker->m_mappedMemories = newBuf;
        }
        g_globalDeleteChecker->m_mappedMemories[g_globalDeleteChecker->m_mappedMemoriesSize++] = std::make_pair(ptr, length);
    });
}

GlobalDeleteChecker::~GlobalDeleteChecker()
{
    // call GC_deinit for releasing bdwgc's vdb resource
    GC_deinit();

    for (size_t i = 0; i < m_mappedMemoriesSize; i++) {
        auto e = m_mappedMemories[i];
#if defined(OS_WINDOWS)
        VirtualFree(e.first, 0, MEM_RELEASE);
#else
        munmap(e.first, e.second);
#endif
    }

    free(m_mappedMemories);
}
#endif

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureMarkStartListeners()
{
    if (!m_markStartListeners) {
        m_markStartListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_markStartListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureMarkEndListeners()
{
    if (!m_markEndListeners) {
        m_markEndListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_markEndListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureReclaimStartListeners()
{
    if (!m_reclaimStartListeners) {
        m_reclaimStartListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_reclaimStartListeners.value();
}

GCEventListenerSet::EventListenerVector* GCEventListenerSet::ensureReclaimEndListeners()
{
    if (!m_reclaimEndListeners) {
        m_reclaimEndListeners = new GCEventListenerSet::EventListenerVector();
    }
    return m_reclaimEndListeners.value();
}

GCEventListenerSet::MarkStackEmptyListenerVector* GCEventListenerSet::ensureMarkStackEmptyListeners()
{
    if (!m_markStackEmptyListeners) {
        m_markStackEmptyListeners = new GCEventListenerSet::MarkStackEmptyListenerVector();
    }
    return m_markStackEmptyListeners.value();
}

bool GCEventListenerSet::hasMarkStackEmptyListener(OnMarkStackEmptyListener listener, void* data) const
{
    if (!m_markStackEmptyListeners) {
        return false;
    }
    auto& listeners = *m_markStackEmptyListeners.value();
    return std::find(listeners.begin(), listeners.end(), std::make_pair(listener, data)) != listeners.end();
}

bool GCEventListenerSet::addMarkStackEmptyListener(OnMarkStackEmptyListener listener, void* data)
{
    if (hasMarkStackEmptyListener(listener, data)) {
        return false;
    }
    ensureMarkStackEmptyListeners()->push_back(std::make_pair(listener, data));
    return true;
}

bool GCEventListenerSet::removeMarkStackEmptyListener(OnMarkStackEmptyListener listener, void* data)
{
    if (!m_markStackEmptyListeners) {
        return false;
    }
    auto& listeners = *m_markStackEmptyListeners.value();
    auto iter = std::find(listeners.begin(), listeners.end(), std::make_pair(listener, data));
    if (iter == listeners.end()) {
        return false;
    }
    listeners.erase(iter);
    return true;
}

void GCEventListenerSet::reset()
{
    if (m_markStartListeners) {
        delete m_markStartListeners.value();
        m_markStartListeners.reset();
    }
    if (m_markEndListeners) {
        delete m_markEndListeners.value();
        m_markEndListeners.reset();
    }
    if (m_reclaimStartListeners) {
        delete m_reclaimStartListeners.value();
        m_reclaimStartListeners.reset();
    }
    if (m_reclaimEndListeners) {
        delete m_reclaimEndListeners.value();
        m_reclaimEndListeners.reset();
    }
    if (m_markStackEmptyListeners) {
        delete m_markStackEmptyListeners.value();
        m_markStackEmptyListeners.reset();
    }
}

static void genericGCFullGCStartCallback()
{
    if (!ThreadLocal::isInited()) {
        return;
    }
    ThreadLocal::gcEventListenerSet().markFullGC();
}

static void genericGCEventListener(GC_EventType evtType)
{
    if (!ThreadLocal::isInited()) {
        return;
    }
    GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
    Optional<GCEventListenerSet::EventListenerVector*> listeners;

    switch (evtType) {
    case GC_EVENT_MARK_START:
        listeners = list.markStartListeners();
        break;
    case GC_EVENT_MARK_END:
        listeners = list.markEndListeners();
        break;
    case GC_EVENT_RECLAIM_START:
        listeners = list.reclaimStartListeners();
        break;
    case GC_EVENT_RECLAIM_END:
        listeners = list.reclaimEndListeners();
        break;
    default:
        break;
    }

    if (listeners) {
        for (size_t i = 0; i < listeners->size(); i++) {
            listeners->at(i).first(listeners->at(i).second);
        }
    }

    if (evtType == GC_EVENT_RECLAIM_END) {
        // the full GC cycle (if any) is finished now. clearing the flag here (instead of
        // consuming it in a listener) lets every listener of this cycle observe it
        list.clearFullGCFlag();
    }
}

static GC_ms_entry* genericGCMarkStackEmptyListener(GC_ms_entry* markStackTop, GC_ms_entry* markStackLimit)
{
    if (g_previousMarkStackEmptyListener != nullptr) {
        markStackTop = g_previousMarkStackEmptyListener(markStackTop, markStackLimit);
    }
    if (!ThreadLocal::isInited()) {
        return markStackTop;
    }

    auto listeners = ThreadLocal::gcEventListenerSet().markStackEmptyListeners();
    if (listeners) {
        for (auto& listener : *listeners.value()) {
            markStackTop = listener.first(markStackTop, markStackLimit, listener.second);
        }
    }
    return markStackTop;
}

#if defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
/*
 * Locate this thread's storage slot for a pthread key as a fixed offset from
 * the thread pointer. The whole mechanism relies on that offset being identical
 * on every thread, which holds because both supported libc families place the
 * per-thread key storage at a compile-time constant displacement from the
 * thread pointer:
 *
 *   - glibc: the slot is `struct pthread.specific_1stblock[idx].data`, and
 *     `THREAD_SELF` is the thread pointer plus or minus a constant. On
 *     TLS_TCB_AT_TP targets (x86, x86_64) `struct pthread` starts at the thread
 *     pointer, so the slot is at a positive offset; on TLS_DTV_AT_TP targets
 *     (arm, aarch64, riscv) it is placed immediately below the thread pointer,
 *     so the offset is negative. Both directions are therefore searched, the
 *     more likely one first.
 *   - bionic changed layout at Android 10. Through Android 9 the key array is
 *     inline in `pthread_internal_t`, immediately above the thread pointer, so
 *     the original one-page upward scan is used. Since Android 10 the array is
 *     the first member of `bionic_tls`; its address is derived through
 *     TLS_SLOT_BIONIC_TLS, see bionicPthreadKeySlot().
 *
 * Note that the direction is a property of the libc rather than of the target
 * architecture: on aarch64 bionic keeps the slot above the thread pointer while
 * glibc keeps it below.
 */
#if defined(ESCARGOT_32)
#define ESCARGOT_TLS_KEY_MAGIC1 ((size_t)0xbeefdeadu)
#define ESCARGOT_TLS_KEY_MAGIC2 ((size_t)0xfeedfaceu)
#else
#define ESCARGOT_TLS_KEY_MAGIC1 ((size_t)0xbeefdeaddeadbeefull)
#define ESCARGOT_TLS_KEY_MAGIC2 ((size_t)0xfeedfacecafebabeull)
#endif

// glibc keeps the slot below the thread pointer on TLS_DTV_AT_TP targets
#if !defined(__BIONIC__) && (defined(CPU_ARM32) || defined(CPU_ARM64) || defined(CPU_RISCV32) || defined(CPU_RISCV64))
#define ESCARGOT_TLS_KEY_SLOT_BELOW_TP
#endif

#if defined(__BIONIC__)
// thread pointer slot holding `bionic_tls`, per bionic's tls_defines.h
#if defined(CPU_ARM32) || defined(CPU_ARM64)
#define ESCARGOT_TLS_SLOT_BIONIC_TLS (-1)
#elif defined(CPU_X86) || defined(CPU_X86_64)
#define ESCARGOT_TLS_SLOT_BIONIC_TLS 9
#elif defined(CPU_RISCV32) || defined(CPU_RISCV64)
#define ESCARGOT_TLS_SLOT_BIONIC_TLS (-9)
#endif
#endif

// tell a real key slot from a stale copy of the magic sitting in unrelated
// memory: the value was just passed to pthread_setspecific, so it may well have
// been spilled onto the stack, and only the real slot follows the key to a
// second value. MAGIC1 is put back on the way out so that rejecting a candidate
// does not disturb the caller's scan
static bool verifyPthreadKeySlot(pthread_key_t key, size_t* candidate)
{
    if (*candidate != ESCARGOT_TLS_KEY_MAGIC1) {
        return false;
    }
    pthread_setspecific(key, reinterpret_cast<void*>(ESCARGOT_TLS_KEY_MAGIC2));
    bool result = (*candidate == ESCARGOT_TLS_KEY_MAGIC2);
    pthread_setspecific(key, reinterpret_cast<void*>(ESCARGOT_TLS_KEY_MAGIC1));
    return result;
}

static Optional<size_t*> scanPthreadKeySlotUp(pthread_key_t key, char* lo, char* hi)
{
    for (size_t* p = reinterpret_cast<size_t*>(lo); p < reinterpret_cast<size_t*>(hi); p++) {
        if (verifyPthreadKeySlot(key, p)) {
            return p;
        }
    }
    return nullptr;
}

// searched downward from `hi` on purpose: the slot sits just below the thread
// pointer, while the bottom of the range is already past the start of the
// thread descriptor (the thread's own stack, or data below the initial break)
// and is the part most likely to hold a stale magic
static Optional<size_t*> scanPthreadKeySlotDown(pthread_key_t key, char* lo, char* hi)
{
    for (size_t* p = reinterpret_cast<size_t*>(hi); p > reinterpret_cast<size_t*>(lo);) {
        if (verifyPthreadKeySlot(key, --p)) {
            return p;
        }
    }
    return nullptr;
}

#if defined(ESCARGOT_TLS_SLOT_BIONIC_TLS)
// layout of bionic's `pthread_key_data_t`
struct BionicKeyData {
    size_t seq;
    void* data;
};

#define ESCARGOT_BIONIC_KEY_VALID_FLAG ((unsigned)1 << 31)

// `bionic_tls` is reachable through a thread pointer slot of a known index and
// `pthread_key_data_t key_data[]` is its first member, so the slot address
// follows from the key index without searching memory. All PTHREAD_KEYS_MAX
// slots live in that array, hence no equivalent of the glibc limit described in
// initializeTlsKeySlotOffsets(). Still verified against the magic, as the
// layout is a libc implementation detail
static Optional<size_t*> bionicPthreadKeySlot(pthread_key_t key, char* tlsBase)
{
    // Android 9 keeps key_data[] in pthread_internal_t instead. Although this
    // TLS slot already exists there, it points at an older bionic_tls whose
    // first member is not key_data[].
    int androidApiLevel = android_get_device_api_level();
    if (androidApiLevel > 0 && androidApiLevel < 29) {
        return nullptr;
    }

    char* bionicTls = reinterpret_cast<char*>(reinterpret_cast<void**>(tlsBase)[ESCARGOT_TLS_SLOT_BIONIC_TLS]);
    if (!bionicTls || (reinterpret_cast<uintptr_t>(bionicTls) & (alignof(BionicKeyData) - 1))) {
        return nullptr;
    }
    BionicKeyData* keyData = reinterpret_cast<BionicKeyData*>(bionicTls);
    size_t* candidate = reinterpret_cast<size_t*>(&keyData[(unsigned)key & ~ESCARGOT_BIONIC_KEY_VALID_FLAG].data);
    return verifyPthreadKeySlot(key, candidate) ? candidate : nullptr;
}
#endif

// returns the offset of the slot from the thread pointer, or zero if it could
// not be located (zero is not a valid slot offset for either libc, the thread
// pointer itself always holds libc's own TCB header)
static ptrdiff_t checkPthreadKey(pthread_key_t key, char* tlsBase)
{
    size_t page = static_cast<size_t>(getpagesize());
    Optional<size_t*> found;

    pthread_setspecific(key, reinterpret_cast<void*>(ESCARGOT_TLS_KEY_MAGIC1));
#if defined(ESCARGOT_TLS_SLOT_BIONIC_TLS)
    int androidApiLevel = android_get_device_api_level();
    if (androidApiLevel >= 29 || androidApiLevel < 0) {
        found = bionicPthreadKeySlot(key, tlsBase);
    } else {
        // Android 9 and earlier: key_data[] follows the TCB slots in
        // pthread_internal_t and is always within the page above TP. This is
        // the pre-Android-10 algorithm; do not probe below TP if it fails.
        found = scanPthreadKeySlotUp(key, tlsBase, tlsBase + page);
    }
#endif
#if defined(ESCARGOT_TLS_KEY_SLOT_BELOW_TP)
    if (!found) {
        found = scanPthreadKeySlotDown(key, tlsBase - page, tlsBase);
    }
    if (!found) {
        found = scanPthreadKeySlotUp(key, tlsBase, tlsBase + page);
    }
#elif !defined(ESCARGOT_TLS_SLOT_BIONIC_TLS)
    if (!found) {
        found = scanPthreadKeySlotUp(key, tlsBase, tlsBase + page);
    }
    if (!found) {
        found = scanPthreadKeySlotDown(key, tlsBase - page, tlsBase);
    }
#else
    // On Android 10+ the direct layout lookup above should succeed. Keep the
    // known-safe upward scan as a checked fallback for vendor bionic variants.
    if (!found && (androidApiLevel >= 29 || androidApiLevel < 0)) {
        found = scanPthreadKeySlotUp(key, tlsBase, tlsBase + page);
    }
#endif
    pthread_setspecific(key, nullptr);
    return found ? reinterpret_cast<char*>(found.value()) - tlsBase : 0;
}

// the offset was probed on whichever thread ran initializeTlsKeySlotOffsets();
// re-check on every other thread that it really addresses this thread's slot,
// rather than trusting that the libc layout is thread-invariant
static bool verifyPthreadKeySlotOffset(pthread_key_t key, char* tlsBase, ptrdiff_t offset)
{
    pthread_setspecific(key, reinterpret_cast<void*>(ESCARGOT_TLS_KEY_MAGIC1));
    bool result = verifyPthreadKeySlot(key, reinterpret_cast<size_t*>(tlsBase + offset));
    pthread_setspecific(key, nullptr);
    return result;
}

static pthread_once_t g_tlsKeySlotOnce = PTHREAD_ONCE_INIT;

static ptrdiff_t createAndProbeTlsKey(pthread_key_t* key, char* tlsBase)
{
    int keyCreateReturn = pthread_key_create(key, nullptr);
    ESCARGOT_RELEASE_ASSERT(keyCreateReturn == 0);

    ptrdiff_t offset = checkPthreadKey(*key, tlsBase);
    // reached when the slot could not be located within a page of the thread
    // pointer. With glibc that happens once the key index is
    // >= PTHREAD_KEY_2NDLEVEL_SIZE (32), i.e. when 32 or more keys were live at
    // the moment the key above was created: those slots live in per-thread
    // malloc'ed blocks, which no single fixed offset can address. Beware that
    // such a block may still happen to land within the searched range on this
    // particular thread, in which case the probe succeeds and yields an offset
    // that is wrong on every other thread -- which is what the per-thread
    // re-check in initialize() is there to catch. The real remedy is to keep
    // the key index low, i.e. to initialize Escargot early. bionic keeps all
    // PTHREAD_KEYS_MAX slots inline, so it has no such limit
    ESCARGOT_RELEASE_ASSERT(offset != 0);
    return offset;
}

// the offsets are process-wide values, so exactly one thread may create the
// keys and probe for them. initialize() runs once per thread, and racing
// threads would otherwise each create a key of their own, probe the
// corresponding (different) offsets and all store the result; every loser would
// then reach its stack limit through an unrelated key's slot
void ThreadLocal::initializeTlsKeySlotOffsets()
{
    char* baseAddr = tlsBaseAddress();
    g_stackLimitKeyOffset = createAndProbeTlsKey(&g_stackLimitKey, baseAddr);
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    g_emptyStringKeyOffset = createAndProbeTlsKey(&g_emptyStringKey, baseAddr);
#endif
    g_gcEpochKeyOffset = createAndProbeTlsKey(&g_gcEpochKey, baseAddr);
}
#endif

static void initGcFlags(Globals::InitializeOption optionFromGlobal)
{
#if defined(OS_POSIX)
    if (static_cast<uint32_t>(optionFromGlobal & Globals::InitializeOption::PreferIncrementalGC)) {
        setenv("GC_ENABLE_INCREMENTAL", "1", 1);
    }
#endif
}

void ThreadLocal::initialize(uint32_t optionFromGlobal)
{
    // initialize should be invoked only once in each thread
    ESCARGOT_RELEASE_ASSERT(!inited);

    initGcFlags(static_cast<Globals::InitializeOption>(optionFromGlobal));

#if defined(ENABLE_THREADING)
    if (!g_globalDeleteChecker) {
        g_globalDeleteChecker = std::unique_ptr<GlobalDeleteChecker>(new GlobalDeleteChecker());
    }
#endif
    // Heap is initialized for each thread
    Heap::initialize();

    if (!ThreadLocal::g_emptyStringInstance) {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        String* emptyStr = new (NoGC) ASCIIStringFromExternalMemory("");
#else
        String* emptyStr = &g_emptyString;
#endif
        // mark empty string as AtomicString source
        // because empty string is the default string value of empty AtomicString
        emptyStr->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)emptyStr;
        ASSERT(emptyStr->isAtomicStringSource());
        ThreadLocal::g_emptyStringInstance = emptyStr;
    }

#if defined(ENABLE_TLS_ACCESS_BY_ADDRESS)
    auto tlsBase = tlsBaseAddress();
    if (!g_stackLimitTlsOffset) {
        g_stackLimitTlsOffset = reinterpret_cast<char*>(&g_stackLimit) - tlsBase;
    } else {
        // runtime check
        size_t newDistance = reinterpret_cast<char*>(&g_stackLimit) - tlsBase;
        ESCARGOT_RELEASE_ASSERT(newDistance == g_stackLimitTlsOffset);
    }

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    if (!g_emptyStringTlsOffset) {
        g_emptyStringTlsOffset = reinterpret_cast<char*>(&g_emptyStringInstance) - tlsBase;
    } else {
        // runtime check
        size_t newDistance = reinterpret_cast<char*>(&g_emptyStringInstance) - tlsBase;
        ESCARGOT_RELEASE_ASSERT(newDistance == g_emptyStringTlsOffset);
    }
#endif

    if (!g_gcEpochTlsOffset) {
        g_gcEpochTlsOffset = reinterpret_cast<char*>(&g_gcEpoch) - tlsBase;
    } else {
        // runtime check
        size_t newDistance = reinterpret_cast<char*>(&g_gcEpoch) - tlsBase;
        ESCARGOT_RELEASE_ASSERT(newDistance == g_gcEpochTlsOffset);
    }

#elif defined(ENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
    // GC_init() (through Heap::initialize() above) probes the very same way for
    // its own key, so the allocator is usable here -- but the offsets are still
    // guarded by pthread_once instead of a lock, see
    // initializeTlsKeySlotOffsets()
    int onceReturn = pthread_once(&g_tlsKeySlotOnce, &ThreadLocal::initializeTlsKeySlotOffsets);
    ESCARGOT_RELEASE_ASSERT(onceReturn == 0);

    auto baseAddr = tlsBaseAddress();
    ESCARGOT_RELEASE_ASSERT(verifyPthreadKeySlotOffset(g_stackLimitKey, baseAddr, g_stackLimitKeyOffset));
    *reinterpret_cast<size_t**>(baseAddr + g_stackLimitKeyOffset) = &g_stackLimit;
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    ESCARGOT_RELEASE_ASSERT(verifyPthreadKeySlotOffset(g_emptyStringKey, baseAddr, g_emptyStringKeyOffset));
    *reinterpret_cast<String***>(baseAddr + g_emptyStringKeyOffset) = &g_emptyStringInstance;
#endif
    ESCARGOT_RELEASE_ASSERT(verifyPthreadKeySlotOffset(g_gcEpochKey, baseAddr, g_gcEpochKeyOffset));
    *reinterpret_cast<size_t**>(baseAddr + g_gcEpochKeyOffset) = &g_gcEpoch;
#endif

    // g_stackLimit
#if defined(OS_WINDOWS)
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    void* stackStartAddress = reinterpret_cast<void*>(low);
    void* stackEndAddress = reinterpret_cast<void*>(high);
    size_t stackSize = reinterpret_cast<size_t>(stackEndAddress) - reinterpret_cast<size_t>(stackStartAddress);
#elif defined(OS_BAREMETAL)
    // On bare-metal/RTOS the stack base is provided by the embedder's
    // Platform (see PlatformRef::stackBase() in src/api/EscargotPublic.h):
    // the lowest valid stack address for the current thread/task.
    void* stackStartAddress = Global::platform()->stackBase();
    void* stackEndAddress = nullptr;
    (void)stackEndAddress;
#else
    void* stackStartAddress;
    void* stackEndAddress;
    size_t stackSize;
#if defined(OS_DARWIN)
    stackStartAddress = pthread_get_stackaddr_np(pthread_self());
    stackSize = pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &stackStartAddress, &stackSize);
    pthread_attr_destroy(&attr);
#endif

    stackSize = std::min(stackSize, (size_t)STACK_USAGE_LIMIT);
#ifdef STACK_GROWS_DOWN
    stackEndAddress = (char*)stackStartAddress - stackSize;
#if defined(OS_DARWIN)
    std::swap(stackStartAddress, stackEndAddress);
#endif
#else
    stackEndAddress = (char*)stackStartAddress + stackSize;
#endif
#endif

#ifdef STACK_GROWS_DOWN
    UNUSED_VARIABLE(stackEndAddress);
    g_stackLimit = reinterpret_cast<size_t>(stackStartAddress) + STACK_FREESPACE_FROM_LIMIT;
#else
    UNUSED_VARIABLE(stackStartAddress);
    g_stackLimit = reinterpret_cast<size_t>(stackEndAddress) - STACK_FREESPACE_FROM_LIMIT;
#endif

    // g_randEngine
    g_randEngine = new std::mt19937(static_cast<unsigned int>(time(NULL)));

    // g_bfContext
    bf_context_init(&g_bfContext, [](void* opaque, void* ptr, size_t size) -> void* { return realloc(ptr, size); }, nullptr);

#if defined(ENABLE_WASM)
    // g_wasmContext
    g_wasmContext.engine = wasm_engine_new();
    g_wasmContext.store = wasm_store_new(g_wasmContext.engine);
    g_wasmContext.lastGCCheckTime = 0;
#endif

    // g_gcEventListenerSet
    g_gcEventListenerSet = new GCEventListenerSet();
    // in addition, register genericGCEventListener here too
    GC_set_on_collection_event(genericGCEventListener);
    g_previousMarkStackEmptyListener = GC_get_on_mark_stack_empty();
    GC_set_on_mark_stack_empty(genericGCMarkStackEmptyListener);
    if (GC_is_incremental_mode()) {
        GC_set_start_callback(genericGCFullGCStartCallback);
    }

    // g_astAllocator
    g_astAllocator = new ASTAllocator();

    // g_bumpPointerAllocator
    g_bumpPointerAllocator = new WTF::BumpPointerAllocator();

#if defined(ENABLE_TCO)
    // g_tcoBuffer
    g_tcoBuffer = reinterpret_cast<Value*>(GC_MALLOC_UNCOLLECTABLE(sizeof(Value) * TCO_ARGUMENT_COUNT_LIMIT));
#endif

    // g_customData
    g_customData = Global::platform()->allocateThreadLocalCustomData();

    // g_gcEpoch
    g_gcEpoch = GC_get_gc_no();

    inited = true;
}

void ThreadLocal::finalize()
{
    ESCARGOT_RELEASE_ASSERT(inited);

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    delete ThreadLocal::g_emptyStringInstance;
    ThreadLocal::g_emptyStringInstance = nullptr;
#endif

    // g_customData
    Global::platform()->deallocateThreadLocalCustomData();
    g_customData = nullptr;

#if defined(ENABLE_TCO)
    // g_tcoBuffer
    GC_FREE(g_tcoBuffer);
    g_tcoBuffer = nullptr;
#endif

    // full gc(Heap::finalize) should be invoked after g_customData deallocation
    // because g_customData might contain GC-object
    Heap::finalize();

    // g_randEngine does not need finalization
    delete g_randEngine;
    g_randEngine = nullptr;

    // g_bfContext
    bf_context_end(&g_bfContext);

#if defined(ENABLE_WASM)
    // g_wasmContext
    wasm_store_delete(g_wasmContext.store);
    wasm_engine_delete(g_wasmContext.engine);
    g_wasmContext.store = nullptr;
    g_wasmContext.engine = nullptr;
    g_wasmContext.lastGCCheckTime = 0;
#endif

    // g_gcEventListenerSet
    delete g_gcEventListenerSet;
    g_gcEventListenerSet = nullptr;
    GC_set_on_collection_event(nullptr);
    GC_set_on_mark_stack_empty(g_previousMarkStackEmptyListener);
    g_previousMarkStackEmptyListener = nullptr;
    if (GC_is_incremental_mode()) {
        GC_set_start_callback(nullptr);
    }

    // g_astAllocator
    delete g_astAllocator;
    g_astAllocator = nullptr;

    // g_bumpPointerAllocator
    delete g_bumpPointerAllocator;
    g_bumpPointerAllocator = nullptr;

    // g_gcEpoch
    g_gcEpoch = 0;

    inited = false;
}

} // namespace Escargot
