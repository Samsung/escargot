#include <hermes/hermes.h>
#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

namespace jsi = facebook::jsi;

static std::string readSource(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot read JavaScript source: " + path);
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static jsi::Value evaluateSource(jsi::Runtime& runtime, const std::string& path)
{
    return runtime.evaluateJavaScript(std::make_shared<jsi::StringBuffer>(readSource(path)), path);
}

static void defineFunction(jsi::Runtime& runtime, const char* name, unsigned parameters, jsi::HostFunctionType function)
{
    runtime.global().setProperty(runtime, name, jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forAscii(runtime, name), parameters, std::move(function)));
}

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "Hermes V1 " << HERMES_BENCHMARK_REVISION << '\n';
            return 0;
        }
        if (argc < 2) {
            throw std::runtime_error("Usage: hermes-benchmark-shell script.js [script.js ...]");
        }
        auto runtime = facebook::hermes::makeHermesRuntime(
            hermes::vm::RuntimeConfig::Builder().withES6BlockScoping(true).build());
        defineFunction(*runtime, "print", 0, [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value* arguments, size_t count) {
            for (size_t index = 0; index < count; ++index) {
                if (index) {
                    std::cout << ' ';
                }
                std::cout << arguments[index].toString(runtime).utf8(runtime);
            }
            std::cout << std::endl;
            return jsi::Value::undefined();
        });
        defineFunction(*runtime, "load", 1, [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value* arguments, size_t count) {
            if (count != 1 || !arguments[0].isString()) {
                throw jsi::JSError(runtime, "load requires one source path");
            }
            return evaluateSource(runtime, arguments[0].getString(runtime).utf8(runtime));
        });
        defineFunction(*runtime, "read", 1, [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value* arguments, size_t count) {
            if (count != 1 || !arguments[0].isString()) {
                throw jsi::JSError(runtime, "read requires one file path");
            }
            return jsi::Value(jsi::String::createFromUtf8(runtime, readSource(arguments[0].getString(runtime).utf8(runtime))));
        });
        defineFunction(*runtime, "readline", 0, [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value*, size_t) {
            std::string line;
            if (!std::getline(std::cin, line)) {
                throw jsi::JSError(runtime, "readline reached end of input");
            }
            return jsi::Value(jsi::String::createFromUtf8(runtime, line));
        });
        defineFunction(*runtime, "gc", 0, [](jsi::Runtime& runtime, const jsi::Value&, const jsi::Value*, size_t) {
            runtime.instrumentation().collectGarbage("benchmark after_gc checkpoint");
            return jsi::Value::undefined();
        });
        jsi::Object console(*runtime);
        console.setProperty(*runtime, "log", runtime->global().getProperty(*runtime, "print"));
        runtime->global().setProperty(*runtime, "console", std::move(console));
        for (int index = 1; index < argc; ++index) {
            evaluateSource(*runtime, argv[index]);
        }
        runtime->drainMicrotasks();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
