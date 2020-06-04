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
#include "FunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/ProgramNode.h"
#include "parser/ScriptParser.h"
#include "parser/esprima_cpp/esprima.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

void FunctionObject::initStructureAndValues(ExecutionState& state, bool isConstructor, bool isGenerator, bool isAsync)
{
    if (isGenerator) {
        // Unlike function instances, the object that is the value of the GeneratorFunction’s of AsyncGeneratorFunction prototype property
        // does not have a constructor property whose value is the GeneratorFunction or the AsyncGeneratorFunction instance.
        m_structure = state.context()->defaultStructureForFunctionObject();
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2 < m_structure->propertyCount());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->functionLength()));
    } else {
        if (isConstructor) {
            m_structure = state.context()->defaultStructureForFunctionObject();
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2 < m_structure->propertyCount());
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->functionLength()));
        } else {
            m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionLength()));
        }
    }
}

// function for derived classes. derived class MUST initlize member variable of FunctionObject.
FunctionObject::FunctionObject(ExecutionState& state, Object* proto, size_t defaultSpace)
    : Object(state, proto, defaultSpace)
#ifndef NDEBUG
    , m_codeBlock((CodeBlock*)SIZE_MAX)
#endif
{
}

FunctionObject::FunctionSource FunctionObject::createFunctionSourceFromScriptSource(ExecutionState& state, AtomicString functionName, size_t argumentValueArrayCount, Value* argumentValueArray, Value bodyString, bool useStrict, bool isGenerator, bool isAsync, bool allowSuperCall)
{
    StringBuilder src, srcToTest;
    if (useStrict) {
        src.appendString("'use strict'; ");
    }

    if (isGenerator && isAsync) {
        src.appendString("async function* ");
        srcToTest.appendString("async function* ");
    } else if (isGenerator) {
        src.appendString("function* ");
        srcToTest.appendString("function* ");
    } else if (isAsync) {
        src.appendString("async function ");
        srcToTest.appendString("async function ");
    } else {
        src.appendString("function ");
        srcToTest.appendString("function ");
    }

    src.appendString(functionName.string());
    src.appendString("(");
    srcToTest.appendString(functionName.string());
    srcToTest.appendString("(");

    for (size_t i = 0; i < argumentValueArrayCount; i++) {
        String* p = argumentValueArray[i].toString(state);
        src.appendString(p);
        srcToTest.appendString(p);
        if (i != argumentValueArrayCount - 1) {
            src.appendString(",");
            srcToTest.appendString(",");

            for (size_t j = 0; j < p->length(); j++) {
                char16_t c = p->charAt(j);
                if (c == '}' || c == '{' || c == ')' || c == '(') {
                    ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
                }
            }
        }
    }

    GC_disable();

    try {
        srcToTest.appendString("\n) { }");
        String* cur = srcToTest.finalize(&state);
        esprima::parseProgram(state.context(), StringView(cur, 0, cur->length()), false, false, false, SIZE_MAX, false, false, true);

        // reset ASTAllocator
        state.context()->astAllocator().reset();
        GC_enable();
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        state.context()->astAllocator().reset();
        GC_enable();

        delete orgError;

        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
    }

    String* source = bodyString.toString(state);

    auto data = source->bufferAccessData();

    for (size_t i = 0; i < data.length; i++) {
        char16_t c;
        if (data.has8BitContent) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c == '{') {
            break;
        } else if (c == '}') {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is unbalanced braces(}) in Function Constructor input");
        }
    }
    src.appendString("\n) {\n");
    src.appendString(source);
    src.appendString("\n}");

    ScriptParser parser(state.context());
    String* scriptSource = src.finalize(&state);

    Script* script = parser.initializeScript(StringView(scriptSource, 0, scriptSource->length()), new ASCIIString("Function Constructor input"), false, nullptr, false, false, false, false, SIZE_MAX, false, allowSuperCall, false, true).scriptThrowsExceptionIfParseError(state);
    InterpretedCodeBlock* cb = script->topCodeBlock()->firstChild();
    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, script->topCodeBlock(), state.context()->globalObject(), state.context()->globalDeclarativeRecord(), state.context()->globalDeclarativeStorage()), nullptr);

    FunctionObject::FunctionSource fs;
    fs.script = script;
    fs.codeBlock = cb;
    fs.outerEnvironment = globalEnvironment;

    return fs;
}
}
