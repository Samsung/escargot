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
    F(AggregateError)             \
    F(Array)                      \
    F(ArrayBuffer)                \
    F(AsyncDisposableStack)       \
    F(AsyncFunction)              \
    F(AsyncGenerator)             \
    F(AsyncGeneratorFunction)     \
    F(BYTES_PER_ELEMENT)          \
    F(BigInt)                     \
    F(BigInt64Array)              \
    F(BigUint64Array)             \
    F(Boolean)                    \
    F(DataView)                   \
    F(Date)                       \
    F(DisposableStack)            \
    F(Empty)                      \
    F(Error)                      \
    F(EvalError)                  \
    F(FinalizationRegistry)       \
    F(Float16Array)               \
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
    F(Map)                        \
    F(Math)                       \
    F(Module)                     \
    F(NaN)                        \
    F(Number)                     \
    F(Object)                     \
    F(Promise)                    \
    F(Proxy)                      \
    F(RangeError)                 \
    F(ReferenceError)             \
    F(Reflect)                    \
    F(RegExp)                     \
    F(Set)                        \
    F(ShadowRealm)                \
    F(String)                     \
    F(Symbol)                     \
    F(SuppressedError)            \
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
    F(add)                        \
    F(adopt)                      \
    F(all)                        \
    F(allSettled)                 \
    F(alphabet)                   \
    F(anchor)                     \
    F(anonymous)                  \
    F(any)                        \
    F(append)                     \
    F(apply)                      \
    F(arguments)                  \
    F(asIntN)                     \
    F(asUintN)                    \
    F(assign)                     \
    F(async)                      \
    F(asyncDispose)               \
    F(asyncIterator)              \
    F(at)                         \
    F(as)                         \
    F(await)                      \
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
    F(call)                       \
    F(callee)                     \
    F(caller)                     \
    F(cause)                      \
    F(ceil)                       \
    F(charAt)                     \
    F(cleanupSome)                \
    F(clear)                      \
    F(codePointAt)                \
    F(compile)                    \
    F(concat)                     \
    F(configurable)               \
    F(construct)                  \
    F(constructor)                \
    F(copyWithin)                 \
    F(create)                     \
    F(debugger)                   \
    F(decodeURI)                  \
    F(decodeURIComponent)         \
    F(defer)                      \
    F(defineProperties)           \
    F(defineProperty)             \
    F(deleteProperty)             \
    F(deref)                      \
    F(description)                \
    F(done)                       \
    F(dotAll)                     \
    F(dispose)                    \
    F(disposeAsync)               \
    F(disposed)                   \
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
    F(extends)                    \
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
    F(freeze)                     \
    F(from)                       \
    F(fromEntries)                \
    F(function)                   \
    F(get)                        \
    F(getBigInt64)                \
    F(getBigUint64)               \
    F(getDate)                    \
    F(getDay)                     \
    F(getFloat16)                 \
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
    F(getOrInsert)                \
    F(getOrInsertComputed)        \
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
    F(has)                        \
    F(hasIndices)                 \
    F(hasInstance)                \
    F(hasOwn)                     \
    F(hasOwnProperty)             \
    F(ignoreCase)                 \
    F(implements)                 \
    F(importValue)                \
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
    F(isError)                    \
    F(isExtensible)               \
    F(isFinite)                   \
    F(isFrozen)                   \
    F(isInteger)                  \
    F(isRawJSON)                  \
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
    F(lastChunkHandling)          \
    F(lastIndex)                  \
    F(lastIndexOf)                \
    F(length)                     \
    F(let)                        \
    F(link)                       \
    F(load)                       \
    F(map)                        \
    F(match)                      \
    F(matchAll)                   \
    F(maxByteLength)              \
    F(message)                    \
    F(min)                        \
    F(mode)                       \
    F(move)                       \
    F(multiline)                  \
    F(name)                       \
    F(next)                       \
    F(nextMethod)                 \
    F(now)                        \
    F(null)                       \
    F(number)                     \
    F(object)                     \
    F(of)                         \
    F(omitPadding)                \
    F(ownKeys)                    \
    F(package)                    \
    F(padding)                    \
    F(parse)                      \
    F(parseFloat)                 \
    F(parseInt)                   \
    F(pop)                        \
    F(pow)                        \
    F(preventExtensions)          \
    F(propertyIsEnumerable)       \
    F(prototype)                  \
    F(proxy)                      \
    F(push)                       \
    F(race)                       \
    F(raw)                        \
    F(rawJSON)                    \
    F(read)                       \
    F(reduce)                     \
    F(reduceRight)                \
    F(reject)                     \
    F(replace)                    \
    F(replaceAll)                 \
    F(resizable)                  \
    F(resize)                     \
    F(resolve)                    \
    F(reverse)                    \
    F(revoke)                     \
    F(round)                      \
    F(run)                        \
    F(seal)                       \
    F(search)                     \
    F(set)                        \
    F(setBigInt64)                \
    F(setBigUint64)               \
    F(setDate)                    \
    F(setFloat16)                 \
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
    F(sign)                       \
    F(size)                       \
    F(slice)                      \
    F(small)                      \
    F(some)                       \
    F(sort)                       \
    F(source)                     \
    F(species)                    \
    F(split)                      \
    F(stack)                      \
    F(startsWith)                 \
    F(sticky)                     \
    F(strike)                     \
    F(string)                     \
    F(stringify)                  \
    F(sub)                        \
    F(substr)                     \
    F(substring)                  \
    F(sup)                        \
    F(super)                      \
    F(suppressed)                 \
    F(symbol)                     \
    F(test)                       \
    F(then)                       \
    F(toDateString)               \
    F(toBase64)                   \
    F(toExponential)              \
    F(toFixed)                    \
    F(toISOString)                \
    F(toJSON)                     \
    F(toLocaleDateString)         \
    F(toLocaleString)             \
    F(toLocaleTimeString)         \
    F(toPrecision)                \
    F(toPrimitive)                \
    F(toReversed)                 \
    F(toSorted)                   \
    F(toSpliced)                  \
    F(toString)                   \
    F(toStringTag)                \
    F(toTimeString)               \
    F(toUTCString)                \
    F(toWellFormed)               \
    F(transfer)                   \
    F(transferToFixedLength)      \
    F(trim)                       \
    F(trimEnd)                    \
    F(trimStart)                  \
    F(trunc)                      \
    F(undefined)                  \
    F(unescape)                   \
    F(unicode)                    \
    F(unicodeSets)                \
    F(unscopables)                \
    F(use)                        \
    F(value)                      \
    F(valueOf)                    \
    F(values)                     \
    F(var)                        \
    F(with)                       \
    F(withResolvers)              \
    F(writable)                   \
    F(written)                    \
    F(yield)

