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
#include <cstdio>

#include "Escargot.h"
#include "HeapSnapshot.h"

#include "Debugger.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/GlobalObject.h"
#include "parser/Script.h"
#include "util/Util.h"

#include "rapidjson/document.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

uint64_t HeapSnapshot::addNode(Node::nodeType type, const std::string& name, uint64_t& id, uint64_t size, bool detachedness)
{
    auto iter = m_strings.find(name);
    if (iter == m_strings.end()) {
        auto result = m_strings.insert(name);
        iter = result.first;
    }

    m_nodes.push_back(Node(type, &(*iter), id, size, detachedness));
    id += 1;
    m_nodeCount++;

    return id - 1;
}

void HeapSnapshot::addEdge(Node::Edge::edgeType type, uint64_t from, uint64_t to, const std::string& name)
{
    Node& node = m_nodes[from];
    node.m_edges.push_back(Node::Edge(type, &name, to * heapSnapshotNodeFieldCount));

    m_edgeCount++;
}

uint64_t HeapSnapshot::addNodeAndEdge(Node::nodeType nodeType, Node::Edge::edgeType edgeType, const std::string& name, uint64_t& id, uint64_t ownerId, uint64_t size, bool detachedness)
{
    uint64_t node = addNode(nodeType, name, id, size, detachedness);
    addEdge(edgeType, ownerId, node, name);

    return node;
}

