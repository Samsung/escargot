/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotYield__
#define __EscargotYield__

#if defined(COMPILER_MSVC)
extern "C" void _mm_pause();
#pragma intrinsic(_mm_pause)
#define YIELD_PROCESSOR _mm_pause()
#else
#if defined(CPU_X86)
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif defined(CPU_X86_64)
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif defined(CPU_ARM32)
#define YIELD_PROCESSOR __asm__ __volatile__("yield")
#elif defined(CPU_ARM64)
#define YIELD_PROCESSOR __asm__ __volatile__("yield")
#elif defined(CPU_RISCV32)
#define YIELD_PROCESSOR __asm__ __volatile__("fence")
#elif defined(CPU_RISCV64)
#define YIELD_PROCESSOR __asm__ __volatile__("fence")
#endif
#endif

#ifndef YIELD_PROCESSOR
#error "Unsupported arch"
#endif


#endif
