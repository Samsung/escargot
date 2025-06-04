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

#ifndef __EscargotStaticStrings__
#define __EscargotStaticStrings__

#include "runtime/AtomicString.h"

namespace Escargot {

#define FOR_EACH_STATIC_STRING(F) \
    F($1)                         \
    F($2)                         \
    F($3)                         \
    F($4)                         \
    F($5)                         \
    F($6)                         \
    F($7)                         \
    F($8)                         \
    F($9)                         \
    F($_)                         \
    F(AggregateError)             \
    F(Array)                      \
    F(ArrayBuffer)                \
    F(ArrayIterator)              \
    F(AsyncFunction)              \
    F(AsyncGenerator)             \
    F(AsyncGeneratorFunction)     \
    F(BYTES_PER_ELEMENT)          \
    F(BigInt)                     \
    F(BigInt64Array)              \
    F(BigUint64Array)             \
    F(Boolean)                    \
    F(Collator)                   \
    F(DataView)                   \
    F(Date)                       \
    F(DateTimeFormat)             \
    F(DisplayNames)               \
    F(E)                          \
    F(EPSILON)                    \
    F(Empty)                      \
    F(Error)                      \
    F(EvalError)                  \
    F(FinalizationRegistry)       \
    F(Float32Array)               \
    F(Float64Array)               \
    F(Function)                   \
    F(Generator)                  \
    F(GeneratorFunction)          \
    F(GlobalObject)               \
    F(Infinity)                   \
    F(Int16Array)                 \
    F(Int32Array)                 \
    F(Int8Array)                  \
    F(Intl)                       \
    F(Iterator)                   \
    F(JSON)                       \
    F(LN10)                       \
    F(LN2)                        \
    F(LOG10E)                     \
    F(LOG2E)                      \
    F(ListFormat)                 \
    F(Locale)                     \
    F(MAX_SAFE_INTEGER)           \
    F(MAX_VALUE)                  \
    F(MIN_SAFE_INTEGER)           \
    F(MIN_VALUE)                  \
    F(Map)                        \
    F(MapIterator)                \
    F(Math)                       \
    F(Module)                     \
    F(NEGATIVE_INFINITY)          \
    F(NaN)                        \
    F(Number)                     \
    F(NumberFormat)               \
    F(Object)                     \
    F(PI)                         \
    F(POSITIVE_INFINITY)          \
    F(PluralRules)                \
    F(Promise)                    \
    F(Proxy)                      \
    F(RangeError)                 \
    F(ReferenceError)             \
    F(Reflect)                    \
    F(RegExp)                     \
    F(RegExpStringIterator)       \
    F(RelativeTimeFormat)         \
    F(SQRT1_2)                    \
    F(SQRT2)                      \
    F(Set)                        \
    F(SetIterator)                \
    F(String)                     \
    F(StringIterator)             \
    F(Symbol)                     \
    F(SyntaxError)                \
    F(TypeError)                  \
    F(TypedArray)                 \
    F(URIError)                   \
    F(UTC)                        \
    F(Uint16Array)                \
    F(Uint32Array)                \
    F(Uint8Array)                 \
    F(Uint8ClampedArray)          \
    F(WeakMap)                    \
    F(WeakRef)                    \
    F(WeakSet)                    \
    F(__defineGetter__)           \
    F(__defineSetter__)           \
    F(__lookupGetter__)           \
    F(__lookupSetter__)           \
    F(__proto__)                  \
    F(abs)                        \
    F(acos)                       \
    F(acosh)                      \
    F(add)                        \
    F(all)                        \
    F(allSettled)                 \
    F(alreadyCalled)              \
    F(alreadyResolved)            \
    F(anchor)                     \
    F(anonymous)                  \
    F(any)                        \
    F(append)                     \
    F(apply)                      \
    F(arguments)                  \
    F(asIntN)                     \
    F(asUintN)                    \
    F(asin)                       \
    F(asinh)                      \
    F(assert)                     \
    F(assign)                     \
    F(async)                      \
    F(asyncIterator)              \
    F(at)                         \
    F(atan)                       \
    F(atan2)                      \
    F(atanh)                      \
    F(as)                         \
    F(await)                      \
    F(baseName)                   \
    F(detached)                   \
    F(big)                        \
    F(bigint)                     \
    F(bind)                       \
    F(blink)                      \
    F(bold)                       \
    F(boolean)                    \
    F(buffer)                     \
    F(byteLength)                 \
    F(byteOffset)                 \
    F(calendar)                   \
    F(call)                       \
    F(callee)                     \
    F(caller)                     \
    F(caseFirst)                  \
    F(cause)                      \
    F(cbrt)                       \
    F(ceil)                       \
    F(charAt)                     \
    F(charCodeAt)                 \
    F(cleanupSome)                \
    F(clear)                      \
    F(clz32)                      \
    F(codePointAt)                \
    F(collation)                  \
    F(compare)                    \
    F(compile)                    \
    F(concat)                     \
    F(configurable)               \
    F(construct)                  \
    F(constructor)                \
    F(copyWithin)                 \
    F(cos)                        \
    F(cosh)                       \
    F(create)                     \
    F(debugger)                   \
    F(decodeURI)                  \
    F(decodeURIComponent)         \
    F(defineProperties)           \
    F(defineProperty)             \
    F(deleteProperty)             \
    F(deref)                      \
    F(description)                \
    F(done)                       \
    F(dotAll)                     \
    F(encodeURI)                  \
    F(encodeURIComponent)         \
    F(endsWith)                   \
    F(entries)                    \
    F(enumerable)                 \
    F(error)                      \
    F(escape)                     \
    F(eval)                       \
    F(every)                      \
    F(exec)                       \
    F(exp)                        \
    F(extends)                    \
    F(expm1)                      \
    F(fill)                       \
    F(filter)                     \
    F(finally)                    \
    F(find)                       \
    F(findIndex)                  \
    F(findLast)                   \
    F(findLastIndex)              \
    F(fixed)                      \
    F(flags)                      \
    F(flat)                       \
    F(flatMap)                    \
    F(floor)                      \
    F(fontcolor)                  \
    F(fontsize)                   \
    F(forEach)                    \
    F(format)                     \
    F(formatToParts)              \
    F(freeze)                     \
    F(from)                       \
    F(fromCharCode)               \
    F(fromCodePoint)              \
    F(fromEntries)                \
    F(fround)                     \
    F(function)                   \
    F(gc)                         \
    F(get)                        \
    F(getBigInt64)                \
    F(getBigUint64)               \
    F(getCanonicalLocales)        \
    F(getDate)                    \
    F(getDay)                     \
    F(getFloat32)                 \
    F(getFloat64)                 \
    F(getFullYear)                \
    F(getHours)                   \
    F(getInt16)                   \
    F(getInt32)                   \
    F(getInt8)                    \
    F(getMilliseconds)            \
    F(getMinutes)                 \
    F(getMonth)                   \
    F(getOwnPropertyDescriptor)   \
    F(getOwnPropertyDescriptors)  \
    F(getOwnPropertyNames)        \
    F(getOwnPropertySymbols)      \
    F(getPrototypeOf)             \
    F(getSeconds)                 \
    F(getTime)                    \
    F(getTimezoneOffset)          \
    F(getUTCDate)                 \
    F(getUTCDay)                  \
    F(getUTCFullYear)             \
    F(getUTCHours)                \
    F(getUTCMilliseconds)         \
    F(getUTCMinutes)              \
    F(getUTCMonth)                \
    F(getUTCSeconds)              \
    F(getUint16)                  \
    F(getUint32)                  \
    F(getUint8)                   \
    F(getYear)                    \
    F(global)                     \
    F(globalThis)                 \
    F(groups)                     \
    F(groupBy)                    \
    F(grow)                       \
    F(growable)                   \
    F(has)                        \
    F(hasIndices)                 \
    F(hasInstance)                \
    F(hasOwn)                     \
    F(hasOwnProperty)             \
    F(hourCycle)                  \
    F(hypot)                      \
    F(ignoreCase)                 \
    F(implements)                 \
    F(imul)                       \
    F(includes)                   \
    F(indices)                    \
    F(index)                      \
    F(indexOf)                    \
    F(input)                      \
    F(instanceof)                 \
    F(interface)                  \
    F(is)                         \
    F(isArray)                    \
    F(isConcatSpreadable)         \
    F(isExtensible)               \
    F(isFinite)                   \
    F(isFrozen)                   \
    F(isInteger)                  \
    F(isNaN)                      \
    F(isPrototypeOf)              \
    F(isSafeInteger)              \
    F(isSealed)                   \
    F(isWellFormed)               \
    F(isView)                     \
    F(italics)                    \
    F(iterator)                   \
    F(join)                       \
    F(keyFor)                     \
    F(keys)                       \
    F(language)                   \
    F(lastIndex)                  \
    F(lastIndexOf)                \
    F(lastMatch)                  \
    F(lastParen)                  \
    F(leftContext)                \
    F(length)                     \
    F(let)                        \
    F(link)                       \
    F(load)                       \
    F(localeCompare)              \
    F(log)                        \
    F(log10)                      \
    F(log1p)                      \
    F(log2)                       \
    F(map)                        \
    F(match)                      \
    F(matchAll)                   \
    F(max)                        \
    F(maxByteLength)              \
    F(maximize)                   \
    F(message)                    \
    F(min)                        \
    F(minimize)                   \
    F(multiline)                  \
    F(name)                       \
    F(next)                       \
    F(nextMethod)                 \
    F(normalize)                  \
    F(now)                        \
    F(null)                       \
    F(number)                     \
    F(numberingSystem)            \
    F(numeric)                    \
    F(object)                     \
    F(of)                         \
    F(ownKeys)                    \
    F(package)                    \
    F(padEnd)                     \
    F(padStart)                   \
    F(parse)                      \
    F(parseFloat)                 \
    F(parseInt)                   \
    F(pop)                        \
    F(pow)                        \
    F(preventExtensions)          \
    F(print)                      \
    F(propertyIsEnumerable)       \
    F(prototype)                  \
    F(proxy)                      \
    F(push)                       \
    F(race)                       \
    F(random)                     \
    F(raw)                        \
    F(read)                       \
    F(reduce)                     \
    F(reduceRight)                \
    F(region)                     \
    F(reject)                     \
    F(remainingElements)          \
    F(repeat)                     \
    F(replace)                    \
    F(replaceAll)                 \
    F(resizable)                  \
    F(resize)                     \
    F(resolve)                    \
    F(resolvedOptions)            \
    F(reverse)                    \
    F(revocable)                  \
    F(revoke)                     \
    F(rightContext)               \
    F(round)                      \
    F(run)                        \
    F(script)                     \
    F(seal)                       \
    F(search)                     \
    F(select)                     \
    F(set)                        \
    F(setBigInt64)                \
    F(setBigUint64)               \
    F(setDate)                    \
    F(setFloat32)                 \
    F(setFloat64)                 \
    F(setFullYear)                \
    F(setHours)                   \
    F(setInt16)                   \
    F(setInt32)                   \
    F(setInt8)                    \
    F(setMilliseconds)            \
    F(setMinutes)                 \
    F(setMonth)                   \
    F(setPrototypeOf)             \
    F(setSeconds)                 \
    F(setTime)                    \
    F(setUTCDate)                 \
    F(setUTCFullYear)             \
    F(setUTCHours)                \
    F(setUTCMilliseconds)         \
    F(setUTCMinutes)              \
    F(setUTCMonth)                \
    F(setUTCSeconds)              \
    F(setUint16)                  \
    F(setUint32)                  \
    F(setUint8)                   \
    F(setYear)                    \
    F(shift)                      \
    F(sign)                       \
    F(sin)                        \
    F(sinh)                       \
    F(size)                       \
    F(slice)                      \
    F(small)                      \
    F(some)                       \
    F(sort)                       \
    F(source)                     \
    F(species)                    \
    F(splice)                     \
    F(split)                      \
    F(sqrt)                       \
    F(stack)                      \
    F(startsWith)                 \
    F(sticky)                     \
    F(strike)                     \
    F(string)                     \
    F(stringify)                  \
    F(sub)                        \
    F(subarray)                   \
    F(substr)                     \
    F(substring)                  \
    F(sup)                        \
    F(super)                      \
    F(supportedLocalesOf)         \
    F(symbol)                     \
    F(tan)                        \
    F(tanh)                       \
    F(test)                       \
    F(then)                       \
    F(toDateString)               \
    F(toExponential)              \
    F(toFixed)                    \
    F(toGMTString)                \
    F(toISOString)                \
    F(toJSON)                     \
    F(toLocaleDateString)         \
    F(toLocaleLowerCase)          \
    F(toLocaleString)             \
    F(toLocaleTimeString)         \
    F(toLocaleUpperCase)          \
    F(toLowerCase)                \
    F(toPrecision)                \
    F(toPrimitive)                \
    F(toReversed)                 \
    F(toSorted)                   \
    F(toSpliced)                  \
    F(toString)                   \
    F(toStringTag)                \
    F(toTimeString)               \
    F(toUTCString)                \
    F(toUpperCase)                \
    F(toWellFormed)               \
    F(transfer)                   \
    F(transferToFixedLength)      \
    F(trim)                       \
    F(trimEnd)                    \
    F(trimLeft)                   \
    F(trimRight)                  \
    F(trimStart)                  \
    F(trunc)                      \
    F(undefined)                  \
    F(unescape)                   \
    F(unicode)                    \
    F(unicodeSets)                \
    F(unregister)                 \
    F(unscopables)                \
    F(unshift)                    \
    F(value)                      \
    F(valueOf)                    \
    F(values)                     \
    F(var)                        \
    F(with)                       \
    F(withResolvers)              \
    F(writable)                   \
    F(yield)

#if defined(ENABLE_WASM)
#define FOR_EACH_STATIC_WASM_STRING(F) \
    F(CompileError)                    \
    F(Global)                          \
    F(Instance)                        \
    F(LinkError)                       \
    F(Memory)                          \
    F(RuntimeError)                    \
    F(Table)                           \
    F(WebAssembly)                     \
    F(anyfunc)                         \
    F(customSections)                  \
    F(element)                         \
    F(exports)                         \
    F(externref)                       \
    F(f32)                             \
    F(f64)                             \
    F(i32)                             \
    F(i64)                             \
    F(imports)                         \
    F(initial)                         \
    F(instance)                        \
    F(instantiate)                     \
    F(kind)                            \
    F(maximum)                         \
    F(memory)                          \
    F(module)                          \
    F(table)                           \
    F(shared)                          \
    F(v128)                            \
    F(validate)
#else
#define FOR_EACH_STATIC_WASM_STRING(F)
#endif

#if defined(ENABLE_THREADING)
#define FOR_EACH_STATIC_THREADING_STRING(F) \
    F(Atomics)                              \
    F(SharedArrayBuffer)                    \
    F(compareExchange)                      \
    F(exchange)                             \
    F(isLockFree)                           \
    F(notify)                               \
    F(store)                                \
    F(wait)                                 \
    F(waitAsync)
#else
#define FOR_EACH_STATIC_THREADING_STRING(F)
#endif

#define FOR_EACH_STATIC_NUMBER(F) \
    F(0)                          \
    F(1)                          \
    F(2)                          \
    F(3)                          \
    F(4)                          \
    F(5)                          \
    F(6)                          \
    F(7)                          \
    F(8)                          \
    F(9)                          \
    F(10)                         \
    F(11)                         \
    F(12)                         \
    F(13)                         \
    F(14)                         \
    F(15)                         \
    F(16)                         \
    F(17)                         \
    F(18)                         \
    F(19)                         \
    F(20)                         \
    F(21)                         \
    F(22)                         \
    F(23)                         \
    F(24)                         \
    F(25)                         \
    F(26)                         \
    F(27)                         \
    F(28)                         \
    F(29)                         \
    F(30)                         \
    F(31)                         \
    F(32)                         \
    F(33)                         \
    F(34)                         \
    F(35)                         \
    F(36)                         \
    F(37)                         \
    F(38)                         \
    F(39)                         \
    F(40)                         \
    F(41)                         \
    F(42)                         \
    F(43)                         \
    F(44)                         \
    F(45)                         \
    F(46)                         \
    F(47)                         \
    F(48)                         \
    F(49)                         \
    F(50)                         \
    F(51)                         \
    F(52)                         \
    F(53)                         \
    F(54)                         \
    F(55)                         \
    F(56)                         \
    F(57)                         \
    F(58)                         \
    F(59)                         \
    F(60)                         \
    F(61)                         \
    F(62)                         \
    F(63)                         \
    F(64)                         \
    F(65)                         \
    F(66)                         \
    F(67)                         \
    F(68)                         \
    F(69)                         \
    F(70)                         \
    F(71)                         \
    F(72)                         \
    F(73)                         \
    F(74)                         \
    F(75)                         \
    F(76)                         \
    F(77)                         \
    F(78)                         \
    F(79)                         \
    F(80)                         \
    F(81)                         \
    F(82)                         \
    F(83)                         \
    F(84)                         \
    F(85)                         \
    F(86)                         \
    F(87)                         \
    F(88)                         \
    F(89)                         \
    F(90)                         \
    F(91)                         \
    F(92)                         \
    F(93)                         \
    F(94)                         \
    F(95)                         \
    F(96)                         \
    F(97)                         \
    F(98)                         \
    F(99)                         \
    F(100)                        \
    F(101)                        \
    F(102)                        \
    F(103)                        \
    F(104)                        \
    F(105)                        \
    F(106)                        \
    F(107)                        \
    F(108)                        \
    F(109)                        \
    F(110)                        \
    F(111)                        \
    F(112)                        \
    F(113)                        \
    F(114)                        \
    F(115)                        \
    F(116)                        \
    F(117)                        \
    F(118)                        \
    F(119)                        \
    F(120)                        \
    F(121)                        \
    F(122)                        \
    F(123)                        \
    F(124)                        \
    F(125)                        \
    F(126)                        \
    F(127)

// name on code, string
#define FOR_EACH_LAZY_STATIC_STRING(F)               \
    F(DotDotDotArgs, "...args")                      \
    F(EvalCode, "eval code")                         \
    F(Fulfilled, "fulfilled")                        \
    F(ObjectArgumentsToString, "[object Arguments]") \
    F(ObjectArrayToString, "[object Array]")         \
    F(ObjectBooleanToString, "[object Boolean]")     \
    F(ObjectDateToString, "[object Date]")           \
    F(ObjectErrorToString, "[object Error]")         \
    F(ObjectFunctionToString, "[object Function]")   \
    F(ObjectNullToString, "[object Null]")           \
    F(ObjectNumberToString, "[object Number]")       \
    F(ObjectObjectToString, "[object Object]")       \
    F(ObjectRegExpToString, "[object RegExp]")       \
    F(ObjectStringToString, "[object String]")       \
    F(ObjectUndefinedToString, "[object Undefined]") \
    F(Promise, "promise")                            \
    F(Reason, "reason")                              \
    F(Rejected, "rejected")                          \
    F(Status, "status")                              \
    F(SuperDotDotDotArgs, "super(...args)")          \
    F(URL, "url")

#if defined(ENABLE_INTL)
#define FOR_EACH_LAZY_INTL_STATIC_STRING(F)                 \
    F(Accent, "accent")                                     \
    F(Accounting, "accounting")                             \
    F(Always, "always")                                     \
    F(Any, "any")                                           \
    F(Auto, "auto")                                         \
    F(Base, "base")                                         \
    F(Basic, "basic")                                       \
    F(BestFit, "best fit")                                  \
    F(Calendars, "calendars")                               \
    F(Cardinal, "cardinal")                                 \
    F(Case, "case")                                         \
    F(Code, "code")                                         \
    F(Collations, "collations")                             \
    F(Compact, "compact")                                   \
    F(CompactDisplay, "compactDisplay")                     \
    F(CompareFunction, "compareFunction")                   \
    F(Conjunction, "conjunction")                           \
    F(Constrain, "constrain")                               \
    F(Currency, "currency")                                 \
    F(CurrencyDisplay, "currencyDisplay")                   \
    F(CurrencySign, "currencySign")                         \
    F(DashU, "-u")                                          \
    F(DataLocale, "dataLocale")                             \
    F(Date, "date")                                         \
    F(DateStyle, "dateStyle")                               \
    F(DateTimeField, "dateTimeField")                       \
    F(Day, "day")                                           \
    F(DayPeriod, "dayPeriod")                               \
    F(Decimal, "decimal")                                   \
    F(Dialect, "dialect")                                   \
    F(Disjunction, "disjunction")                           \
    F(Engineering, "engineering")                           \
    F(Era, "era")                                           \
    F(ExceptZero, "exceptZero")                             \
    F(ExponentInteger, "exponentInteger")                   \
    F(ExponentMinusSign, "exponentMinusSign")               \
    F(ExponentSeparator, "exponentSeparator")               \
    F(Fallback, "fallback")                                 \
    F(FormatMatcher, "formatMatcher")                       \
    F(Fraction, "fraction")                                 \
    F(FractionalSecond, "fractionalSecond")                 \
    F(FractionalSecondDigits, "fractionalSecondDigits")     \
    F(Full, "full")                                         \
    F(Group, "group")                                       \
    F(H11, "h11")                                           \
    F(H12, "h12")                                           \
    F(H23, "h23")                                           \
    F(H24, "h24")                                           \
    F(Hour, "hour")                                         \
    F(Hour12, "hour12")                                     \
    F(HourCycles, "hourCycles")                             \
    F(IgnorePunctuation, "ignorePunctuation")               \
    F(InitializedCollator, "initializedCollator")           \
    F(InitializedIntlObject, "initializedIntlObject")       \
    F(InitializedNumberFormat, "initializedNumberFormat")   \
    F(Integer, "integer")                                   \
    F(Kf, "kf")                                             \
    F(Kn, "kn")                                             \
    F(LanguageDisplay, "languageDisplay")                   \
    F(Literal, "literal")                                   \
    F(LocaleMatcher, "localeMatcher")                       \
    F(Long, "long")                                         \
    F(Lookup, "lookup")                                     \
    F(Lower, "lower")                                       \
    F(MaximumFractionDigits, "maximumFractionDigits")       \
    F(MaximumSignificantDigits, "maximumSignificantDigits") \
    F(Medium, "medium")                                     \
    F(MinimumFractionDigits, "minimumFractionDigits")       \
    F(MinimumIntegerDigits, "minimumIntegerDigits")         \
    F(MinimumSignificantDigits, "minimumSignificantDigits") \
    F(MinusSign, "minusSign")                               \
    F(Minute, "minute")                                     \
    F(Month, "month")                                       \
    F(Narrow, "narrow")                                     \
    F(NarrowSymbol, "narrowSymbol")                         \
    F(Never, "never")                                       \
    F(None, "none")                                         \
    F(Notation, "notation")                                 \
    F(NumberingSystems, "numberingSystems")                 \
    F(Ordinal, "ordinal")                                   \
    F(Percent, "percent")                                   \
    F(PercentSign, "percentSign")                           \
    F(PlusSign, "plusSign")                                 \
    F(Quarter, "quarter")                                   \
    F(Scientific, "scientific")                             \
    F(Second, "second")                                     \
    F(Sensitivity, "sensitivity")                           \
    F(Short, "short")                                       \
    F(SignDisplay, "signDisplay")                           \
    F(SmallLetterInfinity, "infinity")                      \
    F(SmallLetterLocale, "locale")                          \
    F(SmallLetterNaN, "nan")                                \
    F(Standard, "standard")                                 \
    F(Style, "style")                                       \
    F(TextInfo, "textInfo")                                 \
    F(Time, "time")                                         \
    F(TimeStyle, "timeStyle")                               \
    F(TimeZone, "timeZone")                                 \
    F(TimeZones, "timeZones")                               \
    F(TimeZoneName, "timeZoneName")                         \
    F(TwoDigit, "2-digit")                                  \
    F(Type, "type")                                         \
    F(Unit, "unit")                                         \
    F(UnitDisplay, "unitDisplay")                           \
    F(Upper, "upper")                                       \
    F(Usage, "usage")                                       \
    F(UseGrouping, "useGrouping")                           \
    F(Variant, "variant")                                   \
    F(Week, "week")                                         \
    F(Weekday, "weekday")                                   \
    F(WeekInfo, "weekInfo")                                 \
    F(Year, "year")
#else
#define FOR_EACH_LAZY_INTL_STATIC_STRING(F)
#endif

#if defined(ENABLE_TEMPORAL)
#define FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(F)   \
    F(CalendarId, "calendarId")                   \
    F(DayOfWeek, "dayOfWeek")                     \
    F(DayOfYear, "dayOfYear")                     \
    F(DaysInMonth, "daysInMonth")                 \
    F(DaysInWeek, "daysInWeek")                   \
    F(DaysInYear, "daysInYear")                   \
    F(EraYear, "eraYear")                         \
    F(InLeapYear, "inLeapYear")                   \
    F(ISO8601, "iso8601")                         \
    F(MonthCode, "monthCode")                     \
    F(MonthsInYear, "monthsInYear")               \
    F(Overflow, "overflow")                       \
    F(PlainDate, "PlainDate")                     \
    F(Temporal, "Temporal")                       \
    F(TemporalDotPlainDate, "Temporal.PlainDate") \
    F(WeekOfYear, "weekOfYear")                   \
    F(YearOfWeek, "yearOfWeek")
#else
#define FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(F)
#endif

#if defined(ENABLE_THREADING)
#define FOR_EACH_LAZY_THREADING_STATIC_STRING(F) \
    F(NotEqual, "not-equal")                     \
    F(Ok, "ok")                                  \
    F(TimedOut, "timed-out")
#else
#define FOR_EACH_LAZY_THREADING_STATIC_STRING(F)
#endif

#define ESCARGOT_ASCII_TABLE_MAX 256
#define ESCARGOT_STRINGS_NUMBERS_MAX 128

class StaticStrings {
public:
    StaticStrings(AtomicStringMap* atomicStringMap)
        : dtoaCacheSize(5)
        , m_atomicStringMap(atomicStringMap)
    {
        asciiTable = new (malloc(sizeof(AtomicString) * ESCARGOT_ASCII_TABLE_MAX)) AtomicString[ESCARGOT_ASCII_TABLE_MAX];
        numbers = new (malloc(sizeof(AtomicString) * ESCARGOT_STRINGS_NUMBERS_MAX)) AtomicString[ESCARGOT_STRINGS_NUMBERS_MAX];
    }

