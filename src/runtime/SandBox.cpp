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
            result.stackTraceData.pushBack(m_stackTraceData[i].second);
        }
    }
    return result;
}

void SandBox::throwException(ExecutionState& state, Value exception)
{
    m_exception = exception;
    throw exception;
}

void SandBox::fillStackDataIntoErrorObject(const Value& e)
{
    if (e.isObject() && e.asObject()->isErrorObject()) {
        SandBox sb(m_context);
        ExecutionState state(m_context);
        ErrorObject* obj = e.asObject()->asErrorObject();
        sb.run([&]() -> Value {
            StringBuilder builder;

            auto getResult = obj->get(state, state.context()->staticStrings().name);
            if (getResult.hasValue()) {
                builder.appendString(getResult.value(state, obj).toString(state));
                builder.appendString(": ");
            }
            getResult = obj->get(state, state.context()->staticStrings().message);
            if (getResult.hasValue()) {
                builder.appendString(getResult.value(state, obj).toString(state));
                if (m_stackTraceData.size()) {
                    builder.appendChar('\n');
                }
            }
            for (size_t i = 0; i < m_stackTraceData.size(); i++) {
                builder.appendString("at ");
                builder.appendString(m_stackTraceData[i].second.fileName);
                builder.appendChar(':');
                builder.appendString(String::fromDouble(m_stackTraceData[i].second.loc.line));
                builder.appendChar(':');
                builder.appendString(String::fromDouble(m_stackTraceData[i].second.loc.column));
                if (i != m_stackTraceData.size() - 1) {
                    builder.appendChar('\n');
                }
            }
            obj->defineOwnProperty(state, state.context()->staticStrings().stack, ObjectPropertyDescriptor(builder.finalize(), ObjectPropertyDescriptor::AllPresent));
            return Value();
        });
    }
}
}
