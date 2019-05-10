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

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function collect(value) {
    var primitive = y(value)
    if (primitive) return
    var index = z(value)
    if (index !== -1) { return }
    else {
        x.push({ })
        index = x.length - 1
        x[ index ].fv = value
    }

    var ps = Object.getOwnPropertyNames(value)
    for (var i = 0; i < ps.length; i++) {
        var p = ps[i]
        if (a(value, p)) {
            collect(value[p])
        }
    }
}

function y(value) {
    if (value === null)
        return "null"
    var vt = typeof value
    if (vt !== "function" && vt !== "object")
        return vt
}

function a(value, field) {
    try {
        value[field]
        return true
    } catch ( $ ) { }
}

function z(value) {
    for (var i = 0; i < x.length; i++) {
        if (value === x[ i ].fv)
             return i
    }
    return -1
}

var x = [ ];
collect(this);