    ~StaticStrings()
    {
        free(asciiTable);
        free(numbers);
    }

    Escargot::String* charCodeToString(char32_t ch)
    {
        if (LIKELY(ch < ESCARGOT_ASCII_TABLE_MAX)) {
            return asciiTable[ch].string();
        }
        return String::fromCharCode(ch);
    }

    // keyword string
    AtomicString stringBreak;
    AtomicString stringCase;
    AtomicString stringCatch;
    AtomicString stringClass;
    AtomicString stringConst;
    AtomicString stringContinue;
    AtomicString stringDefault;
    AtomicString stringDelete;
    AtomicString stringDo;
    AtomicString stringElse;
    AtomicString stringEnum;
    AtomicString stringExport;
    AtomicString stringFalse;
    AtomicString stringFor;
    AtomicString stringIf;
    AtomicString stringImport;
    AtomicString stringIn;
    AtomicString stringMutable;
    AtomicString stringNew;
    AtomicString stringPrivate;
    AtomicString stringProtected;
    AtomicString stringPublic;
    AtomicString stringRegister;
    AtomicString stringReturn;
    AtomicString stringStarDefaultStar;
    AtomicString stringStarNamespaceStar;
    AtomicString stringStatic;
    AtomicString stringSwitch;
    AtomicString stringThis;
    AtomicString stringThrow;
    AtomicString stringTrue;
    AtomicString stringTry;
    AtomicString stringTypeof;
    AtomicString stringVoid;
    AtomicString stringWhile;
#if defined(ENABLE_THREADING)
    AtomicString stringAnd;
    AtomicString stringOr;
    AtomicString stringXor;
#endif

