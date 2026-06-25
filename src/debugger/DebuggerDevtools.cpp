/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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
#include <memory>
#include <string>
#include <stdexcept>
#include <fstream>

#include "Escargot.h"
#include "DebuggerTcp.h"
#include "DebuggerDevtools.h"
#include "DebuggerHttpRouter.h"
#include "interpreter/ByteCode.h"
#include "parser/Script.h"
#include "HeapSnapshot.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#ifdef ESCARGOT_DEBUGGER

namespace Escargot {

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
    const int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    const auto size = static_cast<size_t>(size_s);
    const std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return { buf.get(), buf.get() + size - 1 }; // We don't want the '\0' inside
}


rapidjson::Value stringToRapidjsonValue(const std::string& value, rapidjson::MemoryPoolAllocator<>& allocator)
{
    rapidjson::Value rapidjsonStringValue(rapidjson::kStringType);
    rapidjsonStringValue.SetString(value.c_str(), value.length(), allocator);
    return rapidjsonStringValue;
}

bool DebuggerDevtools::sendMessage(const std::string& msg, const size_t length)
{
    if (UNLIKELY(!m_networkEnabled || !m_debuggerEnabled || !m_runtimeEnabled)) {
        m_pendingMessages.emplace_back(msg);
        return true;
    }

    const bool result = send(0, msg.c_str(), length == static_cast<size_t>(-1) ? msg.length() : length);
    if (result) {
        ESCARGOT_LOG_INFO("Sent message: %s\n", msg.c_str());
    } else {
        ESCARGOT_LOG_ERROR("Error sending message!");
    }
    return result;
}

bool DebuggerDevtools::sendJSONDocument(const rapidjson::Document& document)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    document.Accept(writer);
    const char* jsonReplyString = sb.GetString();

    return sendMessage(jsonReplyString);
}

void DebuggerDevtools::init(const char* options, Context* context)
{
    const char* msg = "{\"method\":\"Runtime.executionContextCreated\",\"params\":{"
                      "\"context\":{"
                      "\"id\":1,"
                      "\"origin\":\"\","
                      "\"name\":\"escargot\","
                      "\"auxData\":{\"isDefault\":true}"
                      "}"
                      "}}";

    sendMessage(msg);
}

bool DebuggerDevtools::skipSourceCode(String* srcName) const
{
    ESCARGOT_LOG_INFO("Implement this: DebuggerDevtools::skipSourceCode\n");
    return false;
}

uint8_t DebuggerDevtools::registerScript(String* source, String* srcName)
{
    std::string url(srcName->toUTF8StringData().data(), srcName->toUTF8StringData().size());
    const auto it = m_scriptIdByUrl.find(url);
    if (it != m_scriptIdByUrl.end()) {
        return it->second;
    }

    uint8_t newId = m_nextScriptId++;

    m_scriptsById.emplace(newId, ScriptInfo{ newId, srcName, source });
    m_scriptIdByUrl.emplace(url, newId);

    return newId;
}

static void computeEndLocation(const char* src, size_t length, uint32_t& endLine, uint32_t& endColumn)
{
    for (size_t i = 0; i < length; i++) {
        if (src[i] == '\n') {
            endLine++;
            endColumn = 0;
        } else {
            endColumn++;
        }
    }
}

void DebuggerDevtools::parseCompleted(String* source, String* srcName, const size_t originLineOffset, String* error)
{
    if (!enabled() || m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        return;
    }

    if (!source || !srcName || !source->is8Bit() || !srcName->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return;
    }

    const uint8_t scriptId = registerScript(source, srcName);
    std::set<BreakpointByteCodeLocation, decltype(compareBreakpointLocations)*> breakpointLocationsSet(compareBreakpointLocations);
    m_breakpointInfo.emplace(scriptId, breakpointLocationsSet);

    const size_t breakpointLocationsSize = m_breakpointLocationsVector.size();

    if (originLineOffset > 0) {
        for (size_t i = 0; i < breakpointLocationsSize; i++) {
            // adjust line offset for manipulated source code
            // inserted breakpoint's line info should be bigger than `originLineOffset`
            BreakpointLocationVector& locationVector = m_breakpointLocationsVector[i]->breakpointLocations;
            for (auto& j : locationVector) {
                ASSERT(j.line > originLineOffset);
                j.line -= originLineOffset;
            }
        }
    }

    for (size_t i = 0; i < breakpointLocationsSize; i++) {
        /* function bytecode information */
        InterpretedCodeBlock* codeBlock = reinterpret_cast<InterpretedCodeBlock*>(m_breakpointLocationsVector[i]->weakCodeRef);
        uint8_t* byteCodeStart = codeBlock->byteCodeBlock()->m_code.data();

        /* save breakpoint locations. */
        BreakpointLocationVector breakpointLocations = m_breakpointLocationsVector[i]->breakpointLocations;
        for (auto& breakpointLocation : breakpointLocations) {
            auto b = BreakpointByteCodeLocation(breakpointLocation.line, reinterpret_cast<ByteCode*>(byteCodeStart + breakpointLocation.offset));
            b.byteCode->m_loc.line = breakpointLocation.line;
            m_breakpointInfo[scriptId].insert(b);
        }
    }

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("method", "Debugger.scriptParsed", reply.GetAllocator());
    reply.AddMember("params", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());

    reply["params"].AddMember("scriptId", stringToRapidjsonValue(string_format("%d", scriptId), reply.GetAllocator()), reply.GetAllocator());

    const std::string url(srcName->toUTF8StringData().data(), srcName->toUTF8StringData().size());
    reply["params"].AddMember("url", stringToRapidjsonValue(url, reply.GetAllocator()), reply.GetAllocator());

    reply["params"].AddMember("startLine", 0, reply.GetAllocator());
    reply["params"].AddMember("startColumn", 0, reply.GetAllocator());

    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    computeEndLocation(source->toUTF8StringData().data(), source->toUTF8StringData().length(), endLine, endColumn);
    reply["params"].AddMember("endLine", endLine, reply.GetAllocator());
    reply["params"].AddMember("endColumn", endColumn, reply.GetAllocator());

    reply["params"].AddMember("executionContextId", 1, reply.GetAllocator());

    sendJSONDocument(reply);
}

