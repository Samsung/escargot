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

function f () {
  let i = 2;
  if (i < 10) {
    if (i > 1) {
      i++;
    }
  }

  if (i > 10) {
    i++;
  } else {
    i++;
  }

  while (i > 0) {
    i--;
  }

  do {
    i++
  } while (i < 2);

  try {
    throw i;
  } catch (e) {
    i--;
  }

  for (let i = 0; i < 1; i++) {
    i++;
  }

  for (let i of [1]) {
    i++;
  }

  for (let i in { a: 8 }) {
    i++;
  }
}

f();
