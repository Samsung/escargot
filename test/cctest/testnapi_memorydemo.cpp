#if defined(ENABLE_NAPI)
/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

// Demonstrates that a completely standard, unmodified N-API string workload -
// an addon that simply calls napi_create_string_utf8 to hold onto a lot of
// document-sized text - ends up using substantially less resident memory
// (RSS) on Escargot than it otherwise would, because large strings created
// through napi_create_string_utf8/latin1/utf16 (see those functions'
// transparent-compressible-string-routing comments, NapiFunctions.cpp/
// NapiValue.cpp) are eligible for Escargot's compressible-string feature: the
// underlying bytes can be compressed back down once the engine is idle
// (VMInstanceRef::enterIdleMode), and transparently decompressed again on any
// access, with zero visible behavior change to the addon. No napi_* call
// here is anything other than the exact same call a real, pre-existing
// addon would already be making.

#include "api/EscargotPublic.h"
#include "napi/NapiEnv.h"
#include "napi/NapiTypes.h"

using namespace Escargot;
using namespace Escargot::Napi;

#include "gtest/gtest.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

// Resident set size of the current process, in bytes, read straight from
// /proc/self/statm (field 2, resident pages) - the same kernel-reported
// number `ps`/`top` show, rather than anything Escargot's own allocator
// merely believes it has requested.
size_t CurrentRSSBytes()
{
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp == nullptr) {
        return 0;
    }
    long totalPages = 0;
    long residentPages = 0;
    int scanned = fscanf(fp, "%ld %ld", &totalPages, &residentPages);
    fclose(fp);
    if (scanned != 2) {
        return 0;
    }
    return static_cast<size_t>(residentPages) * static_cast<size_t>(sysconf(_SC_PAGESIZE));
}

// Builds `targetBytes` of genuinely compressible ASCII text - a repeated
// lorem-ipsum-like paragraph, the same shape of redundancy a batch of
// similar real-world documents/log records/JSON blobs would have - not
// random bytes (which wouldn't compress at all) and not one single repeated
// byte (which would compress unrealistically well). A short per-document
// header keeps every string distinct, like a real corpus would be, while
// leaving the bulk of the content identical/repetitive.
std::string MakeDocumentText(size_t index, size_t targetBytes)
{
    static const char* const kParagraph = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
                                          "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
                                          "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
                                          "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
                                          "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                                          "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
                                          "mollit anim id est laborum. ";

    std::string text;
    text.reserve(targetBytes + 64);

    char header[64];
    int headerLen = snprintf(header, sizeof(header), "Document #%06zu: ", index);
    text.append(header, static_cast<size_t>(headerLen));

    while (text.size() < targetBytes) {
        text += kParagraph;
    }
    text.resize(targetBytes);
    return text;
}

} // namespace