    AtomicString $Ampersand;
    AtomicString $Apostrophe;
    AtomicString $GraveAccent;
    AtomicString $PlusSign;

    AtomicString NegativeInfinity;
    AtomicString defaultRegExpString;
    AtomicString getBaseName;
    AtomicString getBuffer;
    AtomicString getCalendar;
    AtomicString getCalendars;
    AtomicString getCaseFirst;
    AtomicString getCollation;
    AtomicString getCollations;
    AtomicString getCompare;
    AtomicString getDescription;
    AtomicString getDetached;
    AtomicString getDotAll;
    AtomicString getFlags;
    AtomicString getFormat;
    AtomicString getGlobal;
    AtomicString getHasIndices;
    AtomicString getHourCycle;
    AtomicString getHourCycles;
    AtomicString getIgnoreCase;
    AtomicString getLanguage;
    AtomicString getLength;
    AtomicString getMultiline;
    AtomicString getNumberingSystem;
    AtomicString getNumberingSystems;
    AtomicString getNumeric;
    AtomicString getRegion;
    AtomicString getScript;
    AtomicString getSize;
    AtomicString getSource;
    AtomicString getSticky;
    AtomicString getSymbolSpecies;
    AtomicString getSymbolToStringTag;
    AtomicString getTextInfo;
    AtomicString getTimeZones;
    AtomicString getUnicode;
    AtomicString getUnicodeSets;
    AtomicString getWeekInfo;
    AtomicString get__proto__;
    AtomicString getbyteLength;
    AtomicString getbyteOffset;
    AtomicString getgrowable;
    AtomicString getmaxByteLength;
    AtomicString getresizable;
    AtomicString intlDotCollator;
    AtomicString intlDotDisplayNames;
    AtomicString intlDotListFormat;
    AtomicString intlDotLocale;
    AtomicString intlDotPluralRules;
    AtomicString intlDotRelativeTimeFormat;
    AtomicString set__proto__;
    AtomicString symbolMatch;
    AtomicString symbolMatchAll;
    AtomicString symbolReplace;
    AtomicString symbolSearch;
    AtomicString symbolSplit;

#if defined(ENABLE_WASM)
    AtomicString getExports;
    AtomicString getValue;
    AtomicString setValue;
    AtomicString WebAssemblyDotGlobal;
    AtomicString WebAssemblyDotInstance;
    AtomicString WebAssemblyDotMemory;
    AtomicString WebAssemblyDotModule;
    AtomicString WebAssemblyDotTable;
#endif


    AtomicString* asciiTable;
    AtomicString* numbers;

#define DECLARE_STATIC_STRING(name) AtomicString name;
    FOR_EACH_STATIC_STRING(DECLARE_STATIC_STRING);
    FOR_EACH_STATIC_WASM_STRING(DECLARE_STATIC_STRING);
    FOR_EACH_STATIC_THREADING_STRING(DECLARE_STATIC_STRING);
#undef DECLARE_STATIC_STRING

#define DECLARE_LAZY_STATIC_STRING(Name, unused) AtomicString lazy##Name();
    FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING

    void initStaticStrings();

    const size_t dtoaCacheSize; // 5;
    mutable Vector<std::pair<double, ::Escargot::String*>, GCUtil::gc_malloc_allocator<std::pair<double, ::Escargot::String*>>> dtoaCache;

    ::Escargot::String* dtoa(double d) const;

protected:
    AtomicStringMap* m_atomicStringMap;

#define DECLARE_LAZY_STATIC_STRING(Name, unused) AtomicString m_lazy##Name;
    FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING
};
} // namespace Escargot

#endif
