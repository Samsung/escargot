#!/bin/bash

export BUILD_DEPS_PATH="/home/ksh8281/work/StarFish/build_deps/android/SM-N950N_armv-7a_7.1.1/"
export CXXFLAGS+=" -DENABLE_ICU -I"$BUILD_DEPS_PATH"/include/ "
export LDFLAGS+=" -L"$BUILD_DEPS_PATH"/lib -licui18n -licuuc"

echo $CXXFLAGS
echo $LDFLAGS

#make android.interpreter.release -j12
make android.interpreter.release.shared.standalone -j12
