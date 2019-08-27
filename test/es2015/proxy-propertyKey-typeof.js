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

// dummy call, to invoke proxy's get
function dummyCall() {
}

// check if string conversion is done
var stringHandler = {
    get: function(obj, prop) {
        assert(typeof prop == "string")
    }
};
var proxyStringExample = new Proxy({}, stringHandler);
var proxyArray = new Array(0);
dummyCall(proxyArray[1])


// check if symbol type is handled correctly
var symbolHandler = {
    get: function(obj, prop) {
        assert(typeof prop == "symbol")
    }
};
var proxySymbolExample = new Proxy({}, symbolHandler);
const SymbolKey = Symbol();

proxySymbolExample[SymbolKey] = 123;
dummyCall(proxySymbolExample[SymbolKey])
