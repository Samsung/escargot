#!/usr/bin/env bash

# Copyright 2021-present Samsung Electronics Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

UCD_URL="https://www.unicode.org/Public/UCD/latest/ucd/UCD.zip"
WEBKIT_GENERATOR_URL="https://raw.githubusercontent.com/WebKit/WebKit/main/Source/JavaScriptCore/yarr/generateYarrUnicodePropertyTables.py"
WEBKIT_HASHER_URL="https://raw.githubusercontent.com/WebKit/WebKit/main/Source/JavaScriptCore/yarr/hasher.py"
PATTERN_FILE_PATH="./third_party/yarr/UnicodePatternTables.h"

echo -e "\nAutomated Unicode Pattern Table Tool for Escargot \n"

#Fetching data and scripts
echo -e "\n\nCurl UCD data"
curl $UCD_URL -O

echo -e "\n\nCurl Webkit pattern generator script"
curl $WEBKIT_GENERATOR_URL -o "generateYarrUnicodePropertyTables.py"

echo -e "\n\nCurl Hasher for Generator"
curl $WEBKIT_HASHER_URL -o "hasher.py"

echo -e "\n\nUnzip and move UCD files"
unzip ./UCD.zip -d UCD
mv ./UCD/emoji/emoji-data.txt ./UCD
mv ./UCD/extracted/* ./UCD

echo -e "\n\nGenerate new patter table"
chmod +x generateYarrUnicodePropertyTables.py
./generateYarrUnicodePropertyTables.py ./UCD $PATTERN_FILE_PATH


#Reformatting for Escargot Usage
echo -e "\nReformat for Escargot"
sed -i 's/makeUnique/std::make_unique/g' $PATTERN_FILE_PATH
grep -v "generated" $PATTERN_FILE_PATH > temp_file; mv temp_file $PATTERN_FILE_PATH

perl -0777 -i -p -e 's/,\n        CharacterClassWidths::HasBothBMPAndNonBMP\);/\);\n    characterClass->m_hasNonBMPCharacters = true;/g' $PATTERN_FILE_PATH
perl -0777 -i -p -e 's/,\n        CharacterClassWidths::HasBMPChars\);/\);\n    characterClass->m_hasNonBMPCharacters = false;/g' $PATTERN_FILE_PATH
perl -0777 -i -p -e 's/,\n        CharacterClassWidths::HasNonBMPChars\);/\);\n    characterClass->m_hasNonBMPCharacters = true;/g' $PATTERN_FILE_PATH
sed -i 's/\s*$//g' $PATTERN_FILE_PATH

#Cleanup
echo -e "\nCleaning up files"
rm -rf ./UCD
rm ./UCD.zip
rm generateYarrUnicodePropertyTables.py
rm hasher.py