static void addObjectProperties(ExecutionState* state, PropertyNameValueMap* values, Object* object)
{
    Object::OwnPropertyKeyVector keys = object->ownPropertyKeys(*state);
    for (const auto key : keys) {
        ObjectPropertyName propertyName(*state, key);
        AtomicString name(*state, key.toStringWithoutException(*state));

        try {
            ObjectGetResult result = object->getOwnProperty(*state, propertyName);
            Value value = result.value(*state, value);

            if (values->find(name) == values->end()) {
                values->insert(std::make_pair(name, value));
            }
        } catch (const Value& val) {
            if (values->find(name) == values->end()) {
                values->insert(std::make_pair(name, Value()));
            }
        }
    }
}

static void addRecordProperties(ExecutionState* state, PropertyNameValueMap* values, const IdentifierRecordVector& identifiers, EnvironmentRecord* record)
{
    for (const auto identifier : identifiers) {
        try {
            const EnvironmentRecord::GetBindingValueResult result = record->getBindingValue(*state, identifier.m_name);
            ASSERT(result.m_hasBindingValue);
            if (values->find(identifier.m_name) == values->end()) {
                values->insert(std::make_pair(identifier.m_name, result.m_value));
            }
        } catch (const Value& val) {
            if (values->find(identifier.m_name) == values->end()) {
                values->insert(std::make_pair(identifier.m_name, Value()));
            }
        }
    }
}

static void addRecordProperties(ExecutionState* state, PropertyNameValueMap* values, const ModuleEnvironmentRecord::ModuleBindingRecordVector& bindings, ModuleEnvironmentRecord* record)
{
    for (const auto binding : bindings) {
        try {
            EnvironmentRecord::GetBindingValueResult result = record->getBindingValue(*state, binding.m_localName);
            ASSERT(result.m_hasBindingValue);
            if (values->find(binding.m_localName) == values->end()) {
                values->insert(std::make_pair(binding.m_localName, result.m_value));
            }
        } catch (const Value& val) {
            if (values->find(binding.m_localName) == values->end()) {
                values->insert(std::make_pair(binding.m_localName, Value()));
            }
        }
    }
}

