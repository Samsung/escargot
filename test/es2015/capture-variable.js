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

print = assert

eval("var dd")
var global
function test() {
    global;
    Math
    print(isFunctionAllocatedOnStack(test))
}

test();

new Function("print(isFunctionAllocatedOnStack(arguments.callee))")()


print(!isFunctionAllocatedOnStack(heap))
function heap() {
    var local;
    function foo() {
        global;
    }

    print(isFunctionAllocatedOnStack(nonheap2))
    function nonheap2() {
        print(isFunctionAllocatedOnStack(foo3))
        function foo3() {
             local;
        }
        return foo3;
    }

    return nonheap2
}


heap()()()

print(!isFunctionAllocatedOnStack(heap2))
function heap2() {
    var local;
    print(isFunctionAllocatedOnStack(nonheap2))
    function nonheap2() {
        print(isFunctionAllocatedOnStack(foo3))
        function foo3() {
             local;
        }
        foo3()
    }

    nonheap2()
}

heap2()


print(isFunctionAllocatedOnStack(test5))
function test5() {
    var local;
    dd; Math;
}


let globallet;
print(isFunctionAllocatedOnStack(test6))
function test6() {
    globallet;
}

print(!isFunctionAllocatedOnStack(test7))
print(isBlockAllocatedOnStack(test7, 0))
print(!isBlockAllocatedOnStack(test7, 2))
function test7() {
{ //1
    globallet;
}
{
    //2
    let local;
    print(isFunctionAllocatedOnStack(sub))
    function sub() {
        print(isFunctionAllocatedOnStack(subsub))
        print(isBlockAllocatedOnStack(subsub, 0))
        print(isBlockAllocatedOnStack(subsub, 1))
        function subsub() {
            local;
            {
                let l;
            }
        }
        return subsub
    }
}
    return sub;
}

test7()()()
