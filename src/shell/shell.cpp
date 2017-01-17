/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "runtime/VMInstance.h"
#include "runtime/ExecutionContext.h"
#include "util/Vector.h"
#include "runtime/Value.h"
#include "parser/ScriptParser.h"
#ifdef ESCARGOT_ENABLE_PROMISE
#include "DefaultJobQueue.h"
#endif

#ifdef ANDROID
void __attribute__((optimize("O0"))) fillStack(size_t siz)
{
    volatile char a[siz];
    for (unsigned i = 0; i < siz; i++) {
        a[i] = 0x00;
    }
}
#endif

bool eval(Escargot::Context* context, Escargot::String* str, Escargot::String* fileName, bool shouldPrintScriptResult)
{
    auto result = context->scriptParser().parse(str, fileName);
    if (result.m_error) {
        char msg[10240];
        auto err = result.m_error->message->toUTF8StringData();
        puts(err.data());
        return false;
    } else {
        Escargot::Script::ScriptSandboxExecuteResult resultValue = result.m_script->sandboxExecute(context);
        Escargot::ExecutionState state(context);
        if (!resultValue.result.isEmpty()) {
            if (shouldPrintScriptResult)
                puts(resultValue.msgStr->toUTF8StringData().data());

#ifdef ESCARGOT_ENABLE_PROMISE
            Escargot::DefaultJobQueue* jobQueue = Escargot::DefaultJobQueue::get(context->jobQueue());
            while (jobQueue->hasNextJob()) {
                auto jobResult = jobQueue->nextJob()->run(state);
                if (shouldPrintScriptResult) {
                    if (!jobResult.result.isEmpty()) {
                        printf("%s\n", jobResult.result.toString(state)->toUTF8StringData().data());
                    } else {
                        printf("Uncaught %s:\n", jobResult.msgStr->toUTF8StringData().data());
                    }
                }
            }
#endif

        } else {
            printf("Uncaught %s:\n", resultValue.msgStr->toUTF8StringData().data());
            for (size_t i = 0; i < resultValue.error.stackTrace.size(); i++) {
                printf("%s (%d:%d)\n", resultValue.error.stackTrace[i].fileName->toUTF8StringData().data(), (int)resultValue.error.stackTrace[i].line, (int)resultValue.error.stackTrace[i].column);
            }
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[])
{
    // GC_disable();
    /*
    GC_set_force_unmap_on_gcollect(1);
    for (size_t i = 0; i < 96; i ++) {
        void* ptr = GC_MALLOC_ATOMIC(1024*1024*i);
        printf("%p\n", ptr);
    }
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    */

    Escargot::Heap::initialize();

#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    Escargot::VMInstance* instance = new Escargot::VMInstance();
    Escargot::Context* context = new Escargot::Context(instance);

    bool runShell = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shell") == 0) {
            runShell = true;
            continue;
        }
        if (strcmp(argv[i], "-e") == 0) {
            runShell = false;
            i++;
            Escargot::String* src = new Escargot::ASCIIString(argv[i], strlen(argv[i]));
            const char* source = "shell input";
            if (!eval(context, src, Escargot::String::fromUTF8(source, strlen(source)), false))
                return 3;
            continue;
        }

        FILE* fp = fopen(argv[i], "r");
        if (fp) {
            runShell = false;
            std::string str;
            char buf[512];
            bool hasNonASCIIContent = false;
            while (fgets(buf, sizeof buf, fp) != NULL) {
                str += buf;
                if (!hasNonASCIIContent) {
                    char* check = buf;
                    while (*check) {
                        if (*check < 0) {
                            hasNonASCIIContent = true;
                            break;
                        }
                        check++;
                    }
                }
            }
            fclose(fp);

            Escargot::String* src;
            if (hasNonASCIIContent)
                src = new Escargot::UTF16String(std::move(Escargot::utf8StringToUTF16String(str.data(), str.length())));
            else
                src = new Escargot::ASCIIString(str.data(), str.length());

            if (!eval(context, src, Escargot::String::fromUTF8(argv[i], strlen(argv[i])), false))
                return 3;
        } else {
            runShell = false;
            printf("Cannot open file %s\n", argv[i]);
            return 3;
        }
    }

    while (runShell) {
        char buf[2048];
        printf("escargot> ");
        if (!fgets(buf, sizeof buf, stdin)) {
            printf("ERROR: Cannot read interactive shell input\n");
            return 3;
        }
        Escargot::String* str = new Escargot::UTF16String(std::move(Escargot::utf8StringToUTF16String(buf, strlen(buf))));
        eval(context, str, Escargot::String::fromUTF8("from shell input", strlen("from shell input")), true);
    }

    delete context;
    delete instance;

    Escargot::Heap::finalize();

    return 0;
}


/*
    // small value & value test
    Escargot::Object* obj = new Escargot::Object();
    obj->m_values.push_back(Escargot::Value(1));
    obj->m_values.push_back(Escargot::Value(-1));
    obj->m_values.push_back(Escargot::Value(0xefffffff));
    obj->m_values.push_back(Escargot::Value(0xffffffff));
    obj->m_values.push_back(Escargot::Value(1.1));
    auto obj2 = new Escargot::Object();
    obj->m_values.push_back(Escargot::Value(obj2));

    Escargot::Value v;
    v = obj->m_values[0];
    ASSERT(v.asNumber() == 1);
    v = obj->m_values[1];
    ASSERT(v.asNumber() == -1);
    v = obj->m_values[2];
    ASSERT(v.asNumber() == 0xefffffff);
    v = obj->m_values[3];
    ASSERT(v.asNumber() == 0xffffffff);
    v = obj->m_values[4];
    ASSERT(v.asNumber() == 1.1);
    v = obj->m_values[5];
    ASSERT(v == obj2);

    uint64_t cnt = 0;
    for (int64_t i = std::numeric_limits<int32_t>::min() ; i < std::numeric_limits<int32_t>::max(); i ++) {
        Escargot::SmallValue smallValue = Escargot::Value(i);
        RELEASE_ASSERT(Escargot::Value(smallValue).asNumber() == i);
        cnt++;
        if (cnt % 100000 == 0) {
            printf("%lld\n", i);
        }
    }
 */

/* Custom allocator & Array iterator test

#include "runtime/ArrayObject.h"

    size_t counter = 0;
    Escargot::HeapObjectIteratorCallback callback =
        [&counter](Escargot::ExecutionState& state, void* obj) {
            Escargot::ArrayObject* arr = (Escargot::ArrayObject*) obj;
            printf("ArrayObject %p with length %u\n", obj, arr->getLength(state));
            counter++;
        };
    Escargot::ExecutionState state(context);
    Escargot::ArrayObject::iterateArrays(state, callback);
    printf("Array total count %zu\n", counter);
*/
