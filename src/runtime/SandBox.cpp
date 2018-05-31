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
        result.msgStr = result.error.toString(state);

        fillStackDataIntoErrorObject(err);

        for (size_t i = 0; i < m_stackTraceData.size(); i++) {
            if ((size_t)m_stackTraceData[i].second.loc.index == SIZE_MAX && (size_t)m_stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
                // this means loc not computed yet.
                ExtendedNodeLOC loc = m_stackTraceData[i].second.loc.actualCodeBlock->computeNodeLOCFromByteCode(m_context,
                                                                                                                 m_stackTraceData[i].second.loc.byteCodePosition, m_stackTraceData[i].second.loc.actualCodeBlock->m_codeBlock);
                StackTraceData traceData;
                traceData.loc = loc;
                traceData.fileName = m_stackTraceData[i].second.loc.actualCodeBlock->m_codeBlock->script()->fileName();
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "get DataView.prototype.byteLength called on incompatible receiver");
    }

    ErrorObject* obj = thisValue.asObject()->asErrorObject();
    if (obj->stackTraceData() == nullptr) {
        return String::emptyString;
    }

    auto stackTraceData = obj->stackTraceData();
    StringBuilder builder;

    auto getResult = obj->get(state, state.context()->staticStrings().name);
    if (getResult.hasValue()) {
        builder.appendString(getResult.value(state, obj).toString(state));
        builder.appendString(": ");
    }
    getResult = obj->get(state, state.context()->staticStrings().message);
    if (getResult.hasValue()) {
        builder.appendString(getResult.value(state, obj).toString(state));
        builder.appendChar('\n');
    }
    for (size_t i = 0; i < stackTraceData->gcValues.size(); i++) {
        builder.appendString("at ");
        if (stackTraceData->nonGCValues[i].byteCodePosition == SIZE_MAX) {
            builder.appendString(stackTraceData->gcValues[i].infoString);
        } else {
            ExtendedNodeLOC loc = stackTraceData->gcValues[i].byteCodeBlock->computeNodeLOCFromByteCode(state.context(),
                                                                                                        stackTraceData->nonGCValues[i].byteCodePosition, stackTraceData->gcValues[i].byteCodeBlock->m_codeBlock);
            builder.appendString(stackTraceData->gcValues[i].byteCodeBlock->m_codeBlock->script()->fileName());
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.line));
            builder.appendChar(':');
            builder.appendString(String::fromDouble(loc.column));
        }

        if (i != stackTraceData->gcValues.size() - 1) {
            builder.appendChar('\n');
        }
    }
    return builder.finalize();
}

void SandBox::fillStackDataIntoErrorObject(const Value& e)
{
    if (e.isObject() && e.asObject()->isErrorObject()) {
        ErrorObject* obj = e.asObject()->asErrorObject();
        ErrorObject::StackTraceData* data = new ErrorObject::StackTraceData();

        data->gcValues.resizeWithUninitializedValues(m_stackTraceData.size());
        data->nonGCValues.resizeWithUninitializedValues(m_stackTraceData.size());

        for (size_t i = 0; i < m_stackTraceData.size(); i++) {
            if ((size_t)m_stackTraceData[i].second.loc.index == SIZE_MAX && (size_t)m_stackTraceData[i].second.loc.actualCodeBlock != SIZE_MAX) {
                data->gcValues[i].byteCodeBlock = m_stackTraceData[i].second.loc.actualCodeBlock;
                data->nonGCValues[i].byteCodePosition = m_stackTraceData[i].second.loc.byteCodePosition;
            } else {
                data->gcValues[i].infoString = m_stackTraceData[i].second.fileName;
                data->nonGCValues[i].byteCodePosition = SIZE_MAX;
            }
        }

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
