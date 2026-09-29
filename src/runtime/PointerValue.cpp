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

#include "Escargot.h"
#include "PointerValue.h"
#include "FunctionObject.h"
#include "ErrorObject.h"
#include "ScriptSimpleFunctionObject.h"

namespace Escargot {

constexpr const size_t* PointerValue::g_objectTag;
constexpr const size_t* PointerValue::g_prototypeObjectTag;
constexpr const size_t* PointerValue::g_arrayObjectTag;
constexpr const size_t* PointerValue::g_typedArrayObjectTag;
constexpr const size_t* PointerValue::g_arrayPrototypeObjectTag;
constexpr const size_t* PointerValue::g_scriptFunctionObjectTag;
constexpr const size_t* PointerValue::g_objectRareDataTag;
// tag values for ScriptSimpleFunctionObject
#define DEFINE_SCRIPTSIMPLEFUNCTION_TAGS(STRICT, CLEAR, isStrict, isClear, SIZE) \
    constexpr const size_t* PointerValue::g_scriptSimpleFunctionObject##STRICT##CLEAR##SIZE##Tag;

DECLARE_SCRIPTSIMPLEFUNCTION_LIST(DEFINE_SCRIPTSIMPLEFUNCTION_TAGS);
#undef DEFINE_SCRIPTSIMPLEFUNCTION_TAGS

// A vptr rewrite can be the only use of these specializations. Explicitly
// instantiate them so their vtables survive constructor/check elimination.
#define INSTANTIATE_SCRIPTSIMPLEFUNCTION(STRICT, CLEAR, isStrict, isClear, SIZE) \
    template class ScriptSimpleFunctionObject<isStrict, isClear, SIZE>;
DECLARE_SCRIPTSIMPLEFUNCTION_LIST(INSTANTIATE_SCRIPTSIMPLEFUNCTION);
#undef INSTANTIATE_SCRIPTSIMPLEFUNCTION

} // namespace Escargot
