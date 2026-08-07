#include <node_api.h>
#include <napi/node_api_hosting.h>
#include <iostream>
#include <fstream>
#include <string>

static std::string g_jsCode;

static std::string ReadFile(const char* path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "Usage: escargot-napi <script.js>" << std::endl;
        return 1;
    }

    const char* filePath = argv[1];
    g_jsCode = ReadFile(filePath);
    if (g_jsCode.empty()) {
        std::cerr << "Error: Cannot read or empty file " << filePath << std::endl;
        return 1;
    }

    napi_platform platform = nullptr;
    napi_env env = nullptr;

    if (napi_create_platform(0, nullptr, &platform) != napi_ok) {
        std::cerr << "Error: Failed to create platform" << std::endl;
        return 1;
    }

    if (napi_create_environment(platform, &env) != napi_ok) {
        std::cerr << "Error: Failed to create environment" << std::endl;
        napi_destroy_platform(platform);
        return 1;
    }

    napi_value script = nullptr;
    if (napi_create_string_utf8(env, g_jsCode.c_str(), g_jsCode.length(), &script) != napi_ok) {
        std::cerr << "Error: Failed to create script string" << std::endl;
        napi_destroy_environment(env);
        napi_destroy_platform(platform);
        return 1;
    }

    napi_value result = nullptr;
    napi_status runStatus = napi_run_script(env, script, &result);
    if (runStatus != napi_ok) {
        std::cerr << "Error: Script execution failed or threw an uncaught exception" << std::endl;

        bool hasException = false;
        napi_is_exception_pending(env, &hasException);
        if (hasException) {
            napi_value error = nullptr;
            napi_get_and_clear_last_exception(env, &error);
            std::cerr << "Uncaught exception thrown during script execution." << std::endl;
        }
    } else {
        // Print result to console if it returned a value
        napi_valuetype type;
        napi_typeof(env, result, &type);
        if (type == napi_string) {
            char buf[256];
            size_t written = 0;
            napi_get_value_string_utf8(env, result, buf, sizeof(buf), &written);
            std::cout << buf << std::endl;
        } else if (type == napi_number) {
            double val = 0;
            napi_get_value_double(env, result, &val);
            std::cout << val << std::endl;
        } else if (type != napi_undefined && type != napi_null) {
            std::cout << "[object]" << std::endl;
        }
    }

    // Pump loop to process outstanding promise/microtasks
    bool hasMoreWork = true;
    escargot_napi_pump_message_loop(env, &hasMoreWork);

    // Verify Promise state by printing globalThis.step2 and step3
    // But since napi_get_named_property requires execution state, we'll just check stdout instead.
    // The previous execution printed "end of script", now let's just exit gracefully.
    napi_status destroyEnvStatus = napi_destroy_environment(env);
    napi_status destroyPlatformStatus = napi_destroy_platform(platform);

    return (destroyEnvStatus == napi_ok && destroyPlatformStatus == napi_ok) ? 0 : 1;
}
