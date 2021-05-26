/*
  Copyright JS Foundation and other contributors, https://js.foundation/
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotMessages__
#define __EscargotMessages__

namespace Escargot {
namespace esprima {

class Messages {
public:
    static constexpr const char* UnexpectedToken = "Unexpected token %s";
    static constexpr const char* UnexpectedNumber = "Unexpected number";
    static constexpr const char* UnexpectedString = "Unexpected string";
    static constexpr const char* UnexpectedIdentifier = "Unexpected identifier";
    static constexpr const char* UnexpectedReserved = "Unexpected reserved word";
    static constexpr const char* UnexpectedTemplate = "Unexpected quasi %s";
    static constexpr const char* UnexpectedEOS = "Unexpected end of input";
    static constexpr const char* NewlineAfterThrow = "Illegal newline after throw";
    static constexpr const char* InvalidRegExp = "Invalid regular expression";
    static constexpr const char* InvalidLHSInAssignment = "Invalid left-hand side in assignment";
    static constexpr const char* InvalidLHSInForIn = "Invalid left-hand side in for-in";
    static constexpr const char* InvalidLHSInForLoop = "Invalid left-hand side in for-loop";
    static constexpr const char* InvalidClassElementName = "Invalid class element name";
    static constexpr const char* MultipleDefaultsInSwitch = "More than one default clause in switch statement";
    static constexpr const char* NoCatchOrFinally = "Missing catch or finally after try";
    static constexpr const char* UnknownLabel = "Undefined label \'%s\'";
    static constexpr const char* Redeclaration = "%s \'%s\' has already been declared";
    static constexpr const char* IllegalContinue = "Illegal continue statement";
    static constexpr const char* IllegalBreak = "Illegal break statement";
    static constexpr const char* IllegalReturn = "Illegal return statement";
    static constexpr const char* StrictModeWith = "Strict mode code may not include a with statement";
    static constexpr const char* StrictCatchVariable = "Catch variable may not be eval or arguments in strict mode";
    static constexpr const char* StrictVarName = "Variable name may not be eval or arguments in strict mode";
    static constexpr const char* StrictParamName = "Parameter name eval or arguments is not allowed in strict mode";
    static constexpr const char* StrictParamDupe = "Strict mode function may not have duplicate parameter names";
    static constexpr const char* StrictFunctionName = "Function name may not be eval or arguments in strict mode";
    static constexpr const char* StrictOctalLiteral = "Octal literals are not allowed in strict mode.";
    static constexpr const char* StrictLeadingZeroLiteral = "Decimals with leading zeros are not allowed in strict mode.";
    static constexpr const char* StrictDelete = "Delete of an unqualified identifier in strict mode.";
    static constexpr const char* StrictLHSAssignment = "Assignment to eval or arguments is not allowed in strict mode";
    static constexpr const char* StrictLHSPostfix = "Postfix increment/decrement may not have eval or arguments operand in strict mode";
    static constexpr const char* StrictLHSPrefix = "Prefix increment/decrement may not have eval or arguments operand in strict mode";
    static constexpr const char* StrictReservedWord = "Use of future reserved word in strict mode";
    static constexpr const char* ParameterAfterRestParameter = "Rest parameter must be last formal parameter";
    static constexpr const char* DefaultRestParameter = "Unexpected token =";
    static constexpr const char* ObjectPatternAsRestParameter = "Unexpected token {";
    static constexpr const char* DuplicateProtoProperty = "Duplicate __proto__ fields are not allowed in object literals";
    static constexpr const char* ConstructorSpecialMethod = "Class constructor may not be an accessor";
    static constexpr const char* ConstructorGenerator = "Class constructor may not be a generator";
    static constexpr const char* DuplicateConstructor = "A class may only have one constructor";
    static constexpr const char* StaticPrototype = "Classes may not have static property named prototype";
    static constexpr const char* MissingFromClause = "Unexpected token";
    static constexpr const char* NoAsAfterImportNamespace = "Unexpected token";
    static constexpr const char* InvalidModuleSpecifier = "Unexpected token";
    static constexpr const char* IllegalImportDeclaration = "Unexpected token";
    static constexpr const char* IllegalExportDeclaration = "Unexpected token";
    static constexpr const char* DuplicateBinding = "Duplicate binding %s";
    static constexpr const char* ForInOfLoopInitializer = "%s loop variable declaration may not have an initializer";
    static constexpr const char* BadGetterArity = "Getter must not have any formal parameters";
    static constexpr const char* BadSetterArity = "Setter must have exactly one formal parameter";
    static constexpr const char* BadSetterRestParameter = "Setter function argument must not be a rest parameter";
    static constexpr const char* CannotChainLogicalWithNullish = "Cannot chain logical expression with nullish operator";
    static constexpr const char* KeywordMustNotContainEscapedCharacters = "Keyword must not contain escaped characters";
    static constexpr const char* AsyncGeneratorFuncDeclLocationError = "Async or generator function can only be declared at the top level or inside a block";
    static constexpr const char* PrivateFieldMustBeDeclared = "Private field \'%s\' must be declared in an enclosing class";
    static constexpr const char* CannnotUsePrivateFieldHere = "Cannot use private field here";
};

} // namespace esprima
} // namespace Escargot

#endif
