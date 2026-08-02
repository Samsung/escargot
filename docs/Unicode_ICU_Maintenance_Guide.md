# Unicode / ICU Maintenance Guide

Escargot used to carry generated Unicode tables in its own binary. Those tables are gone:
identifier classification, regular expression `\p{...}` classes and case insensitive
matching are all answered by the ICU that is linked (or `dlopen`ed) at runtime.

The practical consequence is that **a Unicode upgrade is mostly an ICU upgrade**. This
document lists what follows automatically, what does not, and what to do about the parts
that do not.

## 1. What the runtime ICU decides

| Area | Code | ICU API |
| --- | --- | --- |
| `IdentifierStart` / `IdentifierPart` | `src/parser/Lexer.cpp` | `u_hasBinaryProperty(ch, UCHAR_ID_START / UCHAR_ID_CONTINUE)` |
| `\p{...}` code point data | `third_party/yarr/YarrUnicodeProperties.cpp` | `uset_openPattern()` |
| Property / property value name resolution and aliases | `third_party/yarr/YarrUnicodeProperties.cpp` | `u_getPropertyEnum()`, `u_getPropertyValueEnum()`, `u_getPropertyName()`, `u_getPropertyValueName()` |
| Case insensitive equivalence classes | `third_party/yarr/YarrCanonicalize.h` | `uset_closeOver(USET_CASE_INSENSITIVE)` |
| `Canonicalize()` | `third_party/yarr/YarrCanonicalize.h` | `u_foldCase()` (unicode mode), `u_toupper()` (legacy mode) |

Upgrading ICU is enough for all of the above. There is no table to regenerate and no
`DerivedCoreProperties.txt` to refresh for them.

The only remaining build time Unicode inputs are the emoji sequence *name* list (section 2.2)
and the simple case folding exception pairs (section 2.4). Everything else under
`tools/unicode_data/` is unused by the build and kept only as reference material.

## 2. What ICU cannot tell us

ICU answers "what does Unicode say". It does not answer "what does ECMAScript allow".
Those two differ, so a small amount of spec-derived data lives in the source. Each item
below is a name list only — never code point data.

### 2.1 Binary property names — `YarrUnicodeProperties.cpp`, `binaryPropertyNames`

ICU knows more binary properties than ECMAScript accepts (`Case_Sensitive`, `NFC_Inert`,
`ID_Compat_Math_Start`, `Segment_Starter`, POSIX aliases such as `alnum`/`blank`/`xdigit`,
...) and gains more with every Unicode release. `\p{Case_Sensitive}` must be a
`SyntaxError`, so the accepted set has to come from the spec.

*To update*: when TC39 adds a property to the table
*Binary Unicode property aliases and their canonical property names*
(`sec-runtime-semantics-unicodematchproperty-p`), add its **canonical long name** to
`binaryPropertyNames`. Aliases (`ExtPict`, `WSpace`, ...) need no entry — ICU resolves
them and the long name is what gets validated.

*Do not* reintroduce a bound like `UCHAR_BINARY_LIMIT`. That constant is baked in at build
time from `third_party/runtime_icu_binder/ICUTypes.h`, which is a frozen copy of an old
ICU header; the loaded ICU numbers its properties differently and anything above the
stale limit silently disappears (this is how `\p{Extended_Pictographic}` broke once).
The code uses `prop < UCHAR_INT_START` instead, which is a range boundary rather than a
count.

### 2.2 Sequence property names — generated from the Unicode data files

`Basic_Emoji`, `RGI_Emoji`, `Emoji_Keycap_Sequence`, ... are *properties of strings*: valid
only under the `v` flag, and they set `mayContainStrings`. ICU 74+ reports them as ordinary
binary properties, so ICU alone cannot distinguish them.

`tools/code_generators/generateYarrUnicodePropertyTables.py` runs at CMake configure time
(see `build/escargot.cmake`), reads `tools/unicode_data/emoji-sequences.txt` and
`emoji-zwj-sequences.txt`, and emits `isSequencePropertyName()` into the generated
`UnicodePatternTables.h`. It emits **names only** — the code points still come from ICU at
runtime.

*To update*: refresh the two `emoji-*.txt` files from
`https://www.unicode.org/Public/emoji/<version>/` (record the source in
`tools/unicode_data/UCDFILESCOMESFROM`) and rebuild. Nothing else is needed. A sequence
property must not also appear in `binaryPropertyNames`.

### 2.3 Special property names — `specialPropertyNames`

