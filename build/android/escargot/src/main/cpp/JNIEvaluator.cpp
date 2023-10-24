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

static bool stringEndsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static Evaluator::EvaluatorResult evalScript(ContextRef* context, StringRef* source, StringRef* srcName, bool shouldPrintScriptResult, bool shouldExecutePendingJobsAtEnd, bool isModule)
{
    if (stringEndsWith(srcName->toStdUTF8String(), "mjs")) {
        isModule = isModule || true;
    }

    auto scriptInitializeResult = context->scriptParser()->initializeScript(source, srcName, isModule);
    if (!scriptInitializeResult.script) {
        LOGD("Script parsing error: ");
        switch (scriptInitializeResult.parseErrorCode) {
            case Escargot::ErrorObjectRef::Code::SyntaxError:
                LOGD("SyntaxError");
                break;
            case Escargot::ErrorObjectRef::Code::EvalError:
                LOGD("EvalError");
                break;
            case Escargot::ErrorObjectRef::Code::RangeError:
                LOGD("RangeError");
                break;
            case Escargot::ErrorObjectRef::Code::ReferenceError:
                LOGD("ReferenceError");
                break;
            case Escargot::ErrorObjectRef::Code::TypeError:
                LOGD("TypeError");
                break;
            case Escargot::ErrorObjectRef::Code::URIError:
                LOGD("URIError");
                break;
            default:
                break;
        }
        LOGD(": %s\n", scriptInitializeResult.parseErrorMessage->toStdUTF8String().data());
        Evaluator::EvaluatorResult evalResult;
        evalResult.error = StringRef::createFromASCII("script parsing error");
        return evalResult;
    }

    auto evalResult = Evaluator::execute(context, [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
                                             return script->execute(state);
                                         },
                                         scriptInitializeResult.script.get());

    if (!evalResult.isSuccessful()) {
        LOGD("Uncaught %s:\n", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
        for (size_t i = 0; i < evalResult.stackTrace.size(); i++) {
            LOGD("%s (%d:%d)\n", evalResult.stackTrace[i].srcName->toStdUTF8String().data(), (int)evalResult.stackTrace[i].loc.line, (int)evalResult.stackTrace[i].loc.column);
        }
        return evalResult;
    }

    if (shouldPrintScriptResult) {
        LOGD("%s", evalResult.resultOrErrorToString(context)->toStdUTF8String().data());
    }

    if (shouldExecutePendingJobsAtEnd) {
        while (context->vmInstance()->hasPendingJob() || context->vmInstance()->hasPendingJobFromAnotherThread()) {
            if (context->vmInstance()->waitEventFromAnotherThread(10)) {
                context->vmInstance()->executePendingJobFromAnotherThread();
            }
            if (context->vmInstance()->hasPendingJob()) {
                auto jobResult = context->vmInstance()->executePendingJob();
                if (shouldPrintScriptResult) {
                    if (jobResult.error) {
                        LOGD("Uncaught %s:(in promise job)\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                    } else {
                        LOGD("%s(in promise job)\n", jobResult.resultOrErrorToString(context)->toStdUTF8String().data());
                    }
                }
            }
        }
    }

    return evalResult;
}


extern "C"
JNIEXPORT jobject JNICALL
Java_com_samsung_lwe_escargot_Evaluator_evalScript(JNIEnv* env, jclass clazz, jobject context,
                                                   jstring source, jstring sourceFileName,
                                                   jboolean shouldPrintScriptResult,
                                                   jboolean shouldExecutePendingJobsAtEnd)
{
    THROW_NPE_RETURN_NULL(context, "Context");

    auto ptr = getPersistentPointerFromJava<ContextRef>(env, env->GetObjectClass(context), context);
    Evaluator::EvaluatorResult result = evalScript(ptr->get(), createJSStringFromJava(env, source),
                                                   createJSStringFromJava(env, sourceFileName), shouldPrintScriptResult,
                                                   shouldExecutePendingJobsAtEnd, false);
    if (env->ExceptionCheck()) {
        return nullptr;
    }
    jclass optionalClazz = env->FindClass("java/util/Optional");
    if (result.isSuccessful()) {
        return env->CallStaticObjectMethod(optionalClazz,
                                           env->GetStaticMethodID(optionalClazz, "of",
                                                                  "(Ljava/lang/Object;)Ljava/util/Optional;"),
                                           createJavaObjectFromValue(env, result.result));
    }
    return env->CallStaticObjectMethod(optionalClazz, env->GetStaticMethodID(optionalClazz, "empty",
                                                                             "()Ljava/util/Optional;"));
}