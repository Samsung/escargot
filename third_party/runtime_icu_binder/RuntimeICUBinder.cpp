/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the MIT License (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License
 * at
 *
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include "RuntimeICUBinder.h"

#ifdef _WIN32
#define OS_WINDOWS 1
#elif _WIN64
#define OS_WINODWS 1
#elif __APPLE__
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
#define OS_POSIX 1
#elif TARGET_OS_IPHONE
#define OS_POSIX 1
#elif TARGET_OS_MAC
#define OS_POSIX 1
#else
#error "Unknown Apple platform"
#endif
#elif __linux__
#define OS_POSIX 1
#elif __unix__ // all unices not caught above
#define OS_POSIX 1
#elif defined(_POSIX_VERSION)
#define OS_POSIX 1
#else
#error "failed to detect target OS"
#endif

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string>

#if defined(OS_POSIX)
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <dlfcn.h>
#elif defined(OS_WINDOWS)
#include <Windows.h>
#endif

namespace RuntimeICUBinder {

ICU &ICU::instance()
{
    static ICU instance;
    return instance;
}

static void die(const char *message)
{
    fputs(message, stderr);
    abort();
}

static constexpr int s_icuMinVersion = 49; // icu 4.9 was released on 2012-06-06
static constexpr int s_icuMaxVersion = 2048;

#if defined(OS_POSIX)
static void *findSo(ICU::Soname name)
{
    std::string soPath;
    switch (name) {
    case ICU::Soname::uc:
        soPath = "libicuuc.so";
        break;
    case ICU::Soname::i18n:
        soPath = "libicui18n.so";
        break;
    case ICU::Soname::io:
        soPath = "libicuio.so";
        break;
    default:
        die("invalid soname");
    }

    void *handle = dlopen(soPath.data(), RTLD_LAZY);
    if (handle) {
        return handle;
    }

    // try to find with version name
    for (int i = s_icuMinVersion; i < s_icuMaxVersion; i++) {
        void *handle = dlopen((soPath + "." + std::to_string(i)).data(), RTLD_LAZY);
        if (handle) {
            return handle;
        }
    }

    // will fail. reopen with original name to set dlerror
    dlopen(soPath.data(), RTLD_LAZY);
    return nullptr;
}
#endif

void ICU::loadSo(ICU::Soname name)
{
    assert(m_soHandles[name] == nullptr);
    void *handle;
#if defined(OS_POSIX)
    handle = findSo(name);
    if (!handle) {
        char *error = dlerror();
        if (error) {
            fputs(error, stderr);
        }
        die("failed to open so");
    }
#else
    handle = LoadLibraryA("icu.dll");
#endif
    m_soHandles[name] = handle;
}

void *ICU::loadFunction(Soname soname, Function kind)
{
    assert(m_functions[kind] == nullptr);
    const char *name = "";
    switch (kind) {
#define DECLARE_CASE(fnName, fnType, fnReturnType) \
    case function##fnName:                         \
        name = #fnName;                            \
        break;
        FOR_EACH_UC_OP(DECLARE_CASE)
        FOR_EACH_UC_VOID_OP(DECLARE_CASE)
        FOR_EACH_I18N_OP(DECLARE_CASE)
        FOR_EACH_I18N_VOID_OP(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE_STICKY(fnName) \
    case function##fnName:          \
        name = #fnName;             \
        break;
        FOR_EACH_I18N_STICKY_OP(DECLARE_CASE_STICKY)
#undef DECLARE_CASE_STICKY
    default:
        die("failed to load function");
    }

    void *fn;
#if defined(OS_POSIX)
    char nameBuffer[256];
    if (m_icuVersion == -1) {
        fn = dlsym(ensureLoadSo(soname), name);
    } else {
        snprintf(nameBuffer, sizeof(nameBuffer), "%s_%d", name, m_icuVersion);
        fn = dlsym(ensureLoadSo(soname), nameBuffer);
    }

    if (!fn) {
        fputs("failed to load", stderr);
        char *error = dlerror();
        if (error) {
            fputs(error, stderr);
        }
        if (m_icuVersion == -1) {
            die(name);
        } else {
            die(nameBuffer);
        }
    }
#else
    fn = GetProcAddress((HMODULE)ensureLoadSo(soname), name);

    if (!fn) {
        fputs("failed to load", stderr);
        die(name);
    }
#endif

    m_functions[kind] = fn;
    return fn;
}

#if defined(OS_POSIX)
// https://stackoverflow.com/questions/3118582/how-do-i-find-the-current-system-timezone
/* Look for tag=someValue within filename.  When found, return someValue
 * in the provided value parameter up to valueSize in length.  If someValue
 * is enclosed in quotes, remove them. */
static char *timeValueFromFile(const char *filename, const char *tag, char *value, size_t valueSize)
{
    char buffer[128], *lasts;
    int foundTag = 0;

    FILE *fd = fopen(filename, "r");
    if (fd) {
        /* Process the file, line by line. */
        while (fgets(buffer, sizeof(buffer), fd)) {
            lasts = buffer;
            /* Look for lines with tag=value. */
            char *token = strtok_r(lasts, "=", &lasts);
            /* Is this the tag we are looking for? */
            if (token && !strcmp(token, tag)) {
                /* Parse out the value. */
                char *zone = strtok_r(lasts, " \t\n", &lasts);
                /* If everything looks good, copy it to our return var. */
                if (zone && strlen(zone) > 0) {
                    size_t i = 0;
                    size_t j = 0;
                    char quote = 0x00;
                    /* Rather than just simple copy, remove quotes while we copy. */
                    for (i = 0; i < strlen(zone) && i < valueSize - 1; i++) {
                        /* Start quote. */
                        if (quote == 0x00 && zone[i] == '"')
                            quote = zone[i];
                        /* End quote. */
                        else if (quote != 0x00 && quote == zone[i])
                            quote = 0x00;
                        /* Copy bytes. */
                        else {
                            value[j] = zone[i];
                            j++;
                        }
                    }
                    value[j] = 0x00;
                    foundTag = 1;
                }
                break;
            }
        }
        fclose(fd);
    }
    if (foundTag)
        return value;
    return NULL;
}
#endif

static std::string extractLocaleName(std::string input)
{
    if (input.find('.') != std::string::npos) {
        input = input.substr(0, input.find('.'));
    }
    return input;
}


std::string ICU::findSystemLocale()
{
    char *c = getenv("LANG");
    if (c && strlen(c)) {
        return extractLocaleName(c);
    }
    c = setlocale(LC_CTYPE, "");
    if (c && strlen(c)) {
        auto locale = extractLocaleName(c);
        if (locale != "C") {
            return locale;
        }
    }
    return ICU::instance().uloc_getDefault();
}

std::string ICU::findSystemTimezoneName()
{
#if defined(OS_POSIX)
    char filename[PATH_MAX + 1];
    struct stat fstat;

    std::string nowPath = "/etc/localtime";
    while (true) {
        lstat(nowPath.data(), &fstat);

        if (nowPath.length() > 20) {
            if (nowPath.substr(0, 20) == "/usr/share/zoneinfo/") {
                return nowPath.substr(20);
            }
        }

        if (S_ISLNK(fstat.st_mode)) {
            int nSize = readlink(nowPath.data(), filename, sizeof(filename));
            if (nSize > 0) {
                filename[nSize] = 0;
                nowPath = filename;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    char tz[128];
    size_t tzSize = sizeof tz;
    /* If there is an /etc/timezone file, then we expect it to contain
     * nothing except the timezone. */

    FILE *fd = fopen("/etc/timezone", "r"); // Ubuntu
    if (fd) {
        char buffer[128];
        // There should only be one line, in this case.
        while (fgets(buffer, sizeof(buffer), fd)) {
            char *lasts = buffer;
            // We don't want a line feed on the end.
            char *tag = strtok_r(lasts, " \t\n", &lasts);
            // Idiot check.
            if (tag && strlen(tag) > 0 && strlen(tag) < (tzSize - 1) && tag[0] != '#') {
                strncpy(tz, tag, sizeof tz);
                fclose(fd);
                return tz;
            }
        }
        fclose(fd);
    }

    if (timeValueFromFile("/etc/sysconfig/clock", "ZONE", tz, tzSize)) { // Redhat.
        return tz;
    } else if (timeValueFromFile("/etc/TIMEZONE", "TZ", tz, tzSize)) { // Solaris.
        return tz;
    }
#endif
#if defined(ANDROID)
    FILE *fp = popen("getprop persist.sys.timezone", "r");
    if (fp) {
        char buffer[512];
        fgets(buffer, sizeof(buffer), fp);
        std::string ret = buffer;
        fclose(fp);

        if (ret.size() && ret.back() == '\n') {
            ret = ret.substr(0, ret.size() - 1);
        }
        if (ret.size()) {
            return ret;
        }
    }
#endif
    UChar result[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = ICU::instance().ucal_getDefaultTimeZone(result, sizeof(result) / sizeof(UChar), &status);
    if (U_SUCCESS(status)) {
        std::string u8Result;
        for (int32_t i = 0; i < len; i++) {
            u8Result.push_back(result[i]);
        }
        return u8Result;
    }
    return "";
}

ICU::ICU()
{
    memset(m_soHandles, 0, sizeof(void *) * ICU::Soname::SonameMax);
    memset(m_functions, 0, sizeof(void *) * ICU::Function::FunctionMax);
#if defined(OS_POSIX)
    // load first so
    ensureLoadSo(Soname::uc);

    // try to load without version
    void *ptr = dlsym(so(Soname::uc), "u_tolower");
    if (ptr) {
        m_icuVersion = -1;
    } else {
        // find version
        int ver = s_icuMinVersion;
        bool found = false;
        while (ver < s_icuMaxVersion) {
            std::string fn = "u_tolower_";
            fn += std::to_string(ver);
            void *ptr = dlsym(so(Soname::uc), fn.data());
            if (ptr) {
                found = true;
                m_icuVersion = ver;
                break;
            }
            ver++;
        }

        if (!found) {
            die("failed to read version from so");
        }
    }
#endif
}
} // namespace RuntimeICUBinder
