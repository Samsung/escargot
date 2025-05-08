/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "SandBox.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/VMInstance.h"
#include "parser/Script.h"
#include "parser/ast/Node.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

SandBox::SandBox(Context* s)
    : m_context(s)
{
    m_oldSandBox = m_context->vmInstance()->m_currentSandBox;
    m_context->vmInstance()->m_currentSandBox = this;
}

SandBox::~SandBox()
{
    ASSERT(m_context->vmInstance()->m_currentSandBox == this);
    m_context->vmInstance()->m_currentSandBox = m_oldSandBox;
}

void SandBox::processCatch(const Value& error, SandBoxResult& result)
{
    // when exception occurred, an undefined value is allocated for result value which will be never used.
    // this is to avoid dereferencing of null pointer.
    result.result = Value();
    result.error = error;

    fillStackDataIntoErrorObject(error);

#ifdef ESCARGOT_DEBUGGER
    Debugger* debugger = m_context->debugger();
    Debugger::SavedStackTraceDataVector exceptionTrace;
#endif /* ESCARGOT_DEBUGGER */

    ByteCodeLOCDataMap locMap;
    for (size_t i = 0; i < m_stackTraceDataVector.size(); i++) {
        if ((size_t)m_stackTraceDataVector[i].loc.index == SIZE_MAX && (size_t)m_stackTraceDataVector[i].loc.actualCodeBlock != SIZE_MAX) {
            // this means loc not computed yet.
            StackTraceDataOnStack traceData = m_stackTraceDataVector[i];
            ByteCodeBlock* block = traceData.loc.actualCodeBlock;

            ByteCodeLOCData* locData;
            auto iterMap = locMap.find(block);
            if (iterMap == locMap.end()) {
                locData = new ByteCodeLOCData();
                locMap.insert(std::make_pair(block, locData));
            } else {
                locData = iterMap->second;
            }

            ExtendedNodeLOC loc = block->computeNodeLOCFromByteCode(m_context,
                                                                    traceData.loc.byteCodePosition, block->m_codeBlock, locData);

            traceData.loc = loc;
            result.stackTrace.pushBack(traceData);

#ifdef ESCARGOT_DEBUGGER
            if (i < 8 && debugger != nullptr) {
                exceptionTrace.pushBack(Debugger::SavedStackTraceData(block, (uint32_t)loc.line, (uint32_t)loc.column));
            }
#endif /* ESCARGOT_DEBUGGER */
        } else {
            result.stackTrace.pushBack(m_stackTraceDataVector[i]);
        }
    }
    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }

#ifdef ESCARGOT_DEBUGGER
    if (debugger != nullptr) {
        ExecutionState state(m_context);
        String* message = error.toStringWithoutException(state);

        debugger->exceptionCaught(message, exceptionTrace);
    }
#endif /* ESCARGOT_DEBUGGER */
}

SandBox::SandBoxResult SandBox::run(Value (*scriptRunner)(ExecutionState&, void*), void* data)
{
    SandBox::SandBoxResult result;
    try {
        ExecutionState state(m_context);
        result.result = scriptRunner(state, data);
    } catch (const Value& err) {
        processCatch(err, result);
    }
    return result;
}

SandBox::SandBoxResult SandBox::run(ExecutionState& parentState, Value (*runner)(ExecutionState&, void*), void* data)
{
    SandBox::SandBoxResult result;
    try {
        ExecutionState state(m_context, &parentState, reinterpret_cast<LexicalEnvironment*>(NULL), 0, nullptr, false);
        result.result = runner(state, data);
    } catch (const Value& err) {
        processCatch(err, result);
    }
    return result;
}

