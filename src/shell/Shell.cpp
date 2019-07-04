/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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
#include "runtime/VMInstance.h"
#include "util/Vector.h"
#include "runtime/Value.h"
#include "parser/ScriptParser.h"
#include "runtime/JobQueue.h"

#ifdef ESCARGOT_ENABLE_VENDORTEST
namespace Escargot {
void installTestFunctions(Escargot::ExecutionState& state);
}
#endif // ESCARGOT_ENABLE_VENDORTEST

NEVER_INLINE bool eval(Escargot::Context* context, Escargot::String* str, Escargot::String* fileName, bool shouldPrintScriptResult)
{
    Escargot::ExecutionState state(context);
    Escargot::ExecutionState stateForInit(&state, nullptr, false);
    Escargot::SandBox sb(context);
    Escargot::Script::ScriptSandboxExecuteResult result;

    Escargot::SandBox::SandBoxResult sandBoxResult = sb.run([&]() -> Escargot::Value {
        Escargot::Script* script = context->scriptParser().initializeScript(state, str, fileName);
        return script->execute(stateForInit, false, false);
    });

    result.result = sandBoxResult.result;
    result.msgStr = sandBoxResult.msgStr;
    result.error.errorValue = sandBoxResult.error;
    if (!sandBoxResult.error.isEmpty()) {
        for (size_t i = 0; i < sandBoxResult.stackTraceData.size(); i++) {
            Escargot::Script::ScriptSandboxExecuteResult::Error::StackTrace t;
            t.fileName = sandBoxResult.stackTraceData[i].fileName;
            t.line = sandBoxResult.stackTraceData[i].loc.line;
            t.column = sandBoxResult.stackTraceData[i].loc.column;
            result.error.stackTrace.pushBack(t);
        }
    }

    if (UNLIKELY(!result.error.errorValue.isEmpty())) {
        printf("Uncaught %s:\n", result.msgStr->toUTF8StringData().data());
        for (size_t i = 0; i < result.error.stackTrace.size(); i++) {
            printf("%s (%d:%d)\n", result.error.stackTrace[i].fileName->toUTF8StringData().data(), (int)result.error.stackTrace[i].line, (int)result.error.stackTrace[i].column);
        }
        return false;
    }

    if (shouldPrintScriptResult) {
        puts(result.msgStr->toUTF8StringData().data());
    }

    Escargot::DefaultJobQueue* jobQueue = Escargot::DefaultJobQueue::get(context->jobQueue());
    while (jobQueue->hasNextJob()) {
        auto jobResult = jobQueue->nextJob()->run();
        if (shouldPrintScriptResult) {
            if (jobResult.error.isEmpty()) {
                printf("%s\n", jobResult.result.toString(state)->toUTF8StringData().data());
            } else {
                printf("Uncaught %s:\n", jobResult.msgStr->toUTF8StringData().data());
            }
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    Escargot::Heap::initialize();
    Escargot::VMInstance* instance = new Escargot::VMInstance();
    Escargot::Context* context = new Escargot::Context(instance);
    Escargot::ExecutionState stateForInit(context);
#ifdef ESCARGOT_ENABLE_VENDORTEST
    installTestFunctions(stateForInit);
#endif

    bool runShell = true;
    bool memStats = false;

    Escargot::FunctionObject* fnRead = context->globalObject()->getOwnProperty(stateForInit, Escargot::ObjectPropertyName(stateForInit, Escargot::String::fromUTF8("read", 4))).value(stateForInit, context->globalObject()).asFunction();

    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) >= 2 && argv[i][0] == '-') { // parse command line option
            if (argv[i][1] == '-') { // `--option` case
                if (strcmp(argv[i], "--shell") == 0) {
                    runShell = true;
                    continue;
                }
                if (strcmp(argv[i], "--mem-stats") == 0) {
                    memStats = true;
                    continue;
                }
            } else { // `-option` case
                if (strcmp(argv[i], "-e") == 0) {
                    runShell = false;
                    i++;
                    Escargot::String* src = new Escargot::ASCIIString(argv[i], strlen(argv[i]));
                    const char* source = "shell input";
                    if (!eval(context, src, Escargot::String::fromUTF8(source, strlen(source)), false))
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
            runShell = false;
            Escargot::Value arg(Escargot::String::fromUTF8(argv[i], strlen(argv[i])));
            Escargot::String* src = Escargot::Object::call(stateForInit, fnRead, Escargot::Value(), 1, &arg).asString();

            if (!eval(context, src, Escargot::String::fromUTF8(argv[i], strlen(argv[i])), false))
                return 3;
        } else {
            runShell = false;
            printf("Cannot open file %s\n", argv[i]);
            return 3;
        }
    }

    while (runShell) {
        static char buf[2048];
        printf("escargot> ");
        if (!fgets(buf, sizeof buf, stdin)) {
            printf("ERROR: Cannot read interactive shell input\n");
            return 3;
        }
        auto s = Escargot::utf8StringToUTF16String(buf, strlen(buf));
        Escargot::String* str = new Escargot::UTF16String(std::move(s));
        eval(context, str, Escargot::String::fromUTF8("from shell input", strlen("from shell input")), true);
    }

    delete context;
    delete instance;

    Escargot::Heap::finalize();

    if (memStats) {
        Escargot::Heap::printGCHeapUsage();
    }

    return 0;
}