void HeapSnapshot::addObjectProperties(ExecutionState* state, uint64_t& id)
{
    Object* current = nullptr;
    uint64_t owner = 0;
    while (!m_objects.empty()) {
        current = m_objects.front().first;
        owner = m_objects.front().second;
        m_objects.pop();

        Object::OwnPropertyKeyVector objectKeys = current->ownPropertyKeys(*state);

        for (size_t i = 0; i < objectKeys.size(); i++) {
            ObjectPropertyName currentName(*state, objectKeys[i]);
            ObjectGetResult ownProperty = current->getOwnProperty(*state, currentName);

            if (ownProperty.isDataAccessorProperty() && !ownProperty.isDataProperty()) {
                continue;
            }

            Value val = ownProperty.value(*state, val);

            Value currPropertyName = currentName.toPlainValue();
            std::string name = "";
            if (currPropertyName.isString()) {
                name = currPropertyName.asString()->toNonGCUTF8StringData();
            } else if (currPropertyName.isSymbol()) {
                name = currPropertyName.asSymbol()->descriptionString()->toNonGCUTF8StringData();
            }

            Node::Edge::edgeType edgeType = Node::Edge::property;
            if (m_nodes.at(owner).m_type == Node::array) {
                edgeType = Node::Edge::element;
            }

            uint64_t propertyId = 0;
            if (val.isString()) {
                propertyId = addNode(Node::string, name, id, sizeof(String));
                addEdge(Node::Edge::property, owner, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isNumber() || val.isBoolean()) {
                propertyId = addNode(Node::number, name, id, sizeof(val.asNumber()));
                addEdge(Node::Edge::property, owner, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isSymbol()) {
                propertyId = addNode(Node::symbol, name, id, sizeof(*val.asSymbol()));
                std::string str = val.asSymbol()->symbolDescriptiveString()->toNonGCUTF8StringData();
                addEdge(Node::Edge::property, owner, propertyId, str);
            } else if (val.isUndefined()) {
                propertyId = addNode(Node::hidden, name, id, sizeof(Value));
                addEdge(Node::Edge::property, owner, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isObject()) {
                if (m_seenObjects.find(val.asObject()) != m_seenObjects.end()) {
                    continue;
                }

                if (val.isFunction()) {
                    propertyId = addNode(Node::closure, name, id, sizeof(*val.asFunction()));
                    addEdge(Node::Edge::property, owner, propertyId, "Function");
                } else if (val.isBigInt()) {
                    propertyId = addNode(Node::bigint, name, id, sizeof(*val.asBigInt()));
                    addEdge(Node::Edge::property, owner, propertyId, "BigInt");
                } else if (val.asObject()->isArray(*state)) {
                    propertyId = addNode(Node::array, name, id, sizeof(*val.asObject()));
                    addEdge(Node::Edge::property, owner, propertyId, "Array");
                } else {
                    propertyId = addNode(Node::object, name, id, sizeof(*val.asObject()));
                    addEdge(Node::Edge::property, owner, propertyId, "Object");
                }

                m_seenObjects.insert(val.asObject());
                m_objects.push(std::pair<Object*, uint64_t>(val.asObject(), propertyId));
            }
        }
    }
}

void HeapSnapshot::addGlobalEnvrionmentRecord(ExecutionState* state, GlobalEnvironmentRecord* env, uint64_t& id, uint64_t LexicalEnvIdx)
{
    GlobalObject* globalObj = env->globalObject();
    uint64_t gObj = addNodeAndEdge(Node::hidden, Node::Edge::property, "Global Object", id, LexicalEnvIdx, sizeof(GlobalObject));
    uint64_t varDeclarations = addNodeAndEdge(Node::synthetic, Node::Edge::property, "Var Declarations", id, gObj, 0);

    InterpretedCodeBlock* gCodeBlock = env->globalCodeBlock();
    for (size_t i = 0; i < gCodeBlock->identifierInfos().size(); i++) {
        if (!gCodeBlock->identifierInfos()[i].m_isVarDeclaration) {
            continue;
        }

        uint64_t propertyId = 0;
        std::string name = gCodeBlock->identifierInfos()[i].m_name.string()->toNonGCUTF8StringData();
        auto result = env->getBindingValue(*state, gCodeBlock->identifierInfos()[i].m_name);
        if (result.m_hasBindingValue) {
            Value val = result.m_value;

            if (val.isString()) {
                propertyId = addNode(Node::string, name, id, sizeof(String));
                addEdge(Node::Edge::property, varDeclarations, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isNumber() || val.isBoolean()) {
                propertyId = addNode(Node::number, name, id, sizeof(val.asNumber()));
                addEdge(Node::Edge::property, varDeclarations, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isSymbol()) {
                propertyId = addNode(Node::symbol, name, id, sizeof(*val.asSymbol()));
                std::string str = val.asSymbol()->symbolDescriptiveString()->toNonGCUTF8StringData();
                addEdge(Node::Edge::property, varDeclarations, propertyId, str);
            } else if (val.isUndefined()) {
                propertyId = addNode(Node::hidden, name, id, sizeof(Value));
                addEdge(Node::Edge::property, varDeclarations, propertyId, val.toString(*state)->toNonGCUTF8StringData());
            } else if (val.isObject()) {
                propertyId = addNode(Node::object, name, id, sizeof(Value));

                m_seenObjects.insert(val.asObject());
                m_objects.push(std::pair<Object*, uint64_t>(val.asObject(), propertyId));
                addObjectProperties(state, id);
            }
        } else {
            propertyId = addNode(Node::hidden, name, id, sizeof(0));
        }

        addEdge(Node::Edge::property, varDeclarations, propertyId, "Var Declaration");
    }

    m_objects.push(std::pair<Object*, uint64_t>(env->globalObject(), gObj));
    addObjectProperties(state, id);
}

void HeapSnapshot::addObjectEnvironmentRecord(ExecutionState* state, ObjectEnvironmentRecord* env, uint64_t& id, uint64_t& LexicalEnvIdx)
{
    Object* bindingObj = env->bindingObject();
    uint64_t objId = addNodeAndEdge(Node::object, Node::Edge::element, "Object", id, LexicalEnvIdx, sizeof(Object));
    m_objects.push(std::pair<Object*, uint64_t>(bindingObj, objId));
    addObjectProperties(state, id);
}

void HeapSnapshot::addFunctionEnvironmentRecord(ExecutionState* state, FunctionEnvironmentRecord* env, uint64_t& id, uint64_t& LexicalEnvIdx)
{
    ScriptFunctionObject* obj = env->functionObject();
    uint64_t funcObj = addNodeAndEdge(Node::object, Node::Edge::element, "Function Object", id, LexicalEnvIdx, sizeof(ScriptFunctionObject));
    m_objects.push(std::pair<Object*, uint64_t>(obj, funcObj));
    addObjectProperties(state, id);
}

void HeapSnapshot::addModuleEnvironmentRecord(ExecutionState* state, ModuleEnvironmentRecord* env, uint64_t& id, uint64_t& owner)
{
    std::string scriptName = env->script()->srcName()->toNonGCUTF8StringData();
    uint64_t module = addNodeAndEdge(Node::code, Node::Edge::property, scriptName, id, owner, sizeof(ScriptFunctionObject));

    for (uint64_t i = 0; i < env->moduleBindings().size(); i++) {
        std::string name = env->moduleBindings()[i].m_localName.string()->toNonGCUTF8StringData();
        Value val = env->moduleBindings()[i].m_value.toValue();
        if (val.isEmpty()) {
            uint64_t node = addNode(Node::object, name, id, sizeof(String));
            addEdge(Node::Edge::property, module, node, "Empty");
        } else if (val.isString()) {
            uint64_t node = addNode(Node::string, name, id, sizeof(String));
            addEdge(Node::Edge::property, module, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isNumber() || val.isBoolean()) {
            uint64_t node = addNode(Node::number, name, id, sizeof(val.asNumber()));
            addEdge(Node::Edge::property, module, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isSymbol()) {
            uint64_t node = addNode(Node::symbol, name, id, sizeof(val.asSymbol()));
            addEdge(Node::Edge::property, module, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isUndefined()) {
            uint64_t node = addNode(Node::hidden, name, id, sizeof(Value));
            addEdge(Node::Edge::property, module, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isObject()) {
            uint64_t node = addNode(Node::object, name, id, sizeof(Object));
            addEdge(Node::Edge::property, module, node, "Object");

            if (m_seenObjects.find(val.asObject()) != m_seenObjects.end()) {
                continue;
            }

            m_seenObjects.insert(val.asObject());
            m_objects.push(std::pair<Object*, uint64_t>(val.asObject(), node));

            addObjectProperties(state, id);
        }
    }
}

void HeapSnapshot::addIndexedDeclarativeEnvironmentRecord(ExecutionState* state, DeclarativeEnvironmentRecordIndexed* env, uint64_t& id, uint64_t& LexicalEnvIdx)
{
    uint64_t declEnv = addNode(Node::object, "declarative environment record", id, sizeof(*env), false);
    addEdge(Node::Edge::element, declEnv, LexicalEnvIdx, "");
}

void HeapSnapshot::addNotIndexedDeclarativeEnvironmentRecord(ExecutionState* state, DeclarativeEnvironmentRecordNotIndexed* env, uint64_t& id, uint64_t& LexicalEnvIdx)
{
    uint64_t declEnv = addNode(Node::object, "not indexed declarative environment record", id, sizeof(*env), false);
    addEdge(Node::Edge::element, LexicalEnvIdx, declEnv, "");

    IdentifierRecordVector recordVector = env->getIdentifierRecordVector();
    for (uint64_t i = 0; i < recordVector.size(); i++) {
        uint64_t identifier = addNode(Node::object, "identifier record vector element", id, sizeof(recordVector[i]), false);
        addEdge(Node::Edge::element, declEnv, identifier, "");

        uint64_t name = addNode(Node::string, recordVector[i].m_name.string()->toUTF8StringData().data(), id, sizeof(recordVector[i].m_name), false);
        addEdge(Node::Edge::element, identifier, name, "");
    }
}

std::string HeapSnapshot::prepareHeapSnapshotFile()
{
    auto addFields = [](rapidjson::Value& array, Vector<std::string, GCUtil::gc_malloc_allocator<std::string>>& strings, rapidjson::Document::AllocatorType& allocator) {
        for (const std::string& elem : strings) {
            rapidjson::Value s;
            s.SetString(elem.c_str(), elem.length(), allocator);
            array.PushBack(s, allocator);
        }
    };

    rapidjson::Document jsonDoc;
    rapidjson::Document::AllocatorType& allocator = jsonDoc.GetAllocator();
    jsonDoc.SetObject();

    rapidjson::Value meta(rapidjson::kObjectType);

    rapidjson::Value node_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{
            "type",
            "name",
            "id",
            "self_size",
            "edge_count",
            "trace_node_id",
            "detachedness",
            "script_id",
            "line",
            "column"
        };
        addFields(node_fields, strings, allocator);
        meta.AddMember("node_fields", node_fields, allocator);
    }

    rapidjson::Value types(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{
            "hidden", "array", "string", "object", "code", "closure", "regexp",
            "number", "native", "synthetic", "concatenated string",
            "sliced string", "symbol", "bigint", "object shape"
        };
        addFields(types, strings, allocator);
    }

    rapidjson::Value node_types(rapidjson::kArrayType);
    {
        node_types.PushBack(types, allocator);
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{
            "string", "number", "number", "number", "number"
        };
        addFields(node_types, strings, allocator);
        meta.AddMember("node_types", node_types, allocator);
    }

    rapidjson::Value edge_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "type", "name_or_index", "to_node" };
        addFields(edge_fields, strings, allocator);
        meta.AddMember("edge_fields", edge_fields, allocator);
    }

    rapidjson::Value edge_types_inner(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "type", "name_or_index", "to_node" };
        addFields(edge_types_inner, strings, allocator);
    }

    rapidjson::Value edge_types(rapidjson::kArrayType);
    {
        edge_types.PushBack(edge_types_inner, allocator);
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "string_or_number", "node" };
        addFields(edge_types, strings, allocator);
        meta.AddMember("edge_types", edge_types, allocator);
    }

    rapidjson::Value trace_function_info_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "function_id", "name", "script_name", "script_id", "line", "column" };
        addFields(trace_function_info_fields, strings, allocator);
        meta.AddMember("trace_function_info_fields", trace_function_info_fields, allocator);
    }

    rapidjson::Value trace_node_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "id", "function_info_index", "count", "size", "children" };
        addFields(trace_node_fields, strings, allocator);
        meta.AddMember("trace_node_fields", trace_node_fields, allocator);
    }

    rapidjson::Value sample_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "timestamp_us", "last_assigned_id" };
        addFields(sample_fields, strings, allocator);
        meta.AddMember("sample_fields", sample_fields, allocator);
    }

    rapidjson::Value location_fields(rapidjson::kArrayType);
    {
        Vector<std::string, GCUtil::gc_malloc_allocator<std::string>> strings{ "object_index", "script_id", "line", "column" };
        addFields(location_fields, strings, allocator);
        meta.AddMember("location_fields", location_fields, allocator);
    }

    rapidjson::Value snapshot(rapidjson::kObjectType);
    {
        snapshot.AddMember("meta", meta, allocator);
        snapshot.AddMember("node_count", rapidjson::Value(m_nodeCount * heapSnapshotNodeFieldCount), allocator);
        snapshot.AddMember("edge_count", rapidjson::Value(m_edgeCount * heapSnapshotEdgeFieldCount), allocator);
        snapshot.AddMember("extra_native_bytes", 0, allocator);
        snapshot.AddMember("trace_function_count", 0, allocator);
    }

    rapidjson::Value nodes(rapidjson::kArrayType);
    rapidjson::Value edges(rapidjson::kArrayType);
    for (Node& node : m_nodes) {
        nodes.PushBack(node.m_type, allocator);
        auto iter = m_strings.find(*node.m_name);
        auto index = std::distance(m_strings.begin(), iter);
        nodes.PushBack(index, allocator);
        nodes.PushBack(node.m_id, allocator);
        nodes.PushBack(node.m_self_size, allocator);
        nodes.PushBack(node.m_edges.size(), allocator);
        nodes.PushBack(0, allocator);
        nodes.PushBack((uint32_t)node.m_detachedness, allocator);
        nodes.PushBack(0, allocator);
        nodes.PushBack(0, allocator);
        nodes.PushBack(0, allocator);

        for (Node::Edge& edge : node.m_edges) {
            edges.PushBack(edge.m_type, allocator);
            edges.PushBack(index, allocator);
            edges.PushBack(edge.m_to_node_idx, allocator);
        }
    }


    rapidjson::Value trace_function_infos(rapidjson::kArrayType);
    rapidjson::Value trace_tree(rapidjson::kArrayType);
    rapidjson::Value samples(rapidjson::kArrayType);
    rapidjson::Value locations(rapidjson::kArrayType);

    rapidjson::Value strings(rapidjson::kArrayType);
    for (const std::string& str : m_strings) {
        rapidjson::Value s;
        s = rapidjson::StringRef(str.c_str());
        strings.PushBack(s, allocator);
    }


    jsonDoc.AddMember("snapshot", snapshot, allocator);
    jsonDoc.AddMember("nodes", nodes, allocator);
    jsonDoc.AddMember("edges", edges, allocator);
    jsonDoc.AddMember("trace_function_infos", trace_function_infos, allocator);
    jsonDoc.AddMember("trace_tree", trace_tree, allocator);
    jsonDoc.AddMember("samples", samples, allocator);
    jsonDoc.AddMember("locations", locations, allocator);
    jsonDoc.AddMember("strings", strings, allocator);

    std::string outputName = "output-" + std::to_string(timestamp()) + ".heapsnapshot";
    auto deleter = [&](std::FILE* fp) { fclose(fp); };
    std::unique_ptr<std::FILE, decltype(deleter)> output(fopen(outputName.c_str(), "w"), deleter);

    if (output.get() == nullptr) {
        ESCARGOT_LOG_ERROR("Could not create heap snapshot file.\n");
        return "";
    }

    char buffer[65536];
    rapidjson::FileWriteStream os(output.get(), buffer, sizeof(buffer));
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    jsonDoc.Accept(writer);

    return outputName;
}

