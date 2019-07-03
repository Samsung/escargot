/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotStaticStrings__
#define __EscargotStaticStrings__

#include "runtime/AtomicString.h"

namespace Escargot {

#define FOR_EACH_STATIC_STRING(F) \
    F(null)                       \
    F(undefined)                  \
    F(prototype)                  \
    F(construct)                  \
    F(constructor)                \
    F(name)                       \
    F(arguments)                  \
    F(caller)                     \
    F(callee)                     \
    F(length)                     \
    F(__proto__)                  \
    F(__defineGetter__)           \
    F(__defineSetter__)           \
    F(__lookupGetter__)           \
    F(__lookupSetter__)           \
    F(value)                      \
    F(writable)                   \
    F(enumerable)                 \
    F(configurable)               \
    F(get)                        \
    F(set)                        \
    F(String)                     \
    F(Number)                     \
    F(NaN)                        \
    F(Infinity)                   \
    F(EPSILON)                    \
    F(NEGATIVE_INFINITY)          \
    F(POSITIVE_INFINITY)          \
    F(MAX_VALUE)                  \
    F(MIN_VALUE)                  \
    F(eval)                       \
    F(Object)                     \
    F(GlobalObject)               \
    F(Boolean)                    \
    F(Error)                      \
    F(ReferenceError)             \
    F(TypeError)                  \
    F(RangeError)                 \
    F(SyntaxError)                \
    F(URIError)                   \
    F(EvalError)                  \
    F(message)                    \
    F(stack)                      \
    F(valueOf)                    \
    F(Array)                      \
    F(isArray)                    \
    F(of)                         \
    F(from)                       \
    F(concat)                     \
    F(forEach)                    \
    F(indexOf)                    \
    F(lastIndexOf)                \
    F(join)                       \
    F(push)                       \
    F(pop)                        \
    F(slice)                      \
    F(splice)                     \
    F(shift)                      \
    F(sort)                       \
    F(Function)                   \
    F(Empty)                      \
    F(Date)                       \
    F(getDate)                    \
    F(getDay)                     \
    F(getFullYear)                \
    F(getHours)                   \
    F(getMilliseconds)            \
    F(getMinutes)                 \
    F(getMonth)                   \
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
    F(setDate)                    \
    F(setFullYear)                \
    F(setHours)                   \
    F(setMilliseconds)            \
    F(setMinutes)                 \
    F(setMonth)                   \
    F(setSeconds)                 \
    F(setTime)                    \
    F(setUTCDate)                 \
    F(setUTCFullYear)             \
    F(setUTCHours)                \
    F(setUTCMilliseconds)         \
    F(setUTCMinutes)              \
    F(setUTCMonth)                \
    F(setUTCSeconds)              \
    F(toDateString)               \
    F(toISOString)                \
    F(toLocaleDateString)         \
    F(toLocaleTimeString)         \
    F(toTimeString)               \
    F(toUTCString)                \
    F(getYear)                    \
    F(setYear)                    \
    F(toGMTString)                \
    F(Math)                       \
    F(PI)                         \
    F(E)                          \
    F(abs)                        \
    F(cos)                        \
    F(ceil)                       \
    F(max)                        \
    F(min)                        \
    F(floor)                      \
    F(fround)                     \
    F(hypot)                      \
    F(pow)                        \
    F(random)                     \
    F(round)                      \
    F(sign)                       \
    F(sin)                        \
    F(sinh)                       \
    F(sqrt)                       \
    F(tan)                        \
    F(tanh)                       \
    F(trunc)                      \
    F(log)                        \
    F(log1p)                      \
    F(log10)                      \
    F(log2)                       \
    F(toString)                   \
    F(toLocaleString)             \
    F(boolean)                    \
    F(number)                     \
    F(toFixed)                    \
    F(toPrecision)                \
    F(string)                     \
    F(object)                     \
    F(function)                   \
    F(flags)                      \
    F(RegExp)                     \
    F(source)                     \
    F(lastIndex)                  \
    F(test)                       \
    F(exec)                       \
    F(input)                      \
    F(index)                      \
    F(compile)                    \
    F(byteLength)                 \
    F(subarray)                   \
    F(buffer)                     \
    F(JSON)                       \
    F(parse)                      \
    F(stringify)                  \
    F(toJSON)                     \
    F(Intl)                       \
    F(Collator)                   \
    F(DateTimeFormat)             \
    F(format)                     \
    F(NumberFormat)               \
    F(supportedLocalesOf)         \
    F(resolvedOptions)            \
    F(getPrototypeOf)             \
    F(setPrototypeOf)             \
    F(isPrototypeOf)              \
    F(propertyIsEnumerable)       \
    F(ignoreCase)                 \
    F(global)                     \
    F(multiline)                  \
    F(sticky)                     \
    F(unicode)                    \
    F(implements)                 \
    F(interface)                  \
    F(package)                    \
    F(yield)                      \
    F(let)                        \
    F(LN10)                       \
    F(LN2)                        \
    F(LOG10E)                     \
    F(LOG2E)                      \
    F(MAX_SAFE_INTEGER)           \
    F(MIN_SAFE_INTEGER)           \
    F(SQRT1_2)                    \
    F(SQRT2)                      \
    F(UTC)                        \
    F(acos)                       \
    F(acosh)                      \
    F(anonymous)                  \
    F(apply)                      \
    F(asin)                       \
    F(asinh)                      \
    F(atan)                       \
    F(atan2)                      \
    F(atanh)                      \
    F(bind)                       \
    F(call)                       \
    F(cbrt)                       \
    F(charAt)                     \
    F(charCodeAt)                 \
    F(clz32)                      \
    F(cosh)                       \
    F(create)                     \
    F(dbgBreak)                   \
    F(decodeURI)                  \
    F(decodeURIComponent)         \
    F(defineProperties)           \
    F(defineProperty)             \
    F(encodeURI)                  \
    F(encodeURIComponent)         \
    F(escape)                     \
    F(every)                      \
    F(exp)                        \
    F(expm1)                      \
    F(fill)                       \
    F(filter)                     \
    F(find)                       \
    F(findIndex)                  \
    F(freeze)                     \
    F(fromCharCode)               \
    F(getOwnPropertyDescriptor)   \
    F(getOwnPropertyNames)        \
    F(getOwnPropertySymbols)      \
    F(hasOwnProperty)             \
    F(deleteProperty)             \
    F(imul)                       \
    F(isExtensible)               \
    F(isFinite)                   \
    F(isFrozen)                   \
    F(isNaN)                      \
    F(isInteger)                  \
    F(isSafeInteger)              \
    F(isSealed)                   \
    F(keys)                       \
    F(localeCompare)              \
    F(compare)                    \
    F(map)                        \
    F(match)                      \
    F(now)                        \
    F(normalize)                  \
    F(parseFloat)                 \
    F(parseInt)                   \
    F(preventExtensions)          \
    F(print)                      \
    F(read)                       \
    F(append)                     \
    F(reduce)                     \
    F(reduceRight)                \
    F(repeat)                     \
    F(replace)                    \
    F(reverse)                    \
    F(revocable)                  \
    F(revoke)                     \
    F(run)                        \
    F(seal)                       \
    F(search)                     \
    F(some)                       \
    F(split)                      \
    F(startsWith)                 \
    F(endsWith)                   \
    F(includes)                   \
    F(substr)                     \
    F(substring)                  \
    F(toExponential)              \
    F(toLocaleLowerCase)          \
    F(toLocaleUpperCase)          \
    F(toLowerCase)                \
    F(toUpperCase)                \
    F(trim)                       \
    F(unescape)                   \
    F(unshift)                    \
    F(ThrowTypeError)             \
    F(TypedArray)                 \
    F(Int8Array)                  \
    F(Int16Array)                 \
    F(Int32Array)                 \
    F(Uint8Array)                 \
    F(Uint16Array)                \
    F(Uint32Array)                \
    F(Uint8ClampedArray)          \
    F(Float32Array)               \
    F(Float64Array)               \
    F(ArrayBuffer)                \
    F(isView)                     \
    F(DataView)                   \
    F(BYTES_PER_ELEMENT)          \
    F(copyWithin)                 \
    F(byteOffset)                 \
    F(getFloat32)                 \
    F(getFloat64)                 \
    F(getInt8)                    \
    F(getInt16)                   \
    F(getInt32)                   \
    F(getUint8)                   \
    F(getUint16)                  \
    F(getUint32)                  \
    F(setFloat32)                 \
    F(setFloat64)                 \
    F(setInt8)                    \
    F(setInt16)                   \
    F(setInt32)                   \
    F(setUint8)                   \
    F(setUint16)                  \
    F(setUint32)                  \
    F(Promise)                    \
    F(Proxy)                      \
    F(proxy)                      \
    F(alreadyResolved)            \
    F(alreadyCalled)              \
    F(values)                     \
    F(Reflect)                    \
    F(remainingElements)          \
    F(all)                        \
    F(race)                       \
    F(reject)                     \
    F(resolve)                    \
    F(then)                       \
    F(gc)                         \
    F($_)                         \
    F(lastMatch)                  \
    F(lastParen)                  \
    F(leftContext)                \
    F(rightContext)               \
    F($1)                         \
    F($2)                         \
    F($3)                         \
    F($4)                         \
    F($5)                         \
    F($6)                         \
    F($7)                         \
    F($8)                         \
    F($9)                         \
    F(assign)                     \
    F(Map)                        \
    F(clear)                      \
    F(has)                        \
    F(done)                       \
    F(next)                       \
    F(MapIterator)                \
    F(ArrayIterator)              \
    F(StringIterator)             \
    F(SetIterator)                \
    F(entries)                    \
    F(size)                       \
    F(add)                        \
    F(Set)                        \
    F(WeakMap)                    \
    F(WeakSet)                    \
    F(Generator)                  \
    F(Symbol)                     \
    F(symbol)                     \
    F(hasInstance)                \
    F(isConcatSpreadable)         \
    F(iterator)                   \
    F(species)                    \
    F(toPrimitive)                \
    F(toStringTag)                \
    F(unscopables)                \
    F(keyFor)                     \
    F(raw)                        \
    F(load)                       \
    F(getCanonicalLocales)


class StaticStrings {
public:
    StaticStrings()
        : dtoaCacheSize(5)
    {
    }
    AtomicString NegativeInfinity;
    AtomicString stringTrue;
    AtomicString stringFalse;
    AtomicString stringPublic;
    AtomicString stringProtected;
    AtomicString stringPrivate;
    AtomicString stringStatic;
    AtomicString stringCatch;
    AtomicString stringDelete;
    AtomicString stringFor;
    AtomicString stringDefault;
    AtomicString stringThis;

