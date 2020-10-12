Tiny Big Float library
----------------------
Copyright (c) 2017-2020 Fabrice Bellard

LibBF is a small library to handle arbitrary precision binary or
decimal floating point numbers. Its compiled size is about 90 KB of
x86 code and has no dependency on other libraries. It is not the
fastest library nor the smallest but it tries to be simple while using
asymptotically optimal algorithms. The basic arithmetic operations
have a near linear running time.

The TinyPI example computes billions of digits of Pi using the
Chudnovsky formula.

1) Features
-----------

- Arbitrary precision floating point numbers in base 2 using the IEEE
  754 semantics (including subnormal numbers, infinities and
  NaN).
- All operations are exactly rounded using the 5 IEEE 754 rounding
  modes (round to nearest with ties to even or away from zero, round
  to zero, -/+ infinity). The additional non-deterministic faithful
  rounding mode is supported when a lower or deterministic running
  time is necessary.
- Stateless API (each function takes as input the rounding mode,
  mantissa and exponent precisions in bits and return the IEEE status
  flags).
- The basic arithmetic operations (addition, subtraction,
  multiplication, division, square root) have a near linear running
  time.
- Multiplication using a SIMD optimized Number Theoretic Transform.
- Exactly rounded floating point input and output in any base between
  2 and 36 with near linear runnning time. Floating point output can
  select the smallest amount of digits to get the required precision.
- Transcendental functions are supported (exp, log, pow, sin, cos, tan,
  asin, acos, atan, atan2).
- Operations on arbitrarily large integers are supported by using a
  special "infinite" precision. Integer division with remainder and
  logical operations (assuming two complement binary representation)
  are implemented.
- Arbitrary precision floating point numbers in base 10 corresponding
  to the IEEE 754 2008 semantics with the limitation that the mantissa
  is always normalized. The basic arithmetic operations, output and
  input are supported with a quadratic running time.
- Easy to embed: a few C files need to be copied, the memory allocator
  can be redefined, the memory allocation failures are tested.
- MIT license.

2) Compilation
--------------

Edit the top of the Makefile to select the build options. By default,
the MPFR library is used to compile the test tools (bftest and
bfbench) but it is not needed to build libbf. The included SoftFP code
(softfp* files) is only used by the bftest test tool.

TinyPI example: the "tinypi" executable uses the portable code. The
"tinypi-avx2" executable uses the AVX2 implementation. An x86 CPU of
at least the Intel Haswell generation is necessary for AVX2.

3) Design principles
--------------------

- Base 2 and IEEE 754 semantics were chosen so that it is possible to
  get good performance and to compare the results with other libraries
  or hardware implementations. Moreover, base 2 arbitrary precision is
  easier to analyse and implement.

- The support of subnormal numbers and of a configurable number of
  bits for the exponent allows the exact emulation of IEEE 754
  floating hardware.

- The stateless API ensures that there is no global state to save and
  restore between operations. The rounding mode, subnormal flag and
  number of exponent bits are ored to a single "flags" parameter to
  limit the verbosity of the API. The number of exponent bits 'n' is
  specified as '(M-n)' where M is the maximum number of exponent bits
  so that '0' always indicates the maximum number of exponent bits.

- All the IEEE 754 status flags are returned by each operation. The
  user can easily or them when necessary.

- Unlike other libraries (such as MPFR [2]), the numbers have no
  attached precision. The general rule is that each operation is
  internally computed with infinite precision and then rounded with
  the precision and rounding mode specified for the operation.

- In many computations it is necessary to use arbitrarily large
  integers. LibBF support them without adding another number type by
  providing a special "infinite" precision. There is a small overhead
  of course because they are manipulated as floating point numbers but
  there is no cost to convert between floating point numbers and
  integers.

- The faithful rounding mode (i.e. the result is rounded to - or
  +infinity non deterministically) is supported for all operations. It
  usually gives a faster and deterministic running time. The
  transcendental functions, inverse or inverse square root are
  internally implemented to give a faithful rounding. When a
  non-faithful rounding is requested by the user, the Ziv rounding
  algorithm is invoked.

4) Implementation notes
-----------------------

- The code was tested on a 64 bit x86 CPU. It should be portable to
  other CPUs. The portable version handles numbers with up to 4*10^16
  digits. The AVX2 version handles numbers with up to 8*10^12 digits.

- 32 bits: the code compiles on 32 bit architectures but it is not
  designed to be efficient nor scalable in this case. The size of the
  numbers is limited to about 10 million digits.

- The Number Theoretic Transform is not the fastest algorithm for
  small to medium numbers (i.e. a few million digits), but it gets
  better when the size of the numbers grows. There is no round-off
  errors as with Fast Fourier Transform, the memory usage is much
  smaller and it is potentially easier to parallelize. This code
  contains an original SIMD (AVX2 on x86) implementation using 64 bit
  floating point numbers. It relies on the fact that the fused
  multiply accumulate (FMA) operation gives access to the full
  precision of the product of two 64 bit floating point numbers. The
  portable code relies on the fact that the C compiler supports a
  double word integer type (i.e. 128 bit integers on 64 bit). The
  modulo operations were replaced with multiplications which are
  usually faster.

- Base conversion: the algorithm is not the fastest one but it is
  simple and still gives a near linear running time.

- This library reuses some ideas from TachusPI (
  http://bellard.org/pi/pi2700e9/tpi.html ) . It is about 4 times
  slower to compute Pi but is much smaller and simpler.

5) Known limitations
--------------------

- In some operations (such as the transcendental ones), there is no
  rigourous proof of the rounding error. We expect to improve it by
  reusing ideas from the MPFR algorithms. Some unlikely
  overflow/underflow cases are also not handled in exp or pow.

- The transcendental operations are not speed optimized and do not use
  an asymptotically optimal algorithm (the running time is in
  O(n^(1/2)*M(n)) where M(n) is the time to multiply two n bit
  numbers). A possible solution would be to implement a binary
  splitting algorithm for exp and sin/cos (see [1]) and to use a
  Newton based inversion to get log and atan.

- Memory allocation errors are not always correctly reported for the
  transcendental operations.

6) References
-------------

[1] Modern Computer Arithmetic, Richard Brent and Paul Zimmermann,
Cambridge University Press, 2010
(https://members.loria.fr/PZimmermann/mca/pub226.html).

[2] The GNU MPFR Library (http://www.mpfr.org/)
