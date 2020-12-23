/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotParserContext__
#define __EscargotParserContext__

#include "parser/Lexer.h"

namespace Escargot {
namespace esprima {

struct ParserContext {
    // Escargot::esprima::ParserContext always allocated on the stack
    MAKE_STACK_ALLOCATED();

    bool allowIn : 1;
    bool allowYield : 1;
    bool allowLexicalDeclaration : 1;
    bool allowSuperCall : 1;
    bool allowSuperProperty : 1;
    bool allowNewTarget : 1;
    bool allowStrictDirective : 1;
    bool allowArguments : 1;
    bool await : 1;
    bool isAssignmentTarget : 1;
    bool isBindingElement : 1;
    bool inFunctionBody : 1;
    bool inIteration : 1;
    bool inSwitch : 1;
    bool inArrowFunction : 1;
    bool inWith : 1;
    bool inCatchClause : 1;
    bool inLoop : 1;
    bool inParameterParsing : 1;
    bool inParameterNameParsing : 1;
    bool hasRestrictedWordInArrayOrObjectInitializer : 1;
    bool strict : 1;
    ::Escargot::EscargotLexer::Scanner::SmallScannerResult firstCoverInitializedNameError;
    std::vector<std::pair<AtomicString, LexicalBlockIndex>> catchClauseSimplyDeclaredVariableNames;
    std::vector<std::pair<AtomicString, bool>> labelSet; // <LabelString, continue accepted>
};
} // namespace esprima
} // namespace Escargot

#endif