void DebuggerDevtools::sendPausedEvent(ByteCodeBlock* byteCodeBlock, const uint32_t offset, ExecutionState* state, const bool breakpoint)
{
    const auto* stopByteCode = reinterpret_cast<ByteCode*>(byteCodeBlock->m_code.data() + offset);
    const std::string stopFilename = byteCodeBlock->codeBlock()->script()->srcName()->toUTF8StringData().data();
    const uint64_t stopLine = stopByteCode->m_loc.line - 1; // chrome starts line indexes at 0
    const uint64_t stopColumn = stopByteCode->m_loc.column - 1; // chrome starts column indexes at 0

    m_objectIdVectorIndexOffset += m_propertyMapsById.size();
    m_propertyMapsById.clear();

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("method", "Debugger.paused", reply.GetAllocator());
    reply.AddMember("params", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
    reply["params"].AddMember("callFrames", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());
    reply["params"].AddMember("hitBreakpoints", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());
    reply["params"]["hitBreakpoints"].PushBack(stringToRapidjsonValue(string_format("%s:%lu:%lu", stopFilename.c_str(), stopLine, stopColumn), reply.GetAllocator()), reply.GetAllocator());
    reply["params"].AddMember("reason", stringToRapidjsonValue(breakpoint ? "Breakpoint" : "Break on start", reply.GetAllocator()), reply.GetAllocator());

    StackTraceDataOnStackVector stackTraceDataVector;
    SandBox::createStackTrace(stackTraceDataVector, *state, true);
    ByteCodeLOCDataMap locMap;
    bool first = true;

    for (auto stackTraceData : stackTraceDataVector) {
        const std::string currentFunctionNameStr = stackTraceData.functionName->toUTF8StringData().data();
        const uint8_t scripId = m_scriptIdByUrl[stackTraceData.srcName->toUTF8StringData().data()];

        uint64_t line, column;
        ByteCodeBlock* stackTraceByteCodeBlock = stackTraceData.loc.actualCodeBlock;

        if (stackTraceData.loc.index == SIZE_MAX) {
            size_t byteCodePosition = stackTraceData.loc.byteCodePosition;

            ByteCodeLOCData* locData;
            auto iterMap = locMap.find(stackTraceByteCodeBlock);
            if (iterMap == locMap.end()) {
                locData = new ByteCodeLOCData();
                locMap.insert(std::make_pair(stackTraceByteCodeBlock, locData));
            } else {
                locData = iterMap->second;
            }

            ExtendedNodeLOC loc = stackTraceByteCodeBlock->computeNodeLOCFromByteCode(state->context(), byteCodePosition, stackTraceByteCodeBlock->m_codeBlock, locData);
            line = loc.line - 1; // chrome starts line indexes at 0
            column = loc.column - 1; // chrome starts column indexes at 0
        } else {
            line = stackTraceData.loc.line - 1; // chrome starts line indexes at 0
            column = stackTraceData.loc.column - 1; // chrome starts column indexes at 0
        }

        rapidjson::Value callFrameObject(rapidjson::kObjectType);

        callFrameObject.AddMember("callFrameId", stringToRapidjsonValue(string_format("%d", m_nextCallFrameId++), reply.GetAllocator()), reply.GetAllocator());
        callFrameObject.AddMember("functionName", stringToRapidjsonValue(currentFunctionNameStr, reply.GetAllocator()), reply.GetAllocator());
        callFrameObject.AddMember("location", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
        callFrameObject["location"].AddMember("scriptId", stringToRapidjsonValue(string_format("%d", scripId), reply.GetAllocator()), reply.GetAllocator());
        callFrameObject["location"].AddMember("lineNumber", line, reply.GetAllocator());
        callFrameObject["location"].AddMember("columnNumber", column, reply.GetAllocator());
        callFrameObject.AddMember("scopeChain", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());
        callFrameObject.AddMember("canBeRestarted", false, reply.GetAllocator());

        auto* blockValues = new (GC) PropertyNameValueMap();
        auto* localValues = new (GC) PropertyNameValueMap();
        auto* globalValues = new (GC) PropertyNameValueMap();
        auto* moduleValues = new (GC) PropertyNameValueMap();

        LexicalEnvironment* lexEnv;
        if (first) {
            lexEnv = state->lexicalEnvironment();
            first = false;
        } else {
            lexEnv = stackTraceData.lexicalEnvironment;
        }
        while (lexEnv) {
            EnvironmentRecord* envRecord = lexEnv->record();

            if (envRecord->isGlobalEnvironmentRecord()) {
                GlobalEnvironmentRecord* global = envRecord->asGlobalEnvironmentRecord();

                addRecordProperties(stackTraceData.executionState, globalValues, *global->m_globalDeclarativeRecord, envRecord);
                addObjectProperties(stackTraceData.executionState, globalValues, global->m_globalObject);
            } else if (envRecord->isDeclarativeEnvironmentRecord()) {
                DeclarativeEnvironmentRecord* declarativeRecord = envRecord->asDeclarativeEnvironmentRecord();
                if (declarativeRecord->isFunctionEnvironmentRecord()) {
                    IdentifierRecordVector* identifierRecordVector = declarativeRecord->asFunctionEnvironmentRecord()->getRecordVector();

                    if (identifierRecordVector != nullptr) {
                        addRecordProperties(stackTraceData.executionState, localValues, *identifierRecordVector, envRecord);
                    }
                } else if (envRecord->isModuleEnvironmentRecord()) {
                    addRecordProperties(stackTraceData.executionState, moduleValues, envRecord->asModuleEnvironmentRecord()->moduleBindings(), envRecord->asModuleEnvironmentRecord());
                } else if (declarativeRecord->isDeclarativeEnvironmentRecordNotIndexed()) {
                    DeclarativeEnvironmentRecordNotIndexed* record = declarativeRecord->asDeclarativeEnvironmentRecordNotIndexed();
                    addRecordProperties(stackTraceData.executionState, localValues, record->m_recordVector, envRecord);
                }
            } else if (envRecord->isObjectEnvironmentRecord()) {
                addObjectProperties(stackTraceData.executionState, blockValues, envRecord->asObjectEnvironmentRecord()->bindingObject());
            }

            lexEnv = lexEnv->outerEnvironment();
        }

        typedef std::pair<PropertyNameValueMap*, std::string> TypedPropertyDataMap;
        for (const auto& typedValueMap : {
                 // Possible scope types: [ "global", "local", "with", "closure", "catch", "block", "script", "eval", "module", "wasm-expression-stack" ]
                 TypedPropertyDataMap(blockValues, "block"),
                 TypedPropertyDataMap(localValues, "local"),
                 TypedPropertyDataMap(globalValues, "global"),
                 TypedPropertyDataMap(moduleValues, "module") }) {
            if (typedValueMap.first->empty()) {
                continue;
            }
            rapidjson::Value scopeChain(rapidjson::kObjectType);
            scopeChain.AddMember("object", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());

            scopeChain["object"].AddMember("type", stringToRapidjsonValue("object", reply.GetAllocator()), reply.GetAllocator());
            scopeChain["object"].AddMember("objectId", stringToRapidjsonValue(string_format("%d", registerValuesMap(typedValueMap.first)), reply.GetAllocator()), reply.GetAllocator());
            scopeChain.AddMember("type", stringToRapidjsonValue(typedValueMap.second, reply.GetAllocator()), reply.GetAllocator());
            callFrameObject["scopeChain"].PushBack(scopeChain, reply.GetAllocator());
        }

        reply["params"]["callFrames"].PushBack(callFrameObject, reply.GetAllocator());
    }

    sendJSONDocument(reply);
}

bool DebuggerDevtools::stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, const uint32_t offset, ExecutionState* state)
{
    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        m_delay--;
        if (m_delay == 0) {
            processEvents(state, byteCodeBlock);
        }
        return false;
    }

    sendPausedEvent(byteCodeBlock, offset, state, !m_startBreakpoint);

    if (m_startBreakpoint) {
        m_startBreakpoint = false;
    }

    if (!enabled()) {
        return false;
    }

    ASSERT(m_activeObjects.empty());
    m_stopState = ESCARGOT_DEBUGGER_IN_WAIT_MODE;

    while (processEvents(state, byteCodeBlock))
        ;

    m_activeObjects.clear();
    m_delay = ESCARGOT_DEBUGGER_MESSAGE_PROCESS_DELAY;

    return false;
}

void DebuggerDevtools::byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock)
{
}

void DebuggerDevtools::exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace)
{
}

