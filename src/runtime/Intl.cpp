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
#include "runtime/Intl.h"
#include "runtime/VMInstance.h"
#include "runtime/IntlLocale.h"

namespace Escargot {

static std::string grandfatheredLangTag(const std::string& locale)
{
    // grandfathered = irregular / regular
    std::unordered_map<std::string, std::string> tagMap;
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
    tagMap["cel-gaulish"] = "cel-gaulish";
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

static std::string intlPreferredLanguageTag(const std::string& tag)
{
    // 78 possible replacements
    if (tag == "aam")
        return "aas";
    if (tag == "adp")
        return "dz";
    if (tag == "aue")
        return "ktz";
    if (tag == "ayx")
        return "nun";
    if (tag == "bgm")
        return "bcg";
    if (tag == "bjd")
        return "drl";
    if (tag == "ccq")
        return "rki";
    if (tag == "cjr")
        return "mom";
    if (tag == "cka")
        return "cmr";
    if (tag == "cmk")
        return "xch";
    if (tag == "coy")
        return "pij";
    if (tag == "cqu")
        return "quh";
    if (tag == "drh")
        return "khk";
    if (tag == "drw")
        return "prs";
    if (tag == "gav")
        return "dev";
    if (tag == "gfx")
        return "vaj";
    if (tag == "ggn")
        return "gvr";
    if (tag == "gti")
        return "nyc";
    if (tag == "guv")
        return "duz";
    if (tag == "hrr")
        return "jal";
    if (tag == "ibi")
        return "opa";
    if (tag == "ilw")
        return "gal";
    if (tag == "in")
        return "id";
    if (tag == "iw")
        return "he";
    if (tag == "jeg")
        return "oyb";
    if (tag == "ji")
        return "yi";
    if (tag == "jw")
        return "jv";
    if (tag == "kgc")
        return "tdf";
    if (tag == "kgh")
        return "kml";
    if (tag == "koj")
        return "kwv";
    if (tag == "krm")
        return "bmf";
    if (tag == "ktr")
        return "dtp";
    if (tag == "kvs")
        return "gdj";
    if (tag == "kwq")
        return "yam";
    if (tag == "kxe")
        return "tvd";
    if (tag == "kzj")
        return "dtp";
    if (tag == "kzt")
        return "dtp";
    if (tag == "lii")
        return "raq";
    if (tag == "lmm")
        return "rmx";
    if (tag == "meg")
        return "cir";
    if (tag == "mo")
        return "ro";
    if (tag == "mst")
        return "mry";
    if (tag == "mwj")
        return "vaj";
    if (tag == "myt")
        return "mry";
    if (tag == "nad")
        return "xny";
    if (tag == "ncp")
        return "kdz";
    if (tag == "nnx")
        return "ngv";
    if (tag == "nts")
        return "pij";
    if (tag == "oun")
        return "vaj";
    if (tag == "pcr")
        return "adx";
    if (tag == "pmc")
        return "huw";
    if (tag == "pmu")
        return "phr";
    if (tag == "ppa")
        return "bfy";
    if (tag == "ppr")
        return "lcq";
    if (tag == "pry")
        return "prt";
    if (tag == "puz")
        return "pub";
    if (tag == "sca")
        return "hle";
    if (tag == "skk")
        return "oyb";
    if (tag == "tdu")
        return "dtp";
    if (tag == "thc")
        return "tpo";
    if (tag == "thx")
        return "oyb";
    if (tag == "tie")
        return "ras";
    if (tag == "tkk")
        return "twm";
    if (tag == "tlw")
        return "weo";
    if (tag == "tmp")
        return "tyj";
    if (tag == "tne")
        return "kak";
    if (tag == "tnf")
        return "prs";
    if (tag == "tsf")
        return "taj";
    if (tag == "uok")
        return "ema";
    if (tag == "xba")
        return "cax";
    if (tag == "xia")
        return "acn";
    if (tag == "xkh")
        return "waw";
    if (tag == "xsj")
        return "suj";
    if (tag == "ybd")
        return "rki";
    if (tag == "yma")
        return "lrr";
    if (tag == "ymt")
        return "mtm";
    if (tag == "yos")
        return "zom";
    if (tag == "yuu")
        return "yug";
    return "";
}

static std::string intlRedundantLanguageTag(const std::string& tag)
{
    // 24 possible replacements
    if (tag == "hy-arevela")
        return "hy";
    if (tag == "hy-arevmda")
        return "hyw";
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

static std::string intlPreferredExtlangTag(const std::string& tag)
{
    // 235 possible replacements
    if (tag == "aao")
        return "ar";
    if (tag == "abh")
        return "ar";
    if (tag == "abv")
        return "ar";
    if (tag == "acm")
        return "ar";
    if (tag == "acq")
        return "ar";
    if (tag == "acw")
        return "ar";
    if (tag == "acx")
        return "ar";
    if (tag == "acy")
        return "ar";
    if (tag == "adf")
        return "ar";
    if (tag == "ads")
        return "sgn";
    if (tag == "aeb")
        return "ar";
    if (tag == "aec")
        return "ar";
    if (tag == "aed")
        return "sgn";
    if (tag == "aen")
        return "sgn";
    if (tag == "afb")
        return "ar";
    if (tag == "afg")
        return "sgn";
    if (tag == "ajp")
        return "ar";
    if (tag == "apc")
        return "ar";
    if (tag == "apd")
        return "ar";
    if (tag == "arb")
        return "ar";
    if (tag == "arq")
        return "ar";
    if (tag == "ars")
        return "ar";
    if (tag == "ary")
        return "ar";
    if (tag == "arz")
        return "ar";
    if (tag == "ase")
        return "sgn";
    if (tag == "asf")
        return "sgn";
    if (tag == "asp")
        return "sgn";
    if (tag == "asq")
        return "sgn";
    if (tag == "asw")
        return "sgn";
    if (tag == "auz")
        return "ar";
    if (tag == "avl")
        return "ar";
    if (tag == "ayh")
        return "ar";
    if (tag == "ayl")
        return "ar";
    if (tag == "ayn")
        return "ar";
    if (tag == "ayp")
        return "ar";
    if (tag == "bbz")
        return "ar";
    if (tag == "bfi")
        return "sgn";
    if (tag == "bfk")
        return "sgn";
    if (tag == "bjn")
        return "ms";
    if (tag == "bog")
        return "sgn";
    if (tag == "bqn")
        return "sgn";
    if (tag == "bqy")
        return "sgn";
    if (tag == "btj")
        return "ms";
    if (tag == "bve")
        return "ms";
    if (tag == "bvl")
        return "sgn";
    if (tag == "bvu")
        return "ms";
    if (tag == "bzs")
        return "sgn";
    if (tag == "cdo")
        return "zh";
    if (tag == "cds")
        return "sgn";
    if (tag == "cjy")
        return "zh";
    if (tag == "cmn")
        return "zh";
    if (tag == "coa")
        return "ms";
    if (tag == "cpx")
        return "zh";
    if (tag == "csc")
        return "sgn";
    if (tag == "csd")
        return "sgn";
    if (tag == "cse")
        return "sgn";
    if (tag == "csf")
        return "sgn";
    if (tag == "csg")
        return "sgn";
    if (tag == "csl")
        return "sgn";
    if (tag == "csn")
        return "sgn";
    if (tag == "csq")
        return "sgn";
    if (tag == "csr")
        return "sgn";
    if (tag == "czh")
        return "zh";
    if (tag == "czo")
        return "zh";
    if (tag == "doq")
        return "sgn";
    if (tag == "dse")
        return "sgn";
    if (tag == "dsl")
        return "sgn";
    if (tag == "dup")
        return "ms";
    if (tag == "ecs")
        return "sgn";
    if (tag == "esl")
        return "sgn";
    if (tag == "esn")
        return "sgn";
    if (tag == "eso")
        return "sgn";
    if (tag == "eth")
        return "sgn";
    if (tag == "fcs")
        return "sgn";
    if (tag == "fse")
        return "sgn";
    if (tag == "fsl")
        return "sgn";
    if (tag == "fss")
        return "sgn";
    if (tag == "gan")
        return "zh";
    if (tag == "gds")
        return "sgn";
    if (tag == "gom")
        return "kok";
    if (tag == "gse")
        return "sgn";
    if (tag == "gsg")
        return "sgn";
    if (tag == "gsm")
        return "sgn";
    if (tag == "gss")
        return "sgn";
    if (tag == "gus")
        return "sgn";
    if (tag == "hab")
        return "sgn";
    if (tag == "haf")
        return "sgn";
    if (tag == "hak")
        return "zh";
    if (tag == "hds")
        return "sgn";
    if (tag == "hji")
        return "ms";
    if (tag == "hks")
        return "sgn";
    if (tag == "hos")
        return "sgn";
    if (tag == "hps")
        return "sgn";
    if (tag == "hsh")
        return "sgn";
    if (tag == "hsl")
        return "sgn";
    if (tag == "hsn")
        return "zh";
    if (tag == "icl")
        return "sgn";
    if (tag == "iks")
        return "sgn";
    if (tag == "ils")
        return "sgn";
    if (tag == "inl")
        return "sgn";
    if (tag == "ins")
        return "sgn";
    if (tag == "ise")
        return "sgn";
    if (tag == "isg")
        return "sgn";
    if (tag == "isr")
        return "sgn";
    if (tag == "jak")
        return "ms";
    if (tag == "jax")
        return "ms";
    if (tag == "jcs")
        return "sgn";
    if (tag == "jhs")
        return "sgn";
    if (tag == "jls")
        return "sgn";
    if (tag == "jos")
        return "sgn";
    if (tag == "jsl")
        return "sgn";
    if (tag == "jus")
        return "sgn";
    if (tag == "kgi")
        return "sgn";
    if (tag == "knn")
        return "kok";
    if (tag == "kvb")
        return "ms";
    if (tag == "kvk")
        return "sgn";
    if (tag == "kvr")
        return "ms";
    if (tag == "kxd")
        return "ms";
    if (tag == "lbs")
        return "sgn";
    if (tag == "lce")
        return "ms";
    if (tag == "lcf")
        return "ms";
    if (tag == "liw")
        return "ms";
    if (tag == "lls")
        return "sgn";
    if (tag == "lsg")
        return "sgn";
    if (tag == "lsl")
        return "sgn";
    if (tag == "lso")
        return "sgn";
    if (tag == "lsp")
        return "sgn";
    if (tag == "lst")
        return "sgn";
    if (tag == "lsy")
        return "sgn";
    if (tag == "ltg")
        return "lv";
    if (tag == "lvs")
        return "lv";
    if (tag == "lws")
        return "sgn";
    if (tag == "lzh")
        return "zh";
    if (tag == "max")
        return "ms";
    if (tag == "mdl")
        return "sgn";
    if (tag == "meo")
        return "ms";
    if (tag == "mfa")
        return "ms";
    if (tag == "mfb")
        return "ms";
    if (tag == "mfs")
        return "sgn";
    if (tag == "min")
        return "ms";
    if (tag == "mnp")
        return "zh";
    if (tag == "mqg")
        return "ms";
    if (tag == "mre")
        return "sgn";
    if (tag == "msd")
        return "sgn";
    if (tag == "msi")
        return "ms";
    if (tag == "msr")
        return "sgn";
    if (tag == "mui")
        return "ms";
    if (tag == "mzc")
        return "sgn";
    if (tag == "mzg")
        return "sgn";
    if (tag == "mzy")
        return "sgn";
    if (tag == "nan")
        return "zh";
    if (tag == "nbs")
        return "sgn";
    if (tag == "ncs")
        return "sgn";
    if (tag == "nsi")
        return "sgn";
    if (tag == "nsl")
        return "sgn";
    if (tag == "nsp")
        return "sgn";
    if (tag == "nsr")
        return "sgn";
    if (tag == "nzs")
        return "sgn";
    if (tag == "okl")
        return "sgn";
    if (tag == "orn")
        return "ms";
    if (tag == "ors")
        return "ms";
    if (tag == "pel")
        return "ms";
    if (tag == "pga")
        return "ar";
    if (tag == "pgz")
        return "sgn";
    if (tag == "pks")
        return "sgn";
    if (tag == "prl")
        return "sgn";
    if (tag == "prz")
        return "sgn";
    if (tag == "psc")
        return "sgn";
    if (tag == "psd")
        return "sgn";
    if (tag == "pse")
        return "ms";
    if (tag == "psg")
        return "sgn";
    if (tag == "psl")
        return "sgn";
    if (tag == "pso")
        return "sgn";
    if (tag == "psp")
        return "sgn";
    if (tag == "psr")
        return "sgn";
    if (tag == "pys")
        return "sgn";
    if (tag == "rms")
        return "sgn";
    if (tag == "rsi")
        return "sgn";
    if (tag == "rsl")
        return "sgn";
    if (tag == "rsm")
        return "sgn";
    if (tag == "sdl")
        return "sgn";
    if (tag == "sfb")
        return "sgn";
    if (tag == "sfs")
        return "sgn";
    if (tag == "sgg")
        return "sgn";
    if (tag == "sgx")
        return "sgn";
    if (tag == "shu")
        return "ar";
    if (tag == "slf")
        return "sgn";
    if (tag == "sls")
        return "sgn";
    if (tag == "sqk")
        return "sgn";
    if (tag == "sqs")
        return "sgn";
    if (tag == "ssh")
        return "ar";
    if (tag == "ssp")
        return "sgn";
    if (tag == "ssr")
        return "sgn";
    if (tag == "svk")
        return "sgn";
    if (tag == "swc")
        return "sw";
    if (tag == "swh")
        return "sw";
    if (tag == "swl")
        return "sgn";
    if (tag == "syy")
        return "sgn";
    if (tag == "szs")
        return "sgn";
    if (tag == "tmw")
        return "ms";
    if (tag == "tse")
        return "sgn";
    if (tag == "tsm")
        return "sgn";
    if (tag == "tsq")
        return "sgn";
    if (tag == "tss")
        return "sgn";
    if (tag == "tsy")
        return "sgn";
    if (tag == "tza")
        return "sgn";
    if (tag == "ugn")
        return "sgn";
    if (tag == "ugy")
        return "sgn";
    if (tag == "ukl")
        return "sgn";
    if (tag == "uks")
        return "sgn";
    if (tag == "urk")
        return "ms";
    if (tag == "uzn")
        return "uz";
    if (tag == "uzs")
        return "uz";
    if (tag == "vgt")
        return "sgn";
    if (tag == "vkk")
        return "ms";
    if (tag == "vkt")
        return "ms";
    if (tag == "vsi")
        return "sgn";
    if (tag == "vsl")
        return "sgn";
    if (tag == "vsv")
        return "sgn";
    if (tag == "wbs")
        return "sgn";
    if (tag == "wuu")
        return "zh";
    if (tag == "xki")
        return "sgn";
    if (tag == "xml")
        return "sgn";
    if (tag == "xmm")
        return "ms";
    if (tag == "xms")
        return "sgn";
    if (tag == "yds")
        return "sgn";
    if (tag == "ygs")
        return "sgn";
    if (tag == "yhs")
        return "sgn";
    if (tag == "ysl")
        return "sgn";
    if (tag == "yue")
        return "zh";
    if (tag == "zib")
        return "sgn";
    if (tag == "zlm")
        return "ms";
    if (tag == "zmi")
        return "ms";
    if (tag == "zsl")
        return "sgn";
    if (tag == "zsm")
        return "ms";
    return "";
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

static Intl::CanonicalizedLangunageTag canonicalizeLanguageTag(const std::string& locale)
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
        }
    }

    // Check for region.
    // region = 2ALPHA / 3DIGIT
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
        }
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
            return Intl::CanonicalizedLangunageTag();
        }
        subtags.insert(singleton);

        Intl::CanonicalizedLangunageTag::ExtensionSingleton singletonValue;
        singletonValue.key = possibleSingleton[0];

        ++currentIndex;

        if (singletonValue.key == 'u') {
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

            if (key.length() || value.size()) {
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
            return Intl::CanonicalizedLangunageTag();
        }
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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Type of element of locales must be String or Object");
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
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got Invalid locale");
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
        size_t pos = candidate->rfind(String::fromASCII("-"), candidate->length() - 1);
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
                    end = locale->find(String::fromASCII("-"), end);
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


StringMap* Intl::resolveLocale(ExecutionState& state, const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, StringMap* options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sec-resolvelocale

    // Let matcher be the value of options.[[localeMatcher]].
    auto iter = options->find(String::fromASCII("localeMatcher"));
    Value matcher;
    if (iter != options->end()) {
        matcher = iter->second;
    }

    Intl::IntlMatcherResult r;
    // If matcher is "lookup", then
    if (matcher.equalsTo(state, String::fromASCII("lookup"))) {
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
    StringMap* result = new StringMap;

    // Set result.[[dataLocale]] to foundLocale.
    result->insert(std::make_pair(String::fromASCII("dataLocale"), foundLocale));

    // Let supportedExtension be "-u".
    String* supportedExtension = String::fromASCII("-u");

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
        if (options->find(String::fromASCII(key)) != options->end()) {
            // Let optionsValue be options.[[<key>]].
            auto optionsValue = options->at(String::fromASCII(key));
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
        result->insert(std::make_pair(String::fromASCII(key), String::fromUTF8(value.data(), value.length())));

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
    result->insert(std::make_pair(String::fromASCII("locale"), foundLocale));
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
        matcher = options.asObject()->get(state, ObjectPropertyName(state, String::fromASCII("localeMatcher"))).value(state, options.asObject());
        // If matcher is not undefined, then
        if (!matcher.isUndefined()) {
            // Let matcher be ToString(matcher).
            matcher = matcher.toString(state);
            // If matcher is not "lookup" or "best fit", then throw a RangeError exception.
            if (!(matcher.asString()->equals("lookup") || matcher.asString()->equals("best fit"))) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid value on options.localeMatcher");
            }
        }
    }

    // If matcher is undefined or "best fit", then
    ValueVector subset;
    if (matcher.isUndefined() || matcher.equalsTo(state, String::fromASCII("best fit"))) {
        // Let subset be the result of calling the BestFitSupportedLocales abstract operation (defined in 9.2.7) with arguments availableLocales and requestedLocales.
        subset = bestfitSupportedLocales(state, availableLocales, requestedLocales);
    } else {
        // Let subset be the result of calling the LookupSupportedLocales abstract operation (defined in 9.2.6) with arguments availableLocales and requestedLocales.
        subset = lookupSupportedLocales(state, availableLocales, requestedLocales);
    }

    // For each named own property name P of subset,
    ArrayObject* result = new ArrayObject(state);

    result->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().length), ObjectPropertyDescriptor(Value(subset.size()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    for (size_t i = 0; i < subset.size(); i++) {
        String* P = subset[i].toString(state);
        // Let desc be the result of calling the [[GetOwnProperty]] internal method of subset with P.
        // Set desc.[[Writable]] to false.
        // Set desc.[[Configurable]] to false.
        // Call the [[DefineOwnProperty]] internal method of subset with P, desc, and true as arguments.
        result->defineOwnProperty(state, ObjectPropertyName(state, Value(i)), ObjectPropertyDescriptor(P, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    }

    return result;
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
            value = Value(value.toBoolean(state));
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
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got invalid value");
            }
        }
        // Return value.
        return value;
    } else {
        // Else return fallback.
        return fallback;
    }
}

std::vector<std::string> Intl::numberingSystemsForLocale(String* locale)
{
    static std::vector<std::string> availableNumberingSystems;

    UErrorCode status = U_ZERO_ERROR;
    if (availableNumberingSystems.size() == 0) {
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
    }

    status = U_ZERO_ERROR;
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
    const auto& availableLocales = state.context()->globalObject()->caseMappingAvailableLocales();
    // Let locale be BestAvailableLocale(availableLocales, noExtensionsLocale).
    String* locale = bestAvailableLocale(state, availableLocales, noExtensionsLocale);
    return locale;
}

} // namespace Escargot

#endif