#if defined(ENABLE_WASM)
#define FOR_EACH_LAZY_WASM_STATIC_STRING(F)                      \
    F(WASMCapitalCompileError, "CompileError")                   \
    F(WASMCapitalGlobal, "Global")                               \
    F(WASMCapitalInstance, "Instance")                           \
    F(WASMCapitalLinkError, "LinkError")                         \
    F(WASMCapitalMemory, "Memory")                               \
    F(WASMCapitalRuntimeError, "RuntimeError")                   \
    F(WASMCapitalTable, "Table")                                 \
    F(WASMCapitalTag, "Tag")                                     \
    F(WASMCapitalWebAssembly, "WebAssembly")                     \
    F(WASMCapitalWebAssemblyDotGlobal, "WebAssembly.Global")     \
    F(WASMCapitalWebAssemblyDotInstance, "WebAssembly.Instance") \
    F(WASMCapitalWebAssemblyDotMemory, "WebAssembly.Memory")     \
    F(WASMCapitalWebAssemblyDotModule, "WebAssembly.Module")     \
    F(WASMCapitalWebAssemblyDotTable, "WebAssembly.Table")       \
    F(WASMCapitalWebAssemblyDotTag, "WebAssembly.Tag")           \
    F(WASMAnyfunc, "anyfunc")                                    \
    F(WASMCustomSections, "customSections")                      \
    F(WASMElement, "element")                                    \
    F(WASMExports, "exports")                                    \
    F(WASMExternref, "externref")                                \
    F(WASMF32, "f32")                                            \
    F(WASMF64, "f64")                                            \
    F(WASMI32, "i32")                                            \
    F(WASMI64, "i64")                                            \
    F(WASMImports, "imports")                                    \
    F(WASMInitial, "initial")                                    \
    F(WASMInstance, "instance")                                  \
    F(WASMInstantiate, "instantiate")                            \
    F(WASMKind, "kind")                                          \
    F(WASMMaximum, "maximum")                                    \
    F(WASMMemory, "memory")                                      \
    F(WASMModule, "module")                                      \
    F(WASMParameters, "parameters")                              \
    F(WASMSetValue, "set value")                                 \
    F(WASMShared, "shared")                                      \
    F(WASMTable, "table")                                        \
    F(WASMTag, "tag")                                            \
    F(WASMV128, "v128")                                          \
    F(WASMValidate, "validate")
