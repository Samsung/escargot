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

#ifndef __EscargotMessages__
#define __EscargotMessages__

namespace Escargot {
namespace esprima {
namespace Messages {
const char* UnexpectedToken = "Unexpected token %s";
const char* UnexpectedNumber = "Unexpected number";
const char* UnexpectedString = "Unexpected string";
const char* UnexpectedIdentifier = "Unexpected identifier";
const char* UnexpectedReserved = "Unexpected reserved word";
const char* UnexpectedTemplate = "Unexpected quasi %s";
const char* UnexpectedEOS = "Unexpected end of input";
const char* NewlineAfterThrow = "Illegal newline after throw";
const char* InvalidRegExp = "Invalid regular expression";
const char* InvalidLHSInAssignment = "Invalid left-hand side in assignment";
const char* InvalidLHSInForIn = "Invalid left-hand side in for-in";
const char* InvalidLHSInForLoop = "Invalid left-hand side in for-loop";
const char* MultipleDefaultsInSwitch = "More than one default clause in switch statement";
const char* NoCatchOrFinally = "Missing catch or finally after try";
const char* UnknownLabel = "Undefined label \'%s\'";
const char* Redeclaration = "%s \'%s\' has already been declared";
const char* IllegalContinue = "Illegal continue statement";
const char* IllegalBreak = "Illegal break statement";
const char* IllegalReturn = "Illegal return statement";
const char* StrictModeWith = "Strict mode code may not include a with statement";
const char* StrictCatchVariable = "Catch variable may not be eval or arguments in strict mode";
const char* StrictVarName = "Variable name may not be eval or arguments in strict mode";
const char* StrictParamName = "Parameter name eval or arguments is not allowed in strict mode";
const char* StrictParamDupe = "Strict mode function may not have duplicate parameter names";
const char* StrictFunctionName = "Function name may not be eval or arguments in strict mode";
const char* StrictOctalLiteral = "Octal literals are not allowed in strict mode.";
const char* StrictLeadingZeroLiteral = "Decimals with leading zeros are not allowed in strict mode.";
const char* StrictDelete = "Delete of an unqualified identifier in strict mode.";
const char* StrictLHSAssignment = "Assignment to eval or arguments is not allowed in strict mode";
const char* StrictLHSPostfix = "Postfix increment/decrement may not have eval or arguments operand in strict mode";
const char* StrictLHSPrefix = "Prefix increment/decrement may not have eval or arguments operand in strict mode";
const char* StrictReservedWord = "Use of future reserved word in strict mode";
const char* ParameterAfterRestParameter = "Rest parameter must be last formal parameter";
const char* DefaultRestParameter = "Unexpected token =";
const char* ObjectPatternAsRestParameter = "Unexpected token {";
const char* DuplicateProtoProperty = "Duplicate __proto__ fields are not allowed in object literals";
const char* ConstructorSpecialMethod = "Class constructor may not be an accessor";
const char* ConstructorGenerator = "Class constructor may not be a generator";
const char* DuplicateConstructor = "A class may only have one constructor";
const char* StaticPrototype = "Classes may not have static property named prototype";
const char* MissingFromClause = "Unexpected token";
const char* NoAsAfterImportNamespace = "Unexpected token";
const char* InvalidModuleSpecifier = "Unexpected token";
const char* IllegalImportDeclaration = "Unexpected token";
const char* IllegalExportDeclaration = "Unexpected token";
const char* DuplicateBinding = "Duplicate binding %s";
const char* ForInOfLoopInitializer = "%s loop variable declaration may not have an initializer";
const char* BadGetterArity = "Getter must not have any formal parameters";
const char* BadSetterArity = "Setter must have exactly one formal parameter";
const char* BadSetterRestParameter = "Setter function argument must not be a rest parameter";
} // namespace Messages
} // namespace esprima
} // namespace Escargot

#endif
