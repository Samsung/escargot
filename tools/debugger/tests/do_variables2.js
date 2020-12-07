/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

{
  let a;
  let b = null;
  let c = true;
  let d = false;
  let e = 3.14;
  let f = "str";
  let g = Symbol("sym");
  let h = {};
  let i = [];
  let j = function() {};
  let l = 900719925474099123n; // Number.MAX_SAFE_INTEGER x 100 + 23

  j();

  let k; // unaccessible (because it has not been initialized yet)
}
