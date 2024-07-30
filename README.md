# Escargot

[![License](https://img.shields.io/badge/License-LGPL%20v2.1-blue.svg)](LICENSE)
[![GitHub release (latestSemVer)](https://img.shields.io/github/v/release/Samsung/escargot)](https://github.com/Samsung/escargot/releases)
[![Actions Status](https://github.com/Samsung/escargot/workflows/ES-Actions/badge.svg)](https://github.com/Samsung/escargot/actions/workflows/es-actions.yml)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21647/badge.svg)](https://scan.coverity.com/projects/samsung-escargot)
[![codecov](https://codecov.io/gh/Samsung/escargot/branch/master/graph/badge.svg?token=DX8CN6E7A8)](https://codecov.io/gh/Samsung/escargot)

## Prerequisites

#### On Ubuntu Linux

General build prerequisites:
```sh
sudo apt-get install autoconf automake cmake libtool libicu-dev ninja-build
```

Prerequisites for x86-64-to-x86 compilation:
```sh
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install libicu-dev:i386
```

#### On macOS

```sh
brew install autoconf automake cmake icu4c libtool ninja pkg-config
```

## Build Escargot

```sh
git clone https://github.com/Samsung/escargot.git
cd escargot
git submodule update --init third_party
cmake -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell -GNinja
ninja
```

## Build Android version

Build prerequisites on Ubuntu:
```sh
sudo apt install openjdk-17-jdk # require java 17
```

```sh
git clone https://github.com/Samsung/escargot.git
cd escargot
git submodule update --init third_party
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

## Build Windows version

Install VS2022 with cmake and ninja.
Open [ x86 Native Tools Command Prompt for VS 2022 | x64 Native Tools Command Prompt for VS 2022 ]
```sh
git clone https://github.com/Samsung/escargot.git
cd escargot
git submodule update --init third_party

CMake -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=[ Windows | WindowsStore ] -DCMAKE_SYSTEM_VERSION:STRING="10.0"  -DCMAKE_SYSTEM_PROCESSOR=[ x86 | x64 ] -DCMAKE_GENERATOR_PLATFORM=[ Win32 | x64 ],version=10.0.18362.0 -DESCARGOT_ARCH=[ x86 | x64 ] -DESCARGOT_MODE=release -Bout -DESCARGOT_HOST=windows -DESCARGOT_OUTPUT=shell -DESCARGOT_LIBICU_SUPPORT=ON -DESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN=OFF -DESCARGOT_THREADING=ON
cd out
msbuild ESCARGOT.sln /property:Configuration=Release /p:platform=[ Win32 | x64 ]
```

#### Build options

The following build options are supported when generating ninja rules using cmake.

* -DESCARGOT_HOST=[ linux | tizen_obs | darwin | android | windows ]<br>
  Compile Escargot for Linux, Tizen, macOS, or Windows platform
* -DESCARGOT_ARCH=[ x64 | x86 | arm | i686 | aarch64 ]<br>
  Compile Escargot for each architecture
* -DESCARGOT_MODE=[ debug | release ]<br>
  Compile Escargot for either release or debug mode
* -DESCARGOT_OUTPUT=[ shared_lib | static_lib | shell | cctest ]<br>
  Define target output type
* -DESCARGOT_LIBICU_SUPPORT=[ ON | OFF ]<br>
  Enable libicu library if set ON. (Optional, default = ON)
* -DESCARGOT_THREADING=[ ON | OFF ]<br>
  Enable Threading support. (Optional, default = OFF)
* -DESCARGOT_CODE_CACHE=[ ON | OFF ]<br>
  Enable Code cache support. (Optional, default = OFF)
* -DESCARGOT_WASM=[ ON | OFF ]<br>
  Enable WASM support. (Optional, default = OFF)
* -DESCARGOT_SMALL_CONFIG=[ ON | OFF ]<br>
  Enable Options for small devices. (Optional, default = OFF)

## Testing

First, get benchmarks and tests:
```sh
git submodule update --init
```

### Benchmarks

Test run for each benchmark (Sunspider, Octane, V8, Chakracore, test262,
SpiderMonkey, etc.):
```sh
tools/run-tests.py --arch=x86_64 spidermonkey test262 v8
```
