#if defined(ESCARGOT_ENABLE_TEMPORAL)
/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTemporal__
#define __EscargotTemporal__

#include "Escargot.h"
#include "intl/Intl.h"
#include "runtime/VMInstance.h"

namespace Escargot {

class Temporal : public DerivedObject {
public:
    explicit Temporal(ExecutionState& state, Object* proto)
        : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
    {
    }

    bool isTemporalObject() const override
    {
        return true;
    }

    static Value getOptionsObject(ExecutionState& state, const Value& options)
    {
        if (options.isObject()) {
            return options;
        }

        if (!options.isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "options must be object");
        }

        return {};
    }

    static Value toTemporalOverflow(ExecutionState& state, const Value& normalizedOptions)
    {
        if (normalizedOptions.isUndefined()) {
            return state.context()->staticStrings().lazyConstrain().string();
        }
        Value matcherValues[2] = { state.context()->staticStrings().lazyConstrain().string(), state.context()->staticStrings().reject.string() };
        return Intl::getOption(state, normalizedOptions.asObject(), state.context()->staticStrings().lazyoverflow().string(), Intl::StringValue, matcherValues, 2, matcherValues[0]);
    }

    static Value prepareTemporalFields(ExecutionState& state, const Value& fields, const ValueVector& fieldNames, const ValueVector& requiredFields)
    {
        ASSERT(fields.isObject());
        auto* result = new Object(state);
        for (auto property : fieldNames) {
            Value value = fields.asObject()->get(state, ObjectPropertyName(state, fields), property).value(state, Value());
            String* prop = property.asString();
            if (value.isUndefined()) {
                if (std::find(requiredFields.begin(), requiredFields.end(), property) != requiredFields.end()) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, new ASCIIString("requiredFields contains property"));
                }
                if (prop->equals("year") || prop->equals("month") || prop->equals("monthCode") || prop->equals("day") || prop->equals("offset") || prop->equals("era") || prop->equals("eraYear") || prop->equals("timeZone")) {
                    value = Value();
                } else {
                    value = Value(0);
                }
            } else {
                if (!prop->equals("timeZone")) {
                    if (prop->equals("monthCode") || prop->equals("offset") || prop->equals("era")) {
                        value = Value(value.toString(state));
                    } else if (prop->equals("month") || prop->equals("day")) {
                        value = Value(value.toUint32(state));
                    } else {
                        value = Value(value.toInteger(state));
                    }
                }
            }
            result->defineOwnProperty(state, ObjectPropertyName(AtomicString(state, property.asString())), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
        }
        return Value(result);
    }

    static void rejectObjectWithCalendarOrTimeZone(ExecutionState& state, const Value& object)
    {
        ASSERT(object.isObject());
        if (object.asObject()->isTemporalPlainDateObject() || object.asObject()->isTemporalPlainDateTimeObject() || object.asObject()->isTemporalPlainTimeObject() || object.asObject()->isTemporalZonedDateTimeObject() || object.asObject()->isTemporalYearMonthObject() || object.asObject()->isTemporalMonthDayObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, new ASCIIString("Invalid type of Object"));
        }

        if (!object.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lazycalendar().string())).value(state, object).isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, new ASCIIString("Object has calendar property"));
        }

        if (!object.asObject()->get(state, ObjectPropertyName(state, state.context()->staticStrings().lazytimeZone().string())).value(state, object).isUndefined()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, new ASCIIString("Object has timezone property"));
        }
    }
};

} // namespace Escargot

#endif
#endif
