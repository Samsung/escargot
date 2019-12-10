# Copyright (c) 2019-present Samsung Electronics Co., Ltd
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
#  USA

SET (ESCARGOT_CXXFLAGS -fno-rtti -march=armv7-a -mthumb)

SET (ESCARGOT_DEFINITIONS -DESCARGOT_32=1)

SET (ESCARGOT_CXXFLAGS_DEBUG)
SET (ESCARGOT_CXXFLAGS_RELEASE)

SET (ESCARGOT_LDFLAGS -lpthread -lrt -Wl,--gc-sections)

SET (ESCARGOT_LIBRARIES)
SET (ESCARGOT_INCDIRS)

SET (ESCARGOT_GCUTIL_CFLAGS)
