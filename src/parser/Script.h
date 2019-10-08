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

#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/Value.h"

namespace Escargot {

class InterpretedCodeBlock;
class Context;
class ModuleEnvironmentRecord;

class Script : public gc {
    friend class ScriptParser;
    friend class GlobalObject;
    friend class ModuleNamespaceObject;

public:
    // http://www.ecma-international.org/ecma-262/6.0/#table-39
    struct ImportEntry {
        // [[ModuleRequest]]   String  String value of the ModuleSpecifier of the ImportDeclaration.
        String* m_moduleRequest;
        // [[ImportName]]  String  The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. The value "*" indicates that the import request is for the target module’s namespace object.
        AtomicString m_importName;
        // [[LocalName]]   String  The name that is used to locally access the imported value from within the importing module.
        AtomicString m_localName;

        ImportEntry()
            : m_moduleRequest(String::emptyString)
        {
        }
    };
    typedef Vector<ImportEntry, GCUtil::gc_malloc_allocator<ImportEntry>> ImportEntryVector;

    // http://www.ecma-international.org/ecma-262/6.0/#table-41
    struct ExportEntry {
        // [[ExportName]]  String  The name used to export this binding by this module.
        Optional<AtomicString> m_exportName;
        // [[ModuleRequest]]   String | null   The String value of the ModuleSpecifier of the ExportDeclaration. null if the ExportDeclaration does not have a ModuleSpecifier.
        Optional<String*> m_moduleRequest;
        // [[ImportName]]  String | null   The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. null if the ExportDeclaration does not have a ModuleSpecifier. "*" indicates that the export request is for all exported bindings.
        Optional<AtomicString> m_importName;
        // [[LocalName]]   String | null   The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
        Optional<AtomicString> m_localName; // The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
    };
    typedef Vector<ExportEntry, GCUtil::gc_malloc_allocator<ExportEntry>> ExportEntryVector;

    // http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
    struct ModuleData : public gc {
        // [[ImportEntries]]
        ImportEntryVector m_importEntries;
        // [[LocalExportEntries]]
        ExportEntryVector m_localExportEntries;
        // [[IndirectExportEntries]]
        ExportEntryVector m_indirectExportEntries;
        // [[StarExportEntries]]
        ExportEntryVector m_starExportEntries;
        // [[RequestedModules]]
        StringVector m_requestedModules;
        ModuleEnvironmentRecord* m_moduleRecord;

        ModuleData()
            : m_moduleRecord(nullptr)
        {
        }
    };

    Value execute(ExecutionState& state, bool isExecuteOnEvalFunction = false, bool inStrictMode = false);
    String* src()
    {
        return m_src;
    }

    String* sourceCode()
    {
        return m_sourceCode;
    }

    InterpretedCodeBlock* topCodeBlock()
    {
        return m_topCodeBlock;
    }

    Context* context();

    bool isModule()
    {
        return m_moduleData;
    }

    ModuleData* moduleData()
    {
        return m_moduleData;
    }

    size_t moduleRequestsLength();
    String* moduleRequest(size_t i);

    bool isExecuted();

private:
    Script(String* src, String* sourceCode, ModuleData* moduleData)
        : m_src(src)
        , m_sourceCode(sourceCode)
        , m_topCodeBlock(nullptr)
        , m_moduleData(moduleData)
    {
    }
    Value executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isStrictModeOutside = false, bool isEvalCodeOnFunction = false);
    void loadModuleFromScript(ExecutionState& state, String* src);
    void loadExternalModule(ExecutionState& state);
    Value executeModule(ExecutionState& state, Optional<Script*> referrer);
    struct ResolveExportResult {
        enum ResultType {
            Record,
            Null,
            Ambiguous
        };
        ResultType m_type;
        Optional<std::tuple<Script*, AtomicString>> m_record;

        ResolveExportResult(ResultType tp, Optional<std::tuple<Script*, AtomicString>> record = Optional<std::tuple<Script*, AtomicString>>())
            : m_type(tp)
            , m_record(record)
        {
        }
    };

    ResolveExportResult resolveExport(ExecutionState& state, AtomicString exportName)
    {
        std::vector<std::tuple<Script*, AtomicString>> resolveSet;
        std::vector<Script*> exportStarSet;
        return resolveExport(state, exportName, resolveSet, exportStarSet);
    }
    // http://www.ecma-international.org/ecma-262/6.0/#sec-resolveexport
    ResolveExportResult resolveExport(ExecutionState& state, AtomicString exportName, std::vector<std::tuple<Script*, AtomicString>>& resolveSet, std::vector<Script*>& exportStarSet);
    AtomicStringVector exportedNames(ExecutionState& state)
    {
        std::vector<Script*> exportStarSet;
        return exportedNames(state, exportStarSet);
    }
    // http://www.ecma-international.org/ecma-262/6.0/#sec-getexportednames
    AtomicStringVector exportedNames(ExecutionState& state, std::vector<Script*>& exportStarSet);
    String* m_src;
    String* m_sourceCode;
    InterpretedCodeBlock* m_topCodeBlock;
    ModuleData* m_moduleData;
};
}

#endif
