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

#include "Escargot.h"
#include "Context.h"
#include "ExecutionState.h"
#include "Value.h"
#include "Object.h"
#include "ArrayObject.h"
#include "Intl.h"

namespace Escargot {

static std::string grandfatheredLangTag(String* locale)
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
    tagMap["zh-guoyu"] = "cmn";
    tagMap["zh-hakka"] = "hak";
    tagMap["zh-min"] = "zh-min";
    tagMap["zh-min-nan"] = "nan";
    tagMap["zh-xiang"] = "hsn";

    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());
    std::transform(stdUTF8.begin(), stdUTF8.end(), stdUTF8.begin(), tolower);
    auto iter = tagMap.find(stdUTF8);

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

static std::string preferredLanguage(const std::string& language)
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
            privateuse.appendString(String::fromASCII(extPart.c_str()));
        }

        // Requires at least one production.
        if (!numExtParts)
            return std::string();
    }

    // Leftovers makes it invalid.
    if (currentIndex < numParts)
        return std::string();

    String* e = privateuse.finalize();
    auto estd = e->toUTF8StringData();
    return std::string(estd.data(), estd.length());
}

static std::string canonicalLangTag(const std::vector<std::string>& parts)
{
    // Follows the grammar at https://www.rfc-editor.org/rfc/bcp/bcp47.txt
    // langtag = language ["-" script] ["-" region] *("-" variant) *("-" extension) ["-" privateuse]

    size_t numParts = parts.size();
    // Check for language.
    // language = 2*3ALPHA ["-" extlang] / 4ALPHA / 5*8ALPHA
    size_t currentIndex = 0;
    std::string language = parts[currentIndex];
    unsigned languageLength = language.length();
    bool canHaveExtlang = languageLength >= 2 && languageLength <= 3;
    bool isValidLanguage = languageLength >= 2 && languageLength <= 8 && isAllSpecialCharacters(language, isASCIIAlpha);
    if (!isValidLanguage)
        return std::string();

    ++currentIndex;
    StringBuilder canonical;

    std::transform(language.begin(), language.end(), language.begin(), tolower);
    language = preferredLanguage(language);
    canonical.appendString(String::fromASCII(language.c_str()));

    // Check for extlang.
    // extlang = 3ALPHA *2("-" 3ALPHA)
    if (canHaveExtlang) {
        for (unsigned times = 0; times < 3 && currentIndex < numParts; ++times) {
            std::string extlang = parts[currentIndex];
            unsigned extlangLength = extlang.length();
            if (extlangLength == 3 && isAllSpecialCharacters(extlang, isASCIIAlpha)) {
                ++currentIndex;
                std::transform(extlang.begin(), extlang.end(), extlang.begin(), tolower);
                if (!times && intlPreferredExtlangTag(extlang) == language) {
                    canonical.clear();
                    canonical.appendString(String::fromASCII(extlang.c_str()));
                    continue;
                }
                canonical.appendString("-");
                canonical.appendString(String::fromASCII(extlang.c_str()));
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
            canonical.appendChar((char)toupper(script[0]));
            script = script.substr(1, 3);
            canonical.appendString(String::fromASCII(script.c_str()));
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
            auto preffered = preferredRegion(region);
            canonical.appendString(String::fromASCII(preffered.c_str()));
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
        if (!isValidVariant)
            break;

        // Cannot include duplicate subtags (case insensitive).
        std::string lowerVariant = variant;
        std::transform(lowerVariant.begin(), lowerVariant.end(), lowerVariant.begin(), tolower);
        auto iter = subtags.find(lowerVariant);
        if (iter != subtags.end()) {
            return std::string();
        }
        subtags.insert(lowerVariant);

        ++currentIndex;

        // Reordering variant subtags is not required in the spec.
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(lowerVariant.c_str()));
    }

    // Check for extension.
    // extension = singleton 1*("-" (2*8alphanum))
    // singleton = alphanum except x or X
    subtags.clear();
    std::vector<std::string> extensions;
    while (currentIndex < numParts) {
        std::string possibleSingleton = parts[currentIndex];
        unsigned singletonLength = possibleSingleton.length();
        bool isValidSingleton = (singletonLength == 1 && possibleSingleton != "x" && possibleSingleton != "X" && isASCIIAlphanumeric(possibleSingleton[0]));
        if (!isValidSingleton)
            break;

        // Cannot include duplicate singleton (case insensitive).
        std::string singleton = possibleSingleton;
        std::transform(possibleSingleton.begin(), possibleSingleton.end(), possibleSingleton.begin(), tolower);

        auto iter = subtags.find(singleton);
        if (iter != subtags.end()) {
            return std::string();
        }
        subtags.insert(singleton);

        ++currentIndex;
        int numExtParts = 0;
        StringBuilder extension;
        extension.appendString(String::fromASCII(singleton.c_str()));
        while (currentIndex < numParts) {
            std::string extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 2 && extPartLength <= 8 && isAllSpecialCharacters(extPart, isASCIIAlphanumeric));
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            extension.appendChar('-');
            std::transform(extPart.begin(), extPart.end(), extPart.begin(), tolower);
            extension.appendString(String::fromASCII(extPart.c_str()));
        }

        // Requires at least one production.
        if (!numExtParts)
            return std::string();

        String* e = extension.finalize();
        auto estd = e->toUTF8StringData();
        extensions.push_back(std::string(estd.data()));
    }

    // Add extensions to canonical sorted by singleton.
    std::sort(
        extensions.begin(),
        extensions.end(),
        [](const std::string& a, const std::string& b) -> bool {
            return a[0] < b[0];
        });
    size_t numExtenstions = extensions.size();
    for (size_t i = 0; i < numExtenstions; ++i) {
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(extensions[i].c_str()));
    }

    // Check for privateuse.
    if (currentIndex < numParts) {
        std::string privateuse = privateUseLangTag(parts, currentIndex);
        if (privateuse.length() == 0)
            return std::string();
        canonical.appendChar('-');
        canonical.appendString(String::fromASCII(privateuse.c_str()));
    }

    String* e = canonical.finalize();
    auto estd = e->toNonGCUTF8StringData();
    auto preferred = intlRedundantLanguageTag(estd);
    if (preferred != "") {
        return preferred;
    }
    return estd;
}