void DebuggerDevtools::consoleOut(String* output)
{
    rapidjson::Document message;
    message.SetObject();

    message.AddMember("method", "Runtime.consoleAPICalled", message.GetAllocator());
    message.AddMember("params", rapidjson::Value(rapidjson::kObjectType), message.GetAllocator());
    message["params"].AddMember("args", rapidjson::kArrayType, message.GetAllocator());

    auto remoteObject = rapidjson::Value(rapidjson::kObjectType);
    remoteObject.AddMember("type", "string", message.GetAllocator());
    const auto value = std::string(output->toUTF8StringData().data(), output->toUTF8StringData().length());
    remoteObject.AddMember("value", stringToRapidjsonValue(value, message.GetAllocator()), message.GetAllocator());
    message["params"]["args"].PushBack(remoteObject, message.GetAllocator());

    message["params"].AddMember("type", "info", message.GetAllocator());
    message["params"].AddMember("executionContextId", 1, message.GetAllocator());
    message["params"].AddMember("timestamp", 0, message.GetAllocator());

    sendJSONDocument(message);
}

String* DebuggerDevtools::getClientSource(String** sourceName)
{
    return nullptr;
}

bool DebuggerDevtools::getWaitBeforeExitClient()
{
    while (processEvents(nullptr, nullptr))
        ;
    return true;
}

bool DebuggerDevtools::resume(rapidjson::Document& jsonMessage)
{
    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }
    m_stopState = nullptr;

    replyOK(jsonMessage);
    return false;
}

bool DebuggerDevtools::stepInto(rapidjson::Document& jsonMessage)
{
    if (m_stopState == ESCARGOT_DEBUGGER_IN_EVAL_MODE) {
        return true;
    }
    m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;

    replyOK(jsonMessage);
    return false;
}

bool DebuggerDevtools::stepOver(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }
    m_stopState = state;

    replyOK(jsonMessage);
    return false;
}

bool DebuggerDevtools::stepOut(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    if (m_stopState != ESCARGOT_DEBUGGER_IN_WAIT_MODE) {
        return true;
    }

    const LexicalEnvironment* lexEnv = getFunctionLexEnv(state);

    if (!lexEnv) {
        m_stopState = nullptr;
        return false;
    }

    ExecutionState* stopState = state->parent();

    while (stopState && getFunctionLexEnv(stopState) == lexEnv) {
        stopState = stopState->parent();
    }

    m_stopState = stopState;
    return false;
}

uint32_t DebuggerDevtools::registerValuesMap(PropertyNameValueMap* newPropertyMap)
{
    m_propertyMapsById.push_back(newPropertyMap);
    return m_nextObjectId++;
}

std::string objectToStringTypeName(const Value object)
{
    std::string objectType;
    if (object.isSymbol()) {
        objectType = "symbol";
    } else if (object.isFunction()) {
        objectType = "function";
    } else if (object.isUndefined()) {
        objectType = "undefined";
    } else if (object.isString()) {
        objectType = "string";
    } else if (object.isNumber()) {
        objectType = "number";
    } else if (object.isBoolean()) {
        objectType = "boolean";
    } else if (object.isBigInt()) {
        objectType = "bigint";
    } else if (object.isObject()) {
        objectType = "object";
    } else {
        ASSERT_NOT_REACHED();
    }
    return objectType;
}