TEST(NapiMemoryDemo, CompressibleStringRSS)
{
    NapiEnv::globalInit();
    NapiEnv* napiEnv = NapiEnv::create();

    // ~40MB of logical text, split across many document-sized strings - well
    // above napi_create_string_utf8's compressible-string threshold
    // (kCompressibleStringThreshold, NapiTypes.h) per string, and large
    // enough in aggregate for the RSS difference to be clearly visible over
    // background noise.
    const size_t kStringCount = 20000;
    const size_t kStringBytes = 2048;
    const size_t kLogicalBytes = kStringCount * kStringBytes;

    size_t baselineRSS = CurrentRSSBytes();

    napi_ref arrayRef = nullptr;
    Evaluator::EvaluatorResult buildResult = Evaluator::execute(
        napiEnv->context(), [](ExecutionStateRef* state, napi_env env, size_t stringCount, size_t stringBytes, napi_ref* arrayRefOut) -> ValueRef* {
            // A real addon parsing/holding a batch of document-sized strings
            // would build exactly this shape: an array (or any other
            // container) rooted for the life of the addon, populated purely
            // via napi_create_string_utf8 - nothing addon-visible changes
            // here versus any other N-API host.
            napi_value array = nullptr;
            EXPECT_EQ(napi_create_array(env, &array), napi_ok);

            for (size_t i = 0; i < stringCount; i++) {
                std::string text = MakeDocumentText(i, stringBytes);
                napi_value str = nullptr;
                EXPECT_EQ(napi_create_string_utf8(env, text.c_str(), text.size(), &str), napi_ok);
                EXPECT_EQ(napi_set_element(env, array, static_cast<uint32_t>(i), str), napi_ok);
            }

            // Keep the array (and therefore every string in it) rooted for
            // the rest of this test, exactly as a long-lived addon-owned
            // napi_ref would - so nothing measured below is an artifact of
            // the array/strings themselves getting collected.
            napi_ref ref = nullptr;
            EXPECT_EQ(napi_create_reference(env, array, 1, &ref), napi_ok);
            *arrayRefOut = ref;

            return ValueRef::createUndefined();
        },
        napiEnv->env(), kStringCount, kStringBytes, &arrayRef);
    ASSERT_TRUE(buildResult.isSuccessful()) << buildResult.resultOrErrorToString(napiEnv->context())->toStdUTF8String();

    size_t preCompressionRSS = CurrentRSSBytes();

    // Nothing addon-visible happens here either: an embedder (not the addon)
    // calling VMInstanceRef::enterIdleMode - e.g. from an idle-time hook, the
    // same way Escargot's own shell/other hosts do - is what triggers
    // compression of every still-live compressible string, forcing a GC pass
    // and unmapping freed pages along the way.
    VMInstanceRef* vmInstance = napiEnv->vmInstance();
    vmInstance->setConfig(vmInstance->config() | static_cast<size_t>(VMInstanceRef::ConfigFlag::CompressCompressibleStringsEnterIdle));
    vmInstance->enterIdleMode();

    size_t postCompressionRSS = CurrentRSSBytes();

    long long preVsBaseline = static_cast<long long>(preCompressionRSS) - static_cast<long long>(baselineRSS);
    long long saved = static_cast<long long>(preCompressionRSS) - static_cast<long long>(postCompressionRSS);
    double percentSaved = preCompressionRSS > 0 ? (100.0 * static_cast<double>(saved) / static_cast<double>(preCompressionRSS)) : 0.0;

    printf("\n");
    printf("=== Escargot N-API compressible-string RSS demo ===\n");
    printf("workload: %zu strings x %zu bytes = %.2f MB logical text\n",
           kStringCount, kStringBytes, static_cast<double>(kLogicalBytes) / (1024.0 * 1024.0));
    printf("string content: repeated lorem-ipsum-like ASCII text (genuinely compressible,\n");
    printf("                not random bytes) with a short per-document header\n");
    printf("baseline RSS (before creating any strings):     %10.2f MB\n", static_cast<double>(baselineRSS) / (1024.0 * 1024.0));
    printf("pre-compression RSS (all strings created):      %10.2f MB (+%.2f MB over baseline)\n",
           static_cast<double>(preCompressionRSS) / (1024.0 * 1024.0), static_cast<double>(preVsBaseline) / (1024.0 * 1024.0));
    printf("post-compression RSS (after enterIdleMode):     %10.2f MB\n", static_cast<double>(postCompressionRSS) / (1024.0 * 1024.0));
    printf("RSS saved by compression:                        %10.2f MB (%.1f%% of pre-compression RSS)\n",
           static_cast<double>(saved) / (1024.0 * 1024.0), percentSaved);
    printf("====================================================\n");

    // A real, reproducible regression guard - not a faked number. Guard on the
    // absolute number of bytes handed back, not on a percentage: the percentage
    // is taken against the whole process RSS, so it also moves with whatever
    // else the process happens to be holding. Run as part of the full cctest
    // binary, the earlier tests leave tens of MB of warm, fragmented heap
    // behind, which both inflates that denominator and leaves the freed string
    // blocks sharing pages with live objects, so a healthy run legitimately
    // reports a much smaller percentage there than the same workload does
    // standalone. The threshold is intentionally well below what either case
    // shows, to avoid a flaky test while still catching a real regression
    // (e.g. compression silently not happening at all, which would show ~0).
    EXPECT_GT(saved, 4 * 1024 * 1024);

    // keep the reference alive (and therefore silence "unused" concerns)
    // until the very end of the test, exactly as a long-lived addon would.
    ASSERT_NE(arrayRef, nullptr);
}
#endif // ENABLE_NAPI
