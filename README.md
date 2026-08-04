# Escargot

[![License](https://img.shields.io/badge/License-LGPL%20v2.1-blue.svg)](LICENSE)
[![GitHub release (latestSemVer)](https://img.shields.io/github/v/release/Samsung/escargot)](https://github.com/Samsung/escargot/releases)
[![Actions Status](https://github.com/Samsung/escargot/workflows/ES-Actions/badge.svg)](https://github.com/Samsung/escargot/actions/workflows/es-actions.yml)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21647/badge.svg)](https://scan.coverity.com/projects/samsung-escargot)
[![codecov](https://codecov.io/gh/Samsung/escargot/branch/master/graph/badge.svg?token=DX8CN6E7A8)](https://codecov.io/gh/Samsung/escargot)


Escargot is a lightweight JavaScript engine developed by [Samsung](https://github.com/Samsung), designed specifically for resource-constrained environments. It is optimized for performance and low memory usage, making it ideal for use in embedded systems, IoT devices, and other applications where resources are limited.

Key features of Escargot include:
* **ECMAScript Compliance**: Escargot supports a significant portion of the latest ECMAScript version ([ECMAScript 2025](https://262.ecma-international.org/16.0/)), ensuring compatibility with modern JavaScript standards while maintaining a lightweight footprint.
* **Node-API (N-API) Compliance**: Escargot provides a 100% compliant Node-API (N-API) layer supporting ABI stability versions up to v10. This allows developers to load pre-compiled Node.js C++ addons directly and embed the engine using standard C-style hosting APIs. See [`docs/n-api.md`](docs/n-api.md) for details and a complete embedding example.
* **Memory Efficiency**: The engine is designed with memory constraints in mind, making it suitable for devices with limited RAM and storage.
* **Performance Optimization**: Escargot implements various optimization techniques to ensure fast execution of JavaScript code, even on low-power devices.
* **Extensibility**: The engine can be customized and extended to meet the specific needs of different applications, providing flexibility for developers.

Escargot is an open-source project that allows developers to contribute to its development or use it in their own projects, while also powering several services in Samsung products. The engine's design prioritizes simplicity and efficiency, making it an excellent choice for developers working in embedded or resource-limited environments.


## Contents 📋
* [Building](#Building-)
  * [Linux](#Linux)
  * [macOS](#macOS)
  * [Android](#Android)
  * [Windows](#Windows)
  * [Bare-metal / RTOS](#Bare-metal--RTOS)
* [Testing](#Testing-)
* [Contributing](#Contributing-)
* [Research Papers](#Research-Papers-)
* [License](#License-)

## Building 🛠️

### Supported Platforms and Architectures
| **OS** | **Architecture** |
|-|-|
| **Linux(Ubuntu)** | x86/x64/arm/aarch64 |
| macOS | x64/aarch64 |
| Windows | Win32/x64 |
| Android | x86/x64/arm/aarch64 |
| Bare-metal / RTOS | arm/aarch64/x86/x64/riscv64 |

### Build Options

The following build options are supported when generating build rules using cmake.

| **Option** | **Description** | **Flag** | **Value** | **Default** |
|-|-|-|-|-|
| **ARCH** | Choose target architecture | -DESCARGOT_ARCH | x64/x86/arm/aarch64 | |
| **ESCARGOT_BUILD_SHARED_LIBS** | Build shared library | -DESCARGOT_BUILD_SHARED_LIBS | ON/OFF | OFF |
| **ENABLE_SHELL** | Build the Escargot shell (canonical name: -DESCARGOT_ENABLE_SHELL) | -DENABLE_SHELL | ON/OFF | ON, except OFF when ESCARGOT_NAPI is ON |
| **ESCARGOT_BUILD_CCTEST** | Build the C++ tests | -DESCARGOT_BUILD_CCTEST | ON/OFF | OFF |
| **LIBICU** | Include libicu library | -DESCARGOT_LIBICU_SUPPORT | ON/OFF | ON, except OFF on bare-metal |
| **WASM** | Enable WebAssembly support | -DESCARGOT_WASM | ON/OFF | OFF |
| **CODE_CACHE** | Enable code cache | -DESCARGOT_CODE_CACHE | ON/OFF | OFF |
| **TCO** | Enable tail call optimization | -DESCARGOT_TCO | ON/OFF | OFF |
| **THREADING** | Enable threading features (e.g. Atomics, SharedArrayBuffer) | -DESCARGOT_THREADING | ON/OFF | ON, except OFF on bare-metal |
| **TLS_ADDRESS_OFFSET** | Enable thread local storge access optimization(offset) | -DESCARGOT_TLS_ACCESS_BY_ADDRESS | ON/OFF | ON when THREADING is ON, except on Android/Windows/macOS/bare-metal (assumes ELF/glibc-style TLS) |
| **TLS_PTHREAD_KEY** | Enable thread local storge access optimization(pthread_key) | -DESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY | ON/OFF | ON when THREADING is ON and host is Android |
| **TEMPORAL** | Enable Temporal support (requires ICU) | -DESCARGOT_TEMPORAL | ON/OFF | ON when LIBICU is ON, otherwise OFF |
| **SHADOWREALM** | Enable ShadowRealm support | -DESCARGOT_SHADOWREALM | ON/OFF | OFF |
| **SMALL_CONFIG** | Enable aggressive memory optimizations for tiny devices | -DESCARGOT_SMALL_CONFIG | ON/OFF | OFF |
| **EXPORT_ALL** | Export all symbols instead of the default curated public API | -DESCARGOT_EXPORT_ALL | ON/OFF | OFF |
| **TEST** | Enable additional features used only for testing | -DESCARGOT_TEST | ON/OFF | OFF |
| **DEBUGGER** | Enable Debug server | -DESCARGOT_DEBUGGER | ON/OFF | OFF |
| **NAPI** | Enable Node-API (N-API) support and C-style hosting APIs | -DESCARGOT_NAPI | ON/OFF | OFF |

<details>
<summary>Advanced / developer-only options (profiling, sanitizers, internal knobs)</summary>

| **Option** | **Description** | **Flag** | **Value** | **Default** |
|-|-|-|-|-|
| **ESCARGOT_ASAN** | Build with AddressSanitizer | -DESCARGOT_ASAN | ON/OFF | OFF |
| **ESCARGOT_COVERAGE** | Build with gcov/Codecov instrumentation | -DESCARGOT_COVERAGE | ON/OFF | OFF |
| **ESCARGOT_DEPLOY** | Build for deployment (set up RPATH for a bundled ICU) | -DESCARGOT_DEPLOY | ON/OFF | OFF |
| **ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN** | Load libicu at runtime via dlopen() instead of linking directly | -DESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN | ON/OFF | ON, except OFF on macOS (dlopen-loaded ICU doesn't work correctly there) |
| **ESCARGOT_USE_EXTENDED_API** | Enable the extended C++ API (FunctionTemplateRef, etc.) | -DESCARGOT_USE_EXTENDED_API | ON/OFF | ON when NAPI is ON, otherwise OFF |
| **ESCARGOT_USE_CUSTOM_LOGGING** | Use a custom logging backend instead of the host's native log (e.g. dlog on Tizen) | -DESCARGOT_USE_CUSTOM_LOGGING | ON/OFF | OFF |
| **ESCARGOT_TCO_DEBUG** | Enable extra tail-call-optimization debug checks (debug builds only, requires ESCARGOT_TCO) | -DESCARGOT_TCO_DEBUG | ON/OFF | OFF |
| **ESCARGOT_PROFILE_BDWGC** | Enable bdwgc (Boehm GC) profiling | -DESCARGOT_PROFILE_BDWGC | ON/OFF | OFF |
| **ESCARGOT_MEM_STATS** | Enable memory usage statistics | -DESCARGOT_MEM_STATS | ON/OFF | OFF |
| **ESCARGOT_VALGRIND** | Build with Valgrind annotations | -DESCARGOT_VALGRIND | ON/OFF | OFF |
| **ESCARGOT_GOOGLE_PERF** | Build with gperftools (Google Performance Tools) profiling | -DESCARGOT_GOOGLE_PERF | ON/OFF | OFF |
| **ESCARGOT_BUILD_64BIT_FORCE_LARGE** | On 64-bit targets, force full 64-bit pointers instead of 32-bit-in-64-bit compression | -DESCARGOT_BUILD_64BIT_FORCE_LARGE | ON/OFF | ON |

</details>

### Linux

General build prerequisites:
```sh
sudo apt-get install autoconf automake cmake libtool libicu-dev ninja-build pkg-config
```

Prerequisites for x86-64-to-x86 compilation:
```sh
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386
```

Build Escargot:
```sh
git submodule update --init third_party # update submodules
cmake -DENABLE_SHELL=ON -GNinja
ninja
```

> CMake 4.0+ removed support for `cmake_minimum_required(VERSION < 3.5)` and will
> hard-error on some vendored third-party dependencies (e.g. `third_party/wasm/wabt`)
> that still declare an older minimum. If your `cmake --version` is 4.0 or newer, add
> `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the `cmake` command above.

### macOS

General build prerequisites:
```sh
brew install autoconf automake cmake icu4c libtool ninja pkg-config

# add icu path to pkg_config_path (x64)
export PKG_CONFIG_PATH="/usr/local/opt/icu4c/lib/pkgconfig:$PKG_CONFIG_PATH"
# add icu path to pkg_config_path (arm64)
export PKG_CONFIG_PATH="/opt/homebrew/opt/icu4c/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Build Escargot:
```sh
git submodule update --init third_party # update submodules
cmake -DENABLE_SHELL=ON -GNinja
ninja
```

> Same CMake 4.0+ note as the Linux section above applies here too --
> add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` if needed.

### Android

Build prerequisites on Ubuntu:
```sh
sudo apt install openjdk-17-jdk # require java 17
```

Build Escargot using gradle:
```sh
git submodule update --init third_party # update submodules
export ANDROID_SDK_ROOT=.... # set your android SDK root first
cd build/android/
./gradlew bundleReleaseAar # build escargot AAR
./gradlew bundleHostJar # bundle jar for host
./gradlew javadocJar # create java doc
./gradlew sourcesJar # create sources jar

./gradlew assembleDebug # build debug test shell
./gradlew :escargot:connectedDebugAndroidTest # run escargot-jni tests on android device
./gradlew :escargot:testDebugUnitTest # run escargot-jni tests on host
```

### Bare-metal / RTOS

Escargot runs on bare-metal and RTOS targets with no OS underneath
(no pthreads, no `mmap`, no filesystem). Specifying a bare-metal/RTOS target via `CMAKE_SYSTEM_NAME` (such as `Generic`, `NuttX`, `FreeRTOS`) automatically
configures the engine side of this (`-DOS_BAREMETAL=1` and friends,
ICU/threading defaulted off):

```sh
cmake -DCMAKE_SYSTEM_NAME=Generic -DESCARGOT_ARCH=arm ... /path/to/escargot
```

A full port additionally needs its own small CMake project for BDWGC
(`third_party/GCutil`) and a `PlatformRef` implementation providing the
RTOS's task stack bounds and tick source. See
[`docs/porting/RTOS_PORTING_GUIDE.md`](docs/porting/RTOS_PORTING_GUIDE.md)
for the full checklist and code contract, and
[`samples/rtos/freertos/`](samples/rtos/freertos) for a complete, working
in-tree sample (FreeRTOS / Cortex-M55, QEMU `mps3-an547`) — cross-compiled
and boot-tested under QEMU by the `RTOS-FreeRTOS` CI job
(`.github/workflows/rtos-freertos.yml`) whenever engine or sample sources
change.

A second reference port, NuttX / Cortex-M55 (same QEMU target), is also
in-tree: [`samples/rtos/nuttx/`](samples/rtos/nuttx) has the escargot NSH
app (`interpreters-escargot/`, meant to be dropped into your own
NuttX+apps checkout's `apps/interpreters/`) and the out-of-tree CMake
project that builds the engine for it (`escargot-lib-cmake/`). Unlike
FreeRTOS-Kernel, NuttX itself isn't vendored as a submodule here (a NuttX
app fundamentally needs a full NuttX+apps source tree, not a standalone
library dependency) — CI-verified instead by the `RTOS-NuttX` job
(`.github/workflows/rtos-nuttx.yml`), which checks out NuttX + its `apps`
monorepo at pinned commits (cached across runs) and boot-tests the same
way. Both ports' shared contract and checklist are in
[`docs/porting/RTOS_PORTING_GUIDE.md`](docs/porting/RTOS_PORTING_GUIDE.md).

### Windows

Install VS2022 with cmake and ninja.
Open [ x86 Native Tools Command Prompt for VS 2022 | x64 Native Tools Command Prompt for VS 2022 ]

```sh
git submodule update --init third_party # update submodules

CMake -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=[ Windows | WindowsStore ] -DCMAKE_SYSTEM_VERSION:STRING="10.0"  -DCMAKE_SYSTEM_PROCESSOR=[ x86 | x64 ] -DCMAKE_GENERATOR_PLATFORM=[ Win32 | x64 ],version=10.0.18362.0 -DESCARGOT_ARCH=[ x86 | x64 ] -Bout -DENABLE_SHELL=ON -DESCARGOT_LIBICU_SUPPORT=ON -DESCARGOT_THREADING=ON
cd out
msbuild ESCARGOT.sln /property:Configuration=Release /p:platform=[ Win32 | x64 ]
```

## Debugger

Make sure Escargot is built with the `-DESCARGOT_DEBUGGER=1` flag (off by default) enabled;
then start Escargot with the `--start-debug-server` option.

### Connect using a debugger client

- Escargot python debugger
  - run `./tools/debugger/debugger.py`; It will automatically connect to a debug server on the default port `6501`
  - run `./tools/debugger/debugger.py --help` for a list of options
- [Visual Studio Code extension](https://github.com/Samsung/escargot-vscode-extension/?tab=readme-ov-file#how-to-use)
- Chrome Devtools `⚠️ Early in development ⚠️`
  - Initial setup:
    - Navigate to [chrome://inspect](chrome://inspect)
    - Make sure *Discover network targets* is enabled; click configure
    - Add `localhost:6501` as a target; click Done
  - Usage:
    - The started debug server will be listed in the *Remote Target* list (If it is not, the page may need to be reloaded using the browser reload button)
    - Click `inspect`
    - A new window with the Chrome Devtools debugger UI will open

## Testing ✅

Escargot supports various benchmark sets, which can be run using the [tools/run-tests.py](https://github.com/Samsung/escargot/blob/master/tools/run-tests.py) script.

Prerequisites:
```sh
# Python 3 only -- the v8/spidermonkey/test262 runners are pure python3, no python2 needed.
sudo apt-get install python3
sudo apt-get install python3-chardet  # or: pip install chardet -- required by the test262 runner
```

| Benchmark | flag |
| --- | --- |
| SunSpider 1.0.2 | `sunspider` |
| [Octane 2.0](https://github.com/chromium/octane.git) | `octane` |
| [test262](https://github.com/tc39/test262.git) | `test262` |
| [Web Tooling Benchmark](https://github.com/v8/web-tooling-benchmark) | `web-tooling-benchmark` |
| SpiderMonkey (vendor-made) | `spidermonkey` |
| ChakraCore (vendor-made) | `chakracore` |
| V8 (vendor-made) | `v8` |

Run each benchmark separately or all together as shown below:
```sh
tools/run-tests.py --engine=./out/linux/x64/release/escargot web-tooling-benchmark
tools/run-tests.py --engine=./out/linux/x64/release/escargot spidermonkey test262 v8
```

## Contributing 💡
Escargot welcomes contributions from developers in any form, wheter it's code, documentation, bug reports, or suggestions. By contributing to the project, you agree to license your contributions under the [LGPL-2.1](https://github.com/Samsung/escargot/blob/master/LICENSE) license.

#### ❗ Vulnerability Reporting
⚠️ If you identify any vulnerabilities, please report them through the [Issues page](https://github.com/Samsung/escargot/issues). *Reports sent via other channels may not be considered or may be processed with delays*. Please note that our project assumes the execution of valid JavaScript source code only. Handling of invalid source code is not within the main scope of this project and might not be addressed.

## Research Papers 📝
* [Dynamic code compression for JavaScript engine](https://doi.org/10.1002/spe.3186)  
  Software: Practice and Experience Vol. 53 (5), pp. 1196-1217, 2023

* [Tail Call Optimization Tailored for Native Stack Utilization in JavaScript Runtimes](https://doi.org/10.1109/ACCESS.2024.3441750)  
  IEEE Access Vol. 12, pp. 111801-111817, 2024

## License 📜
Escargot is open-source software primarily licensed under [LGPL-2.1](https://github.com/Samsung/escargot/blob/master/LICENSE), with some components covered by other licenses. Complete license and copyright information can be found in the source code.