`Any`, `ASCII` and `Assigned` are not Unicode properties at all; they only exist in the
ECMAScript grammar (and, coincidentally, in ICU's set pattern syntax). They stay hardcoded.

### 2.4 Simple case folding exceptions — generated from `CaseFolding.txt`

A handful of code points fold to a *string* under full case folding but still have a
single code point simple folding: `1FD3; S; 0390`, `1FE3; S; 03B0`, `FB05; S; FB06`,
`1E9E; S; 00DF`, the Greek `1F88..1FFC` iota-subscript letters, ... 31 mappings in total.
ECMAScript canonicalizes with the *simple* mapping, so `/[ﬅ]/ui` must match `ﬆ`
(test262 `built-ins/RegExp/unicode_full_case_folding`).

icu4c 74 and 78 report these correctly through `uset_closeOver()` / `u_foldCase()`, but the
Windows CI runner fails that test262 case when Yarr derives the link from the runtime ICU
alone, so the behaviour is evidently not portable.

`tools/code_generators/generateSimpleCaseFoldingTable.py` therefore parses the `S` lines of
`tools/unicode_data/CaseFolding.txt` at CMake configure time and emits both directions of
each mapping into the generated `SimpleCaseFoldingTable.h`. `YarrCanonicalize.h` unions that
counterpart into the ICU case closure. The table holds **names of code point pairs only** —
6 bytes per entry, no property data.

*To update*: refresh `tools/unicode_data/CaseFolding.txt` and rebuild. Do not hand-edit the
pairs, and do not delete the table on the grounds that "ICU already knows this" — it is
exactly the case where some ICUs do not.

### 2.5 Grammar deviations from Unicode — `src/parser/Lexer.cpp`

`IdentifierPartChar` is `ID_Continue` **plus** U+200C ZERO WIDTH NON-JOINER and U+200D
ZERO WIDTH JOINER (ECMA-262 12.7). Neither has `ID_Continue`, so the two are special cased
in `isIdentifierPartSlow()`. `IdentifierStartChar` needs no such addition: `Other_ID_Start`
(U+2118 and friends) is already part of the derived `ID_Start` property.

### 2.6 Constants missing from the runtime binder — `YarrCanonicalize.h`

`ENABLE_ICU` builds that bind ICU at runtime never include `unicode/uset.h`; the binder
only forward declares `USet`. `USET_CASE_INSENSITIVE` is therefore spelled out as
`usetCaseInsensitive = 2`. If more `uset_*` options are needed, follow the same pattern
rather than including the header — and remember to register the new function in
`FOR_EACH_UC_OP` / `FOR_EACH_UC_VOID_OP` (`RuntimeICUBinder.h`) **and** in `ICUPolyfill.h`.
A symbol that is used but not registered fails at link time in the static build and
silently yields a null function pointer in the `dlopen` build.

## 3. Behaviour follows the host ICU, and so does CI

Because the data is no longer in the binary, the same Escargot build gives different
answers on different hosts. Two test262 groups are pinned to a specific Unicode version
and therefore track the host ICU:

* `built-ins/RegExp/property-escapes/generated/*`, `property-of-strings`, `rgi-emoji`
* `language/identifiers/{part,start}-unicode-<version>`

CI handles this in two different ways, and it is important to keep the distinction:

| Job | ICU | Handling |
| --- | --- | --- |
| `build-test-on-self-hosted-linux` | pinned `/usr/icu78-{32,64}` | runs everything, no skip |
| `build-test-on-self-hosted-arm64-linux` | pinned `/usr/icu78-64` | runs everything, no skip |
| `build-test-on-self-hosted-arm-linux` (arm32) | container's system ICU | skips both groups |
| `test-on-windows-x86-x64` | Windows' bundled `icu.dll` | skips both groups |

**Pin a matching ICU wherever possible; only skip where pinning is not.** When test262 is
bumped to a newer Unicode version, update the pinned ICU on the self-hosted runners first —
otherwise those jobs start failing, and the temptation is to add skips there too, which
would leave nobody actually testing this code.

The version *independent* identifier tests (`part-zwj-zwnj`, `other_id_start`,
`other_id_continue`, `start-dollar-sign`, ...) are deliberately left running everywhere:
they are the ones that catch real bugs, and they pass on any ICU.

## 4. Checklist for an ICU upgrade

1. Bump the pinned ICU on the self-hosted runners and in `.github/workflows/es-actions.yml`.
2. Run test262 with no skips against the new ICU. Expect churn only in the two groups above.
3. If a new `\p{...}` name is now accepted that ECMAScript does not define, it means the
   allow lists in section 2 need tightening, not that the test is wrong.
4. If ICU introduced a binary property that ECMAScript has since adopted, add its canonical
   long name to `binaryPropertyNames`.
5. `third_party/runtime_icu_binder/ICUTypes.h` only needs touching when new *enum values*
   are referenced by name in Escargot code. It is intentionally not kept in sync with the
   runtime ICU, so never derive a limit or a count from it.

## 5. Checklist for a test262 upgrade

1. Diff `test/test262/test/built-ins/RegExp/property-escapes/generated/` for added property
   names; anything new is either a spec addition (section 2.1 / 2.2) or a Unicode data
   change that the runtime ICU already covers.
2. Diff `test/test262/test/language/identifiers/` for a new `*-unicode-<version>` pair; the
   pinned CI ICU must be new enough for it.
3. If `property-of-strings` gained a sequence property, refresh the emoji data files
   (section 2.2).

## 6. Quick triage

| Symptom | Likely cause |
| --- | --- |
| `\p{Foo}` raises `SyntaxError` although the spec allows it | missing from `binaryPropertyNames` (2.1), or a stale property limit (2.1) |
| `\p{Foo}` is accepted although the spec forbids it | ICU loose matching slipped through — the name must match a canonical ICU name exactly |
| `\p{Foo}` matches nothing but does not throw | the loaded ICU does not know the property; graceful degradation is intentional |
| `\p{Basic_Emoji}` rejected under `v` | emoji sequence name list is out of date (2.2) |
| Case insensitive match misses a code point | the pair is newer than the loaded ICU, or `canonicalize()` filtered it out of the closure (if its full folding is a string, see 2.4) |
| An identifier is rejected only on one platform | that platform's ICU has an older Unicode version (section 3) |