#else
#define FOR_EACH_LAZY_WASM_STATIC_STRING(F)
#endif

#if defined(ENABLE_THREADING)
#define FOR_EACH_STATIC_THREADING_STRING(F) \
    F(Atomics)                              \
    F(SharedArrayBuffer)                    \
    F(exchange)                             \
    F(store)                                \
    F(wait)
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
    F(URL, "url")                                    \
    F(ArrayIterator, "ArrayIterator")                \
    F(MapIterator, "MapIterator")                    \
    F(RegExpStringIterator, "RegExpStringIterator")  \
    F(SetIterator, "SetIterator")                    \
    F(StringIterator, "StringIterator")

#if defined(ENABLE_INTL)
#define FOR_EACH_LAZY_INTL_STATIC_STRING(F)                 \
    F(Accent, "accent")                                     \
    F(Accounting, "accounting")                             \
    F(Always, "always")                                     \
    F(Any, "any")                                           \
    F(ApproximatelySign, "approximatelySign")               \
    F(Auto, "auto")                                         \
    F(Base, "base")                                         \
    F(Basic, "basic")                                       \
    F(BestFit, "best fit")                                  \
    F(BaseName, "baseName")                                 \
    F(Calendar, "calendar")                                 \
    F(Calendars, "calendars")                               \
    F(Cardinal, "cardinal")                                 \
    F(Case, "case")                                         \
    F(CaseFirst, "caseFirst")                               \
    F(CapitalCollator, "Collator")                          \
    F(CapitalDateTimeFormat, "DateTimeFormat")              \
    F(CapitalDisplayNames, "DisplayNames")                  \
    F(CapitalDurationFormat, "DurationFormat")              \
    F(CapitalListFormat, "ListFormat")                      \
    F(CapitalLocale, "Locale")                              \
    F(CapitalNumberFormat, "NumberFormat")                  \
    F(CapitalPluralRules, "PluralRules")                    \
    F(CapitalRelativeTimeFormat, "RelativeTimeFormat")      \
    F(CapitalSegmenter, "Segmenter")                        \
    F(Code, "code")                                         \
    F(Collation, "collation")                               \
    F(Collations, "collations")                             \
    F(Compact, "compact")                                   \
    F(CompactDisplay, "compactDisplay")                     \
    F(Compare, "compare")                                   \
    F(CompareFunction, "compareFunction")                   \
    F(Containing, "containing")                             \
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
    F(Days, "days")                                         \
    F(DayPeriod, "dayPeriod")                               \
    F(Decimal, "decimal")                                   \
    F(Dialect, "dialect")                                   \
    F(Digital, "digital")                                   \
    F(Disjunction, "disjunction")                           \
    F(Engineering, "engineering")                           \
    F(Era, "era")                                           \
    F(ExceptZero, "exceptZero")                             \
    F(Expand, "expand")                                     \
    F(ExponentInteger, "exponentInteger")                   \
    F(ExponentMinusSign, "exponentMinusSign")               \
    F(ExponentSeparator, "exponentSeparator")               \
    F(Fallback, "fallback")                                 \
    F(FirstDayOfWeek, "firstDayOfWeek")                     \
    F(Format, "format")                                     \
    F(FormatRange, "formatRange")                           \
    F(FormatRangeToParts, "formatRangeToParts")             \
    F(FormatMatcher, "formatMatcher")                       \
    F(FormatToParts, "formatToParts")                       \
    F(Fraction, "fraction")                                 \
    F(Fractional, "fractional")                             \
    F(FractionalDigits, "fractionalDigits")                 \
    F(FractionalSecond, "fractionalSecond")                 \
    F(FractionalSecondDigits, "fractionalSecondDigits")     \
    F(Full, "full")                                         \
    F(GetCalendars, "getCalendars")                         \
    F(GetCanonicalLocales, "getCanonicalLocales")           \
    F(GetCollations, "getCollations")                       \
    F(GetHourCycles, "getHourCycles")                       \
    F(GetNumberingSystems, "getNumberingSystems")           \
    F(GetSpaceBaseName, "get baseName")                     \
    F(GetSpaceCalendar, "get calendar")                     \
    F(GetSpaceCalendars, "get calendars")                   \
    F(GetSpaceCaseFirst, "get caseFirst")                   \
    F(GetSpaceCollation, "get collation")                   \
    F(GetSpaceCollations, "get collations")                 \
    F(GetSpaceCompare, "get compare")                       \
    F(GetSpaceFirstDayOfWeek, "get firstDayOfWeek")         \
    F(GetSpaceFormat, "get format")                         \
    F(GetSpaceHourCycle, "get hourCycle")                   \
    F(GetSpaceHourCycles, "get hourCycles")                 \
    F(GetSpaceLanguage, "get language")                     \
    F(GetSpaceNumberingSystem, "get numberingSystem")       \
    F(GetSpaceNumberingSystems, "get numberingSystems")     \
    F(GetSpaceNumeric, "get numeric")                       \
    F(GetSpaceRegion, "get region")                         \
    F(GetSpaceScript, "get script")                         \
    F(GetSpaceTextInfo, "get textInfo")                     \
    F(GetSpaceTimeZones, "get timeZones")                   \
    F(GetSpaceVariants, "get variants")                     \
    F(GetSpaceWeekInfo, "get weekInfo")                     \
    F(GetTextInfo, "getTextInfo")                           \
    F(GetTimeZones, "getTimeZones")                         \
    F(GetWeekInfo, "getWeekInfo")                           \
    F(Grapheme, "grapheme")                                 \
    F(Granularity, "granularity")                           \
    F(Group, "group")                                       \
    F(H11, "h11")                                           \
    F(H12, "h12")                                           \
    F(H23, "h23")                                           \
    F(H24, "h24")                                           \
    F(HalfCeil, "halfCeil")                                 \
    F(HalfFloor, "halfFloor")                               \
    F(HalfExpand, "halfExpand")                             \
    F(HalfTrunc, "halfTrunc")                               \
    F(HalfEven, "halfEven")                                 \
    F(Hour, "hour")                                         \
    F(Hours, "hours")                                       \
    F(Hour12, "hour12")                                     \
    F(HourCycle, "hourCycle")                               \
    F(HourCycles, "hourCycles")                             \
    F(IgnorePunctuation, "ignorePunctuation")               \
    F(InitializedCollator, "initializedCollator")           \
    F(InitializedIntlObject, "initializedIntlObject")       \
    F(InitializedNumberFormat, "initializedNumberFormat")   \
    F(Integer, "integer")                                   \
    F(IntlDotCollator, "Intl.Collator")                     \
    F(IntlDotDisplayNames, "Intl.DisplayNames")             \
    F(IntlDotListFormat, "Intl.ListFormat")                 \
    F(IntlDotLocale, "Intl.Locale")                         \
    F(IntlDotPluralRules, "Intl.PluralRules")               \
    F(IntlDotNumberFormat, "Intl.NumberFormat")             \
    F(IntlDotDateTimeFormat, "Intl.DateTimeFormat")         \
    F(IntlDotDurationFormat, "Intl.DurationFormat")         \
    F(IntlDotRelativeTimeFormat, "Intl.RelativeTimeFormat") \
    F(IntlDotSegmenter, "Intl.Segmenter")                   \
    F(IsWordLike, "isWordLike")                             \
    F(Kf, "kf")                                             \
    F(Kn, "kn")                                             \
    F(Language, "language")                                 \
    F(LanguageDisplay, "languageDisplay")                   \
    F(LessPrecision, "lessPrecision")                       \
    F(Literal, "literal")                                   \
    F(LocaleMatcher, "localeMatcher")                       \
    F(Long, "long")                                         \
    F(LongOffset, "longOffset")                             \
    F(LongGeneric, "longGeneric")                           \
    F(Lookup, "lookup")                                     \
    F(Lower, "lower")                                       \
    F(Maximize, "maximize")                                 \
    F(MaximumFractionDigits, "maximumFractionDigits")       \
    F(MaximumSignificantDigits, "maximumSignificantDigits") \
    F(Medium, "medium")                                     \
    F(Millisecond, "millisecond")                           \
    F(Milliseconds, "milliseconds")                         \
    F(Microsecond, "microsecond")                           \
    F(Microseconds, "microseconds")                         \
    F(Minimize, "minimize")                                 \
    F(MinimumFractionDigits, "minimumFractionDigits")       \
    F(MinimumIntegerDigits, "minimumIntegerDigits")         \
    F(MinimumSignificantDigits, "minimumSignificantDigits") \
    F(MinusSign, "minusSign")                               \
    F(Minute, "minute")                                     \
    F(Minutes, "minutes")                                   \
    F(Month, "month")                                       \
    F(Months, "months")                                     \
    F(MorePrecision, "morePrecision")                       \
    F(Nanosecond, "nanosecond")                             \
    F(Nanoseconds, "nanoseconds")                           \
    F(Narrow, "narrow")                                     \
    F(NarrowSymbol, "narrowSymbol")                         \
    F(Negative, "negative")                                 \
    F(Never, "never")                                       \
    F(None, "none")                                         \
    F(Notation, "notation")                                 \
    F(Numeric, "numeric")                                   \
    F(NumberingSystem, "numberingSystem")                   \
    F(NumberingSystems, "numberingSystems")                 \
    F(Ordinal, "ordinal")                                   \
    F(Percent, "percent")                                   \
    F(PercentSign, "percentSign")                           \
    F(PlusSign, "plusSign")                                 \
    F(Region, "region")                                     \
    F(RelatedYear, "relatedYear")                           \
    F(ResolvedOptions, "resolvedOptions")                   \
    F(RoundingIncrement, "roundingIncrement")               \
    F(RoundingMode, "roundingMode")                         \
    F(RoundingPriority, "roundingPriority")                 \
    F(Quarter, "quarter")                                   \
    F(Scientific, "scientific")                             \
    F(Script, "script")                                     \
    F(Second, "second")                                     \
    F(Seconds, "seconds")                                   \
    F(Segment, "segment")                                   \
    F(Segments, "segments")                                 \
    F(Sentence, "sentence")                                 \
    F(Sensitivity, "sensitivity")                           \
    F(Select, "select")                                     \
    F(SelectRange, "selectRange")                           \
    F(Short, "short")                                       \
    F(ShortOffset, "shortOffset")                           \
    F(ShortGeneric, "shortGeneric")                         \
    F(SignDisplay, "signDisplay")                           \
    F(SignificantDigits, "significantDigits")               \
    F(SmallLetterInfinity, "infinity")                      \
    F(SmallLetterLocale, "locale")                          \
    F(SmallLetterNaN, "nan")                                \
    F(Standard, "standard")                                 \
    F(StripIfInteger, "stripIfInteger")                     \
    F(Style, "style")                                       \
    F(SupportedLocalesOf, "supportedLocalesOf")             \
    F(SupportedValuesOf, "supportedValuesOf")               \
    F(TextInfo, "textInfo")                                 \
    F(Time, "time")                                         \
    F(TimeStyle, "timeStyle")                               \
    F(TimeZone, "timeZone")                                 \
    F(TimeZones, "timeZones")                               \
    F(TimeZoneName, "timeZoneName")                         \
    F(TrailingZeroDisplay, "trailingZeroDisplay")           \
    F(TwoDigit, "2-digit")                                  \
    F(Type, "type")                                         \
    F(Unit, "unit")                                         \
    F(UnitDisplay, "unitDisplay")                           \
    F(Upper, "upper")                                       \
    F(Usage, "usage")                                       \
    F(UseGrouping, "useGrouping")                           \
    F(Variant, "variant")                                   \
    F(Variants, "variants")                                 \
    F(Week, "week")                                         \
    F(Weeks, "weeks")                                       \
    F(Weekday, "weekday")                                   \
    F(WeekInfo, "weekInfo")                                 \
    F(Word, "word")                                         \
    F(Year, "year")                                         \
    F(Years, "years")                                       \
    F(YearName, "yearName")
