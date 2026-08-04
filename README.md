# Escargot

[![License](https://img.shields.io/badge/License-LGPL%20v2.1-blue.svg)](LICENSE)
[![GitHub release (latestSemVer)](https://img.shields.io/github/v/release/Samsung/escargot)](https://github.com/Samsung/escargot/releases)
[![Actions Status](https://github.com/Samsung/escargot/workflows/ES-Actions/badge.svg)](https://github.com/Samsung/escargot/actions/workflows/es-actions.yml)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21647/badge.svg)](https://scan.coverity.com/projects/samsung-escargot)
[![codecov](https://codecov.io/gh/Samsung/escargot/branch/master/graph/badge.svg?token=DX8CN6E7A8)](https://codecov.io/gh/Samsung/escargot)


Escargot is a lightweight JavaScript engine developed by [Samsung](https://github.com/Samsung), designed specifically for resource-constrained environments. It is optimized for performance and low memory usage, making it ideal for use in embedded systems, IoT devices, and other applications where resources are limited.

Key features of Escargot include:
* **ECMAScript Compliance**: Escargot supports a significant portion of the latest ECMAScript version ([ECMAScript 2025](https://262.ecma-international.org/16.0/)), ensuring compatibility with modern JavaScript standards while maintaining a lightweight footprint.
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

Escargot uses **standard CMake variables** for build configuration.

#### Standard CMake Variables

| **Option** | **Description** | **Flag** | **Value** | **Default** |
|-|-|-|-|-|
| **Build Type** | Choose release/debug mode | `-DCMAKE_BUILD_TYPE` | `Debug`/`Release`/`RelWithDebInfo`/`MinSizeRel` | `Release` |
| **Library Type** | Build shared or static library | `-DBUILD_SHARED_LIBS` | `ON`/`OFF` | `OFF` |
| **Build Shell** | Build shell executable | `-DESCARGOT_SHELL` | `ON`/`OFF` | `ON` |
| **Build Tests** | Build cctest (unit tests) | `-DBUILD_TESTING` | `ON`/`OFF` | `OFF` |

> **Note**: `MinSizeRel` build type automatically enables `ESCARGOT_SMALL_CONFIG` for size optimization.
> 
> **Note**: `BUILD_TESTING` builds the cctest executable (requires googletest). For runtime test features (test262, etc.), use `-DESCARGOT_TEST=ON` instead.

#### Platform Detection (Auto-detected)

| **Option** | **Description** | **CMake Standard** | **Notes** |
|-|-|-|-|
| **Host OS** | Auto-detected from system | `CMAKE_SYSTEM_NAME` | linux/darwin/android/windows/baremetal |
| **Architecture** | Auto-detected from CPU | `CMAKE_SYSTEM_PROCESSOR` | x64/x86/arm/aarch64/riscv64 |

> **Note**: Platform is automatically detected. Override only for cross-compilation.

#### Feature Options

| **Option** | **Description** | **Flag** | **Value** | **Default** |
|-|-|-|-|-|
| **LIBICU** | Include libicu library | `-DESCARGOT_LIBICU_SUPPORT` | ON/OFF | ON |
| **WASM** | Enable WebAssembly support | `-DESCARGOT_WASM` | ON/OFF | OFF |
| **CODE_CACHE** | Enable code cache | `-DESCARGOT_CODE_CACHE` | ON/OFF | OFF |
| **TCO** | Enable tail call optimization | `-DESCARGOT_TCO` | ON/OFF | OFF |
| **THREADING** | Enable threading features (e.g. Atomics, SharedArrayBuffer) | `-DESCARGOT_THREADING` | ON/OFF | ON |
| **TLS_ADDRESS_OFFSET** | Enable thread local storage access optimization (offset) | `-DESCARGOT_TLS_ACCESS_BY_ADDRESS` | ON/OFF | OFF |
| **TLS_PTHREAD_KEY** | Enable thread local storage access optimization (pthread_key) | `-DESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY` | ON/OFF | OFF |
| **TEMPORAL** | Enable Temporal support | `-DESCARGOT_TEMPORAL` | ON/OFF | OFF |
| **SHADOWREALM** | Enable ShadowRealm support | `-DESCARGOT_SHADOWREALM` | ON/OFF | OFF |
| **SMALL_CONFIG** | Enable aggressive memory optimizations for tiny devices | `-DESCARGOT_SMALL_CONFIG` | ON/OFF | OFF |
| **TEST** | Enable additional features used only for testing (test262, etc.) | `-DESCARGOT_TEST` | ON/OFF | OFF |
| **DEBUGGER** | Enable debug server | `-DESCARGOT_DEBUGGER` | ON/OFF | OFF |

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
cmake -DCMAKE_BUILD_TYPE=Release -DESCARGOT_SHELL=ON -GNinja
ninja
```

#### 32-bit Cross-Compilation (x86)

To build 32-bit binary on 64-bit Linux host, you need to set cross-compile flags and specify the target architecture:

```sh
# Prerequisites
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386

# Set compiler/linker flags for 32-bit
export CFLAGS="-m32"
export CXXFLAGS="-m32"
export LDFLAGS="-m32"

# Build with CMAKE_SYSTEM_PROCESSOR=x86
cmake -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86 -DCMAKE_BUILD_TYPE=Release -DESCARGOT_SHELL=ON -GNinja
ninja
```

> **Important**: All three elements are required for proper 32-bit build:
> 1. **Compiler flags** (`CFLAGS`/`CXXFLAGS="-m32"`) - Generate 32-bit instructions
> 2. **Linker flags** (`LDFLAGS="-m32"`) - Link 32-bit libraries  
> 3. **CMake target** (`CMAKE_SYSTEM_PROCESSOR=x86`) - Recognize 32-bit architecture
>
> Without `CMAKE_SYSTEM_PROCESSOR=x86`, CMake may still generate 64-bit code despite `-m32` flags, causing bit-field width errors.

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
cmake -DCMAKE_BUILD_TYPE=Release -DESCARGOT_SHELL=ON -GNinja
ninja
```

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
(no pthreads, no `mmap`, no filesystem).

Platform is **auto-detected** from CMake standard variables:
- `CMAKE_SYSTEM_NAME=Generic` (or other RTOS names: FreeRTOS, NuttX, Zephyr, etc.)
- `CMAKE_SYSTEM_PROCESSOR=arm` (or your target architecture)

```sh
# Example: Cross-compile for ARM bare-metal
cmake -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=arm \
      -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DESCARGOT_SHELL=OFF -DBUILD_SHARED_LIBS=OFF \
      /path/to/escargot
```

**Note**: 
- `-DESCARGOT_SHELL=OFF` - Build library only (no shell executable)
- `-DBUILD_SHARED_LIBS=OFF` - Build static library
- ICU and threading are automatically disabled for bare-metal targets

See [`docs/porting/RTOS_PORTING_GUIDE.md`](docs/porting/RTOS_PORTING_GUIDE.md)
for the full porting guide and
[`samples/rtos/freertos/`](samples/rtos/freertos) for a complete working sample
(FreeRTOS / Cortex-M55, QEMU `mps3-an547`).

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

# x64 build (x86 build: change x64 to Win32)
cmake -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x64 -DCMAKE_BUILD_TYPE=Release -DESCARGOT_SHELL=ON -DESCARGOT_LIBICU_SUPPORT=ON -DESCARGOT_THREADING=ON
cmake --build . --config Release
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
