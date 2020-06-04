/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "LeakChecker.h"

#include "LeakCheckerBridge.h"
#include "CustomAllocator.h"
#include "runtime/ErrorObject.h"

#ifdef PROFILE_BDWGC

#include <sys/time.h>
#include <sys/resource.h>

namespace Escargot {

/* Usage in JS: registerLeakCheck( object: PointerValue, description: String ); */
Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!argv[0].isPointerValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::None, "builtinRegisterLeakCheck should get pointer-type argument");
    }

    PointerValue* ptr = argv[0].asPointerValue();
    std::string description = argv[1].toString(state)->toUTF8StringData().data();
    RELEASE_ASSERT(ptr);

    GCUtil::GCLeakChecker::registerAddress(ptr, description);

    return Value();
}

/* Usage in JS: dumpBackTrace( phase: String ); */
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    GCUtil::GCLeakChecker::dumpBackTrace(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}

/* Usage in JS: setPhaseName( phase: String ); */
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    GCUtil::GCLeakChecker::setGCPhaseName(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}
}

#endif // PROFILE_BDWGC
