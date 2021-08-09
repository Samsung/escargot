/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

var o = null

class Class1 {
  c1_field1
    = 1
  c1_field2
  [
    "c1_field3"
  ] = 2
  c1_field4 = 3; c1_field5 = 4

  constructor () {
    this.c1_field1 = 5
  }

  static
  c1_sfield1
    = 1
  static c1_sfield2
  static
  [
    "c1_sfield3"
  ] = 2
  static c1_sfield4 = 3; static c1_sfield5 = 4
}

class Class2 extends Class1 {
  #c2_field1
    = 1
  #c2_field2
  #c2_field3 = 2
  #c2_field4 = 3; c2_field5 = 4

  constructor () {
    var v1 = 5
    super()
    this.#c2_field1 = v1
  }

  static
  #c2_sfield1
    = 1
  static #c2_sfield2
  static
  #c2_sfield3
    = 2
  static #c2_sfield4 = 3; static #c1_sfield5 = 4
}

o = new Class2
