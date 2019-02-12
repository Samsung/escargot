/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "SandBox.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "parser/Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

SandBox::SandBoxResult SandBox::run(const std::function<Value()>& scriptRunner)
{
    SandBox::SandBoxResult result;
    ExecutionState state(m_context);
    try {
        result.result = scriptRunner();
        result.msgStr = result.result.toString(state);
    } catch (const Value& err) {
        result.error = err;

        try {
            result.msgStr = result.error.toString(state);
        } catch (const Value&) {
            result.msgStr = String::fromASCII("Error while executing script but could not convert error to string");
        }

        fillStackDataIntoErrorObject(err);

        for (size_t i = 0; i < m_stackTraceData.size(); i++) {
            if ((size_t)m_stackTraceData[i].second.loc.index == SIZE_MAX && (size_t)m_stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
                // this means loc not computed yet.
                ExtendedNodeLOC loc = m_stackTraceData[i].second.loc.actualCodeBlock->computeNodeLOCFromByteCode(m_context,
                                                                                                                 m_stackTraceData[i].second.loc.byteCodePosition, m_stackTraceData[i].second.loc.actualCodeBlock->m_codeBlock);
                StackTraceData traceData;
                traceData.loc = loc;
                traceData.fileName = m_stackTraceData[i].second.loc.actualCodeBlock->m_codeBlock->script()->fileName();
                traceData.source = m_stackTraceData[i].second.loc.actualCodeBlock->m_codeBlock->script()->src();
                result.stackTraceData.pushBack(traceData);
            } else {
                result.stackTraceData.pushBack(m_stackTraceData[i].second);
            }
        }
    }
    return result;
}

void SandBox::throwException(ExecutionState& state, Value exception)
{
    m_exception = exception;
    throw exception;
}

static Value builtinErrorObjectStackInfo(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(thisValue.isPointerValue() && thisValue.asPointerValue()->isErrorObject())) {
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get Error.prototype.stack called on incompatible receiver");
    }

    ErrorObject* obj = thisValue.asObject()->asErrorObject();
    if (obj->stackTraceData() == nullptr) {
        return String::emptyString;
    }

    auto stackTraceData = obj->stackTraceData();
    StringBuilder builder;
    stackTraceData->buildStackTrace(state.context(), builder);
    return builder.finalize();
}

ErrorObject::StackTraceData* ErrorObject::StackTraceData::create(SandBox* sandBox)
{
    ErrorObject::StackTraceData* data = new ErrorObject::StackTraceData();
    data->gcValues.resizeWithUninitializedValues(sandBox->m_stackTraceData.size());
    data->nonGCValues.resizeWithUninitializedValues(sandBox->m_stackTraceData.size());
    data->exception = sandBox->m_exception;

    for (size_t i = 0; i < sandBox->m_stackTraceData.size(); i++) {
        if ((size_t)sandBox->m_stackTraceData[i].second.loc.index == SIZE_MAX && (size_t)sandBox->m_stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
            data->gcValues[i].byteCodeBlock = sandBox->m_stackTraceData[i].second.loc.actualCodeBlock;
            data->nonGCValues[i].byteCodePosition = sandBox->m_stackTraceData[i].second.loc.byteCodePosition;
        } else {
            data->gcValues[i].infoString = sandBox->m_stackTraceData[i].second.fileName;
            data->nonGCValues[i].byteCodePosition = SIZE_MAX;
        }
    }

    return data;
}

void ErrorObject::StackTraceData::buildStackTrace(Context* context, StringBuilder& builder)
{
    if (exception.isObject()) {
        ExecutionState state(context);
        SandBox sb(context);
        sb.run([&]() {
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
            return Value();
        });
    }
    for (size_t i = 0; i < gcValues.size(); i++) {
        builder.appendString("at ");
        if (nonGCValues[i].byteCodePosition == SIZE_MAX) {
            builder.appendString(gcValues[i].infoString);
        } else {
            ExtendedNodeLOC loc = gcValues[i].byteCodeBlock->computeNodeLOCFromByteCode(context,
                                                                                        nonGCValues[i].byteCodePosition, gcValues[i].byteCodeBlock->m_codeBlock);
            builder.appendString(gcValues[i].byteCodeBlock->m_codeBlock->script()->fileName());
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.line));
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.column));

            String* src = gcValues[i].byteCodeBlock->m_codeBlock->script()->src();
            if (src->length()) {
                const size_t preLineMax = 40;
                const size_t afterLineMax = 40;

                size_t preLineSoFar = 0;
                size_t afterLineSoFar = 0;

                size_t start = loc.index;
                int64_t idx = (int64_t)start;
                while (start - idx < preLineMax) {
                    if (idx == 0) {
                        break;
                    }
                    if (src->charAt((size_t)idx) == '\r' || src->charAt((size_t)idx) == '\n') {
                        idx++;
                        break;
                    }
                    idx--;
                }
                preLineSoFar = idx;

                idx = start;
                while (idx - start < afterLineMax) {
                    if ((size_t)idx == src->length() - 1) {
                        break;
                    }
                    if (src->charAt((size_t)idx) == '\r' || src->charAt((size_t)idx) == '\n') {
                        break;
                    }
                    idx++;
                }
                afterLineSoFar = idx;

                if (preLineSoFar <= afterLineSoFar && preLineSoFar <= src->length() && afterLineSoFar <= src->length()) {
                    auto subSrc = src->substring(preLineSoFar, afterLineSoFar);
                    builder.appendChar('\n');
                    builder.appendString(subSrc);
                    builder.appendChar('\n');
                    std::string sourceCodePosition;
                    for (size_t i = preLineSoFar; i < start; i++) {
                        sourceCodePosition += " ";
                    }
                    sourceCodePosition += "^";
                    builder.appendString(String::fromUTF8(sourceCodePosition.data(), sourceCodePosition.length()));
                }
            }
        }

        if (i != gcValues.size() - 1) {
            builder.appendChar('\n');
        }
    }
}

void SandBox::fillStackDataIntoErrorObject(const Value& e)
{
    if (e.isObject() && e.asObject()->isErrorObject()) {
        ErrorObject* obj = e.asObject()->asErrorObject();
        ErrorObject::StackTraceData* data = ErrorObject::StackTraceData::create(this);
        obj->setStackTraceData(data);

        ExecutionState state(m_context);
        JSGetterSetter gs(
            new FunctionObject(state, NativeFunctionInfo(m_context->staticStrings().stack, builtinErrorObjectStackInfo, 0, nullptr, NativeFunctionInfo::Strict)),
            Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        obj->defineOwnProperty(state, ObjectPropertyName(m_context->staticStrings().stack), desc);
    }
}
}
