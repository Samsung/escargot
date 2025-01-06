#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
/*
 * Copyright (C) 2015 Andy VanWagoner (thetalecrafter@gmail.com)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/ExecutionState.h"
#include "runtime/Value.h"
#include "runtime/Object.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "Intl.h"
#include "IntlLocale.h"

namespace Escargot {

// Invalid tags starting with: https://github.com/tc39/ecma402/pull/289
static bool isValidTagInBCP47ButInvalidOnUTS35(const std::string& s)
{
    if (s.rfind("no-nyn", 0) == 0) {
        return true;
    } else if (s.rfind("i-klingon", 0) == 0) {
        return true;
    } else if (s.rfind("zh-hak-CN", 0) == 0) {
        return true;
    } else if (s.rfind("sgn-ils", 0) == 0) {
        return true;
    }

    return false;
}

static std::string grandfatheredLangTag(const std::string& locale)
{
    // grandfathered = irregular / regular
    HashMap<std::string, std::string> tagMap;
    // Irregular.
    tagMap["en-gb-oed"] = "en-GB-oed";
    tagMap["i-ami"] = "ami";
    tagMap["i-bnn"] = "bnn";
    tagMap["i-default"] = "i-default";
    tagMap["i-enochian"] = "i-enochian";
    tagMap["i-hak"] = "hak";
    tagMap["i-klingon"] = "tlh";
    tagMap["i-lux"] = "lb";
    tagMap["i-mingo"] = "i-mingo";
    tagMap["i-navajo"] = "nv";
    tagMap["i-pwn"] = "pwn";
    tagMap["i-tao"] = "tao";
    tagMap["i-tay"] = "tay";
    tagMap["i-tsu"] = "tsu";
    tagMap["sgn-be-fr"] = "sfb";
    tagMap["sgn-be-nl"] = "vgt";
    tagMap["sgn-ch-de"] = "sgg";
    // Regular.
    tagMap["art-lojban"] = "jbo";
    tagMap["cel-gaulish"] = "xtg-x-cel-gaulish";
    tagMap["no-bok"] = "nb";
    tagMap["no-nyn"] = "nn";
    tagMap["zh-guoyu"] = "zh";
    tagMap["zh-hakka"] = "hak";
    tagMap["zh-min"] = "zh-min";
    tagMap["zh-min-nan"] = "nan";
    tagMap["zh-xiang"] = "hsn";

    auto tempLocale = locale;
    std::transform(tempLocale.begin(), tempLocale.end(), tempLocale.begin(), tolower);
    auto iter = tagMap.find(tempLocale);

    if (iter != tagMap.end()) {
        return iter->second;
    }
    return std::string();
}

static std::string intlPreferredLanguageTagStartWithA(const std::string& tag)
{
    if (tag == "aam")
        return "aas";
    else if (tag == "adp")
        return "dz";
    else if (tag == "asd")
        return "snz";
    else if (tag == "aue")
        return "ktz";
    else if (tag == "ayx")
        return "nun";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithB(const std::string& tag)
{
    if (tag == "bgm")
        return "bcg";
    else if (tag == "bic")
        return "bir";
    else if (tag == "bjd")
        return "drl";
    else if (tag == "blg")
        return "iba";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithC(const std::string& tag)
{
    if (tag == "ccq")
        return "rki";
    else if (tag == "cjr")
        return "mom";
    else if (tag == "cka")
        return "cmr";
    else if (tag == "cmk")
        return "xch";
    else if (tag == "coy")
        return "pij";
    else if (tag == "cqu")
        return "quh";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithD(const std::string& tag)
{
    if (tag == "dit")
        return "dif";
    else if (tag == "drh")
        return "khk";
    else if (tag == "drr")
        return "kzk";
    else if (tag == "drw")
        return "prs";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithG(const std::string& tag)
{
    if (tag == "gav")
        return "dev";
    else if (tag == "gfx")
        return "vaj";
    else if (tag == "ggn")
        return "gvr";
    else if (tag == "gli")
        return "kzk";
    else if (tag == "gti")
        return "nyc";
    else if (tag == "guv")
        return "duz";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithI(const std::string& tag)
{
    if (tag == "ibi")
        return "opa";
    else if (tag == "ilw")
        return "gal";
    else if (tag == "in")
        return "id";
    else if (tag == "iw")
        return "he";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithJ(const std::string& tag)
{
    if (tag == "jeg")
        return "oyb";
    else if (tag == "ji")
        return "yi";
    else if (tag == "jw")
        return "jv";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithK(const std::string& tag)
{
    if (tag == "kgc")
        return "tdf";
    else if (tag == "kgh")
        return "kml";
    else if (tag == "koj")
        return "kwv";
    else if (tag == "krm")
        return "bmf";
    else if (tag == "ktr")
        return "dtp";
    else if (tag == "kvs")
        return "gdj";
    else if (tag == "kwq")
        return "yam";
    else if (tag == "kxe")
        return "tvd";
    else if (tag == "kxl")
        return "kru";
    else if (tag == "kzj")
        return "dtp";
    else if (tag == "kzt")
        return "dtp";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithL(const std::string& tag)
{
    if (tag == "lii")
        return "raq";
    else if (tag == "llo")
        return "ngt";
    else if (tag == "lmm")
        return "rmx";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithM(const std::string& tag)
{
    if (tag == "meg")
        return "cir";
    else if (tag == "mo")
        return "ro";
    else if (tag == "mst")
        return "mry";
    else if (tag == "mwj")
        return "vaj";
    else if (tag == "myd")
        return "aog";
    else if (tag == "myt")
        return "mry";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithN(const std::string& tag)
{
    if (tag == "nad")
        return "xny";
    else if (tag == "ncp")
        return "kdz";
    else if (tag == "nns")
        return "nbr";
    else if (tag == "nnx")
        return "ngv";
    else if (tag == "nts")
        return "pij";
    else if (tag == "nxu")
        return "bpp";

    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithP(const std::string& tag)
{
    if (tag == "pat")
        return "kxr";
    else if (tag == "pcr")
        return "adx";
    else if (tag == "pmc")
        return "huw";
    else if (tag == "pmu")
        return "phr";
    else if (tag == "ppa")
        return "bfy";
    else if (tag == "ppr")
        return "lcq";
    else if (tag == "pry")
        return "prt";
    else if (tag == "puz")
        return "pub";

    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithT(const std::string& tag)
{
    if (tag == "tdu")
        return "dtp";
    else if (tag == "thc")
        return "tpo";
    else if (tag == "thw")
        return "ola";
    else if (tag == "thx")
        return "oyb";
    else if (tag == "tie")
        return "ras";
    else if (tag == "tkk")
        return "twm";
    else if (tag == "tlw")
        return "weo";
    else if (tag == "tmp")
        return "tyj";
    else if (tag == "tne")
        return "kak";
    else if (tag == "tnf")
        return "prs";
    else if (tag == "tsf")
        return "taj";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithX(const std::string& tag)
{
    if (tag == "xba")
        return "cax";
    else if (tag == "xia")
        return "acn";
    else if (tag == "xkh")
        return "waw";
    else if (tag == "xrq")
        return "dmw";
    else
        return "";
}

static std::string intlPreferredLanguageTagStartWithY(const std::string& tag)
{
    if (tag == "ybd")
        return "rki";
    else if (tag == "yma")
        return "lrr";
    else if (tag == "ymt")
        return "mtm";
    else if (tag == "yos")
        return "zom";
    else if (tag == "yuu")
        return "yug";
    else
        return "";
}

static std::string intlPreferredLanguageTag(const std::string& tag)
{
    // languageAlias element in
    // https://www.unicode.org/repos/cldr/trunk/common/supplemental/supplementalMetadata.xml
    if (tag == "cmn")
        return "zh";
    else if (tag == "ces")
        return "cs";
    else if (tag == "heb")
        return "he";
    else if (tag == "xsj")
        return "suj";
    else if (tag == "aar")
        return "aa";
    // legacy
    else if (tag == "tl")
        return "fil";

    switch (tag[0]) {
        // 92 possible replacements
    case 'a':
        return intlPreferredLanguageTagStartWithA(tag);
    case 'b':
        return intlPreferredLanguageTagStartWithB(tag);
    case 'c':
        return intlPreferredLanguageTagStartWithC(tag);
    case 'd':
        return intlPreferredLanguageTagStartWithD(tag);
    case 'g':
        return intlPreferredLanguageTagStartWithG(tag);
    case 'h': {
        if (tag == "hrr")
            return "jal";
        return "";
    }
    case 'i':
        return intlPreferredLanguageTagStartWithI(tag);
    case 'j':
        return intlPreferredLanguageTagStartWithJ(tag);
    case 'k':
        return intlPreferredLanguageTagStartWithK(tag);
    case 'l':
        return intlPreferredLanguageTagStartWithL(tag);
    case 'm':
        return intlPreferredLanguageTagStartWithM(tag);
    case 'n':
        return intlPreferredLanguageTagStartWithN(tag);
    case 'o': {
        if (tag == "oun")
            return "vaj";
        return "";
    }
    case 'p':
        return intlPreferredLanguageTagStartWithP(tag);
    case 's': {
        if (tag == "sca")
            return "hle";
        else if (tag == "skk")
            return "oyb";
        else
            return "";
    }
    case 't':
        return intlPreferredLanguageTagStartWithT(tag);
    case 'u': {
        if (tag == "uok")
            return "ema";
        else
            return "";
    }
    case 'x':
        return intlPreferredLanguageTagStartWithX(tag);
    case 'y':
        return intlPreferredLanguageTagStartWithY(tag);
    case 'z': {
        if (tag == "zir")
            return "scv";
        return "";
    }
    default:
        return "";
    }
}

static std::string intlRedundantLanguageTag(const std::string& tag)
{
    // non-iana
    if (tag == "hy-arevela")
        return "hy";
    if (tag == "hy-arevmda")
        return "hyw";

    // 24 possible replacements
    if (tag == "ja-Latn-hepburn-heploc")
        return "ja-Latn-alalc97";
    if (tag == "sgn-BR")
        return "bzs";
    if (tag == "sgn-CO")
        return "csn";
    if (tag == "sgn-DE")
        return "gsg";
    if (tag == "sgn-DK")
        return "dsl";
    if (tag == "sgn-ES")
        return "ssp";
    if (tag == "sgn-FR")
        return "fsl";
    if (tag == "sgn-GB")
        return "bfi";
    if (tag == "sgn-GR")
        return "gss";
    if (tag == "sgn-IE")
        return "isg";
    if (tag == "sgn-IT")
        return "ise";
    if (tag == "sgn-JP")
        return "jsl";
    if (tag == "sgn-MX")
        return "mfs";
    if (tag == "sgn-NI")
        return "ncs";
    if (tag == "sgn-NL")
        return "dse";
    if (tag == "sgn-NO")
        return "nsl";
    if (tag == "sgn-PT")
        return "psr";
    if (tag == "sgn-SE")
        return "swl";
    if (tag == "sgn-US")
        return "ase";
    if (tag == "sgn-ZA")
        return "sfs";
    if (tag == "zh-cmn-Hans")
        return "cmn-Hans";
    if (tag == "zh-cmn-Hant")
        return "cmn-Hant";
    return "";
}

std::string Intl::preferredLanguage(const std::string& language)
{
    auto preferred = intlPreferredLanguageTag(language);
    if (preferred != "") {
        return preferred;
    }
    return language;
}

static std::string intlPreferredExtlangTagStartWithA(const std::string& tag)
{
    if (tag == "aao")
        return "ar";
    else if (tag == "abh")
        return "ar";
    else if (tag == "abv")
        return "ar";
    else if (tag == "acm")
        return "ar";
    else if (tag == "acq")
        return "ar";
    else if (tag == "acw")
        return "ar";
    else if (tag == "acx")
        return "ar";
    else if (tag == "acy")
        return "ar";
    else if (tag == "adf")
        return "ar";
    else if (tag == "ads")
        return "sgn";
    else if (tag == "aeb")
        return "ar";
    else if (tag == "aec")
        return "ar";
    else if (tag == "aed")
        return "sgn";
    else if (tag == "aen")
        return "sgn";
    else if (tag == "afb")
        return "ar";
    else if (tag == "afg")
        return "sgn";
    else if (tag == "ajp")
        return "ar";
    else if (tag == "apc")
        return "ar";
    else if (tag == "apd")
        return "ar";
    else if (tag == "arb")
        return "ar";
    else if (tag == "arq")
        return "ar";
    else if (tag == "ars")
        return "ar";
    else if (tag == "ary")
        return "ar";
    else if (tag == "arz")
        return "ar";
    else if (tag == "ase")
        return "sgn";
    else if (tag == "asf")
        return "sgn";
    else if (tag == "asp")
        return "sgn";
    else if (tag == "asq")
        return "sgn";
    else if (tag == "asw")
        return "sgn";
    else if (tag == "auz")
        return "ar";
    else if (tag == "avl")
        return "ar";
    else if (tag == "ayh")
        return "ar";
    else if (tag == "ayl")
        return "ar";
    else if (tag == "ayn")
        return "ar";
    else if (tag == "ayp")
        return "ar";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithB(const std::string& tag)
{
    if (tag == "bbz")
        return "ar";
    else if (tag == "bfi")
        return "sgn";
    else if (tag == "bfk")
        return "sgn";
    else if (tag == "bjn")
        return "ms";
    else if (tag == "bog")
        return "sgn";
    else if (tag == "bqn")
        return "sgn";
    else if (tag == "bqy")
        return "sgn";
    else if (tag == "btj")
        return "ms";
    else if (tag == "bve")
        return "ms";
    else if (tag == "bvl")
        return "sgn";
    else if (tag == "bvu")
        return "ms";
    else if (tag == "bzs")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithC(const std::string& tag)
{
    if (tag == "cdo")
        return "zh";
    else if (tag == "cds")
        return "sgn";
    else if (tag == "cjy")
        return "zh";
    else if (tag == "cmn")
        return "zh";
    else if (tag == "cnp")
        return "zh";
    else if (tag == "coa")
        return "ms";
    else if (tag == "cpx")
        return "zh";
    else if (tag == "csc")
        return "sgn";
    else if (tag == "csd")
        return "sgn";
    else if (tag == "cse")
        return "sgn";
    else if (tag == "csf")
        return "sgn";
    else if (tag == "csg")
        return "sgn";
    else if (tag == "csl")
        return "sgn";
    else if (tag == "csn")
        return "sgn";
    else if (tag == "csp")
        return "zh";
    else if (tag == "csq")
        return "sgn";
    else if (tag == "csr")
        return "sgn";
    else if (tag == "csx")
        return "sgn";
    else if (tag == "czh")
        return "zh";
    else if (tag == "czo")
        return "zh";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithD(const std::string& tag)
{
    if (tag == "doq")
        return "sgn";
    else if (tag == "dse")
        return "sgn";
    else if (tag == "dsl")
        return "sgn";
    else if (tag == "dup")
        return "ms";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithE(const std::string& tag)
{
    if (tag == "ecs")
        return "sgn";
    else if (tag == "ehs")
        return "sgn";
    else if (tag == "esl")
        return "sgn";
    else if (tag == "esn")
        return "sgn";
    else if (tag == "eso")
        return "sgn";
    else if (tag == "eth")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithF(const std::string& tag)
{
    if (tag == "fcs")
        return "sgn";
    else if (tag == "fse")
        return "sgn";
    else if (tag == "fsl")
        return "sgn";
    else if (tag == "fss")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithG(const std::string& tag)
{
    if (tag == "gan")
        return "zh";
    else if (tag == "gds")
        return "sgn";
    else if (tag == "gom")
        return "kok";
    else if (tag == "gse")
        return "sgn";
    else if (tag == "gsg")
        return "sgn";
    else if (tag == "gsm")
        return "sgn";
    else if (tag == "gss")
        return "sgn";
    else if (tag == "gus")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithH(const std::string& tag)
{
    if (tag == "hab")
        return "sgn";
    else if (tag == "haf")
        return "sgn";
    else if (tag == "hak")
        return "zh";
    else if (tag == "hds")
        return "sgn";
    else if (tag == "hji")
        return "ms";
    else if (tag == "hks")
        return "sgn";
    else if (tag == "hos")
        return "sgn";
    else if (tag == "hps")
        return "sgn";
    else if (tag == "hsh")
        return "sgn";
    else if (tag == "hsl")
        return "sgn";
    else if (tag == "hsn")
        return "zh";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithI(const std::string& tag)
{
    if (tag == "icl")
        return "sgn";
    else if (tag == "iks")
        return "sgn";
    else if (tag == "ils")
        return "sgn";
    else if (tag == "inl")
        return "sgn";
    else if (tag == "ins")
        return "sgn";
    else if (tag == "ise")
        return "sgn";
    else if (tag == "isg")
        return "sgn";
    else if (tag == "isr")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithJ(const std::string& tag)
{
    if (tag == "jak")
        return "ms";
    else if (tag == "jax")
        return "ms";
    else if (tag == "jcs")
        return "sgn";
    else if (tag == "jhs")
        return "sgn";
    else if (tag == "jks")
        return "sgn";
    else if (tag == "jls")
        return "sgn";
    else if (tag == "jos")
        return "sgn";
    else if (tag == "jsl")
        return "sgn";
    else if (tag == "jus")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithK(const std::string& tag)
{
    if (tag == "kgi")
        return "sgn";
    else if (tag == "knn")
        return "kok";
    else if (tag == "kvb")
        return "ms";
    else if (tag == "kvk")
        return "sgn";
    else if (tag == "kvr")
        return "ms";
    else if (tag == "kxd")
        return "ms";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithL(const std::string& tag)
{
    if (tag == "lbs")
        return "sgn";
    else if (tag == "lce")
        return "ms";
    else if (tag == "lcf")
        return "ms";
    else if (tag == "liw")
        return "ms";
    else if (tag == "lls")
        return "sgn";
    else if (tag == "lsb")
        return "sgn";
    else if (tag == "lsg")
        return "sgn";
    else if (tag == "lsl")
        return "sgn";
    else if (tag == "lsn")
        return "sgn";
    else if (tag == "lso")
        return "sgn";
    else if (tag == "lsp")
        return "sgn";
    else if (tag == "lst")
        return "sgn";
    else if (tag == "lsv")
        return "sgn";
    else if (tag == "lsy")
        return "sgn";
    else if (tag == "ltg")
        return "lv";
    else if (tag == "lvs")
        return "lv";
    else if (tag == "lws")
        return "sgn";
    else if (tag == "lzh")
        return "zh";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithM(const std::string& tag)
{
    if (tag == "max")
        return "ms";
    else if (tag == "mdl")
        return "sgn";
    else if (tag == "meo")
        return "ms";
    else if (tag == "mfa")
        return "ms";
    else if (tag == "mfb")
        return "ms";
    else if (tag == "mfs")
        return "sgn";
    else if (tag == "min")
        return "ms";
    else if (tag == "mnp")
        return "zh";
    else if (tag == "mqg")
        return "ms";
    else if (tag == "mre")
        return "sgn";
    else if (tag == "msd")
        return "sgn";
    else if (tag == "msi")
        return "ms";
    else if (tag == "msr")
        return "sgn";
    else if (tag == "mui")
        return "ms";
    else if (tag == "mzc")
        return "sgn";
    else if (tag == "mzg")
        return "sgn";
    else if (tag == "mzy")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithN(const std::string& tag)
{
    if (tag == "nan")
        return "zh";
    else if (tag == "nbs")
        return "sgn";
    else if (tag == "ncs")
        return "sgn";
    else if (tag == "nsi")
        return "sgn";
    else if (tag == "nsl")
        return "sgn";
    else if (tag == "nsp")
        return "sgn";
    else if (tag == "nsr")
        return "sgn";
    else if (tag == "nzs")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithO(const std::string& tag)
{
    if (tag == "okl")
        return "sgn";
    else if (tag == "orn")
        return "ms";
    else if (tag == "ors")
        return "ms";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithP(const std::string& tag)
{
    if (tag == "pel")
        return "ms";
    else if (tag == "pga")
        return "ar";
    else if (tag == "pgz")
        return "sgn";
    else if (tag == "pks")
        return "sgn";
    else if (tag == "prl")
        return "sgn";
    else if (tag == "prz")
        return "sgn";
    else if (tag == "psc")
        return "sgn";
    else if (tag == "psd")
        return "sgn";
    else if (tag == "pse")
        return "ms";
    else if (tag == "psg")
        return "sgn";
    else if (tag == "psl")
        return "sgn";
    else if (tag == "pso")
        return "sgn";
    else if (tag == "psp")
        return "sgn";
    else if (tag == "psr")
        return "sgn";
    else if (tag == "pys")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithR(const std::string& tag)
{
    if (tag == "rms")
        return "sgn";
    else if (tag == "rsi")
        return "sgn";
    else if (tag == "rsl")
        return "sgn";
    else if (tag == "rsm")
        return "sgn";
    else
        return "";
}


static std::string intlPreferredExtlangTagStartWithS(const std::string& tag)
{
    if (tag == "sdl")
        return "sgn";
    else if (tag == "sfb")
        return "sgn";
    else if (tag == "sfs")
        return "sgn";
    else if (tag == "sgg")
        return "sgn";
    else if (tag == "sgx")
        return "sgn";
    else if (tag == "shu")
        return "ar";
    else if (tag == "slf")
        return "sgn";
    else if (tag == "sls")
        return "sgn";
    else if (tag == "sqk")
        return "sgn";
    else if (tag == "sqs")
        return "sgn";
    else if (tag == "sqx")
        return "sgn";
    else if (tag == "ssh")
        return "ar";
    else if (tag == "ssp")
        return "sgn";
    else if (tag == "ssr")
        return "sgn";
    else if (tag == "svk")
        return "sgn";
    else if (tag == "swc")
        return "sw";
    else if (tag == "swh")
        return "sw";
    else if (tag == "swl")
        return "sgn";
    else if (tag == "syy")
        return "sgn";
    else if (tag == "szs")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithT(const std::string& tag)
{
    if (tag == "tmw")
        return "ms";
    else if (tag == "tse")
        return "sgn";
    else if (tag == "tsm")
        return "sgn";
    else if (tag == "tsq")
        return "sgn";
    else if (tag == "tss")
        return "sgn";
    else if (tag == "tsy")
        return "sgn";
    else if (tag == "tza")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithU(const std::string& tag)
{
    if (tag == "ugn")
        return "sgn";
    else if (tag == "ugy")
        return "sgn";
    else if (tag == "ukl")
        return "sgn";
    else if (tag == "uks")
        return "sgn";
    else if (tag == "urk")
        return "ms";
    else if (tag == "uzn")
        return "uz";
    else if (tag == "uzs")
        return "uz";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithV(const std::string& tag)
{
    if (tag == "vgt")
        return "sgn";
    else if (tag == "vkk")
        return "ms";
    else if (tag == "vkt")
        return "ms";
    else if (tag == "vsi")
        return "sgn";
    else if (tag == "vsl")
        return "sgn";
    else if (tag == "vsv")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithW(const std::string& tag)
{
    if (tag == "wbs")
        return "sgn";
    else if (tag == "wuu")
        return "zh";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithX(const std::string& tag)
{
    if (tag == "xki")
        return "sgn";
    else if (tag == "xml")
        return "sgn";
    else if (tag == "xmm")
        return "ms";
    else if (tag == "xms")
        return "sgn";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithY(const std::string& tag)
{
    if (tag == "yds")
        return "sgn";
    else if (tag == "ygs")
        return "sgn";
    else if (tag == "yhs")
        return "sgn";
    else if (tag == "ysl")
        return "sgn";
    else if (tag == "ysm")
        return "sgn";
    else if (tag == "yue")
        return "zh";
    else
        return "";
}

static std::string intlPreferredExtlangTagStartWithZ(const std::string& tag)
{
    if (tag == "zib")
        return "sgn";
    else if (tag == "zlm")
        return "ms";
    else if (tag == "zmi")
        return "ms";
    else if (tag == "zsl")
        return "sgn";
    else if (tag == "zsm")
        return "ms";
    else
        return "";
}


static std::string intlPreferredExtlangTag(const std::string& tag)
{
    ASSERT(tag.length() == 3);

    switch (tag[0]) {
    case 'a':
        return intlPreferredExtlangTagStartWithA(tag);
    case 'b':
        return intlPreferredExtlangTagStartWithB(tag);
    case 'c':
        return intlPreferredExtlangTagStartWithC(tag);
    case 'd':
        return intlPreferredExtlangTagStartWithD(tag);
    case 'e':
        return intlPreferredExtlangTagStartWithE(tag);
    case 'f':
        return intlPreferredExtlangTagStartWithF(tag);
    case 'g':
        return intlPreferredExtlangTagStartWithG(tag);
    case 'h':
        return intlPreferredExtlangTagStartWithH(tag);
    case 'i':
        return intlPreferredExtlangTagStartWithI(tag);
    case 'j':
        return intlPreferredExtlangTagStartWithJ(tag);
    case 'k':
        return intlPreferredExtlangTagStartWithK(tag);
    case 'l':
        return intlPreferredExtlangTagStartWithL(tag);
    case 'm':
        return intlPreferredExtlangTagStartWithM(tag);
    case 'n':
        return intlPreferredExtlangTagStartWithN(tag);
    case 'o':
        return intlPreferredExtlangTagStartWithO(tag);
    case 'p':
        return intlPreferredExtlangTagStartWithP(tag);
    case 'r':
        return intlPreferredExtlangTagStartWithR(tag);
    case 's':
        return intlPreferredExtlangTagStartWithS(tag);
    case 't':
        return intlPreferredExtlangTagStartWithT(tag);
    case 'u':
        return intlPreferredExtlangTagStartWithU(tag);
    case 'v':
        return intlPreferredExtlangTagStartWithV(tag);
    case 'w':
        return intlPreferredExtlangTagStartWithW(tag);
    case 'x':
        return intlPreferredExtlangTagStartWithX(tag);
    case 'y':
        return intlPreferredExtlangTagStartWithY(tag);
    case 'z':
        return intlPreferredExtlangTagStartWithZ(tag);
    default:
        return "";
    }
}

static std::string intlPreferredRegionTag(const std::string& tag)
{
    // 6 possible replacements
    if (tag == "BU")
        return "MM";
    if (tag == "DD")
        return "DE";
    if (tag == "FX")
        return "FR";
    if (tag == "TP")
        return "TL";
    if (tag == "YD")
        return "YE";
    if (tag == "ZR")
        return "CD";
    return "";
}

static std::string preferredRegion(const std::string& region)
{
    auto preferred = intlPreferredRegionTag(region);
    if (preferred != "") {
        return preferred;
    }
    return region;
}

static std::string privateUseLangTag(const std::vector<std::string>& parts, size_t startIndex)
{
    size_t numParts = parts.size();
    size_t currentIndex = startIndex;

    // Check for privateuse.
    // privateuse = "x" 1*("-" (2*8alphanum))
    StringBuilder privateuse;
    while (currentIndex < numParts) {
        std::string singleton = parts[currentIndex];
        unsigned singletonLength = singleton.length();
        bool isValid = (singletonLength == 1 && (singleton == "x" || singleton == "X"));
        if (!isValid)
            break;

        if (currentIndex != startIndex)
            privateuse.appendChar('-');

        ++currentIndex;
        unsigned numExtParts = 0;
        privateuse.appendChar('x');
        while (currentIndex < numParts) {
            std::string extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 1 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            privateuse.appendChar('-');
            std::transform(extPart.begin(), extPart.end(), extPart.begin(), tolower);
            privateuse.appendString(String::fromUTF8(extPart.data(), extPart.length()));
        }

        // Requires at least one production.
        if (!numExtParts)
            return std::string();
    }

    // Leftovers makes it invalid.
    if (currentIndex < numParts)
        return std::string();

    return privateuse.finalize()->toNonGCUTF8StringData();
}

static bool canonicalizeLanguageTagHelper(std::vector<std::string>& parts, const std::string& unicodeExtensionNameShouldIgnored, std::unordered_set<std::string>& subtags, const size_t numParts, size_t& currentIndex, Intl::CanonicalizedLangunageTag& result)
{
    // Check for extension.
    // extension = singleton 1*("-" (2*8alphanum))
    // singleton = alphanum except x or X
    subtags.clear();
    while (currentIndex < numParts) {
        std::string possibleSingleton = parts[currentIndex];
        unsigned singletonLength = possibleSingleton.length();
        bool isValidSingleton = (singletonLength == 1 && possibleSingleton != "x" && possibleSingleton != "X" && isASCIIAlphanumeric(possibleSingleton[0]));
        if (!isValidSingleton) {
            break;
        }

        // Cannot include duplicate singleton (case insensitive).
        std::string singleton = possibleSingleton;
        std::transform(possibleSingleton.begin(), possibleSingleton.end(), possibleSingleton.begin(), tolower);

        auto iter = subtags.find(singleton);
        if (iter != subtags.end()) {
            return false;
        }
        subtags.insert(singleton);

        Intl::CanonicalizedLangunageTag::ExtensionSingleton singletonValue;
        singletonValue.key = possibleSingleton[0];

        ++currentIndex;

        if (singletonValue.key == 'u') {
            bool unicodeExtensionIgnored = false;

            // "u" needs ordered key, value
            // "u" doesn't allow single "true" as value

            std::vector<std::pair<std::string, std::vector<std::string>>> values;

            std::string key;
            std::vector<std::string> value;

            while (currentIndex < numParts) {
                std::string extPart = parts[currentIndex];
                unsigned extPartLength = extPart.length();

                bool isValid = (extPartLength >= 2 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
                if (!isValid) {
                    break;
                }

                ++currentIndex;
                std::transform(extPart.begin(), extPart.end(), extPart.begin(), tolower);

                if (extPartLength == 2) {
                    if (key.length() || value.size()) {
                        if (key.length() && value.size() == 1 && value[0] == "true") {
                            value.clear();
                        }
                        values.push_back(std::make_pair(key, value));
                        key.clear();
                        value.clear();
                    }
                    key = extPart;
                } else {
                    value.push_back(extPart);
                }
            }

            bool unicodeExtensionIgnoredOnce = (key.length() && key == unicodeExtensionNameShouldIgnored);
            unicodeExtensionIgnored |= unicodeExtensionIgnoredOnce;

            if (!unicodeExtensionIgnoredOnce && (key.length() || value.size())) {
                if (key.length() && value.size() == 1 && value[0] == "true") {
                    value.clear();
                }
                values.push_back(std::make_pair(key, value));
                key.clear();
                value.clear();
            }

            std::sort(values.begin(), values.end(),
                      [](const std::pair<std::string, std::vector<std::string>>& a, const std::pair<std::string, std::vector<std::string>>& b) -> bool {
                          return a.first < b.first;
                      });

            // Attributes in Unicode extensions are reordered in US-ASCII order.
            if (values.size() && !values.begin()->first.length()) {
                std::sort(values.begin()->second.begin(), values.begin()->second.end());
            }

            std::string resultValue;
            for (size_t i = 0; i < values.size(); i++) {
                if (values[i].first.size()) {
                    if (resultValue.size()) {
                        resultValue += '-';
                    }
                    resultValue += values[i].first;
                }

                for (size_t j = 0; j < values[i].second.size(); j++) {
                    if (resultValue.size()) {
                        resultValue += '-';
                    }
                    resultValue += values[i].second[j];
                }
            }

            singletonValue.value = std::move(resultValue);

            std::vector<std::pair<std::string, std::string>> unicodeExtensionValue;

            for (size_t i = 0; i < values.size(); i++) {
                std::string singleUnicodeExtensionValue;
                for (size_t j = 0; j < values[i].second.size(); j++) {
                    if (singleUnicodeExtensionValue.length()) {
                        singleUnicodeExtensionValue += '-';
                    }
                    singleUnicodeExtensionValue += values[i].second[j];
                }
                unicodeExtensionValue.push_back(std::make_pair(values[i].first, singleUnicodeExtensionValue));
            }

            result.unicodeExtension = std::move(unicodeExtensionValue);

            if (unicodeExtensionIgnored && !result.unicodeExtension.size()) {
                continue;
            }
        } else {
            std::string single;
            while (currentIndex < numParts) {
                std::string extPart = parts[currentIndex];
                unsigned extPartLength = extPart.length();

                bool isValid = (extPartLength >= 2 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
                if (!isValid) {
                    break;
                }

                ++currentIndex;
                std::transform(extPart.begin(), extPart.end(), extPart.begin(), tolower);

                if (single.length()) {
                    single += '-';
                }
                single += extPart;
            }

            singletonValue.value = std::move(single);
        }

        result.extensions.push_back(singletonValue);

        // Requires at least one production.
        if (!result.extensions.back().value.size()) {
            return false;
        }
    }

    return true;
}

Intl::CanonicalizedLangunageTag Intl::canonicalizeLanguageTag(const std::string& locale, const std::string& unicodeExtensionNameShouldIgnored)
{
    std::vector<std::string> parts = split(locale, '-');

    Intl::CanonicalizedLangunageTag result;

    // Follows the grammar at https://www.rfc-editor.org/rfc/bcp/bcp47.txt
    // langtag = language ["-" script] ["-" region] *("-" variant) *("-" extension) ["-" privateuse]
    size_t numParts = parts.size();
    // Check for language.
    // language = 2*3ALPHA ["-" extlang] / 4ALPHA (reserved) / 5*8ALPHA
    size_t currentIndex = 0;
    std::string language = parts[currentIndex];
    unsigned languageLength = language.length();
    bool canHaveExtlang = languageLength >= 2 && languageLength <= 3;
    bool isValidLanguage = languageLength != 4 && languageLength >= 2 && languageLength <= 8 && isAllSpecialCharacters(language, isASCIIAlpha);
    if (!isValidLanguage) {
        return Intl::CanonicalizedLangunageTag();
    }

    ++currentIndex;
    StringBuilder canonical;

    std::transform(language.begin(), language.end(), language.begin(), tolower);
    result.language = language;
    language = Intl::preferredLanguage(language);

    // test legacy cases
    // https://github.com/unicode-org/cldr/blob/master/common/supplemental/supplementalMetadata.xml
    if (result.language == "sh") {
        language = result.language = "sr";
        if (!result.script.length()) {
            result.script = "Latn";
        }
    } else if (result.language == "cnr") {
        language = result.language = "sr";
        if (!result.region.length()) {
            result.region = "ME";
        }
    }

    canonical.appendString(String::fromUTF8(language.data(), language.length()));

    // Check for extlang.
    // extlang = 3ALPHA *2("-" 3ALPHA)
    if (canHaveExtlang) {
        for (unsigned times = 0; times < 3 && currentIndex < numParts; ++times) {
            std::string extlang = parts[currentIndex];
            unsigned extlangLength = extlang.length();
            if (extlangLength == 3 && isAllSpecialCharacters(extlang, isASCIIAlpha)) {
                ++currentIndex;
                std::transform(extlang.begin(), extlang.end(), extlang.begin(), tolower);
                result.extLang.push_back(extlang);
                if (!times && intlPreferredExtlangTag(extlang) == language) {
                    canonical.clear();
                    canonical.appendString(String::fromUTF8(extlang.data(), extlang.length()));
                    continue;
                }
                canonical.appendString("-");
                canonical.appendString(String::fromUTF8(extlang.data(), extlang.length()));
            } else {
                break;
            }
        }
    }

    // Check for script.
    // script = 4ALPHA
    bool isScriptAddToCanonical = false;
    if (currentIndex < numParts) {
        std::string script = parts[currentIndex];
        unsigned scriptLength = script.length();
        if (scriptLength == 4 && isAllSpecialCharacters(script, isASCIIAlpha)) {
            ++currentIndex;
            canonical.appendString("-");
            std::transform(script.begin(), script.end(), script.begin(), tolower);
            result.script += (char)toupper(script[0]);
            canonical.appendChar((char)toupper(script[0]));
            script = script.substr(1, 3);
            result.script += script;
            canonical.appendString(String::fromUTF8(script.data(), script.length()));
            isScriptAddToCanonical = true;
        }
    }

    if (!isScriptAddToCanonical && result.script.length()) {
        canonical.appendString("-");
        canonical.appendString(String::fromUTF8(result.script.data(), result.script.length()));
    }

    // Check for region.
    // region = 2ALPHA / 3DIGIT
    bool isRegionAddToCanonical = false;
    if (currentIndex < numParts) {
        std::string region = parts[currentIndex];
        unsigned regionLength = region.length();
        bool isValidRegion = ((regionLength == 2 && isAllSpecialCharacters(region, isASCIIAlpha))
                              || (regionLength == 3 && isAllSpecialCharacters(region, isASCIIDigit)));
        if (isValidRegion) {
            ++currentIndex;
            canonical.appendChar('-');
            std::transform(region.begin(), region.end(), region.begin(), toupper);
            result.region = region;
            auto preffered = preferredRegion(region);
            canonical.appendString(String::fromUTF8(preffered.data(), preffered.length()));
            isRegionAddToCanonical = true;
        }
    }

    if (!isRegionAddToCanonical && result.region.length()) {
        canonical.appendString("-");
        canonical.appendString(String::fromUTF8(result.region.data(), result.region.length()));
    }

    // Check for variant.
    // variant = 5*8alphanum / (DIGIT 3alphanum)
    std::unordered_set<std::string> subtags;
    while (currentIndex < numParts) {
        std::string variant = parts[currentIndex];
        unsigned variantLength = variant.length();
        bool isValidVariant = ((variantLength >= 5 && variantLength <= 8 && isAllSpecialCharacters(variant, isASCIIAlphanumeric))
                               || (variantLength == 4 && isASCIIDigit(variant[0]) && isAllSpecialCharacters(variant.substr(1, 3), isASCIIAlphanumeric)));
        if (!isValidVariant) {
            break;
        }

        // Cannot include duplicate subtags (case insensitive).
        std::string lowerVariant = variant;
        std::transform(lowerVariant.begin(), lowerVariant.end(), lowerVariant.begin(), tolower);
        auto iter = subtags.find(lowerVariant);
        if (iter != subtags.end()) {
            return Intl::CanonicalizedLangunageTag();
        }
        subtags.insert(lowerVariant);

        ++currentIndex;

        result.variant.push_back(lowerVariant);
    }

    std::sort(result.variant.begin(), result.variant.end());
    for (size_t i = 0; i < result.variant.size(); i++) {
        canonical.appendChar('-');
        canonical.appendString(result.variant[i].data());
    }

    // Check for extension.
    // extension = singleton 1*("-" (2*8alphanum))
    // singleton = alphanum except x or X
    if (!canonicalizeLanguageTagHelper(parts, unicodeExtensionNameShouldIgnored, subtags, numParts, currentIndex, result)) {
        return Intl::CanonicalizedLangunageTag();
    }

    // Add extensions to canonical sorted by singleton.
    std::sort(result.extensions.begin(), result.extensions.end(),
              [](const Intl::CanonicalizedLangunageTag::ExtensionSingleton& a, const Intl::CanonicalizedLangunageTag::ExtensionSingleton& b) -> bool {
                  return a.key < b.key;
              });
    size_t numExtenstions = result.extensions.size();
    for (size_t i = 0; i < numExtenstions; ++i) {
        canonical.appendChar('-');
        canonical.appendChar(result.extensions[i].key);
        canonical.appendChar('-');
        canonical.appendString(String::fromUTF8(result.extensions[i].value.data(), result.extensions[i].value.length()));
    }

    // Check for privateuse.
    if (currentIndex < numParts) {
        std::string privateUse = privateUseLangTag(parts, currentIndex);
        if (privateUse.length() == 0) {
            return Intl::CanonicalizedLangunageTag();
        }
        canonical.appendChar('-');
        canonical.appendString(String::fromUTF8(privateUse.data(), privateUse.length()));
        result.privateUse = privateUse;
    }

    String* e = canonical.finalize();
    auto estd = e->toNonGCUTF8StringData();
    auto preferred = intlRedundantLanguageTag(estd);
    if (preferred != "") {
        e = String::fromUTF8(preferred.data(), preferred.length());
    }

    result.canonicalizedTag = e;
    return result;
}

Intl::CanonicalizedLangunageTag Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(const std::string& locale)
{
    if (isValidTagInBCP47ButInvalidOnUTS35(locale)) {
        return Intl::CanonicalizedLangunageTag();
    }
    std::string grandfather = grandfatheredLangTag(locale);
    if (grandfather.length()) {
        return canonicalizeLanguageTag(grandfather);
    }

    return canonicalizeLanguageTag(locale);
}

String* Intl::icuLocaleToBCP47Tag(String* string)
{
    StringBuilder sb;
    for (size_t i = 0; i < string->length(); i++) {
        char16_t ch = string->charAt(i);
        if (ch == '_')
            ch = '-';
        sb.appendChar(ch);
    }
    return sb.finalize();
}

std::string Intl::convertICUCalendarKeywordToBCP47KeywordIfNeeds(const std::string& icuCalendar)
{
    if (icuCalendar == std::string("gregorian")) {
        return "gregory";
    } else if (icuCalendar == std::string("islamic-civil")) {
        return "islamicc";
    } else if (icuCalendar == std::string("ethiopic-amete-alem")) {
        return "ethioaa";
    }
    return icuCalendar;
}

std::string Intl::convertICUCollationKeywordToBCP47KeywordIfNeeds(const std::string& icuCollation)
{
    if (icuCollation == std::string("dictionary")) {
        return "dict";
    } else if (icuCollation == std::string("gb2312han")) {
        return "gb2312";
    } else if (icuCollation == std::string("phonebook")) {
        return "phonebk";
    } else if (icuCollation == std::string("traditional")) {
        return "trad";
    }
    return icuCollation;
}

static String* defaultLocale(ExecutionState& state)
{
    String* localeString = String::fromUTF8(state.context()->vmInstance()->locale().data(), state.context()->vmInstance()->locale().length());
    return Intl::icuLocaleToBCP47Tag(localeString);
}

static String* removeUnicodeLocaleExtension(ExecutionState& state, String* locale)
{
    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());
    std::vector<std::string> parts = split(stdUTF8, '-');

    StringBuilder builder;
    size_t partsSize = parts.size();
    if (partsSize > 0)
        builder.appendString(String::fromUTF8(parts[0].data(), parts[0].size()));
    for (size_t p = 1; p < partsSize; ++p) {
        if (parts[p] == "u") {
            // Skip the u- and anything that follows until another singleton.
            // While the next part is part of the unicode extension, skip it.
            while (p + 1 < partsSize && parts[p + 1].length() > 1)
                ++p;
        } else {
            builder.appendChar('-');
            builder.appendString(String::fromUTF8(parts[p].data(), parts[p].size()));
        }
    }
    return builder.finalize();
}

ValueVector Intl::canonicalizeLocaleList(ExecutionState& state, Value locales)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.1
    // If locales is undefined, then
    if (locales.isUndefined()) {
        // Return a new empty List.
        return ValueVector();
    }
    // Let seen be a new empty List.
    ValueVector seen;

    Object* O;
    // If Type(locales) is String or locales has an [[InitializedLocale]] internal slot, then
    if (locales.isString() || (locales.isObject() && locales.asObject()->isIntlLocaleObject())) {
        // Let O be CreateArrayFromList(« locales »).
        ValueVector vv;
        vv.push_back(locales);
        O = Object::createArrayFromList(state, vv);
    } else {
        // Else
        // Let O be ToObject(locales).
        O = locales.toObject(state);
    }

    uint64_t len = O->length(state);
    // Let k be 0.
    // Repeat, while k < len
    uint64_t k = 0;
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectPropertyName pk(state, Value(k));
        ObjectHasPropertyResult pkResult = O->hasProperty(state, pk);
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        // If kPresent is true, then
        if (pkResult.hasProperty()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = pkResult.value(state, pk, O);
            // If the type of kValue is not String or Object, then throw a TypeError exception.
            if (!kValue.isString() && !kValue.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Type of element of locales must be String or Object");
            }

            String* tag;
            // If Type(kValue) is Object and kValue has an [[InitializedLocale]] internal slot, then
            if (kValue.isObject() && kValue.asObject()->isIntlLocaleObject()) {
                // Let tag be kValue.[[Locale]].
                tag = kValue.asObject()->asIntlLocaleObject()->locale();
            } else {
                // Else,
                // Let tag be ToString(kValue).
                tag = kValue.toString(state);
            }
            // If the result of calling the abstract operation IsStructurallyValidLanguageTag (defined in 6.2.2), passing tag as the argument,
            // is false, then throw a RangeError exception.
            // Let tag be the result of calling the abstract operation CanonicalizeLanguageTag (defined in 6.2.3), passing tag as the argument.
            // If tag is not an element of seen, then append tag as the last element of seen.
            auto canonicalizedTag = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(tag->toNonGCUTF8StringData());
            if (!canonicalizedTag.canonicalizedTag) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "got Invalid locale");
            }
            tag = canonicalizedTag.canonicalizedTag.value();
            bool has = false;
            for (size_t i = 0; i < seen.size(); i++) {
                if (seen[i].equalsTo(state, tag)) {
                    has = true;
                    break;
                }
            }
            if (!has) {
                seen.pushBack(tag);
            }

            // Increase k by 1.
            k++;
        } else {
            // Increase k by 1.
            int64_t nextIndex;
            Object::nextIndexForward(state, O, k, len, nextIndex);
            k = nextIndex;
        }
    }

    // Return seen.
    return seen;
}

static String* bestAvailableLocale(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, Value locale)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.2
    // Let candidate be locale.
    String* candidate = locale.toString(state);

    // Repeat
    while (candidate->length()) {
        // If availableLocales contains an element equal to candidate, then return candidate.
        bool contains = false;
        for (size_t i = 0; i < availableLocales.size(); i++) {
            if (availableLocales[i]->equals(candidate)) {
                contains = true;
                break;
            }
        }

        if (contains)
            return candidate;

        // Let pos be the character index of the last occurrence of "-" (U+002D) within candidate. If that character does not occur, return undefined.
        size_t pos = candidate->rfind(state.context()->staticStrings().asciiTable[(size_t)'-'].string(), candidate->length() - 1);
        if (pos == SIZE_MAX)
            return String::emptyString;

        // If pos ≥ 2 and the character "-" occurs at index pos-2 of candidate, then decrease pos by 2.
        if (pos >= 2 && candidate->charAt(pos - 2) == '-')
            pos -= 2;

        // d. Let candidate be the substring of candidate from position 0, inclusive, to position pos, exclusive.
        candidate = candidate->substring(0, pos);
    }

    return String::emptyString;
}

static Intl::IntlMatcherResult lookupMatcher(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.3
    // Let i be 0.
    size_t i = 0;
    // Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();
    // Let availableLocale be undefined.
    String* availableLocale = String::emptyString;

    String* locale = String::emptyString;
    String* noExtensionsLocale = String::emptyString;

    // Repeat while i < len and availableLocale is undefined:
    while (i < len && availableLocale->length() == 0) {
        // Let locale be the element of requestedLocales at 0-origined list position i.
        locale = requestedLocales[i].toString(state);
        // Let noExtensionsLocale be the String value that is locale with all Unicode locale extension sequences removed.
        noExtensionsLocale = removeUnicodeLocaleExtension(state, locale);
        // Let availableLocale be the result of calling the BestAvailableLocale abstract operation (defined in 9.2.2) with arguments availableLocales and noExtensionsLocale.
        availableLocale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
        // Increase i by 1.
        i++;
    }

    // Let result be a new Record.
    Intl::IntlMatcherResult result;

    // If availableLocale is not undefined, then
    if (availableLocale->length()) {
        // Set result.[[locale]] to availableLocale.
        result.locale = availableLocale;
        // If locale and noExtensionsLocale are not the same String value, then
        if (!locale->equals(noExtensionsLocale)) {
            // Let extension be the String value consisting of the first substring of locale that is a Unicode locale extension sequence.
            // Let extensionIndex be the character position of the initial "-" of the first Unicode locale extension sequence within locale.
            // Set result.[[extension]] to extension.
            // Set result.[[extensionIndex]] to extensionIndex.

            bool unicodeExtensionExists = false;
            if (locale->find("-x-") != SIZE_MAX) {
                unicodeExtensionExists = true;
            }

            if (!unicodeExtensionExists) {
                size_t extensionIndex = locale->find("-u-");
                ASSERT(extensionIndex != SIZE_MAX);

                size_t extensionLength = locale->length() - extensionIndex;
                size_t end = extensionIndex + 3;
                while (end < locale->length()) {
                    end = locale->find("-", end);
                    if (end == SIZE_MAX)
                        break;
                    if (end + 2 < locale->length() && locale->charAt(end + 2) == '-') {
                        extensionLength = end - extensionIndex;
                        break;
                    }
                    end++;
                }
                result.extension = locale->substring(extensionIndex, extensionIndex + extensionLength);
                result.extensionIndex = extensionIndex;
            }
        }
    } else {
        // Set result.[[locale]] to the value returned by the DefaultLocale abstract operation (defined in 6.2.4).
        result.locale = defaultLocale(state);
    }

    return result;
}

static Intl::IntlMatcherResult bestFitMatcher(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
{
    // TODO
    return lookupMatcher(state, availableLocales, requestedLocales);
}

static Optional<String*> unicodeExtensionValue(ExecutionState& state, String* extension, const char* key)
{
    // Let size be the number of elements in extension.
    auto size = extension->length();
    // Let searchValue be the concatenation of "-", key, and "-".
    StringBuilder searchValueBuilder;
    searchValueBuilder.appendChar('-');
    searchValueBuilder.appendString(key);
    searchValueBuilder.appendChar('-');
    String* searchValue = searchValueBuilder.finalize();

    // Let pos be Call(%StringProto_indexOf%, extension, « searchValue »).
    size_t pos = extension->find(searchValue);

    // If pos ≠ -1, then
    if (pos != SIZE_MAX) {
        // Let start be pos + 4.
        size_t start = pos + 4;
        // Let end be start.
        size_t end = start;
        // Let k be start.
        size_t k = start;
        // Let done be false.
        bool done = false;

        // Repeat, while done is false
        while (!done) {
            // Let e be Call(%StringProto_indexOf%, extension, « "-", k »).
            size_t e = extension->find("-", k);
            size_t len;
            // If e = -1,
            if (e == SIZE_MAX) {
                // let len be size - k;
                len = size - k;
            } else {
                // else let len be e - k.
                len = e - k;
            }

            // If len = 2, then
            if (len == 2) {
                // Let done be true.
                done = true;
            } else if (e == SIZE_MAX) {
                // Else if e = -1, then
                // Let end be size.
                end = size;
                // Let done be true.
                done = true;
            } else {
                // Else,
                // Let end be e.
                end = e;
                // Let k be e + 1.
                k = e + 1;
            }
        }

        // Return the String value equal to the substring of extension
        // consisting of the code units at indices start (inclusive) through end (exclusive).
        return extension->substring(start, end);
    }

    // Let searchValue be the concatenation of "-" and key.
    searchValueBuilder.appendChar('-');
    searchValueBuilder.appendString(key);
    searchValue = searchValueBuilder.finalize();
    // Let pos be Call(%StringProto_indexOf%, extension, « searchValue »).
    pos = extension->find(searchValue);
    // If pos ≠ -1 and pos + 3 = size, then
    if (pos != SIZE_MAX && pos + 3 == size) {
        return String::emptyString;
    }

    return nullptr;
}


static bool stringVectorContains(const std::vector<std::string>& v, const std::string& key)
{
    for (size_t j = 0; j < v.size(); j++) {
        if (v[j] == key) {
            return true;
        }
    }

    return false;
}


StringMap Intl::resolveLocale(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, const StringMap& options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-resolvelocale

    // Let matcher be the value of options.[[localeMatcher]].
    auto iter = options.find("localeMatcher");
    Value matcher;
    if (iter != options.end()) {
        matcher = iter->second;
    }

    Intl::IntlMatcherResult r;
    // If matcher is "lookup", then
    if (matcher.equalsTo(state, state.context()->staticStrings().lazyLookup().string())) {
        // Let r be LookupMatcher(availableLocales, requestedLocales).
        r = lookupMatcher(state, availableLocales, requestedLocales);
    } else {
        // Else
        // Let r be BestFitMatcher(availableLocales, requestedLocales).
        r = bestFitMatcher(state, availableLocales, requestedLocales);
    }

    // Let foundLocale be the value of r.[[locale]].
    String* foundLocale = r.locale;

    // Let result be a new Record.
    StringMap result;

    // Set result.[[dataLocale]] to foundLocale.
    result.insert(std::make_pair("dataLocale", foundLocale));

    // Let supportedExtension be "-u".
    String* supportedExtension = state.context()->staticStrings().lazyDashU().string();

    // For each element key of relevantExtensionKeys in List order, do
    size_t i = 0;
    size_t len = relevantExtensionKeyCount;
    while (i < len) {
        const char* key = relevantExtensionKeys[i];
        // Let foundLocaleData be localeData.[[<foundLocale>]].
        // Let keyLocaleData be foundLocaleData.[[<key>]].
        std::vector<std::string> keyLocaleData = localeData(foundLocale, i);
        // Let value be keyLocaleData[0].
        std::string value = keyLocaleData[0];

        // Let supportedExtensionAddition be "".
        String* supportedExtensionAddition = String::emptyString;

        // If r has an [[extension]] field, then
        if (r.extension->length()) {
            // Let requestedValue be UnicodeExtensionValue(r.[[extension]], key).
            Optional<String*> requestedValue = unicodeExtensionValue(state, r.extension, key);

            // If requestedValue is not undefined, then
            if (requestedValue) {
                // If requestedValue is not the empty String, then
                if (requestedValue.value()->length()) {
                    // If keyLocaleData contains requestedValue, then
                    if (stringVectorContains(keyLocaleData, requestedValue.value()->toNonGCUTF8StringData())) {
                        // Let value be requestedValue.
                        value = requestedValue.value()->toNonGCUTF8StringData();
                        // Let supportedExtensionAddition be the concatenation of "-", key, "-", and value.
                        StringBuilder supportedExtensionAdditionBuilder;
                        supportedExtensionAdditionBuilder.appendChar('-');
                        supportedExtensionAdditionBuilder.appendString(key);
                        supportedExtensionAdditionBuilder.appendChar('-');
                        supportedExtensionAdditionBuilder.appendString(value.data());
                        supportedExtensionAddition = supportedExtensionAdditionBuilder.finalize();
                    }
                } else if (stringVectorContains(keyLocaleData, "true")) {
                    // Else if keyLocaleData contains "true", then
                    // Let value be "true".
                    value = "true";
                    // Let supportedExtensionAddition be the concatenation of "-" and key.
                    StringBuilder supportedExtensionAdditionBuilder;
                    supportedExtensionAdditionBuilder.appendChar('-');
                    supportedExtensionAdditionBuilder.appendString(key);
                    supportedExtensionAddition = supportedExtensionAdditionBuilder.finalize();
                }
            }
        }

        // If options has a field [[<key>]], then
        if (options.find(key) != options.end()) {
            // Let optionsValue be options.[[<key>]].
            auto optionsValue = options.at(key);
            // Assert: Type(optionsValue) is either String, Undefined, or Null.
            // If keyLocaleData contains optionsValue, then
            if (stringVectorContains(keyLocaleData, optionsValue->toNonGCUTF8StringData())) {
                // If SameValue(optionsValue, value) is false, then
                if (!optionsValue->equals(value.data(), value.length())) {
                    // Let value be optionsValue.
                    value = optionsValue->toNonGCUTF8StringData();
                    // Let supportedExtensionAddition be "".
                    supportedExtensionAddition = String::emptyString;
                }
            }
        }

        // Set result.[[<key>]] to value.
        result.insert(std::make_pair(key, String::fromUTF8(value.data(), value.length())));

        // Append supportedExtensionAddition to supportedExtension.
        StringBuilder sb;
        sb.appendString(supportedExtension);
        sb.appendString(supportedExtensionAddition);
        supportedExtension = sb.finalize();

        i++;
    }

    // If the length of supportedExtension is greater than 2, then
    if (supportedExtension->length() > 2) {
        // Let privateIndex be Call(%StringProto_indexOf%, foundLocale, « "-x-" »).
        size_t privateIndex = foundLocale->find("-x-");

        // If privateIndex = -1, then
        if (privateIndex != SIZE_MAX) {
            // Let foundLocale be the concatenation of foundLocale and supportedExtension.
            StringBuilder sb;
            sb.appendString(foundLocale);
            sb.appendString(supportedExtension);
            foundLocale = sb.finalize();
        } else {
            size_t len = foundLocale->length();
            // Let preExtension be the substring of foundLocale from position 0, inclusive, to position extensionIndex, exclusive.
            String* preExtension = foundLocale->substring(0, len > r.extensionIndex ? r.extensionIndex : len);

            // Let postExtension be the substring of foundLocale from position extensionIndex to the end of the string.
            String* postExtension = String::emptyString;
            if (r.extensionIndex < len) {
                postExtension = foundLocale->substring(r.extensionIndex, len);
            }
            // Let foundLocale be the concatenation of preExtension, supportedExtension, and postExtension.
            StringBuilder sb;
            sb.appendString(preExtension);
            sb.appendString(supportedExtension);
            sb.appendString(postExtension);
            foundLocale = sb.finalize();
        }

        // Let foundLocale be CanonicalizeLanguageTag(foundLocale).
        // Assert: IsStructurallyValidLanguageTag(foundLocale) is true.
        foundLocale = Intl::isStructurallyValidLanguageTagAndCanonicalizeLanguageTag(foundLocale->toNonGCUTF8StringData()).canonicalizedTag.value();
    }
    // Set result.[[locale]] to foundLocale.
    result.insert(std::make_pair("locale", foundLocale));
    // Return result.
    return result;
}


static ValueVector lookupSupportedLocales(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.6
    // Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();
    // Let subset be a new empty List.
    ValueVector subset;
    // Let k be 0.
    size_t k = 0;
    // Repeat while k < len
    while (k < len) {
        // Let locale be the element of requestedLocales at 0-origined list position k.
        Value locale = requestedLocales[k];
        // Let noExtensionsLocale be the String value that is locale with all Unicode locale extension sequences removed.
        String* noExtensionsLocale = removeUnicodeLocaleExtension(state, locale.toString(state));
        // Let availableLocale be the result of calling the BestAvailableLocale abstract operation (defined in 9.2.2) with arguments availableLocales and noExtensionsLocale.
        String* availableLocale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
        // If availableLocale is not undefined, then append locale to the end of subset.
        if (availableLocale->length()) {
            subset.pushBack(locale);
        }
        // Increment k by 1.
        k++;
    }

    return subset;
}

static ValueVector bestfitSupportedLocales(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.7
    // TODO
    return lookupSupportedLocales(state, availableLocales, requestedLocales);
}

Value Intl::supportedLocales(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.8
    Value matcher;
    // If options is not undefined, then
    if (!options.isUndefined()) {
        // Let options be ToObject(options).
        options = options.toObject(state);
        // Let matcher be the result of calling the [[Get]] internal method of options with argument "localeMatcher".
        matcher = options.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().lazyLocaleMatcher())).value(state, options.asObject());
        // If matcher is not undefined, then
        if (!matcher.isUndefined()) {
            // Let matcher be ToString(matcher).
            matcher = matcher.toString(state);
            // If matcher is not "lookup" or "best fit", then throw a RangeError exception.
            if (!(matcher.asString()->equals("lookup") || matcher.asString()->equals("best fit"))) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "got invalid value on options.localeMatcher");
            }
        }
    }

    // If matcher is undefined or "best fit", then
    ValueVector subset;
    if (matcher.isUndefined() || matcher.equalsTo(state, state.context()->staticStrings().lazyBestFit().string())) {
        // Let subset be the result of calling the BestFitSupportedLocales abstract operation (defined in 9.2.7) with arguments availableLocales and requestedLocales.
        subset = bestfitSupportedLocales(state, availableLocales, requestedLocales);
    } else {
        // Let subset be the result of calling the LookupSupportedLocales abstract operation (defined in 9.2.6) with arguments availableLocales and requestedLocales.
        subset = lookupSupportedLocales(state, availableLocales, requestedLocales);
    }

    // Return CreateArrayFromList(supportedLocales).
    return Object::createArrayFromList(state, subset);
}

Value Intl::getOption(ExecutionState& state, Object* options, Value property, Intl::OptionValueType type, Value* values, size_t valuesLength, const Value& fallback)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.9
    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Assert: type is "boolean" or "string".
        // If type is "boolean", then let value be ToBoolean(value).
        if (type == Intl::OptionValueType::BooleanValue) {
            value = Value(value.toBoolean());
        }
        // If type is "string", then let value be ToString(value).
        if (type == Intl::OptionValueType::StringValue) {
            value = Value(value.toString(state));
        }
        // If values is not undefined, then
        if (valuesLength) {
            // If values does not contain an element equal to value, then throw a RangeError exception.
            bool contains = false;
            for (size_t i = 0; i < valuesLength; i++) {
                if (values[i].equalsTo(state, value)) {
                    contains = true;
                }
            }
            if (!contains) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "got invalid value");
            }
        }
        // Return value.
        return value;
    } else {
        // Else return fallback.
        return fallback;
    }
}

template <typename T>
T getNumberOptionResultFactory(double value)
{
    return T(value);
}

template <>
Value getNumberOptionResultFactory(double value)
{
    return Value(Value::DoubleToIntConvertibleTestNeeds, value);
}

template <typename T>
T Intl::getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const T& fallback)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.10
    if (!options) {
        return fallback;
    }

    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options.value());
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Let value be ToNumber(value).
        double doubleValue = value.toNumber(state);
        // If value is NaN or less than minimum or greater than maximum, throw a RangeError exception.
        if (std::isnan(doubleValue) || doubleValue < minimum || maximum < doubleValue) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Got invalid number option value");
        }
        // Return floor(value).
        return getNumberOptionResultFactory<T>(floor(doubleValue));
    } else {
        // Else return fallback.
        return fallback;
    }
}

template Value Intl::getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const Value& fallback);
template double Intl::getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const double& fallback);
template int Intl::getNumberOption(ExecutionState& state, Optional<Object*> options, String* property, double minimum, double maximum, const int& fallback);

static std::vector<std::string> initAvailableNumberingSystems()
{
    std::vector<std::string> availableNumberingSystems;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* numberingSystemNames = unumsys_openAvailableNames(&status);
    ASSERT(U_SUCCESS(status));

    int32_t resultLength;
    // Numbering system names are always ASCII, so use char[].
    while (const char* result = uenum_next(numberingSystemNames, &resultLength, &status)) {
        ASSERT(U_SUCCESS(status));
        auto numsys = unumsys_openByName(result, &status);
        ASSERT(U_SUCCESS(status));
        if (!unumsys_isAlgorithmic(numsys)) {
            availableNumberingSystems.push_back(std::string(result, resultLength));
        }
        unumsys_close(numsys);
    }
    uenum_close(numberingSystemNames);

    return availableNumberingSystems;
}

std::vector<std::string> Intl::numberingSystemsForLocale(String* locale)
{
    static std::vector<std::string> availableNumberingSystems = initAvailableNumberingSystems();

    UErrorCode status = U_ZERO_ERROR;
    UNumberingSystem* defaultSystem = unumsys_open(locale->toUTF8StringData().data(), &status);
    ASSERT(U_SUCCESS(status));
    std::string defaultSystemName(unumsys_getName(defaultSystem));
    unumsys_close(defaultSystem);

    std::vector<std::string> numberingSystems;
    numberingSystems.push_back(defaultSystemName);
    numberingSystems.insert(numberingSystems.end(), availableNumberingSystems.begin(), availableNumberingSystems.end());
    return numberingSystems;
}

String* Intl::getLocaleForStringLocaleConvertCase(ExecutionState& state, Value locales)
{
    // Let requestedLocales be ? CanonicalizeLocaleList(locales).
    ValueVector requestedLocales = Intl::canonicalizeLocaleList(state, locales);
    // If requestedLocales is not an empty List, then
    // a. Let requestedLocale be requestedLocales[0].
    // Else Let requestedLocale be DefaultLocale().
    String* requestedLocale = requestedLocales.size() > 0 ? requestedLocales[0].toString(state) : defaultLocale(state);
    // Let noExtensionsLocale be the String value that is requestedLocale with all Unicode locale extension sequences (6.2.1) removed.
    String* noExtensionsLocale = removeUnicodeLocaleExtension(state, requestedLocale);

    // Let availableLocales be a List with language tags that includes the languages for which the Unicode Character Database contains language sensitive case mappings. Implementations may add additional language tags if they support case mapping for additional locales.
    const auto& availableLocales = state.context()->vmInstance()->caseMappingAvailableLocales();
    // Let locale be BestAvailableLocale(availableLocales, noExtensionsLocale).
    String* locale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
    return locale;
}

static bool is38Alphanum(const std::string& str)
{
    if (str.length() >= 3 && str.length() <= 8) {
        return isAllSpecialCharacters(str, isASCIIAlphanumeric);
    }
    return false;
}

static bool is38AlphanumList(const std::string& str, bool allowUnderScore = false)
{
    std::size_t found = str.find("-");
    if (found == std::string::npos) {
        if (allowUnderScore) {
            found = str.find("_");
            if (found != std::string::npos) {
                return is38Alphanum(str.substr(0, found)) && is38AlphanumList(str.substr(found + 1), allowUnderScore);
            }
        }
        return is38Alphanum(str);
    }
    return is38Alphanum(str.substr(0, found)) && is38AlphanumList(str.substr(found + 1), allowUnderScore);
}

bool Intl::isValidUnicodeLocaleIdentifierTypeNonterminalOrTypeSequence(String* value)
{
    return is38AlphanumList(value->toNonGCUTF8StringData());
}

bool Intl::isValidUnicodeLocaleIdentifier(String* value)
{
    return is38AlphanumList(value->toNonGCUTF8StringData(), true);
}

void Intl::convertICUNumberFieldToEcmaNumberField(std::vector<NumberFieldItem>& fields, double x, const UTF16StringDataNonGCStd& resultString)
{
    // we need to divide integer field
    // because icu returns result as
    // 1.234 $
    // "1.234" <- integer field
    // but we want
    // "1", "234"
    std::vector<NumberFieldItem> integerFields;
    for (size_t i = 0; i < fields.size(); i++) {
        NumberFieldItem item = fields[i];

        if (item.type == UNUM_INTEGER_FIELD) {
            fields.erase(fields.begin() + i);
            i--;

            std::vector<std::pair<int32_t, int32_t>> apertures;

            for (size_t j = 0; j < fields.size(); j++) {
                if (fields[j].start >= item.start && fields[j].end <= item.end) {
                    apertures.push_back(std::make_pair(fields[j].start, fields[j].end));
                }
            }
            if (apertures.size()) {
                NumberFieldItem newItem;
                newItem.type = item.type;

                int32_t lastStart = item.start;
                for (size_t j = 0; j < apertures.size(); j++) {
                    newItem.start = lastStart;
                    newItem.end = apertures[j].first;
                    integerFields.push_back(newItem);

                    lastStart = apertures[j].second;
                }

                if (lastStart != item.end) {
                    newItem.start = lastStart;
                    newItem.end = item.end;
                    integerFields.push_back(newItem);
                }


            } else {
                integerFields.push_back(item);
            }
        }
    }

    fields.insert(fields.end(), integerFields.begin(), integerFields.end());

    // add literal field if needs
    if (!std::isnan(x) && !std::isinf(x)) {
        std::vector<NumberFieldItem> literalFields;
        for (size_t i = 0; i < resultString.length(); i++) {
            bool found = false;
            for (size_t j = 0; j < fields.size(); j++) {
                if ((size_t)fields[j].start <= i && i < (size_t)fields[j].end) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                size_t end = i + 1;
                while (end != resultString.length()) {
                    bool found = false;
                    for (size_t j = 0; j < fields.size(); j++) {
                        if ((size_t)fields[j].start <= end && end < (size_t)fields[j].end) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        break;
                    }
                    end++;
                }

                NumberFieldItem newItem;
                newItem.type = -1;
                newItem.start = i;
                newItem.end = end;
                i = end - 1;
                literalFields.push_back(newItem);
            }
        }
        fields.insert(fields.end(), literalFields.begin(), literalFields.end());
    }

    std::sort(fields.begin(), fields.end(),
              [](const NumberFieldItem& a, const NumberFieldItem& b) -> bool {
                  return a.start < b.start;
              });
}

String* Intl::icuNumberFieldToString(ExecutionState& state, int32_t fieldName, double d)
{
    if (fieldName == -1) {
        return state.context()->staticStrings().lazyLiteral().string();
    }

    switch ((UNumberFormatFields)fieldName) {
    case UNUM_INTEGER_FIELD:
        if (std::isnan(d)) {
            return state.context()->staticStrings().lazySmallLetterNaN().string();
        }
        if (std::isinf(d)) {
            return state.context()->staticStrings().lazySmallLetterInfinity().string();
        }
        return state.context()->staticStrings().lazyInteger().string();
    case UNUM_GROUPING_SEPARATOR_FIELD:
        return state.context()->staticStrings().lazyGroup().string();
    case UNUM_DECIMAL_SEPARATOR_FIELD:
        return state.context()->staticStrings().lazyDecimal().string();
    case UNUM_FRACTION_FIELD:
        return state.context()->staticStrings().lazyFraction().string();
    case UNUM_SIGN_FIELD:
        return std::signbit(d) ? state.context()->staticStrings().lazyMinusSign().string() : state.context()->staticStrings().lazyPlusSign().string();
    case UNUM_PERCENT_FIELD:
        return state.context()->staticStrings().lazyPercentSign().string();
    case UNUM_CURRENCY_FIELD:
        return state.context()->staticStrings().lazyCurrency().string();
    case UNUM_EXPONENT_SYMBOL_FIELD:
        return state.context()->staticStrings().lazyExponentSeparator().string();
    case UNUM_EXPONENT_SIGN_FIELD:
        return state.context()->staticStrings().lazyExponentMinusSign().string();
    case UNUM_EXPONENT_FIELD:
        return state.context()->staticStrings().lazyExponentInteger().string();
#ifndef UNUM_MEASURE_UNIT_FIELD
#define UNUM_MEASURE_UNIT_FIELD (UNUM_SIGN_FIELD + 1)
#endif
#ifndef UNUM_COMPACT_FIELD
#define UNUM_COMPACT_FIELD (UNUM_SIGN_FIELD + 2)
#endif
    case UNUM_MEASURE_UNIT_FIELD:
        return state.context()->staticStrings().lazyUnit().string();
    case UNUM_COMPACT_FIELD:
        return state.context()->staticStrings().lazyCompact().string();
    default:
        ASSERT_NOT_REACHED();
        return String::emptyString;
    }
}

} // namespace Escargot

#endif
