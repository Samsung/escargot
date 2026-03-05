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

#ifndef __EscargotHeapSnapshot__
#define __EscargotHeapSnapshot__

#include "runtime/Object.h"
#include "util/Vector.h"

namespace Escargot {

class Object;
class EnvironmentRecord;
class ObjectEnvironmentRecord;
class GlobalEnvironmentRecord;
class ModuleEnvironmentRecord;
class FunctionEnvironmentRecord;
class DeclarativeEnvironmentRecord;
class DeclarativeEnvironmentRecordIndexed;
class DeclarativeEnvironmentRecordNotIndexed;

class HeapSnapshot : public gc {
public:
    const uint32_t heapSnapshotNodeFieldCount = 10;
    const uint32_t heapSnapshotEdgeFieldCount = 3;

    struct Node {
        // /"edge_types":[["context","element","property","internal","hidden","shortcut","weak"]
        struct Edge {
            enum edgeType {
                context,
                element,
                property,
                internal,
                hidden,
                shortcut,
                weak,
            };

            Edge(edgeType type, const std::string* name, uint64_t to_node_idx)
                : m_type(type)
                , m_name(const_cast<std::string*>(name))
                , m_to_node_idx(to_node_idx)
            {
            }

            edgeType m_type;
            std::string* m_name;
            uint64_t m_to_node_idx;
        };

        enum nodeType {
            hidden,
            array,
            string,
            object,
            code,
            closure,
            regexp,
            number,
            native,
            synthetic,
            concatenated_string,
            sliced_string,
            symbol,
            bigint,
            object_shape
        };

        Node(nodeType type, const std::string* name, uint64_t id, uint64_t self_size, bool detachedness)
            : m_type(type)
            , m_name(const_cast<std::string*>(name))
            , m_id(id)
            , m_self_size(self_size)
            , m_detachedness(detachedness)
        {
        }

        ~Node()
        {
            m_edges.clear();
        }

        nodeType m_type;
        const std::string* m_name;
        uint64_t m_id;
        uint64_t m_self_size;
        bool m_detachedness;
        Vector<Edge, GCUtil::gc_malloc_allocator<Edge>> m_edges;
    };


    HeapSnapshot()
    {
        m_nodeCount = 0;
        m_edgeCount = 0;
    }

    ~HeapSnapshot()
    {
        m_nodes.clear();
        m_strings.clear();
    }

    uint64_t addNode(Node::nodeType type, const std::string& name, uint64_t& id, uint64_t size, bool detachedness = false);
    void addEdge(Node::Edge::edgeType type, uint64_t from, uint64_t to, const std::string& name);
    uint64_t addNodeAndEdge(Node::nodeType nodeType, Node::Edge::edgeType edgeType, const std::string& name, uint64_t& id, uint64_t ownerId, uint64_t size, bool detachedness = false);
    void addObjectProperties(ExecutionState* state, uint64_t& id);

    void addGlobalEnvrionmentRecord(ExecutionState* state, GlobalEnvironmentRecord* env, uint64_t& id, uint64_t LexicalEnvIdx);
    void addObjectEnvironmentRecord(ExecutionState* state, ObjectEnvironmentRecord* env, uint64_t& id, uint64_t& LexicalEnvIdx);
    void addFunctionEnvironmentRecord(ExecutionState* state, FunctionEnvironmentRecord* env, uint64_t& id, uint64_t& LexicalEnvIdx);
    void addModuleEnvironmentRecord(ExecutionState* state, ModuleEnvironmentRecord* env, uint64_t& id, uint64_t& LexicalEnvIdx);
    void addIndexedDeclarativeEnvironmentRecord(ExecutionState* state, DeclarativeEnvironmentRecordIndexed* env, uint64_t& id, uint64_t& LexicalEnvIdx);
    void addNotIndexedDeclarativeEnvironmentRecord(ExecutionState* state, DeclarativeEnvironmentRecordNotIndexed* env, uint64_t& id, uint64_t& LexicalEnvIdx);

    bool prepareHeapSnapshotFile();
    void takeHeapSnapshot(ExecutionState* state);

private:
    uint64_t m_nodeCount;
    uint64_t m_edgeCount;

    Vector<Node, GCUtil::gc_malloc_allocator<Node>> m_nodes;
    std::unordered_set<std::string> m_strings;
    std::unordered_set<Object*> m_seenObjects;
    std::queue<std::pair<Object*, uint64_t>> m_objects;
};

} // namespace Escargot

#endif
