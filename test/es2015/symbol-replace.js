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


function Replace(find, newString) {
	this.find = find;
	this.newString = newString;
}

Replace.prototype[Symbol.replace] = function(string) {
	return string.replace(this.find, this.newString);
}
function assertEquals(string, expectedResult)
{
	assert(string == expectedResult)
}



var string = 'foobar';
assertEquals(string.replace(new Replace('foo', 'bar')) , 'barbar')
assertEquals(string.replace(new Replace('foobar', 'barfoo')) , 'barfoo')
assertEquals(string.replace(new Replace('', 'bar')) , 'barfoobar')

string = '';
assertEquals(string.replace(new Replace('foo', 'bar')) , '')
assertEquals(string.replace(new Replace('', 'bar')) , 'bar')

var re = /-/g
string = '2018-01-01'
var replace = '.'
var newStr = re[Symbol.replace](string,replace)
assertEquals(newStr , '2018.01.01')

re = /apples/gi
string = 'Apples and apples'
replace = 'orange'
newStr = re[Symbol.replace](string,replace)
assertEquals(newStr , 'orange and orange')

re = /(\w+)\s(\w+)/
string = 'John Smith'
replace = '$2, $1'
newStr = re[Symbol.replace](string,replace)
assertEquals(newStr , 'Smith, John')
