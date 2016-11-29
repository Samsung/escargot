#include "Escargot.h"
#include "Value.h"
#include "Context.h"
#include "StaticStrings.h"

namespace Escargot {

/*
Value toPrimitive(ExecutionState& ec, PrimitiveTypeHint = PreferNumber) const; // $7.1.1 ToPrimitive
bool toBoolean(ExecutionState& ec) const; // $7.1.2 ToBoolean
double toNumber(ExecutionState& ec) const; // $7.1.3 ToNumber
double toInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
int32_t toInt32(ExecutionState& ec) const; // $7.1.5 ToInt32
uint32_t toUint32(ExecutionState& ec) const; // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
*/
String* Value::toStringSlowCase(ExecutionState& ec) const // $7.1.12 ToString
{
    ASSERT(!isString());
     if (isInt32()) {
         int num = asInt32();
         if (num >= 0 && num < ESCARGOT_STRINGS_NUMBERS_MAX)
             return ec.context()->staticStrings().numbers[num].string();
         RELEASE_ASSERT_NOT_REACHED();
         return String::fromInt32(num);
     } else if (isNumber()) {
         double d = asNumber();
         if (std::isnan(d))
             return ec.context()->staticStrings().NaN.string();
         if (std::isinf(d)) {
             if (std::signbit(d))
                 return ec.context()->staticStrings().NegativeInfinity.string();
             else
                 return ec.context()->staticStrings().Infinity.string();
         }
         // convert -0.0 into 0.0
         // in c++, d = -0.0, d == 0.0 is true
         if (d == 0.0)
             d = 0;
         return String::fromDouble(d);
     } else if (isUndefined()) {
         return ec.context()->staticStrings().undefined.string();
     } else if (isNull()) {
         return ec.context()->staticStrings().null.string();
     } else if (isBoolean()) {
         if (asBoolean())
             return ec.context()->staticStrings().stringTrue.string();
         else
             return ec.context()->staticStrings().stringFalse.string();
     } else {
         // TODO
         RELEASE_ASSERT_NOT_REACHED();
         // return toPrimitive(PreferString).toString();
     }
}
/*
Object* toObject(ExecutionState& ec) const; // $7.1.13 ToObject
Object* toTransientObject(ExecutionState& ec) const; // ES5 $8.7.2 transient object
Object* toTransientObjectSlowPath(ExecutionState& ec) const; // ES5 $8.7.2 transient object
double toLength(ExecutionState& ec) const; // $7.1.15 ToLength
*/
}
