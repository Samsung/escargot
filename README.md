# Escargot

[![License](https://img.shields.io/badge/License-LGPL%20v2.1-blue.svg)](LICENSE)
[![GitHub release (latestSemVer)](https://img.shields.io/github/v/release/Samsung/escargot)](https://github.com/Samsung/escargot/releases)
![Build status](http://13.209.201.68:8080/job/escargot-pr/job/master/badge/icon)
[![Actions Status](https://github.com/Samsung/escargot/workflows/Escargot%20Actions/badge.svg)](https://github.com/Samsung/escargot/actions)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21647/badge.svg)](https://scan.coverity.com/projects/samsung-escargot)

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

Note: For later build steps (cmake, pkg-config) to find ICU libraries, you may
need to set the `PKG_CONFIG_PATH` environment variable, as instructed by brew.
E.g.:

```sh
export PKG_CONFIG_PATH="/usr/local/opt/icu4c/lib/pkgconfig"
# ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN should be `OFF` to link icu lib in static
cmake -DESCARGOT_HOST=darwin -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN=OFF -DESCARGOT_OUTPUT=shell -GNinja
```

## Build Escargot

```sh
git clone https://github.com/Samsung/escargot.git
cd escargot
git submodule update --init third_party
cmake -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell -GNinja
ninja
```

## Build android version of Escargot

```sh
git clone https://github.com/Samsung/escargot.git
cd escargot
git submodule update --init third_party
export ANDROID_SDK_ROOT=.... # set your android SDK root first
cd build/android/
./gradlew bundleReleaseAar # build escargot AAR
./gradlew assembleDebug # build debug test shell
./gradlew :escargot:connectedDebugAndroidTest # run escargot-jni tests on android device
./gradlew :escargot:testDebugUnitTest # run escargot-jni tests on host
./gradlew bundleHostJar # bundle jar for host
```

#### Build options

The following build options are supported when generating ninja rules using cmake.

* -DESCARGOT_HOST=[ linux | tizen_obs | darwin | android ]<br>
  Compile Escargot for Linux, Tizen, or macOS platform
* -DESCARGOT_ARCH=[ x64 | x86 | arm | i686 | aarch64 ]<br>
  Compile Escargot for each architecture
* -DESCARGOT_MODE=[ debug | release ]<br>
  Compile Escargot for either release or debug mode
* -DESCARGOT_OUTPUT=[ shared_lib | static_lib | shell | shell_test | cctest ]<br>
  Define target output type
* -DESCARGOT_LIBICU_SUPPORT=[ ON | OFF ]<br>
  Enable libicu library if set ON. (Optional, default = ON)

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