    // this js keywords are used in esprima
    // after removing esprima::keywordToString
    // these strings are can be removed
    AtomicString stringIf;
    AtomicString stringIn;
    AtomicString stringDo;
    AtomicString stringVar;
    AtomicString stringNew;
    AtomicString stringTry;
    AtomicString stringElse;
    AtomicString stringCase;
    AtomicString stringVoid;
    AtomicString stringWith;
    AtomicString stringEnum;
    AtomicString stringAwait;
    AtomicString stringWhile;
    AtomicString stringBreak;
    AtomicString stringThrow;
    AtomicString stringConst;
    AtomicString stringClass;
    AtomicString stringSuper;
    AtomicString stringReturn;
    AtomicString stringTypeof;
    AtomicString stringSwitch;
    AtomicString stringExport;
    AtomicString stringImport;
    AtomicString stringFinally;
    AtomicString stringExtends;
    AtomicString stringFunction;
    AtomicString stringContinue;
    AtomicString stringDebugger;
    AtomicString stringInstanceof;
    AtomicString stringImplements;
    AtomicString stringInterface;
    AtomicString stringPackage;
    AtomicString stringYield;
    AtomicString stringLet;
    AtomicString stringError;

    AtomicString defaultRegExpString;
    AtomicString get__proto__;
    AtomicString set__proto__;
    AtomicString getbyteLength;
    AtomicString getbyteOffset;
    AtomicString getLength;
    AtomicString getBuffer;
    AtomicString getFlags;
    AtomicString getGlobal;
    AtomicString getIgnoreCase;
    AtomicString getMultiline;
    AtomicString getSource;
    AtomicString getSticky;
    AtomicString getUnicode;
    AtomicString getSize;
    AtomicString getSymbolSpecies;
    AtomicString getSymbolSearch;
    AtomicString getSymbolToStringTag;
    AtomicString symbolSplit;
    AtomicString $Ampersand;
    AtomicString $PlusSign;
    AtomicString $GraveAccent;
    AtomicString $Apostrophe;

#define ESCARGOT_ASCII_TABLE_MAX 256
    AtomicString asciiTable[ESCARGOT_ASCII_TABLE_MAX];

#define ESCARGOT_STRINGS_NUMBERS_MAX 128
    AtomicString numbers[ESCARGOT_STRINGS_NUMBERS_MAX];

#define DECLARE_STATIC_STRING(name) AtomicString name;
    FOR_EACH_STATIC_STRING(DECLARE_STATIC_STRING);
#undef DECLARE_STATIC_STRING

    void initStaticStrings(AtomicStringMap* map);

    const size_t dtoaCacheSize; // 5;
    mutable Vector<std::pair<double, ::Escargot::String*>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<double, ::Escargot::String*>>> dtoaCache;
    //mutable std::list<std::pair<double, ::Escargot::String*>, GCUtil::gc_malloc_ignore_off_page_allocator<std::pair<double, ::Escargot::String*>>> dtoaCache;

    ::Escargot::String* dtoa(double d) const;
};
}

#endif