bool SandBox::createStackTrace(StackTraceDataOnStackVector& stackTraceDataVector, ExecutionState& state, bool stopAtPause)
{
    UNUSED_VARIABLE(stopAtPause);

    ExecutionState* curState = &state;
#ifdef ESCARGOT_DEBUGGER
    uint32_t executionStateDepthIndex = 0;
    ExecutionState* activeSavedStackTraceExecutionState = nullptr;

    if (stopAtPause && state.context()->debuggerEnabled()) {
        activeSavedStackTraceExecutionState = state.context()->debugger()->activeSavedStackTraceExecutionState();
    }
#endif /* ESCARGOT_DEBUGGER */

    while (curState) {
        ExecutionState* pState = curState;

        while (pState) {
            if (!pState->lexicalEnvironment()) {
                // has no lexical scope like native function
                break;
            }

            if (pState->lexicalEnvironment()) {
                LexicalEnvironment* env = pState->lexicalEnvironment();
                EnvironmentRecord* record = env->record();
                ASSERT(!!record);

                if (pState->parent() && pState->parent()->lexicalEnvironment() == env) {
                    // check if state is on the same lexical scope of the parent state e.g. try-catch-finally block
                    // if so, move upward
                    pState = pState->parent();
                    continue;
                }

                if (pState->isLocalEvalCode()) {
                    // check eval scope first
                    break;
                }

                if (record->isDeclarativeEnvironmentRecord() && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                    // function scope
                    break;
                } else if (record->isGlobalEnvironmentRecord()) {
                    // global scope
                    break;
                } else if (record->isModuleEnvironmentRecord()) {
                    // module scope
                    break;
                }
            }

            pState = pState->parent();
        }

        if (!pState) {
            break;
        }


        if (pState->isLocalEvalCode()) {
            // for eval code case
            ASSERT(pState->codeBlock());
            InterpretedCodeBlock* cb = pState->codeBlock().value();
            ByteCodeBlock* b = cb->byteCodeBlock();

            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            if (curState->m_programCounter != nullptr) {
                if ((*curState->m_programCounter >= (size_t)b->m_code.data()) && (*curState->m_programCounter < (size_t)b->m_code.data() + b->m_code.size())) {
                    loc.byteCodePosition = *curState->m_programCounter - (size_t)b->m_code.data();
                    loc.actualCodeBlock = b;
                }
            }
            StackTraceDataOnStack data;
            data.loc = loc;
            data.srcName = cb->script()->srcName();
            data.sourceCode = cb->script()->sourceCode();
            data.isEval = true;
            data.isFunction = false;
            data.isAssociatedWithJavaScriptCode = true;
            data.isConstructor = false;
#ifdef ESCARGOT_DEBUGGER
            data.executionStateDepth = executionStateDepthIndex;
#endif /* ESCARGOT_DEBUGGER */

            stackTraceDataVector.pushBack(data);
        } else if (pState->lexicalEnvironment()) {
            // can be null on module outer env
            LexicalEnvironment* env = pState->lexicalEnvironment();
            EnvironmentRecord* record = env->record();

            InterpretedCodeBlock* cb = nullptr;
            bool isFunction = false;

            if (record->isGlobalEnvironmentRecord()) {
                cb = pState->lexicalEnvironment()->record()->asGlobalEnvironmentRecord()->globalCodeBlock();
            } else if (record->isModuleEnvironmentRecord()) {
                cb = pState->lexicalEnvironment()->outerEnvironment()->record()->asGlobalEnvironmentRecord()->globalCodeBlock();
            } else {
                ASSERT(record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
                isFunction = true;
                cb = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->asInterpretedCodeBlock();
            }

            ASSERT(!!cb);
            ByteCodeBlock* b = cb->byteCodeBlock();
            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            ASSERT(!curState->isNativeFunctionObjectExecutionContext());
            if (curState->m_programCounter != nullptr) {
                if ((*curState->m_programCounter >= (size_t)b->m_code.data()) && (*curState->m_programCounter < (size_t)b->m_code.data() + b->m_code.size())) {
                    loc.byteCodePosition = *curState->m_programCounter - (size_t)b->m_code.data();
                    loc.actualCodeBlock = b;
                }
            }
            StackTraceDataOnStack data;
            data.loc = loc;
            data.srcName = cb->script()->srcName();
            data.sourceCode = cb->script()->sourceCode();

            data.functionName = cb->functionName().string();
            data.isEval = false;
            data.isFunction = isFunction;
            data.callee = isFunction ? record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject() : nullptr;
            data.isAssociatedWithJavaScriptCode = true;
            data.isConstructor = isFunction ? record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isConstructor() : false;
#ifdef ESCARGOT_DEBUGGER
            data.executionStateDepth = executionStateDepthIndex;
#endif /* ESCARGOT_DEBUGGER */

            stackTraceDataVector.pushBack(data);
        } else if (pState->isNativeFunctionObjectExecutionContext()) {
            // find the CodeBlock of the same lexical scope
            ASSERT(!!pState->m_calledNativeFunctionObject);
            NativeCodeBlock* cb = pState->m_calledNativeFunctionObject->codeBlock()->asNativeCodeBlock();
            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            StackTraceDataOnStack data;
            data.loc = loc;

            StringBuilder builder;
            builder.appendString("function ");
            builder.appendString(cb->functionName().string());
            builder.appendString("() { ");
            builder.appendString("[native function]");
            builder.appendString(" } ");
            data.srcName = builder.finalize();
            data.sourceCode = String::emptyString;

            data.functionName = cb->functionName().string();
            data.isEval = false;
            data.isFunction = true;
            data.callee = pState->m_calledNativeFunctionObject;
            data.isAssociatedWithJavaScriptCode = cb->isInterpretedCodeBlock();
            data.isConstructor = cb->isNativeConstructor();
#ifdef ESCARGOT_DEBUGGER
            data.executionStateDepth = executionStateDepthIndex;
#endif /* ESCARGOT_DEBUGGER */

            stackTraceDataVector.pushBack(data);
        }

#ifdef ESCARGOT_DEBUGGER
        if (curState == activeSavedStackTraceExecutionState) {
            return true;
        }
#endif /* ESCARGOT_DEBUGGER */

        curState = pState->parent();
#ifdef ESCARGOT_DEBUGGER
        executionStateDepthIndex++;
#endif /* ESCARGOT_DEBUGGER */
    }

    return false;
}

void SandBox::throwException(ExecutionState& state, const Value& exception)
{
    m_stackTraceDataVector.clear();
    createStackTrace(m_stackTraceDataVector, state);

    // We MUST save thrown exception Value.
    // because bdwgc cannot track `thrown value`(may turned off by GC_DONT_REGISTER_MAIN_STATIC_DATA)
    m_exception = exception;
    throw exception;
}

void SandBox::rethrowPreviouslyCaughtException(ExecutionState& state, Value exception, StackTraceDataOnStackVector&& stackTraceDataVector)
{
    m_stackTraceDataVector = stackTraceDataVector;
    // update stack trace data if needs
    createStackTrace(m_stackTraceDataVector, state);

    // We MUST save thrown exception Value.
    // because bdwgc cannot track `thrown value`(may turned off by GC_DONT_REGISTER_MAIN_STATIC_DATA)
    m_exception = exception;
    throw exception;
}

StackTraceData* StackTraceData::create(SandBox* sandBox)
{
    StackTraceData* data = create(sandBox->stackTraceDataVector());
    data->exception = sandBox->exception();
    return data;
}

StackTraceData* StackTraceData::create(StackTraceDataOnStackVector& stackData)
{
    StackTraceData* data = new StackTraceData();
    data->gcValues.resizeWithUninitializedValues(stackData.size());
    data->nonGCValues.resizeWithUninitializedValues(stackData.size());
    data->exception = Value();

    for (size_t i = 0; i < stackData.size(); i++) {
        if ((size_t)stackData[i].loc.index == SIZE_MAX && (size_t)stackData[i].loc.actualCodeBlock != SIZE_MAX) {
            data->gcValues[i].byteCodeBlock = stackData[i].loc.actualCodeBlock;
            data->nonGCValues[i].byteCodePosition = stackData[i].loc.byteCodePosition;
        } else {
            data->gcValues[i].infoString = stackData[i].srcName;
            data->nonGCValues[i].byteCodePosition = SIZE_MAX;
        }
    }

    return data;
}

void StackTraceData::buildStackTrace(Context* context, StringBuilder& builder)
{
    if (exception.isObject()) {
        ExecutionState state(context);
        try {
            auto getResult = exception.asObject()->get(state, state.context()->staticStrings().name);
            if (getResult.hasValue()) {
                builder.appendString(getResult.value(state, exception.asObject()).toString(state));
                builder.appendString(": ");
            }
            getResult = exception.asObject()->get(state, state.context()->staticStrings().message);
            if (getResult.hasValue()) {
                builder.appendString(getResult.value(state, exception.asObject()).toString(state));
                builder.appendChar('\n');
            }
        } catch (const Value& v) {
            // ignore exception
        }
    }

    ByteCodeLOCDataMap locMap;
    for (size_t i = 0; i < gcValues.size(); i++) {
        builder.appendString("at ");
        if (nonGCValues[i].byteCodePosition == SIZE_MAX) {
            builder.appendString(gcValues[i].infoString);
        } else {
            ByteCodeBlock* block = gcValues[i].byteCodeBlock;

            ByteCodeLOCData* locData;
            auto iterMap = locMap.find(block);
            if (iterMap == locMap.end()) {
                locData = new ByteCodeLOCData();
                locMap.insert(std::make_pair(block, locData));
            } else {
                locData = iterMap->second;
            }

            ExtendedNodeLOC loc = gcValues[i].byteCodeBlock->computeNodeLOCFromByteCode(context,
                                                                                        nonGCValues[i].byteCodePosition, block->m_codeBlock, locData);

            builder.appendString(block->m_codeBlock->script()->srcName());
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.line));
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.column));

            String* src = block->m_codeBlock->script()->sourceCode();
            if (src->length() && loc.index != SIZE_MAX) {
                const size_t preLineMax = 40;
                const size_t afterLineMax = 40;

                size_t preLineSoFar = 0;
                size_t afterLineSoFar = 0;

                auto bad = src->bufferAccessData();
                size_t start = loc.index;
                int64_t idx = (int64_t)start;
                while (start - idx < preLineMax) {
                    if (idx == 0) {
                        break;
                    }
                    if (bad.charAt((size_t)idx) == '\r' || bad.charAt((size_t)idx) == '\n') {
                        idx++;
                        break;
                    }
                    idx--;
                }
                preLineSoFar = idx;

                idx = start;
                while (idx - start < afterLineMax) {
                    if ((size_t)idx == bad.length - 1) {
                        break;
                    }
                    if (bad.charAt((size_t)idx) == '\r' || bad.charAt((size_t)idx) == '\n') {
                        break;
                    }
                    idx++;
                }
                afterLineSoFar = idx;

                if (preLineSoFar <= afterLineSoFar && preLineSoFar <= bad.length && afterLineSoFar <= bad.length) {
                    builder.appendChar('\n');
                    builder.appendSubString(src, preLineSoFar, afterLineSoFar);
                    builder.appendChar('\n');
                    std::string sourceCodePosition;
                    for (size_t i = preLineSoFar; i < start; i++) {
                        sourceCodePosition += " ";
                    }
                    sourceCodePosition += "^";
                    builder.appendString(String::fromASCII(sourceCodePosition.data(), sourceCodePosition.length()));
                }
            }
        }

        if (i != gcValues.size() - 1) {
            builder.appendChar('\n');
        }
    }
    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }
}

void SandBox::fillStackDataIntoErrorObject(const Value& e)
{
    if (e.isObject() && e.asObject()->isErrorObject()) {
        ErrorObject* obj = e.asObject()->asErrorObject();

        obj->setStackTraceData(StackTraceData::create(this));
    }
}
} // namespace Escargot