static String* isStructurallyValidLanguageTagAndcanonicalizeLanguageTag(ExecutionState& state, String* locale)
{
    std::string grandfather = grandfatheredLangTag(locale);
    if (grandfather.length() != 0)
        return String::fromUTF8(grandfather.c_str(), grandfather.length());

    auto utf8 = locale->toUTF8StringData();
    std::string stdUTF8(utf8.data(), utf8.length());

    std::vector<std::string> parts = split(stdUTF8, '-');

    if (parts.size()) {
        std::string langtag = canonicalLangTag(parts);
        if (langtag.length() != 0)
            return String::fromASCII(langtag.c_str());

        std::string privateuse = privateUseLangTag(parts, 0);
        if (privateuse.length() != 0)
            return String::fromASCII(privateuse.c_str());
    }

    return nullptr;
}

static String* icuLocaleToBCP47Tag(String* string)
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
    const char* locale = uloc_getDefault();
    String* localeString = String::fromUTF8(locale, strlen(locale));
    return icuLocaleToBCP47Tag(localeString);
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
    // If locales is a String value, then
    if (locales.isString()) {
        // Let locales be a new array created as if
        // by the expression new Array(locales) where Array is the standard built-in constructor with that name and locales is the value of locales.
        Value callArg[] = { locales };
        locales = Object::construct(state, state.context()->globalObject()->array(), 1, callArg);
    }
    // Let O be ToObject(locales).
    Object* O = locales.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    Value lenValue = Value(O->length(state));
    // Let len be ToUint32(lenValue).
    uint32_t len = lenValue.toUint32(state);
    // Let k be 0.
    // Repeat, while k < len
    uint32_t k = 0;
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectGetResult pkResult = O->get(state, ObjectPropertyName(state, Value(k)));
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        // If kPresent is true, then
        if (pkResult.hasValue()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = pkResult.value(state, O);
            // If the type of kValue is not String or Object, then throw a TypeError exception.
            if (!kValue.isString() && !kValue.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Type of element of locales must be String or Object");
            }
            // Let tag be ToString(kValue).
            String* tag = kValue.toString(state);
            // If the result of calling the abstract operation IsStructurallyValidLanguageTag (defined in 6.2.2), passing tag as the argument,
            // is false, then throw a RangeError exception.
            // Let tag be the result of calling the abstract operation CanonicalizeLanguageTag (defined in 6.2.3), passing tag as the argument.
            // If tag is not an element of seen, then append tag as the last element of seen.
            tag = isStructurallyValidLanguageTagAndcanonicalizeLanguageTag(state, tag);
            if (!tag) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "got Invalid locale");
            }
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
        }
        // Increase k by 1.
        k++;
    }

    // Return seen.
    return seen;
}

