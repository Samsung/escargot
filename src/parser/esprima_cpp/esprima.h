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

#ifndef __EscargotEsprima__
#define __EscargotEsprima__

namespace Escargot {

class Context;
class CodeBlock;
class String;
class StringView;
class InterpretedCodeBlock;
class Node;
class FunctionNode;
class ProgramNode;

struct NodeLOC;
struct ASTClassInfo;

enum class ErrorCode : uint8_t;

// Based on the latest esprima version 4.0.1

namespace esprima {

typedef std::function<void(::Escargot::Node*, NodeLOC start, NodeLOC end)> ParserASTNodeHandler;

struct Error : public gc {
    String* name;
    String* message;
    size_t index;
    size_t lineNumber;
    size_t column;
    String* description;
    ErrorCode errorCode;

    explicit Error(String* message);
};

#define ESPRIMA_RECURSIVE_LIMIT 1024

ProgramNode* parseProgram(::Escargot::Context* ctx, StringView source, ASTClassInfo* outerClassInfo,
                          bool isModule, bool strictFromOutside, bool inWith, size_t stackRemain, bool allowSuperCallFromOutside,
                          bool allowSuperPropertyFromOutside, bool allowNewTargetFromOutside, bool allowArgumentsFromOutside);
FunctionNode* parseSingleFunction(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock, size_t stackRemain);

ASTClassInfo* generateClassInfoFrom(::Escargot::Context* ctx, InterpretedCodeBlock* codeBlock);

void simpleSyntaxCheckFunctionElements(::Escargot::Context* ctx, String* parameters, String* body, bool isStrict, bool isGenerator, bool isAsync);
} // namespace esprima
} // namespace Escargot

#endif
