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

### Build Options

The following build options are supported when generating ninja rules using cmake.

| **Option** | **Description** | **Flag** | **Value** | **Default** |
|-|-|-|-|-|
| **HOST** | Choose target platform | -DESCARGOT_HOST | linux/darwin/android/windows | |
| **ARCH** | Choose target architecture | -DESCARGOT_ARCH | x64/x86/arm/aarch64 | |
| **MODE** | Choose release/debug mode | -DESCARGOT_MODE | release/debug | release |
| **OUTPUT** | Choose build output type | -DESCARGOT_OUTPUT | shared_lib/static_lib/shell/cctest | shell |
| **LIBICU** | Include libicu library | -DESCARGOT_LIBICU_SUPPORT | ON/OFF | ON |
| **THREADING** | Enable threading features (e.g. Atomics, SharedArrayBuffer) | -DESCARGOT_THREADING | ON/OFF | ON |
| **WASM** | Enable WebAssembly support | -DESCARGOT_WASM | ON/OFF | OFF |
| **CODE_CACHE** | Enable code cache | -DESCARGOT_CODE_CACHE | ON/OFF | OFF |
| **TCO** | Enable tail call optimization | -DESCARGOT_TCO | ON/OFF | OFF |
| **TLS_ADDRESS_OFFSET** | Enable thread local storge access optimization(offset) | -DESCARGOT_TLS_ACCESS_BY_ADDRESS | ON/OFF | OFF |
| **TLS_PTHREAD_KEY** | Enable thread local storge access optimization(pthread_key) | -DESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY | ON/OFF | OFF |
| **SMALL_CONFIG** | Enable aggressive memory optimizations for tiny devices | -DESCARGOT_SMALL_CONFIG | ON/OFF | OFF |
| **TEST** | Enable additional features used only for testing | -DESCARGOT_TEST | ON/OFF | OFF |

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
cmake -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell -GNinja
ninja
```

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
cmake -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell -GNinja
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

### Windows

Install VS2022 with cmake and ninja.
Open [ x86 Native Tools Command Prompt for VS 2022 | x64 Native Tools Command Prompt for VS 2022 ]

```sh
git submodule update --init third_party # update submodules

CMake -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=[ Windows | WindowsStore ] -DCMAKE_SYSTEM_VERSION:STRING="10.0"  -DCMAKE_SYSTEM_PROCESSOR=[ x86 | x64 ] -DCMAKE_GENERATOR_PLATFORM=[ Win32 | x64 ],version=10.0.18362.0 -DESCARGOT_ARCH=[ x86 | x64 ] -DESCARGOT_MODE=release -Bout -DESCARGOT_HOST=windows -DESCARGOT_OUTPUT=shell -DESCARGOT_LIBICU_SUPPORT=ON -DESCARGOT_THREADING=ON
cd out
msbuild ESCARGOT.sln /property:Configuration=Release /p:platform=[ Win32 | x64 ]
```

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