std::string HeapSnapshot::takeHeapSnapshot(ExecutionState* state)
{
    uint64_t id = 0;
    uint64_t stateIdx = 0;

    uint64_t root = addNode(HeapSnapshot::Node::hidden, "Root", id, sizeof(ExecutionState));
    uint64_t context = addNodeAndEdge(Node::hidden, Node::Edge::property, "Context", id, root, sizeof(Context));
    uint64_t staticStrings = addNodeAndEdge(Node::hidden, Node::Edge::property, "Static Strings", id, context, sizeof(StaticStrings));

    for (auto& str : *state->context()->atomicStringMap()) {
        addNodeAndEdge(Node::string, Node::Edge::property, str->toNonGCUTF8StringData(), id, staticStrings, sizeof(String));
    }

    uint64_t userDefined = addNodeAndEdge(Node::synthetic, Node::Edge::property, "User Defined variables", id, root, 0);
    for (size_t i = 0; i < state->context()->globalDeclarativeRecord()->size(); i++) {
        std::string name = state->context()->globalDeclarativeRecord()->at(i).m_name.string()->toNonGCUTF8StringData();

        if (state->context()->globalDeclarativeStorage()->at(i).isEmpty()) {
            addNodeAndEdge(Node::hidden, Node::Edge::property, name, id, userDefined, 0);
            continue;
        }

        Value val = state->context()->globalDeclarativeStorage()->at(i).toValue();
        if (val.isString()) {
            uint64_t node = addNode(Node::string, name, id, sizeof(String));
            addEdge(Node::Edge::property, userDefined, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isNumber() || val.isBoolean()) {
            uint64_t node = addNode(Node::number, name, id, sizeof(val.asNumber()));
            addEdge(Node::Edge::property, userDefined, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isSymbol()) {
            uint64_t node = addNode(Node::symbol, name, id, sizeof(val.asSymbol()));
            addEdge(Node::Edge::property, userDefined, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isUndefined()) {
            uint64_t node = addNode(Node::hidden, name, id, sizeof(Value));
            addEdge(Node::Edge::property, userDefined, node, val.toString(*state)->toNonGCUTF8StringData());
        } else if (val.isObject()) {
            uint64_t node = addNode(Node::object, name, id, sizeof(Object));
            addEdge(Node::Edge::property, userDefined, node, "Object");

            if (m_seenObjects.find(val.asObject()) != m_seenObjects.end()) {
                continue;
            }

            m_seenObjects.insert(val.asObject());
            m_objects.push(std::pair<Object*, uint64_t>(val.asObject(), node));

            addObjectProperties(state, id);
        }
    }

    LexicalEnvironment* lexEnv = state->lexicalEnvironment();
    while (lexEnv) {
        uint64_t lexEnvId = addNodeAndEdge(Node::hidden, Node::Edge::property, "LexicalEnvironment", id, root, sizeof(LexicalEnvironment));
        EnvironmentRecord* envRecord = lexEnv->record();

        if (envRecord->isGlobalEnvironmentRecord()) {
            addGlobalEnvrionmentRecord(state, envRecord->asGlobalEnvironmentRecord(), id, lexEnvId);
        } else if (envRecord->isObjectEnvironmentRecord()) {
            addObjectEnvironmentRecord(state, envRecord->asObjectEnvironmentRecord(), id, lexEnvId);
        } else if (envRecord->isDeclarativeEnvironmentRecord()) {
            DeclarativeEnvironmentRecord* declarativeRecord = envRecord->asDeclarativeEnvironmentRecord();
            if (declarativeRecord->isFunctionEnvironmentRecord()) {
                FunctionEnvironmentRecord* funcEnv = envRecord->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                addFunctionEnvironmentRecord(state, funcEnv, id, lexEnvId);
            } else if (envRecord->isModuleEnvironmentRecord()) {
                ModuleEnvironmentRecord* moduleEnv = envRecord->asDeclarativeEnvironmentRecord()->asModuleEnvironmentRecord();
                uint64_t modules = addNodeAndEdge(Node::synthetic, Node::Edge::property, "Modules", id, root, 0);
                addModuleEnvironmentRecord(state, moduleEnv, id, modules);
            } else if (envRecord->asDeclarativeEnvironmentRecord()->isDeclarativeEnvironmentRecordIndexed()) {
                DeclarativeEnvironmentRecordIndexed* declEnv = envRecord->asDeclarativeEnvironmentRecord()->asDeclarativeEnvironmentRecordIndexed();
                addIndexedDeclarativeEnvironmentRecord(state, declEnv, id, lexEnvId);
            } else {
                DeclarativeEnvironmentRecordNotIndexed* declEnv = envRecord->asDeclarativeEnvironmentRecord()->asDeclarativeEnvironmentRecordNotIndexed();
                addNotIndexedDeclarativeEnvironmentRecord(state, declEnv, id, lexEnvId);
            }
        }

        lexEnv = lexEnv->outerEnvironment();
    }

    ESCARGOT_LOG_INFO("nodes: %ld | edges: %ld | strings: %zu\n", m_nodeCount, m_edgeCount, m_strings.size());
    return prepareHeapSnapshotFile();
}
} // namespace Escargot
#endif
