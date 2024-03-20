/*
 * Copyright (c) 2023-present Samsung Electronics Co., Ltd
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

#include "EscargotJNI.h"

static void gcCallback(void* data)
{
    auto env = fetchJNIEnvFromCallback();
    if (!env) {
        LOGE("failed to fetch env from gc event callback");
        return;
    }
    if (!env->ExceptionCheck()) {
        env->PushLocalFrame(32);
        jclass clazz = env->FindClass("com/samsung/lwe/escargot/NativePointerHolder");
        jmethodID mId = env->GetStaticMethodID(clazz, "cleanUp", "()V");
        env->CallStaticVoidMethod(clazz, mId);
        env->PopLocalFrame(NULL);
    }
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
            const size_t maxNameLength = 980;
            if ((strnlen(builtinName, maxNameLength) + strnlen(fileName, maxNameLength)) < maxNameLength) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "GlobalObject.%s: cannot open file %s", builtinName, fileName);
                state->throwException(URIErrorObjectRef::create(state.get(), StringRef::createFromUTF8(msg, strnlen(msg, sizeof msg))));
            } else {
                state->throwException(URIErrorObjectRef::create(state.get(), StringRef::createFromASCII("invalid file name")));
            }
        } else {
            LOGE("%s", fileName);
        }
        return nullptr;
    }
}

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

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_initializeGlobals(JNIEnv* env, jclass clazz)
{
    if (!Globals::isInitialized()) {
        Globals::initialize(new ShellPlatform());
        Memory::addGCEventListener(Memory::MARK_START, gcCallback, nullptr);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_finalizeGlobals(JNIEnv* env, jclass clazz)
{
    if (Globals::isInitialized()) {
        // java object cleanup
        gcCallback(nullptr);

        Memory::removeGCEventListener(Memory::MARK_START, gcCallback, nullptr);
        Globals::finalize();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_initializeThread(JNIEnv* env, jclass clazz)
{
    if (!Globals::isInitialized()) {
        Globals::initializeThread();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_samsung_lwe_escargot_Globals_finalizeThread(JNIEnv* env, jclass clazz)
{
    if (Globals::isInitialized()) {
        Globals::finalizeThread();
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
        Java_com_samsung_lwe_escargot_Globals_isInitialized(JNIEnv* env, jclass clazz)
{
    return Globals::isInitialized();
}

extern "C"
JNIEXPORT jstring JNICALL
        Java_com_samsung_lwe_escargot_Globals_version(JNIEnv* env, jclass clazz)
{
    std::string version = Globals::version();
    std::basic_string<uint16_t> u16Version;

    for (auto c: version) {
        u16Version.push_back(c);
    }

    return env->NewString(u16Version.data(), u16Version.length());
}

extern "C"
JNIEXPORT jstring JNICALL
        Java_com_samsung_lwe_escargot_Globals_buildDate(JNIEnv* env, jclass clazz)
{
    std::string version = Globals::buildDate();
    std::basic_string<uint16_t> u16Version;

    for (auto c: version) {
        u16Version.push_back(c);
    }

    return env->NewString(u16Version.data(), u16Version.length());
}
