#!/bin/bash

if [ -z "$ANDROID_NDK_STAND_ALONE" ]; then
    echo "Need to set ANDROID_NDK_STAND_ALONE"
    exit 1
fi

echo "ANDROID_NDK_STAND_ALONE env is ..."
echo $ANDROID_NDK_STAND_ALONE

cd ..
autoreconf -vif
automake --add-missing
cd android

make distclean

export CC="arm-linux-androideabi-gcc"
export PATH=$ANDROID_NDK_STAND_ALONE/bin:$PATH

../configure --disable-gc-debug --disable-parallel-mark --build=arm-linux-androideabi --target=arm-linux-androideabi --with-sysroot=$ANDROID_NDK_STAND_ALONE --host=x86_64-unknown-linux-gnu CFLAGS='-march=armv7-a'
make
