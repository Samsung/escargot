/* Copyright 2019-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function Search(value) {
   this.value = value;
 }
 Search.prototype[Symbol.search]= function(string) {
return string.indexOf(this.value);
//return string.;
 }


 function assertPositionSymbol(string,find, expected) {
     assert(string.search(new Search(find)) == expected);
 }

 function assertPosition(string,find, expected) {
     assert(string.search(find) == expected);
 }

var string = 'foobar';

assertPositionSymbol(string,'f',0);
assertPositionSymbol(string,'o',1);
assertPositionSymbol(string,'b',3);
assertPositionSymbol(string,'a',4);
assertPositionSymbol(string,'r',5);
assertPositionSymbol(string,'z',-1);

assertPosition(string,'f',0);
assertPosition(string,'o',1);
assertPosition(string,'b',3);
assertPosition(string,'a',4);
assertPosition(string,'r',5);
assertPosition(string,'z',-1);

var string2 = ''

assertPositionSymbol(string2,'',0)
assertPositionSymbol(string2,'a',-1)

assertPosition(string2,'',0)
assertPosition(string2,'a',-1)