static String* bestAvailableLocale(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, Value locale)
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

static Intl::IntlMatcherResult lookupMatcher(ExecutionState& state, const Vector<String*, gc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
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

            size_t extensionIndex = locale->find(String::fromASCII("-u-"));
            RELEASE_ASSERT(extensionIndex != SIZE_MAX);

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
    } else {
        // Set result.[[locale]] to the value returned by the DefaultLocale abstract operation (defined in 6.2.4).
        result.locale = defaultLocale(state);
    }

    return result;
}

static Intl::IntlMatcherResult bestFitMatcher(ExecutionState& state, const Vector<String*, gc_allocator<String*>> availableLocales, const ValueVector& requestedLocales)
{
    // TODO
    return lookupMatcher(state, availableLocales, requestedLocales);
}

StringMap* Intl::resolveLocale(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, StringMap* options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, LocaleDataImplFunction localeData)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.5

    // Let matcher be the value of options.[[localeMatcher]].
    auto iter = options->find(String::fromASCII("localeMatcher"));
    Value matcher;
    if (iter != options->end()) {
        matcher = iter->second;
    }

    Intl::IntlMatcherResult r;
    // If matcher is "lookup", then
    if (matcher.equalsTo(state, String::fromASCII("lookup"))) {
        // Let r be the result of calling the LookupMatcher abstract operation (defined in 9.2.3) with arguments availableLocales and requestedLocales.
        r = lookupMatcher(state, availableLocales, requestedLocales);
    } else {
        // Else
        // Let r be the result of calling the BestFitMatcher abstract operation (defined in 9.2.4) with arguments availableLocales and requestedLocales.
        r = bestFitMatcher(state, availableLocales, requestedLocales);
    }

    // Let foundLocale be the value of r.[[locale]].
    String* foundLocale = r.locale;
    std::vector<std::string> extensionSubtags;
    if (r.extension->length()) {
        // Let extension be the value of r.[[extension]].
        String* extension = r.extension;
        // Let extensionIndex be the value of r.[[extensionIndex]].
        size_t extensionIndex = r.extensionIndex;
        // Let split be the standard built-in function object defined in ES5, 15.5.4.14.
        // Let extensionSubtags be the result of calling the [[Call]] internal method of split with extension as the this value and an argument list containing the single item "-".
        // Let extensionSubtagsLength be the result of calling the [[Get]] internal method of extensionSubtags with argument "length".
        auto utf8 = extension->toUTF8StringData();
        std::string stdUTF8(utf8.data(), utf8.length());
        extensionSubtags = split(stdUTF8, '-');
    }

    // Let result be a new Record.
    StringMap* result = new StringMap;

    // Set result.[[dataLocale]] to foundLocale.
    result->insert(std::make_pair(String::fromASCII("dataLocale"), foundLocale));

    // Let supportedExtension be "-u".
    String* supportedExtension = String::fromASCII("-u");

    // Let i be 0.
    size_t i = 0;
    // Let len be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument "length".
    size_t len = relevantExtensionKeyCount;

    // Repeat while i < len:
    while (i < len) {
        // Let key be the result of calling the [[Get]] internal method of relevantExtensionKeys with argument ToString(i).
        const char* key = relevantExtensionKeys[i];
        // Let foundLocaleData be the result of calling the [[Get]] internal method of localeData with the argument foundLocale.
        // Let keyLocaleData be the result of calling the [[Get]] internal method of foundLocaleData with the argument key.
        // Let value be the result of calling the [[Get]] internal method of keyLocaleData with argument "0".
        std::vector<std::string> keyLocaleData = localeData(foundLocale, i);

        // Let supportedExtensionAddition be "".
        String* supportedExtensionAddition = String::emptyString;

        std::string value = keyLocaleData[0];
        // If extensionSubtags is not undefined, then
        if (extensionSubtags.size()) {
            // Let keyPos be Call(%ArrayProto_indexOf%, extensionSubtags, «key») .
            size_t keyPos = SIZE_MAX;
            for (size_t i = 0; i < extensionSubtags.size(); i++) {
                if (extensionSubtags[i] == key) {
                    keyPos = i;
                    break;
                }
            }
            // If keyPos != -1, then
            if (keyPos != SIZE_MAX) {
                // If keyPos + 1 < extensionSubtagsLength and the length of the result of Get(extensionSubtags, ToString(keyPos +1)) is greater than 2, then
                if (keyPos + 1 < extensionSubtags.size() && extensionSubtags[keyPos + 1].length() > 2) {
                    const std::string& requestedValue = extensionSubtags[keyPos + 1];

                    bool contains = false;
                    for (size_t k = 0; k < keyLocaleData.size(); k++) {
                        if (keyLocaleData[k] == requestedValue) {
                            contains = true;
                            break;
                        }
                    }
                    if (contains) {
                        value = requestedValue;
                        std::string s = std::string("-") + key + std::string("-") + value;
                        supportedExtensionAddition = String::fromUTF8(s.data(), s.length());
                    }
                } else if (std::find(keyLocaleData.begin(), keyLocaleData.end(), std::string("true")) != keyLocaleData.end()) {
                    // Else, if the result of Call(%StringProto_includes%, keyLocaleData, «"true"») is true, then
                    value = "true";
                }
            }
        }

        // If options has a field [[<key>]], then
        if (options->find(String::fromASCII(key)) != options->end()) {
            // Let optionsValue be the value of options.[[<key>]].
            std::string optionsValue = std::string(options->at(String::fromASCII(key))->toUTF8StringData().data());

            // If the result of calling the [[Call]] internal method of indexOf with keyLocaleData as the this value
            // and an argument list containing the single item optionsValue is not -1, then
            // If optionsValue is not equal to value, then
            if (std::find(keyLocaleData.begin(), keyLocaleData.end(), optionsValue) != keyLocaleData.end() && optionsValue != value) {
                // Let value be optionsValue.
                // Let supportedExtensionAddition be "".
                value = optionsValue;
                supportedExtensionAddition = String::emptyString;
            }
        }
        // Set result.[[<key>]] to value.
        result->insert(std::make_pair(String::fromASCII(key), String::fromUTF8(value.data(), value.length())));

        // Append supportedExtensionAddition to supportedExtension.
        StringBuilder sb;
        sb.appendString(supportedExtension);
        sb.appendString(supportedExtensionAddition);
        supportedExtension = sb.finalize();

        // Increase i by 1.
        i++;
    }

    // If the length of supportedExtension is greater than 2, then
    if (supportedExtension->length() > 2) {
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
    // Set result.[[locale]] to foundLocale.
    result->insert(std::make_pair(String::fromASCII("locale"), foundLocale));
    // Return result.
    return result;
}