bool DebuggerDevtools::sendProperties(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    if (jsonMessage["params"]["ownProperties"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: getProperties: parameter 'ownProperties' is not supported!\n");
    }
    if (jsonMessage["params"]["accessorPropertiesOnly"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: getProperties: parameter 'accessorPropertiesOnly' is not supported!\n");
    }
    if (!jsonMessage["params"]["nonIndexedPropertiesOnly"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: getProperties: sending indexed properties is not supported!\n");
    }
    if (jsonMessage["params"]["generatePreview"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: getProperties: parameter 'generatePreview' is not supported!\n");
    }

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"].GetInt(), reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());

    std::string objectIdStr = jsonMessage["params"]["objectId"].GetString();
    uint32_t objectId;
    try {
        objectId = stoi(objectIdStr);
    } catch (const std::exception& e) {
        return sendJSONDocument(reply);
    }

    if (objectId < m_objectIdVectorIndexOffset) {
        return sendJSONDocument(reply);
    }

    reply["result"].AddMember("result", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());
    PropertyNameValueMap* properties = m_propertyMapsById[objectId - m_objectIdVectorIndexOffset];

    for (const auto& property : *properties) {
        const std::string& propertyName = property.first.string()->toUTF8StringData().data();
        const Value& propertyValue = property.second;

        rapidjson::Value propertyObject(rapidjson::kObjectType);
        propertyObject.SetObject();

        rapidjson::Value propertyNameStringValue = stringToRapidjsonValue(propertyName, reply.GetAllocator());

        propertyObject.AddMember("name", propertyNameStringValue, reply.GetAllocator());
        propertyObject.AddMember("configurable", true, reply.GetAllocator());
        propertyObject.AddMember("enumerable", true, reply.GetAllocator());
        propertyObject.AddMember("symbol", propertyValue.isSymbol(), reply.GetAllocator());

        propertyObject.AddMember("value", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());

        rapidjson::Value propertyValueString;
        if (propertyValue.isSymbol()) {
            propertyValueString = stringToRapidjsonValue(string_format("Symbol (%s)", propertyName.c_str()), reply.GetAllocator());
        } else {
            propertyValueString = stringToRapidjsonValue(propertyValue.toStringWithoutException(*state)->toUTF8StringData().data(), reply.GetAllocator());
        }

        propertyObject["value"].AddMember("type", stringToRapidjsonValue(objectToStringTypeName(propertyValue), reply.GetAllocator()), reply.GetAllocator());
        if (propertyValue.isObject()) {
            propertyObject["value"].AddMember("className", stringToRapidjsonValue(propertyValue.asObject()->constructorName(*state)->toUTF8StringData().data(), reply.GetAllocator()), reply.GetAllocator());
        }
        propertyObject["value"].AddMember("value", propertyValueString, reply.GetAllocator());
        propertyObject["value"].AddMember("description", propertyValueString, reply.GetAllocator()); // string representation of the object

        if (propertyValue.isObject()) {
            auto* values = new (GC) PropertyNameValueMap();
            addObjectProperties(state, values, propertyValue.asObject());
            propertyObject["value"].AddMember("objectId", stringToRapidjsonValue(string_format("%d", registerValuesMap(values)), reply.GetAllocator()), reply.GetAllocator());
        }

        reply["result"]["result"].PushBack(propertyObject, reply.GetAllocator());
    }

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::sendSourceCode(rapidjson::Document& jsonMessage)
{
    const uint32_t scriptId = std::stoi(jsonMessage["params"]["scriptId"].GetString());

    const auto it = m_scriptsById.find(scriptId);
    if (it == m_scriptsById.end()) {
        return false;
    }

    const String* source = it->second.source;

    if (!source->is8Bit()) {
        ESCARGOT_LOG_ERROR("Only 8 bit characters are supported right now...");
        return false;
    }

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"].GetUint(), reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
    reply["result"].AddMember("scriptSource", stringToRapidjsonValue(source->toUTF8StringData().data(), reply.GetAllocator()), reply.GetAllocator());

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::replyOK(rapidjson::Document& jsonMessage)
{
    const std::string jsonReplyString = string_format(R"({"id": %d, "result": {}})",
                                                      jsonMessage["id"].GetInt());

    sendMessage(jsonReplyString, jsonReplyString.length());
    return true;
}

bool DebuggerDevtools::enableNetwork(rapidjson::Document& jsonMessage)
{
    this->m_networkEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableDebugger(rapidjson::Document& jsonMessage)
{
    this->m_debuggerEnabled = true;

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"], reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
    reply["result"].AddMember("debuggerId", stringToRapidjsonValue(string_format("escargot-%d", rand() % 1000000), reply.GetAllocator()), reply.GetAllocator());

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::enableRuntime(rapidjson::Document& jsonMessage)
{
    this->m_runtimeEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::enableProfiler(rapidjson::Document& jsonMessage)
{
    this->m_profilerEnabled = true;
    return replyOK(jsonMessage);
}

bool DebuggerDevtools::setPauseOnExceptions(rapidjson::Document& jsonMessage)
{
    // TODO: handdle other parameters: state: none (unset), uncaught, caught, all
    this->m_pauseOnExceptions = true;
    return replyOK(jsonMessage);
}

void setBreakpointState(ByteCode* breakpoint, const bool enabled)
{
#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
    if (enabled) {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointDisabledOpcode]) {
            return;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointEnabledOpcode];
    } else {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointEnabledOpcode]) {
            return;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointDisabledOpcode];
    }
#else
    if (enabled) {
        if (breakpoint->m_opcode != BreakpointDisabledOpcode) {
            return;
        }
        breakpoint->m_opcode = BreakpointEnabledOpcode;
    } else {
        if (breakpoint->m_opcode != BreakpointEnabledOpcode) {
            return;
        }
        breakpoint->m_opcode = BreakpointDisabledOpcode;
    }
#endif
}

bool DebuggerDevtools::setBreakpointsActive(rapidjson::Document& jsonMessage)
{
    this->m_breakpointsActive = jsonMessage["params"]["active"].GetBool();

    for (ByteCode* breakpointBytecode : m_setBreakPoints) {
        setBreakpointState(breakpointBytecode, m_breakpointsActive);
    }

    return replyOK(jsonMessage);
}

bool DebuggerDevtools::setBreakpointByUrl(rapidjson::Document& jsonMessage)
{
    const std::string breakpointCondition = jsonMessage["params"]["condition"].GetString();
    if (!breakpointCondition.empty()) {
        ESCARGOT_LOG_ERROR("Warning: Breakpoint conditions are not supported!");
    }

    std::string breakpointFile = jsonMessage["params"]["url"].GetString();
    if (breakpointFile.find("file://") == 0) {
        breakpointFile.erase(0, 7);
    }

    const uint32_t lineNumber = jsonMessage["params"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t columnNumber = jsonMessage["params"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0

    rapidjson::Document reply;
    reply.SetObject();

    const uint8_t scriptId = this->m_scriptIdByUrl[breakpointFile];
    rapidjson::Value scriptIdValue = stringToRapidjsonValue(string_format("%d", scriptId), reply.GetAllocator());

    reply.AddMember("id", jsonMessage["id"].GetInt(), reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
    reply["result"].AddMember("locations", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if ((lineNumber == breakpointInfo.line && (columnNumber == breakpointInfo.byteCode->m_loc.column || columnNumber == 1))
            || (reply["result"]["locations"].Empty() && breakpointInfo.line > lineNumber)) {
            rapidjson::Value locationObject(rapidjson::kObjectType);
            locationObject.AddMember("scriptId", scriptIdValue, reply.GetAllocator());
            locationObject.AddMember("lineNumber", breakpointInfo.line - 1, reply.GetAllocator());
            locationObject.AddMember("columnNumber", breakpointInfo.byteCode->m_loc.column - 1, reply.GetAllocator());
            reply["result"]["locations"].PushBack(locationObject, reply.GetAllocator());

            std::string breakpointIdString = string_format("%s:%d:%lu", breakpointFile.c_str(), breakpointInfo.line, breakpointInfo.byteCode->m_loc.column);
            reply["result"].AddMember("breakpointId", stringToRapidjsonValue(breakpointIdString, reply.GetAllocator()), reply.GetAllocator());

            /* Enable breakpoint */
            setBreakpointState(breakpointInfo.byteCode, true);
            m_setBreakPoints.emplace(breakpointInfo.byteCode);
            break;
        }
    }

    const auto ret = sendJSONDocument(reply);
    return ret;
}

bool DebuggerDevtools::removeBreakpoint(rapidjson::Document& jsonMessage)
{
    const std::string breakpointId = jsonMessage["params"]["breakpointId"].GetString();

    std::stringstream ss(breakpointId);
    std::string breakpointIdInfo[3];
    constexpr char delim = ':';
    int i = 0;

    while (std::getline(ss, breakpointIdInfo[i], delim)) {
        ++i;
    }
    ASSERT(i == 3);

    const std::string breakpointFile = breakpointIdInfo[0];
    const uint32_t lineNumber = stoi(breakpointIdInfo[1]);
    const uint32_t columnNumber = stoi(breakpointIdInfo[2]);
    const uint8_t scriptId = this->m_scriptIdByUrl[breakpointFile];

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if (lineNumber == breakpointInfo.line && columnNumber == breakpointInfo.byteCode->m_loc.column) {
            /* Disable breakpoint */
            setBreakpointState(breakpointInfo.byteCode, false);
            m_setBreakPoints.erase(breakpointInfo.byteCode);
            break;
        }
    }

    return replyOK(jsonMessage);
}

bool DebuggerDevtools::sendPossibleBreakpoints(rapidjson::Document& jsonMessage)
{
    if (jsonMessage["params"]["restrictToFunction"].GetBool()) {
        ESCARGOT_LOG_ERROR("Warning: restrictToFunction is not supported\n");
    }

    std::string scriptIdString = jsonMessage["params"]["start"]["scriptId"].GetString();
    const uint8_t scriptId = std::stoi(scriptIdString);

    if (m_breakpointInfo.find(scriptId) == m_breakpointInfo.end()) {
        ESCARGOT_LOG_ERROR("Script Id not found: %d", scriptId);
        return replyOK(jsonMessage);
    }

    if (scriptId != std::stoi(jsonMessage["params"]["end"]["scriptId"].GetString())) {
        ESCARGOT_LOG_ERROR("Error: Script ranges across multiple scripts not supported!\n");
        return replyMethodNotFound(jsonMessage);
    }

    const uint32_t startLine = jsonMessage["params"]["start"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t startColumn = jsonMessage["params"]["start"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0
    const uint32_t endLine = jsonMessage["params"]["end"]["lineNumber"].GetUint() + 1; // chrome starts line indexes at 0
    const uint32_t endColumn = jsonMessage["params"]["end"]["columnNumber"].GetUint() + 1; // chrome starts column indexes at 0

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"].GetInt(), reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());
    reply["result"].AddMember("locations", rapidjson::Value(rapidjson::kArrayType), reply.GetAllocator());

    rapidjson::Value scriptIdValue = stringToRapidjsonValue(string_format("%d", scriptId), reply.GetAllocator());

    for (BreakpointByteCodeLocation breakpointInfo : m_breakpointInfo[scriptId]) {
        if (((startLine <= breakpointInfo.line && breakpointInfo.line <= endLine)
             && startColumn <= breakpointInfo.byteCode->m_loc.column && breakpointInfo.byteCode->m_loc.column <= endColumn)
            || (reply["result"]["locations"].Empty() && breakpointInfo.line > startLine && breakpointInfo.line > endLine)) {
            rapidjson::Value locationObject(rapidjson::kObjectType);

            locationObject.AddMember("scriptId", scriptIdValue, reply.GetAllocator());
            locationObject.AddMember("lineNumber", breakpointInfo.line - 1, reply.GetAllocator());
            locationObject.AddMember("columnNumber", breakpointInfo.byteCode->m_loc.column - 1, reply.GetAllocator());

            reply["result"]["locations"].PushBack(locationObject, reply.GetAllocator());
        }
    }

    return sendJSONDocument(reply);
}

bool DebuggerDevtools::takeHeapSnapshot(rapidjson::Document& jsonMessage, ExecutionState* state)
{
    HeapSnapshot snapshot;
    std::string fileName = snapshot.takeHeapSnapshot(state);

    if (fileName.empty()) {
        ESCARGOT_LOG_ERROR("Error: Error happened during heap snapshot creation. Aborting now.\n");
        abort();
    }
    // DevTools just sends done and total messages, then adds "finished" before sending the snapshot
    // Since we dont record progress, just send a finished and then the snapshot chunks
    // Chrome might change this in the future
    rapidjson::Document progressReport;
    progressReport.SetObject();

    progressReport.AddMember("method", "HeapProfiler.reportHeapSnapshotProgress", progressReport.GetAllocator());
    progressReport.AddMember("params", rapidjson::Value(rapidjson::kObjectType), progressReport.GetAllocator());
    progressReport["params"].AddMember("done", 0, progressReport.GetAllocator());
    progressReport["params"].AddMember("total", 0, progressReport.GetAllocator());
    progressReport["params"].AddMember("finished", true, progressReport.GetAllocator());
    sendJSONDocument(progressReport);

    std::ifstream file(fileName, std::ios::in);
    if (!file.is_open()) {
        ESCARGOT_LOG_ERROR("ERROR: Could not open snapshot file. Aborting now.\n");
        abort();
    }

    const uint16_t bufferSize = 4096 * 4;
    char buffer[bufferSize];
    while (!file.eof()) {
        file.read(buffer, bufferSize);

        rapidjson::Document snapshotChunk;
        snapshotChunk.SetObject();

        snapshotChunk.AddMember("method", "HeapProfiler.addHeapSnapshotChunk", snapshotChunk.GetAllocator());
        snapshotChunk.AddMember("params", rapidjson::Value(rapidjson::kObjectType), snapshotChunk.GetAllocator());
        auto chunkStr = std::string(buffer, file.gcount());
        snapshotChunk["params"].AddMember("chunk", stringToRapidjsonValue(chunkStr, snapshotChunk.GetAllocator()), snapshotChunk.GetAllocator());

        sendJSONDocument(snapshotChunk);
    }

    return replyOK(jsonMessage);
}

bool DebuggerDevtools::replyMethodNotFound(rapidjson::Document& jsonMessage)
{
    const std::string jsonReplyString = string_format(R"({"id":%d,"error":{"code":-32601,"message":"'%s' wasn't found"}})",
                                                      jsonMessage["id"].GetInt(), jsonMessage["method"].GetString());

    sendMessage(jsonReplyString, jsonReplyString.length());

    return true;
}

bool DebuggerDevtools::evaluate(rapidjson::Document& jsonMessage, ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock)
{
    if (!state) {
        auto msg = "The debugger has already terminated!";
        consoleOut(String::fromUTF8(msg, std::strlen(msg)));
        return true;
    }

    if (!jsonMessage["params"].HasMember("objectGroup")) {
        return true;
    }

    std::string group = jsonMessage["params"]["objectGroup"].GetString();
    if (group != "console" && group != "watch-group") {
        return true;
    }

    const char* source = jsonMessage["params"]["expression"].GetString();
    size_t sourceLen = jsonMessage["params"]["expression"].GetStringLength();

    Value evalExpression(new Latin1String(source, sourceLen));
    Value result(Value::ForceUninitialized);

    m_stopState = ESCARGOT_DEBUGGER_IN_EVAL_MODE;
    try {
        result = state->context()->globalObject()->evalLocal(*state, evalExpression, state->thisValue(), byteCodeBlock->m_codeBlock, true);
    } catch (const Value& val) {
        result = val;
    }
    m_stopState = ESCARGOT_DEBUGGER_IN_WAIT_MODE;

    rapidjson::Document reply;
    reply.SetObject();

    reply.AddMember("id", jsonMessage["id"], reply.GetAllocator());
    reply.AddMember("result", rapidjson::Value(rapidjson::kObjectType), reply.GetAllocator());

    auto resultObj = rapidjson::Value(rapidjson::kObjectType);

    if (result.isString()) {
        resultObj.AddMember("type", "string", reply.GetAllocator());

        auto str = std::string(result.asString()->toUTF8StringData().data());
        resultObj.AddMember("value", stringToRapidjsonValue(str, reply.GetAllocator()), reply.GetAllocator());
        resultObj.AddMember("description", stringToRapidjsonValue(str, reply.GetAllocator()), reply.GetAllocator());
    } else if (result.isNumber()) {
        std::string description;
        if (result.isDouble()) {
            description = std::to_string(result.asDouble());
        } else {
            description = std::to_string(result.asInt32());
        }

        resultObj.AddMember("type", "number", reply.GetAllocator());
        resultObj.AddMember("value", result.asNumber(), reply.GetAllocator());
        resultObj.AddMember("description", stringToRapidjsonValue(description, reply.GetAllocator()), reply.GetAllocator());
    } else if (result.isBigInt()) {
        std::string str = result.asBigInt()->toString()->toUTF8StringData().data();

        resultObj.AddMember("type", "bigint", reply.GetAllocator());
        resultObj.AddMember("value", stringToRapidjsonValue(str, reply.GetAllocator()), reply.GetAllocator());
        resultObj.AddMember("description", stringToRapidjsonValue(str, reply.GetAllocator()), reply.GetAllocator());
    } else if (result.isUndefined()) {
        resultObj.AddMember("type", "undefined", reply.GetAllocator());
    } else if (result.isObject()) {
        Object* resObj = result.asObject();

        if (resObj->isArray(*state)) {
            rapidjson::Value arrayDescription;

            if (result.toStringWithoutException(*state) != nullptr) {
                UTF8StringData arrayStr = result.toStringWithoutException(*state)->toUTF8StringData();
                arrayDescription = stringToRapidjsonValue("Array: [" + std::string(arrayStr.data(), arrayStr.length()) + "]", reply.GetAllocator());
            }

            resultObj.AddMember("type", "array", reply.GetAllocator());
            resultObj.AddMember("className", "Array", reply.GetAllocator());
            resultObj.AddMember("description", arrayDescription, reply.GetAllocator());
        } else {
            rapidjson::Value objectDescription;

            if (result.toStringWithoutException(*state) != nullptr) {
                UTF8StringData objectStr = result.toStringWithoutException(*state)->toUTF8StringData();
                objectDescription = stringToRapidjsonValue("Object: " + std::string(objectStr.data(), objectStr.length()), reply.GetAllocator());
            }
            resultObj.AddMember("type", "object", reply.GetAllocator());
            resultObj.AddMember("className", "Object", reply.GetAllocator());
            resultObj.AddMember("description", objectDescription, reply.GetAllocator());
        }
    } else {
        auto msg = "Could not determine result type of expression!";
        consoleOut(String::fromUTF8(msg, std::strlen(msg)));
        return true;
    }
    reply["result"].AddMember("result", resultObj, reply.GetAllocator());

    sendJSONDocument(reply);

    return true;
}

#define MESSAGE_TYPE(n)                                                                                            \
    template <size_t N>                                                                                            \
    constexpr MessageTypeArg##n messageTypeArg##n(const char (&methodName)[N], const MessageHandlerArg##n handler) \
    {                                                                                                              \
        return MessageTypeArg##n{ methodName, (uint8_t)N, handler };                                               \
    }

MESSAGE_TYPE(1);
MESSAGE_TYPE(2);
MESSAGE_TYPE(3);

bool DebuggerDevtools::processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest)
{
    uint8_t buffer[ESCARGOT_WS_MAX_MESSAGE_LENGTH];
    size_t length;

    // NOTE: keep sorted
    static constexpr MessageTypeArg1 messageTypesArg1[] = {
        messageTypeArg1("Debugger.enable", &DebuggerDevtools::enableDebugger),
        messageTypeArg1("Debugger.getPossibleBreakpoints", &DebuggerDevtools::sendPossibleBreakpoints),
        messageTypeArg1("Debugger.getScriptSource", &DebuggerDevtools::sendSourceCode),
        messageTypeArg1("Debugger.removeBreakpoint", &DebuggerDevtools::removeBreakpoint),
        messageTypeArg1("Debugger.resume", &DebuggerDevtools::resume),
        messageTypeArg1("Debugger.setAsyncCallStackDepth", &DebuggerDevtools::replyOK), // we may be able to set something for this one
        messageTypeArg1("Debugger.setBlackboxPatterns", &DebuggerDevtools::replyOK), // we ignore this for now, but if needed set skipSourceName in DebuggerTcp
        messageTypeArg1("Debugger.setBreakpointByUrl", &DebuggerDevtools::setBreakpointByUrl),
        messageTypeArg1("Debugger.setBreakpointsActive", &DebuggerDevtools::setBreakpointsActive),
        messageTypeArg1("Debugger.setPauseOnExceptions", &DebuggerDevtools::setPauseOnExceptions),
        messageTypeArg1("Debugger.stepInto", &DebuggerDevtools::stepInto),
        messageTypeArg1("Network.clearAcceptedEncodingsOverride", &DebuggerDevtools::replyMethodNotFound),
        messageTypeArg1("Network.emulateNetworkConditionsByRule", &DebuggerDevtools::replyMethodNotFound),
        messageTypeArg1("Network.enable", &DebuggerDevtools::enableNetwork),
        messageTypeArg1("Network.overrideNetworkState", &DebuggerDevtools::replyMethodNotFound),
        messageTypeArg1("Network.setAttachDebugStack", &DebuggerDevtools::replyMethodNotFound),
        messageTypeArg1("Network.setBlockedURLs", &DebuggerDevtools::replyMethodNotFound),
        messageTypeArg1("Profiler.enable", &DebuggerDevtools::enableProfiler),
        messageTypeArg1("Runtime.enable", &DebuggerDevtools::enableRuntime),
        messageTypeArg1("Runtime.runIfWaitingForDebugger", &DebuggerDevtools::replyOK),
    };

    static constexpr MessageTypeArg2 messageTypesArg2[] = {
        messageTypeArg2("Debugger.stepOut", &DebuggerDevtools::stepOut),
        messageTypeArg2("Debugger.stepOver", &DebuggerDevtools::stepOver),
        messageTypeArg2("HeapProfiler.takeHeapSnapshot", &DebuggerDevtools::takeHeapSnapshot),
        messageTypeArg2("Runtime.getProperties", &DebuggerDevtools::sendProperties),
    };

    static constexpr MessageTypeArg3 messageTypesArg3[] = {
        messageTypeArg3("Debugger.evaluateOnCallFrame", &DebuggerDevtools::evaluate),
    };

    while (true) {
        if (isBlockingRequest) {
            if (!receive(buffer, length)) {
                break;
            }
        } else {
            if (isThereAnyEvent()) {
                if (!receive(buffer, length)) {
                    break;
                }
            } else {
                return false;
            }
        }

        printf("MESSAGE: %.*s\n", static_cast<int>(length), reinterpret_cast<const char*>(buffer));

        rapidjson::Document document;
        document.Parse(reinterpret_cast<const rapidjson::GenericDocument<rapidjson::UTF8<>>::Ch*>(buffer));

        rapidjson::Document jsonMessage;
        if (UNLIKELY(jsonMessage.Parse(reinterpret_cast<const char*>(buffer)).HasParseError())) {
            ESCARGOT_LOG_ERROR("Json Message parsing error: %s at offset: %d\n", GetParseError_En(jsonMessage.GetParseError()), static_cast<int32_t>(jsonMessage.GetErrorOffset()));
            return false;
        }
        const char* methodName = jsonMessage["method"].GetString();
        if (UNLIKELY(methodName == nullptr)) {
            ESCARGOT_LOG_ERROR("Debugger method not provided: %s\n", reinterpret_cast<const char*>(buffer));
            return false;
        }
#define FOR_EACH_MESSAGE_TYPE(messageTypesList, ...)                                                                                                                       \
    for (const auto& message : messageTypesList) {                                                                                                                         \
        if (!DebuggerHttpRouter::requestStartsWith(reinterpret_cast<const uint8_t*>(methodName), message.methodNameLen, message.methodName, strlen(message.methodName))) { \
            continue;                                                                                                                                                      \
        }                                                                                                                                                                  \
        return (this->*message.handler)(__VA_ARGS__);                                                                                                                      \
    }

        FOR_EACH_MESSAGE_TYPE(messageTypesArg1, jsonMessage);
        FOR_EACH_MESSAGE_TYPE(messageTypesArg2, jsonMessage, state);
        FOR_EACH_MESSAGE_TYPE(messageTypesArg3, jsonMessage, state, byteCodeBlock);

        ESCARGOT_LOG_ERROR("Debugger function not supported: %s\n", reinterpret_cast<const char*>(buffer));
        this->replyMethodNotFound(jsonMessage); // maybe reply with not supported instead? if there is such a reply in the spec
    }

    if (!m_pendingMessages.empty() && m_networkEnabled && m_debuggerEnabled && m_runtimeEnabled) {
        for (const std::string& msg : m_pendingMessages) {
            sendMessage(msg);
        }
        m_pendingMessages.clear();
    }

    return enabled();
}

} // namespace Escargot

#endif /* ESCARGOT_DEBUGGER */