#else
#define FOR_EACH_LAZY_INTL_STATIC_STRING(F)
#endif

#if defined(ENABLE_TEMPORAL)
#define FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(F)             \
    F(Blank, "blank")                                       \
    F(CalendarId, "calendarId")                             \
    F(CalendarName, "calendarName")                         \
    F(CapitalDuration, "Duration")                          \
    F(CapitalInstant, "Instant")                            \
    F(CapitalNow, "Now")                                    \
    F(CapitalPlainDate, "PlainDate")                        \
    F(CapitalPlainDateTime, "PlainDateTime")                \
    F(CapitalPlainTime, "PlainTime")                        \
    F(CapitalPlainMonthDay, "PlainMonthDay")                \
    F(CapitalPlainYearMonth, "PlainYearMonth")              \
    F(CapitalTemporal, "Temporal")                          \
    F(CapitalZonedDateTime, "ZonedDateTime")                \
    F(Critical, "critical")                                 \
    F(Compatible, "compatible")                             \
    F(DayOfWeek, "dayOfWeek")                               \
    F(DayOfYear, "dayOfYear")                               \
    F(DaysInMonth, "daysInMonth")                           \
    F(DaysInWeek, "daysInWeek")                             \
    F(DaysInYear, "daysInYear")                             \
    F(Direction, "direction")                               \
    F(Disambiguation, "disambiguation")                     \
    F(Earlier, "earlier")                                   \
    F(EraYear, "eraYear")                                   \
    F(EpochMilliseconds, "epochMilliseconds")               \
    F(EpochNanoseconds, "epochNanoseconds")                 \
    F(Equals, "equals")                                     \
    F(FromEpochMilliseconds, "fromEpochMilliseconds")       \
    F(FromEpochNanoseconds, "fromEpochNanoseconds")         \
    F(GetEpochMilliseconds, "get epochMilliseconds")        \
    F(GetEpochNanoseconds, "get epochNanoseconds")          \
    F(GetTimeZoneTransition, "getTimeZoneTransition")       \
    F(HoursInDay, "hoursInDay")                             \
    F(Ignore, "ignore")                                     \
    F(InLeapYear, "inLeapYear")                             \
    F(Instant, "instant")                                   \
    F(ISO8601, "iso8601")                                   \
    F(LargestUnit, "largestUnit")                           \
    F(Later, "later")                                       \
    F(MonthCode, "monthCode")                               \
    F(MonthsInYear, "monthsInYear")                         \
    F(Negated, "negated")                                   \
    F(Offset, "offset")                                     \
    F(OffsetNanoseconds, "offsetNanoseconds")               \
    F(Overflow, "overflow")                                 \
    F(RelativeTo, "relativeTo")                             \
    F(Reject, "reject")                                     \
    F(PlainDateISO, "plainDateISO")                         \
    F(PlainDateTimeISO, "plainDateTimeISO")                 \
    F(PlainTime, "plainTime")                               \
    F(PlainTimeISO, "plainTimeISO")                         \
    F(Prefer, "prefer")                                     \
    F(Previous, "previous")                                 \
    F(Since, "since")                                       \
    F(StartOfDay, "startOfDay")                             \
    F(SmallestUnit, "smallestUnit")                         \
    F(Subtract, "subtract")                                 \
    F(TemporalDotDuration, "Temporal.Duration")             \
    F(TemporalDotInstant, "Temporal.Instant")               \
    F(TemporalDotNow, "Temporal.Now")                       \
    F(TemporalDotPlainDate, "Temporal.PlainDate")           \
    F(TemporalDotPlainDateTime, "Temporal.PlainDateTime")   \
    F(TemporalDotPlainTime, "Temporal.PlainTime")           \
    F(TemporalDotPlainMonthDay, "Temporal.PlainMonthDay")   \
    F(TemporalDotPlainYearMonth, "Temporal.PlainYearMonth") \
    F(TemporalDotZonedDateTime, "Temporal.ZonedDateTime")   \
    F(ToTemporalInstant, "toTemporalInstant")               \
    F(ToInstant, "toInstant")                               \
    F(Total, "total")                                       \
    F(ToPlainDate, "toPlainDate")                           \
    F(ToPlainDateTime, "toPlainDateTime")                   \
    F(ToPlainTime, "toPlainTime")                           \
    F(ToPlainMonthDay, "toPlainMonthDay")                   \
    F(ToPlainYearMonth, "toPlainYearMonth")                 \
    F(ToZonedDateTime, "toZonedDateTime")                   \
    F(ToZonedDateTimeISO, "toZonedDateTimeISO")             \
    F(TimeZoneId, "timeZoneId")                             \
    F(Until, "until")                                       \
    F(WeekOfYear, "weekOfYear")                             \
    F(WithCalendar, "withCalendar")                         \
    F(WithTimeZone, "withTimeZone")                         \
    F(WithPlainTime, "withPlainTime")                       \
    F(YearOfWeek, "yearOfWeek")                             \
    F(ZonedDateTimeISO, "zonedDateTimeISO")
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