static ValueVector lookupSupportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
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

static ValueVector bestfitSupportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.7
    // TODO
    return lookupSupportedLocales(state, availableLocales, requestedLocales);
}

Value Intl::supportedLocales(ExecutionState& state, const Vector<String*, gc_allocator<String*>>& availableLocales, const ValueVector& requestedLocales, Value options)
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

Value Intl::getOption(ExecutionState& state, Object* options, Value property, String* type, ValueVector values, const Value& fallback)
{
    // http://www.ecma-international.org/ecma-402/1.0/index.html#sec-9.2.9
    // Let value be the result of calling the [[Get]] internal method of options with argument property.
    Value value = options->get(state, ObjectPropertyName(state, property)).value(state, options);
    // If value is not undefined, then
    if (!value.isUndefined()) {
        // Assert: type is "boolean" or "string".
        // If type is "boolean", then let value be ToBoolean(value).
        if (type->equals("boolean")) {
            value = Value(value.toBoolean(state));
        }
        // If type is "string", then let value be ToString(value).
        if (type->equals("string")) {
            value = Value(value.toString(state));
        }
        // If values is not undefined, then
        if (values.size()) {
            // If values does not contain an element equal to value, then throw a RangeError exception.
            bool contains = false;
            for (size_t i = 0; i < values.size(); i++) {
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

} // namespace Escargot

#endif