#if defined(ESCARGOT_DEBUGGER)
#define FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(F)                          \
    F(DebuggerSectionPrototype, "[[Prototype]]")                         \
    F(DebuggerSectionEntries, "[[Entries]]")                             \
    F(DebuggerSectionInt8Array, "[[Int8Array]]")                         \
    F(DebuggerSectionUint8Array, "[[Uint8Array]]")                       \
    F(DebuggerSectionInt16Array, "[[Int16Array]]")                       \
    F(DebuggerSectionInt32Array, "[[Int32Array]]")                       \
    F(DebuggerSectionArrayBufferByteLength, "[[ArrayBufferByteLength]]") \
    F(DebuggerSectionHandler, "[[Handler]]")                             \
    F(DebuggerSectionTarget, "[[Target]]")                               \
    F(DebuggerSectionPromiseState, "[[PromiseState]]")                   \
    F(DebuggerSectionPromiseResult, "[[PromiseResult]]")                 \
    F(DebuggerSectionGeneratorState, "[[GeneratorState]]")               \
    F(DebuggerGeneratorSuspended, "suspended")                           \
    F(DebuggerGeneratorExucuting, "executing")                           \
    F(DebuggerGeneratorCompleted, "completed")                           \
    F(DebuggerIteratorHasMore, "[[IteratorHasMore]]")                    \
    F(DebuggerIteratorIndex, "[[IteratorIndex]]")                        \
    F(DebuggerIteratorKind, "[[IteratorKind]]")                          \
    F(DebuggerSectionFunctions, "[[Functions]]")                         \
    F(DebuggerSectionGlobals, "[[Globals]]")                             \
    F(DebuggerSectionMemories, "[[Memories]]")                           \
    F(DebuggerSectionExports, "[[Exports]]")                             \
    F(DebuggerSectionImports, "[[Imports]]")                             \
    F(DebuggerPending, "pending")                                        \
    F(DebuggerFulfilled, "fulfilled")                                    \
    F(DebuggerRejected, "rejected")                                      \
    F(DebuggerKey, "key")                                                \
    F(DebuggerType, "type")                                              \
    F(DebuggerAnyref, "anyRef")                                          \
    F(DebuggerFuncref, "funcRef")                                        \
    F(DebuggerUnknown, "unknown")                                        \
    F(DebuggerKeyValue, "key-value")
#else
#define FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(F)
#endif

#define ESCARGOT_ASCII_TABLE_MAX 256
#define ESCARGOT_STRINGS_NUMBERS_MAX 128

class StaticStrings {
public:
    StaticStrings(AtomicStringMap* atomicStringMap)
        : m_atomicStringMap(atomicStringMap)
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
    AtomicString stringUsing;
    AtomicString stringVoid;
    AtomicString stringWhile;

    AtomicString NegativeInfinity;
    AtomicString defaultRegExpString;
    AtomicString getBuffer;
    AtomicString getLength;
    AtomicString getSymbolSpecies;
    AtomicString getSymbolToStringTag;
    AtomicString getbyteLength;
    AtomicString getbyteOffset;
    AtomicString getmaxByteLength;
    AtomicString getdisposed;
    AtomicString symbolIterator;

    AtomicString* asciiTable;
    AtomicString* numbers;

#define DECLARE_STATIC_STRING(name) AtomicString name;
    FOR_EACH_STATIC_STRING(DECLARE_STATIC_STRING);
    FOR_EACH_STATIC_THREADING_STRING(DECLARE_STATIC_STRING);
#undef DECLARE_STATIC_STRING

#define DECLARE_LAZY_STATIC_STRING(Name, unused) AtomicString lazy##Name();
    FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_WASM_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING

    void initStaticStrings();
    // Out-of-line helper so the hundreds of static-string registrations in
    // initStaticStrings() collapse to a single call each instead of inlining
    // the ASCIIStringFromExternalMemory construction at every site (code-size).
    NEVER_INLINE void initStaticStringImpl(AtomicString& as, const char* str, size_t len);

protected:
    AtomicStringMap* m_atomicStringMap;

// The intern map owns these strings, so cached lazy slots can use offsets.
#define DECLARE_LAZY_STATIC_STRING(Name, unused) CompressibleAtomicString m_lazy##Name;
    FOR_EACH_LAZY_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_WASM_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_DEBUGGER_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_INTL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_TEMPORAL_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
    FOR_EACH_LAZY_THREADING_STATIC_STRING(DECLARE_LAZY_STATIC_STRING);
#undef DECLARE_LAZY_STATIC_STRING
};
} // namespace Escargot

#endif
